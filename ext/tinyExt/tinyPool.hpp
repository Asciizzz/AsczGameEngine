#pragma once

#include "tinyType.hpp"

#include <type_traits>
#include <utility>
#include <memory>
#include <deque>
#include <cstring>

// Compiler hints for branch prediction
#if defined(__GNUC__) || defined(__clang__)
    #define TINY_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define TINY_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define TINY_LIKELY(x)   (x)
    #define TINY_UNLIKELY(x) (x)
#endif

// Force inline for hot path functions
#if defined(_MSC_VER)
    #define TINY_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define TINY_FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define TINY_FORCE_INLINE inline
#endif

// Prefetch hint for cache optimization
#if defined(__GNUC__) || defined(__clang__)
    #define TINY_PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#elif defined(_MSC_VER)
    #include <xmmintrin.h>
    #define TINY_PREFETCH(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
#else
    #define TINY_PREFETCH(addr) ((void)0)
#endif

// Helper traits for pointer types
template<typename T>
struct tinyPoolTraits {
    static constexpr bool is_pointer = std::is_pointer_v<T>;
    static constexpr bool is_unique_ptr = false;
    static constexpr bool is_shared_ptr = false;
    static constexpr bool is_trivial = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;
};

template<typename T>
struct tinyPoolTraits<std::unique_ptr<T>> {
    static constexpr bool is_pointer = false;
    static constexpr bool is_unique_ptr = true;
    static constexpr bool is_shared_ptr = false;
    static constexpr bool is_trivial = false;
    using element_type = T;
};

template<typename T>
struct tinyPoolTraits<std::shared_ptr<T>> {
    static constexpr bool is_pointer = false;
    static constexpr bool is_unique_ptr = false;
    static constexpr bool is_shared_ptr = true;
    static constexpr bool is_trivial = false;
    using element_type = T;
};

// tinyPoolRaw with type-aware methods
template<typename Type>
struct tinyPool {
    tinyPool() noexcept = default;

    void alloc(uint32_t size) {
        if (size == 0) return;
        
        uint32_t startIndex = static_cast<uint32_t>(items_.size());
        
        // Reserve space to avoid reallocations
        items_.reserve(items_.size() + size);
        states_.reserve(states_.size() + size);
        freeList_.reserve(freeList_.size() + size);
        
        // Push in reverse so that lower indices are used first
        for (uint32_t i = 0; i < size; ++i) {
            uint32_t index = startIndex + i;
            items_.emplace_back();
            states_.emplace_back();
            states_[index].setOccupied(false);
            states_[index].version = 0;
            freeList_.insert(freeList_.begin(), index);
        }
    }

    [[nodiscard]] TINY_FORCE_INLINE uint32_t count() const noexcept {
        return static_cast<uint32_t>(items_.size() - freeList_.size());
    }
    
    [[nodiscard]] TINY_FORCE_INLINE uint32_t capacity() const noexcept {
        return static_cast<uint32_t>(items_.size());
    }

    void clear() noexcept {
        // Fast path for trivial types - use memset instead of destructors
        if constexpr (tinyPoolTraits<Type>::is_trivial && sizeof(Type) <= 64) {
            // For small trivial types, bulk clear is faster
            if (!items_.empty()) {
                std::memset(&items_[0], 0, items_.size() * sizeof(Type));
            }
        }
        
        items_.clear();
        states_.clear();
        freeList_.clear();
        pendingRms_.clear();
    }

    // Hot path: inlined and branch-predicted
    [[nodiscard]] TINY_FORCE_INLINE bool valid(tinyHandle handle) const noexcept {
        // Hint: most handles in hot loops are valid
        if (TINY_LIKELY(handle.index < states_.size())) {
            const State& state = states_[handle.index];
            return TINY_LIKELY(state.isOccupied() && state.version == handle.version);
        }
        return false;
    }

    [[nodiscard]] TINY_FORCE_INLINE bool isOccupied(uint32_t index) const noexcept {
        return TINY_LIKELY(index < items_.size()) && states_[index].isOccupied();
    }


