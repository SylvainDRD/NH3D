#include "vulkan_rhi.hpp"
#include <cmath>
#include <cstdint>
#include <general/window.hpp>
#include <initializer_list>
#include <memory>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/buffer.hpp>
#include <rendering/core/resource_manager.hpp>
#include <rendering/vulkan/vulkan_enums.hpp>
#include <scene/ecs/components/render_component.hpp>
#include <scene/scene.hpp>
#include <unordered_set>
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace NH3D {

// TODO: delete
struct DrawRecord {
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress indexBuffer;
    Material material;
    mat4x3 modelMatrix;
};

VulkanRHI::VulkanRHI(const Window& Window)
    : IRHI {}
    , _textureManager { 1000, 100 }
    , _bufferManager { 10000, 400 }
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

    const VkFormat renderTargetsFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    for (uint32_t i = 0; i < MaxFramesInFlight; ++i) {
        _presentSemaphores[i] = createSemaphore(_device);
        _frameFences[i] = createFence(_device, true);

        _renderTargets[i] = createTexture({
            .format = renderTargetsFormat,
            .extent = { Window.getWidth(), Window.getHeight(), 1 },
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            .createSampler = true, // TODO: evaluate that, but I suspect post processing will sample from it and render to the swapchain
        });
    }

    const VkDescriptorType storageImageType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    auto [computeDescriptorSets, computeMetadata] = VulkanBindGroup::create(_device,
        {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .bindingTypes = { &storageImageType, 1 },
        });
    _computeBindGroup = _bindGroupManager.store(std::move(computeDescriptorSets), std::move(computeMetadata));

    const VkDescriptorType storageBufferType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    auto [drawRecordDescriptorSets, drawRecordMetadata] = VulkanBindGroup::create(_device,
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .bindingTypes = { &storageBufferType, 1 },
        });
    _drawRecordBindGroup = _bindGroupManager.store(std::move(drawRecordDescriptorSets), std::move(drawRecordMetadata));

    // TODO: make GPU only
    const VkDrawIndirectCommand drawIndirectCommand = {
        .vertexCount = 3,
        .instanceCount = 1,
    };
    auto [drawIndirectBuffer, drawIndirectAllocation] = VulkanBuffer::create(*this,
        {
            .size = sizeof(VkDrawIndirectCommand),
            .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
            .initialData = { reinterpret_cast<const byte*>(&drawIndirectCommand), sizeof(drawIndirectCommand) },
        });
    _drawIndirectBuffer = _bufferManager.store(std::move(drawIndirectBuffer), std::move(drawIndirectAllocation));

    // TODO: make GPU only
    auto [drawRecordBuffer, drawRecordAllocation] = VulkanBuffer::create(*this,
        {
            .size = sizeof(DrawRecord) * 1,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        });
    const VkDescriptorBufferInfo bufferInfo { .buffer = drawRecordBuffer, .offset = 0, .range = VK_WHOLE_SIZE };
    for (uint i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        VulkanBindGroup::updateDescriptorSet(_device, drawRecordDescriptorSets.sets[i], bufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
    }
    _drawRecordBuffer = _bufferManager.store(std::move(drawRecordBuffer), std::move(drawRecordAllocation));

    const VkDescriptorType textureBindingType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    auto [textureDescriptorSets, textureMetadata] = VulkanBindGroup::create(_device,
        {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .bindingTypes = { &textureBindingType, 1 },
            .finalBindingCount = 1000,
        });
    _textureBindGroup = _bindGroupManager.store(std::move(textureDescriptorSets), std::move(textureMetadata));

    auto [computeShader, computeLayout] = VulkanComputeShader::create(_device,
        {
            .computeShaderPath = NH3D_DIR "src/rendering/shaders/gradient.comp.spv",
            .descriptorSetsLayouts = { &computeMetadata.layout, 1 },
        });
    _computeShader = _computeShaderManager.store(std::move(computeShader), std::move(computeLayout));

    const VkPushConstantRange pushConstantRange { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(mat4) };

    // TODO: also watch out for tiny vector allocs, though it's probably fine since it's only at load time
    const VkDescriptorSetLayout layouts[] = { textureMetadata.layout, drawRecordMetadata.layout };
    auto [shader, layout] = VulkanShader::create(_device,
        {
            .vertexShaderPath = NH3D_DIR "src/rendering/shaders/triangle.vert.spv",
            .fragmentShaderPath = NH3D_DIR "src/rendering/shaders/triangle.frag.spv",
            .colorAttachmentFormats = { renderTargetsFormat },
            .descriptorSetsLayouts = { layouts, 2 },
            .pushConstantRanges = { &pushConstantRange, 1 },
        });
    _graphicsShader = _shaderManager.store(std::move(shader), std::move(layout));
}

