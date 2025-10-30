#pragma once

#include "tinyExt/tinyPool.hpp"

#include <assert.h>
#include <typeindex>

struct typeHandle {
    tinyHandle handle;
    size_t typeHash;

    typeHandle() noexcept : typeHash(0) {}

    template<typename T>
    static typeHandle make(tinyHandle h) noexcept {
        typeHandle th;
        th.handle = h;
        th.typeHash = typeid(T).hash_code();
        return th;
    }

    bool valid() const noexcept { return handle.valid() && typeHash != 0; }

    template<typename T>
    bool isType() const noexcept { return valid() && typeHash == typeid(T).hash_code(); }
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
    };

    UnorderedMap<std::type_index, UniquePtr<IPool>> pools;
    UnorderedMap<size_t, IPool*> hashToPool;

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
        auto indx = std::type_index(typeid(T));
        auto it = pools.find(indx);
        if (it == pools.end()) {
            auto wrapper = std::make_unique<PoolWrapper<T>>();
            auto* ptr = wrapper.get();
            hashToPool[indx.hash_code()] = ptr; // hash for O(1) lookup
            pools[indx] = std::move(wrapper);
            return *ptr;
        }

        auto* existing = static_cast<PoolWrapper<T>*>(pools[indx].get());
        hashToPool[indx.hash_code()] = existing; // map stays valid
        return *existing;
    }

    // Remove execution
    template<typename T>
    void remove(const tinyHandle& handle) noexcept {
        if (auto* wrapper = getWrapper<T>()) wrapper->pool.instaRm(handle);
    }

    void remove(const typeHandle& th) noexcept {
        auto it = hashToPool.find(th.typeHash);
        if (it != hashToPool.end()) {
            it->second->instaRm(th.handle);
        }
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

    template<typename T> // For unsafe removal (vulkan resources for example)
    void tQueueRm(const tinyHandle& handle) noexcept {
        auto* wrapper = getWrapper<T>(); // check validity
        if (wrapper) wrapper->pool.queueRm(handle);
    }
    void tQueueRm(const typeHandle& th) noexcept {
        auto it = hashToPool.find(th.typeHash);
        if (it != hashToPool.end()) {
            it->second->queueRm(th.handle);
        }
    }

    template<typename T>
    void tFlushRm(uint32_t index) noexcept {
        auto* wrapper = getWrapper<T>(); // check validity
        if (wrapper) wrapper->pool.flushRm(index);
    }

    template<typename T>
    void tFlushAllRms() noexcept {
        auto* wrapper = getWrapper<T>(); // check validity
        if (wrapper) wrapper->pool.flushAllRms();
    }

    template<typename T>
    bool tHasPendingRms() const noexcept {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.hasPendingRms() : false;
    }

    void flushAllRms() { // Every pool
        for (auto& [typeIndx, poolPtr] : pools) {
            poolPtr->flushAllRms();
        }
    }

    // At least one pool has pending removals
    bool hasPendingRms() const noexcept { 
        for (const auto& [typeIndx, poolPtr] : pools) {
            if (poolPtr->hasPendingRms()) return true;
        }
        return false;
    }


    template<typename T>
    T* get(const tinyHandle& handle) noexcept {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.get(handle) : nullptr;
    }

    template<typename T>
    const T* get(const tinyHandle& handle) const noexcept {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.get(handle) : nullptr;
    }

    void* get(const typeHandle& th) noexcept {
        if (!th.valid()) return nullptr;

        auto it = hashToPool.find(th.typeHash);
        return (it != hashToPool.end()) ? it->second->get(th.handle) : nullptr;
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

    template<typename T>
    bool has(const tinyHandle& handle) const noexcept {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.valid(handle) : false;
    }

    bool has(const typeHandle& th) const noexcept {
        if (!th.valid()) return false;

        auto it = hashToPool.find(th.typeHash);
        if (it == hashToPool.end()) return false;

        return it->second->get(th.handle) != nullptr;
    }

    template<typename T>
    T* data() noexcept {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.data() : nullptr;
    }

    template<typename T>
    const T* data() const noexcept {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.data() : nullptr;
    }

    void* data(size_t typeHash) noexcept {
        auto it = hashToPool.find(typeHash);
        return (it != hashToPool.end()) ? it->second->get(tinyHandle()) : nullptr;
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
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.capacity : 0;
    }

    template<typename T>
    uint32_t count() const noexcept {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.count() : 0;
    }

    template<typename T>
    static size_t typeHash() {
        return std::type_index(typeid(T)).hash_code();
    }
};
