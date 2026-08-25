#include "value_types/indirect.h"

#include <algorithm>
#include <dxp/sm5/step/ApplyRuleStep.hpp>
#include <format>
#include <iterator>
#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/Condition_impl.hpp"
#include "dxp/ExportTypes.hpp"
#include "dxp/Logging.hpp"
#include "dxp/ResultFieldTraits.hpp"
#include "dxp/sm5/ShaderProgram.hpp"
#include "dxp/sm5/step/ApplyRuleStep_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/Model.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm5::step {
using namespace dxp::sm5::model;

namespace {

/// SM5 token/bit encoding constants.
constexpr uint32_t kBitsPerDword = 32U;
constexpr uint64_t kU32Mask = 0xFFFFFFFFULL;
constexpr uint32_t kExtendedOpcodeMask = 0x3fU;
constexpr uint32_t kAllComponentsMask = 0xfU;

struct OperandIndexMatchPattern {
  bool any = false;
  bool has_representation = false;
  Operand::IndexRepresentation representation = Operand::IndexRepresentation::Immediate32;
  std::optional<uint32_t> immediate_lo;
  std::optional<uint32_t> immediate_hi;
  std::string capture;
  std::string match_capture;
};
/// @brief True for opcodes whose immediate operands are integers.
/// DXBC immediates carry no type info (only width), so export labeling infers the
/// type from the opcode: integer ALU, load/store addresses/offsets, and atomics
/// are integer; everything else is treated as float. Ambiguous ops (Mov, sample
/// offsets) default to float. Best-effort heuristic — see MS shader-model docs.
bool IsIntegerImmediateOpcode(Opcode opcode) {
  switch (opcode) {
    case Opcode::And:
    case Opcode::Or:
    case Opcode::Xor:
    case Opcode::Not:
    case Opcode::IAdd:
    case Opcode::IEq:
    case Opcode::IGe:
    case Opcode::ILt:
    case Opcode::INe:
    case Opcode::INeg:
    case Opcode::IMad:
    case Opcode::IMax:
    case Opcode::IMin:
    case Opcode::IMul:
    case Opcode::IShl:
    case Opcode::IShr:
    case Opcode::UDiv:
    case Opcode::UMul:
    case Opcode::UMad:
    case Opcode::UMax:
    case Opcode::UMin:
    case Opcode::ULt:
    case Opcode::UGe:
    case Opcode::UShr:
    case Opcode::UAddC:
    case Opcode::USubb:
    case Opcode::CountBits:
    case Opcode::FirstBitHi:
    case Opcode::FirstBitLo:
    case Opcode::FirstBitSHI:
    case Opcode::UBFE:
    case Opcode::IBFE:
    case Opcode::BFI:
    case Opcode::BFRev:
    case Opcode::SwapC:
    case Opcode::MSAD:
    case Opcode::Itof:
    case Opcode::Utof:
    case Opcode::IToD:
    case Opcode::UToD:
    // Load/store address and offset immediates are integers.
    case Opcode::Ld:
    case Opcode::LdMs:
    case Opcode::LdUavTyped:
    case Opcode::LdRaw:
    case Opcode::LdStructured:
    case Opcode::StoreUavTyped:
    case Opcode::StoreRaw:
    case Opcode::StoreStructured:
    case Opcode::LdFeedback:
    case Opcode::LdMsFeedback:
    case Opcode::LdUavTypedFeedback:
    case Opcode::LdRawFeedback:
    case Opcode::LdStructuredFeedback:
    case Opcode::Resinfo:
    // Atomics operate on integers.
    case Opcode::AtomicAnd:
    case Opcode::AtomicOr:
    case Opcode::AtomicXor:
    case Opcode::AtomicCmpStore:
    case Opcode::AtomicIAdd:
    case Opcode::AtomicIMax:
    case Opcode::AtomicIMin:
    case Opcode::AtomicUMax:
    case Opcode::AtomicUMin:
    case Opcode::ImmAtomicAlloc:
    case Opcode::ImmAtomicConsume:
    case Opcode::ImmAtomicIAdd:
    case Opcode::ImmAtomicAnd:
    case Opcode::ImmAtomicOr:
    case Opcode::ImmAtomicXor:
    case Opcode::ImmAtomicExch:
    case Opcode::ImmAtomicCmpExch:
    case Opcode::ImmAtomicIMax:
    case Opcode::ImmAtomicIMin:
    case Opcode::ImmAtomicUMax:
    case Opcode::ImmAtomicUMin:
      return true;
    default:
      return false;
  }
}

struct MatchResult {
  uint32_t instruction_index = 0;
  const Instruction* instruction = nullptr;
  uint32_t range_start_index = 0;
  uint32_t range_end_index = 0;
  std::unordered_map<std::string, CapturedOperand> operands;
  std::unordered_map<std::string, Instruction> instructions;
  std::unordered_map<std::string, Operand::Index> index_values;
};

/// @brief Copies a match's captures into the global capture store (shared by the
/// probe path and the rewrite path).
void StoreCaptures(ExecutionContext& ctx, const MatchResult& match) {
  for (const auto& entry : match.operands) {
    ctx.captures.operands.emplace(entry.first, entry.second);
  }
  for (const auto& entry : match.instructions) {
    ctx.captures.instructions[entry.first] = CapturedInstruction{entry.second};
  }
  ctx.captures.index_values.insert(match.index_values.begin(), match.index_values.end());
}

struct MatchPatternResolved {
  InstructionPattern single_match;
  std::vector<InstructionPattern> sequence_matches;
};

struct MatchPattern {
  std::optional<Opcode> opcode;
  std::string capture;
  std::optional<bool> saturate;
  std::optional<InterpolationMode> interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<OperandPattern> operands;
  std::vector<InstructionPattern> sequence;
};

enum class RewriteActionType : std::uint8_t {
  ReplaceRange,
  InsertBefore,
};

struct RewriteAction {
  RewriteActionType type = RewriteActionType::ReplaceRange;
  uint32_t replace_index = 0;
  uint32_t range_start = 0;
  uint32_t range_end = 0;
  uint32_t insert_position = 0;
  uint32_t remove_start = 0;
  uint32_t remove_end = 0;
  uint32_t required_temp_count = 0;
  std::vector<Instruction> new_instructions;
};

auto CollectMatches(const ShaderProgram& program, const InstructionPattern& pattern, CaptureStore& captures, const ExecutionContext& context) -> std::vector<MatchResult>;
auto CollectSequenceMatches(const ShaderProgram& program, const std::vector<InstructionPattern>& patterns, CaptureStore& captures, const ExecutionContext& context) -> std::vector<MatchResult>;
auto ResolveEmit(const MatchResult& match, ExecutionContext& context, const EmitPattern& emit, const std::string& path, std::string& error) -> Instruction;
auto ResolveOperand(const MatchResult& match, ExecutionContext& context, const OperandPattern& op, const std::string& path, std::string& error, size_t emit_operand_index) -> Operand;
auto ResolveOperandIndex(const MatchResult& match, ExecutionContext& context, const OperandIndexPattern& pattern, const std::string& path, std::string& error) -> Operand::Index;
auto MatchesOperand(const Operand& operand, std::unordered_map<std::string, Operand::Index>& captured_index_values, const OperandPattern& op, const ExecutionContext& context) -> bool;
auto MatchesOperandIndex(const Operand::Index& idx, const std::unordered_map<std::string, Operand::Index>& captured_index_values, const OperandIndexPattern& pattern, const ExecutionContext& context) -> bool;
auto MatchesInstruction(const Instruction& instr, std::unordered_map<std::string, Operand::Index>& captured_index_values, const InstructionPattern& pattern, const ExecutionContext& context) -> bool;
bool ValidateOperandRole(const Operand& operand, OperandRole expected_role, const std::string& path, ExecutionContext& context, std::string& error);
bool ResolveImmediateFromVariable(const std::string& path, const std::string& vn, const ExecutionContext& ctx, ImmediateFamily family, uint32_t& ol, uint32_t& oh, bool& hh, std::string& error);
auto ExecuteSingleRuleImpl(dxp::sm5::ShaderProgram& program, const std::string& step_name,
                           const Rule& rule_model, MatchKind mode,
                           bool required, RewriteKind rewrite_mode,
                           ExecutionContext& ctx) -> std::expected<dxp::ApplyRuleResults, std::string>;

bool ApplyRewriteActions(ShaderProgram& program, const std::vector<RewriteAction>& actions) {
  if (actions.empty()) {
    return true;
  }

  auto get_pos = [](const RewriteAction& a) -> uint32_t {
    return a.type == RewriteActionType::ReplaceRange ? a.range_start : a.insert_position;
  };

  std::vector<const RewriteAction*> sorted_actions;
  sorted_actions.reserve(actions.size());
  for (const auto& action : actions) {
    sorted_actions.push_back(&action);
  }
  std::ranges::sort(sorted_actions, [&get_pos](const RewriteAction* a, const RewriteAction* b) {
    return get_pos(*a) < get_pos(*b);
  });

  size_t out_size = program.instructions.size();
  for (const auto* action : sorted_actions) {
    out_size += action->new_instructions.size();
    if (action->type == RewriteActionType::ReplaceRange) {
      out_size -= (action->range_end - action->range_start + 1);
    }
  }

  std::vector<Instruction> output;
  output.reserve(out_size);

  uint32_t instr_idx = 0;
  size_t a_idx = 0;

  while (instr_idx < program.instructions.size()) {
    while (a_idx < sorted_actions.size() && get_pos(*sorted_actions[a_idx]) == instr_idx) {
      const auto& action = *sorted_actions[a_idx];

      if (action.type == RewriteActionType::InsertBefore) {
        output.insert(output.end(), action.new_instructions.begin(), action.new_instructions.end());
      } else {
        output.insert(output.end(), action.new_instructions.begin(), action.new_instructions.end());
        instr_idx = action.range_end + 1;
        ++a_idx;
        break;
      }
      ++a_idx;
    }

    if (instr_idx < program.instructions.size() && (a_idx >= sorted_actions.size() || get_pos(*sorted_actions[a_idx]) != instr_idx)) {
      output.push_back(std::move(program.instructions[instr_idx]));
      ++instr_idx;
    }
  }

  while (a_idx < sorted_actions.size()) {
    output.insert(output.end(), sorted_actions[a_idx]->new_instructions.begin(), sorted_actions[a_idx]->new_instructions.end());
    ++a_idx;
  }

  program.instructions = std::move(output);
  return true;
}
inline uint32_t ExtractComponentMask(uint32_t fromComponentMode, uint32_t fromSelectionMode) {
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

/// @brief Maps a component letter to its xyzw index (x=0, y=1, z=2, w=3), or -1.
inline int ComponentIndex(char c) {
  switch (c) {
    case 'x': return 0;
    case 'y': return 1;
    case 'z': return 2;
    case 'w': return 3;
    default:  return -1;
  }
}

/// @brief Converts an operand pattern's component spec (mask/swizzle/select value
/// strings) into the ground-truth component mode (token bits 2-11). Returns
/// std::nullopt when the pattern carries no component constraint. The value
/// strings are validated against the xyzw alphabet by the recipe Validate phase.
inline auto PatternComponentMode(const OperandPattern& op) -> std::optional<uint32_t> {
  if (!op.mask.empty()) {
    uint32_t mask = 0;
    for (const char c : op.mask) {
      const int idx = ComponentIndex(c);
      if (idx < 0) return std::nullopt;
      mask |= (1U << idx);
    }
    // ENCODE_..._MASK takes the mask in token position (bits 4-7), so the
    // xyzw nibble is shifted up by the mask field's bit offset.
    return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(mask << 4);
  }
  if (!op.swizzle.empty()) {
    uint32_t swizzle = 0;
    int slot = 0;
    for (const char c : op.swizzle) {
      const int idx = ComponentIndex(c);
      if (idx < 0) return std::nullopt;
      swizzle |= (static_cast<uint32_t>(idx) << (slot * 2));
      ++slot;
    }
    return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) | (swizzle << 4);
  }
  if (!op.select.empty()) {
    const int idx = ComponentIndex(op.select.front());
    if (idx < 0) return std::nullopt;
    return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(static_cast<uint32_t>(idx));
  }
  return std::nullopt;
}

auto SelectMatchIndices(const std::vector<MatchResult>& matches,
                        MatchKind application_mode) -> std::vector<uint32_t> {
  std::vector<uint32_t> selected;
  if (matches.empty()) {
    return selected;
  }
  switch (application_mode) {
    case MatchKind::First:
      selected.push_back(0);
      break;
    case MatchKind::Last:
      selected.push_back(static_cast<uint32_t>(matches.size() - 1));
      break;
    case MatchKind::MatchAll:
      selected.reserve(matches.size());
      for (uint32_t i = 0; i < matches.size(); ++i) {
        selected.push_back(i);
      }
      break;
  }
  return selected;
}

auto ResolveInsertAnchorIndex(const MatchResult& match, const std::string& rewrite_path, int32_t insert_idx, std::string& error) -> bool {
  if (insert_idx < 0) {
    error = rewrite_path + ": before/after rewrites require non-negative insert index";
    return false;
  }
  const uint32_t kWindowStart = match.range_start_index;
  const uint32_t kWindowEnd = match.range_end_index;
  if (kWindowStart > kWindowEnd) {
    error = rewrite_path + ": invalid SM5 match window";
    return false;
  }
  const uint32_t kWindowLength = kWindowEnd - kWindowStart + 1;
  const auto kRelativeIndex = static_cast<uint32_t>(insert_idx);
  if (kRelativeIndex >= kWindowLength) {
    error = rewrite_path + ": relative_index is out of match window bounds";
    return false;
  }
  return true;
}

auto ResolveReplacementRange(RewriteKind rewrite_mode, int32_t range_start_offset, int32_t range_end_offset, const MatchResult& match, const std::string& rewrite_path,
                             uint32_t& range_start, uint32_t& range_end, std::string& error) -> bool {
  if (rewrite_mode == RewriteKind::Replace) {
    // Replace swaps the entire matched window (mirrors sm6: all matched
    // instructions are erased and the emit block is inserted in their place).
    // Use ReplaceRange + offsets for a custom sub-range.
    range_start = match.range_start_index;
    range_end = match.range_end_index;
    return true;
  }
  if (rewrite_mode != RewriteKind::ReplaceRange) {
    error = rewrite_path + ": only Replace and ReplaceRange modes use range resolution";
    return false;
  }
  if (range_start_offset < 0 || range_end_offset < -1) {
    error = rewrite_path + ": invalid range offsets in rewrite rule";
    return false;
  }
  const uint32_t kWindowStart = match.range_start_index;
  const uint32_t kWindowEnd = match.range_end_index;
  const uint32_t kWindowLength = kWindowEnd - kWindowStart + 1;
  const auto kStartOffset = static_cast<uint32_t>(range_start_offset);
  const int32_t kEndOffset = range_end_offset;
  if (kStartOffset >= kWindowLength || (kEndOffset >= 0 && std::cmp_greater_equal(kEndOffset, kWindowLength))) {
    error = rewrite_path + ": range offset out of match window bounds";
    return false;
  }
  range_start = kWindowStart + kStartOffset;
  if (kEndOffset < 0) {
    range_end = kWindowEnd;
    return true;
  }
  range_end = kWindowStart + static_cast<uint32_t>(kEndOffset);
  if (range_start > range_end) {
    error = rewrite_path + ": range start exceeds range end";
    return false;
  }
  return true;
}

auto ResolveRangeReplacement(RewriteKind rewrite_mode, int32_t range_start_offset, int32_t range_end_offset, int32_t insert_index, const MatchResult& match, const std::string& rewrite_path,
                             RewriteAction& action, std::string& error) -> bool {
  if (rewrite_mode == RewriteKind::Replace) {
    uint32_t range_start = 0;
    uint32_t range_end = 0;
    if (!ResolveReplacementRange(rewrite_mode, range_start_offset, range_end_offset, match, rewrite_path, range_start, range_end, error)) {
      return false;
    }
    action.type = RewriteActionType::ReplaceRange;
    action.replace_index = range_start;
    action.range_start = range_start;
    action.range_end = range_end;
    return true;
  }

  if (rewrite_mode == RewriteKind::Before) {
    if (!ResolveInsertAnchorIndex(match, rewrite_path, insert_index, error)) {
      return false;
    }
    action.type = RewriteActionType::InsertBefore;
    action.insert_position = static_cast<uint32_t>(insert_index);
    return true;
  }

  if (rewrite_mode == RewriteKind::After) {
    if (insert_index < 0) {
      insert_index = static_cast<int32_t>(match.range_end_index);
    }
    if (!ResolveInsertAnchorIndex(match, rewrite_path, insert_index, error)) {
      return false;
    }
    action.type = RewriteActionType::InsertBefore;
    action.insert_position = static_cast<uint32_t>(insert_index) + 1;
    return true;
  }

  if (rewrite_mode == RewriteKind::ReplaceRange) {
    uint32_t range_start = 0;
    uint32_t range_end = 0;
    if (!ResolveReplacementRange(rewrite_mode, range_start_offset, range_end_offset, match, rewrite_path, range_start, range_end, error)) {
      return false;
    }
    action.type = RewriteActionType::ReplaceRange;
    action.replace_index = range_start;
    action.range_start = range_start;
    action.range_end = range_end;
    return true;
  }

  error = rewrite_path + ": unsupported SM5 rewrite mode";
  return false;
}

auto EvaluateRuleRewriteCallback(RewriteKind rewrite_mode, const Rule& rule, int32_t range_start_offset, int32_t range_end_offset, int32_t insert_index,
                                 [[maybe_unused]] const std::string& step_name, [[maybe_unused]] bool required,
                                 [[maybe_unused]] const dxp::sm5::ShaderProgram& program, const MatchResult& match,
                                 const std::string& rewrite_path, ExecutionContext& ctx,
                                 std::vector<RewriteAction>& actions, std::string& error) -> bool {
  error.clear();
  actions.clear();
  RewriteAction action;
  if (!ResolveRangeReplacement(rewrite_mode, range_start_offset, range_end_offset, insert_index, match, rewrite_path, action, error)) {
    return false;
  }

  for (size_t emit_index = 0; emit_index < rule.emit_patterns.size(); ++emit_index) {
    const std::string kEmitPath = rewrite_path + ".emit_patterns[" + std::to_string(emit_index) + "]";
    Instruction resolved_instruction = ResolveEmit(match, ctx, rule.emit_patterns[emit_index], kEmitPath, error);
    if (!error.empty()) return false;
    action.new_instructions.push_back(std::move(resolved_instruction));
  }

  actions.push_back(std::move(action));
  return true;
}

auto ExecuteSingleRuleImpl(dxp::sm5::ShaderProgram& program, const std::string& step_name,
                           const Rule& rule_model, MatchKind mode,
                           bool required, RewriteKind rewrite_mode,
                           int32_t insert_index, int32_t range_start_offset, int32_t range_end_offset,
                           ExecutionContext& ctx) -> std::expected<dxp::ApplyRuleResults, std::string> {
  dxp::ApplyRuleResults result;

  const std::string& rule_name = step_name;

  std::vector<MatchResult> matches;
  if (rule_model.match_patterns.size() > 1) {
    matches = CollectSequenceMatches(program, rule_model.match_patterns, ctx.captures, ctx);
  } else {
    matches = CollectMatches(program, rule_model.match_patterns.front(), ctx.captures, ctx);
  }
  const bool kMatchedRule = !matches.empty();
  if (!rule_name.empty()) {
    ctx.state[rule_name] = kMatchedRule;
  }
  result.match_count = static_cast<uint32_t>(matches.size());

  for (const auto& match : matches) {
    for (const auto& [cap_name, cap] : match.operands) {
      if (!cap.export_as.has_value()) continue;
      const std::string& export_key = *cap.export_as;
      if (cap.operand_data.type == OperandType::Resource || cap.operand_data.type == OperandType::Sampler || cap.operand_data.type == OperandType::CBuffer) {
        dxp::ResourceUsage usage;
        usage.register_index = cap.operand_data.index_entries.empty() || !cap.operand_data.index_entries[0].immediate_lo.has_value() ? 0 : *cap.operand_data.index_entries[0].immediate_lo;
        if (cap.operand_data.type == OperandType::CBuffer) {
          usage.handle = "cbuffer";
        } else if (cap.operand_data.type == OperandType::Sampler) {
          usage.handle = "sampler";
        } else {
          usage.handle = "texture";
        }
        if (cap.role == OperandRole::Destination && (cap.destination_mask != 0u)) {
          usage.accessed_components = cap.destination_mask;
        } else {
          auto sel = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(cap.operand_data.component_mode);
          usage.accessed_components = ExtractComponentMask(cap.operand_data.component_mode, sel);
        }
        ctx.resource_exports[export_key] = std::move(usage);
      } else if (cap.operand_data.type == OperandType::Immediate32 || cap.operand_data.type == OperandType::Immediate64) {
        bool has_relative = false;
        for (const auto& idx : cap.operand_data.index_entries) {
          if (idx.representation == Operand::IndexRepresentation::Relative || idx.representation == Operand::IndexRepresentation::Immediate32PlusRelative || idx.representation == Operand::IndexRepresentation::Immediate64PlusRelative) {
            has_relative = true;
            break;
          }
        }
        if (has_relative) continue;

        dxp::ImmediateValue imm;
        // DXBC carries no type info on immediate operands; infer from the opcode.
        // Integer ops (incl. load/store addresses and atomics) label I32/I64;
        // everything else is treated as float (ambiguous ops like Mov default float).
        const bool is_64bit = cap.operand_data.type == OperandType::Immediate64;
        const bool is_int = match.instruction != nullptr && IsIntegerImmediateOpcode(match.instruction->opcode);
        if (is_int) {
          imm.type = is_64bit ? dxp::ComponentType::I64 : dxp::ComponentType::I32;
        } else {
          imm.type = is_64bit ? dxp::ComponentType::F64 : dxp::ComponentType::F32;
        }
        for (const auto& idx : cap.operand_data.index_entries) {
          if (idx.immediate_hi.has_value()) {
            imm.raw_values.push_back(static_cast<uint64_t>(*idx.immediate_lo) | (static_cast<uint64_t>(*idx.immediate_hi) << kBitsPerDword));
          } else {
            imm.raw_values.push_back(*idx.immediate_lo);
          }
        }
        if (!imm.raw_values.empty()) {
          ctx.immediate_exports[export_key] = std::move(imm);
        }
      }
    }
  }
  if (matches.empty()) {
    return result;
  }

  const auto kSelectedMatches = SelectMatchIndices(matches, mode);

  if (rewrite_mode == RewriteKind::None) {
    if (!kSelectedMatches.empty()) {
      StoreCaptures(ctx, matches[kSelectedMatches.back()]);
    }
    ctx.state[step_name] = true;
    return result;
  }

  std::vector<RewriteAction> actions;
  actions.reserve(kSelectedMatches.size());
  for (const uint32_t selected_index : kSelectedMatches) {
    const auto& match = matches[selected_index];

    StoreCaptures(ctx, match);

    std::vector<RewriteAction> local_actions;
    std::string error;
    const std::string kRewritePath = "step[" + step_name + "].match[" + std::to_string(selected_index) + "]";
    if (!EvaluateRuleRewriteCallback(rewrite_mode, rule_model, range_start_offset, range_end_offset, insert_index, step_name, required, program, match, kRewritePath, ctx,
                                     local_actions, error)) {
      return std::unexpected(std::move(error));
    }
    if (local_actions.empty()) {
      continue;
    }

    for (RewriteAction& action : local_actions) {
      actions.push_back(std::move(action));
    }
    ++result.applied_count;
  }

  if (actions.empty()) {
    ctx.state[step_name] = true;
    return result;
  }

  if (!ApplyRewriteActions(program, actions)) {
    return std::unexpected("step[" + step_name + "]: failed to apply rewrite action");
  }

  ctx.program_modified = true;

  ctx.state[step_name] = true;
  return result;
}

}  // namespace

