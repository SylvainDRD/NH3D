#include "vulkan_bind_group.hpp"

namespace NH3D {

// TODO: fix
std::pair<VulkanBindGroup::DescriptorSets, VulkanBindGroup::Metadata> VulkanBindGroup::create(
    VkDevice device, const VkShaderStageFlags stageFlags, const std::initializer_list<VkDescriptorType>& bindingTypes)
{
    for (uint32_t i = 0; i < VulkanRHI::MaxFramesInFlight; ++i) {
        _descriptorSets[i].reserve(_maxSets / VulkanRHI::MaxFramesInFlight);
    }

    // TODO: write a custom linear allocator for these kind of small allocations
    std::vector<VkDescriptorSetLayoutBinding> descriptorBindings {};
    descriptorBindings.reserve(bindingTypes.size());

    std::unordered_map<VkDescriptorType, uint32_t> descriptorTypeCounts {};
    descriptorTypeCounts.reserve(bindingTypes.size());

    uint32_t i = 0;
    for (VkDescriptorType descriptorType : bindingTypes) {
        descriptorBindings.emplace_back(VkDescriptorSetLayoutBinding {
            .binding = i++, .descriptorType = descriptorType, .descriptorCount = 1, .stageFlags = stageFlags });
        ++descriptorTypeCounts[descriptorType];
    }

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(descriptorBindings.size()),
        .pBindings = descriptorBindings.data(),
    };

    if (vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &_layout) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan descritor set layout");
    }

    std::vector<VkDescriptorPoolSize> poolSizes {};
    poolSizes.reserve(descriptorTypeCounts.size());

    for (auto [descriptorType, count] : descriptorTypeCounts) {
        poolSizes.emplace_back(VkDescriptorPoolSize { descriptorType, _maxSets * count });
    }

    VkDescriptorPoolCreateInfo poolCreateInfo { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = _maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data() };

    if (vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &_pool) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan descriptor pool");
    }

    // Allocate descriptor sets
    std::array<VkDescriptorSetLayout, VulkanRHI::MaxFramesInFlight> layouts;
    layouts.fill(_layout);

    VkDescriptorSetAllocateInfo allocInfo { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = _pool,
        .descriptorSetCount = VulkanRHI::MaxFramesInFlight,
        .pSetLayouts = layouts.data() };
    if (vkAllocateDescriptorSets(device, &allocInfo, _descriptorSets.sets) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to allocate Vulkan descriptor sets");
    }
}

void VulkanBindGroup::release(const VkDevice device, DescriptorSets& descriptorSets, Metadata& cold)
{
    vkDestroyDescriptorSetLayout(device, cold.layout, nullptr);
    cold.layout = nullptr;

    vkDestroyDescriptorPool(device, cold.pool, nullptr);
    cold.pool = nullptr;
}

VkDescriptorSet VulkanBindGroup::getDescriptorSet(
    const VkDevice device, const DescriptorSets& descriptorSets, const uint32_t frameInFlightId)
{
    NH3D_ASSERT(frameInFlightId < MaxFramesInFlight, "Requested out of bound descriptor set");

    return descriptorSets.sets[frameInFlightId];
}

}