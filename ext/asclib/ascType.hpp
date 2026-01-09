#pragma once

#include <cstdint>
#include <functional>

namespace Asc {

namespace Type {
    /*
    There is no chance in hell you are using over 65k types
    Just >50 is already hard enough to manage
    
    Keep in mind, void is 0, UINT16_MAX is invalid, big difference
    */
    using ID = uint16_t;

    namespace detail {
        inline ID globalCounter = 1;
    }

    template<typename T>
    ID TypeID() noexcept {
        // Return 0 for void type as a special case
        if constexpr (std::is_void_v<T>) return 0;

        static ID typeID = detail::globalCounter++;
        return typeID;
    }

    template<typename T>
    ID TypeID(const T&) noexcept {
        return TypeID<T>();
    }
};

// -------------------- Handle --------------------

union Handle {
    uint64_t value;

    struct {
        uint32_t index;
        uint16_t version;
        Type::ID typeID;
    };

    static constexpr uint64_t INVALID_VAL = UINT64_MAX;

    constexpr Handle() noexcept : value(INVALID_VAL) {}
    constexpr Handle(uint32_t index, uint16_t version = 0, Type::ID typeID = 0) noexcept : index(index), version(version), typeID(typeID) {}

    template<typename T>
    static constexpr Handle make(uint32_t index, uint16_t version = 0) noexcept {
        return Handle(index, version, Type::TypeID<T>());
    }

    constexpr bool operator==(Handle o) const noexcept { return value == o.value; }
    constexpr bool operator!=(Handle o) const noexcept { return value != o.value; } 
    // I dont see a reason for comparison lol

    [[nodiscard]] bool valid() const noexcept { return value != INVALID_VAL; }
    [[nodiscard]] constexpr operator bool() const noexcept { return value != INVALID_VAL; }
    constexpr void invalidate() noexcept { value = INVALID_VAL; }

    template<typename T>
    [[nodiscard]] constexpr bool is() const noexcept { return typeID == Asc::Type::TypeID<T>(); }
    [[nodiscard]] constexpr bool is(Type::ID tID) const noexcept { return typeID == tID; }

    [[nodiscard]] constexpr Type::ID tID() const noexcept { return typeID; }
    [[nodiscard]] constexpr uint32_t idx() const noexcept { return index; }
    [[nodiscard]] constexpr uint16_t ver() const noexcept { return version; }
    [[nodiscard]] constexpr uint64_t raw() const noexcept { return value; }
};

}

namespace std {
    template<> struct hash<Asc::Handle> {
        size_t operator()(const Asc::Handle& h) const noexcept {
            return std::hash<uint64_t>()(h.value);
        }
    };
}