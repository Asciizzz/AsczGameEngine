#pragma once

#include "tinyExt/tinyPool.hpp"

#include <assert.h>
#include <typeindex>
#include <unordered_map>

struct typeHandle {
    tinyHandle handle = tinyHandle();
    std::type_index typeIndex = std::type_index(typeid(void));

    typeHandle() noexcept = default;

    bool valid() const noexcept { return handle.valid() && typeIndex != std::type_index(typeid(void)); }

    template<typename T>
    bool isType() const noexcept { return valid() && typeIndex == std::type_index(typeid(T)); }

    template<typename T>
    static typeHandle make(tinyHandle h) noexcept {
        typeHandle th;
        th.handle = h;
        th.typeIndex = std::type_index(typeid(T));
        return th;
    }
};

class tinyRegistry { // For raw resource data
    struct IPool {
        virtual ~IPool() noexcept = default;
        virtual void* get(const tinyHandle& handle) noexcept = 0;
        virtual void instaRm(const tinyHandle& handle) noexcept = 0;
        virtual void queueRm(const tinyHandle& handle) noexcept = 0;
        virtual void flushRm(uint32_t index) noexcept = 0;
        virtual void flushAllRms() noexcept = 0;
        virtual bool hasPendingRms() const noexcept = 0;
        virtual std::vector<tinyHandle> pendingRms() const noexcept = 0;
        virtual void clear() noexcept = 0;
    };

    template<typename T>
    struct PoolWrapper : public IPool {
        tinyPool<T> pool;

        void* get(const tinyHandle& handle) noexcept override { return pool.get(handle); }
        void instaRm(const tinyHandle& handle) noexcept override { pool.instaRm(handle); }
        void queueRm(const tinyHandle& handle) noexcept override { pool.queueRm(handle); }
        void flushRm(uint32_t index) noexcept override { pool.flushRm(index); }
        void flushAllRms() noexcept override { pool.flushAllRms(); }
        bool hasPendingRms() const noexcept override { return pool.hasPendingRms(); }
        std::vector<tinyHandle> pendingRms() const noexcept override { return pool.pendingRms(); }
        void clear() noexcept override { pool.clear(); }
    };

    std::unordered_map<std::type_index, std::unique_ptr<IPool>> pools;

    template<typename T>
    PoolWrapper<T>* getWrapper() noexcept {
        auto it = pools.find(std::type_index(typeid(T)));
        if (it == pools.end()) return nullptr;
        return static_cast<PoolWrapper<T>*>(it->second.get());
    }

    template<typename T>
    const PoolWrapper<T>* getWrapper() const noexcept {
        auto it = pools.find(std::type_index(typeid(T)));
        if (it == pools.end()) return nullptr;
        return static_cast<const PoolWrapper<T>*>(it->second.get());
    }

    // Ensure pool exists for type T
    template<typename T>
    PoolWrapper<T>& ensurePool() {
        auto tIndx = std::type_index(typeid(T));
        auto it = pools.find(tIndx);

        if (it == pools.end()) { // Create new pool if not found
            auto wrapper = std::make_unique<PoolWrapper<T>>();
            auto* ptr = wrapper.get();
            pools[tIndx] = std::move(wrapper);

            return *ptr;
        }

        return *static_cast<PoolWrapper<T>*>(it->second.get());
    }

    // Remove execution
    void remove(const typeHandle& th) noexcept {
        auto it = pools.find(th.typeIndex);
        if (it != pools.end()) it->second->instaRm(th.handle);
    }

    template<typename T>
    void remove(const tinyHandle& handle) noexcept {
        remove(typeHandle::make<T>(handle));
    }

public:
    tinyRegistry() noexcept = default;

    tinyRegistry(const tinyRegistry&) = delete;
    tinyRegistry& operator=(const tinyRegistry&) = delete;

    tinyRegistry(tinyRegistry&&) noexcept = default;
    tinyRegistry& operator=(tinyRegistry&&) noexcept = default;

    template<typename T>
    typeHandle add(T&& data) {
        return typeHandle::make<T>(ensurePool<T>().pool.add(std::forward<T>(data)));
    }

    template<typename T>
    tinyPool<T>& make() {
        return ensurePool<T>().pool;
    }

    template<typename T>
    void tInstaRm(const tinyHandle& handle) noexcept { remove<T>(handle); }
    void tInstaRm(const typeHandle& th) noexcept { remove(th); }

// For unsafe removal (vulkan resources for example)
    void tQueueRm(const typeHandle& th) noexcept {
        auto it = pools.find(th.typeIndex);
        if (it != pools.end()) {
            it->second->queueRm(th.handle);
        }
    }
    template<typename T>
    void tQueueRm(const tinyHandle& handle) noexcept {
        tQueueRm(typeHandle::make<T>(handle));
    }

