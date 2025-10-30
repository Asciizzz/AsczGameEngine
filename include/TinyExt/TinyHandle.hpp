#pragma once

#include <cstdint>

union tinyHandle {
    struct {
        uint32_t index;
        uint32_t version;
    };

    // Full pack representation
    uint64_t value;

    constexpr tinyHandle() noexcept : value(UINT64_MAX) {}
    tinyHandle(uint32_t index, uint32_t version = 0) noexcept {
        this->index = index;
        this->version = version;
    }

    // Value operators
    constexpr bool operator==(const tinyHandle& other) const noexcept { return value == other.value; }
    constexpr bool operator!=(const tinyHandle& other) const noexcept { return value != other.value; }

    constexpr bool valid() const noexcept { return value != UINT64_MAX && index != UINT32_MAX; }
    constexpr bool invalid() const noexcept { return !valid(); }
    constexpr void invalidate() noexcept { value = UINT64_MAX; }
};

namespace std {
    template<> struct hash<tinyHandle> {
        size_t operator()(const tinyHandle& h) const noexcept {
            return std::hash<uint64_t>()(h.value);
        }
    };
};
