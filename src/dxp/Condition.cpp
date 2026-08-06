#include <dxp/Condition.hpp>
#include "dxp/Condition_impl.hpp"

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace dxp {

namespace {

/// @brief A ConditionValue is "absent" only when it holds an empty string.
bool IsEmptyConditionValue(const ConditionValue& value) {
  if (const auto* str = std::get_if<std::string>(&value)) {
    return str->empty();
  }
  return false;
}

}  // namespace

std::optional<ConditionNode> ConditionData::Compile() const {
  std::optional<ConditionNode> node;

  if (!this->is.empty()) {
    node = ConditionNode{TruthyOp{.var_name = this->is}};
  } else if (!IsEmptyConditionValue(this->eq.lhs)) {
    node = ConditionNode{CompareOp{.lhs = this->eq.lhs, .operation = Operation::Eq, .rhs = this->eq.rhs}};
  } else if (!IsEmptyConditionValue(this->ne.lhs)) {
    node = ConditionNode{CompareOp{.lhs = this->ne.lhs, .operation = Operation::Ne, .rhs = this->ne.rhs}};
  } else if (!IsEmptyConditionValue(this->gt.lhs)) {
    node = ConditionNode{CompareOp{.lhs = this->gt.lhs, .operation = Operation::Gt, .rhs = this->gt.rhs}};
  } else if (!IsEmptyConditionValue(this->gte.lhs)) {
    node = ConditionNode{CompareOp{.lhs = this->gte.lhs, .operation = Operation::Gte, .rhs = this->gte.rhs}};
  } else if (!IsEmptyConditionValue(this->lt.lhs)) {
    node = ConditionNode{CompareOp{.lhs = this->lt.lhs, .operation = Operation::Lt, .rhs = this->lt.rhs}};
  } else if (!IsEmptyConditionValue(this->lte.lhs)) {
    node = ConditionNode{CompareOp{.lhs = this->lte.lhs, .operation = Operation::Lte, .rhs = this->lte.rhs}};
  } else if (!this->and_conditions.empty()) {
    std::vector<ConditionNode> children;
    for (const auto& child : this->and_conditions) {
      auto compiled = child.Compile();
      if (!compiled) return std::nullopt;
      children.push_back(std::move(*compiled));
    }
    node = ConditionNode{AndOp{.conditions = std::move(children)}};
  } else if (!this->or_conditions.empty()) {
    std::vector<ConditionNode> children;
    for (const auto& child : this->or_conditions) {
      auto compiled = child.Compile();
      if (!compiled) return std::nullopt;
      children.push_back(std::move(*compiled));
    }
    node = ConditionNode{OrOp{.conditions = std::move(children)}};
  }

  if (!node) return std::nullopt;
  if (this->not_condition) {
    // `not` is a modifier on the active condition form (schema default: false).
    return ConditionNode{NegateOp{.condition = std::vector<ConditionNode>{std::move(*node)}}};
  }
  return node;
}

bool EvaluateComparison(Operation operation, const ConditionValue& lhs, const ConditionValue& rhs) {
  return std::visit([operation, &rhs](const auto& left_val) -> bool {
    return std::visit([operation, &left_val](const auto& right_val) -> bool {
      using L = std::decay_t<decltype(left_val)>;
      using R = std::decay_t<decltype(right_val)>;

      if constexpr (std::is_same_v<L, R>) {
        switch (operation) {
          case Operation::Eq:  return left_val == right_val;
          case Operation::Ne:  return left_val != right_val;
          case Operation::Gt:  return left_val > right_val;
          case Operation::Gte: return left_val >= right_val;
          case Operation::Lt:  return left_val < right_val;
          case Operation::Lte: return left_val <= right_val;
        }
      }
      return false;
    },
                      rhs);
  },
                    lhs);
}

}  // namespace dxp