auto RuleData::Compile() const -> std::expected<Rule, std::string> {
  std::string error;
  Rule rule{};

  auto convert_operand = [&](auto&& self, const OperandData& operand_data, bool is_emit_operand = false) -> std::expected<OperandPattern, std::string> {
    auto convert = [&self](const OperandData& d, bool e) {
      return self(self, d, e);
    };

    OperandPattern op_pattern;
    op_pattern.any = operand_data.any;
    if (operand_data.type.has_value()) {
      op_pattern.type = operand_data.type;
    }
    if (!operand_data.capture.empty()) {
      op_pattern.capture = operand_data.capture;
    }
    if (!operand_data.match_capture.empty()) {
      op_pattern.match_capture = operand_data.match_capture;
    }
    if (operand_data.modifier.has_value()) {
      op_pattern.modifier = operand_data.modifier;
    }
    if (operand_data.components.num_components != NumComponents::Four) {
      op_pattern.num_components = static_cast<int32_t>(operand_data.components.num_components);
    }
    if (operand_data.handle) {
      op_pattern.handle = OperandPattern::Handle{
          .name = operand_data.handle->name,
          .element_index = operand_data.handle->element_index,
      };
    }
    op_pattern.export_as = operand_data.export_as;
    if (operand_data.components.selection_mode == SelectionMode::Mask) {
      op_pattern.mask = operand_data.components.value;
    } else if (operand_data.components.selection_mode == SelectionMode::Swizzle) {
      op_pattern.swizzle = operand_data.components.value;
    } else if (operand_data.components.selection_mode == SelectionMode::Select) {
      op_pattern.select = operand_data.components.value;
    }
    for (const auto& idx : operand_data.indices) {
      OperandIndexPattern idx_pattern;
      idx_pattern.any = idx.any;
      idx_pattern.representation = idx.representation;
      if (idx.immediate_lo.has_value()) {
        idx_pattern.immediate_lo = idx.immediate_lo;
      }
      if (idx.immediate_hi.has_value()) {
        idx_pattern.immediate_hi = idx.immediate_hi;
      }
      if (!idx.capture.empty()) {
        idx_pattern.capture = idx.capture;
      }
      if (!idx.match_capture.empty()) {
        idx_pattern.match_capture = idx.match_capture;
      }
      if (idx.relative_operand) {
        auto rel_pattern = convert(*idx.relative_operand, is_emit_operand);
        if (!rel_pattern) {
          return std::unexpected(error);
        }
        idx_pattern.relative_operand = xyz::indirect<OperandPattern>(std::move(*rel_pattern));
      }
      op_pattern.indices.push_back(std::move(idx_pattern));
    }
    // Typed immediates shorthand carries through as-is (literals or variable names);
    // expansion into index patterns happens lazily via OperandPattern::IndexPatterns().
    op_pattern.immediates_u32 = operand_data.immediates_u32;
    op_pattern.immediates_u64 = operand_data.immediates_u64;
    op_pattern.immediates_i32 = operand_data.immediates_i32;
    op_pattern.immediates_i64 = operand_data.immediates_i64;
    op_pattern.immediates_f32 = operand_data.immediates_f32;
    op_pattern.immediates_f64 = operand_data.immediates_f64;
    if (operand_data.handle) {
      op_pattern.handle = OperandPattern::Handle{
          .name = operand_data.handle->name,
          .element_index = operand_data.handle->element_index,
      };
    }
    return op_pattern;
  };

  const std::vector<InstructionMatchData>& effective_match = match;
  if (effective_match.empty()) {
    error = "rules require at least one match instruction pattern";
    return std::unexpected(error);
  }
  for (const auto& match_item : effective_match) {
    InstructionPattern pattern;
    pattern.opcode = match_item.opcode;
    pattern.capture = match_item.capture;
    if (match_item.saturate.has_value()) {
      pattern.saturate = *match_item.saturate;
    }
    pattern.interpolation_mode = match_item.interpolation;
    pattern.test_boolean = match_item.test_boolean;
    for (const auto& operand : match_item.operands) {
      const bool has_indices = !operand.indices.empty();
      const bool has_immediates = !operand.immediates_u32.empty() || !operand.immediates_u64.empty() || !operand.immediates_i32.empty() || !operand.immediates_i64.empty() || !operand.immediates_f32.empty() || !operand.immediates_f64.empty();
      if (has_indices && has_immediates) {
        error =
            "SM5 match operands may use explicit indices or immediate shorthand arrays "
            "(immediates_u32/immediates_u64/immediates_i32/"
            "immediates_i64/immediates_f32/immediates_f64), but not both";
        return std::unexpected(error);
      }
      auto converted = convert_operand(convert_operand, operand);
      if (!converted) {
        return std::unexpected(error);
      }
      pattern.operands.push_back(std::move(*converted));
    }
    if (match_item.extended_opcodes.has_value()) {
      std::vector<ExtendedOpcodePattern> compiled_ext;
      compiled_ext.reserve(match_item.extended_opcodes->size());
      for (const auto& ext : *match_item.extended_opcodes) {
        ExtendedOpcodePattern compiled;
        const int kSpecCount = (ext.any ? 1 : 0) + (ext.type.has_value() ? 1 : 0) + (ext.raw.has_value() ? 1 : 0);
        if (kSpecCount != 1) {
          error = "SM5 extended_opcodes entries require exactly one of 'any', 'type', or 'raw'";
          return std::unexpected(error);
        }
        if (ext.any) {
          compiled.kind = ExtendedOpcodePattern::Kind::Any;
        } else if (ext.raw.has_value()) {
          compiled.kind = ExtendedOpcodePattern::Kind::Raw;
          compiled.raw = *ext.raw;
        } else {
          compiled.kind = ExtendedOpcodePattern::Kind::Type;
          compiled.type = *ext.type;
          compiled.sample_controls = ext.sample_controls;
          compiled.resource_dim = ext.resource_dim;
          if (ext.resource_return_type.has_value()) {
            compiled.resource_return_type = ResourceReturnTypePayload{*ext.resource_return_type};
          }
          if (compiled.sample_controls.has_value() && compiled.type != ExtendedOpcodeType::SampleControls) {
            error = "SM5 extended_opcodes: 'sample_controls' payload requires type: sample_controls";
            return std::unexpected(error);
          }
          if (compiled.resource_dim.has_value() && compiled.type != ExtendedOpcodeType::ResourceDim) {
            error = "SM5 extended_opcodes: 'resource_dim' payload requires type: resource_dim";
            return std::unexpected(error);
          }
          if (compiled.resource_return_type.has_value() && compiled.type != ExtendedOpcodeType::ResourceType) {
            error = "SM5 extended_opcodes: 'resource_return_type' payload requires type: resource_type";
            return std::unexpected(error);
          }
        }
        compiled_ext.push_back(compiled);
      }
      pattern.extended_opcodes = std::move(compiled_ext);
    }
    rule.match_patterns.push_back(std::move(pattern));
  }

  for (const auto& emit_entry : emit) {
    if (emit_entry.opcode.has_value() && !emit_entry.capture.empty()) {
      error = "SM5 emit cannot have both opcode and capture on the same instruction";
      return std::unexpected(error);
    }
    EmitPattern tpl;
    tpl.opcode = emit_entry.opcode;
    tpl.capture = emit_entry.capture;
    if (emit_entry.saturate.has_value()) {
      tpl.saturate = *emit_entry.saturate;
    }
    tpl.interpolation_mode = emit_entry.interpolation;
    tpl.test_boolean = emit_entry.test_boolean;
    for (const auto& operand : emit_entry.operands) {
      const bool has_indices = !operand.indices.empty();
      const bool has_immediates = !operand.immediates_u32.empty() || !operand.immediates_u64.empty() || !operand.immediates_i32.empty() || !operand.immediates_i64.empty() || !operand.immediates_f32.empty() || !operand.immediates_f64.empty();
      if (has_indices && has_immediates) {
        error =
            "SM5 emit operands may use explicit indices or immediate shorthand arrays "
            "(immediates_u32/immediates_u64/immediates_i32/"
            "immediates_i64/immediates_f32/immediates_f64), but not both";
        return std::unexpected(error);
      }
      auto operand_pattern_opt = convert_operand(convert_operand, operand, true);
      if (!operand_pattern_opt) {
        return std::unexpected(error);
      }
      auto& operand_pattern = *operand_pattern_opt;
      tpl.operands.push_back(std::move(operand_pattern));
    }
    for (const auto& ext : emit_entry.extended_opcodes) {
      ApplyRuleStep::EmitExtendedOpcode compiled;
      const int kSpecCount = (ext.type.has_value() ? 1 : 0) + (ext.raw.has_value() ? 1 : 0);
      if (kSpecCount != 1) {
        error = "SM5 emit extended_opcodes entries require exactly one of 'type' or 'raw'";
        return std::unexpected(error);
      }
      if (ext.raw.has_value()) {
        compiled.kind = ApplyRuleStep::EmitExtendedOpcode::Kind::Raw;
        compiled.raw = *ext.raw;
      } else {
        compiled.kind = ApplyRuleStep::EmitExtendedOpcode::Kind::Type;
        compiled.type = *ext.type;
        compiled.sample_controls = ext.sample_controls;
        compiled.resource_dim = ext.resource_dim;
        if (ext.resource_return_type.has_value()) {
          compiled.resource_return_type = ResourceReturnTypePayload{*ext.resource_return_type};
        }
        const bool has_payload = compiled.sample_controls.has_value() || compiled.resource_dim.has_value()
                                 || compiled.resource_return_type.has_value();
        if (!has_payload) {
          error = "SM5 emit extended_opcodes: 'type' entries require a structured payload";
          return std::unexpected(error);
        }
        if (compiled.sample_controls.has_value() && compiled.type != ExtendedOpcodeType::SampleControls) {
          error = "SM5 emit extended_opcodes: 'sample_controls' payload requires type: sample_controls";
          return std::unexpected(error);
        }
        if (compiled.resource_dim.has_value() && compiled.type != ExtendedOpcodeType::ResourceDim) {
          error = "SM5 emit extended_opcodes: 'resource_dim' payload requires type: resource_dim";
          return std::unexpected(error);
        }
        if (compiled.resource_return_type.has_value() && compiled.type != ExtendedOpcodeType::ResourceType) {
          error = "SM5 emit extended_opcodes: 'resource_return_type' payload requires type: resource_type";
          return std::unexpected(error);
        }
        if (compiled.sample_controls.has_value()) {
          const auto& sc = *compiled.sample_controls;
          const int32_t kMinOffset = -(1 << (kSampleControlOffsetBits - 1));
          const int32_t kMaxOffset = (1 << (kSampleControlOffsetBits - 1)) - 1;
          if (sc.u < kMinOffset || sc.u > kMaxOffset || sc.v < kMinOffset || sc.v > kMaxOffset
              || sc.w < kMinOffset || sc.w > kMaxOffset) {
            error = "SM5 emit extended_opcodes: sample_controls offsets must fit 4-bit two's complement (-8..7)";
            return std::unexpected(error);
          }
        }
        if (compiled.resource_dim.has_value()) {
          const uint32_t kDim = compiled.resource_dim->dimension;
          if (kDim < D3D10_SB_RESOURCE_DIMENSION_BUFFER
              || kDim > D3D11_SB_RESOURCE_DIMENSION_STRUCTURED_BUFFER) {
            error = "SM5 emit extended_opcodes: resource_dim dimension must be a valid D3D10_SB_RESOURCE_DIMENSION";
            return std::unexpected(error);
          }
        }
        if (compiled.resource_return_type.has_value()) {
          for (const uint32_t kType : compiled.resource_return_type->component_types) {
            if (kType < D3D10_SB_RETURN_TYPE_UNORM || kType > D3D10_SB_RETURN_TYPE_MIXED) {
              error = "SM5 emit extended_opcodes: resource_return_type components must be D3D10_SB_RESOURCE_RETURN_TYPE";
              return std::unexpected(error);
            }
          }
        }
      }
      tpl.extended_opcodes.push_back(compiled);
    }
    // Extended opcodes are only meaningful on opcodes whose canonical chain
    // supports them (resource-access opcodes); everything else must stay bare.
    if (!tpl.extended_opcodes.empty() && tpl.opcode.has_value()
        && !RequiredExtendedChainForOpcode(*tpl.opcode).RequiresResourcePair()) {
      error = "SM5 emit extended_opcodes are only supported on resource-access opcodes (ld, sample, gather4 families, resinfo)";
      return std::unexpected(error);
    }
    // sample_controls only ride on the sample/gather4 families.
    for (const auto& ext : tpl.extended_opcodes) {
      if (ext.sample_controls.has_value() && tpl.opcode.has_value()) {
        const auto kChain = RequiredExtendedChainForOpcode(*tpl.opcode);
        if (kChain.kind != ExtendedChainKind::ResourcePairControls
            && kChain.kind != ExtendedChainKind::ResourcePairControlsFixed) {
          error = "SM5 emit extended_opcodes: sample_controls are only valid on sample/gather4-family opcodes";
          return std::unexpected(error);
        }
      }
    }
    // Typed entries must form the canonical chain: at most one of each type,
    // in canonical order (sample_controls, resource_dim, resource_return_type).
    // Anything else would serialize a non-canonical chain.
    {
      int seen_types = 0;
      int last_rank = -1;
      for (const auto& ext : tpl.extended_opcodes) {
        if (ext.kind != ApplyRuleStep::EmitExtendedOpcode::Kind::Type) {
          continue;
        }
        int rank = -1;
        if (ext.type == ExtendedOpcodeType::SampleControls) {
          rank = 0;
        } else if (ext.type == ExtendedOpcodeType::ResourceDim) {
          rank = 1;
        } else if (ext.type == ExtendedOpcodeType::ResourceType) {
          rank = 2;
        }
        if (rank < 0) {
          error = "SM5 emit extended_opcodes: unsupported extended-opcode type";
          return std::unexpected(error);
        }
        const int kBit = 1 << rank;
        if ((seen_types & kBit) != 0) {
          error = "SM5 emit extended_opcodes: duplicate extended-opcode entry";
          return std::unexpected(error);
        }
        if (rank < last_rank) {
          error =
              "SM5 emit extended_opcodes: entries must be in canonical order "
              "(sample_controls, resource_dim, resource_return_type)";
          return std::unexpected(error);
        }
        seen_types |= kBit;
        last_rank = rank;
      }
    }
    rule.emit_patterns.push_back(std::move(tpl));
  }

  return rule;
}

