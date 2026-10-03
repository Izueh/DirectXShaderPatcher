#include "dxp/sm5/step/common/Match.hpp"
#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/sm5/Model_impl.hpp"
#include "dxp/sm5/step/common/Rule_impl.hpp"

namespace dxp::sm5::step::common {
namespace {
constexpr uint32_t kExtendedOpcodeMask = 0x3fU;
constexpr uint32_t kAllComponentsMask = 0xfU;

auto MatchesOperand(const Operand& operand, std::unordered_map<std::string, Operand::Index>& captured_index_values, const OperandPattern& op, const MatchContext& context) -> bool;
auto MatchesOperandIndex(const Operand::Index& idx, const std::unordered_map<std::string, Operand::Index>& captured_index_values, const OperandIndexPattern& pattern, const MatchContext& context) -> bool;
bool MatchesDeclConstraint(const Operand& operand, const OperandPattern::DeclConstraint& constraint, const MatchContext& context);
auto MatchesInstruction(const Instruction& instr, std::unordered_map<std::string, Operand::Index>& captured_index_values, const InstructionPattern& pattern, const MatchContext& context) -> bool;

auto IndexValueForCapture(const Operand::Index& index) -> const uint32_t* {
  if (index.immediate_lo.has_value()) return &(*index.immediate_lo);
  if (index.immediate_hi.has_value()) return &(*index.immediate_hi);
  return nullptr;
}

auto CaptureOperands(const Instruction& instruction, const InstructionPattern& pattern,
                     std::unordered_map<std::string, CapturedOperand>& local_operands,
                     [[maybe_unused]] const CaptureStore& global_captures) {
  for (size_t i = 0; i < pattern.operands.size(); ++i) {
    const auto& op = pattern.operands[i];
    const auto& opnd = instruction.operands[i];
    if (op.capture.empty() && !op.export_as.has_value()) {
      continue;  // neither captured nor exported — nothing to store
    }
    CapturedOperand cap;
    cap.operand_data = opnd;
    cap.role = GetOperandRole(instruction.opcode, i);
    if (i == 0 && !instruction.operands.empty()) cap.destination_mask = ExtractComponentMask(instruction.operands[0].component_mode, DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(instruction.operands[0].component_mode));
    cap.export_as = op.export_as;
    // Export-only operands store under their export_as key.
    local_operands[op.capture.empty() ? *op.export_as : op.capture] = std::move(cap);
  }
}

auto CollectMatchesImpl(const std::vector<Instruction>& instructions, const InstructionPattern& pattern, const MatchContext& context) -> std::vector<MatchResult> {
  std::vector<MatchResult> matches;
  matches.reserve(instructions.size());
  for (uint32_t i = 0; i < instructions.size(); ++i) {
    std::unordered_map<std::string, Operand::Index> captured_index_values;
    if (!MatchesInstruction(instructions[i], captured_index_values, pattern, context)) continue;
    MatchResult r;
    r.instruction_index = i;
    r.instruction = &instructions[i];
    r.range_start_index = i;
    r.range_end_index = i;
    CaptureOperands(instructions[i], pattern, r.operands, context.captures);
    r.index_values = std::move(captured_index_values);
    if (!pattern.capture.empty()) {
      r.instructions[pattern.capture] = instructions[i];
      Operand::Index idx;
      idx.immediate_lo = i;
      r.index_values[pattern.capture + "_index"] = std::move(idx);
    }
    matches.push_back(std::move(r));
  }
  return matches;
}

/// @brief Blob window matching: `match_start` pattern at i, `match_end` pattern at
/// j >= i, everything between (inclusive) belongs to the window. Captures from both
/// endpoint patterns are collected into the match result.
auto CollectWindowMatchesImpl(const std::vector<Instruction>& instructions, const InstructionPattern& start_pattern,
                              const InstructionPattern& end_pattern, const MatchContext& context) -> std::vector<MatchResult> {
  std::vector<MatchResult> matches;
  if (instructions.empty()) return matches;

  for (uint32_t i = 0; i < instructions.size(); ++i) {
    std::unordered_map<std::string, Operand::Index> start_index_values;
    if (!MatchesInstruction(instructions[i], start_index_values, start_pattern, context)) continue;

    for (uint32_t j = i; j < instructions.size(); ++j) {
      std::unordered_map<std::string, Operand::Index> end_index_values;
      if (!MatchesInstruction(instructions[j], end_index_values, end_pattern, context)) continue;

      MatchResult r;
      r.instruction_index = i;
      r.instruction = &instructions[i];
      r.range_start_index = i;
      r.range_end_index = j;
      r.index_values = std::move(start_index_values);
      r.index_values.insert(end_index_values.begin(), end_index_values.end());
      CaptureOperands(instructions[i], start_pattern, r.operands, context.captures);
      CaptureOperands(instructions[j], end_pattern, r.operands, context.captures);
      if (!start_pattern.capture.empty()) {
        r.instructions[start_pattern.capture] = instructions[i];
        Operand::Index idx;
        idx.immediate_lo = i;
        r.index_values[start_pattern.capture + "_index"] = std::move(idx);
      }
      if (!end_pattern.capture.empty()) {
        r.instructions[end_pattern.capture] = instructions[j];
        Operand::Index idx;
        idx.immediate_lo = j;
        r.index_values[end_pattern.capture + "_index"] = std::move(idx);
      }
      matches.push_back(std::move(r));
      break;  // innermost end match for this start; next start index is a new window
    }
  }
  return matches;
}

auto CollectSequenceMatchesImpl(const std::vector<Instruction>& instructions, const std::vector<InstructionPattern>& patterns, const MatchContext& context) -> std::vector<MatchResult> {
  std::vector<MatchResult> matches;
  if (patterns.empty() || patterns.size() > instructions.size()) return matches;
  const auto limit = static_cast<uint32_t>(instructions.size() - patterns.size() + 1);
  for (uint32_t start = 0; start < limit; ++start) {
    std::unordered_map<std::string, Operand::Index> captured_index_values;
    bool ok = true;
    for (uint32_t pi = 0; pi < patterns.size(); ++pi) {
      if (!MatchesInstruction(instructions[start + pi], captured_index_values, patterns[pi], context)) {
        ok = false;
        break;
      }
    }
    if (!ok) continue;
    MatchResult r;
    r.instruction_index = start;
    r.instruction = &instructions[start];
    r.range_start_index = start;
    r.range_end_index = start + static_cast<uint32_t>(patterns.size() - 1);
    r.index_values = std::move(captured_index_values);
    for (uint32_t pi = 0; pi < patterns.size(); ++pi) {
      CaptureOperands(instructions[start + pi], patterns[pi], r.operands, context.captures);
      if (!patterns[pi].capture.empty()) {
        r.instructions[patterns[pi].capture] = instructions[start + pi];
        Operand::Index idx;
        idx.immediate_lo = start + pi;
        r.index_values[patterns[pi].capture + "_index"] = std::move(idx);
      }
    }
    matches.push_back(std::move(r));
  }
  return matches;
}

auto MatchesInstruction(const Instruction& instr, std::unordered_map<std::string, Operand::Index>& captured_index_values, const InstructionPattern& pattern, const MatchContext& context) -> bool {
  if (pattern.opcode.has_value() && instr.opcode != *pattern.opcode) return false;
  if (pattern.saturate.has_value() && instr.controls.saturate != *pattern.saturate) return false;
  if (pattern.test_boolean >= 0 && instr.controls.test_boolean != static_cast<uint32_t>(pattern.test_boolean)) return false;
  if (pattern.interpolation_mode.has_value()) {
    if (instr.opcode != static_cast<uint32_t>(Opcode::DclInputPs) && instr.opcode != static_cast<uint32_t>(Opcode::DclInputPsSiv) && instr.opcode != static_cast<uint32_t>(Opcode::DclInputPsSgv)) return false;
    if (instr.controls.input_interpolation_mode.has_value() && *instr.controls.input_interpolation_mode != static_cast<uint32_t>(*pattern.interpolation_mode)) return false;
  }
  if (pattern.dimension.has_value() && instr.controls.resource_dimension != pattern.dimension) return false;
  for (uint32_t component = 0; component < 4; ++component) {
    if (pattern.return_type[component].has_value() && instr.controls.resource_return_type[component] != pattern.return_type[component]) return false;
  }
  if (pattern.structure_stride != 0 && instr.controls.structure_stride != pattern.structure_stride) return false;
  if (pattern.access_pattern.has_value() && instr.controls.access_pattern != pattern.access_pattern) return false;
  if (pattern.mode.has_value() && instr.controls.mode != pattern.mode) return false;
  if (pattern.uav_flags != 0 && instr.controls.uav_flags != pattern.uav_flags) return false;
  // Extended-opcode expectations: absent = wildcard (any chain, including
  // none); present = exact full-chain match (count + per-entry rules).
  if (pattern.extended_opcodes.has_value()) {
    const auto& pattern_ext = *pattern.extended_opcodes;
    if (pattern_ext.size() != instr.controls.extended_op_codes.size()) return false;
    for (size_t i = 0; i < pattern_ext.size(); ++i) {
      const auto& pattern_val = pattern_ext[i];
      const auto& instr_val = instr.controls.extended_op_codes[i];
      switch (pattern_val.kind) {
        case ExtendedOpcodePattern::Kind::Any:
          break;
        case ExtendedOpcodePattern::Kind::Raw:
          if (pattern_val.raw != instr_val.value) return false;
          break;
        case ExtendedOpcodePattern::Kind::Type: {
          if (static_cast<dxp::sm5::ExtendedOpcodeType>(instr_val.value & kExtendedOpcodeMask) != pattern_val.type) {
            return false;
          }
          // Structured payload expectations compare the decoded token.
          if (pattern_val.sample_controls.has_value() || pattern_val.resource_dim.has_value()
              || pattern_val.resource_return_type.has_value()) {
            const auto kDecoded = ParseExtendedOpcodeToken(instr_val.value);
            if (pattern_val.sample_controls.has_value()) {
              const auto* kPayload = std::get_if<SampleControlsPayload>(&kDecoded.payload);
              if (kPayload == nullptr) return false;
              if (kPayload->u != pattern_val.sample_controls->u || kPayload->v != pattern_val.sample_controls->v
                  || kPayload->w != pattern_val.sample_controls->w) {
                return false;
              }
            }
            if (pattern_val.resource_dim.has_value()) {
              const auto* kPayload = std::get_if<ResourceDimPayload>(&kDecoded.payload);
              if (kPayload == nullptr) return false;
              if (kPayload->dimension != pattern_val.resource_dim->dimension
                  || kPayload->structure_stride != pattern_val.resource_dim->structure_stride) {
                return false;
              }
            }
            if (pattern_val.resource_return_type.has_value()) {
              const auto* kPayload = std::get_if<ResourceReturnTypePayload>(&kDecoded.payload);
              if (kPayload == nullptr) return false;
              if (kPayload->component_types != pattern_val.resource_return_type->component_types) {
                return false;
              }
            }
          }
          break;
        }
      }
    }
  }
  if (pattern.operands.size() > instr.operands.size()) return false;
  for (size_t i = 0; i < pattern.operands.size(); ++i) {
    if (!MatchesOperand(instr.operands[i], captured_index_values, pattern.operands[i], context)) return false;
  }
  return true;
}

/// @brief Resolves the operand's register through the declaration index and
/// compares every specified constraint field. Missing declaration = no match.
bool MatchesDeclConstraint(const Operand& operand, const OperandPattern::DeclConstraint& constraint, const MatchContext& context) {
  if (operand.index_entries.empty() || !operand.index_entries.front().immediate_lo.has_value()) {
    return false;
  }
  const uint32_t register_index = *operand.index_entries.front().immediate_lo;
  const auto& index = context.declarations;

  switch (operand.type) {
    case OperandType::Resource:
    case OperandType::UAV:      {
      const ResourceDeclInfo* decl = index.FindResourceDecl(operand.type, register_index);
      if (decl == nullptr) return false;
      if (constraint.dimension.has_value() && decl->dimension != *constraint.dimension) return false;
      for (uint32_t component = 0; component < 4; ++component) {
        if (constraint.return_type[component].has_value() && decl->return_types[component] != *constraint.return_type[component]) return false;
      }
      if (constraint.structure_stride.has_value() && decl->structure_stride != *constraint.structure_stride) return false;
      if (constraint.mode.has_value() || constraint.access_pattern.has_value()
          || constraint.semantic.has_value() || constraint.interpolation.has_value()) {
        return false;  // fields invalid for this operand type never match
      }
      return true;
    }
    case OperandType::Sampler: {
      auto it = index.samplers.find(register_index);
      if (it == index.samplers.end()) return false;
      if (constraint.mode.has_value() && it->second != *constraint.mode) return false;
      if (constraint.dimension.has_value() || constraint.structure_stride.has_value()
          || constraint.access_pattern.has_value() || constraint.semantic.has_value() || constraint.interpolation.has_value()) {
        return false;
      }
      for (uint32_t component = 0; component < 4; ++component) {
        if (constraint.return_type[component].has_value()) return false;
      }
      return true;
    }
    case OperandType::CBuffer: {
      auto it = index.cbuffers.find(register_index);
      if (it == index.cbuffers.end()) return false;
      if (constraint.access_pattern.has_value() && it->second.access_pattern != *constraint.access_pattern) return false;
      if (constraint.dimension.has_value() || constraint.structure_stride.has_value()
          || constraint.mode.has_value() || constraint.semantic.has_value() || constraint.interpolation.has_value()) {
        return false;
      }
      for (uint32_t component = 0; component < 4; ++component) {
        if (constraint.return_type[component].has_value()) return false;
      }
      return true;
    }
    case OperandType::Input:
    case OperandType::Output: {
      const auto& signatures = operand.type == OperandType::Input ? index.inputs : index.outputs;
      auto it = signatures.find(register_index);
      if (it == signatures.end()) return false;
      if (constraint.semantic.has_value() && (!it->second.semantic.has_value() || *it->second.semantic != *constraint.semantic)) return false;
      if (constraint.interpolation.has_value() && (!it->second.interpolation.has_value() || *it->second.interpolation != *constraint.interpolation)) return false;
      if (constraint.dimension.has_value() || constraint.structure_stride.has_value()
          || constraint.mode.has_value() || constraint.access_pattern.has_value()) {
        return false;
      }
      for (uint32_t component = 0; component < 4; ++component) {
        if (constraint.return_type[component].has_value()) return false;
      }
      return true;
    }
    default:
      return false;  // decl constraints only apply to declaration-backed operand types
  }
}

auto MatchesOperand(const Operand& operand, std::unordered_map<std::string, Operand::Index>& captured_index_values, const OperandPattern& op, const MatchContext& context) -> bool {
  if (op.any) return true;
  if (op.type.has_value() && operand.type != *op.type) return false;
  if (op.num_components >= 0 && static_cast<uint32_t>(operand.components.num_components) != static_cast<uint32_t>(op.num_components)) return false;
  if (const auto kExpectedMode = PatternComponentMode(op)) {
    if (operand.component_mode != *kExpectedMode) return false;
  }
  if (op.modifier.has_value() && operand.modifier != *op.modifier) return false;
  // decl constraint: the operand's register must resolve to a declaration whose
  // payload matches all specified fields. Missing declaration/index = no match.
  if (op.decl.has_value()) {
    if (!MatchesDeclConstraint(operand, *op.decl, context)) return false;
  }
  // match_capture: the operand must equal a previously captured operand. The
  // cross-step global store (captured by a prior step's match) is authoritative;
  // same-match references are resolved the same way once earlier patterns store.
  if (!op.match_capture.empty()) {
    auto git = context.captures.operands.find(op.match_capture);
    if (git == context.captures.operands.end()) return false;
    if (operand != git->second.operand_data) return false;
  }
  if (!op.IndexPatterns().empty()) {
    const auto& patterns = op.IndexPatterns();
    if (operand.index_entries.size() != patterns.size()) return false;
    for (size_t i = 0; i < patterns.size(); ++i) {
      if (!MatchesOperandIndex(operand.index_entries[i], captured_index_values, patterns[i], context)) return false;
      const auto& ip = patterns[i];
      if (!ip.capture.empty()) {
        const uint32_t* cur = IndexValueForCapture(operand.index_entries[i]);
        if (cur != nullptr) captured_index_values[ip.capture] = operand.index_entries[i];
      }
    }
  }
  return true;
}

auto MatchesOperandIndex(const Operand::Index& idx, const std::unordered_map<std::string, Operand::Index>& captured_index_values, const OperandIndexPattern& pattern, const MatchContext& context) -> bool {
  if (pattern.any) return true;
  if (static_cast<uint32_t>(idx.representation) != static_cast<uint32_t>(pattern.representation)) return false;
  if (pattern.immediate_lo.has_value() && idx.immediate_lo != pattern.immediate_lo) return false;
  if (pattern.immediate_hi.has_value() && idx.immediate_hi != pattern.immediate_hi) return false;
  if (!pattern.match_capture.empty()) {
    // Same-match equality first (a value captured earlier in this match), then
    // the cross-step global store (captured by a prior step).
    const uint32_t* cv = nullptr;
    auto it = captured_index_values.find(pattern.match_capture);
    if (it != captured_index_values.end()) {
      cv = IndexValueForCapture(it->second);
    } else {
      auto git = context.captures.index_values.find(pattern.match_capture);
      if (git != context.captures.index_values.end()) cv = IndexValueForCapture(git->second);
    }
    const uint32_t* cur = IndexValueForCapture(idx);
    if ((cv == nullptr) || (cur == nullptr) || *cv != *cur) return false;
  }
  return true;
}

}  // namespace

uint32_t ExtractComponentMask(uint32_t fromComponentMode, uint32_t fromSelectionMode) {
  switch (static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(fromSelectionMode)) {
    case D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE: {
      const uint32_t mask = DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(fromComponentMode);
      return mask >> 4;
    }
    case D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE: {
      const uint32_t selected = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(fromComponentMode);
      return 1U << selected;
    }
    case D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE: {
      uint32_t unique = 0;
      for (int c = 0; c < 4; ++c) {
        const uint32_t src = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_SOURCE(fromComponentMode, c);
        unique |= (1U << src);
      }
      return unique;
    }
    default:
      return kAllComponentsMask;
  }
}

auto CollectMatches(const std::vector<Instruction>& instructions, const InstructionPattern& pattern,
                    const MatchContext& context) -> std::vector<MatchResult> {
  return CollectMatchesImpl(instructions, pattern, context);
}

auto CollectSequenceMatches(const std::vector<Instruction>& instructions, const std::vector<InstructionPattern>& patterns,
                            const MatchContext& context) -> std::vector<MatchResult> {
  return CollectSequenceMatchesImpl(instructions, patterns, context);
}

auto CollectWindowMatches(const std::vector<Instruction>& instructions, const InstructionPattern& start_pattern,
                          const InstructionPattern& end_pattern, const MatchContext& context) -> std::vector<MatchResult> {
  return CollectWindowMatchesImpl(instructions, start_pattern, end_pattern, context);
}

}  // namespace dxp::sm5::step::common
