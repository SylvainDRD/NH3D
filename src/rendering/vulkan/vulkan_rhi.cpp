#include "vulkan_rhi.hpp"
#include <cmath>
#include <cstdint>
#include <general/window.hpp>
#include <misc/math.hpp>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/handle.hpp>
#include <rendering/core/material.hpp>
#include <rendering/core/resource_manager.hpp>
#include <rendering/vulkan/vulkan_bind_group.hpp>
#include <rendering/vulkan/vulkan_buffer.hpp>
#include <rendering/vulkan/vulkan_compute_shader.hpp>
#include <rendering/vulkan/vulkan_debug_drawer.hpp>
#include <rendering/vulkan/vulkan_enums.hpp>
#include <rendering/vulkan/vulkan_shader.hpp>
#include <rendering/vulkan/vulkan_texture.hpp>
#include <scene/ecs/components/render_component.hpp>
#include <scene/ecs/components/transform_component.hpp>
#include <scene/scene.hpp>
#include <unordered_set>
#include <utility>
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace NH3D {

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

    uint32 swapchainImageCount;
    vkGetSwapchainImagesKHR(_device, _swapchain, &swapchainImageCount, nullptr);

    std::vector<VkImage> swapchainImages;
    swapchainImages.resize(swapchainImageCount);
    _swapchainTextures.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(_device, _swapchain, &swapchainImageCount, swapchainImages.data());

    for (int i = 0; i < swapchainImageCount; ++i) {
        _swapchainTextures[i] = _textureManager.create(*this,
            {
                .image = swapchainImages[i],
                .format = surfaceFormat,
                .extent = VkExtent3D { Window.getWidth(), Window.getHeight(), 1 },
                .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
            });
    }

    _commandPool = createCommandPool(_device, queues.GraphicsQueueFamilyID);
    allocateCommandBuffers(_device, _commandPool, MaxFramesInFlight, &_commandBuffers[0]);

    _immediateCommandPool = createCommandPool(_device, queues.GraphicsQueueFamilyID);
    allocateCommandBuffers(_device, _immediateCommandPool, 1, &_immediateCommandBuffer);
    _immediateAndUploadFence = createFence(_device, false);

    _uploadCommandPool = createCommandPool(_device, queues.GraphicsQueueFamilyID);
    allocateCommandBuffers(_device, _uploadCommandPool, 1, &_uploadCommandBuffer);
    beginCommandBuffer(_uploadCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    _renderSemaphores.resize(swapchainImageCount);
    for (uint32_t i = 0; i < _renderSemaphores.size(); ++i) {
        _renderSemaphores[i] = createSemaphore(_device);
    }

    // Octahedron normal projection
    const VkFormat normalRTFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    // Fairly low precision acceptable for cartoonish rendering
    const VkFormat albedoRTFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;

    for (int i = 0; i < MaxFramesInFlight; ++i) {
        _presentSemaphores[i] = createSemaphore(_device);
        _frameFences[i] = createFence(_device, true);

        _gbufferRTs[i].normalRT = _textureManager.create(*this,
            {
                .format = normalRTFormat,
                .extent = { Window.getWidth(), Window.getHeight(), 1 },
                .usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
            });

        _gbufferRTs[i].albedoRT = _textureManager.create(*this,
            {
                .format = albedoRTFormat,
                .extent = { Window.getWidth(), Window.getHeight(), 1 },
                .usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
            });

        _gbufferRTs[i].depthRT = _textureManager.create(*this,
            {
                .format = VK_FORMAT_D32_SFLOAT,
                .extent = { Window.getWidth(), Window.getHeight(), 1 },
                .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
            });

        _finalRTs[i] = _textureManager.create(*this,
            {
                .format = VK_FORMAT_R16G16B16A16_SFLOAT, // Alpha?
                .extent = { Window.getWidth(), Window.getHeight(), 1 },
                .usageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
            });
    }

    constexpr uint32 MaxObjects = 640'000;

    const VkDescriptorType objectDataTypes[] = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // RenderData buffer
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // VisibleFlags buffer
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // AABBs buffer
    };
    _cullingRenderDataBindGroup = _bindGroupManager.create(*this,
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .bindingTypes = objectDataTypes,
        });

    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        _cullingRenderDataBuffers[i] = _bufferManager.create(*this,
            {
                .size = sizeof(RenderData) * MaxObjects,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
            });

        _cullingRenderDataStagingBuffers[i] = _bufferManager.create(*this,
            {
                .size = sizeof(RenderData) * MaxObjects,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
            });

        _cullingVisibleFlagBuffers[i] = _bufferManager.create(*this,
            {
                .size = MaxObjects / 8 + 1,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
            });

        _cullingVisibleFlagStagingBuffers[i] = _bufferManager.create(*this,
            {
                .size = MaxObjects / 8 + 1,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
            });

        _cullingAABBsBuffers[i] = _bufferManager.create(*this,
            {
                .size = sizeof(AABB) * MaxObjects,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
            });

        _cullingAABBsStagingBuffers[i] = _bufferManager.create(*this,
            {
                .size = sizeof(AABB) * MaxObjects,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
            });

        const VkBuffer cullingAABBsBuffer = _bufferManager.get<GPUBuffer>(_cullingAABBsBuffers[i]).buffer;

        auto& objectDataDescriptorSets = _bindGroupManager.get<DescriptorSets>(_cullingRenderDataBindGroup);
        VulkanBindGroup::updateDescriptorSet(_device, objectDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = _bufferManager.get<GPUBuffer>(_cullingRenderDataBuffers[i]).buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
        VulkanBindGroup::updateDescriptorSet(_device, objectDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = _bufferManager.get<GPUBuffer>(_cullingVisibleFlagBuffers[i]).buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
        VulkanBindGroup::updateDescriptorSet(_device, objectDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = _bufferManager.get<GPUBuffer>(_cullingAABBsBuffers[i]).buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2);
    }

    const VkDescriptorType frameDataTypes[] = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // TransformData buffer
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // DrawCounter buffer
    };
    _cullingFrameDataBindGroup = _bindGroupManager.create(*this,
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .bindingTypes = frameDataTypes,
        });

    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        _cullingTransformBuffers[i] = _bufferManager.create(*this,
            {
                .size = sizeof(TransformComponent) * MaxObjects,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
            });
        const VkBuffer cullingTransformBuffer = _bufferManager.get<GPUBuffer>(_cullingTransformBuffers[i]).buffer;

        _cullingDrawCounterBuffers[i] = _bufferManager.create(*this,
            {
                .size = sizeof(uint32),
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
            });

        auto& frameDataDescriptorSets = _bindGroupManager.get<DescriptorSets>(_cullingFrameDataBindGroup);
        VulkanBindGroup::updateDescriptorSet(_device, frameDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = _bufferManager.get<GPUBuffer>(_cullingTransformBuffers[i]).buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
        VulkanBindGroup::updateDescriptorSet(_device, frameDataDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = _bufferManager.get<GPUBuffer>(_cullingDrawCounterBuffers[i]).buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
    }

    const VkDescriptorType drawIndirectTypes[] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
    _drawIndirectCommandBindGroup = _bindGroupManager.create(*this,
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .bindingTypes = drawIndirectTypes,
        });
    const VkDescriptorType storageBufferType[] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
    _drawRecordBindGroup = _bindGroupManager.create(*this,
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
            .bindingTypes = storageBufferType,
        });
    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        _drawIndirectBuffers[i] = _bufferManager.create(*this,
            {
                .size = MaxObjects * sizeof(VkDrawIndirectCommand),
                .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
            });
        auto& drawIndirectBuffer = _bufferManager.get<GPUBuffer>(_drawIndirectBuffers[i]);
        auto& drawIndirectAllocation = _bufferManager.get<BufferAllocationInfo>(_drawIndirectBuffers[i]);

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(_gpu, &properties);
        NH3D_ASSERT(drawIndirectAllocation.allocatedSize / sizeof(VkDrawIndirectCommand) < properties.limits.maxDrawIndirectCount,
            "Insufficient max indirect draw count");

        auto& drawIndirectDescriptorSets = _bindGroupManager.get<DescriptorSets>(_drawIndirectCommandBindGroup);
        VulkanBindGroup::updateDescriptorSet(_device, drawIndirectDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = drawIndirectBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

        _drawRecordBuffers[i] = _bufferManager.create(*this,
            {
                .size = sizeof(DrawRecord) * MaxObjects,
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
            });

        auto& drawRecordDescriptorSets = _bindGroupManager.get<DescriptorSets>(_drawRecordBindGroup);
        VulkanBindGroup::updateDescriptorSet(_device, drawRecordDescriptorSets.sets[i],
            VkDescriptorBufferInfo {
                .buffer = _bufferManager.get<GPUBuffer>(_drawRecordBuffers[i]).buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            },
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
    }

    const VkDescriptorType textureBindingTypes[] = { VK_DESCRIPTOR_TYPE_SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE };
    _albedoTextureBindGroup = _bindGroupManager.create(*this,
        {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, // TODO: update for compute shaders
            .bindingTypes = textureBindingTypes,
            .finalBindingCount = 1000,
        });
    _linearSampler = createSampler(_device, false);
    _nearestSampler = createSampler(_device, true);
    auto& textureDescriptorSets = _bindGroupManager.get<DescriptorSets>(_albedoTextureBindGroup);
    for (int i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        VulkanBindGroup::updateDescriptorSet(_device, textureDescriptorSets.sets[i],
            VkDescriptorImageInfo {
                .sampler = _linearSampler,
                .imageView = VK_NULL_HANDLE,
                .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            },
            VK_DESCRIPTOR_TYPE_SAMPLER, 0);
    }

    const VkDescriptorType deferredShadingBindingTypes[] = {
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // Normal RT
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // Albedo RT
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // Depth RT
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // Final output RT
    };
    _deferredShadingBindGroup = _bindGroupManager.create(*this,
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .bindingTypes = deferredShadingBindingTypes,
        });
    auto& deferredShadingDescriptorSets = _bindGroupManager.get<DescriptorSets>(_deferredShadingBindGroup);

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

    const VkPushConstantRange cullingPushConstantRange {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(CullingParameters),
    };

    const auto& objectDataMetadataLayout = _bindGroupManager.get<BindGroupMetadata>(_cullingRenderDataBindGroup).layout;
    const auto& frameDataMetadataLayout = _bindGroupManager.get<BindGroupMetadata>(_cullingFrameDataBindGroup).layout;
    const auto& drawIndirectMetadataLayout = _bindGroupManager.get<BindGroupMetadata>(_drawIndirectCommandBindGroup).layout;
    const auto& textureMetadataLayout = _bindGroupManager.get<BindGroupMetadata>(_albedoTextureBindGroup).layout;
    const auto& drawRecordMetadataLayout = _bindGroupManager.get<BindGroupMetadata>(_drawRecordBindGroup).layout;
    const VkDescriptorSetLayout cullingLayouts[] = {
        objectDataMetadataLayout,
        frameDataMetadataLayout,
        drawIndirectMetadataLayout,
        drawRecordMetadataLayout,
    };
    _frustumCullingCS = _computeShaderManager.create(*this,
        {
            .computeShaderPath = NH3D_DIR "src/rendering/shaders/culling.comp.spv",
            .descriptorSetsLayouts = cullingLayouts,
            .pushConstantRanges = cullingPushConstantRange,
        });

    const VkPushConstantRange gbufferPushConstantRange { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(mat4) };

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

    const VkDescriptorSetLayout gbufferLayouts[] = { textureMetadataLayout, drawRecordMetadataLayout };
    _gbufferShader = _shaderManager.create(*this,
        {
            .vertexShaderPath = NH3D_DIR "src/rendering/shaders/default_gbuffer_deferred.vert.spv",
            .fragmentShaderPath = NH3D_DIR "src/rendering/shaders/default_gbuffer_deferred.frag.spv",
            .colorAttachmentFormats = { colorAttachmentInfos, std::size(colorAttachmentInfos) },
            .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
            .descriptorSetsLayouts = gbufferLayouts,
            .pushConstantRanges = gbufferPushConstantRange,
        });

    const auto& deferredShadingMetadataLayout = _bindGroupManager.get<BindGroupMetadata>(_deferredShadingBindGroup).layout;
    const VkDescriptorSetLayout deferredShadingLayouts[] = {
        deferredShadingMetadataLayout,
    };
    _deferredShadingCS = _computeShaderManager.create(*this,
        {
            .computeShaderPath = NH3D_DIR "src/rendering/shaders/deferred_shading.comp.spv",
            .descriptorSetsLayouts = deferredShadingLayouts,
        });

    std::array<Handle<Texture>, MaxFramesInFlight> depthTextures;
    for (int i = 0; i < MaxFramesInFlight; ++i) {
        depthTextures[i] = _gbufferRTs[i].depthRT;
    }

    _debugDrawer = std::make_unique<VulkanDebugDrawer>(this,
        DebugDrawSetupData {
            .extent = { Window.getWidth(), Window.getHeight() },
            .attachmentFormat = surfaceFormat,
            .transformDataBuffers = _cullingTransformBuffers,
            .objectAABBsBuffers = _cullingAABBsBuffers,
        });
}

