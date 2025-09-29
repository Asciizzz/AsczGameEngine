#pragma once

#include <cstdint>
#include <functional>

union TinyHandle {
    enum class Type : uint8_t {
        Mesh,
        Material,
        Texture,
        Skeleton,
        Animation,
        Node,
        Unknown = 255
    };
    
    struct {
        uint32_t index;
        uint16_t generation;
        uint8_t type;
        uint8_t owned;
    };

    // Full pack representation
    uint64_t value;

    TinyHandle() : value(UINT64_MAX) {}

    // Value operators
    bool operator==(const TinyHandle& other) const { return value == other.value; }
    bool operator!=(const TinyHandle& other) const { return value != other.value; }

    // Fast index setter
    TinyHandle& operator=(uint32_t newIndex) { index = newIndex; return *this; }

    static TinyHandle invalid() { return TinyHandle(); }
    bool isValid() const { return value != UINT64_MAX; }
    void invalidate() { value = UINT64_MAX; }

    bool isType(Type t) const { return static_cast<Type>(type) == t; }

    /**
    * Create a handle
    @param index The index in the respective pool/array
    @param generation The generation for safety checks (default 0)
    @param type The type of the handle
    @param owned Whether the handle is owned or not
    */
    static TinyHandle make(uint32_t index, uint16_t generation, Type type, bool owned) {
        TinyHandle handle;
        handle.index = index;
        handle.generation = generation;
        handle.type = static_cast<uint8_t>(type);
        handle.owned = owned ? 1 : 0;
        return handle;
    }

    // First generation, owned by default
    static TinyHandle make(uint32_t index, Type type=Type::Unknown, bool owned=true) {
        return make(index, 0, type, owned);
    }
};

namespace std {
    template<> struct hash<TinyHandle> {
        size_t operator()(const TinyHandle& h) const noexcept {
            return std::hash<uint64_t>()(h.value);
        }
    };
}