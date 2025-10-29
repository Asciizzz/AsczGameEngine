#pragma once

#include "tinyExt/tinyPool.hpp"

#include <assert.h>

class tinyRegistry { // For raw resource data
    struct IPool {
        virtual ~IPool() noexcept = default;
        virtual void* get(const tinyHandle& handle) = 0;
        virtual void instaRm(const tinyHandle& handle) = 0;
        virtual void queueRm(const tinyHandle& handle) = 0;
        virtual void flushRm(uint32_t index) = 0;
        virtual void flushAllRms() = 0;
        virtual bool hasPendingRms() const = 0;
    };

    template<typename T>
    struct PoolWrapper : public IPool {
        tinyPool<T> pool;

        void* get(const tinyHandle& handle) override { return pool.get(handle); }
        void instaRm(const tinyHandle& handle) override { pool.instaRm(handle); }
        void queueRm(const tinyHandle& handle) override { pool.queueRm(handle); }
        void flushRm(uint32_t index) override { pool.flushRm(index); }
        void flushAllRms() override { pool.flushAllRms(); }
        bool hasPendingRms() const override { return pool.hasPendingRms(); }
    };

    UnorderedMap<std::type_index, UniquePtr<IPool>> pools;
    UnorderedMap<size_t, IPool*> hashToPool;

    template<typename T>
    PoolWrapper<T>* getWrapper() {
        auto it = pools.find(std::type_index(typeid(T)));
        if (it == pools.end()) return nullptr;
        return static_cast<PoolWrapper<T>*>(it->second.get());
    }

    template<typename T>
    const PoolWrapper<T>* getWrapper() const {
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
    void remove(const tinyHandle& handle) {
        if (auto* wrapper = getWrapper<T>()) wrapper->pool.instaRm(handle);
    }

    void remove(const typeHandle& th) {
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
    void tInstaRm(const tinyHandle& handle) {
        remove<T>(handle);
    }
    void tInstaRm(const typeHandle& th) {
        remove(th);
    }

    template<typename T> // For unsafe removal (vulkan resources for example)
    void tQueueRm(const tinyHandle& handle) {
        auto* wrapper = getWrapper<T>(); // check validity
        if (wrapper) wrapper->pool.queueRm(handle);
    }
    void tQueueRm(const typeHandle& th) {
        auto it = hashToPool.find(th.typeHash);
        if (it != hashToPool.end()) {
            it->second->queueRm(th.handle);
        }
    }

    template<typename T>
    void tFlushRm(uint32_t index) {
        auto* wrapper = getWrapper<T>(); // check validity
        if (wrapper) wrapper->pool.flushRm(index);
    }

    template<typename T>
    void tFlushAllRms() {
        auto* wrapper = getWrapper<T>(); // check validity
        if (wrapper) wrapper->pool.flushAllRms();
    }

    template<typename T>
    bool tHasPendingRms() const {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.hasPendingRms() : false;
    }

    void flushAllRms() { // Every pool
        for (auto& [typeIndx, poolPtr] : pools) {
            poolPtr->flushAllRms();
        }
    }

    bool hasPendingRms() const { // At least one pool has pending removals
        for (const auto& [typeIndx, poolPtr] : pools) {
            if (poolPtr->hasPendingRms()) return true;
        }
        return false;
    }


    template<typename T>
    T* get(const tinyHandle& handle) {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.get(handle) : nullptr;
    }

    template<typename T>
    const T* get(const tinyHandle& handle) const {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.get(handle) : nullptr;
    }

    void* get(const typeHandle& th) {
        if (!th.valid()) return nullptr;

        auto it = hashToPool.find(th.typeHash);
        return (it != hashToPool.end()) ? it->second->get(th.handle) : nullptr;
    }

    const void* get(const typeHandle& th) const {
        return const_cast<tinyRegistry*>(this)->get(th);
    }

    template<typename T>
    T* get(const typeHandle& th) {
        assert(th.isType<T>() && "typeHandle does not match requested type T");
        return static_cast<T*>(get(th));
    }

    template<typename T>
    const T* get(const typeHandle& th) const {
        return const_cast<tinyRegistry*>(this)->get<T>(th);
    }

    template<typename T>
    bool has(const tinyHandle& handle) const {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.valid(handle) : false;
    }

    bool has(const typeHandle& th) const {
        if (!th.valid()) return false;

        auto it = hashToPool.find(th.typeHash);
        if (it == hashToPool.end()) return false;

        return it->second->get(th.handle) != nullptr;
    }

    template<typename T>
    T* data() {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.data() : nullptr;
    }

    template<typename T>
    const T* data() const {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.data() : nullptr;
    }

    void* data(size_t typeHash) {
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
    uint32_t capacity() const {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.capacity : 0;
    }

    template<typename T>
    uint32_t count() const {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.count() : 0;
    }

    template<typename T>
    static size_t typeHash() {
        return std::type_index(typeid(T)).hash_code();
    }
};
