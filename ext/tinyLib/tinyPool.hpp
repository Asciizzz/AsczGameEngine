#pragma once

#include "tinyType.hpp"

#include <cstdint>
#include <deque>
#include <vector>
#include <cstring>
#include <type_traits>
#include <utility>

#if defined(__GNUC__) || defined(__clang__)
    #define TINY_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define TINY_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define TINY_FORCE_INLINE __attribute__((always_inline)) inline
    #define TINY_PREFETCH(addr) __builtin_prefetch((const char*)(addr), 0, 3)
#elif defined(_MSC_VER)
    #define TINY_LIKELY(x)   (x)
    #define TINY_UNLIKELY(x) (x)
    #define TINY_FORCE_INLINE __forceinline
    #include <xmmintrin.h>
    #define TINY_PREFETCH(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
#else
    #define TINY_LIKELY(x)   (x)
    #define TINY_UNLIKELY(x) (x)
    #define TINY_FORCE_INLINE inline
    #define TINY_PREFETCH(addr) ((void)0)
#endif

template<typename T>
struct tinyPool {
    inline static const tinyType::ID TYPE_ID = tinyType::TypeID<T>();

    tinyPool() noexcept = default;

    void reserve(uint32_t n) {
        if (n <= items_.size()) return;

        uint32_t oldSize = static_cast<uint32_t>(items_.size());
        items_.resize(oldSize + n);
        states_.resize(oldSize + n);

        for (uint32_t i = oldSize; i < items_.size(); ++i) {
            states_[i].setVersion(0);
            states_[i].setOccupied(false);
            freeList_.push_back(i);
        }
    }

    [[nodiscard]] TINY_FORCE_INLINE uint32_t count() const noexcept {
        return static_cast<uint32_t>(items_.size() - freeList_.size());
    }

    [[nodiscard]] TINY_FORCE_INLINE uint32_t capacity() const noexcept {
        return static_cast<uint32_t>(items_.size());
    }

    void clear() noexcept {
        if constexpr (std::is_trivially_destructible_v<T>) {
            items_.clear();
        } else {
            for (auto& item : items_) item.~T();
            items_.clear();
        }
        states_.clear();
        freeList_.clear();
    }

    template<typename... Args>
    [[nodiscard]] TINY_FORCE_INLINE tinyHandle emplace(Args&&... args) {
        uint32_t index;
        uint16_t version = 0;

        if (TINY_LIKELY(!freeList_.empty())) {
            index = freeList_.back();
            freeList_.pop_back();
            version = states_[index].version();
        } else {
            index = static_cast<uint32_t>(items_.size());
            items_.emplace_back();
            states_.emplace_back();
        }

        // Construct in-place
        if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= 32) {
            std::memset(&items_[index], 0, sizeof(T)); // optional zero-init
        }

        new (&items_[index]) T(std::forward<Args>(args)...);

        states_[index].setOccupied(true);

        return tinyHandle(index, version, TYPE_ID);
    }

    [[nodiscard]] TINY_FORCE_INLINE T* get(tinyHandle h) noexcept {
        if (TINY_LIKELY(h && h.typeID == TYPE_ID)) {
            const uint32_t idx = h.index;
            if (TINY_LIKELY(idx < states_.size())) {
                const State& s = states_[idx];
                if (TINY_LIKELY(s.isOccupied() && s.version() == h.ver())) {
                    TINY_PREFETCH(&items_[idx + 1 < items_.size() ? idx + 1 : idx]);
                    return &items_[idx];
                }
            }
        }
        return nullptr;
    }

    [[nodiscard]] TINY_FORCE_INLINE const T* get(tinyHandle h) const noexcept {
        return const_cast<tinyPool*>(this)->get(h);
    }

    void erase(tinyHandle h) noexcept {
        if (TINY_UNLIKELY(!h || h.typeID != TYPE_ID)) return;
        if (TINY_UNLIKELY(h.index >= states_.size())) return;

        State& s = states_[h.index];
        if (TINY_UNLIKELY(!s.isOccupied() || s.version() != h.ver())) return;

        if constexpr (!std::is_trivially_destructible_v<T>) {
            items_[h.index].~T();
        }

        s.setOccupied(false);
        s.markFree();
        freeList_.push_back(h.index);
    }

    [[nodiscard]] std::deque<T>& data() noexcept { return items_; }
    [[nodiscard]] const std::deque<T>& data() const noexcept { return items_; }

private:
    struct State {
        uint32_t data = 0;

        static constexpr uint32_t OCCUPIED = 1u << 0;
        static constexpr uint32_t VERSION_SHIFT = 16;

        TINY_FORCE_INLINE bool isOccupied() const noexcept {
            return (data & OCCUPIED) != 0;
        }

        TINY_FORCE_INLINE uint16_t version() const noexcept {
            return static_cast<uint16_t>(data >> VERSION_SHIFT);
        }

        TINY_FORCE_INLINE void setOccupied(bool val) noexcept {
            data = val ? (data | OCCUPIED) : (data & ~OCCUPIED);
        }

        TINY_FORCE_INLINE void markFree() noexcept {
            data &= ~OCCUPIED;
            data += (1u << VERSION_SHIFT);
        }

        TINY_FORCE_INLINE void reset() noexcept {
            data = 0;
        }

        TINY_FORCE_INLINE void setVersion(uint16_t v) noexcept {
            data = (data & 0x0000FFFFu) | (static_cast<uint32_t>(v) << VERSION_SHIFT);
        }
    };

    std::deque<T>           items_;
    std::vector<State>      states_;
    std::vector<uint32_t>   freeList_;  // back = fastest
};
