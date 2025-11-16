#pragma once

#include <cstdint>
#include <initializer_list>
#include <misc/utils.hpp>
#include <rendering/core/bind_group.hpp>
#include <rendering/core/rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

struct DescriptorSets {
    std::array<VkDescriptorSet, IRHI::MaxFramesInFlight> sets;

private:
    // I don't like how much this pollutes the struct, but I expect only few direct set manipulations per frame
    using UpdateArrayType = std::vector<VkWriteDescriptorSet>;
    mutable std::array<UpdateArrayType, IRHI::MaxFramesInFlight> updates;

    friend struct VulkanBindGroup;
};

struct BindGroupMetadata {
    VkDescriptorSetLayout layout;
    VkDescriptorPool pool;
};

struct VulkanBindGroup {
    using ResourceType = BindGroup;

    using HotType = DescriptorSets;

    using ColdType = BindGroupMetadata;

    struct CreateInfo {
        const VkShaderStageFlags stageFlags;
        const ArrayWrapper<VkDescriptorType> bindingTypes;
        const uint32 finalBindingCount = 1;
    };

    /// "Constructor / Destructor": finalBindingCount is used to determine if we are using variable descriptor count
    [[nodiscard]] static std::pair<DescriptorSets, BindGroupMetadata> create(VkDevice device, const CreateInfo& info);

    // Used generically by the ResourceManager, must be API agnostic, non-const ref for invalidation
    static void release(const IRHI& rhi, DescriptorSets& descriptorSets, BindGroupMetadata& metadata);

    static bool valid(const DescriptorSets& descriptorSets, const BindGroupMetadata& metadata);

    /// Helper functions
    [[nodiscard]] static VkDescriptorSet getUpdatedDescriptorSet(
        const VkDevice device, const DescriptorSets& descriptorSets, const uint32_t frameInFlightId);

    static void bind(VkCommandBuffer commandBuffer, const ArrayWrapper<VkDescriptorSet> descriptorSets, const VkPipelineBindPoint bindPoint,
        const VkPipelineLayout pipelineLayout);

    template <typename T>
    static inline void updateDescriptorSet(
        VkDevice device, const VkDescriptorSet descriptorSet, const T& info, const VkDescriptorType type, uint32 binding = 0)
    {
        const VkWriteDescriptorSet descWrite = createWriteDescriptorSet(descriptorSet, info, type, binding);
        vkUpdateDescriptorSets(device, 1, &descWrite, 0, nullptr);
    }

    template <typename T>
    static inline void registerBufferedUpdate(
        const DescriptorSets& descriptorSets, const T& info, const VkDescriptorType type, uint32_t binding = 0)
    {
        for (uint32 i = 0; i < IRHI::MaxFramesInFlight; ++i) {
            VkWriteDescriptorSet descWrite = createWriteDescriptorSet(descriptorSets.sets[i], info, type, binding);
            descriptorSets.updates[i].emplace_back(descWrite);
        }
    }

private:
    template <typename T>
    constexpr static inline VkWriteDescriptorSet createWriteDescriptorSet(
        const VkDescriptorSet descriptorSet, const T& info, const VkDescriptorType type, uint32 binding)
    {
        VkWriteDescriptorSet descWrite;

        if constexpr (std::is_same_v<T, VkDescriptorImageInfo>) {
            NH3D_ASSERT(type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                    || type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || type == VK_DESCRIPTOR_TYPE_SAMPLER,
                "Descriptor type does not match info type");

            descWrite = VkWriteDescriptorSet { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSet,
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = type,
                .pImageInfo = &info };
        } else if constexpr (std::is_same_v<T, VkDescriptorBufferInfo>) {
            NH3D_ASSERT(type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                    || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
                "Descriptor type does not match info type");

            descWrite = VkWriteDescriptorSet { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptorSet,
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = type,
                .pBufferInfo = &info };
        }
        return descWrite;
    }
};

}
