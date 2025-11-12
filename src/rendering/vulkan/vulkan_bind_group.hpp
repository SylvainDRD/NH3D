#pragma once

#include <cstdint>
#include <initializer_list>
#include <misc/utils.hpp>
#include <rendering/core/bind_group.hpp>
#include <rendering/core/rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

struct VulkanBindGroup {
    using ResourceType = BindGroup;

    struct DescriptorSets {
        std::array<VkDescriptorSet, IRHI::MaxFramesInFlight> sets;
    };
    using Hot = DescriptorSets;

    struct Metadata {
        VkDescriptorSetLayout layout;
        VkDescriptorPool pool;
    };
    using Cold = Metadata;

    /// "Constructor / Destructor"
    [[nodiscard]] static std::pair<DescriptorSets, Metadata> create(
        VkDevice device, const VkShaderStageFlags stageFlags, const std::initializer_list<VkDescriptorType>& bindingTypes);

    // Used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, DescriptorSets& descriptorSets, Metadata& cold);

    // Used generically by the ResourceManager, must be API agnostic
    static inline bool valid(const DescriptorSets& descriptorSets, const Metadata& cold)
    {
        return descriptorSets.sets[0] != nullptr && cold.layout != nullptr && cold.pool != nullptr;
    }

    /// Helper functions
    [[nodiscard]] static VkDescriptorSet getDescriptorSet(
        const VkDevice device, const DescriptorSets& descriptorSets, const uint32_t frameInFlightId);

    static inline void bind(VkCommandBuffer commandBuffer, const VkDescriptorSet descriptorSet, const VkPipelineBindPoint bindPoint,
        const VkPipelineLayout pipelineLayout)
    {
        vkCmdBindDescriptorSets(commandBuffer, bindPoint, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    }

    // TODO: buffer write to make sure the set is not in used while updating
    template <typename T> static inline void updateDescriptorSets(VkDevice device, const DescriptorSets& descriptorSets, const T& info)
    {
        VkWriteDescriptorSet descWrite;

        // TODO: generalize the descriptor type, its currently too specific
        for (const VkDescriptorSet descriptorSet : descriptorSets.sets) {
            if constexpr (std::is_same_v<T, VkDescriptorImageInfo>) {
                descWrite = VkWriteDescriptorSet { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSet,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 
                    .pImageInfo = &info };
            } else if constexpr (std::is_same_v<T, VkDescriptorBufferInfo>) {
                descWrite = VkWriteDescriptorSet { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSet,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &info };
            }

            vkUpdateDescriptorSets(device, 1, &descWrite, 0, nullptr);
        }
    }
};

}
