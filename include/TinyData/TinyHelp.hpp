#pragma once

#include <cstdint>
#include <functional>

enum class TinyScope {
    Local,
    Global
};

union TinyHandle {
    enum class Type : uint8_t {
        Mesh,
        Material,
        Texture,
        Skeleton,
        Animation,
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

    Type getType() const { return static_cast<Type>(type); }
    uint32_t getIndex() const { return index; }
    uint16_t getGeneration() const { return generation; }
    bool isOwned() const { return owned == 1; }

    bool operator==(const TinyHandle& other) const { return value == other.value; }
    bool operator!=(const TinyHandle& other) const { return value != other.value; }

    static TinyHandle invalid() { return TinyHandle(); }
    bool isValid() const { return value != UINT64_MAX; }

    static TinyHandle make(uint32_t index, uint16_t generation, Type type, bool owned) {
        TinyHandle handle;
        handle.index = index;
        handle.generation = generation;
        handle.type = static_cast<uint8_t>(type);
        handle.owned = owned ? 1 : 0;
        return handle;
    }
};

namespace std {
    template<> struct hash<TinyHandle> {
        size_t operator()(const TinyHandle& h) const noexcept {
            return std::hash<uint64_t>()(h.value);
        }
    };
}