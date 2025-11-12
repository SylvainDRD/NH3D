#include "vulkan_pipeline.hpp"
#include <rendering/vulkan/vulkan_rhi.hpp>

namespace NH3D {

void VulkanPipeline::release(const IRHI& rhi, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout)
{
    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);
    if (pipelineLayout) {
        vkDestroyPipelineLayout(vrhi.getVkDevice(), pipelineLayout, nullptr);
        pipelineLayout = nullptr;
    }

    if (pipeline) {
        vkDestroyPipeline(vrhi.getVkDevice(), pipeline, nullptr);
        pipeline = nullptr;
    }
}
}