    template<typename U>
    tinyHandle add(U&& item) {
        uint32_t index;

        // Hot path: reuse slot (more common than grow)
        if (TINY_LIKELY(!freeList_.empty())) {
            index = freeList_.back();
            freeList_.pop_back();
            
            // Optimization: for trivial types, use memcpy instead of assignment
            if constexpr (tinyPoolTraits<Type>::is_trivial && std::is_same_v<std::decay_t<U>, Type>) {
                std::memcpy(&items_[index], &item, sizeof(Type));
            } else if constexpr (std::is_assignable_v<Type&, U&&>) {
                items_[index] = std::forward<U>(item);
            } else {
                // Non-assignable: use placement new
                items_[index].~Type();
                new (&items_[index]) Type(std::forward<U>(item));
            }
        } else {
            // Cold path: grow (less common)
            index = static_cast<uint32_t>(items_.size());
            items_.emplace_back(std::forward<U>(item));
            states_.emplace_back();
        }

        states_[index].setOccupied(true);
        return tinyHandle(index, states_[index].version);
    }

    // ---- Getters (return true type, no raw ptr deduction) ----

    // Hot path: force inline + branch prediction + prefetching
    [[nodiscard]] TINY_FORCE_INLINE Type* get(const tinyHandle& handle) noexcept {
        // Fast path: bounds check with likely hint
        if (TINY_LIKELY(handle.index < states_.size())) {
            const State& state = states_[handle.index];
            
            // Version check with likely hint (most accesses are valid)
            if (TINY_LIKELY(state.isOccupied() && state.version == handle.version)) {
                Type* ptr = &items_[handle.index];
                
                // Prefetch next element for better cache performance in loops
                if (TINY_LIKELY(handle.index + 1 < items_.size())) {
                    TINY_PREFETCH(&items_[handle.index + 1]);
                }
                
                return ptr;
            }
        }
        return nullptr;
    }

    [[nodiscard]] TINY_FORCE_INLINE const Type* get(const tinyHandle& handle) const noexcept {
        return const_cast<tinyPool<Type>*>(this)->get(handle);
    }

    // Get handle by index (useful for accessing items_ by their position)
    [[nodiscard]] tinyHandle getHandle(uint32_t index) const noexcept {
        return isOccupied(index) ? tinyHandle(index, states_[index].version) : tinyHandle();
    }

    // Reference is better since this is never null
    [[nodiscard]] std::deque<Type>& view() noexcept { return items_; }
    [[nodiscard]] const std::deque<Type>& view() const noexcept { return items_; }

    // ---- CRITICAL: Aware removal ----

    void remove(const tinyHandle& handle) noexcept {
        if (TINY_UNLIKELY(!valid(handle))) return;

        uint32_t index = handle.index;

        // Optimize destruction for different types
        if constexpr (tinyPoolTraits<Type>::is_unique_ptr || tinyPoolTraits<Type>::is_shared_ptr) {
            items_[index].reset();
        } else if constexpr (tinyPoolTraits<Type>::is_trivial) {
            // For trivial types, just zero out memory (faster than destructor + placement new)
            std::memset(&items_[index], 0, sizeof(Type));
        } else {
            // Non-trivial types need proper destruction
            items_[index].~Type();
            new (&items_[index]) Type();
        }

        states_[index].setOccupied(false);
        states_[index].version++;

        freeList_.push_back(index);
    }

    void queueRm(const tinyHandle& handle) noexcept {
        if (valid(handle)) pendingRms_.push_back(handle);
    }

    [[nodiscard]] const std::vector<tinyHandle>& pendingRms() const noexcept {
        return pendingRms_;
    }

    void flushRm(uint32_t index) noexcept {
        if (index >= pendingRms_.size()) return;
        remove(pendingRms_[index]);
    }

    void flushAllRms() {
        for (const auto& handle : pendingRms_) remove(handle);
        pendingRms_.clear();
    }

    [[nodiscard]] bool hasPendingRms() const noexcept {
        return !pendingRms_.empty();
    }

private:

    Type& getEmpty() noexcept {
        static Type empty{};
        return empty;
    }

    // Cache-aligned state for better performance
    // Pack version and occupied into single 64-bit value for atomic-friendly layout
    struct alignas(8) State {
        uint32_t version = 0;
        uint32_t occupied = 0;  // Use uint32 instead of bool for better alignment
        
        TINY_FORCE_INLINE bool isOccupied() const noexcept {
            return occupied != 0;
        }
        
        TINY_FORCE_INLINE void setOccupied(bool val) noexcept {
            occupied = val ? 1u : 0u;
        }
    };

    std::deque<Type> items_;
    std::vector<State> states_;
    std::vector<uint32_t> freeList_;

    // Pending removals for deferred deletion (some types require this)
    std::vector<tinyHandle> pendingRms_;
};
