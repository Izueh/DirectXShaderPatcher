#pragma once

#include <algorithm>
#include <any>
#include <expected>
#include <glaze/glaze.hpp>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <dxp/Condition.hpp>
#include "dxp/ResultFieldTraits.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp {

/// @brief Resolves variable names from context state/variables/results maps.
template <typename Context>
struct ConditionResolver {
  explicit ConditionResolver(const Context& ctx) : ctx_(ctx) {}
  /// @brief Resolves a variable name from context state/variables/results maps.
  /// std::nullopt means the name is not present — treated as false at evaluation.
  [[nodiscard]] std::optional<ConditionValue> Resolve(const std::string& name) const {
    auto dot = name.find('.');
    if (dot != std::string::npos) {
      auto step_name = name.substr(0, dot);
      auto field_name = name.substr(dot + 1);
      auto result_it = ctx_.results.find(step_name);
      if (result_it != ctx_.results.end()) {
        if (auto* results_variant = std::any_cast<ResultsVariant>(&result_it->second)) {
          return std::visit(
              [&](const auto& result_value) -> std::optional<ConditionValue> {
                auto value =
                    ExtractFieldTrait<std::decay_t<decltype(result_value)>>::GetValue(result_value, field_name);
                if (!value) return std::nullopt;
                return std::visit(
                    [](const auto& primitive_value) -> ConditionValue { return primitive_value; }, *value);
              },
              *results_variant);
        }
      }
      return std::nullopt;
    }

    auto state_it = ctx_.state.find(name);
    if (state_it != ctx_.state.end()) {
      // state holds PrimitiveValue — extract active alternative for flat ConditionValue
      return std::visit(
          [](const auto& primitive_value) -> ConditionValue { return primitive_value; }, state_it->second);
    }

    auto var_it = ctx_.variables.find(name);
    if (var_it != ctx_.variables.end()) {
      // variables now stores PrimitiveValue — extract the active alternative
      return std::visit(
          [](const auto& variant_value) -> ConditionValue { return variant_value; }, var_it->second);
    }
    return std::nullopt;
  }

 private:
  const Context& ctx_;
};

/// @brief Type-safe comparison over ConditionValue; mismatched types return false.
bool EvaluateComparison(Operation operation, const ConditionValue& lhs, const ConditionValue& rhs);

/// @brief Evaluates a ConditionNode against a resolver.
template <ResolverConcept Resolver>
struct EngineEvaluator {
  const Resolver& resolver;

  /// Resolves variable and checks truthiness.
  bool operator()(const TruthyOp& n) const {
    auto val = resolver.Resolve(n.var_name);
    if (!val) return false;  // missing state/variable/field is false
    return std::visit([](const auto& value) -> bool {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, bool>) {
        return value;
      } else if constexpr (std::is_integral_v<T>) {
        return value != 0;
      } else if constexpr (std::is_floating_point_v<T>) {
        return value != 0.0;
      } else if constexpr (std::is_same_v<T, std::string>) {
        return !value.empty();
      }
      std::unreachable();
    },
                      *val);
  }

  /// Compares resolved lhs/rhs values.
  bool operator()(const CompareOp& n) const {
    auto resolve_value = [this](const ConditionValue& condition_value) -> std::optional<ConditionValue> {
      return std::visit(
          [this](const auto& arg) -> std::optional<ConditionValue> {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
              // string operands are variable references — resolve them
              return resolver.Resolve(arg);
            } else {
              // literal value (bool, int, double) — return as-is
              return arg;
            }
          },
          condition_value);
    };

    auto left = resolve_value(n.lhs);
    auto right = resolve_value(n.rhs);
    if (!left || !right) return false;  // missing operand — comparison is false
    return EvaluateComparison(n.operation, *left, *right);
  }

  /// All children must be true (short-circuit).
  bool operator()(const AndOp& n) const {
    return std::ranges::all_of(n.conditions, [this](const auto& child) {
      return std::visit(*this, child.node);
    });
  }

  /// At least one child must be true (short-circuit).
  bool operator()(const OrOp& n) const {
    return std::ranges::any_of(n.conditions, [this](const auto& child) {
      return std::visit(*this, child.node);
    });
  }

  /// Inverts the child condition.
  bool operator()(const NegateOp& n) const {
    if (n.condition.empty()) return true;
    return !std::visit(*this, n.condition[0].node);
  }
};

/// @brief Unified framework entry point — evaluates a ConditionNode against a resolver.
template <ResolverConcept Resolver>
inline bool Evaluate(const ConditionNode& node, const Resolver& resolver) {
  return std::visit(EngineEvaluator<Resolver>{resolver}, node.node);
}

