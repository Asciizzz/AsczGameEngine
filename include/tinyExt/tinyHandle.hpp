#pragma once

#include <cstdint>
#include <functional>
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

#include <typeindex>
struct typeHandle {
    typeHandle() noexcept = default;

    tinyHandle handle = tinyHandle();
    std::type_index typeIndex = std::type_index(typeid(void));

    constexpr bool operator==(const typeHandle& other) const noexcept {
        return handle == other.handle && typeIndex == other.typeIndex;
    }
    constexpr bool operator!=(const typeHandle& other) const noexcept {
        return !(*this == other);
    }

    bool valid() const noexcept { return handle.valid(); }

    template<typename T>
    bool isType() const noexcept { return valid() && typeIndex == std::type_index(typeid(T)); }

    bool sameType(const typeHandle& other) const noexcept {
        return typeIndex == other.typeIndex;
    }

    template<typename T>
    static typeHandle make(tinyHandle h) noexcept {
        typeHandle th;
        th.handle = h;
        th.typeIndex = std::type_index(typeid(T));
        return th;
    }

    static typeHandle make(tinyHandle h, const std::type_index& tIndex) noexcept {
        typeHandle th;
        th.handle = h;
        th.typeIndex = tIndex;
        return th;
    }
};