auto ApplyRuleData::Compile() const -> std::expected<ApplyRuleStep, std::string> {
  auto cond = condition.Compile();
  auto compiled = this->rule.Compile();
  if (!compiled) {
    return std::unexpected(name + ": " + compiled.error());
  }
  ApplyRuleStep step{name, required, rewrite_mode, cond, std::move(*compiled), match_mode};
  int32_t resolved_insert = insert_index;
  if (rewrite_mode == RewriteKind::Before && resolved_insert < 0) {
    resolved_insert = 0;
  }
  step.insert_index = resolved_insert;
  step.range_start_offset = range_start_offset;
  step.range_end_offset = range_end_offset;
  return step;
}

std::expected<dxp::ApplyRuleResults, std::string> Execute(const ApplyRuleStep& step, ExecutionContext& ctx) {
  return ExecuteSingleRuleImpl(ctx.program, step.name, step.rule, step.match_mode, step.required, step.rewrite_mode, step.insert_index, step.range_start_offset, step.range_end_offset, ctx);
}

std::expected<void, std::string> Validate(const ApplyRuleStep& step, std::string& error, dxp::ValidationContext& ctx) {
  if (step.name.empty()) {
    error = "apply_rule step requires a name";
    return std::unexpected(std::move(error));
  }

  if (!ctx.names.insert(step.name).second) {
    error = "duplicate SM5 name '" + step.name + "' reused by step";
    return std::unexpected(std::move(error));
  }

  std::unordered_set<std::string> global_instruction_captures;
  std::unordered_set<std::string> global_operand_captures;
  std::unordered_set<std::string> global_index_captures;

  for (const auto& pattern : step.rule.match_patterns) {
    if (!pattern.capture.empty()) {
      global_instruction_captures.insert(pattern.capture);
    }
    for (const auto& op : pattern.operands) {
      if (!op.capture.empty()) {
        global_operand_captures.insert(op.capture);
      }
      for (const auto& idx : op.IndexPatterns()) {
        if (!idx.capture.empty()) {
          global_index_captures.insert(idx.capture);
        }
      }
    }
  }

  auto checkExportAs = [&](const std::vector<OperandPattern>& operands) -> std::expected<void, std::string> {
    for (const auto& op : operands) {
      if (op.export_as.has_value()) {
        if (!ctx.names.insert(*op.export_as).second) {
          error = "duplicate export_as key '" + *op.export_as + "' must be unique across all names";
          return std::unexpected(std::move(error));
        }
      }
    }
    return {};
  };
  for (const auto& pattern : step.rule.match_patterns) {
    if (auto r = checkExportAs(pattern.operands); !r) return r;
  }

  auto checkHandleRefs = [&](const std::vector<OperandPattern>& operands) -> std::expected<void, std::string> {
    for (const auto& op : operands) {
      if (op.handle && !ctx.handles.contains(op.handle->name)) {
        error = "unknown resource declaration handle '" + op.handle->name + "'";
        return std::unexpected(std::move(error));
      }
    }
    return {};
  };
  for (const auto& pattern : step.rule.match_patterns) {
    if (auto r = checkHandleRefs(pattern.operands); !r) return r;
  }
  for (const auto& emit : step.rule.emit_patterns) {
    if (auto r = checkHandleRefs(emit.operands); !r) return r;
  }

  for (const auto& emit : step.rule.emit_patterns) {
    if (!emit.capture.empty() && !emit.opcode.has_value()) {
      if (!global_instruction_captures.contains(emit.capture)) {
        error = "SM5 emit instruction capture reference '" + emit.capture + "' not found in match captures";
        return std::unexpected(std::move(error));
      }
    }
  }

  if ((step.rewrite_mode == RewriteKind::Before || step.rewrite_mode == RewriteKind::After) && step.insert_index < 0) {
    error = "SM5 before/after rewrites require insert_index >= 0";
    return std::unexpected(std::move(error));
  }

  if (step.rewrite_mode != RewriteKind::None) {
    if (step.rule.emit_patterns.empty()) {
      error = "SM5 rules without emit must use rewrite_mode: None";
      return std::unexpected(std::move(error));
    }
  }

  auto validateInterpolation = [&](const std::string& context) -> std::expected<void, std::string> {
    for (const auto& pattern : step.rule.match_patterns) {
      if (pattern.interpolation_mode.has_value()) {
        if (pattern.opcode.has_value()) {
          if (*pattern.opcode != Opcode::DclInputPs && *pattern.opcode != Opcode::DclInputPsSiv) {
            error = "interpolation is only valid for dcl_input_ps and dcl_input_ps_siv (in " + context + ")";
            return std::unexpected(std::move(error));
          }
        }
      }
    }
    for (const auto& emit : step.rule.emit_patterns) {
      if (emit.interpolation_mode.has_value()) {
        if (emit.opcode.has_value()) {
          if (*emit.opcode != Opcode::DclInputPs && *emit.opcode != Opcode::DclInputPsSiv) {
            error = "interpolation is only valid for dcl_input_ps and dcl_input_ps_siv (in " + context + ")";
            return std::unexpected(std::move(error));
          }
        }
      }
    }
    return {};
  };
  if (auto r = validateInterpolation("rule '" + step.name + "'"); !r) return r;

  if (step.rule.match_patterns.empty()) {
    error = "SM5 rules require at least one match instruction pattern";
    return std::unexpected(std::move(error));
  }

  auto validateIndexPatterns = [&](const std::vector<OperandIndexPattern>& indices, const std::string& path) -> std::expected<void, std::string> {
    for (size_t i = 0; i < indices.size(); ++i) {
      const std::string idxPath = path + ".indices[" + std::to_string(i) + "]";
      const auto& idx = indices[i];
      const bool kReprAllowsRelative = idx.representation == OperandIndexRepresentation::Relative || idx.representation == OperandIndexRepresentation::Immediate32PlusRelative || idx.representation == OperandIndexRepresentation::Immediate64PlusRelative;
      const bool kReprRequiresRelative = idx.representation == OperandIndexRepresentation::Immediate32PlusRelative || idx.representation == OperandIndexRepresentation::Immediate64PlusRelative;
      if (idx.relative_operand) {
        if (idx.any) {
          error = idxPath + ": relative_operand is incompatible with any: true";
          return std::unexpected(std::move(error));
        }
        if (!kReprAllowsRelative) {
          error = idxPath + ": relative_operand requires representation relative, immediate32_plus_relative, or immediate64_plus_relative";
          return std::unexpected(std::move(error));
        }
        // SM5 supports at most one nested relative operand — reject deeper nesting.
        for (const auto& nested_idx : (**idx.relative_operand).IndexPatterns()) {
          if (nested_idx.relative_operand) {
            error = idxPath + ".relative_operand: SM5 relative operands support at most one nesting level";
            return std::unexpected(std::move(error));
          }
        }
      } else if (kReprRequiresRelative) {
        error = idxPath + ": immediate32_plus_relative/immediate64_plus_relative requires relative_operand";
        return std::unexpected(std::move(error));
      }
    }
    return {};
  };
  auto validateIndexForm = [&](const OperandPattern& op, const std::string& path) -> std::expected<void, std::string> {
    const bool has_typed = !op.immediates_u32.empty() || !op.immediates_u64.empty() || !op.immediates_i32.empty() || !op.immediates_i64.empty() || !op.immediates_f32.empty() || !op.immediates_f64.empty();
    if (!op.indices.empty() && has_typed) {
      error = path + ": cannot use both explicit indices and typed immediates (immediates_u32/etc.)";
      return std::unexpected(std::move(error));
    }
    return {};
  };
  for (size_t pi = 0; pi < step.rule.match_patterns.size(); ++pi) {
    const std::string matchPath = "rule.match_patterns[" + std::to_string(pi) + "]";
    for (size_t oi = 0; oi < step.rule.match_patterns[pi].operands.size(); ++oi) {
      const std::string opPath = matchPath + ".operands[" + std::to_string(oi) + "]";
      if (auto r = validateIndexForm(step.rule.match_patterns[pi].operands[oi], opPath); !r) return r;
      if (auto r = validateIndexPatterns(step.rule.match_patterns[pi].operands[oi].IndexPatterns(), opPath); !r) return r;
    }
  }
  for (size_t ei = 0; ei < step.rule.emit_patterns.size(); ++ei) {
    const std::string emitPath = "rule.emit_patterns[" + std::to_string(ei) + "]";
    for (size_t oi = 0; oi < step.rule.emit_patterns[ei].operands.size(); ++oi) {
      const std::string opPath = emitPath + ".operands[" + std::to_string(oi) + "]";
      if (auto r = validateIndexForm(step.rule.emit_patterns[ei].operands[oi], opPath); !r) return r;
      if (auto r = validateIndexPatterns(step.rule.emit_patterns[ei].operands[oi].IndexPatterns(), opPath); !r) return r;
    }
  }

  // --- Emit operand completeness + opcode layout validation ---
  auto validateEmitOperand = [&](const OperandPattern& op, const std::string& path) -> std::expected<void, std::string> {
    if (!op.match_capture.empty()) {
      error = path + ": match_capture is match-only; use capture to replay a captured operand in emit";
      return std::unexpected(std::move(error));
    }
    if (op.any) {
      error = path + ": any is not valid in emit operands";
      return std::unexpected(std::move(error));
    }
    if (!op.mask.empty()) {
      if (op.mask.size() > 4) {
        error = path + ": mask value '" + op.mask + "' has more than 4 components";
        return std::unexpected(std::move(error));
      }
      for (const char c : op.mask) {
        if (ComponentIndex(c) < 0) {
          error = path + ": mask value '" + op.mask + "' contains invalid component (expected xyzw)";
          return std::unexpected(std::move(error));
        }
      }
    }
    if (!op.swizzle.empty()) {
      if (op.swizzle.size() != 4) {
        error = path + ": swizzle value '" + op.swizzle + "' must have exactly 4 components (e.g. xyzw)";
        return std::unexpected(std::move(error));
      }
      for (const char c : op.swizzle) {
        if (ComponentIndex(c) < 0) {
          error = path + ": swizzle value '" + op.swizzle + "' contains invalid component (expected xyzw)";
          return std::unexpected(std::move(error));
        }
      }
    }
    if (!op.select.empty()) {
      if (op.select.size() != 1 || ComponentIndex(op.select.front()) < 0) {
        error = path + ": select value '" + op.select + "' must be a single component (x, y, z, or w)";
        return std::unexpected(std::move(error));
      }
    }
    const bool is_immediate =
        op.type.has_value() && (*op.type == OperandType::Immediate32 || *op.type == OperandType::Immediate64);
    // Sampler operands carry no component selection in DXBC (e.g. the sampler
    // operand of sample_l is a bare s#); their encoding is derived.
    const bool is_sampler = op.type.has_value() && *op.type == OperandType::Sampler;
    if (PatternComponentMode(op).has_value() == false && op.capture.empty() && !is_immediate && !is_sampler) {
      error = path + ": emit operand has no component selection; specify components: or capture: a previously matched operand";
      return std::unexpected(std::move(error));
    }
    if (is_immediate) {
      if (op.num_components >= 0) {
        error = path + ": immediate operands derive num_components from the immediates count";
        return std::unexpected(std::move(error));
      }
      // Count values from either form: typed immediates arrays (immediates_u32 etc.)
      // or the manual indices: form — IndexPatterns() resolves both.
      const size_t value_count = op.IndexPatterns().size();
      if (value_count != 1 && value_count != 4) {
        error = path + ": immediate operands must carry exactly 1 or 4 values (got " + std::to_string(value_count) + ")";
        return std::unexpected(std::move(error));
      }
    }
    return {};
  };
  for (size_t ei = 0; ei < step.rule.emit_patterns.size(); ++ei) {
    const auto& emit = step.rule.emit_patterns[ei];
    if (!emit.opcode.has_value()) continue;
    const uint32_t kExpectedOperands = GetExpectedOperandCount(*emit.opcode);
    if (kExpectedOperands > 0 && emit.operands.size() != kExpectedOperands) {
      error = "rule.emit_patterns[" + std::to_string(ei) + "]: opcode " + std::to_string(static_cast<uint32_t>(*emit.opcode)) + " expects " + std::to_string(kExpectedOperands) + " operands, recipe provides " + std::to_string(emit.operands.size());
      return std::unexpected(std::move(error));
    }
    for (size_t oi = 0; oi < emit.operands.size(); ++oi) {
      const OperandPattern& op = emit.operands[oi];
      const std::string opPath = "rule.emit_patterns[" + std::to_string(ei) + "].operands[" + std::to_string(oi) + "]";
      if (auto r = validateEmitOperand(op, opPath); !r) return r;
      const OperandRole kRole = GetOperandRole(*emit.opcode, oi);
      if (kRole == OperandRole::Destination && (!op.swizzle.empty() || !op.select.empty())) {
        error = opPath + ": destination operand must use mask selection mode (not swizzle/select)";
        return std::unexpected(std::move(error));
      }
      const OperandScalarType kExpectedType = GetExpectedOperandType(*emit.opcode, oi);
      if (op.type.has_value()) {
        const bool kTypeOk =
            kExpectedType == OperandScalarType::Unknown || (kExpectedType == OperandScalarType::Texture && *op.type == OperandType::Resource) || (kExpectedType == OperandScalarType::Sampler && *op.type == OperandType::Sampler) || (kExpectedType == OperandScalarType::Uav && *op.type == OperandType::UAV) || (kExpectedType == OperandScalarType::CBuffer && *op.type == OperandType::CBuffer) || (kExpectedType == OperandScalarType::F32 || kExpectedType == OperandScalarType::U32 || kExpectedType == OperandScalarType::I32 || kExpectedType == OperandScalarType::F64 || kExpectedType == OperandScalarType::Bool);
        if (!kTypeOk) {
          error = opPath + ": operand type does not match the opcode's expected slot type (" + std::to_string(static_cast<uint32_t>(kExpectedType)) + ")";
          return std::unexpected(std::move(error));
        }
      }
    }
  }

  // --- match_capture name resolution (match side only) ---
  auto knownOperandCapture = [&](const std::string& name) {
    return global_operand_captures.contains(name) || ctx.operand_captures.contains(name);
  };
  auto knownIndexCapture = [&](const std::string& name) {
    return global_index_captures.contains(name) || ctx.index_captures.contains(name);
  };
  for (size_t pi = 0; pi < step.rule.match_patterns.size(); ++pi) {
    for (size_t oi = 0; oi < step.rule.match_patterns[pi].operands.size(); ++oi) {
      const auto& op = step.rule.match_patterns[pi].operands[oi];
      const std::string opPath = "rule.match_patterns[" + std::to_string(pi) + "].operands[" + std::to_string(oi) + "]";
      if (!op.match_capture.empty() && !knownOperandCapture(op.match_capture)) {
        error = opPath + ": match_capture references unknown captured operand '" + op.match_capture + "'";
        return std::unexpected(std::move(error));
      }
      for (const auto& idx : op.IndexPatterns()) {
        if (!idx.match_capture.empty() && !knownIndexCapture(idx.match_capture)) {
          error = opPath + ": index match_capture references unknown captured index '" + idx.match_capture + "'";
          return std::unexpected(std::move(error));
        }
      }
    }
  }

  for (const auto& cap : global_instruction_captures) {
    ctx.instruction_captures.insert(cap);
  }
  for (const auto& cap : global_operand_captures) {
    ctx.operand_captures.insert(cap);
  }
  for (const auto& cap : global_index_captures) {
    ctx.index_captures.insert(cap);
  }

  if (auto r = ValidateCondition<typename std::decay_t<decltype(step)>::Results>(step.condition, ctx); !r) {
    error = r.error();
    return std::unexpected(error);
  }

  return {};
}

