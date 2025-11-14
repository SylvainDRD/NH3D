#pragma once

#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <vector>

namespace NH3D {

class DynamicBitset {
public:
    DynamicBitset() = delete;

    DynamicBitset(const size_t bitCapacity)
        : _data (static_cast<uint32>((bitCapacity + sizeof(uint32) * 8 - 1) / (sizeof(uint32) * 8)))
    {
    }

    inline void setFlag(const size_t index, const bool flag)
    {
        // TODO: msvc equivalent
        const size_t dataIndex = index >> __builtin_ctz(sizeof(uint32) * 8);
        const size_t bitIndex = index & ((sizeof(uint32) * 8) - 1);

        ensureCapacity(index + 1);

        _data[dataIndex] = (_data[dataIndex] & ~(1U << bitIndex)) | (static_cast<uint32>(flag) << bitIndex);
    }

    [[nodiscard]] inline bool operator[](const size_t index) const
    {
        // TODO: msvc equivalent
        const size_t dataIndex = index >> __builtin_ctz(sizeof(uint32) * 8);
        const size_t bitIndex = index & ((sizeof(uint32) * 8) - 1);
        NH3D_ASSERT(dataIndex < _data.size(), "Out of bound DynamicBitset access");
        return _data[dataIndex] & (1U << bitIndex);
    }

    [[nodiscard]] inline const void* data() const { return reinterpret_cast<const void*>(_data.data()); }

private:
    inline void ensureCapacity(const size_t size)
    {
        const size_t requiredCapacity = (static_cast<uint32>(size) + sizeof(uint32) * 8U - 1) / (sizeof(uint32) * 8U);

        if (requiredCapacity > _data.size()) {
            _data.resize(requiredCapacity);
        }
    }

private:
    std::vector<uint32> _data;
};

} // namespace NH3D