VulkanRHI::~VulkanRHI()
{
    vkDeviceWaitIdle(_device);

    _debugDrawer.reset();

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

    vkDestroyFence(_device, _immediateAndUploadFence, nullptr);
    vkDestroyCommandPool(_device, _uploadCommandPool, nullptr);
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

void VulkanRHI::recordBufferUploadCommands(const std::function<void(VkCommandBuffer)>& recordFunction) const
{
    _uploadsToBeFlushed = true;
    recordFunction(_uploadCommandBuffer);
}

void VulkanRHI::flushUploadCommands() const
{
    vkEndCommandBuffer(_uploadCommandBuffer);
    submitCommandBuffer(_graphicsQueue, {}, {}, _uploadCommandBuffer, _immediateAndUploadFence);

    vkWaitForFences(_device, 1, &_immediateAndUploadFence, VK_TRUE, NH3D_MAX_T(uint64_t));
    vkResetFences(_device, 1, &_immediateAndUploadFence);

    NH3D_DEBUGLOG("Flushed upload commands");
    VulkanBuffer::flushedUploadCommands();
    _uploadsToBeFlushed = false;

    beginCommandBuffer(_uploadCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
}

void VulkanRHI::executeImmediateCommandBuffer(const std::function<void(VkCommandBuffer)>& recordFunction) const
{
    beginCommandBuffer(_immediateCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    recordFunction(_immediateCommandBuffer);

    vkEndCommandBuffer(_immediateCommandBuffer);
    submitCommandBuffer(_graphicsQueue, {}, {}, _immediateCommandBuffer, _immediateAndUploadFence);

    vkWaitForFences(_device, 1, &_immediateAndUploadFence, VK_TRUE, NH3D_MAX_T(uint64_t));
    vkResetFences(_device, 1, &_immediateAndUploadFence);
}

Handle<Texture> VulkanRHI::createTexture(const Texture::CreateInfo& info)
{
    Handle<Texture> texture = _textureManager.create(*this,
        {
            .format = MapTextureFormat(info.format),
            .extent = { info.size.x, info.size.y, info.size.z },
            .usageFlags = MapTextureUsageFlags(info.usage),
            .aspectFlags = MapTextureAspectFlags(info.aspect),
            .initialData = info.initialData,
            .generateMipMaps = info.generateMipMaps,
        });

    auto& descriptorSets = _bindGroupManager.get<DescriptorSets>(_albedoTextureBindGroup);
    const VkDescriptorImageInfo imageInfo {
        .sampler = VK_NULL_HANDLE,
        .imageView = _textureManager.get<ImageView>(texture).view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VulkanBindGroup::registerBufferedUpdate(descriptorSets, imageInfo, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, texture.index);

    return texture;
}

void VulkanRHI::destroyTexture(const Handle<Texture> handle) { _textureManager.release(*this, handle); }

Handle<Buffer> VulkanRHI::createBuffer(const Buffer::CreateInfo& info)
{
    return _bufferManager.create(*this,
        {
            .size = info.size,
            .usage = MapBufferUsageFlags(info.usageFlags),
            .memoryUsage = MapBufferMemoryUsage(info.memoryUsage),
            .initialData = info.initialData,
        });
}

void VulkanRHI::destroyBuffer(const Handle<Buffer> handle) { _bufferManager.release(*this, handle); }

// TODO: cache command buffer?
void VulkanRHI::render(Scene& scene) const
{
    if (_uploadsToBeFlushed) {
        flushUploadCommands();
    }

    if (scene.getMainCamera() == InvalidEntity) {
        return;
    }

    const uint32 frameInFlightId = _frameId % MaxFramesInFlight;
    if (vkWaitForFences(_device, 1, &_frameFences[frameInFlightId], VK_TRUE, NH3D_MAX_T(uint64)) != VK_SUCCESS) {
        NH3D_ABORT_VK("GPU stall detected");
    }
    vkResetFences(_device, 1, &_frameFences[frameInFlightId]);

    uint32 swapchainImageId;
    // TODO: handle resize
    if (vkAcquireNextImageKHR(_device, _swapchain, NH3D_MAX_T(uint64), _presentSemaphores[frameInFlightId], nullptr, &swapchainImageId)
        != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to acquire next swapchain image");
    }

    VkCommandBuffer commandBuffer = _commandBuffers[frameInFlightId];

    beginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // Buffer updates
    const GPUBuffer& transformBuffer = _bufferManager.get<GPUBuffer>(_cullingTransformBuffers[frameInFlightId]);
    const BufferAllocationInfo& transformAllocation = _bufferManager.get<BufferAllocationInfo>(_cullingTransformBuffers[frameInFlightId]);
    TransformComponent* transformDataPtr
        = reinterpret_cast<TransformComponent*>(VulkanBuffer::getMappedAddress(*this, transformAllocation));
    uint32 objectCount = 0;
    // TODO: track dirty state properly
    static bool dirtyRenderingData = true;
    if (dirtyRenderingData) {
        dirtyRenderingData = _frameId < MaxFramesInFlight;

        const GPUBuffer& objectDataStagingBuffer = _bufferManager.get<GPUBuffer>(_cullingRenderDataStagingBuffers[frameInFlightId]);
        const BufferAllocationInfo& objectDataStagingAllocation
            = _bufferManager.get<BufferAllocationInfo>(_cullingRenderDataStagingBuffers[frameInFlightId]);
        RenderData* objectDataPtr = reinterpret_cast<RenderData*>(VulkanBuffer::getMappedAddress(*this, objectDataStagingAllocation));

        const GPUBuffer& aabbStagingBuffer = _bufferManager.get<GPUBuffer>(_cullingAABBsStagingBuffers[frameInFlightId]);
        AABB* aabbDataPtr = reinterpret_cast<AABB*>(
            VulkanBuffer::getMappedAddress(*this, _bufferManager.get<BufferAllocationInfo>(_cullingAABBsStagingBuffers[frameInFlightId])));

        for (const auto& [entity, renderComponent, transformComponent] : scene.makeView<RenderComponent, TransformComponent>()) {
            RenderData objectData;

            const Mesh& mesh = renderComponent.getMesh();
            const VkBuffer vertexBuffer = _bufferManager.get<GPUBuffer>(mesh.vertexBuffer).buffer;
            const VkBuffer& indexBuffer = _bufferManager.get<GPUBuffer>(mesh.indexBuffer).buffer;
            // Buffers used as index/vertex buffers are assumed to be created with the exact size needed
            const uint32 indexBufferSize = _bufferManager.get<BufferAllocationInfo>(mesh.indexBuffer).allocatedSize;
            objectData.vertexBuffer = VulkanBuffer::getDeviceAddress(*this, vertexBuffer);
            objectData.indexBuffer = VulkanBuffer::getDeviceAddress(*this, indexBuffer);
            objectData.material = renderComponent.getMaterial();
            objectData.indexCount = indexBufferSize / sizeof(uint32);

            aabbDataPtr[objectCount] = mesh.objectAABB;
            objectDataPtr[objectCount] = objectData;
            transformDataPtr[objectCount] = transformComponent;
            objectCount++;
        }

        const GPUBuffer& aabbBuffer = _bufferManager.get<GPUBuffer>(_cullingAABBsBuffers[frameInFlightId]);
        VulkanBuffer::copyBuffer(commandBuffer, aabbStagingBuffer.buffer, aabbBuffer.buffer, sizeof(AABB) * objectCount);

        const GPUBuffer& objectDataBuffer = _bufferManager.get<GPUBuffer>(_cullingRenderDataBuffers[frameInFlightId]);
        VulkanBuffer::copyBuffer(commandBuffer, objectDataStagingBuffer.buffer, objectDataBuffer.buffer, sizeof(RenderData) * objectCount);

        VulkanBuffer::insertMemoryBarrier(commandBuffer, objectDataBuffer.buffer, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    } else {
        // Only update transforms
        // TODO: remove the need for RenderComponent here, it should only be used to filter
        for (const auto& [entity, _, transformComponent] : scene.makeView<RenderComponent, TransformComponent>()) {
            transformDataPtr[objectCount] = transformComponent;
            objectCount++;
        }
    }
    VulkanBuffer::flush(*this, transformAllocation);

    // Reset the culling draw counter
    const VkBuffer drawCountBuffer = _bufferManager.get<GPUBuffer>(_cullingDrawCounterBuffers[frameInFlightId]).buffer;
    vkCmdFillBuffer(commandBuffer, drawCountBuffer, 0, sizeof(uint32), 0);
    VulkanBuffer::insertMemoryBarrier(commandBuffer, drawCountBuffer, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    // Update visible flags buffer
    const uint32 visibleFlagsBufferSize = (objectCount + (sizeof(uint32) << 3) - 1) / (sizeof(uint32) << 3) * sizeof(uint32);
    const GPUBuffer& visibleFlagStagingBuffer = _bufferManager.get<GPUBuffer>(_cullingVisibleFlagStagingBuffers[frameInFlightId]);
    const BufferAllocationInfo& visibleFlagStagingAllocation
        = _bufferManager.get<BufferAllocationInfo>(_cullingVisibleFlagStagingBuffers[frameInFlightId]);
    const GPUBuffer& visibleFlagBuffer = _bufferManager.get<GPUBuffer>(_cullingVisibleFlagBuffers[frameInFlightId]);

    void* visibleFlagsPtr = VulkanBuffer::getMappedAddress(*this, visibleFlagStagingAllocation);
    std::memcpy(visibleFlagsPtr, scene.getRawVisibleFlags(), visibleFlagsBufferSize);
    VulkanBuffer::copyBuffer(commandBuffer, visibleFlagStagingBuffer.buffer, visibleFlagBuffer.buffer, visibleFlagsBufferSize);
    VulkanBuffer::insertMemoryBarrier(commandBuffer, visibleFlagBuffer.buffer, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    // Compute updated culling parameters
    const Entity mainCameraEntity = scene.getMainCamera();
    const CameraComponent& cameraSettings = scene.get<CameraComponent>(mainCameraEntity);
    const TransformComponent& cameraTransform = scene.get<TransformComponent>(mainCameraEntity);
    const VkExtent3D rtExtent = _textureManager.get<TextureMetadata>(_gbufferRTs[frameInFlightId].albedoRT).extent;
    const float aspectRatio = rtExtent.width / static_cast<float>(rtExtent.height);

    const mat4 projectionMatrix = perspective(cameraSettings.fovY, aspectRatio, cameraSettings.near, cameraSettings.far);
    const mat4 viewMatrix = inverse(mat4(cameraTransform)); // assumes scale is uniform and non-zero

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

    VulkanBindGroup::bind(commandBuffer, cullingDescriptorSets, VK_PIPELINE_BIND_POINT_COMPUTE, cullingPipelineLayout);

    // Update culling parameters buffer
    const CullingParameters cullingParameters {
        .viewMatrix = viewMatrix,
        .frustumPlanes = getFrustumPlanes(projectionMatrix),
        .objectCount = objectCount,
    };

    vkCmdPushConstants(commandBuffer, cullingPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullingParameters), &cullingParameters);

    const vec3i cullingKernelSize { std::ceil(objectCount / 64.f), 1, 1 };
    VulkanComputeShader::dispatch(commandBuffer, cullingPipeline, cullingKernelSize);
    VulkanBuffer::insertMemoryBarrier(commandBuffer, drawCountBuffer, VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);

    const GPUBuffer& drawIndirectBuffer = _bufferManager.get<GPUBuffer>(_drawIndirectBuffers[frameInFlightId]);
    VulkanBuffer::insertMemoryBarrier(commandBuffer, drawIndirectBuffer.buffer, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);

    const GPUBuffer& drawRecordBuffer = _bufferManager.get<GPUBuffer>(_drawRecordBuffers[frameInFlightId]);
    VulkanBuffer::insertMemoryBarrier(commandBuffer, drawRecordBuffer.buffer, VK_ACCESS_2_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);

    auto graphicsPipeline = _shaderManager.get<VkPipeline>(_gbufferShader);
    auto graphicsLayout = _shaderManager.get<VkPipelineLayout>(_gbufferShader);

    const VkDescriptorSet graphicsDescriptorSets[] = {
        VulkanBindGroup::getUpdatedDescriptorSet(_device, _bindGroupManager.get<DescriptorSets>(_albedoTextureBindGroup), frameInFlightId),
        VulkanBindGroup::getUpdatedDescriptorSet(_device, _bindGroupManager.get<DescriptorSets>(_drawRecordBindGroup), frameInFlightId),
    };
    VulkanBindGroup::bind(commandBuffer, graphicsDescriptorSets, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsLayout);

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
    VulkanTexture::clearDepth(commandBuffer, depthRTViewData.image, 1.0f, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); // 0 for reverse Z?
    VulkanTexture::insertMemoryBarrier(commandBuffer, depthRTViewData.image, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_CLEAR_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);

    vkCmdPushConstants(commandBuffer, graphicsLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4), &projectionMatrix);

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

    NH3D_ASSERT(objectCount <= _bufferManager.get<BufferAllocationInfo>(_drawIndirectBuffers[frameInFlightId]).allocatedSize
                / sizeof(VkDrawIndirectCommand),
        "Draw indirect buffer too small for the number of objects");
    VulkanShader::multiDrawIndirect(commandBuffer, graphicsPipeline, {
                .drawIndirectBuffer = drawIndirectBuffer.buffer,
                .drawIndirectCountBuffer = drawCountBuffer,
                .maxDrawCount = objectCount, // Good enough for now
                .drawParams = { 
                    .extent = { albedoRTMetadata.extent.width, albedoRTMetadata.extent.height },
                    .colorAttachments = colorAttachmentsInfo,
                    .depthAttachment = {
                        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .imageView = depthRTViewData.view,
                        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    },
                },
            });

    VulkanTexture::insertMemoryBarrier(commandBuffer, normalRTImageViewData.image, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VulkanTexture::insertMemoryBarrier(commandBuffer, albedoRTImageViewData.image, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    constexpr bool EnableDebugDraw = true;
    VkAccessFlags2 depthRtDstAccess = VK_ACCESS_2_SHADER_READ_BIT;
    VkPipelineStageFlags2 depthRtDstStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    if (EnableDebugDraw) {
        // If debug drawing is enabled, we need to transition the depth RT back to depth attachment layout for the AABB drawer
        // Note: assumes read only depth
        depthRtDstAccess |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        depthRtDstStage |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    }

    VulkanTexture::insertMemoryBarrier(commandBuffer, depthRTViewData.image, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, depthRtDstAccess, depthRtDstStage,
        VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, true);

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
    VulkanBindGroup::bind(commandBuffer, shadingDescriptorSets, VK_PIPELINE_BIND_POINT_COMPUTE, deferredShadingPipelineLayout);
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

    if (EnableDebugDraw) {
        VulkanTexture::insertMemoryBarrier(commandBuffer, scImageViewData.image, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        // _debugDrawer->renderAABBs(commandBuffer, frameInFlightId, viewMatrix, projectionMatrix, objectCount,
        //     _gbufferRTs[frameInFlightId].depthRT, _swapchainTextures[swapchainImageId]);

        // VulkanTexture::insertMemoryBarrier(commandBuffer, scImageViewData.image, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        //     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        //     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        //     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        _debugDrawer->renderDebugUI(commandBuffer, frameInFlightId, _swapchainTextures[swapchainImageId]);

        VulkanTexture::insertMemoryBarrier(commandBuffer, scImageViewData.image, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else {
        VulkanTexture::insertMemoryBarrier(commandBuffer, scImageViewData.image, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }

    vkEndCommandBuffer(commandBuffer);
    submitCommandBuffer(_graphicsQueue, makeSemaphoreSubmitInfo(_presentSemaphores[frameInFlightId], VK_PIPELINE_STAGE_2_BLIT_BIT),
        makeSemaphoreSubmitInfo(_renderSemaphores[swapchainImageId], VK_PIPELINE_STAGE_2_BLIT_BIT), commandBuffer,
        _frameFences[frameInFlightId]);

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

    // VkValidationFeatureEnableEXT enables[]
    //     = { VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT };
    // VkValidationFeaturesEXT features = {};
    // features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    // features.enabledValidationFeatureCount = std::size(enables);
    // features.pEnabledValidationFeatures = enables;
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
    uint32 physicalDeviceCount;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

    std::vector<VkPhysicalDevice> availableGpus { physicalDeviceCount };
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, availableGpus.data());

    PhysicalDeviceQueueFamilyID queues {};
    std::vector<VkQueueFamilyProperties> queueData { 16 };

    std::string selectedDeviceName;
    uint32 deviceId = NH3D_MAX_T(uint32);

    const VkQueueFlags requiredQueueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

    for (uint32 i = 0; i < physicalDeviceCount; ++i) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(availableGpus[i], &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(availableGpus[i], &features);

        if (features.multiDrawIndirect
            && (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                || (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && deviceId == NH3D_MAX_T(uint32)))) {
            uint32 familyCount;
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

    if (deviceId == NH3D_MAX_T(uint32)) {
        NH3D_ABORT_VK("Couldn't find a suitable physical device");
    }

    VkPhysicalDevice gpu = availableGpus[deviceId];

    NH3D_LOG("Selected Vulkan device: " << selectedDeviceName);

    return { availableGpus[deviceId], queues };
}

VkDevice VulkanRHI::createLogicalDevice(const VkPhysicalDevice gpu, const PhysicalDeviceQueueFamilyID queues) const
{
    std::unordered_set<uint32> queueIndices { queues.GraphicsQueueFamilyID, queues.PresentQueueFamilyID };

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
        .storageBuffer16BitAccess = VK_TRUE,
        .shaderDrawParameters = VK_TRUE,
    };
    VkPhysicalDeviceVulkan12Features features12 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features11,
        .drawIndirectCount = VK_TRUE,
        .storageBuffer8BitAccess = VK_TRUE,
        .descriptorIndexing = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        .scalarBlockLayout = VK_TRUE,
        .uniformBufferStandardLayout = VK_TRUE,
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

    // From Vulkan samples
    uint32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes { presentModeCount };
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, presentModes.data());

    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32 i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
        if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
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
        .presentMode = swapchainPresentMode,
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

VulkanRHI::FrustumPlanes VulkanRHI::getFrustumPlanes(const mat4& projectionMatrix) const
{
    FrustumPlanes frustumPlanes;

    // Dropping the Y component for left and right planes as it is zero, and the X component for top and bottom planes as it is zero

    frustumPlanes.left = vec2 {
        projectionMatrix[0][3] + projectionMatrix[0][0],
        // projectionMatrix[1][3] + projectionMatrix[1][0],
        projectionMatrix[2][3] + projectionMatrix[2][0],
    };

    frustumPlanes.right = vec2 {
        projectionMatrix[0][3] - projectionMatrix[0][0],
        // projectionMatrix[1][3] - projectionMatrix[1][0],
        projectionMatrix[2][3] - projectionMatrix[2][0],
    };

    frustumPlanes.top = vec2 {
        // projectionMatrix[0][3] - projectionMatrix[0][1],
        projectionMatrix[1][3] + projectionMatrix[1][1], // sign flipped due to Vulkan NDC
        projectionMatrix[2][3] - projectionMatrix[2][1],
    };

    frustumPlanes.bottom = vec2 {
        // projectionMatrix[0][3] + projectionMatrix[0][1],
        projectionMatrix[1][3] - projectionMatrix[1][1], // sign flipped due to Vulkan NDC
        projectionMatrix[2][3] + projectionMatrix[2][1],
    };

    return frustumPlanes;
}

}