namespace {

void StampResourceAccessControls(ExecutionContext& context, Instruction& instr);

// Builds the emitted extended-opcode chain: explicit entries verbatim, then the
// canonical ResourceDim/ResourceReturnType pair completed from the declaration.
// Unresolvable declarations are a hard error (no silent bare emits).
bool BuildExtendedOpcodeChain(ExecutionContext& context, Instruction& instr,
                              const std::vector<ApplyRuleStep::EmitExtendedOpcode>& entries,
                              const std::string& path, std::string& error) {
  const auto kChain = RequiredExtendedChainForOpcode(instr.opcode);
  std::vector<uint32_t> tokens;
  tokens.reserve(entries.size() + 2);
  for (const auto& entry : entries) {
    if (entry.kind == ApplyRuleStep::EmitExtendedOpcode::Kind::Raw) {
      tokens.push_back(entry.raw & ~D3D10_SB_OPCODE_EXTENDED_MASK);
    } else {
      auto token = static_cast<uint32_t>(entry.type);
      if (entry.sample_controls.has_value()) {
        const auto& sc = *entry.sample_controls;
        token |= ENCODE_IMMEDIATE_D3D10_SB_ADDRESS_OFFSET(0, sc.u);
        token |= ENCODE_IMMEDIATE_D3D10_SB_ADDRESS_OFFSET(1, sc.v);
        token |= ENCODE_IMMEDIATE_D3D10_SB_ADDRESS_OFFSET(2, sc.w);
      } else if (entry.resource_dim.has_value()) {
        token |= ENCODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION(entry.resource_dim->dimension);
        token |= ENCODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION_STRUCTURE_STRIDE(entry.resource_dim->structure_stride);
      } else if (entry.resource_return_type.has_value()) {
        uint32_t component = 0;
        for (const uint32_t return_type : entry.resource_return_type->component_types) {
          token |= ENCODE_D3D11_SB_EXTENDED_RESOURCE_RETURN_TYPE(return_type, component);
          ++component;
        }
      }
      tokens.push_back(token);
    }
  }

  if (kChain.RequiresResourcePair()) {
    bool has_dim = false;
    bool has_return = false;
    size_t return_pos = tokens.size();
    for (size_t i = 0; i < tokens.size(); ++i) {
      const uint32_t type = tokens[i] & kExtendedOpcodeTypeMask;
      if (type == static_cast<uint32_t>(ExtendedOpcodeType::ResourceDim)) has_dim = true;
      if (type == static_cast<uint32_t>(ExtendedOpcodeType::ResourceType)) {
        has_return = true;
        return_pos = i;
      }
    }
    if (!has_dim || !has_return) {
      uint32_t dimension = 0;
      uint32_t packed_return = 0;
      if (kChain.HasFixedMetadata()) {
        dimension = kChain.fixed_dimension;
        packed_return = kChain.fixed_return_type;
      } else {
        StampResourceAccessControls(context, instr);
        if (instr.controls.resource_dimension == 0) {
          error = path
                  +
                  ": resource-access emit requires a declared resource to synthesize the canonical "
                  "ResourceDim/ResourceReturnType extended pair (no silent bare emit)";
          return false;
        }
        dimension = instr.controls.resource_dimension;
        packed_return = instr.controls.resource_return_type;
      }
      // Insert the missing members at their canonical positions (dim before
      // return), never after the return token.
      if (!has_dim) {
        auto token = static_cast<uint32_t>(ExtendedOpcodeType::ResourceDim);
        token |= ENCODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION(dimension);
        token |= ENCODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION_STRUCTURE_STRIDE(instr.controls.structure_stride);
        auto insert_at = tokens.begin();
        std::advance(insert_at, has_return ? return_pos : tokens.size());
        tokens.insert(insert_at, token);
        if (has_return) {
          ++return_pos;
        }
      }
      if (!has_return) {
        auto token = static_cast<uint32_t>(ExtendedOpcodeType::ResourceType);
        for (uint32_t component = 0; component < 4; ++component) {
          const uint32_t return_type = (packed_return >> (4 * component)) & 0xF;
          token |= ENCODE_D3D11_SB_EXTENDED_RESOURCE_RETURN_TYPE(
              return_type != 0 ? return_type : D3D10_SB_RETURN_TYPE_FLOAT, component);
        }
        tokens.push_back(token);
      }
      context.logger.Log(LogLevel::Warning,
                         "[Patch] synthesized ResourceDim/ResourceReturnType for emitted opcode "
                             + std::to_string(static_cast<uint32_t>(instr.opcode)) + " (" + path + ")");
    }
  }

  for (size_t i = 0; i < tokens.size(); ++i) {
    if (i + 1 < tokens.size()) {
      tokens[i] |= D3D10_SB_OPCODE_EXTENDED_MASK;
    }
  }
  instr.controls.extended_op_codes.clear();
  instr.controls.extended_op_codes.reserve(tokens.size());
  for (const uint32_t token : tokens) {
    instr.controls.extended_op_codes.emplace_back(token);
  }
  return true;
}

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
    if (!op.capture.empty()) {
      CapturedOperand cap;
      cap.operand_data = opnd;
      cap.role = GetOperandRole(instruction.opcode, i);
      if (i == 0 && !instruction.operands.empty()) cap.destination_mask = ExtractComponentMask(instruction.operands[0].component_mode, DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(instruction.operands[0].component_mode));
      cap.export_as = op.export_as;
      local_operands[op.capture] = std::move(cap);
    }
  }
}

