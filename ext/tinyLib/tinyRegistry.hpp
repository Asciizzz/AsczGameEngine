#pragma once

#include "tinyPool.hpp"

#include <unordered_map>
#include <memory>
#include <cassert>

class tinyRegistry {
    struct IPool {
        virtual ~IPool() noexcept = default;
        virtual void* get(tinyHandle h) noexcept = 0;
        virtual void erase(tinyHandle h) noexcept = 0;
        virtual void clear() noexcept = 0;
    };

    template<typename T>
    struct PoolWrapper : IPool {
        tinyPool<T> pool;

        void* get(tinyHandle h) noexcept override {
            return pool.get(h);
        }
        void erase(tinyHandle h) noexcept override {
            pool.erase(h);
        }
        void clear() noexcept override {
            pool.clear();
        }
    };

    std::unordered_map<tinyType::ID, std::unique_ptr<IPool>> pools_;

    template<typename T>
    TINY_FORCE_INLINE PoolWrapper<T>* getPool() noexcept {
        auto it = pools_.find(tinyType::TypeID<T>());
        return it != pools_.end() ? static_cast<PoolWrapper<T>*>(it->second.get()) : nullptr;
    }

    template<typename T>
    PoolWrapper<T>& ensurePool() {
        auto id = tinyType::TypeID<T>();
        auto [it, inserted] = pools_.try_emplace(id);
        if (inserted) {
            it->second = std::make_unique<PoolWrapper<T>>();
        }
        return *static_cast<PoolWrapper<T>*>(it->second.get());
    }

public:
    tinyRegistry() noexcept = default;

    template<typename T, typename... Args>
    [[nodiscard]] tinyHandle emplace(Args&&... args) {
        return ensurePool<T>().pool.emplace(std::forward<Args>(args)...);
    }

    template<typename T>
    void reserve(uint32_t capacity) {
        ensurePool<T>().pool.reserve(capacity);
    }

    template<typename T>
    [[nodiscard]] TINY_FORCE_INLINE T* get(tinyHandle h) noexcept {
        auto* pool = getPool<T>();
        return pool ? pool->pool.get(h) : nullptr;
    }

    template<typename T>
    [[nodiscard]] TINY_FORCE_INLINE const T* get(tinyHandle h) const noexcept {
        return const_cast<tinyRegistry*>(this)->get<T>(h);
    }

    [[nodiscard]] TINY_FORCE_INLINE void* get(tinyHandle h) noexcept {
        if (!h) return nullptr;
        auto it = pools_.find(h.typeID);
        return it != pools_.end() ? it->second->get(h) : nullptr;
    }

    void erase(tinyHandle h) noexcept {
        if (!h) return;
        auto it = pools_.find(h.typeID);
        if (it != pools_.end()) it->second->erase(h);
    }

    // View raw pools
    template<typename T>
    [[nodiscard]] tinyPool<T>& view() {
        return ensurePool<T>().pool;
    }

    template<typename T>
    [[nodiscard]] const tinyPool<T>& view() const {
        static tinyPool<T> empty;
        auto it = pools_.find(tinyType::TypeID<T>());
        return it != pools_.end()
            ? static_cast<PoolWrapper<T>*>(it->second.get())->pool
            : empty;
    }

    // Stats
    template<typename T>
    [[nodiscard]] uint32_t count() const noexcept {
        auto* p = getPool<T>();
        return p ? p->pool.count() : 0;
    }

    template<typename T>
    [[nodiscard]] uint32_t capacity() const noexcept {
        auto* p = getPool<T>();
        return p ? p->pool.capacity() : 0;
    }

    // Nukes
    void clearAll() noexcept {
        pools_.clear();
    }

    void clear(tinyType::ID typeID) noexcept {
        pools_.erase(typeID);
    }

    template<typename T>
    void clear() noexcept {
        clear(tinyType::TypeID<T>());
    }

    [[nodiscard]] bool empty() const noexcept { return pools_.empty(); }
    [[nodiscard]] size_t size() const noexcept { return pools_.size(); }
};