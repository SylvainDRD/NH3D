#include "vulkan_rhi.hpp"
// #include <components/mesh_component.hpp>
#include <cmath>
#include <cstdint>
#include <general/window.hpp>
#include <initializer_list>
#include <memory>
#include <misc/utils.hpp>
#include <rendering/render_graph/render_graph.hpp>
#include <rendering/vulkan/vulkan_descriptor_set_pool.hpp>
#include <rendering/vulkan/vulkan_compute_pipeline.hpp>
#include <rendering/vulkan/vulkan_graphics_pipeline.hpp>
#include <rendering/vulkan/vulkan_texture.hpp>
#include <sys/types.h>
#include <unordered_set>
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace NH3D {

VulkanRHI::VulkanRHI(const Window& Window)
    : IRHI {}
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

    // The extent provided should match the surface, hopefully glfw
    auto [swapchain, surfaceFormat] = createSwapchain(_device, _gpu, _surface, queues, { Window.getWidth(), Window.getHeight() });
    _swapchain = swapchain;

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(_device, _swapchain, &swapchainImageCount, nullptr);

    std::vector<VkImage> swapchainImages;
    swapchainImages.resize(swapchainImageCount);
    _swapchainTextures.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(_device, _swapchain, &swapchainImageCount, swapchainImages.data());

    for (int i = 0; i < swapchainImageCount; ++i) {
        _swapchainTextures[i] = _textures.allocate(this, swapchainImages[i], surfaceFormat, VkExtent3D { Window.getWidth(), Window.getHeight(), 1 }, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    _commandPool = createCommandPool(_device, queues.GraphicsQueueFamilyID);

    allocateCommandBuffers(_device, _commandPool, MaxFramesInFlight, _commandBuffers.data());

    _renderSemaphores.resize(swapchainImageCount);
    for (uint32_t i = 0; i < _renderSemaphores.size(); ++i) {
        _renderSemaphores[i] = createSemaphore(_device);
    }

    for (uint32_t i = 0; i < MaxFramesInFlight; ++i) {
        _presentSemaphores[i] = createSemaphore(_device);
        _frameFences[i] = createFence(_device);
        _renderTargets[i] = _textures.allocate(
            this,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VkExtent3D { Window.getWidth(), Window.getHeight(), 1 },
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
    }

    _descriptorSetPoolCompute = std::make_unique<VulkanDescriptorSetPool<MaxFramesInFlight>>(_device,
        VK_SHADER_STAGE_COMPUTE_BIT,
        std::initializer_list<VkDescriptorType> { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
        1000);

    _computePipeline = std::make_unique<VulkanComputePipeline>(
        _device,
        _descriptorSetPoolCompute->getLayout(),
        PROJECT_DIR "src/rendering/shaders/.cache/gradient.comp.spv");

    _graphicsPipeline = std::make_unique<VulkanGraphicsPipeline>(_device,
        nullptr,
        VulkanGraphicsPipeline::ShaderData {
            .vertexShaderPath = PROJECT_DIR "src/rendering/shaders/.cache/triangle.vert.spv",
            .fragmentShaderPath = PROJECT_DIR "src/rendering/shaders/.cache/triangle.frag.spv",
            .colorAttachmentFormats { _textures.getResource(_renderTargets[0]).getFormat() } });

    // MeshComponent<VulkanRHI> mesh { *this, {}, {} };
}

VulkanRHI::~VulkanRHI()
{
    vkDeviceWaitIdle(_device);

    _descriptorSetPoolCompute->releasePool(_device);
    _computePipeline->release(_device);
    _graphicsPipeline->release(_device);

    _textures.clear(*this);
    _buffers.clear(*this);

    for (uint32_t i = 0; i < MaxFramesInFlight; ++i) {
        vkDestroyFence(_device, _frameFences[i], nullptr);
        vkDestroySemaphore(_device, _presentSemaphores[i], nullptr);
    }

    for (uint32_t i = 0; i < _renderSemaphores.size(); ++i) {
        vkDestroySemaphore(_device, _renderSemaphores[i], nullptr);
    }

    vkDestroyCommandPool(_device, _commandPool, nullptr);

    vmaDestroyAllocator(_allocator);

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    vkDestroyDevice(_device, nullptr);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);

#if NH3D_DEBUG
    PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT"));
    destroyDebugMessenger(_instance, _debugUtilsMessenger, nullptr);
#endif

    vkDestroyInstance(_instance, nullptr);

    NH3D_LOG("Vulkan objects cleanup completed");
}

void VulkanRHI::render(const RenderGraph& rdag) const
{
    // rdag.render<VulkanRHI>(this);

    const uint32_t frameInFlightId = _frameId % MaxFramesInFlight;

    if (vkWaitForFences(_device, 1, &_frameFences[frameInFlightId], VK_TRUE, NH3D_MAX_T(uint64_t)) != VK_SUCCESS) {
        NH3D_ABORT_VK("GPU stall detected");
    }
    vkResetFences(_device, 1, &_frameFences[frameInFlightId]);

    uint32_t swapchainImageId;
    if (vkAcquireNextImageKHR(_device, _swapchain, NH3D_MAX_T(uint64_t), _presentSemaphores[frameInFlightId], nullptr, &swapchainImageId) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to acquire next swapchain image");
    }

    VkCommandBuffer commandBuffer = getCommandBuffer();

    beginCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VulkanTexture& renderTarget = _textures.getResource(_renderTargets[frameInFlightId]);
    renderTarget.insertBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);

    VkDescriptorSet descriptorSet = _descriptorSetPoolCompute->getDescriptorSet(_device, frameInFlightId);

    // update DS, bind pipeline, bind DS, dispatch
    VkDescriptorImageInfo imageInfo {
        .imageView = renderTarget.getView(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    _descriptorSetPoolCompute->updateDescriptorSet(_device, commandBuffer, descriptorSet, _computePipeline->getLayout(), imageInfo);
    _descriptorSetPoolCompute->bind(commandBuffer, descriptorSet, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline->getLayout());
    _computePipeline->dispatch(commandBuffer, { std::ceil(renderTarget.getWidth() / 8.f), std::ceil(renderTarget.getHeight() / 8.f), 1 });

    renderTarget.insertBarrier(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // TODO: watch for the tiny vector allocations
    _graphicsPipeline->draw(commandBuffer,
        { renderTarget.getWidth(), renderTarget.getHeight() },
        { renderTarget.getAttachmentInfo() });

    renderTarget.insertBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VulkanTexture& swapchainImage = _textures.getResource(_swapchainTextures[swapchainImageId]);
    swapchainImage.insertBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    renderTarget.blit(commandBuffer, swapchainImage);

    swapchainImage.insertBarrier(commandBuffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(commandBuffer);
    submitCommandBuffer(_graphicsQueue,
        makeSemaphoreSubmitInfo(_presentSemaphores[frameInFlightId], VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR),
        makeSemaphoreSubmitInfo(_renderSemaphores[swapchainImageId], VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT),
        commandBuffer,
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

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanValidationCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        NH3D_WARN_VK(pCallbackData->pMessage);
    } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        NH3D_ERROR_VK(pCallbackData->pMessage);
    }

    return VK_FALSE;
}

VkInstance VulkanRHI::createVkInstance(std::vector<const char*>&& requiredWindowExtensions) const
{
    VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = PROJECT_NAME,
        .applicationVersion = VK_MAKE_VERSION(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH),
        .pEngineName = PROJECT_NAME,
        .engineVersion = VK_MAKE_VERSION(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH),
        .apiVersion = VK_API_VERSION_1_3
    };

    std::vector<const char*> requiredExtensions = std::move(requiredWindowExtensions);
#if NH3D_DEBUG
    requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
#endif

    VkInstanceCreateInfo vkInstanceCreateInfo {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pApplicationInfo = &appInfo,
#if NH3D_DEBUG
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &validationLayerName,
#endif
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()
    };

    VkInstance instance;

    if (vkCreateInstance(&vkInstanceCreateInfo, nullptr, &instance) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan instance");
    }

    NH3D_LOG("Vulkan instance successfully created");
    return instance;
}

VkDebugUtilsMessengerEXT VulkanRHI::createDebugMessenger(VkInstance instance) const
{
    PFN_vkCreateDebugUtilsMessengerEXT createDebugMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

    VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
        .pfnUserCallback = vulkanValidationCallback
    };

    VkDebugUtilsMessengerEXT debugMessenger;
    if (createDebugMessenger(instance, &messengerCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan debug utils messenger");
    }

    NH3D_LOG("Vulkan debug messenger successfully created");
    return debugMessenger;
}

std::pair<VkPhysicalDevice, VulkanRHI::PhysicalDeviceQueueFamilyID> VulkanRHI::selectPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) const
{
    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

    std::vector<VkPhysicalDevice> availableGpus { physicalDeviceCount };
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, availableGpus.data());

    PhysicalDeviceQueueFamilyID queues {};
    std::vector<VkQueueFamilyProperties> queueData { 16 };

    std::string selectedDeviceName;
    uint32_t deviceId = NH3D_MAX_T(uint32_t);

    for (uint32_t i = 0; i < physicalDeviceCount; ++i) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(availableGpus[i], &properties);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            || (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && deviceId == NH3D_MAX_T(uint32_t))) {
            uint32_t familyCount;
            vkGetPhysicalDeviceQueueFamilyProperties(availableGpus[i], &familyCount, nullptr);

            queueData.resize(familyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(availableGpus[i], &familyCount, queueData.data());

            queues = {};
            for (int32_t queueId = 0; queueId < familyCount; ++queueId) {
                if (queues.GraphicsQueueFamilyID == NH3D_MAX_T(decltype(queues.GraphicsQueueFamilyID)) && queueData[queueId].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
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

VkDevice VulkanRHI::createLogicalDevice(VkPhysicalDevice gpu, PhysicalDeviceQueueFamilyID queues) const
{
    std::unordered_set<uint32_t> queueIndices { queues.GraphicsQueueFamilyID, queues.PresentQueueFamilyID };

    const float priority = 1.f;
    std::vector<VkDeviceQueueCreateInfo> queuesCreateInfo {};
    VkDeviceQueueCreateInfo queueCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueCount = 1,
        .pQueuePriorities = &priority
    };
    for (uint32_t queueID : queueIndices) {
        queueCreateInfo.queueFamilyIndex = queueID;

        queuesCreateInfo.emplace_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures features {};
    VkPhysicalDeviceVulkan12Features features12 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE
    };
    VkPhysicalDeviceVulkan13Features features13 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &features12,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };

    const char* swapchainExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    VkDeviceCreateInfo deviceCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features13,
        .queueCreateInfoCount = static_cast<uint32_t>(queuesCreateInfo.size()),
        .pQueueCreateInfos = queuesCreateInfo.data(),
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &swapchainExt,
        .pEnabledFeatures = &features
    };

    VkDevice device;
    if (vkCreateDevice(gpu, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan device creation failed");
    }

    NH3D_LOG("Vulkan device created successfully");

    return device;
}

std::pair<VkSwapchainKHR, VkFormat> VulkanRHI::createSwapchain(VkDevice device, VkPhysicalDevice gpu, VkSurfaceKHR surface, PhysicalDeviceQueueFamilyID queues, VkExtent2D extent, VkSwapchainKHR previousSwapchain) const
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surfaceCapabilities);

    uint32_t formatsCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatsCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats { formatsCount };
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatsCount, surfaceFormats.data());

    VkFormat preferredFormats[] = {
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB
    };

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
        .minImageCount = surfaceCapabilities.maxImageCount > 0 && surfaceCapabilities.minImageCount + 1 > surfaceCapabilities.maxImageCount ? surfaceCapabilities.maxImageCount : surfaceCapabilities.minImageCount + 1,
        .imageFormat = preferredFormats[formatId],
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = previousSwapchain
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

VkCommandPool VulkanRHI::createCommandPool(VkDevice device, uint32_t queueFamilyIndex) const
{
    VkCommandPoolCreateInfo commandPoolCreateInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };

    VkCommandPool commandPool;
    if (vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan command pool creation failed");
    }

    return commandPool;
}

void VulkanRHI::allocateCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t bufferCount, VkCommandBuffer* buffers) const
{
    VkCommandBufferAllocateInfo cbAllocInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = bufferCount
    };

    if (vkAllocateCommandBuffers(device, &cbAllocInfo, buffers) != VK_SUCCESS) {
        NH3D_ABORT_VK("Vulkan command buffers allocation failed");
    }
}

