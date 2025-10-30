#pragma once

#include "tinyHandle.hpp"

#include <type_traits>
#include <utility>
#include <memory>
#include <deque>

// Helper traits for pointer types
template<typename T>
struct tinyPoolTraits {
    static constexpr bool is_pointer = std::is_pointer_v<T>;
    static constexpr bool is_unique_ptr = false;
    static constexpr bool is_shared_ptr = false;
};

template<typename T>
struct tinyPoolTraits<std::unique_ptr<T>> {
    static constexpr bool is_pointer = false;
    static constexpr bool is_unique_ptr = true;
    static constexpr bool is_shared_ptr = false;
    using element_type = T;
};

template<typename T>
struct tinyPoolTraits<std::shared_ptr<T>> {
    static constexpr bool is_pointer = false;
    static constexpr bool is_unique_ptr = false;
    static constexpr bool is_shared_ptr = true;
    using element_type = T;
};

// tinyPoolRaw with type-aware methods
template<typename Type>
struct tinyPool {
    tinyPool() noexcept = default;

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

    uint32_t count() const noexcept {
        return items.size() - freeList.size();
    }

    void clear() noexcept {
        items.clear();
        states.clear();
        freeList.clear();
    }

    bool valid(tinyHandle handle) const noexcept {
        return  isOccupied(handle.index) &&
                states[handle.index].version == handle.version;
    }

    bool isOccupied(uint32_t index) const noexcept {
        return index < items.size() && states[index].occupied;
    }


    template<typename U>
    tinyHandle add(U&& item) {
        uint32_t index;

        // Reuse a slot from freeList if available
        if (!freeList.empty()) {
            index = freeList.back();
            freeList.pop_back();
        } else {
            index = static_cast<uint32_t>(items.size());
            items.emplace_back();   // default construct
            states.emplace_back();
        }

        // Construct or assign the value
        if constexpr (std::is_assignable_v<Type&, U&&>) {
            items[index] = std::forward<U>(item);
        } else {
            // Non-assignable: use placement new
            items[index].~Type();
            new (&items[index]) Type(std::forward<U>(item));
        }

        states[index].occupied = true;
        return tinyHandle(index, states[index].version);
    }

    // ---- Getters (return true type, no raw ptr deduction) ----

    Type* get(const tinyHandle& handle) noexcept {
        return valid(handle) ? &items[handle.index] : nullptr;
    }

    const Type* get(const tinyHandle& handle) const noexcept {
        return const_cast<tinyPool<Type>*>(this)->get(handle);
    }

    // Get handle by index (useful for accessing items by their position)
    tinyHandle getHandle(uint32_t index) const noexcept {
        return isOccupied(index) ? tinyHandle(index, states[index].version) : tinyHandle();
    }

    // Reference is better since this is never null
    std::deque<Type>& view() noexcept { return items; }
    const std::deque<Type>& view() const noexcept { return items; }

    Type* data() noexcept { return items.data(); }
    const Type* data() const noexcept { return items.data(); }

    // ---- CRITICAL: Aware removal ----

    void instaRm(const tinyHandle& handle) noexcept {
        remove(handle);
    }

    void queueRm(const tinyHandle& handle) noexcept {
        if (valid(handle)) pendingRms.push_back(handle);
    }

    const std::vector<tinyHandle>& listRms() const noexcept {
        return pendingRms;
    }

    void flushRm(uint32_t index) noexcept {
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
    void remove(const tinyHandle& handle) noexcept {
        if (!valid(handle)) return;

        uint32_t index = handle.index;

        if constexpr (tinyPoolTraits<Type>::is_unique_ptr || tinyPoolTraits<Type>::is_shared_ptr) {
            items[index].reset();
        } else {
            items[index].~Type();
            new (&items[index]) Type();
        }

        states[index].occupied = false;
        states[index].version++;

        freeList.push_back(index);
    }

    Type& getEmpty() noexcept {
        static Type empty{};
        return empty;
    }

    struct State {
        bool occupied = false;
        uint32_t version = 0;
    };

    std::deque<Type> items;
    std::vector<State> states;
    std::vector<uint32_t> freeList;

    // Pending removals for deferred deletion (some types require this)
    std::vector<tinyHandle> pendingRms;
};
