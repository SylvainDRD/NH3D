#include "vulkan_rhi.hpp"
#include "rendering/vulkan/vulkan_bind_group.hpp"
#include "rendering/vulkan/vulkan_compute_shader.hpp"
#include <cmath>
#include <cstdint>
#include <general/window.hpp>
#include <initializer_list>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/resource_manager.hpp>
#include <rendering/vulkan/vulkan_enums.hpp>
#include <scene/ecs/components/render_component.hpp>
#include <scene/ecs/components/transform_component.hpp>
#include <scene/scene.hpp>
#include <unordered_set>
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace NH3D {

struct RenderData {
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress indexBuffer;
    uint indexCount;
    Material material;
    AABB localBoundingBox;
};

struct CullingParameters {
    mat4 viewMatrix;
    vec4 frustumPlanes[6];
    uint objectCount;
};

VulkanRHI::VulkanRHI(const Window& Window)
    : IRHI {}
    , _textureManager { 1000, 100 }
    , _bufferManager { 40000, 400 }
    , _shaderManager { 100, 10 }
    , _computeShaderManager { 100, 10 }
    , _bindGroupManager { 200, 20 }
{
    _instance = createVkInstance(Window.requiredVulkanExtensions());

#if NH3D_DEBUG
    _debugUtilsMessenger = createDebugMessenger(_instance);
#endif

    _surface = Window.createVkSurface(_instance);

    auto [gpu, queues] = selectPhysicalDevice(_instance, _surface);
    _gpu = gpu;

    _device = createLogicalDevice(_gpu, queues);
    _allocator = createVMAAllocator(_instance, _gpu, _device);

    vkGetDeviceQueue(_device, queues.GraphicsQueueFamilyID, 0, &_graphicsQueue);
    vkGetDeviceQueue(_device, queues.PresentQueueFamilyID, 0, &_presentQueue);

    // The extent provided should match the surface, hopefully glfw does it right
    auto [swapchain, surfaceFormat] = createSwapchain(_device, _gpu, _surface, queues, { Window.getWidth(), Window.getHeight() });
    _swapchain = swapchain;

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(_device, _swapchain, &swapchainImageCount, nullptr);

    std::vector<VkImage> swapchainImages;
    swapchainImages.resize(swapchainImageCount);
    _swapchainTextures.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(_device, _swapchain, &swapchainImageCount, swapchainImages.data());

    for (int i = 0; i < swapchainImageCount; ++i) {
        auto&& [imageViewData, textureMetadata] = VulkanTexture::wrapSwapchainImage(std::cref(*this), swapchainImages[i], surfaceFormat,
            VkExtent3D { Window.getWidth(), Window.getHeight(), 1 }, VK_IMAGE_ASPECT_COLOR_BIT);
        _swapchainTextures[i] = _textureManager.store(std::move(imageViewData), std::move(textureMetadata));
    }

    _commandPool = createCommandPool(_device, queues.GraphicsQueueFamilyID);
    allocateCommandBuffers(_device, _commandPool, MaxFramesInFlight, _commandBuffers.data());

    _immediateCommandPool = createCommandPool(_device, queues.GraphicsQueueFamilyID);
    allocateCommandBuffers(_device, _immediateCommandPool, 1, &_immediateCommandBuffer);
    _immediateCommandFence = createFence(_device, false);

    _renderSemaphores.resize(swapchainImageCount);
    for (uint32_t i = 0; i < _renderSemaphores.size(); ++i) {
        _renderSemaphores[i] = createSemaphore(_device);
    }

    // Octahedron normal projection
    const VkFormat normalRTFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    // Fairly low precision acceptable for cartoonish rendering
    const VkFormat albedoRTFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    // const VkFormat albedoRTFormat = VK_FORMAT_R32_UINT; <---- To be used to prevent oversampling

    for (uint32_t i = 0; i < MaxFramesInFlight; ++i) {
        _presentSemaphores[i] = createSemaphore(_device);
        _frameFences[i] = createFence(_device, true);

        _gbufferRTs[i].normalRT = createTexture({
            .format = normalRTFormat,
            .extent = { Window.getWidth(), Window.getHeight(), 1 },
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
        });

        _gbufferRTs[i].albedoRT = createTexture({
            .format = albedoRTFormat,
            .extent = { Window.getWidth(), Window.getHeight(), 1 },
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
        });

        _gbufferRTs[i].depthRT = createTexture({
            .format = VK_FORMAT_D32_SFLOAT,
            .extent = { Window.getWidth(), Window.getHeight(), 1 },
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
        });

        _finalRTs[i] = createTexture({
            .format = VK_FORMAT_R16G16B16A16_SFLOAT, // Alpha?
            .extent = { Window.getWidth(), Window.getHeight(), 1 },
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
        });
    }

    const VkDescriptorType objectDataTypes[] = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // RenderData buffer
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // VisibleFlags buffer
    };
    auto [objectDataDescriptorSets, objectDataMetadata] = VulkanBindGroup::create(_device,
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .bindingTypes = { objectDataTypes, std::size(objectDataTypes) },
        });
    _cullingRenderDataBindGroup = _bindGroupManager.store(std::move(objectDataDescriptorSets), std::move(objectDataMetadata));

    auto [objectDataBuffer, objectDataAllocation] = VulkanBuffer::create(*this,
        {
            .size = sizeof(RenderData) * 100000,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        });
    _cullingRenderDataBuffer = _bufferManager.store(std::move(objectDataBuffer), std::move(objectDataAllocation));

    auto [objectDataStagingBuffer, objectDataStagingAllocation] = VulkanBuffer::create(*this,
        {
            .size = sizeof(RenderData) * 100000,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
        });
    _cullingRenderDataStagingBuffer = _bufferManager.store(std::move(objectDataStagingBuffer), std::move(objectDataStagingAllocation));

    constexpr size_t UniformBufferGuaranteedMaxSize = 16384;
    auto [visibleFlagBuffer, visibleFlagAllocation] = VulkanBuffer::create(*this,
        {
            .size = UniformBufferGuaranteedMaxSize,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        });
    _cullingVisibleFlagBuffer = _bufferManager.store(std::move(visibleFlagBuffer), std::move(visibleFlagAllocation));

    auto [visibleFlagStagingBuffer, visibleFlagStagingAllocation] = VulkanBuffer::create(*this,
        {
            .size = UniformBufferGuaranteedMaxSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
        });
    _cullingVisibleFlagStagingBuffer = _bufferManager.store(std::move(visibleFlagStagingBuffer), std::move(visibleFlagStagingAllocation));

    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        VulkanBindGroup::updateDescriptorSet(_device, objectDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = objectDataBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
        VulkanBindGroup::updateDescriptorSet(_device, objectDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = visibleFlagBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
    }

    const VkDescriptorType frameDataTypes[] = {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // CullingParams buffer
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // TransformData buffer
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // DrawCounter buffer
    };
    auto [frameDataDescriptorSets, frameDataMetadata] = VulkanBindGroup::create(_device,
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .bindingTypes = { frameDataTypes, std::size(frameDataTypes) },
        });
    _cullingFrameDataBindGroup = _bindGroupManager.store(std::move(frameDataDescriptorSets), std::move(frameDataMetadata));

    auto [cullingParametersBuffer, cullingParametersAllocation] = VulkanBuffer::create(*this,
        {
            .size = sizeof(CullingParameters),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        });
    _cullingParametersBuffer = _bufferManager.store(std::move(cullingParametersBuffer), std::move(cullingParametersAllocation));

    auto [cullingTransformBuffer, cullingTransformAllocation] = VulkanBuffer::create(*this,
        {
            .size = (sizeof(vec4) + sizeof(vec3) * 2) * 100000,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        });
    _cullingTransformBuffer = _bufferManager.store(std::move(cullingTransformBuffer), std::move(cullingTransformAllocation));

    auto [cullingTransformStagingBuffer, cullingTransformStagingAllocation] = VulkanBuffer::create(*this,
        {
            .size = (sizeof(vec4) + sizeof(vec3) * 2) * 100000,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
        });
    _cullingTransformStagingBuffer
        = _bufferManager.store(std::move(cullingTransformStagingBuffer), std::move(cullingTransformStagingAllocation));

    auto [cullingDrawCounterBuffer, cullingDrawCounterAllocation] = VulkanBuffer::create(*this,
        {
            .size = sizeof(uint32),
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        });
    _cullingDrawCounterBuffer = _bufferManager.store(std::move(cullingDrawCounterBuffer), std::move(cullingDrawCounterAllocation));

    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        VulkanBindGroup::updateDescriptorSet(_device, frameDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = cullingParametersBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0);
        VulkanBindGroup::updateDescriptorSet(_device, frameDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = cullingTransformBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
        VulkanBindGroup::updateDescriptorSet(_device, frameDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = cullingDrawCounterBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2);
    }

    const VkDescriptorType drawIndirectTypes[] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
    auto [drawIndirectDescriptorSets, drawIndirectMetadata] = VulkanBindGroup::create(_device,
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .bindingTypes = { drawIndirectTypes, std::size(drawIndirectTypes) },
        });
    _drawIndirectCommandBindGroup = _bindGroupManager.store(std::move(drawIndirectDescriptorSets), std::move(drawIndirectMetadata));

    auto [drawIndirectBuffer, drawIndirectAllocation] = VulkanBuffer::create(*this,
        {
            .size = sizeof(VkDrawIndirectCommand),
            .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        });
    _drawIndirectBuffer = _bufferManager.store(std::move(drawIndirectBuffer), std::move(drawIndirectAllocation));
    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        VulkanBindGroup::updateDescriptorSet(_device, drawIndirectDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = drawIndirectBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
    }
    const VkDescriptorType storageBufferType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    auto [drawRecordDescriptorSets, drawRecordMetadata] = VulkanBindGroup::create(_device,
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
            .bindingTypes = { &storageBufferType, 1 },
        });
    _drawRecordBindGroup = _bindGroupManager.store(std::move(drawRecordDescriptorSets), std::move(drawRecordMetadata));

    auto [drawRecordBuffer, drawRecordAllocation] = VulkanBuffer::create(*this,
        {
            .size = (sizeof(VkDeviceAddress) * 2 + sizeof(Material) + sizeof(mat4x3)) * 100000,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
        });
    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        VulkanBindGroup::updateDescriptorSet(_device, drawRecordDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = drawRecordBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
    }
    _drawRecordBuffer = _bufferManager.store(std::move(drawRecordBuffer), std::move(drawRecordAllocation));

    const VkDescriptorType textureBindingTypes[] = { VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE };
    auto [textureDescriptorSets, textureMetadata] = VulkanBindGroup::create(_device,
        {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .bindingTypes = { textureBindingTypes, std::size(textureBindingTypes) },
            .finalBindingCount = 1000,
        });

    _linearSampler = createSampler(_device, false);
    _nearestSampler = createSampler(_device, true);
    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        VulkanBindGroup::updateDescriptorSet(_device, textureDescriptorSets.sets[i],
            VkDescriptorImageInfo {
                .sampler = _linearSampler,
                .imageView = VK_NULL_HANDLE,
                .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            },
            VK_DESCRIPTOR_TYPE_SAMPLER, 0);
    }
    _albedoTextureBindGroup = _bindGroupManager.store(std::move(textureDescriptorSets), std::move(textureMetadata));

    const VkDescriptorType deferredShadingBindingTypes[] = {
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // Normal RT
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // Albedo RT
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // Depth RT
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // Final output RT
    };
    auto [deferredShadingDescriptorSets, deferredShadingMetadata] = VulkanBindGroup::create(_device,
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .bindingTypes = { deferredShadingBindingTypes, std::size(deferredShadingBindingTypes) },
        });
    _deferredShadingBindGroup = _bindGroupManager.store(std::move(deferredShadingDescriptorSets), std::move(deferredShadingMetadata));

    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        const auto& normalRTImageViewData = _textureManager.get<ImageView>(_gbufferRTs[i].normalRT);
        const auto& albedoRTImageViewData = _textureManager.get<ImageView>(_gbufferRTs[i].albedoRT);
        const auto& depthRTImageViewData = _textureManager.get<ImageView>(_gbufferRTs[i].depthRT);
        const auto& finalRTImageViewData = _textureManager.get<ImageView>(_finalRTs[i]);

        VulkanBindGroup::updateDescriptorSet(_device, deferredShadingDescriptorSets.sets[i],
            VkDescriptorImageInfo {
                .sampler = VK_NULL_HANDLE,
                .imageView = normalRTImageViewData.view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0);
        VulkanBindGroup::updateDescriptorSet(_device, deferredShadingDescriptorSets.sets[i],
            VkDescriptorImageInfo {
                .sampler = VK_NULL_HANDLE,
                .imageView = albedoRTImageViewData.view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);
        VulkanBindGroup::updateDescriptorSet(_device, deferredShadingDescriptorSets.sets[i],
            VkDescriptorImageInfo {
                .sampler = _nearestSampler,
                .imageView = depthRTImageViewData.view,
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
            },
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2);
        VulkanBindGroup::updateDescriptorSet(_device, deferredShadingDescriptorSets.sets[i],
            VkDescriptorImageInfo {
                .sampler = VK_NULL_HANDLE,
                .imageView = finalRTImageViewData.view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3);
    }

    const VkDescriptorSetLayout cullingLayouts[] = {
        objectDataMetadata.layout,
        frameDataMetadata.layout,
        drawIndirectMetadata.layout,
        drawRecordMetadata.layout,
    };
    auto [cullingComputeShader, cullingComputeLayout] = VulkanComputeShader::create(_device,
        {
            .computeShaderPath = NH3D_DIR "src/rendering/shaders/culling.comp.spv",
            .descriptorSetsLayouts = { cullingLayouts, std::size(cullingLayouts) },
        });
    _frustumCullingCS = _computeShaderManager.store(std::move(cullingComputeShader), std::move(cullingComputeLayout));

    const VkPushConstantRange pushConstantRange { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(mat4) };

    const VulkanShader::ColorAttachmentInfo colorAttachmentInfos[] = {
        {
            .format = normalRTFormat,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
            .blendEnable = false,
        },
        {
            .format = albedoRTFormat,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
            .blendEnable = false,
        },
    };

    const VkDescriptorSetLayout gbufferLayouts[] = { textureMetadata.layout, drawRecordMetadata.layout };
    auto [gbufferShader, gbufferPipelineLayout] = VulkanShader::create(_device,
        {
            .vertexShaderPath = NH3D_DIR "src/rendering/shaders/default_gbuffer_deferred.vert.spv",
            .fragmentShaderPath = NH3D_DIR "src/rendering/shaders/default_gbuffer_deferred.frag.spv",
            .colorAttachmentFormats = { colorAttachmentInfos, std::size(colorAttachmentInfos) },
            .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
            .descriptorSetsLayouts = { gbufferLayouts, std::size(gbufferLayouts) },
            .pushConstantRanges = { &pushConstantRange, 1 },
        });
    _gbufferShader = _shaderManager.store(std::move(gbufferShader), std::move(gbufferPipelineLayout));

    const VkDescriptorSetLayout deferredShadingLayouts[] = {
        deferredShadingMetadata.layout,
    };
    auto [deferredShadingShader, deferredShadingPipelineLayout] = VulkanComputeShader::create(_device,
        {
            .computeShaderPath = NH3D_DIR "src/rendering/shaders/deferred_shading.comp.spv",
            .descriptorSetsLayouts = { deferredShadingLayouts, std::size(deferredShadingLayouts) },
        });
    _deferredShadingCS = _computeShaderManager.store(std::move(deferredShadingShader), std::move(deferredShadingPipelineLayout));
}

VulkanRHI::~VulkanRHI()
{
    vkDeviceWaitIdle(_device);

    vkDestroySampler(_device, _linearSampler, nullptr);
    vkDestroySampler(_device, _nearestSampler, nullptr);
    _textureManager.clear(*this);
    _bufferManager.clear(*this);
    _shaderManager.clear(*this);
    _computeShaderManager.clear(*this);
    _bindGroupManager.clear(*this);

    for (uint32_t i = 0; i < MaxFramesInFlight; ++i) {
        vkDestroyFence(_device, _frameFences[i], nullptr);
        vkDestroySemaphore(_device, _presentSemaphores[i], nullptr);
    }

    for (uint32_t i = 0; i < _renderSemaphores.size(); ++i) {
        vkDestroySemaphore(_device, _renderSemaphores[i], nullptr);
    }

    vkDestroyFence(_device, _immediateCommandFence, nullptr);
    vkDestroyCommandPool(_device, _immediateCommandPool, nullptr);
    vkDestroyCommandPool(_device, _commandPool, nullptr);

    vmaDestroyAllocator(_allocator);

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    vkDestroyDevice(_device, nullptr);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);

#if NH3D_DEBUG
    PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugMessenger
        = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT"));
    destroyDebugMessenger(_instance, _debugUtilsMessenger, nullptr);
#endif

    vkDestroyInstance(_instance, nullptr);

    NH3D_LOG("Vulkan objects cleanup completed");
}