auto CollectMatches(const ShaderProgram& program, const InstructionPattern& pattern, CaptureStore& captures, const ExecutionContext& context) -> std::vector<MatchResult> {
  std::vector<MatchResult> matches;
  matches.reserve(program.instructions.size());
  for (uint32_t i = 0; i < program.instructions.size(); ++i) {
    std::unordered_map<std::string, Operand::Index> captured_index_values;
    if (!MatchesInstruction(program.instructions[i], captured_index_values, pattern, context)) continue;
    MatchResult r;
    r.instruction_index = i;
    r.instruction = &program.instructions[i];
    r.range_start_index = i;
    r.range_end_index = i;
    CaptureOperands(program.instructions[i], pattern, r.operands, captures);
    r.index_values = std::move(captured_index_values);
    if (!pattern.capture.empty()) {
      r.instructions[pattern.capture] = program.instructions[i];
      Operand::Index idx;
      idx.immediate_lo = i;
      r.index_values[pattern.capture + "_index"] = std::move(idx);
    }
    matches.push_back(std::move(r));
  }
  return matches;
}

auto CollectSequenceMatches(const ShaderProgram& program, const std::vector<InstructionPattern>& patterns, CaptureStore& captures, const ExecutionContext& context) -> std::vector<MatchResult> {
  std::vector<MatchResult> matches;
  if (patterns.empty() || patterns.size() > program.instructions.size()) return matches;
  const auto limit = static_cast<uint32_t>(program.instructions.size() - patterns.size() + 1);
  for (uint32_t start = 0; start < limit; ++start) {
    std::unordered_map<std::string, Operand::Index> captured_index_values;
    bool ok = true;
    for (uint32_t pi = 0; pi < patterns.size(); ++pi) {
      if (!MatchesInstruction(program.instructions[start + pi], captured_index_values, patterns[pi], context)) {
        ok = false;
        break;
      }
    }
    if (!ok) continue;
    MatchResult r;
    r.instruction_index = start;
    r.instruction = &program.instructions[start];
    r.range_start_index = start;
    r.range_end_index = start + static_cast<uint32_t>(patterns.size() - 1);
    r.index_values = std::move(captured_index_values);
    for (uint32_t pi = 0; pi < patterns.size(); ++pi) {
      CaptureOperands(program.instructions[start + pi], patterns[pi], r.operands, captures);
      if (!patterns[pi].capture.empty()) {
        r.instructions[patterns[pi].capture] = program.instructions[start + pi];
        Operand::Index idx;
        idx.immediate_lo = start + pi;
        r.index_values[patterns[pi].capture + "_index"] = std::move(idx);
      }
    }
    matches.push_back(std::move(r));
  }
  return matches;
}

