#include "vulkan_pipeline.hpp"
#include <rendering/vulkan/vulkan_rhi.hpp>

namespace NH3D {

void VulkanPipeline::release(const IRHI& rhi, Pipeline& pipeline, PipelineLayout& pipelineLayout)
{
    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);
    if (pipelineLayout.layout) {
        vkDestroyPipelineLayout(vrhi.getVkDevice(), pipelineLayout.layout, nullptr);
        pipelineLayout.layout = nullptr;
    }

    if (pipeline.pipeline) {
        vkDestroyPipeline(vrhi.getVkDevice(), pipeline.pipeline, nullptr);
        pipeline.pipeline = nullptr;
    }
}
}