void VulkanRHI::executeImmediateCommandBuffer(const std::function<void(VkCommandBuffer)>& recordFunction) const
{
    beginCommandBuffer(_immediateCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    recordFunction(_immediateCommandBuffer);

    vkEndCommandBuffer(_immediateCommandBuffer);
    submitCommandBuffer(_graphicsQueue, {}, {}, _immediateCommandBuffer, _immediateCommandFence);

    vkWaitForFences(_device, 1, &_immediateCommandFence, VK_TRUE, NH3D_MAX_T(uint64_t));
    vkResetFences(_device, 1, &_immediateCommandFence);
    vkResetCommandBuffer(_immediateCommandBuffer, 0);
}

VkCommandBuffer VulkanRHI::getCommandBuffer() const { return _commandBuffers[_frameId % MaxFramesInFlight]; }

Handle<Texture> VulkanRHI::createTexture(const Texture::CreateInfo& info)
{
    return createTexture({
        .format = MapTextureFormat(info.format),
        .extent = { info.size.x, info.size.y, info.size.z },
        .usage = MapTextureUsageFlags(info.usage),
        .aspect = MapTextureAspectFlags(info.aspect),
        .initialData = info.initialData,
        .generateMipMaps = info.generateMipMaps,
    });
}

Handle<Texture> VulkanRHI::createTexture(const VulkanTexture::CreateInfo& info)
{
    auto&& [imageViewData, metadata] = VulkanTexture::create(*this, info);

    return _textureManager.store(std::move(imageViewData), std::move(metadata));
}

void VulkanRHI::destroyTexture(const Handle<Texture> handle) { _textureManager.release(*this, handle); }

Handle<Buffer> VulkanRHI::createBuffer(const Buffer::CreateInfo& info)
{
    auto&& [buffer, allocation] = VulkanBuffer::create(*this,
        {
            .size = info.size,
            .usage = MapBufferUsageFlags(info.usage),
            .memoryUsage = MapBufferMemoryUsage(info.memory),
            .initialData = info.initialData,
        });

    return _bufferManager.store(std::move(buffer), std::move(allocation));
}

void VulkanRHI::destroyBuffer(const Handle<Buffer> handle) { _bufferManager.release(*this, handle); }

void VulkanRHI::render(Scene& scene) const
{
    if (scene.size<RenderComponent>() == 0) {
        return;
    }

    const uint32_t frameInFlightId = _frameId % MaxFramesInFlight;

    if (vkWaitForFences(_device, 1, &_frameFences[frameInFlightId], VK_TRUE, NH3D_MAX_T(uint64_t)) != VK_SUCCESS) {
        NH3D_ABORT_VK("GPU stall detected");
    }
    vkResetFences(_device, 1, &_frameFences[frameInFlightId]);

    uint32_t swapchainImageId;
    // TODO: handle resize
    if (vkAcquireNextImageKHR(_device, _swapchain, NH3D_MAX_T(uint64_t), _presentSemaphores[frameInFlightId], nullptr, &swapchainImageId)
        != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to acquire next swapchain image");
    }

    VkCommandBuffer commandBuffer = getCommandBuffer();

    beginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // Buffer updates
    const GPUBuffer& transformStagingBuffer = _bufferManager.get<GPUBuffer>(_cullingTransformStagingBuffer);
    const BufferAllocationInfo& transformStagingAllocation = _bufferManager.get<BufferAllocationInfo>(_cullingTransformStagingBuffer);
    TransformComponent* transformDataPtr
        = reinterpret_cast<TransformComponent*>(VulkanBuffer::getMappedAddress(*this, transformStagingAllocation));

    uint objectCount = 0;
    // TODO: track dirty state properly
    constexpr bool dirtyRenderingData = true;
    if (dirtyRenderingData) {
        const GPUBuffer& objectDataStagingBuffer = _bufferManager.get<GPUBuffer>(_cullingRenderDataStagingBuffer);
        const BufferAllocationInfo& objectDataStagingAllocation = _bufferManager.get<BufferAllocationInfo>(_cullingRenderDataStagingBuffer);
        RenderData* objectDataPtr = reinterpret_cast<RenderData*>(VulkanBuffer::getMappedAddress(*this, objectDataStagingAllocation));

        for (const auto& [entity, renderComponent, transformComponent] : scene.makeView<RenderComponent, TransformComponent>()) {
            RenderData objectData;

            const VkBuffer vertexBuffer = _bufferManager.get<GPUBuffer>(renderComponent.getVertexBuffer()).buffer;
            objectData.vertexBuffer = VulkanBuffer::getDeviceAddress(*this, vertexBuffer);

            const GPUBuffer& indexBuffer = _bufferManager.get<GPUBuffer>(renderComponent.getIndexBuffer());
            objectData.indexBuffer = VulkanBuffer::getDeviceAddress(*this, indexBuffer.buffer);
            objectData.indexCount = indexBuffer.size / sizeof(uint32);

            objectData.material = renderComponent.getMaterial();
            objectData.localBoundingBox = AABB {};

            objectDataPtr[objectCount] = objectData;
            transformDataPtr[objectCount] = transformComponent;
            objectCount++;
        }

        const GPUBuffer& objectDataBuffer = _bufferManager.get<GPUBuffer>(_cullingRenderDataBuffer);
        VulkanBuffer::copyBuffer(commandBuffer, objectDataStagingBuffer.buffer, objectDataBuffer.buffer, sizeof(RenderData) * objectCount);

        VulkanBuffer::insertMemoryBarrier(commandBuffer, objectDataBuffer.buffer, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    } else {
        // Only update transforms
        // TODO: remove the need for RenderComponent here, it should only be used
        for (const auto& [entity, _, transformComponent] : scene.makeView<RenderComponent, TransformComponent>()) {
            transformDataPtr[objectCount] = transformComponent;
            objectCount++;
        }
    }

    // Reset the culling draw counter
    vkCmdFillBuffer(commandBuffer, _bufferManager.get<GPUBuffer>(_cullingDrawCounterBuffer).buffer, 0, sizeof(uint32), 0);

    const GPUBuffer& transformBuffer = _bufferManager.get<GPUBuffer>(_cullingTransformBuffer);
    VulkanBuffer::copyBuffer(
        commandBuffer, transformStagingBuffer.buffer, transformBuffer.buffer, sizeof(TransformComponent) * objectCount);
    VulkanBuffer::insertMemoryBarrier(commandBuffer, transformBuffer.buffer, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    // Update visible flags buffer
    const uint32 visibleFlagsBufferSize = objectCount > 0 ? ((objectCount - 1) >> 3) + 1 : 0;
    const GPUBuffer& visibleFlagStagingBuffer = _bufferManager.get<GPUBuffer>(_cullingVisibleFlagStagingBuffer);
    const BufferAllocationInfo& visibleFlagStagingAllocation = _bufferManager.get<BufferAllocationInfo>(_cullingVisibleFlagStagingBuffer);
    const GPUBuffer& visibleFlagBuffer = _bufferManager.get<GPUBuffer>(_cullingVisibleFlagBuffer);

    void* visibleFlagsPtr = VulkanBuffer::getMappedAddress(*this, visibleFlagStagingAllocation);
    std::memcpy(visibleFlagsPtr, scene.getRawVisibleFlags(), visibleFlagsBufferSize);
    VulkanBuffer::copyBuffer(commandBuffer, visibleFlagStagingBuffer.buffer, visibleFlagBuffer.buffer, visibleFlagsBufferSize);
    VulkanBuffer::insertMemoryBarrier(commandBuffer, visibleFlagBuffer.buffer, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    // Update culling parameters buffer
    const VkBuffer cullingParametersBuffer = _bufferManager.get<GPUBuffer>(_cullingParametersBuffer).buffer;
    // TODO: get actual camera data
    const CullingParameters cullingParameters {
        .objectCount = objectCount,
    };

    vkCmdUpdateBuffer(
        commandBuffer, cullingParametersBuffer, 0, sizeof(CullingParameters), reinterpret_cast<const void*>(&cullingParameters));
    VulkanBuffer::insertMemoryBarrier(commandBuffer, cullingParametersBuffer, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    // Frustum culling dispatch
    const auto cullingPipeline = _computeShaderManager.get<VkPipeline>(_frustumCullingCS);
    const auto cullingPipelineLayout = _computeShaderManager.get<VkPipelineLayout>(_frustumCullingCS);
    const VkDescriptorSet cullingDescriptorSets[] = {
        VulkanBindGroup::getUpdatedDescriptorSet(
            _device, _bindGroupManager.get<DescriptorSets>(_cullingRenderDataBindGroup), frameInFlightId),
        VulkanBindGroup::getUpdatedDescriptorSet(
            _device, _bindGroupManager.get<DescriptorSets>(_cullingFrameDataBindGroup), frameInFlightId),
        VulkanBindGroup::getUpdatedDescriptorSet(
            _device, _bindGroupManager.get<DescriptorSets>(_drawIndirectCommandBindGroup), frameInFlightId),
        VulkanBindGroup::getUpdatedDescriptorSet(_device, _bindGroupManager.get<DescriptorSets>(_drawRecordBindGroup), frameInFlightId),
    };

    VulkanBindGroup::bind(
        commandBuffer, { cullingDescriptorSets, std::size(cullingDescriptorSets) }, VK_PIPELINE_BIND_POINT_COMPUTE, cullingPipelineLayout);

    const vec3i cullingKernelSize { std::ceil(objectCount / 64.f), 1, 1 };
    VulkanComputeShader::dispatch(commandBuffer, cullingPipeline, cullingKernelSize);

    const GPUBuffer& drawIndirectBuffer = _bufferManager.get<GPUBuffer>(_drawIndirectBuffer);
    VulkanBuffer::insertMemoryBarrier(commandBuffer, drawIndirectBuffer.buffer, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);

    const GPUBuffer& drawRecordBuffer = _bufferManager.get<GPUBuffer>(_drawRecordBuffer);
    VulkanBuffer::insertMemoryBarrier(commandBuffer, drawRecordBuffer.buffer, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);

    auto graphicsPipeline = _shaderManager.get<VkPipeline>(_gbufferShader);
    auto graphicsLayout = _shaderManager.get<VkPipelineLayout>(_gbufferShader);

    const VkDescriptorSet graphicsDescriptorSets[] = {
        VulkanBindGroup::getUpdatedDescriptorSet(_device, _bindGroupManager.get<DescriptorSets>(_albedoTextureBindGroup), frameInFlightId),
        VulkanBindGroup::getUpdatedDescriptorSet(_device, _bindGroupManager.get<DescriptorSets>(_drawRecordBindGroup), frameInFlightId),
    };
    VulkanBindGroup::bind(
        commandBuffer, { graphicsDescriptorSets, std::size(graphicsDescriptorSets) }, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsLayout);

    const auto& albedoRTImageViewData = _textureManager.get<ImageView>(_gbufferRTs[frameInFlightId].albedoRT);
    const auto& albedoRTMetadata = _textureManager.get<TextureMetadata>(_gbufferRTs[frameInFlightId].albedoRT);
    VulkanTexture::insertMemoryBarrier(commandBuffer, albedoRTImageViewData.image, VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VulkanTexture::clearColor(
        commandBuffer, albedoRTImageViewData.image, color4 { 0.0f, 0.0f, 0.0f, 1.0f }, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VulkanTexture::insertMemoryBarrier(commandBuffer, albedoRTImageViewData.image, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    const auto& normalRTImageViewData = _textureManager.get<ImageView>(_gbufferRTs[frameInFlightId].normalRT);
    const auto& normalRTMetadata = _textureManager.get<TextureMetadata>(_gbufferRTs[frameInFlightId].normalRT);
    VulkanTexture::insertMemoryBarrier(commandBuffer, normalRTImageViewData.image, VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VulkanTexture::clearColor(commandBuffer, normalRTImageViewData.image, color4 { 0.0f }, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VulkanTexture::insertMemoryBarrier(commandBuffer, normalRTImageViewData.image, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    const auto& depthRTViewData = _textureManager.get<ImageView>(_gbufferRTs[frameInFlightId].depthRT);
    const auto& depthRTMetadata = _textureManager.get<TextureMetadata>(_gbufferRTs[frameInFlightId].depthRT);
    VulkanTexture::insertMemoryBarrier(commandBuffer, depthRTViewData.image, VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_CLEAR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED, true);
    VulkanTexture::clearDepth(commandBuffer, depthRTViewData.image, 0.0f, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); // 1 for reverse Z?
    VulkanTexture::insertMemoryBarrier(commandBuffer, depthRTViewData.image, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_CLEAR_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);

    const VkRenderingAttachmentInfo colorAttachmentsInfo[] = {
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = normalRTImageViewData.view,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        },
        {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = albedoRTImageViewData.view,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        },
    };

    // TODO: projection matrix from camera into push constant

    VulkanShader::draw(commandBuffer, graphicsPipeline,
            {
                .drawIndirectBuffer = drawIndirectBuffer.buffer,
                .extent = { albedoRTMetadata.extent.width, albedoRTMetadata.extent.height },
                .colorAttachments = { colorAttachmentsInfo, std::size(colorAttachmentsInfo) },
                .depthAttachment = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = depthRTViewData.view,
                    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                },
            });

    // TODO: transition GBuffer textures for compute reading
    VulkanTexture::insertMemoryBarrier(commandBuffer, normalRTImageViewData.image, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VulkanTexture::insertMemoryBarrier(commandBuffer, albedoRTImageViewData.image, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VulkanTexture::insertMemoryBarrier(commandBuffer, depthRTViewData.image, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        true);

    // Clearing is not necessary I believe
    const auto& finalRTImageViewData = _textureManager.get<ImageView>(_finalRTs[frameInFlightId]);
    const auto& finalRTMetadata = _textureManager.get<TextureMetadata>(_finalRTs[frameInFlightId]);
    VulkanTexture::insertMemoryBarrier(commandBuffer, finalRTImageViewData.image, VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_GENERAL);

    const auto deferredShadingPipeline = _computeShaderManager.get<VkPipeline>(_deferredShadingCS);
    const auto deferredShadingPipelineLayout = _computeShaderManager.get<VkPipelineLayout>(_deferredShadingCS);

    const VkDescriptorSet shadingDescriptorSets[] = {
        VulkanBindGroup::getUpdatedDescriptorSet(
            _device, _bindGroupManager.get<DescriptorSets>(_deferredShadingBindGroup), frameInFlightId),
    };
    VulkanBindGroup::bind(commandBuffer, { shadingDescriptorSets, std::size(shadingDescriptorSets) }, VK_PIPELINE_BIND_POINT_COMPUTE,
        deferredShadingPipelineLayout);
    const vec3i shadingKernelSize { std::ceil(finalRTMetadata.extent.width / 8.f), std::ceil(finalRTMetadata.extent.height / 8.f), 1 };

    VulkanComputeShader::dispatch(commandBuffer, deferredShadingPipeline, shadingKernelSize);

    VulkanTexture::insertMemoryBarrier(commandBuffer, finalRTImageViewData.image, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    const auto& scImageViewData = _textureManager.get<ImageView>(_swapchainTextures[swapchainImageId]);
    const auto& scMetadata = _textureManager.get<TextureMetadata>(_swapchainTextures[swapchainImageId]);
    VulkanTexture::insertMemoryBarrier(commandBuffer, scImageViewData.image, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE,
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VulkanTexture::blit(commandBuffer, finalRTImageViewData.image, finalRTMetadata.extent, scImageViewData.image, scMetadata.extent);

    VulkanTexture::insertMemoryBarrier(commandBuffer, scImageViewData.image, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT,
        VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkEndCommandBuffer(commandBuffer);
    submitCommandBuffer(_graphicsQueue,
        makeSemaphoreSubmitInfo(_presentSemaphores[frameInFlightId], VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT),
        makeSemaphoreSubmitInfo(_renderSemaphores[swapchainImageId], VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT), commandBuffer,
        _frameFences[frameInFlightId]); // TODO: watch out for the ALL_GRAPHICS_BIT when doing Compute based shading

    const VkPresentInfoKHR presentInfo {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &_renderSemaphores[swapchainImageId],
        .swapchainCount = 1,
        .pSwapchains = &_swapchain,
        .pImageIndices = &swapchainImageId,
    };
    vkQueuePresentKHR(_presentQueue, &presentInfo);

    ++_frameId;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        NH3D_WARN_VK(pCallbackData->pMessage);
    } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        NH3D_ERROR_VK(pCallbackData->pMessage);
    } else if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        NH3D_WARN_VK(pCallbackData->pMessage);
    }

    return VK_FALSE;
}

VkInstance VulkanRHI::createVkInstance(std::vector<const char*>&& requiredWindowExtensions) const
{
    const VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = NH3D_NAME,
        .applicationVersion = VK_MAKE_VERSION(NH3D_VERSION_MAJOR, NH3D_VERSION_MINOR, NH3D_VERSION_PATCH),
        .pEngineName = NH3D_NAME,
        .engineVersion = VK_MAKE_VERSION(NH3D_VERSION_MAJOR, NH3D_VERSION_MINOR, NH3D_VERSION_PATCH),
        .apiVersion = VK_API_VERSION_1_3,
    };

    std::vector<const char*> requiredExtensions = std::move(requiredWindowExtensions);
#if NH3D_DEBUG
    requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
#endif

    const VkInstanceCreateInfo vkInstanceCreateInfo {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pApplicationInfo = &appInfo,
#if NH3D_DEBUG
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &validationLayerName,
#endif
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data(),
    };

    VkInstance instance;
    if (vkCreateInstance(&vkInstanceCreateInfo, nullptr, &instance) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan instance");
    }

    NH3D_LOG("Vulkan instance successfully created");
    return instance;
}

VkDebugUtilsMessengerEXT VulkanRHI::createDebugMessenger(const VkInstance instance) const
{
    PFN_vkCreateDebugUtilsMessengerEXT createDebugMessenger
        = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

    const VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = vulkanValidationCallback,
    };

    VkDebugUtilsMessengerEXT debugMessenger;
    if (createDebugMessenger(instance, &messengerCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan debug utils messenger");
    }

    NH3D_LOG("Vulkan debug messenger successfully created");
    return debugMessenger;
}

std::pair<VkPhysicalDevice, VulkanRHI::PhysicalDeviceQueueFamilyID> VulkanRHI::selectPhysicalDevice(
    const VkInstance instance, const VkSurfaceKHR surface) const
{
    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

    std::vector<VkPhysicalDevice> availableGpus { physicalDeviceCount };
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, availableGpus.data());

    PhysicalDeviceQueueFamilyID queues {};
    std::vector<VkQueueFamilyProperties> queueData { 16 };

    std::string selectedDeviceName;
    uint32_t deviceId = NH3D_MAX_T(uint32_t);

    const VkQueueFlags requiredQueueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

    for (uint32_t i = 0; i < physicalDeviceCount; ++i) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(availableGpus[i], &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(availableGpus[i], &features);

        if (features.multiDrawIndirect
            && (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                || (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && deviceId == NH3D_MAX_T(uint32_t)))) {
            uint32_t familyCount;
            vkGetPhysicalDeviceQueueFamilyProperties(availableGpus[i], &familyCount, nullptr);

            queueData.resize(familyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(availableGpus[i], &familyCount, queueData.data());

            queues = {};
            for (int32_t queueId = 0; queueId < familyCount; ++queueId) {
                if (queues.GraphicsQueueFamilyID == NH3D_MAX_T(decltype(queues.GraphicsQueueFamilyID))
                    && queueData[queueId].queueFlags & requiredQueueFlags) {
                    queues.GraphicsQueueFamilyID = queueId;
                }

                if (queues.PresentQueueFamilyID == NH3D_MAX_T(decltype(queues.PresentQueueFamilyID))) {
                    VkBool32 supported;
                    vkGetPhysicalDeviceSurfaceSupportKHR(availableGpus[i], queueId, surface, &supported);
                    queues.PresentQueueFamilyID = queueId;
                }
            }

            if (queues.isValid()) {
                deviceId = i;
                selectedDeviceName = properties.deviceName;
            }
        }
    }

    if (deviceId == NH3D_MAX_T(uint32_t)) {
        NH3D_ABORT_VK("Couldn't find a suitable physical device");
    }

    VkPhysicalDevice gpu = availableGpus[deviceId];

    NH3D_LOG("Selected Vulkan device: " << selectedDeviceName);

    return { availableGpus[deviceId], queues };
}

VkDevice VulkanRHI::createLogicalDevice(const VkPhysicalDevice gpu, const PhysicalDeviceQueueFamilyID queues) const
{
    std::unordered_set<uint32_t> queueIndices { queues.GraphicsQueueFamilyID, queues.PresentQueueFamilyID };

    const float priority = 1.f;
    std::vector<VkDeviceQueueCreateInfo> queuesCreateInfo {};
    VkDeviceQueueCreateInfo queueCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    for (uint32_t queueID : queueIndices) {
        queueCreateInfo.queueFamilyIndex = queueID;

        queuesCreateInfo.emplace_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures features {};
    VkPhysicalDeviceVulkan11Features features11 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .shaderDrawParameters = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features features12 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features11,
        .descriptorIndexing = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        // .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE, won't be needed I'm pretty sure
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        .scalarBlockLayout = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
        // descriptorBindingUniformBufferUpdateAfterBind seems to be poorly supported, see
        // https://vulkan.gpuinfo.org/listfeaturescore12.php
    };
    VkPhysicalDeviceVulkan13Features features13 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &features12,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    const char* swapchainExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    const VkDeviceCreateInfo deviceCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features13,
        .queueCreateInfoCount = static_cast<uint32_t>(queuesCreateInfo.size()),
        .pQueueCreateInfos = queuesCreateInfo.data(),
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &swapchainExt,
        .pEnabledFeatures = &features,
    };

    VkDevice device;
    if (vkCreateDevice(gpu, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan device creation failed");
    }

    NH3D_LOG("Vulkan device created successfully");

    return device;
}

std::pair<VkSwapchainKHR, VkFormat> VulkanRHI::createSwapchain(const VkDevice device, const VkPhysicalDevice gpu,
    const VkSurfaceKHR surface, const PhysicalDeviceQueueFamilyID queues, const VkExtent2D extent,
    const VkSwapchainKHR previousSwapchain) const
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surfaceCapabilities);

    uint32_t formatsCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatsCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats { formatsCount };
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatsCount, surfaceFormats.data());

    const VkFormat preferredFormats[] = { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB };

    uint32_t formatId = 0;
    for (; formatId < sizeof(preferredFormats); ++formatId) {
        VkFormat format = preferredFormats[formatId];
        auto predicate = [format](VkSurfaceFormatKHR surfaceFormat) { return surfaceFormat.format == format; };
        if (std::find_if(surfaceFormats.cbegin(), surfaceFormats.cend(), predicate) != surfaceFormats.cend()) {
            break;
        }
    }

    if (formatId == sizeof(preferredFormats)) {
        NH3D_ABORT_VK("Couldn't find a supported surface format");
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = surfaceCapabilities.maxImageCount > 0 && surfaceCapabilities.minImageCount + 1 > surfaceCapabilities.maxImageCount
            ? surfaceCapabilities.maxImageCount
            : surfaceCapabilities.minImageCount + 1,
        .imageFormat = preferredFormats[formatId],
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR, // TODO: allow uncapped framerate
        .clipped = VK_TRUE,
        .oldSwapchain = previousSwapchain,
    };

    if (queues.GraphicsQueueFamilyID != queues.PresentQueueFamilyID) {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = (uint32_t*)&queues; // a bit freaky
    } else {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.queueFamilyIndexCount = 0;
        swapchainCreateInfo.pQueueFamilyIndices = nullptr;
    }

    VkSwapchainKHR swapchain;
    if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan swapchain creation failed");
    }

    return { swapchain, preferredFormats[formatId] };
}

VkCommandPool VulkanRHI::createCommandPool(const VkDevice device, const uint32_t queueFamilyIndex) const
{
    const VkCommandPoolCreateInfo commandPoolCreateInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };

    VkCommandPool commandPool;
    if (vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan command pool creation failed");
    }

    return commandPool;
}

void VulkanRHI::allocateCommandBuffers(
    const VkDevice device, const VkCommandPool commandPool, const uint32_t bufferCount, VkCommandBuffer* buffers) const
{
    const VkCommandBufferAllocateInfo cbAllocInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = bufferCount,
    };

    if (vkAllocateCommandBuffers(device, &cbAllocInfo, buffers) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan command buffers allocation failed");
    }
}

VkSemaphore VulkanRHI::createSemaphore(const VkDevice device) const
{
    const VkSemaphoreCreateInfo semCreateInfo {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkSemaphore semaphore;
    if (vkCreateSemaphore(device, &semCreateInfo, nullptr, &semaphore) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan semaphore");
    }

    return semaphore;
}

VkFence VulkanRHI::createFence(const VkDevice device, const bool signaled) const
{
    const VkFenceCreateInfo fenceCreateInfo {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlags {},
    };

    VkFence fence;
    if (vkCreateFence(device, &fenceCreateInfo, nullptr, &fence) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan fence");
    }

    return fence;
}

void VulkanRHI::beginCommandBuffer(
    const VkCommandBuffer commandBuffer, const VkCommandBufferUsageFlags flags, const bool resetCommandBuffer) const
{
    const VkCommandBufferBeginInfo cbBeginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags,
        .pInheritanceInfo = nullptr,
    };

    if (resetCommandBuffer) {
        vkResetCommandBuffer(commandBuffer, 0);
    }

    vkBeginCommandBuffer(commandBuffer, &cbBeginInfo);
}

VkSemaphoreSubmitInfo VulkanRHI::makeSemaphoreSubmitInfo(const VkSemaphore semaphore, const VkPipelineStageFlags2 stageMask) const
{
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = semaphore,
        .value = 1,
        .stageMask = stageMask,
        .deviceIndex = 0,
    };
}

void VulkanRHI::submitCommandBuffer(const VkQueue queue, const VkSemaphoreSubmitInfo& waitSemaphore,
    const VkSemaphoreSubmitInfo& signalSemaphore, const VkCommandBuffer commandBuffer, const VkFence fence) const
{
    const VkCommandBufferSubmitInfo cbSubmitInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commandBuffer,
        .deviceMask = 0,
    };

    const VkSubmitInfo2 submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = waitSemaphore.semaphore ? 1u : 0u,
        .pWaitSemaphoreInfos = &waitSemaphore,
        .commandBufferInfoCount = commandBuffer ? 1u : 0u,
        .pCommandBufferInfos = &cbSubmitInfo,
        .signalSemaphoreInfoCount = signalSemaphore.semaphore ? 1u : 0u,
        .pSignalSemaphoreInfos = &signalSemaphore,
    };

    vkQueueSubmit2(queue, 1, &submitInfo, fence);
}

VmaAllocator VulkanRHI::createVMAAllocator(const VkInstance instance, const VkPhysicalDevice gpu, const VkDevice device) const
{
    const VmaAllocatorCreateInfo allocatorCreateInfo {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = gpu,
        .device = device,
        .instance = instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };

    VmaAllocator allocator;
    if (vmaCreateAllocator(&allocatorCreateInfo, &allocator) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create VMA allocator");
    }

    return allocator;
}

VkSampler VulkanRHI::createSampler(const VkDevice device, const bool linear) const
{
    const VkFilter filter = linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

    const VkSamplerCreateInfo samplerCreateInfo {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = filter,
        .minFilter = filter,
        .mipmapMode = linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    };

    VkSampler sampler;
    if (vkCreateSampler(_device, &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan sampler");
    }

    return sampler;
}
}
