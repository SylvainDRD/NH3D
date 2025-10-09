#pragma once

#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <initializer_list>
#include <misc/utils.hpp>

namespace NH3D {

class VulkanRHI;

template <uint32_t MaxFramesInFlight>
class VulkanDescriptorSetPool {
    NH3D_NO_COPY_MOVE(VulkanDescriptorSetPool)
public:
    VulkanDescriptorSetPool() = delete;

    // Bindings are expected in order i.e. first descritor type for index 0, second for index 1, etc.
    inline VulkanDescriptorSetPool(VkDevice device, VkShaderStageFlags stageFlags, std::initializer_list<VkDescriptorType> bindingTypes, uint32_t maxSets);

    [[nodiscard]] inline VkDescriptorSet getDescriptorSet(VkDevice device, uint32_t frameInFlightId);

    [[nodiscard]] inline VkDescriptorSetLayout getLayout() const;

    inline void bind(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout);

    inline void updateDescriptorSet(VkDevice device, VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet, VkPipelineLayout layout, VkDescriptorImageInfo imageInfo);

    inline void clearPool(VkDevice device);

    inline void releasePool(VkDevice device);

private:
    void allocateSet(VkDevice device, uint32_t frameInFlightId);

private:
    VkDescriptorSetLayout _layout;

    VkShaderStageFlags _stageFlags;

    VkDescriptorPool _pool;

    using DescriptorSets = std::vector<VkDescriptorSet>;
    std::array<DescriptorSets, MaxFramesInFlight> _descriptorSets;

    uint32_t _maxSets;

    uint32_t _lastFrameInFlightId = NH3D_MAX_T(decltype(_lastFrameInFlightId));

    uint32_t _nextDescriptorId;
};

template <uint32_t MaxFramesInFlight>
inline VulkanDescriptorSetPool<MaxFramesInFlight>::VulkanDescriptorSetPool(VkDevice device, VkShaderStageFlags stageFlags, std::initializer_list<VkDescriptorType> bindingTypes, uint32_t maxSets)
    : _stageFlags { stageFlags }
    , _maxSets { maxSets }
{
    for (uint32_t i = 0; i < MaxFramesInFlight; ++i) {
        _descriptorSets[i].reserve(_maxSets / MaxFramesInFlight);
    }

    // TODO: write a custom linear allocator for these kind of small allocations
    std::vector<VkDescriptorSetLayoutBinding> descriptorBindings {};
    descriptorBindings.reserve(bindingTypes.size());

    std::unordered_map<VkDescriptorType, uint32_t> descriptorTypeCounts {};
    descriptorTypeCounts.reserve(bindingTypes.size());

    uint32_t i = 0;
    for (VkDescriptorType descriptorType : bindingTypes) {
        descriptorBindings.emplace_back(VkDescriptorSetLayoutBinding { .binding = i++, .descriptorType = descriptorType, .descriptorCount = 1, .stageFlags = stageFlags });
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

    VkDescriptorPoolCreateInfo poolCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = _maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    if (vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &_pool) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to create Vulkan descriptor pool");
    }
}

template <uint32_t MaxFramesInFlight>
inline VkDescriptorSet VulkanDescriptorSetPool<MaxFramesInFlight>::getDescriptorSet(VkDevice device, uint32_t frameInFlightId)
{
    NH3D_ASSERT(frameInFlightId < MaxFramesInFlight, "Requested out of bound descriptor set");
    std::vector<VkDescriptorSet>& _currentFramePool = _descriptorSets[frameInFlightId];

    if (_lastFrameInFlightId != frameInFlightId) {
        _lastFrameInFlightId = frameInFlightId;
        _nextDescriptorId = 0;
    }

    if (_nextDescriptorId >= _currentFramePool.size()) {
        if (_currentFramePool.size() == _maxSets / MaxFramesInFlight) {
            return nullptr;
        }

        allocateSet(device, frameInFlightId);
    }

    return _currentFramePool[_nextDescriptorId++ ];
}

template <uint32_t MaxFramesInFlight>
inline VkDescriptorSetLayout VulkanDescriptorSetPool<MaxFramesInFlight>::getLayout() const
{
    return _layout;
}

template <uint32_t MaxFramesInFlight>
inline void VulkanDescriptorSetPool<MaxFramesInFlight>::bind(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout)
{
    vkCmdBindDescriptorSets(commandBuffer, bindPoint, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
}

template <uint32_t MaxFramesInFlight>
inline void VulkanDescriptorSetPool<MaxFramesInFlight>::updateDescriptorSet(VkDevice device, VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet, VkPipelineLayout layout, VkDescriptorImageInfo imageInfo)
{
    VkWriteDescriptorSet descWrite {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &imageInfo
    };

    vkUpdateDescriptorSets(device, 1, &descWrite, 0, nullptr);
}

template <uint32_t MaxFramesInFlight>
inline void VulkanDescriptorSetPool<MaxFramesInFlight>::clearPool(VkDevice device)
{
    vkResetDescriptorPool(device, _pool, 0);
    for (uint32_t i = 0; i < MaxFramesInFlight; ++i) {
        _descriptorSets[i].clear();
    }
}

template <uint32_t MaxFramesInFlight>
inline void VulkanDescriptorSetPool<MaxFramesInFlight>::releasePool(VkDevice device)
{
    vkDestroyDescriptorSetLayout(device, _layout, nullptr);
    vkDestroyDescriptorPool(device, _pool, nullptr);
}

template <uint32_t MaxFramesInFlight>
inline void VulkanDescriptorSetPool<MaxFramesInFlight>::allocateSet(VkDevice device, uint32_t frameInFlightId)
{
    VkDescriptorSetAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = _pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &_layout
    };

    VkDescriptorSet set;
    if (vkAllocateDescriptorSets(device, &allocInfo, &set) != VK_SUCCESS) {
        NH3D_ABORT_VK("Failed to allocate Vulkan descriptor set");
    }

    _descriptorSets[frameInFlightId].emplace_back(set);
}

}