#pragma once

#include <cstring>
#include <memory>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <scene/ecs/entity.hpp>

namespace NH3D {

class ISparseSet {
public:
    virtual ~ISparseSet() = default;

    // This seals the deal as removal being a garbage operation
    // Perhapse if removal becomes performance critical, there is an argument for forward declaring all SparseSet types
    virtual void remove(entity e) = 0;
};

template <typename T>
class SparseSet : public ISparseSet {
    NH3D_NO_COPY(SparseSet<T>)
public:
    SparseSet();

    SparseSet(SparseSet<T>&&) = default;

    ~SparseSet() = default;

    void add(entity e, T&& component);

    void remove(entity e) override;

    T& get(entity e);

private:
    constexpr static uint8 BufferBitSize = 10;
    constexpr static uint32 BufferSize = 1 << BufferBitSize;
    constexpr static uint32 InvalidIndex = NH3D_MAX_T(uint32);

    using indices = Uptr<uint32[]>;
    std::vector<indices> _entityLUT;
    std::vector<T> _data;
    std::vector<entity> _entities; // required for deletion
};

template <typename T>
SparseSet<T>::SparseSet()
{
    _entityLUT.reserve(1'000); // allow for up to 1'024'000 entities without reallocation
    _data.reserve(40'000);
    // desync the reserved size to limit the chances of both reallocating at the same frame (in the best case realloc never happens anyway)
    _entities.reserve(60'000);
}

template <typename T>
void SparseSet<T>::add(entity e, T&& component)
{
    const uint32 bufferId = e >> BufferBitSize;
    const uint32 indexId = e & (BufferSize - 1);

    if (bufferId >= _entityLUT.size()) {
        _entityLUT.resize(bufferId + 1);
    }

    if (_entityLUT[bufferId] == nullptr) {
        _entityLUT[bufferId] = std::make_unique_for_overwrite<uint32[]>(BufferSize);
        std::memset(_entityLUT[BufferSize].get(), InvalidIndex, BufferSize * sizeof(uint32));
    }

    uint32& index = _entityLUT[bufferId][indexId];
    NH3D_ASSERT(index == InvalidIndex, "Trying to overwrite an existing component");
    index = _data.size();

    _data.emplace_back(std::forward<T>(component));
    _entities.emplace_back(e);
}

template <typename T>
void SparseSet<T>::remove(entity e)
{
    const uint32 bufferId = e >> BufferBitSize;
    const uint32 indexId = e & (BufferSize - 1);
    NH3D_ASSERT(bufferId < _entityLUT.size(), "Trying to clear a component for an entity without storage");

    const auto& indices = _entityLUT[bufferId];
    const uint32 deletedId = indices[indexId];
    NH3D_ASSERT(deletedId != InvalidIndex, "Trying to clear a non-existing component");

    indices[indexId] = InvalidIndex;

    const uint32 lastEntity = _entities.back();
    _entities.pop_back();
    _entities[deletedId] = lastEntity;
    _data[deletedId] = _data[lastEntity];
    _data.pop_back();
}

template <typename T>
T& SparseSet<T>::get(entity e)
{
    const uint32 bufferId = e >> BufferBitSize;
    const uint32 indexId = e & (BufferSize - 1);
    NH3D_ASSERT(bufferId < _entityLUT.size(), "Requested a component for an entity without storage");

    const auto& indices = _entityLUT[bufferId];
    const uint32 id = indices[indexId];
    NH3D_ASSERT(id != InvalidIndex, "Requested a non-existing component: Samir you're thrashing the cache");

    return _data[id];
}

}