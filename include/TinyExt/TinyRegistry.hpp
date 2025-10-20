#pragma once

#include "TinyExt/TinyPool.hpp"

#include <typeindex>

struct TypeHandle {
    TinyHandle handle;
    size_t typeHash;

    TypeHandle() : typeHash(0) {}

    template<typename T>
    static TypeHandle make(TinyHandle h) {
        TypeHandle th;
        th.handle = h;
        th.typeHash = typeid(T).hash_code();
        return th;
    }

    bool valid() const { return handle.valid() && typeHash != 0; }

    template<typename T>
    bool isType() const { return valid() && typeHash == typeid(T).hash_code(); }
};

class TinyRegistry { // For raw resource data
    struct IPool {
        virtual ~IPool() = default;
        virtual void* get(const TinyHandle& handle) = 0;
        virtual void remove(const TinyHandle& handle) = 0;
    };

    template<typename T>
    struct PoolWrapper : public IPool {
        TinyPool<T> pool;

        void* get(const TinyHandle& handle) override {
            return pool.get(handle);
        }

        void remove(const TinyHandle& handle) override {
            pool.remove(handle);
        }
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
        auto idx = std::type_index(typeid(T));
        auto it = pools.find(idx);
        if (it == pools.end()) {
            auto wrapper = std::make_unique<PoolWrapper<T>>();
            auto* ptr = wrapper.get();
            hashToPool[idx.hash_code()] = ptr; // hash for O(1) lookup
            pools[idx] = std::move(wrapper);
            return *ptr;
        }

        auto* existing = static_cast<PoolWrapper<T>*>(pools[idx].get());
        hashToPool[idx.hash_code()] = existing; // map stays valid
        return *existing;
    }

public:
    TinyRegistry() = default;

    TinyRegistry(const TinyRegistry&) = delete;
    TinyRegistry& operator=(const TinyRegistry&) = delete;

    TinyRegistry(TinyRegistry&&) = default;
    TinyRegistry& operator=(TinyRegistry&&) = default;

    template<typename T>
    TypeHandle add(T& data) {
        auto& pool = ensurePool<T>().pool;
        TinyHandle handle = pool.add(std::move(data));

        return TypeHandle::make<T>(handle);
    }

    template<typename T>
    T* get(const TinyHandle& handle) {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.get(handle) : nullptr;
    }

    template<typename T>
    const T* get(const TinyHandle& handle) const {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.get(handle) : nullptr;
    }

    void* get(const TypeHandle& th) {
        if (!th.valid()) return nullptr;

        auto it = hashToPool.find(th.typeHash);
        return (it != hashToPool.end()) ? it->second->get(th.handle) : nullptr;
    }

    template<typename T>
    T* get(const TypeHandle& th) {
        assert(th.isType<T>() && "TypeHandle does not match requested type T");
        return static_cast<T*>(get(th));
    }

    template<typename T>
    void remove(const TinyHandle& handle) {
        auto* wrapper = getWrapper<T>(); // check validity
        if (wrapper) wrapper->pool.remove(handle);
    }

    void remove(const TypeHandle& th) {
        if (!th.valid()) return;

        auto it = hashToPool.find(th.typeHash);
        if (it != hashToPool.end()) {
            it->second->remove(th.handle);
        }
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
        return (it != hashToPool.end()) ? it->second->get(TinyHandle()) : nullptr;
    }

    template<typename T>
    TinyPool<T>& view() {
        return ensurePool<T>().pool;
    }

    template<typename T>
    const TinyPool<T>& view() const {
        const auto* wrapper = getWrapper<T>();

        // Cannot retrieve non-existing pool in const context
        if (!wrapper) {
            static TinyPool<T> empty; // or throw/assert
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
