#pragma once

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>

template<typename T> using UniquePtr = std::unique_ptr<T>;
template<typename T> using UniquePtrVec = std::vector<UniquePtr<T>>;
template<typename T, typename... Args>
UniquePtr<T> MakeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T> using SharedPtr = std::shared_ptr<T>;
template<typename T> using SharedPtrVec = std::vector<SharedPtr<T>>;
template<typename T, typename... Args>
SharedPtr<T> MakeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template<typename K, typename V> using OrderedMap = std::map<K, V>;
template<typename K, typename V> using UnorderedMap = std::unordered_map<K, V>;