auto ResolveEmit(const MatchResult& match, ExecutionContext& context, const EmitPattern& emit, const std::string& path, std::string& error) -> Instruction {
  Instruction instr;
  if (!emit.capture.empty()) {
    auto it = context.captures.instructions.find(emit.capture);
    if (it == context.captures.instructions.end()) {
      error = path + ": unknown captured instruction '" + emit.capture + "'";
      return {};
    }
    const auto& cap = it->second;
    instr = cap.instruction_data;
    if (emit.opcode.has_value()) instr.opcode = *emit.opcode;
    if (emit.saturate.has_value()) instr.controls.saturate = *emit.saturate;
    if (emit.test_boolean >= 0) {
      instr.controls.test_boolean = static_cast<uint32_t>(emit.test_boolean);
    }
    if (emit.interpolation_mode.has_value()) {
      instr.controls.input_interpolation_mode = static_cast<uint32_t>(*emit.interpolation_mode);
    }
    for (size_t oi = 0; oi < emit.operands.size(); ++oi) {
      const std::string op = path + ".operands[" + std::to_string(oi) + "]";
      Operand ro = ResolveOperand(match, context, emit.operands[oi], op, error, oi);
      if (!error.empty()) return {};
      instr.operands.push_back(std::move(ro));
    }
    return ShaderProgram::FinalizeInstruction(std::move(instr));
  }
  instr.opcode = emit.opcode.has_value() ? *emit.opcode : OpcodeUnknown();
  instr.controls.saturate = emit.saturate.value_or(false);
  instr.controls.test_boolean.reset();
  if (emit.test_boolean >= 0) {
    instr.controls.test_boolean = 1U;
    instr.controls.test_boolean = static_cast<uint32_t>(emit.test_boolean);
  }
  if (emit.interpolation_mode.has_value()) {
    instr.controls.input_interpolation_mode = static_cast<uint32_t>(*emit.interpolation_mode);
  }
  instr.length_in_dwords = 0;
  for (size_t oi = 0; oi < emit.operands.size(); ++oi) {
    const std::string op = path + ".operands[" + std::to_string(oi) + "]";
    Operand ro = ResolveOperand(match, context, emit.operands[oi], op, error, oi);
    if (!error.empty()) return {};
    instr.operands.push_back(std::move(ro));
  }
  for (size_t oi = 0; oi < emit.operands.size(); ++oi) {
    const auto& op = emit.operands[oi];
    const bool needs_idx = op.type.has_value() && (op.type == OperandType::Resource || op.type == OperandType::CBuffer || op.type == OperandType::Sampler || op.type == OperandType::UAV || op.type == OperandType::Stream);
    if (instr.operands[oi].index_entries.empty() && op.capture.empty() && op.IndexPatterns().empty() && !op.handle && needs_idx) {
      error = path + ".operands[" + std::to_string(oi) + "]" + ": emit operand has no index_entries source";
      return {};
    }
  }
  if (!BuildExtendedOpcodeChain(context, instr, emit.extended_opcodes, path, error)) {
    return {};
  }
  return ShaderProgram::FinalizeInstruction(std::move(instr));
}

