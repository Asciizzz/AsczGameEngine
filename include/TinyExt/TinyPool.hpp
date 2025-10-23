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
    TinyPool() = default;

    uint32_t count() const {
        return items.size() - freeList.size();
    }

    void clear() {
        items.clear();
        states.clear();
        freeList.clear();
    }

    bool valid(TinyHandle handle) const {
        return  isOccupied(handle.index) &&
                states[handle.index].version == handle.version;
    }

    bool isOccupied(uint32_t index) const {
        return index < items.size() && states[index].occupied;
    }

    // ---- Type-aware add ----
    template<typename U>
    TinyHandle add(U&& item) {
        uint32_t index;

        // Check if we can reuse a slot from the free list
        if (!freeList.empty()) {
            index = freeList.back();
            freeList.pop_back();
        } else {
            // No free slots, add new item to the end
            index = static_cast<uint32_t>(items.size());
            items.emplace_back();
            states.emplace_back();
        }

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
        } else if constexpr (std::is_copy_assignable_v<Type> || std::is_move_assignable_v<Type>) {
            items[index] = std::forward<U>(item);
        } else {
            // For non-assignable types, use placement new
            items[index].~Type();
            new(&items[index]) Type(std::forward<U>(item));
        }

        states[index].occupied = true;

        return TinyHandle(index, states[index].version);
    }

    // ---- Getters ----

    Type* get(const TinyHandle& handle) {
        bool isValid = valid(handle);

        if constexpr (TinyPoolTraits<Type>::is_unique_ptr) {
            return isValid ? items[handle.index].get() : nullptr;
        } else {
            return isValid ? &items[handle.index] : nullptr;
        }
    }

    const Type* get(const TinyHandle& handle) const {
        return const_cast<TinyPool<Type>*>(this)->get(handle);
    }

    // Reserved for special cases
    Type* get(uint32_t index) {
        if (isOccupied(index)) {
            if constexpr(TinyPoolTraits<Type>::is_unique_ptr ||
                        TinyPoolTraits<Type>::is_shared_ptr) {
                return items[index].get();
            } else {
                return &items[index];
            }
        }
        return nullptr;
    }

    const Type* get(uint32_t index) const {
        return const_cast<TinyPool<Type>*>(this)->get(index);
    }

    // Get handle by index (useful for accessing items by their position)
    TinyHandle getHandle(uint32_t index) const {
        if (isOccupied(index)) return TinyHandle(index, states[index].version);
        return TinyHandle(); // Invalid handle
    }

    // Reference is better since this is never null
    std::vector<Type>& view() { return items; }
    const std::vector<Type>& view() const { return items; }

    Type* data() { return items.data(); }
    const Type* data() const { return items.data(); }

    // ---- CRITICAL: Aware removal ----

    void instaRm(const TinyHandle& handle) {
        remove(handle);
    }

    void queueRm(const TinyHandle& handle) {
        if (valid(handle)) pendingRms.push_back(handle);
    }

    const std::vector<TinyHandle>& listRms() const {
        return pendingRms;
    }

    void flushRm(uint32_t index) {
        if (index >= pendingRms.size()) return;
        remove(pendingRms[index]);
    }

    void flushAllRms() {
        for (const auto& handle : pendingRms) remove(handle);
        pendingRms.clear();
    }

    bool hasPendingRms() const {
        return !pendingRms.empty();
    }


    // Soon to be private
    void remove(const TinyHandle& handle) {
        if (!valid(handle)) return;

        uint32_t index = handle.index;

        if constexpr(TinyPoolTraits<Type>::is_unique_ptr ||
                    TinyPoolTraits<Type>::is_shared_ptr) {
            items[index].reset();
        } else if constexpr(std::is_copy_assignable_v<Type>) {
            items[index] = {};
        } else {
            // For non-copyable types, use placement new to reconstruct
            items[index].~Type();
            new(&items[index]) Type{};
        }

        states[index].occupied = false;
        states[index].version++;

        freeList.push_back(index);
    }

private:
    struct State {
        bool occupied = false;
        uint32_t version = 0;
    };

    TinyPoolType poolType = TinyPoolTraits<Type>::poolType;

    std::vector<Type> items;
    std::vector<State> states;
    std::vector<uint32_t> freeList;

    // Pending removals for deferred deletion (some types require this)
    std::vector<TinyHandle> pendingRms;
};
