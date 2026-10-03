#pragma once

#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/step/common/Rule.hpp"

namespace dxp::sm5::step::common {
using model::CapturedOperand;
using model::Instruction;
using model::Operand;

/// Read-only inputs used by matching. Captures are returned locally.
struct MatchContext {
  const CaptureStore& captures;
  const DeclarationIndex& declarations;
};

struct MatchResult {
  uint32_t instruction_index = 0;
  const Instruction* instruction = nullptr;
  uint32_t range_start_index = 0;
  uint32_t range_end_index = 0;
  std::unordered_map<std::string, CapturedOperand> operands;
  std::unordered_map<std::string, Instruction> instructions;
  std::unordered_map<std::string, Operand::Index> index_values;
};

auto CollectMatches(const std::vector<Instruction>& instructions, const InstructionPattern& pattern, const MatchContext& context) -> std::vector<MatchResult>;
auto CollectSequenceMatches(const std::vector<Instruction>& instructions, const std::vector<InstructionPattern>& patterns, const MatchContext& context) -> std::vector<MatchResult>;
auto CollectWindowMatches(const std::vector<Instruction>& instructions, const InstructionPattern& start_pattern,
                          const InstructionPattern& end_pattern, const MatchContext& context) -> std::vector<MatchResult>;

uint32_t ExtractComponentMask(uint32_t component_mode, uint32_t selection_mode);

}  // namespace dxp::sm5::step::common
