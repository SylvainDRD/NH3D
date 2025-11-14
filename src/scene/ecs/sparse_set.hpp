#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/components/render_component.hpp>
#include <scene/ecs/dynamic_bitset.hpp>
#include <scene/ecs/entity.hpp>
#include <scene/ecs/interface_sparse_set.hpp>
#include <scene/ecs/subtree_view.hpp>

namespace NH3D {

template <typename T> class SparseSet : public ISparseSet {
    NH3D_NO_COPY(SparseSet)
public:
    inline SparseSet();

    SparseSet(SparseSet<T>&&) = default;

    virtual ~SparseSet() = default;

    inline void add(const Entity entity, T&& component);

    inline void remove(const Entity entity) override;

    [[nodiscard]] inline T& get(const Entity entity);

    [[nodiscard]] inline T& getRaw(const uint32 id);

    [[nodiscard]] inline const std::vector<Entity>& entities() const;

    [[nodiscard]] inline size_t size() const;

    [[nodiscard]] inline bool getFlag(const Entity entity) const;

    inline void setFlag(const Entity entity, const bool flag);

    [[nodiscard]] inline const void* getRawFlags() const;

protected:
    [[nodiscard]] inline uint32& getId(const Entity entity) const;

protected:
    constexpr static uint8 BufferBitSize = 10;
    constexpr static uint32 BufferSize = 1U << BufferBitSize;
    constexpr static uint32 InvalidIndex = NH3D_MAX_T(uint32);

    using indices = Uptr<uint32[]>;
    std::vector<indices> _entityLUT;
    std::vector<T> _data;
    std::vector<Entity> _entities;

    // A boolean flag per component that can be used for various purposes (e.g. marking dirty components, settings visible
    // flag to the render component, enabled physics on RigidBodyComponent, etc.)
    DynamicBitset _flags;
};

template <typename T>
inline SparseSet<T>::SparseSet()
    : _flags { 40'000 }
{
    _entityLUT.reserve(1'000); // allow for up to 1'024'000 entities without reallocation
    _data.reserve(40'000);
    // desync the reserved size to limit the chances of both reallocating at the same frame (in the best case realloc never happens anyway)
    _entities.reserve(60'000);
}

template <typename T> [[nodiscard]] inline uint32& SparseSet<T>::getId(const Entity entity) const
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    const uint32 bufferId = entity >> BufferBitSize;
    const uint32 indexId = entity & (BufferSize - 1);
    NH3D_ASSERT(bufferId < _entityLUT.size(), "Requested a component for an entity without storage");

    const auto& indices = _entityLUT[bufferId];

    return indices[indexId];
}

template <typename T> inline void SparseSet<T>::add(const Entity entity, T&& component)
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

    _flags.setFlag(entity, true);
}

template <typename T> inline void SparseSet<T>::remove(const Entity entity)
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    uint32& id = getId(entity);
    NH3D_ASSERT(id != InvalidIndex, "Trying to remove a non-existing component");
    const uint32 deletedId = id;

    id = InvalidIndex;

    const uint32 lastEntity = _entities.back();
    _entities.pop_back();
    _entities[deletedId] = lastEntity;
    _data[deletedId] = _data[lastEntity];
    _data.pop_back();

    _flags.setFlag(deletedId, _flags[_entities.size()]);
    _flags.setFlag(_entities.size(), false);
}

template <typename T> [[nodiscard]] inline T& SparseSet<T>::get(const Entity entity)
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    const uint32 id = getId(entity);
    NH3D_ASSERT(id != InvalidIndex, "Requested a non-existing component: Samir you're thrashing the cache");
    return _data[id];
}

template <typename T> [[nodiscard]] inline T& SparseSet<T>::getRaw(const uint32 id)
{
    NH3D_ASSERT(id < _data.size(), "Out of bound raw data SparseSet access");
    return _data[id];
}

template <typename T> [[nodiscard]] const std::vector<Entity>& SparseSet<T>::entities() const { return _entities; }

template <typename T> [[nodiscard]] inline size_t SparseSet<T>::size() const { return _entities.size(); }

template <typename T> [[nodiscard]] inline bool SparseSet<T>::getFlag(const Entity entity) const
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    const uint32 id = getId(entity);
    NH3D_ASSERT(id != InvalidIndex, "Requested a non-existing component flag");
    return _flags[id];
}

template <typename T> inline void SparseSet<T>::setFlag(const Entity entity, const bool flag)
{
    NH3D_ASSERT(entity != InvalidEntity, "Unexpected invalid entity");
    const uint32 id = getId(entity);
    NH3D_ASSERT(id != InvalidIndex, "Requested a non-existing component flag");
    _flags.setFlag(id, flag);
}

template <typename T> [[nodiscard]] inline const void* SparseSet<T>::getRawFlags() const { return _flags.data(); }

} // namespace NH3D