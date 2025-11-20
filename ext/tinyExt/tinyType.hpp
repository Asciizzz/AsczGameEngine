#pragma once

#include <cstdint>
#include <functional>

namespace tinyType {
    using ID = uint64_t;

    namespace detail {
        inline ID globalCounter = 0;
    }

    template<typename T>
    ID TypeID() noexcept {
        static ID typeID = detail::globalCounter++;
        return typeID;
    }

    template<typename T>
    ID TypeID(const T&) noexcept {
        return TypeID<T>();
    }
};

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
    tinyHandle handle;
    tinyType::ID typeID = tinyType::TypeID<void>();

    constexpr typeHandle() noexcept = default;
    constexpr typeHandle(tinyType::ID tID, const tinyHandle& h) noexcept : handle(h), typeID(tID) {}

    constexpr bool operator==(const typeHandle& other) const noexcept {
        return handle == other.handle && typeID == other.typeID;
    }
    constexpr bool operator!=(const typeHandle& other) const noexcept {
        return !(*this == other);
    }

    constexpr explicit operator bool() const noexcept { 
        return static_cast<bool>(handle) && typeID != tinyType::TypeID<void>();
    }

    template<typename T>
    static typeHandle make(tinyHandle h = tinyHandle()) noexcept {
        return typeHandle(tinyType::TypeID<T>(), h);
    }

    template<typename T>
    [[nodiscard]] bool isType() const noexcept {
        return typeID == tinyType::TypeID<T>();
    }

    [[nodiscard]] bool sameType(const typeHandle& other) const noexcept {
        return typeID == other.typeID;
    }

    [[nodiscard]] bool hvalid() const noexcept { return static_cast<bool>(handle); }

    [[nodiscard]] uint64_t hash() const noexcept {
        uint64_t h1 = handle.value;
        uint64_t h2 = typeID;
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

namespace std {
    template<> struct hash<typeHandle> {
        size_t operator()(const typeHandle& th) const noexcept {
            return static_cast<size_t>(th.hash());
        }
    };
}
