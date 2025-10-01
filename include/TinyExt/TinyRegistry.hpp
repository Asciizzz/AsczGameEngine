#pragma once

#include "TinyExt/TinyPool.hpp"
#include "TinyExt/TinyHandle.hpp"

#include <typeindex>

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
        uint32_t index = pool.insert(std::move(data));

        return TinyHandle(index); // Will add version in the future
    }

    template<typename T>
    T* get(const TinyHandle& handle) {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.get(handle.index) : nullptr;
    }

    template<typename T>
    const T* get(const TinyHandle& handle) const {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.get(handle.index) : nullptr;
    }

    template<typename T>
    uint32_t poolCapacity() const {
        auto* wrapper = getWrapper<T>(); // check validity
        return wrapper ? wrapper->pool.capacity : 0;
    }
};


