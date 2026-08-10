#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dxp/Condition.hpp"
#include "dxp/sm5/Model.hpp"
#include "dxp/StepResults.hpp"

namespace dxp::sm5::step {

/// @brief Checks opcode counts in the shader program and publishes results.
struct CheckOpcodeCountStep {
  static constexpr std::string_view kind = "check_opcode_count";
  using Results = dxp::CheckOpcodeCountResults;
  using Opcode = dxp::sm5::model::Opcode;
  std::string name;
  std::vector<Opcode> opcodes;
  bool required = true;
  std::optional<ConditionNode> condition;

  CheckOpcodeCountStep(std::string name_val, std::vector<Opcode> ops, bool required, std::optional<ConditionNode> condition_val)
      : name(std::move(name_val)), opcodes(std::move(ops)), required(required), condition(std::move(condition_val)) {}
};

}  // namespace dxp::sm5::step
