#include "vulkan_bind_group.hpp"
#include <cstring>
#include <rendering/core/frame_resource.hpp>
#include <rendering/vulkan/vulkan_rhi.hpp>
#include <vulkan/vulkan_core.h>

namespace NH3D {

[[nodiscard]] std::pair<DescriptorSets, BindGroupMetadata> VulkanBindGroup::create(const IRHI& rhi, const CreateInfo& info)
{
    NH3D_ASSERT(info.bindingTypes.size != 0, "Binding types list cannot be empty");

    const VkDevice device = static_cast<const VulkanRHI&>(rhi).getVkDevice();

    static std::vector<VkDescriptorSetLayoutBinding> descriptorBindings {};
    descriptorBindings.resize(info.bindingTypes.size);

    static std::unordered_map<VkDescriptorType, uint32_t> descriptorTypeCounts {};
    descriptorTypeCounts.reserve(info.bindingTypes.size);
    descriptorTypeCounts.clear();

    static std::vector<VkDescriptorBindingFlags> bindingFlags {};
    bindingFlags.resize(info.bindingTypes.size);
    std::memset(bindingFlags.data(), 0, bindingFlags.size() * sizeof(VkDescriptorBindingFlags));

    for (uint32 i = 0; i < info.bindingTypes.size; ++i) {
        const VkDescriptorType descriptorType = info.bindingTypes.data[i];

        descriptorBindings[i] = VkDescriptorSetLayoutBinding {
            .binding = i, .descriptorType = descriptorType, .descriptorCount = 1, .stageFlags = info.stageFlags
        };
        ++descriptorTypeCounts[descriptorType];

        // If the last binding has more than one descriptor, use a variable count binding
        if (i == info.bindingTypes.size - 1 && info.finalBindingCount > 1) {
            NH3D_ASSERT(descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                "Currently expect only combined image samplers to have variable descriptor count");

            bindingFlags[i] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
                | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
            descriptorBindings[i].descriptorCount = info.finalBindingCount;
        }
    }

    const VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlag {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindingFlags.size()),
        .pBindingFlags = bindingFlags.data(),
    };

    const VkDescriptorSetLayoutCreateInfo layoutCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &bindingFlag,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = static_cast<uint32_t>(descriptorBindings.size()),
        .pBindings = descriptorBindings.data(),
    };

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &layout) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan descritor set layout");
    }

    static std::vector<VkDescriptorPoolSize> poolSizes {};
    poolSizes.reserve(descriptorTypeCounts.size());
    poolSizes.clear();

    for (auto [descriptorType, count] : descriptorTypeCounts) {
        poolSizes.emplace_back(VkDescriptorPoolSize { descriptorType, IRHI::MaxFramesInFlight * count });

        if (info.finalBindingCount > 1 && descriptorType == info.bindingTypes.data[info.bindingTypes.size - 1]) {
            poolSizes.back().descriptorCount *= info.finalBindingCount;
        }
    }

    VkDescriptorPoolCreateInfo poolCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = IRHI::MaxFramesInFlight,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    VkDescriptorPool pool;
    if (vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &pool) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan descriptor pool");
    }

    // Allocate descriptor sets
    std::array<VkDescriptorSetLayout, IRHI::MaxFramesInFlight> layouts;
    layouts.fill(layout);

    VkDescriptorSetAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = IRHI::MaxFramesInFlight,
        .pSetLayouts = layouts.data(),
    };

    std::array<uint32, IRHI::MaxFramesInFlight> bindingSizes;
    bindingSizes.fill(info.finalBindingCount);
    const VkDescriptorSetVariableDescriptorCountAllocateInfo varDescCountInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = bindingSizes.size(),
        .pDescriptorCounts = bindingSizes.data(),
    };
    if (info.finalBindingCount > 1) {
        allocInfo.pNext = &varDescCountInfo;
    }

    DescriptorSets descriptorSets {};
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.sets[0]) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to allocate Vulkan descriptor sets");
    }

    FrameResource<VkDescriptorImageInfo> textureInfos;
    FrameResource<VkDescriptorBufferInfo> bufferInfos;
    for (uint32 i = 0; i < IRHI::MaxFramesInFlight; ++i) {

        descriptorSets.updates[i] = new VkWriteDescriptorSet[MaxRegisteredBindingUpdates * 2];
        descriptorSets.textureInfos[i] = new VkDescriptorImageInfo[MaxRegisteredBindingUpdates];
        descriptorSets.bufferInfos[i] = new VkDescriptorBufferInfo[MaxRegisteredBindingUpdates];
    }

    return { { descriptorSets }, BindGroupMetadata { layout, pool } };
}

void VulkanBindGroup::release(const IRHI& rhi, DescriptorSets& descriptorSets, BindGroupMetadata& metadata)
{
    for (uint32 i = 0; i < IRHI::MaxFramesInFlight; ++i) {
        delete[] descriptorSets.updates[i];
        delete[] descriptorSets.textureInfos[i];
        delete[] descriptorSets.bufferInfos[i];

        descriptorSets.updateCounts[i] = 0;
        descriptorSets.textureInfoCounts[i] = 0;
        descriptorSets.bufferInfoCounts[i] = 0;
    }

    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);
    vkDestroyDescriptorSetLayout(vrhi.getVkDevice(), metadata.layout, nullptr);
    metadata.layout = nullptr;

    vkDestroyDescriptorPool(vrhi.getVkDevice(), metadata.pool, nullptr);
    metadata.pool = nullptr;
}

bool VulkanBindGroup::valid(const DescriptorSets& descriptorSets, const BindGroupMetadata& metadata)
{
    return descriptorSets.sets[0] != nullptr && metadata.layout != nullptr && metadata.pool != nullptr;
}

[[nodiscard]] VkDescriptorSet VulkanBindGroup::getUpdatedDescriptorSet(
    const VkDevice device, const DescriptorSets& descriptorSets, const uint32_t frameInFlightId)
{
    NH3D_ASSERT(frameInFlightId < IRHI::MaxFramesInFlight, "Requested out of bound descriptor set");

    if (descriptorSets.updateCounts[frameInFlightId] == 0) {
        return descriptorSets.sets[frameInFlightId];
    }

    vkUpdateDescriptorSets(
        device, static_cast<uint32>(descriptorSets.updateCounts[frameInFlightId]), descriptorSets.updates[frameInFlightId], 0, nullptr);
    descriptorSets.updateCounts[frameInFlightId] = 0;
    descriptorSets.textureInfoCounts[frameInFlightId] = 0;
    descriptorSets.bufferInfoCounts[frameInFlightId] = 0;

    return descriptorSets.sets[frameInFlightId];
}

void VulkanBindGroup::bind(VkCommandBuffer commandBuffer, const ArrayWrapper<VkDescriptorSet> descriptorSets,
    const VkPipelineBindPoint bindPoint, const VkPipelineLayout pipelineLayout)
{
    vkCmdBindDescriptorSets(commandBuffer, bindPoint, pipelineLayout, 0, descriptorSets.size, descriptorSets.data, 0, nullptr);
}

}
