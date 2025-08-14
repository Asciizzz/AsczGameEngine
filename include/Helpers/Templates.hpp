#pragma once

#include <memory>
#include <vector>
#include <array>
#include <unordered_map>

template<typename T> using UniquePtr = std::unique_ptr<T>;
template<typename T> using SharedPtr = std::shared_ptr<T>;
template<typename T> using SharedPtrVec = std::vector<SharedPtr<T>>;

template<typename T, typename... Args>
SharedPtr<T> MakeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template<typename K, typename V> using UnorderedMap = std::unordered_map<K, V>;