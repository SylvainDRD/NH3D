#pragma once

#include <concepts>
#include <cstddef>
#include <cstring>
#include <memory>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/components/hierarchy_component.hpp>
#include <scene/ecs/entity.hpp>
#include <scene/ecs/subtree_view.hpp>

namespace NH3D {

class ISparseSet {
public:
    virtual ~ISparseSet() = default;

    // This seals the deal as removal being a garbage operation
    // Perhapse if removal becomes performance critical, there is an argument for forward declaring all SparseSet types
    virtual void remove(const Entity entity) = 0;
};

template <typename T>
class SparseSet : public ISparseSet {
    NH3D_NO_COPY(SparseSet)
public:
    inline SparseSet();

    SparseSet(SparseSet<T>&&) = default;

    ~SparseSet() = default;

    inline void add(const Entity entity, T&& component);

    inline void remove(const Entity entity) override;

    [[nodiscard]] inline T& get(const Entity entity);

    [[nodiscard]] inline T& getRaw(const uint32 id);

    [[nodiscard]] inline const std::vector<Entity>& entities() const;

    [[nodiscard]] inline size_t size() const;

    [[nodiscard]] inline SubtreeView getSubtree(const Entity entity) const
        requires std::same_as<HierarchyComponent, T>;

    inline void setParent(const Entity entity, const Entity parent)
        requires std::same_as<HierarchyComponent, T>;

    [[nodiscard]] inline bool isLeaf(const Entity entity) const
        requires std::same_as<HierarchyComponent, T>;

private:
    [[nodiscard]] inline uint32& getId(const Entity entity) const;

    [[nodiscard]] inline uint32 getSubtreeEndId(const uint32 entityId, std::vector<uint32>& buffer) const
        requires std::same_as<HierarchyComponent, T>;

    inline void swapConsecutiveSubarrays(const uint32 begin1, const uint32 begin2, const uint32 end, std::vector<uint32>& buffer);

private:
    constexpr static uint8 BufferBitSize = 10;
    constexpr static uint32 BufferSize = 1 << BufferBitSize;
    constexpr static uint32 InvalidIndex = NH3D_MAX_T(uint32);

    using indices = Uptr<uint32[]>;
    std::vector<indices> _entityLUT;
    std::vector<T> _data;
    std::vector<Entity> _entities; // required for deletion
};

template <typename T>
inline SparseSet<T>::SparseSet()
{
    _entityLUT.reserve(1'000); // allow for up to 1'024'000 entities without reallocation
    _data.reserve(40'000);
    // desync the reserved size to limit the chances of both reallocating at the same frame (in the best case realloc never happens anyway)
    _entities.reserve(60'000);
}

template <typename T>
[[nodiscard]] inline uint32& SparseSet<T>::getId(const Entity entity) const
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    const uint32 bufferId = entity >> BufferBitSize;
    const uint32 indexId = entity & (BufferSize - 1);
    NH3D_ASSERT(bufferId < _entityLUT.size(), "Requested a component for an entity without storage");

    const auto& indices = _entityLUT[bufferId];

    return indices[indexId];
}

template <typename T>
inline void SparseSet<T>::add(const Entity entity, T&& component)
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
    index = _data.size();

    _data.emplace_back(std::forward<T>(component));
    _entities.emplace_back(entity);
}

template <typename T>
inline void SparseSet<T>::remove(const Entity entity)
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    uint32& id = getId(entity);
    const uint32 deletedId = id;

    id = InvalidIndex;

    const uint32 lastEntity = _entities.back();
    _entities.pop_back();
    _entities[deletedId] = lastEntity;
    _data[deletedId] = _data[lastEntity];
    _data.pop_back();
}

template <typename T>
[[nodiscard]] inline T& SparseSet<T>::get(const Entity entity)
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    const uint32 id = getId(entity);
    NH3D_ASSERT(id != InvalidIndex, "Requested a non-existing component: Samir you're thrashing the cache");
    return _data[id];
}

template <typename T>
[[nodiscard]] inline T& SparseSet<T>::getRaw(const uint32 id)
{
    NH3D_ASSERT(id < _data.size(), "Out of bound raw data SparseSet access");
    return _data[id];
}
template <typename T>
[[nodiscard]] const std::vector<Entity>& SparseSet<T>::entities() const
{
    return _entities;
}

template <typename T>
[[nodiscard]] inline size_t SparseSet<T>::size() const
{
    return _entities.size();
}

template <typename T>
[[nodiscard]] inline SubtreeView SparseSet<T>::getSubtree(const Entity entity) const
    requires std::same_as<HierarchyComponent, T>
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    const uint32 id = getId(entity);
    NH3D_ASSERT(id != InvalidIndex, "Requested a non-existing component: Samir you're thrashing the cache");

    return SubtreeView { &_entities[id], &_data[id], static_cast<uint32>(_data.size()) - id };
}

template <>
inline void SparseSet<HierarchyComponent>::add(const Entity entity, HierarchyComponent&& component)
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

    const uint32 parentId = getId(component.parent());

    if (parentId != InvalidEntity) {
        index = parentId + 1;
    } else {
        // Edge case: we're likely to call that function again with entity as component.parent(), i.e. we're creating a new subtree from a root & leaf entity
        // It would be more performant & cleaner to have a separate function doing both, since this leaves the set in a weird state
        index = _entities.size();
    }

    _entities.emplace(_entities.begin() + index, entity);
    _data.emplace(_data.begin() + index, std::forward<HierarchyComponent>(component));

    for (uint32 i = index + 2; i < _entities.size(); ++i) {
        uint32& id = getId(_entities[i]);
        NH3D_ASSERT(id != InvalidIndex, "Unexpected invalid index");
        ++id;
    }
}

