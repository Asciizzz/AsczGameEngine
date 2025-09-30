#pragma once

#include "Helpers/Templates.hpp"

/* TinyPool:
A fixed-capacity free-list pool for managing resources by stable indices.
  - Preallocates a contiguous vector of unique_ptr<T>, avoiding reallocation.
  - Insert reuses freed slots (O(1)), ensuring indices remain stable.
  - Remove frees the object and recycles the slot into the free list.
  - Prevents memory fragmentation and shifting of resources, making it safe
    to reference resources by index (e.g., for scenes or GPU descriptor indexing).
*/

#define TINYPOOL_CAPACITY_STEP 128

template<typename Type>
struct TinyPoolRaw {
    TinyPoolRaw() = default;
    TinyPoolRaw(uint32_t initialCapacity) { allocate(initialCapacity); }

    std::vector<Type> items;
    std::vector<uint32_t> freeList;
    std::vector<bool> occupied; // For arbitrary types
    uint32_t capacity = 0;
    uint32_t count = 0;

    void clear() {
        items.clear();
        freeList.clear();
        occupied.clear();
        capacity = 0;
        count = 0;
    }

    void allocate(uint32_t capacity) {
        clear();

        this->capacity = capacity;

        items.resize(capacity);
        occupied.resize(capacity, false);

        freeList.reserve(capacity);
        for (uint32_t i = 0; i < capacity; ++i) {
            freeList.push_back(capacity - 1 - i);
        }
    }

    void resize(uint32_t newCapacity) {
        if (newCapacity <= capacity) return; // no shrinking allowed

        items.resize(newCapacity);
        occupied.resize(newCapacity, false);

        // Add new slots to free list
        for (uint32_t i = newCapacity; i-- > capacity;) {
            freeList.push_back(i);
        }

        capacity = newCapacity;
    }

    void* data() { return items.data(); }

    template<typename U>
    uint32_t insert(U&& item) {
        // Resize until there's space
        while (!hasSpace()) resize(capacity + TINYPOOL_CAPACITY_STEP);

        count++;

        uint32_t index = freeList.back();
        freeList.pop_back();
        items[index] = std::forward<U>(item); // move or copy
        occupied[index] = true;
        return index;
    }

    void remove(uint32_t index) {
        if (!isValid(index)) return;
        count--;

        items[index] = {};
        occupied[index] = false;
        freeList.push_back(index);
    }

    bool hasSpace() const {
        return !freeList.empty();
    }

    bool isValid(uint32_t index) const {
        return index < items.size() && occupied[index];
    }

    void checkValid(uint32_t index) const {
        if (!isValid(index)) {
            throw std::runtime_error("TinyPoolRaw: Invalid index access");
        }
    }

    Type& get(uint32_t index) {
        checkValid(index);
        return items[index];
    }

    const Type& get(uint32_t index) const {
        checkValid(index);
        return items[index];
    }

    Type& operator[](uint32_t index) { return get(index); }
    const Type& operator[](uint32_t index) const { return get(index); }
};

template<typename Type>
class TinyPoolPtr {
public:
    TinyPoolPtr() = default;
    TinyPoolPtr(uint32_t initialCapacity) { allocate(initialCapacity); }

    UniquePtrVec<Type> items;
    std::vector<uint32_t> freeList;
    uint32_t capacity = 0;
    uint32_t count = 0;

    void clear() {
        items.clear();
        freeList.clear();
        capacity = 0;
        count = 0;
    }

    void allocate(uint32_t capacity) {
        clear();
        this->capacity = capacity;

        items.resize(capacity);

        freeList.reserve(capacity);
        for (uint32_t i = 0; i < capacity; ++i) {
            freeList.push_back(capacity - 1 - i);
        }
    }

    void resize(uint32_t newCapacity) {
        if (newCapacity <= capacity) return; // no shrinking allowed

        items.resize(newCapacity);

        // Add new slots to free list
        for (uint32_t i = newCapacity; i-- > capacity;) {
            freeList.push_back(i);
        }

        capacity = newCapacity;
    }

    uint32_t insert(UniquePtr<Type> item) {
        while (!hasSpace()) resize(capacity + TINYPOOL_CAPACITY_STEP);
        count++;

        uint32_t index = freeList.back();
        freeList.pop_back();
        items[index] = std::move(item);
        return index;
    }

    template<typename... Args>
    uint32_t emplace(Args&&... args) {
        return insert(MakeUnique<Type>(std::forward<Args>(args)...));
    }

    void remove(uint32_t index) {
        if (!isValid(index)) return;
        count--;

        items[index].reset();
        freeList.push_back(index);
    }

    bool hasSpace() const {
        return !freeList.empty();
    }

    bool isValid(uint32_t index) const {
        return index < items.size() && static_cast<bool>(items[index]);
    }

    void checkValid(uint32_t index) const {
        if (!isValid(index)) {
            throw std::runtime_error("TinyPoolPtr: Invalid index access");
        }
    }

    Type& get(uint32_t index) {
        checkValid(index);
        return *items[index];
    }

    const Type& get(uint32_t index) const {
        checkValid(index);
        return *items[index];
    }

    Type* getPtr(uint32_t index) {
        checkValid(index);
        return items[index].get();
    }

    const Type* getPtr(uint32_t index) const {
        checkValid(index);
        return items[index].get();
    }


    Type& operator[](uint32_t index) { return get(index); }
    const Type& operator[](uint32_t index) const { return get(index); }
};