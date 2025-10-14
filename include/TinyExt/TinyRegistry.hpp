#pragma once

#include "TinyExt/TinyPool.hpp"
#include "TinyExt/TinyHandle.hpp"

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
};

class TinyRegistry { // For raw resource data
    struct IPool {
        virtual ~IPool() = default;
    };

    template<typename T>
    struct PoolWrapper : public IPool {
        TinyPool<T> pool;
    };

    UnorderedMap<std::type_index, UniquePtr<IPool>> pools;

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
            pools[idx] = std::move(wrapper);
            return *ptr;
        }
        return *static_cast<PoolWrapper<T>*>(pools[idx].get());
    }

public:
    TinyRegistry() = default;

    TinyRegistry(const TinyRegistry&) = delete;
    TinyRegistry& operator=(const TinyRegistry&) = delete;

    template<typename T>
    TinyHandle add(T& data) {
        auto& pool = ensurePool<T>().pool;
        TinyHandle handle = pool.insert(std::move(data));

        return handle;
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
        return wrapper ? wrapper->pool.count : 0;
    }
};
