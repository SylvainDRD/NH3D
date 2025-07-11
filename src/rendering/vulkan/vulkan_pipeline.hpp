#pragma once

#include <filesystem>
#include <misc/utils.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

class VulkanPipeline {
    NH3D_NO_COPY_MOVE(VulkanPipeline)
public:
    VulkanPipeline(VkDevice device, VkDescriptorSetLayout layout);

    virtual ~VulkanPipeline() = default;

    void release(VkDevice device);

    [[nodiscard]]
    VkPipelineLayout getLayout() const
    {
        return _pipelineLayout;
    }

    [[nodiscard]]
    bool isValid() const
    {
        return _pipeline != nullptr;
    }

protected:
    bool loadShader(VkDevice device, const std::filesystem::path& path, VkShaderModule& module) const;

protected:
    VkPipeline _pipeline = nullptr;
    VkPipelineLayout _pipelineLayout;

    std::vector<VkShaderModule> _shaderModules;
};

}