#pragma once

#include <vector>

namespace dxp::utils {

/// @brief Construct a vector from initializers without requiring copy-constructibility.
template <typename T, typename... Args>
[[nodiscard]] std::vector<T> MakeVector(Args&&... args) {
  std::vector<T> vec;
  vec.reserve(sizeof...(Args));
  (vec.emplace_back(std::forward<Args>(args)), ...);
  return vec;
}

}  // namespace dxp::utils
