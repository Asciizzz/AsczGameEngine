#pragma once

#include <memory>
#include <vector>

template<typename T> using SharedPtr = std::shared_ptr<T>;
template<typename T> using SharedPtrVec = std::vector<SharedPtr<T>>;

template<typename T> using UniquePtr = std::unique_ptr<T>;