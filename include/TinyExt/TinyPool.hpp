#pragma once

#include ".ext/Templates.hpp"

#include "TinyHandle.hpp"

#include <type_traits>
#include <stdexcept>
#include <utility>

// Helper traits for pointer types
template<typename T>
struct TinyPoolTraits {
    static constexpr bool is_pointer = std::is_pointer_v<T>;
    static constexpr bool is_unique_ptr = false;
    static constexpr bool is_shared_ptr = false;
};

template<typename T>
struct TinyPoolTraits<std::unique_ptr<T>> {
    static constexpr bool is_pointer = false;
    static constexpr bool is_unique_ptr = true;
    static constexpr bool is_shared_ptr = false;
    using element_type = T;
};

template<typename T>
struct TinyPoolTraits<std::shared_ptr<T>> {
    static constexpr bool is_pointer = false;
    static constexpr bool is_unique_ptr = false;
    static constexpr bool is_shared_ptr = true;
    using element_type = T;
};

// TinyPoolRaw with type-aware methods
template<typename Type>
struct TinyPool {
    TinyPool() = default;

    void alloc(uint32_t size) {
        // Push in reverse so that lower indices are used first
        for (uint32_t i = 0; i < size; ++i) {
            uint32_t index = static_cast<uint32_t>(items.size());
            items.emplace_back();
            states.emplace_back();
            states[index].occupied = false;
            states[index].version = 0;
            freeList.insert(freeList.begin(), index);
        }
    }

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
        if (!valid(handle)) return nullptr;

        if constexpr (TinyPoolTraits<Type>::is_unique_ptr ||
                    TinyPoolTraits<Type>::is_shared_ptr) {
            auto& ptr = items[handle.index];
            return ptr ? ptr.get() : nullptr;
        } else {
            return &items[handle.index];
        }
    }

    const Type* get(const TinyHandle& handle) const {
        return const_cast<TinyPool<Type>*>(this)->get(handle);
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

private:
    void remove(const TinyHandle& handle) {
        if (!valid(handle)) return;

        uint32_t index = handle.index;

        if constexpr (TinyPoolTraits<Type>::is_unique_ptr ||
                    TinyPoolTraits<Type>::is_shared_ptr) {
            items[index].reset();
        } else if constexpr (std::is_default_constructible_v<Type>) {
            items[index] = Type{};
        } else {
            items[index].~Type();
            new(&items[index]) Type{};
        }

        states[index].occupied = false;
        states[index].version++;

        freeList.push_back(index);
    }

    Type& getEmpty() {
        static Type empty{};
        return empty;
    }

    struct State {
        bool occupied = false;
        uint32_t version = 0;
    };

    std::vector<Type> items;
    std::vector<State> states;
    std::vector<uint32_t> freeList;

    // Pending removals for deferred deletion (some types require this)
    std::vector<TinyHandle> pendingRms;
};