template <>
inline void SparseSet<HierarchyComponent>::remove(const Entity entity)
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");

    uint32& id = getId(entity);
    const uint32 deletedId = id;
    id = InvalidIndex;

    NH3D_ASSERT(deletedId + 1 < _entities.size() ? _data[deletedId + 1].parent() != entity : true, "Can only delete leaves from the HierarchyComponent SparseSet");

    _entities.erase(_entities.begin() + id);
    _data.erase(_data.begin() + id);

    for (uint32 i = id; i < _entities.size(); ++i) {
        uint32& id = getId(_entities[i]);
        NH3D_ASSERT(id != InvalidIndex, "Unexpected invalid index");
        --id;
    }
}

template <typename T>
[[nodiscard]] inline uint32 SparseSet<T>::getSubtreeEndId(const uint32 entityId, std::vector<uint32>& buffer) const
    requires std::same_as<HierarchyComponent, T>
{
    // Find the end of the subtree to move
    // One-past the end index
    uint32 entitySubtreeEndId = entityId;
    do {
        buffer.emplace_back(_entities[entitySubtreeEndId]);
        ++entitySubtreeEndId;

        if (entitySubtreeEndId >= _entities.size()) {
            break;
        }

        const Entity parent = static_cast<HierarchyComponent>(_data[entitySubtreeEndId]).parent();
        while (!buffer.empty() && buffer.back() != parent) {
            buffer.pop_back();
        }
    } while (!buffer.empty());

    buffer.clear();
    return entitySubtreeEndId;
}

template <typename T>
inline void SparseSet<T>::swapConsecutiveSubarrays(const uint32 begin1, const uint32 begin2, const uint32 end, std::vector<uint32>& buffer)
{
    // Swap both subarrays in the entities vector
    // To do so, copy everything into the previously allocated buffer
    // The alternative is to perform a full permutation of the entities, and a permutation for each subarray
    // I expect this version to be faster, even more so since we can do the same thing for the data
    const uint32 size = end - begin1;
    if (buffer.capacity() < size) {
        buffer.reserve(2 * size);
    }
    buffer.resize(size);
    const uint32 firstSubSize = begin2 - begin1;
    const uint32 secondSubSize = end - begin2;
    std::memcpy(buffer.data(), &_entities[begin1], size * sizeof(Entity));
    std::memcpy(&_entities[begin1], &buffer[firstSubSize], secondSubSize * sizeof(Entity));
    std::memcpy(&_entities[begin1 + secondSubSize], &buffer[0], firstSubSize * sizeof(Entity));

    // Update the LUT
    for (uint32 i = begin1; i < end; ++i) {
        uint32& id = getId(_entities[i]);
        NH3D_ASSERT(id != InvalidIndex, "Unexpected invalid index");
        id = i;
    }

    // Swap the data around, exploits the fact that the HierarchyComponent is just a parent Entity
    // If this assumption ever changes, allocate another buffer (or use the existing one as a byte array? nasty) or swap to a loop
    NH3D_STATIC_ASSERT(sizeof(Entity) == sizeof(HierarchyComponent));
    std::memcpy(buffer.data(), &_data[begin1], size * sizeof(Entity));
    std::memcpy(&_data[begin1], &buffer[firstSubSize], secondSubSize * sizeof(Entity));
    std::memcpy(&_data[begin1 + secondSubSize], &buffer[0], firstSubSize * sizeof(Entity));

    buffer.clear();
}

// TODO: remove the extra array shift induced by the removal of orphaned components
template <typename T>
inline void SparseSet<T>::setParent(const Entity entity, const Entity newParent)
    requires std::same_as<HierarchyComponent, T>
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");

    static std::vector<uint32> buffer;
    buffer.reserve(2048);

    // Defines two consecutive subarrays, where begin2 is the beginning of the second subarray
    uint32 begin1, begin2, end;

    // In this case, we're deleting the component if it is a leaf
    if (newParent == InvalidEntity && isLeaf(entity)) {
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
        begin2 = getSubtreeEndId(entityId, buffer);
        end = _entities.size(); // One-past the end index
    } else {
        _data[entityId]._parent = newParent;
        const uint32 &newParentId = getId(newParent);
        if(newParentId == InvalidIndex) {
            // Ensures the parent component is present
            HierarchyComponent component;
            component._parent = InvalidEntity;
            add(newParent, std::move(component));
        }

        const uint32 entitySubtreeEndId = getSubtreeEndId(entityId, buffer);
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

    swapConsecutiveSubarrays(begin1, begin2, end, buffer);

    // Delete old parent if it is orphaned (i.e. at root level without children)
    if (isLeaf(previousParent) && _data[getId(previousParent)].parent() == InvalidEntity) {
        remove(previousParent);
    }
}

template <typename T>
[[nodiscard]] inline bool SparseSet<T>::isLeaf(const Entity entity) const
    requires std::same_as<HierarchyComponent, T>
{
    const uint32 id = getId(entity);
    NH3D_ASSERT(id != InvalidIndex, "Unexpected invalid index");
    return id + 1 == _entities.size() || _data[id + 1].parent() != entity;
}

}