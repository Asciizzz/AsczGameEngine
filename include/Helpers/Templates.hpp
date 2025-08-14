#pragma once

#include <memory>
#include <vector>
#include <array>

template<typename T> using SharedPtr = std::shared_ptr<T>;
template<typename T> using SharedPtrVec = std::vector<SharedPtr<T>>;

template<typename T> using UniquePtr = std::unique_ptr<T>;