VkSemaphore VulkanRHI::createSemaphore(VkDevice device) const
{
    VkSemaphoreCreateInfo semCreateInfo {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkSemaphore semaphore;
    if (vkCreateSemaphore(device, &semCreateInfo, nullptr, &semaphore) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan semaphore");
    }

    return semaphore;
}

VkFence VulkanRHI::createFence(VkDevice device) const
{
    VkFenceCreateInfo fenceCreateInfo {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VkFence fence;
    if (vkCreateFence(device, &fenceCreateInfo, nullptr, &fence) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan fence");
    }

    return fence;
}

void VulkanRHI::beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags, bool resetCommandBuffer) const
{
    VkCommandBufferBeginInfo cbBeginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags,
        .pInheritanceInfo = nullptr
    };

    if (resetCommandBuffer) {
        vkResetCommandBuffer(commandBuffer, 0);
    }

    vkBeginCommandBuffer(commandBuffer, &cbBeginInfo);
}

VkSemaphoreSubmitInfo VulkanRHI::makeSemaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2 stageMask) const
{
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = semaphore,
        .value = 1,
        .stageMask = stageMask,
        .deviceIndex = 0
    };
}

void VulkanRHI::submitCommandBuffer(VkQueue queue, const VkSemaphoreSubmitInfo& waitSemaphore, const VkSemaphoreSubmitInfo& signalSemaphore, VkCommandBuffer commandBuffer, VkFence fence) const
{
    VkCommandBufferSubmitInfo cbSubmitInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commandBuffer,
        .deviceMask = 0
    };

    VkSubmitInfo2 submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = waitSemaphore.semaphore ? 1u : 0u,
        .pWaitSemaphoreInfos = &waitSemaphore,
        .commandBufferInfoCount = commandBuffer ? 1u : 0u,
        .pCommandBufferInfos = &cbSubmitInfo,
        .signalSemaphoreInfoCount = signalSemaphore.semaphore ? 1u : 0u,
        .pSignalSemaphoreInfos = &signalSemaphore
    };

    vkQueueSubmit2(queue, 1, &submitInfo, fence);
}

VmaAllocator VulkanRHI::createVMAAllocator(VkInstance instance, VkPhysicalDevice gpu, VkDevice device) const
{
    VmaAllocatorCreateInfo allocatorCreateInfo {
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .physicalDevice = gpu,
        .device = device,
        .instance = instance,
        .vulkanApiVersion = VK_API_VERSION_1_3
    };

    VmaAllocator allocator;
    if (vmaCreateAllocator(&allocatorCreateInfo, &allocator) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create VMA allocator");
    }

    return allocator;
}

}