#include "vulkan_bind_group.hpp"
#include <cstring>
#include <rendering/vulkan/vulkan_rhi.hpp>

namespace NH3D {

std::pair<VulkanBindGroup::DescriptorSets, VulkanBindGroup::Metadata> VulkanBindGroup::create(
    const VkDevice device, const VkShaderStageFlags stageFlags, const std::initializer_list<VkDescriptorType>& bindingTypes)
{
    static std::vector<VkDescriptorSetLayoutBinding> descriptorBindings {};
    descriptorBindings.reserve(bindingTypes.size());
    descriptorBindings.clear();

    static std::unordered_map<VkDescriptorType, uint32_t> descriptorTypeCounts {};
    descriptorTypeCounts.reserve(bindingTypes.size());
    descriptorTypeCounts.clear();

    uint32_t i = 0;
    for (VkDescriptorType descriptorType : bindingTypes) {
        descriptorBindings.emplace_back(VkDescriptorSetLayoutBinding {
            .binding = i++, .descriptorType = descriptorType, .descriptorCount = 1, .stageFlags = stageFlags });
        ++descriptorTypeCounts[descriptorType];
    }

    VkDescriptorBindingFlags bindingFlags = { VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlag {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .bindingCount = 1, .pBindingFlags = &bindingFlags
    };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo {
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
    }

    VkDescriptorPoolCreateInfo poolCreateInfo { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = IRHI::MaxFramesInFlight,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data() };

    VkDescriptorPool pool;
    if (vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &pool) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan descriptor pool");
    }

    // Allocate descriptor sets
    std::array<VkDescriptorSetLayout, IRHI::MaxFramesInFlight> layouts;
    layouts.fill(layout);

    DescriptorSets descriptorSets {};
    VkDescriptorSetAllocateInfo allocInfo { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = IRHI::MaxFramesInFlight,
        .pSetLayouts = layouts.data() };
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.sets.data()) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to allocate Vulkan descriptor sets");
    }

    return { descriptorSets, Metadata { layout, pool } };
}

void VulkanBindGroup::release(const IRHI& rhi, DescriptorSets& descriptorSets, Metadata& cold)
{
    const VulkanRHI& vrhi = static_cast<const VulkanRHI&>(rhi);
    vkDestroyDescriptorSetLayout(vrhi.getVkDevice(), cold.layout, nullptr);
    cold.layout = nullptr;

    vkDestroyDescriptorPool(vrhi.getVkDevice(), cold.pool, nullptr);
    cold.pool = nullptr;
}

VkDescriptorSet VulkanBindGroup::getDescriptorSet(
    const VkDevice device, const DescriptorSets& descriptorSets, const uint32_t frameInFlightId)
{
    NH3D_ASSERT(frameInFlightId < IRHI::MaxFramesInFlight, "Requested out of bound descriptor set");

    return descriptorSets.sets[frameInFlightId];
}

}