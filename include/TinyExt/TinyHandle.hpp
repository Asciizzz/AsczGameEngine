#pragma once

#include <cstdint>
#include <functional>

union TinyHandle {
    struct {
        uint32_t index;
        uint32_t version;
    };

    // Full pack representation
    uint64_t value;

    constexpr TinyHandle() : value(UINT64_MAX) {}
    TinyHandle(uint32_t index, uint32_t version = 0) {
        *this = make(index, version);
    }

    /**
    * Create a handle
    @param index The index in the respective pool/array
    @param version The version for safety checks (default 0)
    */
    template<typename IndexType, typename VersionType>
    static TinyHandle make(IndexType index, VersionType version) {
        TinyHandle handle;
        handle.index = static_cast<uint32_t>(index);
        handle.version = static_cast<uint32_t>(version);
        return handle;
    }

    // Value operators
    constexpr bool operator==(const TinyHandle& other) const { return value == other.value; }
    constexpr bool operator!=(const TinyHandle& other) const { return value != other.value; }

    constexpr static TinyHandle invalid() { return TinyHandle(); }
    constexpr bool valid() const { return value != UINT64_MAX && index != UINT32_MAX; }
    constexpr void invalidate() { value = UINT64_MAX; }
};

namespace std {
    template<> struct hash<TinyHandle> {
        size_t operator()(const TinyHandle& h) const noexcept {
            return std::hash<uint64_t>()(h.value);
        }
    };
}