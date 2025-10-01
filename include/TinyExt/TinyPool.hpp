#pragma once

#include ".ext/Templates.hpp"

#include "TinyHandle.hpp"

#include <type_traits>
#include <stdexcept>
#include <utility>

enum class TinyPoolType {
    Raw,  // Direct storage
    UPtr, // Unique pointer storage
};

// Helper traits for pointer types
template<typename T>
struct TinyPoolTraits {
    static constexpr TinyPoolType poolType = TinyPoolType::Raw;
    static constexpr bool is_pointer = std::is_pointer_v<T>;
    static constexpr bool is_unique_ptr = false;
    static constexpr bool is_shared_ptr = false;
};

template<typename T>
struct TinyPoolTraits<std::unique_ptr<T>> {
    static constexpr TinyPoolType poolType = TinyPoolType::UPtr;
    static constexpr bool is_pointer = false;
    static constexpr bool is_unique_ptr = true;
    static constexpr bool is_shared_ptr = false;
    using element_type = T;
};

template<typename T>
struct TinyPoolTraits<std::shared_ptr<T>> {
    static constexpr TinyPoolType poolType = TinyPoolType::UPtr; // Treat same as unique_ptr for now
    static constexpr bool is_pointer = false;
    static constexpr bool is_unique_ptr = false;
    static constexpr bool is_shared_ptr = true;
    using element_type = T;
};

// TinyPoolRaw with type-aware methods
template<typename Type>
struct TinyPool {
    struct State {
        bool occupied = false;
        uint32_t version = 0;
    };

    TinyPool() = default;
    TinyPool(uint32_t initialCapacity, uint32_t capacityStep = 16)
    : expandStep(capacityStep) {
        allocate(initialCapacity);
    }

    // Delete copy semantics
    TinyPool(const TinyPool&) = delete;
    TinyPool& operator=(const TinyPool&) = delete;

    TinyPoolType poolType = TinyPoolTraits<Type>::poolType;

    std::vector<Type> items;
    std::vector<State> states;
    std::vector<uint32_t> freeList;

    uint32_t capacity = 0;
    uint32_t count = 0;

    uint32_t capacityStep = 16; // Default expansion step
    void setCapacityStep(uint32_t step) { capacityStep = step; }

    bool resizeFlag = false; // Helpful toggleable flag for register resize logics
    bool hasResized() const { return resizeFlag; }
    void resetResizeFlag() { resizeFlag = false; }

    void clear() {
        items.clear();
        states.clear();
        freeList.clear();
        capacity = 0;
        count = 0;
    }

    TinyPool& allocate(uint32_t capacity) {
        clear();

        this->capacity = capacity;
        items.resize(capacity);
        states.resize(capacity);

        freeList.reserve(capacity);
        for (uint32_t i = 0; i < capacity; ++i) {
            freeList.push_back(capacity - 1 - i);
        }
        return *this;
    }

    TinyPool& resize(uint32_t newCapacity) {
        if (newCapacity <= capacity) return *this;

        items.resize(newCapacity);
        states.resize(newCapacity);

        for (uint32_t i = newCapacity; i-- > capacity;) {
            freeList.push_back(i);
        }

        capacity = newCapacity;
        resizeFlag = true;
        return *this;
    }

    bool hasSpace() const { return !freeList.empty(); }

    bool isValid(uint32_t index) const {
        return index < items.size() && states[index].occupied;
    }

    void checkValid(uint32_t index) const {
        if (!isValid(index)) {
            throw std::runtime_error("TinyPool: Invalid index access");
        }
    }

    // ---- Type-aware insert ----
    template<typename U>
    uint32_t insert(U&& item) {
        while (!hasSpace()) resize(capacity + capacityStep);
        count++;

        uint32_t index = freeList.back();
        freeList.pop_back();

        if constexpr (TinyPoolTraits<Type>::is_unique_ptr) {
            if constexpr (std::is_same_v<std::decay_t<U>, Type>) {
                // If U is already a unique_ptr of the correct type, just move it
                items[index] = std::forward<U>(item);
            } else {
                // If U is the raw type, wrap it in a unique_ptr and move the data
                static_assert(std::is_move_constructible_v<std::decay_t<U>>, 
                    "Type must be move constructible to insert into unique_ptr pool");

                items[index] = std::make_unique<typename TinyPoolTraits<Type>::element_type>(std::move(item));
            }
        } else {
            items[index] = std::forward<U>(item);
        }

        states[index].occupied = true;
        states[index].version++;
        return index;
    }

    // ---- Remove ----
    void remove(uint32_t index) {
        if (!isValid(index)) return;
        count--;

        if constexpr (TinyPoolTraits<Type>::is_unique_ptr || TinyPoolTraits<Type>::is_shared_ptr) {
            items[index].reset();
        } else {
            items[index] = {};
        }

        states[index].occupied = false;
        freeList.push_back(index);
    }

    // ---- Get item data ----

    auto data() {
        if constexpr (TinyPoolTraits<Type>::is_unique_ptr) {
            return items.data()->get();
        } else {
            return items.data();
        }
    }

    // ---- Type-aware getters ----
    auto get(uint32_t index) {
        checkValid(index);
        if constexpr (TinyPoolTraits<Type>::is_unique_ptr) {
            return items[index].get();
        } else {
            return &items[index];
        }
    }

    auto get(uint32_t index) const {
        checkValid(index);
        if constexpr (TinyPoolTraits<Type>::is_unique_ptr) {
            return items[index].get();
        } else {
            return &items[index];
        }
    }
};
