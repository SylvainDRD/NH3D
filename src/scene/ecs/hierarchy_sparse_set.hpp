#pragma once
#include "misc/utils.hpp"
#include "scene/ecs/entity.hpp"
#include <scene/ecs/components/hierarchy_component.hpp>
#include <scene/ecs/sparse_set.hpp>

namespace NH3D {

class HierarchySparseSet : public SparseSet<HierarchyComponent> {
    NH3D_NO_COPY(HierarchySparseSet)
public:
    inline HierarchySparseSet();

    inline void add(const Entity entity, HierarchyComponent&& component);

    inline void remove(const Entity entity) override;

    // Uses pre-order traversal
    [[nodiscard]] inline SubtreeView getSubtree(const Entity entity) const;

    inline void setParent(const Entity entity, const Entity parent);

    [[nodiscard]] inline bool isLeaf(const Entity entity) const;

    inline void deleteSubtree(const Entity root);

private:
    [[nodiscard]] inline bool isAllocated(const Entity entity) const;

    [[nodiscard]] inline uint32 getSubtreeEndId(const uint32 entityId) const;

    inline void swapConsecutiveSubarrays(const uint32 begin1, const uint32 begin2, const uint32 end);

    inline void ensureExists(const uint32 entity);

private:
    static std::vector<uint32> g_buffer;
};

inline std::vector<uint32> HierarchySparseSet::g_buffer;

inline HierarchySparseSet::HierarchySparseSet()
    : SparseSet<HierarchyComponent>()
{
    g_buffer.reserve(2048);
}

inline void HierarchySparseSet::add(const Entity entity, HierarchyComponent&& component)
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");

    const uint32 bufferId = entity >> BufferBitSize;
    const uint32 indexId = entity & (BufferSize - 1);

    if (bufferId >= _entityLUT.size()) {
        _entityLUT.resize(bufferId + 1);
    }

    if (_entityLUT[bufferId] == nullptr) {
        _entityLUT[bufferId] = std::make_unique_for_overwrite<uint32[]>(BufferSize);
        std::memset(_entityLUT[bufferId].get(), InvalidIndex, BufferSize * sizeof(uint32));
    }

    uint32& index = _entityLUT[bufferId][indexId];
    NH3D_ASSERT(index == InvalidIndex, "Trying to overwrite an existing component");

    if (component.parent() == InvalidEntity) {
        index = _entities.size();
    } else {
        const uint32 parentId = getId(component.parent());

        NH3D_ASSERT(parentId != InvalidIndex, "Unexpected invalid index");
        index = parentId + 1;
    }

    _entities.emplace(_entities.begin() + index, entity);
    _data.emplace(_data.begin() + index, std::forward<HierarchyComponent>(component));

    for (uint32 i = index + 2; i < _entities.size(); ++i) {
        uint32& id = getId(_entities[i]);
        NH3D_ASSERT(id != InvalidIndex, "Unexpected invalid index");
        ++id;
    }
}

inline void HierarchySparseSet::remove(const Entity entity)
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");

    uint32& entityId = getId(entity);
    const uint32 deletedId = entityId;

    NH3D_ASSERT(isLeaf(entity), "Can only delete leaves from the Hierarchy");
    entityId = InvalidIndex;

    _entities.erase(_entities.begin() + deletedId);
    _data.erase(_data.begin() + deletedId);

    for (uint32 i = deletedId; i < _entities.size(); ++i) {
        uint32& id = getId(_entities[i]);
        NH3D_ASSERT(id != InvalidIndex, "Unexpected invalid index");
        --id;
    }
}

[[nodiscard]] inline SubtreeView HierarchySparseSet::getSubtree(const Entity entity) const
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    if (isLeaf(entity)) {
        return SubtreeView { entity };
    }

    const uint32 id = getId(entity);
    NH3D_ASSERT(id != InvalidIndex, "Requested a non-existing component: Samir you're thrashing the cache");

    return SubtreeView { &_entities[id], &_data[id], static_cast<uint32>(_data.size()) - id };
}

[[nodiscard]] inline bool HierarchySparseSet::isLeaf(const Entity entity) const
{
    if (entity == InvalidEntity) {
        return false;
    }
    if (!isAllocated(entity)) {
        return true;
    }

    const uint32 id = getId(entity);
    NH3D_ASSERT(id != InvalidIndex, "Unexpected invalid index");
    return id + 1 == _entities.size() || _data[id + 1].parent() != entity;
}

inline void HierarchySparseSet::deleteSubtree(const Entity root)
{
    NH3D_ASSERT(root != InvalidEntity, "Unexpected invalid entity");

    if (!isAllocated(root)) {
        return;
    }

    if (isLeaf(root)) {
        remove(root);
        return;
    }

    const uint32 rootId = getId(root);
    const uint32 subtreeEndId = getSubtreeEndId(rootId);

    for (uint32 i = rootId; i < subtreeEndId; ++i) {
        uint32& id = getId(_entities[i]);
        NH3D_ASSERT(id != InvalidIndex, "Unexpected invalid index");
        id = InvalidIndex;
    }
    _entities.erase(_entities.begin() + rootId, _entities.begin() + subtreeEndId);
    _data.erase(_data.begin() + rootId, _data.begin() + subtreeEndId);

    const uint32 parent = _data[rootId].parent();
    if (isLeaf(parent)) {
        // TODO: avoid extra array shift from the removal
        remove(_data[rootId].parent());
    }
}

