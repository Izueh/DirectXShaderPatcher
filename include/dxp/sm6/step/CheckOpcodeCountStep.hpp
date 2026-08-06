#pragma once
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dxp/Condition.hpp"
#include "dxp/StepResults.hpp"

namespace dxp::sm6::step {

/// @brief Counts DXIL and LLVM opcodes in the shader program entry function.
/// Publishes State[name] = true and Results[name] with per-opcode counts
/// for both DXIL and LLVM.
struct CheckOpcodeCountStep {
  static constexpr std::string_view kind = "check_opcode_count";
  using Results = dxp::CheckOpcodeCountResults;
  std::string name;
  std::vector<std::string> dxil_opcodes;
  std::vector<std::string> llvm_opcodes;
  bool required = true;
  std::optional<ConditionNode> condition;

  CheckOpcodeCountStep(std::string name_val,
                       std::vector<std::string> dxil_ops,
                       std::vector<std::string> llvm_ops,
                       bool required,
                       std::optional<ConditionNode> condition_val)
      : name(std::move(name_val)),
        dxil_opcodes(std::move(dxil_ops)),
        llvm_opcodes(std::move(llvm_ops)),
        required(required),
        condition(std::move(condition_val)) {}
};

}  // namespace dxp::sm6::step