/// @brief Checks whether a recipe step should execute based on its condition.
/// Shared by all step types on both backends; step structs only need a
/// `condition` member (enforced by the RecipeStep concept).
/// @param step Any recipe step type.
/// @param resolver Resolver for condition variables.
/// @return true if the step should execute.
template <typename Step, ResolverConcept Resolver>
inline bool ShouldExecute(const Step& step, const Resolver& resolver) {
  if (!step.condition.has_value()) return true;
  return Evaluate(*step.condition, resolver);
}

namespace detail {

inline bool ValidateConditionNode(const ConditionStorage& storage,
                                  const ValidationContext& ctx) {
  return std::visit([&](const auto& n) -> bool {
    using T = std::decay_t<decltype(n)>;

    if constexpr (std::is_same_v<T, TruthyOp>) {
      if (n.var_name.empty()) return false;
      auto dot = n.var_name.find('.');
      if (dot != std::string::npos) {
        auto step_name = n.var_name.substr(0, dot);
        auto field_name = n.var_name.substr(dot + 1);
        return ctx.names.find(step_name) != ctx.names.end();
      }
      return true;
    } else if constexpr (std::is_same_v<T, CompareOp>) {
      auto check = [&](const ConditionValue& condition_value) -> bool {
        return std::visit(
            [&](const auto& arg) -> bool {
              using A = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<A, std::string>) {
                auto dot = arg.find('.');
                if (dot != std::string::npos) {
                  auto step_name = arg.substr(0, dot);
                  auto field_name = arg.substr(dot + 1);
                  return ctx.names.find(step_name) != ctx.names.end();
                }
                return true;
              } else {
                return true;
              }
            },
            condition_value);
      };
      return check(n.lhs) && check(n.rhs);
    } else if constexpr (std::is_same_v<T, AndOp>) {
      if (n.conditions.empty()) return false;
      return std::ranges::all_of(n.conditions, [&ctx](const auto& child) {
        return ValidateConditionNode(child.node, ctx);
      });
    } else if constexpr (std::is_same_v<T, OrOp>) {
      if (n.conditions.empty()) return false;
      return std::ranges::any_of(n.conditions, [&ctx](const auto& child) {
        return ValidateConditionNode(child.node, ctx);
      });
    } else if constexpr (std::is_same_v<T, NegateOp>) {
      if (n.condition.empty()) return false;
      return ValidateConditionNode(n.condition[0].node, ctx);
    }
    std::unreachable();
  },
                    storage);
}

}  // namespace detail

/// @brief Validates structural correctness and dot-notation references.
/// Returns success if condition is nullopt (no condition = valid).
template <typename Results>
std::expected<void, std::string> ValidateCondition(const std::optional<ConditionNode>& node,
                                                   const ValidationContext& ctx) {
  if (!node.has_value()) return {};
  auto validate = [&](const ConditionStorage& storage) -> bool {
    return detail::ValidateConditionNode(storage, ctx);
  };

  if (!validate(node->node)) {
    return std::unexpected("invalid condition structure");
  }
  return {};
}

/// @brief YAML deserialization struct for conditions.
/// Mirrors the runtime ConditionNode but with string-based fields for YAML parsing.
struct ConditionData {
  /// @brief Truthy check — resolves this variable and checks truthiness.
  std::string is;

  /// @brief Comparison — lhs/rhs are typed literals or variable references.
  struct Comparison {
    ConditionValue lhs;  // Typed literal or variable reference
    ConditionValue rhs;  // Typed literal or variable reference
  } eq, ne, gt, gte, lt, lte;

  /// @brief AND composition.
  std::vector<ConditionData> and_conditions;

  /// @brief OR composition.
  std::vector<ConditionData> or_conditions;

  /// @brief Negation flag.
  bool not_condition = false;

  /// @brief Compile YAML data into runtime AST.
  [[nodiscard]] std::optional<ConditionNode> Compile() const;
};

}  // namespace dxp

namespace glz {

template <>
struct meta<dxp::ConditionData> {
  using T = dxp::ConditionData;
  static constexpr auto value = object("is", &T::is, "eq", &T::eq, "ne", &T::ne, "gt", &T::gt,
                                       "gte", &T::gte, "lt", &T::lt, "lte", &T::lte,
                                       "and", &T::and_conditions, "or", &T::or_conditions,
                                       "not", &T::not_condition);
};

template <>
struct meta<dxp::ConditionData::Comparison> {
  using T = dxp::ConditionData::Comparison;
  static constexpr auto value = object("lhs", &T::lhs, "rhs", &T::rhs);
};

}  // namespace glz
