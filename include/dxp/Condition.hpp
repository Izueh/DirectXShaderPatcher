#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "dxp/ExportTypes.hpp"

namespace dxp {

/// @brief Condition value — flat variant of literal or variable reference (string). std::string first avoids Debug-mode _invalid_parameter crashes in glaze YAML parsing.
using ConditionValue = std::variant<std::string, bool, int32_t, uint32_t, int64_t, uint64_t, double>;

/// @brief Concept: any type with Resolve(name) -> std::optional<ConditionValue>.
/// std::nullopt means the name is not present in any context (state/variables/results)
/// and is treated as false at evaluation time.
template <typename T>
concept ResolverConcept = requires(const T& resolver, const std::string& key) {
  { resolver.Resolve(key) } -> std::same_as<std::optional<ConditionValue>>;
};

/// @brief Comparison operators for condition evaluation.
/// Recipe concept — not a token-format enum.
enum class Operation : std::uint8_t { Eq,
                                      Ne,
                                      Gt,
                                      Gte,
                                      Lt,
                                      Lte };

/// @brief Resolves a variable and checks truthiness.
struct TruthyOp {
  std::string var_name;
};

/// @brief Forward declaration for recursive variant.
struct ConditionNode;

/// @brief Inverts a child condition.
struct NegateOp {
  std::vector<ConditionNode> condition;
};

/// @brief All children must be true.
struct AndOp {
  std::vector<ConditionNode> conditions;
};

/// @brief At least one child must be true.
struct OrOp {
  std::vector<ConditionNode> conditions;
};

/// @brief Symmetric lhs/rhs comparison.
struct CompareOp {
  ConditionValue lhs;
  Operation operation;
  ConditionValue rhs;
};

/// @brief Runtime AST storage — variant of condition operation types.
using ConditionStorage = std::variant<TruthyOp, CompareOp, AndOp, OrOp, NegateOp>;

/// @brief Runtime AST node — wraps ConditionStorage.
struct ConditionNode {
  ConditionStorage node;
};

}  // namespace dxp
