#pragma once

#include <memory>
#include <vector>
#include <string>

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

#include <map>
#include <unordered_map>
#include <unordered_set>
template<typename K, typename V> using OrderedMap = std::map<K, V>;
template<typename K, typename V, typename Hash = std::hash<K>, typename KeyEqual = std::equal_to<K>> 
using UnorderedMap = std::unordered_map<K, V, Hash, KeyEqual>;
template<typename K, typename Hash = std::hash<K>, typename KeyEqual = std::equal_to<K>>
using UnorderedSet = std::unordered_set<K, Hash, KeyEqual>;


#include <variant>
template<typename... Ts>
using MonoVariant = std::variant<std::monostate, Ts...>;


template<typename A, typename B>
inline constexpr bool type_eq = std::is_same_v<A, B>;