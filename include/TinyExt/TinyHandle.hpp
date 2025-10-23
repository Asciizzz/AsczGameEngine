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

    constexpr bool valid() const { return value != UINT64_MAX && index != UINT32_MAX; }
    constexpr bool invalid() const { return !valid(); }
    constexpr void invalidate() { value = UINT64_MAX; }
};

namespace std {
    template<> struct hash<TinyHandle> {
        size_t operator()(const TinyHandle& h) const noexcept {
            return std::hash<uint64_t>()(h.value);
        }
    };
}

#include <typeindex>
struct TypeHandle {
    TinyHandle handle;
    size_t typeHash;

    TypeHandle() : typeHash(0) {}

    template<typename T>
    static TypeHandle make(TinyHandle h) {
        TypeHandle th;
        th.handle = h;
        th.typeHash = typeid(T).hash_code();
        return th;
    }

    bool valid() const { return handle.valid() && typeHash != 0; }

    template<typename T>
    bool isType() const { return valid() && typeHash == typeid(T).hash_code(); }
};