    void tFlushRm(std::type_index typeIndx, uint32_t index) noexcept {
        auto it = pools.find(typeIndx);
        if (it != pools.end()) {
            it->second->flushRm(index);
        }
    }
    template<typename T>
    void tFlushRm(uint32_t index) noexcept {
        tFlushRm(std::type_index(typeid(T)), index);
    }

    void tFlushAllRms(std::type_index typeIndx) noexcept {
        auto it = pools.find(typeIndx);
        if (it != pools.end()) {
            it->second->flushAllRms();
        }
    }
    template<typename T>
    void tFlushAllRms() noexcept {
        tFlushAllRms(std::type_index(typeid(T)));
    }


    bool tHasPendingRms(std::type_index typeIndx) const noexcept {
        auto it = pools.find(typeIndx);
        if (it == pools.end()) return false;

        return it->second->hasPendingRms();
    }
    template<typename T>
    bool tHasPendingRms() const noexcept {
        return tHasPendingRms(std::type_index(typeid(T)));
    }

    std::vector<tinyHandle> tPendingRms(std::type_index typeIndx) const noexcept {
        auto it = pools.find(typeIndx);
        if (it != pools.end()) {
            return it->second->pendingRms();
        }
        return std::vector<tinyHandle>();
    }
    template<typename T>
    std::vector<tinyHandle> tPendingRms() const noexcept {
        return tPendingRms(std::type_index(typeid(T)));
    }

    // Every pool's pending removals check

    void flushAllRms() {
        for (auto& [typeIndx, poolPtr] : pools) {
            poolPtr->flushAllRms();
        }
    }

    bool hasPendingRms() const noexcept { 
        for (const auto& [typeIndx, poolPtr] : pools) {
            if (poolPtr->hasPendingRms()) return true;
        }
        return false;
    }

    // Literal nukes
    void clear(std::type_index typeIndx) noexcept {
        auto it = pools.find(typeIndx);
        if (it != pools.end()) it->second->clear();
    }
    template<typename T>
    void clear() noexcept {
        clear(std::type_index(typeid(T)));
    }

// ------------------- Data Access ------------------

    template<typename T>
    T* get(const tinyHandle& handle) noexcept {
        auto* wrapper = getWrapper<T>();
        return wrapper ? wrapper->pool.get(handle) : nullptr;
    }
    template<typename T>
    const T* get(const tinyHandle& handle) const noexcept {
        return const_cast<tinyRegistry*>(this)->get<T>(handle);
    }

    void* get(const typeHandle& th) noexcept {
        if (!th.valid()) return nullptr;

        auto it = pools.find(th.typeIndex);
        return (it != pools.end()) ? it->second->get(th.handle) : nullptr;
    }
    const void* get(const typeHandle& th) const noexcept {
        return const_cast<tinyRegistry*>(this)->get(th);
    }

    template<typename T>
    T* get(const typeHandle& th) noexcept {
        return th.isType<T>() ? static_cast<T*>(get(th)) : nullptr;
    }
    template<typename T>
    const T* get(const typeHandle& th) const noexcept {
        return const_cast<tinyRegistry*>(this)->get<T>(th);
    }

    bool has(const typeHandle& th) const noexcept {
        if (!th.valid()) return false;

        auto it = pools.find(th.typeIndex);
        if (it == pools.end()) return false;

        return it->second->get(th.handle) != nullptr;
    }
    template<typename T>
    bool has(const tinyHandle& handle) const noexcept {
        return has(typeHandle::make<T>(handle));
    }

    template<typename T>
    tinyPool<T>& view() {
        return ensurePool<T>().pool;
    }
    template<typename T>
    const tinyPool<T>& view() const {
        const auto* wrapper = getWrapper<T>();

        // Cannot retrieve non-existing pool in const context
        if (!wrapper) {
            static tinyPool<T> empty; // or throw/assert
            return empty;
        }

        return wrapper->pool;
    }

    template<typename T>
    uint32_t capacity() const noexcept {
        auto* wrapper = getWrapper<T>();
        return wrapper ? wrapper->pool.capacity : 0;
    }

    template<typename T>
    uint32_t count() const noexcept {
        auto* wrapper = getWrapper<T>();
        return wrapper ? wrapper->pool.count() : 0;
    }

    template<typename T>
    static std::type_index typeIndex() {
        return std::type_index(typeid(T));
    }

    template<typename T>
    static size_t typeHash() { // Legacy compatibility
        return std::type_index(typeid(T)).hash_code();
    }
};