VulkanRHI::~VulkanRHI()
{
    vkDeviceWaitIdle(_device);

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

// TODO: pass data
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

    const auto& rtImageViewData = _textureManager.get<ImageView>(_renderTargets[frameInFlightId]);
    auto& rtMetadata = _textureManager.get<TextureMetadata>(_renderTargets[frameInFlightId]);
    VulkanTexture::changeLayoutBarrier(commandBuffer, rtImageViewData.image, rtMetadata.layout, VK_IMAGE_LAYOUT_GENERAL);

    const auto& computeDescriptorSets = _bindGroupManager.get<DescriptorSets>(_computeBindGroup);
    VkDescriptorSet descriptorSet = VulkanBindGroup::getUpdatedDescriptorSet(_device, computeDescriptorSets, frameInFlightId);

    // TODO: cleanup
    const RenderComponent& mesh = scene.get<RenderComponent>(Entity { 0 });
    if (_frameId == 0) {
        DrawRecord drawRecord {};
        drawRecord.vertexBuffer = VulkanBuffer::getDeviceAddress(*this, _bufferManager.get<VkBuffer>(mesh.getVertexBuffer()));
        drawRecord.indexBuffer = VulkanBuffer::getDeviceAddress(*this, _bufferManager.get<VkBuffer>(mesh.getIndexBuffer()));
        drawRecord.material.albedo = vec3 { 1.0f, 0.0f, 0.0f };
        drawRecord.modelMatrix = mat4x3(1.0f);

        const auto& allocationInfo = _bufferManager.get<BufferAllocationInfo>(_drawRecordBuffer);
        void* mappedAddress = VulkanBuffer::getMappedAddress(*this, allocationInfo);
        std::memcpy(mappedAddress, &drawRecord, sizeof(DrawRecord));
        VulkanBuffer::flush(*this, allocationInfo);
    }

    auto graphicsPipeline = _shaderManager.get<VkPipeline>(_graphicsShader);
    auto graphicsLayout = _shaderManager.get<VkPipelineLayout>(_graphicsShader);

    // TODO: wrap push constant in pipeline + update view projection matrix
    // const VkDeviceAddress meshVertexBufferAddress
    //     = VulkanBuffer::getDeviceAddress(*this, _bufferManager.get<VkBuffer>(mesh.getVertexBuffer()));
    // vkCmdPushConstants(commandBuffer, graphicsLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &meshVertexBufferAddress);

    auto computePipeline = _computeShaderManager.get<VkPipeline>(_computeShader);
    auto computePipelineLayout = _computeShaderManager.get<VkPipelineLayout>(_computeShader);

    // update DS, bind pipeline, bind DS, dispatch
    const VkDescriptorImageInfo imageInfo { .imageView = rtImageViewData.view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };

    VulkanBindGroup::updateDescriptorSet(_device, descriptorSet, imageInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    VulkanBindGroup::bind(commandBuffer, { &descriptorSet, 1 }, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout);

    const vec3i kernelSize { std::ceil(rtMetadata.extent.width / 8.f), std::ceil(rtMetadata.extent.height / 8.f), 1 };
    VulkanComputeShader::dispatch(commandBuffer, computePipeline, kernelSize);

    VulkanTexture::changeLayoutBarrier(commandBuffer, rtImageViewData.image, rtMetadata.layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    const VkDescriptorSet graphicsDescriptorSets[] = {
        VulkanBindGroup::getUpdatedDescriptorSet(_device, _bindGroupManager.get<DescriptorSets>(_textureBindGroup), frameInFlightId),
        VulkanBindGroup::getUpdatedDescriptorSet(_device, _bindGroupManager.get<DescriptorSets>(_drawRecordBindGroup), frameInFlightId),
    };
    VulkanBindGroup::bind(
        commandBuffer, { graphicsDescriptorSets, std::size(graphicsDescriptorSets) }, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsLayout);
    const VkRenderingAttachmentInfo colorAttachmentInfo {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = rtImageViewData.view,
        .imageLayout = rtMetadata.layout,
    };
    const VkBuffer drawIndirectBuffer = _bufferManager.get<VkBuffer>(_drawIndirectBuffer);
    VulkanShader::draw(commandBuffer, graphicsPipeline,
        {
            .drawIndirectBuffer = drawIndirectBuffer,
            .extent = { rtMetadata.extent.width, rtMetadata.extent.height },
            .colorAttachments = { &colorAttachmentInfo, 1 },
        });

    VulkanTexture::changeLayoutBarrier(commandBuffer, rtImageViewData.image, rtMetadata.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    const auto& scImageViewData = _textureManager.get<ImageView>(_swapchainTextures[swapchainImageId]);
    auto& scMetadata = _textureManager.get<TextureMetadata>(_swapchainTextures[swapchainImageId]);

    VulkanTexture::changeLayoutBarrier(commandBuffer, scImageViewData.image, scMetadata.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VulkanTexture::blit(commandBuffer, rtImageViewData.image, rtMetadata.extent, scImageViewData.image, scMetadata.extent);

    VulkanTexture::changeLayoutBarrier(commandBuffer, scImageViewData.image, scMetadata.layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(commandBuffer);
    submitCommandBuffer(_graphicsQueue,
        makeSemaphoreSubmitInfo(_presentSemaphores[frameInFlightId], VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR),
        makeSemaphoreSubmitInfo(_renderSemaphores[swapchainImageId], VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT), commandBuffer,
        _frameFences[frameInFlightId]);

    VkPresentInfoKHR presentInfo {
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
}