// TODO: remove the extra array shift from the removal of orphaned components and last minute insertions
inline void HierarchySparseSet::setParent(const Entity entity, const Entity newParent)
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");

    ensureExists(entity);

    // Defines two consecutive subarrays, where begin2 is the beginning of the second subarray
    uint32 begin1, begin2, end;

    // In this case, we're deleting the component if it is a leaf
    if (newParent == InvalidEntity && isLeaf(entity)) {
        // Note that if the entity was just created by calling ensureExists, this function is just a costly no-op
        remove(entity);
        return;
    }

    const uint32 entityId = getId(entity);
    NH3D_ASSERT(entityId != InvalidIndex, "Unexpected invalid index");
    const Entity previousParent = _data[entityId].parent();
    // In this case, we're shifting the subtree to the end
    if (newParent == InvalidEntity) {
        _data[entityId]._parent = InvalidEntity;
        begin1 = entityId;
        begin2 = getSubtreeEndId(entityId);
        end = _entities.size(); // One-past the end index
    } else {
        _data[entityId]._parent = newParent;

        ensureExists(newParent);

        const uint32 newParentId = getId(newParent);
        const uint32 entitySubtreeEndId = getSubtreeEndId(entityId);
        if (newParentId < entityId) {
            begin1 = newParentId + 1;
            begin2 = entityId;
            end = entitySubtreeEndId;
        } else {
            begin1 = entityId;
            begin2 = entitySubtreeEndId;
            end = newParentId + 1; // One-past the end index
        }
    }

    swapConsecutiveSubarrays(begin1, begin2, end);

    // Delete old parent if it is orphaned (i.e. at root level without children)
    if (isLeaf(previousParent) && _data[getId(previousParent)].parent() == InvalidEntity) {
        remove(previousParent);
    }
}

[[nodiscard]] inline bool HierarchySparseSet::isAllocated(const Entity entity) const
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    const uint32 bufferId = entity >> BufferBitSize;
    const uint32 indexId = entity & (BufferSize - 1);
    return bufferId < _entityLUT.size() && _entityLUT[bufferId] != nullptr && _entityLUT[bufferId][indexId] != InvalidIndex;
}

[[nodiscard]] inline uint32 HierarchySparseSet::getSubtreeEndId(const uint32 entityId) const
{
    // Find the end of the subtree to move
    // One-past the end index
    uint32 entitySubtreeEndId = entityId;
    do {
        g_buffer.emplace_back(_entities[entitySubtreeEndId]);
        ++entitySubtreeEndId;

        if (entitySubtreeEndId >= _entities.size()) {
            break;
        }

        const Entity parent = static_cast<HierarchyComponent>(_data[entitySubtreeEndId]).parent();
        while (!g_buffer.empty() && g_buffer.back() != parent) {
            g_buffer.pop_back();
        }
    } while (!g_buffer.empty());

    g_buffer.clear();
    return entitySubtreeEndId;
}

inline void HierarchySparseSet::swapConsecutiveSubarrays(const uint32 begin1, const uint32 begin2, const uint32 end)
{
    // Swap both subarrays in the entities vector
    // To do so, copy everything into the previously allocated g_buffer
    // The alternative is to perform a full permutation of the entities, and a permutation for each subarray
    // I expect this version to be faster, even more so since we can do the same thing for the data
    const uint32 size = end - begin1;
    if (g_buffer.capacity() < size) {
        g_buffer.reserve(2 * size);
    }
    g_buffer.resize(size);
    const uint32 firstSubSize = begin2 - begin1;
    const uint32 secondSubSize = end - begin2;
    std::memcpy(g_buffer.data(), &_entities[begin1], size * sizeof(Entity));
    std::memcpy(&_entities[begin1], &g_buffer[firstSubSize], secondSubSize * sizeof(Entity));
    std::memcpy(&_entities[begin1 + secondSubSize], &g_buffer[0], firstSubSize * sizeof(Entity));

    // Update the LUT
    for (uint32 i = begin1; i < end; ++i) {
        uint32& id = getId(_entities[i]);
        NH3D_ASSERT(id != InvalidIndex, "Unexpected invalid index");
        id = i;
    }

    // Swap the data around, exploits the fact that the HierarchyComponent is just a parent Entity
    // If this assumption ever changes, allocate another g_buffer (or use the existing one as a byte array? nasty) or swap to a loop
    NH3D_STATIC_ASSERT(sizeof(Entity) == sizeof(HierarchyComponent));
    std::memcpy(g_buffer.data(), &_data[begin1], size * sizeof(Entity));
    std::memcpy(&_data[begin1], &g_buffer[firstSubSize], secondSubSize * sizeof(Entity));
    std::memcpy(&_data[begin1 + secondSubSize], &g_buffer[0], firstSubSize * sizeof(Entity));

    g_buffer.clear();
}

inline void HierarchySparseSet::ensureExists(const uint32 entity)
{
    if (!isAllocated(entity)) {
        add(entity, std::move(HierarchyComponent{}));
    }
}

}