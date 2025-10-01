#pragma once

#include "TinyExt/TinyPool.hpp"
#include "TinyExt/TinyHandle.hpp"

#include "TinyEngine/TinyRData.hpp"

struct IPool {
    virtual ~IPool() = default;
};

template<typename T>
struct PoolWrapper : public IPool {
    TinyPool<T> pool;
};

class TinyRegistry { // For raw resource data
    std::tuple<
        TinyPool<TinyRMesh>,
        TinyPool<TinyRMaterial>,
        TinyPool<TinyRTexture>,
        TinyPool<TinyRSkeleton>,
        TinyPool<TinyRNode>
    > pools;

    template<typename T>
    TinyPool<T>& pool() {
        return std::get<TinyPool<T>>(pools);
    }

    template<typename T>
    const TinyPool<T>& pool() const {
        return std::get<TinyPool<T>>(pools);
    }

public:
    TinyRegistry() = default;

    TinyRegistry(const TinyRegistry&) = delete;
    TinyRegistry& operator=(const TinyRegistry&) = delete;

    template<typename T>
    TinyHandle add(T& data) {
        uint32_t index = pool<T>().insert(std::move(data));

        return TinyHandle(index);
    }

    template<typename T>
    T* get(const TinyHandle& handle) {
        return pool<T>().get(handle.index);
    }

    template<typename T>
    uint32_t poolCapacity() const {
        return pool<T>().capacity;
    }
};


