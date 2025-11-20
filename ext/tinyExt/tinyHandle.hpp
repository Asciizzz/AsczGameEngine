#pragma once

#include <cstdint>
#include <typeindex>
#include <functional>

// -------------------- tinyHandle --------------------

union tinyHandle {
    struct {
        uint32_t index;
        uint32_t version;
    };

    uint64_t value;

    constexpr tinyHandle() noexcept : value(UINT64_MAX) {}
    constexpr tinyHandle(uint32_t index, uint32_t version = 0) noexcept : index(index), version(version) {}

    constexpr bool operator==(const tinyHandle& other) const noexcept { return value == other.value; }
    constexpr bool operator!=(const tinyHandle& other) const noexcept { return value != other.value; }

    constexpr explicit operator bool() const noexcept { return value != UINT64_MAX; }

    constexpr void invalidate() noexcept { value = UINT64_MAX; }
};

namespace std {
    template<> struct hash<tinyHandle> {
        size_t operator()(const tinyHandle& h) const noexcept {
            return std::hash<uint64_t>()(h.value);
        }
    };
}

// -------------------- typeHandle --------------------

struct typeHandle {
    union {
        struct {
            tinyHandle handle;
            uint64_t typeHash;
        };
        struct {
            uint64_t hashLow;
            uint64_t hashHigh;
        };
    };

    std::type_index typeIndex = std::type_index(typeid(void));

    typeHandle() noexcept : handle(), typeHash(0), typeIndex(typeid(void)) {}
    typeHandle(tinyHandle h, const std::type_index& tIndex) noexcept : handle(h), typeIndex(tIndex) {
        typeHash = std::hash<std::type_index>()(tIndex);
    }

    template<typename T>
    [[nodiscard]] static typeHandle make(tinyHandle h = tinyHandle()) noexcept {
        typeHandle th;
        th.handle = h;
        th.typeIndex = std::type_index(typeid(T));
        th.typeHash = std::hash<std::type_index>()(th.typeIndex);
        return th;
    }

    [[nodiscard]] static typeHandle make(const std::type_index& tIndex, tinyHandle h = tinyHandle()) noexcept {
        return typeHandle(h, tIndex);
    }

    [[nodiscard]] uint64_t hash() const noexcept {
        uint64_t h1 = std::hash<uint64_t>()(handle.value);
        uint64_t h2 = typeHash;
        // The numbers appeared to me in a dream
        // The numbers were promised to me 3000 years ago
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }

    constexpr bool operator==(const typeHandle& other) const noexcept {
        return handle == other.handle && typeIndex == other.typeIndex;
    }
    constexpr bool operator!=(const typeHandle& other) const noexcept {
        return !(*this == other);
    }

    constexpr explicit operator bool() const noexcept {
        return static_cast<bool>(handle) && typeIndex != std::type_index(typeid(void));
    }

    [[nodiscard]] bool hValid() const noexcept { return static_cast<bool>(handle); }

    template<typename T>
    [[nodiscard]] bool isType() const noexcept { return typeIndex == std::type_index(typeid(T)); }

    [[nodiscard]] bool sameType(const typeHandle& other) const noexcept {
        return typeIndex == other.typeIndex;
    }
};

namespace std {
    template<> struct hash<typeHandle> {
        size_t operator()(const typeHandle& th) const noexcept {
            return static_cast<size_t>(th.hash());
        }
    };
}