// Stamps resource dimension/return type from the DclResource declaration so the
// canonical extended pair can be synthesized. Parsed instructions are untouched.
void StampResourceAccessControls(ExecutionContext& context, Instruction& instr) {
  if (!instr.controls.extended_op_codes.empty() || !RequiredExtendedChainForOpcode(instr.opcode).RequiresResourcePair()) {
    return;
  }
  for (const auto& operand : instr.operands) {
    if (operand.type != OperandType::Resource || operand.index_entries.empty() || !operand.index_entries[0].immediate_lo.has_value()) {
      continue;
    }
    const uint32_t register_index = *operand.index_entries[0].immediate_lo;
    for (const auto& dcl : context.program.instructions) {
      if (dcl.opcode != Opcode::DclResource || dcl.operands.empty() || dcl.operands[0].type != OperandType::Resource
          || dcl.operands[0].index_entries.empty() || !dcl.operands[0].index_entries[0].immediate_lo.has_value()) {
        continue;
      }
      if (*dcl.operands[0].index_entries[0].immediate_lo == register_index) {
        instr.controls.resource_dimension = dcl.controls.resource_dimension;
        instr.controls.resource_return_type = dcl.controls.resource_return_type;
        return;
      }
    }
    break;
  }
}

auto MatchesInstruction(const Instruction& instr, std::unordered_map<std::string, Operand::Index>& captured_index_values, const InstructionPattern& pattern, const ExecutionContext& context) -> bool {
  if (pattern.opcode.has_value() && instr.opcode != *pattern.opcode) return false;
  if (pattern.saturate.has_value() && instr.controls.saturate != *pattern.saturate) return false;
  if (pattern.test_boolean >= 0 && instr.controls.test_boolean != static_cast<uint32_t>(pattern.test_boolean)) return false;
  if (pattern.interpolation_mode.has_value()) {
    if (instr.opcode != static_cast<uint32_t>(Opcode::DclInputPs) && instr.opcode != static_cast<uint32_t>(Opcode::DclInputPsSiv)) return false;
    if (instr.controls.input_interpolation_mode.has_value() && *instr.controls.input_interpolation_mode != static_cast<uint32_t>(*pattern.interpolation_mode)) return false;
  }
  // Extended-opcode expectations: absent = wildcard (any chain, including
  // none); present = exact full-chain match (count + per-entry rules).
  if (pattern.extended_opcodes.has_value()) {
    const auto& pattern_ext = *pattern.extended_opcodes;
    if (pattern_ext.size() != instr.controls.extended_op_codes.size()) return false;
    for (size_t i = 0; i < pattern_ext.size(); ++i) {
      const auto& pattern_val = pattern_ext[i];
      const auto& instr_val = instr.controls.extended_op_codes[i];
      switch (pattern_val.kind) {
        case ApplyRuleStep::ExtendedOpcodePattern::Kind::Any:
          break;
        case ApplyRuleStep::ExtendedOpcodePattern::Kind::Raw:
          if (pattern_val.raw != instr_val.value) return false;
          break;
        case ApplyRuleStep::ExtendedOpcodePattern::Kind::Type: {
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

auto ResolveOperand(const MatchResult& match, ExecutionContext& context, const OperandPattern& op, const std::string& path, std::string& error, size_t emit_operand_index) -> Operand {
  Operand operand;
  const CapturedOperand* co = nullptr;
  OperandRole cr = OperandRole::Source;
  if (!op.capture.empty()) {
    // Prefer the current match's local captures (per-match correctness for
    // match_all rewrites); fall back to the cross-step global store.
    auto lit = match.operands.find(op.capture);
    if (lit != match.operands.end()) {
      co = &lit->second;
    } else {
      auto cit = context.captures.operands.find(op.capture);
      if (cit == context.captures.operands.end()) {
        error = path + ": missing captured operand '" + op.capture + "'";
        return {};
      }
      co = &cit->second;
    }
    cr = co->role;
    operand = co->operand_data;
    if (op.type.has_value()) operand.type = *op.type;
    if (op.modifier.has_value()) operand.modifier = *op.modifier;
  }
  if (co != nullptr && match.instruction != nullptr) {
    const OperandRole er = (emit_operand_index == 0) ? OperandRole::Destination : OperandRole::Source;
    if (!ValidateOperandRole(co->operand_data, cr, path, context, error)) return {};
    if (cr != er) operand = co->ResolveForRole(er);
    if (!ValidateOperandRole(operand, er, path, context, error)) return {};
  }
  if (op.capture.empty()) {
    if (op.type.has_value()) operand.type = *op.type;
    if (op.modifier.has_value()) operand.modifier = *op.modifier;
  }
  if (op.capture.empty() && !op.IndexPatterns().empty()) {
    const auto& patterns = op.IndexPatterns();
    operand.index_entries.clear();
    for (size_t i = 0; i < patterns.size(); ++i) {
      const std::string ip = path + ".indices[" + std::to_string(i) + "]";
      Operand::Index idx = ResolveOperandIndex(match, context, patterns[i], ip, error);
      if (!error.empty()) return {};
      operand.index_entries.push_back(std::move(idx));
    }
  }
  if (op.handle) {
    const auto lookup = [&](BindingKind kind) -> const uint32_t* {
      auto& m = context.Bindings(kind);
      auto it = m.find(op.handle->name);
      return it != m.end() ? &it->second : nullptr;
    };
    const uint32_t* rbp = nullptr;
    if (!op.type.has_value()) {
      error = path + ": SM5 handle operand type is unsupported for resource binding";
      return {};
    }
    switch (*op.type) {
      case OperandType::Temp:   rbp = lookup(BindingKind::Temp); break;
      case OperandType::Input:  rbp = lookup(BindingKind::Input); break;
      case OperandType::Output: rbp = lookup(BindingKind::Output); break;
      case OperandType::Resource:
        rbp = lookup(BindingKind::Texture);
        if (rbp == nullptr) rbp = lookup(BindingKind::RawResource);
        if (rbp == nullptr) rbp = lookup(BindingKind::StructuredResource);
        break;
      case OperandType::Sampler: rbp = lookup(BindingKind::Sampler); break;
      case OperandType::CBuffer: rbp = lookup(BindingKind::CBuffer); break;
      case OperandType::UAV:     rbp = lookup(BindingKind::Uav); break;
      default:
        error = path + ": SM5 handle operand type is unsupported for resource binding";
        return {};
    }
    if (rbp == nullptr) {
      error = path + ": missing SM5 declaration handle binding '" + op.handle->name + "'";
      return {};
    }
    Operand::Index operand_index;
    operand_index.representation = Operand::IndexRepresentation::Immediate32;
    operand_index.immediate_lo = *rbp;
    operand.index_entries.push_back(std::move(operand_index));
    if (op.handle->element_index.has_value()) {
      uint32_t resolved_element = 0;
      const auto& elem_idx = *op.handle->element_index;
      if (std::holds_alternative<std::string>(elem_idx)) {
        if (const auto* var = context.FindVariable(std::get<std::string>(elem_idx))) {
          std::visit(
              [&resolved_element](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
                  resolved_element = static_cast<uint32_t>(v);
                }
              },
              *var);
        }
      } else {
        resolved_element = std::get<uint32_t>(elem_idx);
      }
      Operand::Index element_index;
      element_index.representation = Operand::IndexRepresentation::Immediate32;
      element_index.immediate_lo = resolved_element;
      operand.index_entries.push_back(std::move(element_index));
    } else if (*op.type == OperandType::CBuffer) {
      Operand::Index element_index;
      element_index.representation = Operand::IndexRepresentation::Immediate32;
      element_index.immediate_lo = 0U;
      operand.index_entries.push_back(std::move(element_index));
    }
  }
  // Component selection: an explicit recipe spec overrides; a capture replay
  // inherits the captured operand's ground-truth component mode; immediates
  // follow the D3D convention (mask mode, no mask bits).
  if (const auto kSpecMode = PatternComponentMode(op)) {
    operand.component_mode = *kSpecMode;
  }
  if (op.num_components >= 0) {
    operand.components.num_components = static_cast<NumComponents>(op.num_components);
  }
  if (op.type.has_value() && (*op.type == OperandType::Immediate32 || *op.type == OperandType::Immediate64)) {
    // Immediates carry no component selection; the component count derives from
    // the value list (1 value -> One, 4 -> Four), matching the D3D convention.
    operand.component_mode = 0;
    operand.components.num_components =
        (operand.index_entries.size() == 1) ? NumComponents::One : NumComponents::Four;
  } else if (op.type.has_value() && *op.type == OperandType::Sampler && !PatternComponentMode(op)) {
    // Sampler operands carry no component selection in DXBC (bare s# token).
    operand.component_mode = 0;
    operand.components.num_components = NumComponents::Zero;
  }
  return operand;
}

auto MatchesOperand(const Operand& operand, std::unordered_map<std::string, Operand::Index>& captured_index_values, const OperandPattern& op, const ExecutionContext& context) -> bool {
  if (op.any) return true;
  if (op.type.has_value() && operand.type != *op.type) return false;
  if (op.num_components >= 0 && static_cast<uint32_t>(operand.components.num_components) != static_cast<uint32_t>(op.num_components)) return false;
  if (const auto kExpectedMode = PatternComponentMode(op)) {
    if (operand.component_mode != *kExpectedMode) return false;
  }
  if (op.modifier.has_value() && operand.modifier != *op.modifier) return false;
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

auto ResolveOperandIndex(const MatchResult& match, ExecutionContext& context, const OperandIndexPattern& pattern, const std::string& path, std::string& error) -> Operand::Index {
  Operand::Index idx;
  switch (pattern.representation) {
    case OperandIndexRepresentation::Immediate32:             idx.representation = Operand::IndexRepresentation::Immediate32; break;
    case OperandIndexRepresentation::Immediate64:             idx.representation = Operand::IndexRepresentation::Immediate64; break;
    case OperandIndexRepresentation::Relative:                idx.representation = Operand::IndexRepresentation::Relative; break;
    case OperandIndexRepresentation::Immediate32PlusRelative: idx.representation = Operand::IndexRepresentation::Immediate32PlusRelative; break;
    case OperandIndexRepresentation::Immediate64PlusRelative: idx.representation = Operand::IndexRepresentation::Immediate64PlusRelative; break;
  }
  idx.immediate_lo = pattern.immediate_lo;
  idx.immediate_hi = pattern.immediate_hi;
  // capture: emit a previously captured index value (from a match's index capture).
  if (!pattern.capture.empty()) {
    auto it = context.captures.index_values.find(pattern.capture);
    if (it == context.captures.index_values.end()) {
      error = path + ": missing captured operand index '" + pattern.capture + "'";
      return {};
    }
    idx = it->second;
  }
  if (pattern.relative_operand) {
    Operand ro = ResolveOperand(match, context, **pattern.relative_operand, path + ".relative_operand", error, 0);
    if (!error.empty()) return {};
    idx.relative_operand = xyz::indirect<Operand>(std::move(ro));
  }
  return idx;
}

auto MatchesOperandIndex(const Operand::Index& idx, const std::unordered_map<std::string, Operand::Index>& captured_index_values, const OperandIndexPattern& pattern, const ExecutionContext& context) -> bool {
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

bool ValidateOperandRole(const Operand& operand, OperandRole expected_role, const std::string& path, [[maybe_unused]] ExecutionContext& context, std::string& error) {
  if (operand.components.num_components != NumComponents::Four) return true;
  const uint32_t sm = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(operand.component_mode);
  if (expected_role == OperandRole::Destination && sm != static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)) {
    error = path + ": destination operand uses non-mask selection mode";
    return false;
  }
  if (sm == static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)) {
    for (int i = 0; i < 4; ++i) {
      if (DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_SOURCE(operand.component_mode, i) > 3) {
        error = path + ": operand swizzle selector out of range";
        return false;
      }
    }
  }
  if (expected_role == OperandRole::Destination && operand.type == OperandType::UAV) {
    error = path + ": UAV cannot be used as destination operand";
    return false;
  }
  return true;
}

template <typename TD, typename TS>
auto BitCastValue(TS v) -> TD {
  static_assert(sizeof(TD) == sizeof(TS), "size mismatch");
  TD r{};
  std::memcpy(&r, &v, sizeof(r));
  return r;
}

bool ResolveImmediateFromVariable(const std::string& path, const std::string& vn, const ExecutionContext& ctx, ImmediateFamily family, uint32_t& ol, uint32_t& oh, bool& hh, std::string& error) {
  const auto* v = ctx.FindVariable(vn);
  if (v == nullptr) {
    error = path + ": missing variable '" + vn + "'";
    return false;
  }
  hh = false;

  // The typed immediates array declares the target type (immediates_u32 -> 32-bit int, etc.);
  // the variable's value is converted to that target, or the resolution fails clearly.
  auto fail = [&]() {
    error = path + ": variable '" + vn + "' type does not match its typed immediates array";
    return false;
  };
  auto visit_primitive = [&](const auto& pv) -> bool {
    using T = std::decay_t<decltype(pv)>;
    if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t> || std::is_same_v<T, double>) {
      switch (family) {
        case ImmediateFamily::U32:
        case ImmediateFamily::I32: {
          if constexpr (std::is_same_v<T, bool>) {
            ol = pv ? 1U : 0U;
            return true;
          } else if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t>) {
            ol = BitCastValue<uint32_t>(pv);
            return true;
          } else {
            return fail();
          }
        }
        case ImmediateFamily::U64:
        case ImmediateFamily::I64: {
          if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>) {
            auto r = BitCastValue<uint64_t>(pv);
            ol = static_cast<uint32_t>(r & kU32Mask);
            oh = static_cast<uint32_t>(r >> kBitsPerDword);
            hh = true;
            return true;
          } else {
            return fail();
          }
        }
        case ImmediateFamily::F32: {
          if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
            ol = BitCastValue<uint32_t>(static_cast<float>(pv));
            return true;
          } else {
            return fail();
          }
        }
        case ImmediateFamily::F64: {
          if constexpr (std::is_same_v<T, double>) {
            auto r = BitCastValue<uint64_t>(pv);
            ol = static_cast<uint32_t>(r & kU32Mask);
            oh = static_cast<uint32_t>(r >> kBitsPerDword);
            hh = true;
            return true;
          } else {
            return fail();
          }
        }
        default:
          return fail();
      }
    }
    return fail();
  };

  return std::visit(visit_primitive, *v);
}

}  // namespace

std::string DescribeOutcome(const ApplyRuleStep& step, const dxp::ApplyRuleResults& results,
                            const ExecutionContext& /*ctx*/) {
  if (results.match_count == 0) {
    return "no match — nothing applied";
  }
  std::string message = std::format("matched {}", results.match_count);
  if (step.rewrite_mode != RewriteKind::None && results.applied_count > 0) {
    message += std::format(", applied {}", results.applied_count);
  }
  return message;
}

static_assert(RecipeStep<ApplyRuleStep>);
static_assert(ExecutableStep<ApplyRuleStep, ExecutionContext>);

}  // namespace dxp::sm5::step
