#pragma once

#include "tinyType.hpp"

#include <new>
#include <deque>
#include <vector>
#include <utility>
#include <cstring>
#include <type_traits>

#if defined(__GNUC__) || defined(__clang__)
    #define TINY_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define TINY_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define TINY_FORCE_INLINE __attribute__((always_inline)) inline
    #define TINY_PREFETCH(addr) __builtin_prefetch((const char*)(addr), 0, 3)
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

    [[nodiscard]] TINY_FORCE_INLINE uint32_t count() const noexcept { return static_cast<uint32_t>(denseData_.size()); }
    [[nodiscard]] TINY_FORCE_INLINE uint32_t capacity() const noexcept { return static_cast<uint32_t>(denseData_.capacity()); }

    void reserve(uint32_t n) {
        denseData_.reserve(n);
        denseIDs_.reserve(n);
        sparse_.reserve(n);
        versions_.reserve(n);
    }

    // -------------------------- Emplace --------------------------
    template<typename... Args>
    tinyHandle emplace(Args&&... args) {
        uint32_t index;
        uint16_t ver = 0;

        // reuse free handle index if available
        if (!freeList_.empty()) {
            index = freeList_.back();
            freeList_.pop_back();
            ver = versions_[index];
        } else {
            index = static_cast<uint32_t>(versions_.size());
            versions_.push_back(0);
            sparse_.push_back(UINT32_MAX);
        }

        // dense array position
        uint32_t densePos = static_cast<uint32_t>(denseData_.size());
        denseIDs_.push_back(index);

        // construct object in-place
        denseData_.emplace_back(std::forward<Args>(args)...);

        // map handle index -> dense position
        sparse_[index] = densePos;

        return tinyHandle(index, ver, TYPE_ID);
    }

    // -------------------------- Get --------------------------
    T* get(tinyHandle h) noexcept {
        if (TINY_UNLIKELY(!h || h.typeID != TYPE_ID)) return nullptr;
        uint32_t idx = h.index;
        if (idx >= sparse_.size()) return nullptr;
        uint32_t densePos = sparse_[idx];
        if (densePos == UINT32_MAX) return nullptr;
        if (versions_[idx] != h.ver()) return nullptr;
        TINY_PREFETCH(&denseData_[densePos + 1 < denseData_.size() ? densePos + 1 : densePos]);
        return &denseData_[densePos];
    }

    const T* get(tinyHandle h) const noexcept { return const_cast<tinyPool*>(this)->get(h); }

    // -------------------------- Erase --------------------------
    void erase(tinyHandle h) noexcept {
        if (TINY_UNLIKELY(!h || h.typeID != TYPE_ID)) return;
        uint32_t idx = h.index;

        if (idx >= sparse_.size()) return;
        if (versions_[idx] != h.ver()) return;
        uint32_t densePos = sparse_[idx];
        if (densePos == UINT32_MAX) return;

        uint32_t last = static_cast<uint32_t>(denseData_.size() - 1);

        // swap last element into erased slot if not last
        if (densePos != last) {
            denseData_[densePos] = std::move(denseData_[last]);
            uint32_t movedID = denseIDs_[last];
            denseIDs_[densePos] = movedID;
            sparse_[movedID] = densePos;
        }

        denseData_.pop_back();
        denseIDs_.pop_back();

        // mark sparse slot free and bump version
        sparse_[idx] = UINT32_MAX;
        ++versions_[idx];
        freeList_.push_back(idx);
    }

    // -------------------------- Clear --------------------------
    void clear() noexcept {
        // destroy non-trivial objects
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (T& obj : denseData_) obj.~T();
        }

        denseData_.clear();
        denseIDs_.clear();

        // mark all sparse slots free, increment version
        for (uint32_t i = 0; i < sparse_.size(); ++i) {
            sparse_[i] = UINT32_MAX;
            ++versions_[i];
        }

        // rebuild free list
        freeList_.clear();
        freeList_.reserve(sparse_.size());
        for (uint32_t i = 0; i < sparse_.size(); ++i) freeList_.push_back(i);
    }

    // -------------------------- Iterate --------------------------
    template<typename F>
    void forEach(F&& f) {
        for (uint32_t i = 0; i < denseData_.size(); ++i) {
            f(denseData_[i], denseIDs_[i]);
        }
    }

private:
    std::deque<T>         denseData_;   // tightly packed elements
    std::vector<uint32_t> denseIDs_;    // corresponding handle indices
    std::vector<uint32_t> sparse_;      // maps handle index -> dense position, UINT32_MAX = free
    std::vector<uint16_t> versions_;    // version per handle index
    std::vector<uint32_t> freeList_;    // recycled indices
};
