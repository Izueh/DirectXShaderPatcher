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
#include "dxp/sm5/step/common/Emit.hpp"
#include "dxp/sm5/step/common/Match.hpp"
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
using common::MatchContext;
using common::MatchResult;
using common::CollectMatches;
using common::CollectSequenceMatches;
using common::CollectWindowMatches;
using common::ExtractComponentMask;
using common::EmitContext;
using common::ResolveEmitEntry;
using common::StampResourceAccessControls;

namespace {

/// SM5 token/bit encoding constants.
constexpr uint32_t kBitsPerDword = 32U;
constexpr uint64_t kU32Mask = 0xFFFFFFFFULL;

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

// Match/rewrite targets are instruction vectors, not the whole ShaderProgram, so the
// same machinery can run scoped to a captured blob interior (blob steps) as well as
// the full program. Declaration lookups (resource stamps, handles) still resolve
// through ExecutionContext's parent program.
auto ExecuteSingleRuleImpl(std::vector<Instruction>& instructions, const std::string& step_name,
                           const Rule& rule_model, MatchKind mode,
                           bool required, RewriteKind rewrite_mode,
                           ExecutionContext& ctx) -> std::expected<dxp::ApplyRuleResults, std::string>;

bool ApplyRewriteActions(std::vector<Instruction>& instructions, const std::vector<RewriteAction>& actions) {
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

  size_t out_size = instructions.size();
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

  while (instr_idx < instructions.size()) {
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

    if (instr_idx < instructions.size() && (a_idx >= sorted_actions.size() || get_pos(*sorted_actions[a_idx]) != instr_idx)) {
      output.push_back(std::move(instructions[instr_idx]));
      ++instr_idx;
    }
  }

  while (a_idx < sorted_actions.size()) {
    output.insert(output.end(), sorted_actions[a_idx]->new_instructions.begin(), sorted_actions[a_idx]->new_instructions.end());
    ++a_idx;
  }

  instructions = std::move(output);
  return true;
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
                                 const MatchResult& match,
                                 const std::string& rewrite_path, ExecutionContext& ctx,
                                 std::vector<RewriteAction>& actions, std::string& error) -> bool {
  error.clear();
  actions.clear();
  RewriteAction action;
  if (!ResolveRangeReplacement(rewrite_mode, range_start_offset, range_end_offset, insert_index, match, rewrite_path, action, error)) {
    return false;
  }

  EmitContext emission{ctx.captures, ctx, ctx.bindings, ctx.templates, ctx.template_pool_base,
                       ctx.Declarations(), ctx.logger, &match.operands, match.instruction != nullptr};
  for (size_t emit_index = 0; emit_index < rule.emit_patterns.size(); ++emit_index) {
    const std::string kEmitPath = rewrite_path + ".emit_patterns[" + std::to_string(emit_index) + "]";
    const auto& emit = rule.emit_patterns[emit_index];

    if (!ResolveEmitEntry(emit, kEmitPath, action.new_instructions, error, emission)) {
      return false;
    }
  }

  actions.push_back(std::move(action));
  return true;
}

auto ExecuteSingleRuleImpl(std::vector<Instruction>& instructions, const std::string& step_name,
                           const Rule& rule_model, MatchKind mode,
                           bool required, RewriteKind rewrite_mode,
                           int32_t insert_index, int32_t range_start_offset, int32_t range_end_offset,
                           ExecutionContext& ctx) -> std::expected<dxp::ApplyRuleResults, std::string> {
  dxp::ApplyRuleResults result;

  const std::string& rule_name = step_name;

  // --- before_last_return: anchor comes from the program, not a match ---
  // Match patterns are optional; when present they act as a guard (no-match →
  // no-match outcome). When absent, matching is skipped entirely.
  if (rewrite_mode == RewriteKind::BeforeLastReturn) {
    std::vector<MatchResult> guard_matches;
    if (!rule_model.match_patterns.empty()) {
      if (rule_model.match_patterns.size() > 1) {
        guard_matches = CollectSequenceMatches(instructions, rule_model.match_patterns, MatchContext{ctx.captures, ctx.Declarations()});
      } else {
        guard_matches = CollectMatches(instructions, rule_model.match_patterns.front(), MatchContext{ctx.captures, ctx.Declarations()});
      }
    }
    if (!rule_model.match_patterns.empty() && guard_matches.empty()) {
      ctx.state[rule_name] = false;
      return result;
    }

    // Find the LAST ret/retc in the program
    int32_t last_return_index = -1;
    for (uint32_t i = instructions.size(); i-- > 0;) {
      if (instructions[i].opcode == Opcode::Ret || instructions[i].opcode == Opcode::RetC) {
        last_return_index = static_cast<int32_t>(i);
        break;
      }
    }
    if (last_return_index < 0) {
      ctx.state[rule_name] = false;
      return result;
    }

    // Build a synthetic window match so capture/emit resolution works normally
    MatchResult anchor_match;
    anchor_match.instruction_index = static_cast<uint32_t>(last_return_index);
    anchor_match.instruction = &instructions[static_cast<size_t>(last_return_index)];
    anchor_match.range_start_index = static_cast<uint32_t>(last_return_index);
    anchor_match.range_end_index = static_cast<uint32_t>(last_return_index);
    if (!rule_model.match_patterns.empty()) {
      const auto& guard = guard_matches.back();
      anchor_match.operands = guard.operands;
      anchor_match.instructions = guard.instructions;
      anchor_match.index_values = guard.index_values;
    }
    StoreCaptures(ctx, anchor_match);
    result.match_count = 1;

    RewriteAction action;
    action.type = RewriteActionType::InsertBefore;
    action.insert_position = static_cast<uint32_t>(last_return_index);
    std::string emit_error;
    EmitContext emission{ctx.captures, ctx, ctx.bindings, ctx.templates, ctx.template_pool_base,
                         ctx.Declarations(), ctx.logger, &anchor_match.operands, anchor_match.instruction != nullptr};
    for (size_t emit_index = 0; emit_index < rule_model.emit_patterns.size(); ++emit_index) {
      const std::string kEmitPath = "step[" + step_name + "].emit_patterns[" + std::to_string(emit_index) + "]";
      if (!ResolveEmitEntry(rule_model.emit_patterns[emit_index], kEmitPath, action.new_instructions, emit_error, emission)) {
        return std::unexpected(emit_error);
      }
    }

    if (!ApplyRewriteActions(instructions, {action})) {
      return std::unexpected("step[" + step_name + "]: failed to apply before_last_return action");
    }
    ctx.MarkProgramMutated();
    ++result.applied_count;
    ctx.state[rule_name] = true;
    return result;
  }

  std::vector<MatchResult> matches;
  if (rule_model.match_patterns.size() > 1) {
    matches = CollectSequenceMatches(instructions, rule_model.match_patterns, MatchContext{ctx.captures, ctx.Declarations()});
  } else {
    matches = CollectMatches(instructions, rule_model.match_patterns.front(), MatchContext{ctx.captures, ctx.Declarations()});
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
      if (cap.operand_data.type == OperandType::Resource || cap.operand_data.type == OperandType::Sampler || cap.operand_data.type == OperandType::CBuffer || cap.operand_data.type == OperandType::UAV) {
        dxp::ResourceUsage usage;
        usage.binding_class = cap.operand_data.type == OperandType::CBuffer   ? dxp::BindingClass::CBuffer
                              : cap.operand_data.type == OperandType::Sampler ? dxp::BindingClass::Sampler
                              : cap.operand_data.type == OperandType::UAV     ? dxp::BindingClass::Uav
                                                                              : dxp::BindingClass::Texture;
        usage.register_index = cap.operand_data.index_entries.empty() || !cap.operand_data.index_entries[0].immediate_lo.has_value() ? 0 : *cap.operand_data.index_entries[0].immediate_lo;
        if (cap.operand_data.type == OperandType::CBuffer) {
          usage.handle = "cbuffer";
        } else if (cap.operand_data.type == OperandType::Sampler) {
          usage.handle = "sampler";
        } else if (cap.operand_data.type == OperandType::UAV) {
          usage.handle = "uav";
        } else {
          usage.handle = "texture";
        }
        if (cap.role == OperandRole::Destination && (cap.destination_mask != 0u)) {
          usage.accessed_components = cap.destination_mask;
        } else {
          auto sel = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(cap.operand_data.component_mode);
          usage.accessed_components = ExtractComponentMask(cap.operand_data.component_mode, sel);
        }
        // Enrich from the declaration when the register resolves.
        if (cap.operand_data.type == OperandType::Resource || cap.operand_data.type == OperandType::UAV) {
          const ResourceDeclInfo* decl = ctx.Declarations().FindResourceDecl(cap.operand_data.type, usage.register_index);
          if (decl != nullptr) {
            usage.dimension = decl->dimension;
            usage.return_type = decl->return_types[0];
            if (decl->structure_stride != 0) {
              usage.structure_stride = decl->structure_stride;
            }
          }
        }
        ctx.resource_exports[export_key] = std::move(usage);
      } else if (cap.operand_data.type == OperandType::Input || cap.operand_data.type == OperandType::Output) {
        const auto& signatures = cap.operand_data.type == OperandType::Input ? ctx.Declarations().inputs : ctx.Declarations().outputs;
        if (cap.operand_data.index_entries.empty() || !cap.operand_data.index_entries[0].immediate_lo.has_value()) continue;
        const uint32_t register_index = *cap.operand_data.index_entries[0].immediate_lo;
        auto it = signatures.find(register_index);
        if (it == signatures.end()) continue;
        dxp::ResourceUsage usage;
        usage.binding_class = cap.operand_data.type == OperandType::Input ? dxp::BindingClass::Input : dxp::BindingClass::Output;
        usage.register_index = register_index;
        usage.handle = cap.operand_data.type == OperandType::Input ? "input" : "output";
        if (cap.role == OperandRole::Destination && (cap.destination_mask != 0u)) {
          usage.accessed_components = cap.destination_mask;
        } else {
          auto sel = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(cap.operand_data.component_mode);
          usage.accessed_components = ExtractComponentMask(cap.operand_data.component_mode, sel);
        }
        if (it->second.semantic.has_value()) {
          usage.semantic = *it->second.semantic;
        }
        if (it->second.interpolation.has_value()) {
          usage.interpolation = *it->second.interpolation;
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
    if (!EvaluateRuleRewriteCallback(rewrite_mode, rule_model, range_start_offset, range_end_offset, insert_index, step_name, required, match, kRewritePath, ctx,
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

  if (!ApplyRewriteActions(instructions, actions)) {
    return std::unexpected("step[" + step_name + "]: failed to apply rewrite action");
  }

  ctx.MarkProgramMutated();

  ctx.state[step_name] = true;
  return result;
}

/// @brief Runs one rule against a given instruction vector. Shared by the blob
/// interior path; result counts accumulate into the caller's result struct.
auto RunRuleOnVector(std::vector<Instruction>& target, const std::string& step_name, const Rule& rule,
                     MatchKind mode, bool required, RewriteKind rewrite_mode, int32_t insert_index,
                     int32_t range_start_offset, int32_t range_end_offset, ExecutionContext& ctx,
                     dxp::ApplyRuleResults& result) -> std::expected<void, std::string> {
  auto r = ExecuteSingleRuleImpl(target, step_name, rule, mode, required, rewrite_mode, insert_index,
                                 range_start_offset, range_end_offset, ctx);
  if (!r) {
    return std::unexpected(r.error());
  }
  result.match_count += r->match_count;
  result.applied_count += r->applied_count;
  return {};
}

/// @brief Applies a MatchBlob step: window capture, optional interior rule scoped
/// to the blob copy, then emit_blob disposition (none/replace/before/after).
/// The blob store always ends up holding the post-mutation copy.
auto ExecuteBlobStep(std::vector<Instruction>& program_instructions, const ApplyRuleStep& step,
                     ExecutionContext& ctx) -> std::expected<dxp::ApplyRuleResults, std::string> {
  dxp::ApplyRuleResults result;
  const auto& blob = *step.match_blob;
  const EmitBlob::Mode kDisposition = step.emit_blob.has_value() ? step.emit_blob->mode : EmitBlob::Mode::None;

  // 1. Window capture (match_mode selects among window matches)
  auto matches = CollectWindowMatches(program_instructions, blob.match_start, blob.match_end, MatchContext{ctx.captures, ctx.Declarations()});
  const bool kMatched = !matches.empty();
  ctx.state[step.name] = kMatched;
  result.match_count = static_cast<uint32_t>(matches.size());
  if (!kMatched) {
    return result;
  }

  const auto kSelected = SelectMatchIndices(matches, step.match_mode);

  // 2. Per selected window: copy slice by value, run interior rule, apply disposition
  std::vector<RewriteAction> actions;
  for (const uint32_t selected_index : kSelected) {
    const auto& match = matches[selected_index];
    StoreCaptures(ctx, match);

    CapturedBlob blob_copy;
    blob_copy.instructions.assign(program_instructions.begin() + static_cast<ptrdiff_t>(match.range_start_index),
                                  program_instructions.begin() + static_cast<ptrdiff_t>(match.range_end_index) + 1);

    // 2a. Interior rule (if any) — scoped to the blob copy, with per-rule modes
    if (!step.rule.match_patterns.empty()) {
      const auto& rule = step.rule;
      auto interior = RunRuleOnVector(blob_copy.instructions, step.name, rule, rule.match_mode, step.required,
                                      rule.rewrite_mode, rule.insert_index, rule.range_start_offset,
                                      rule.range_end_offset, ctx, result);
      if (!interior) {
        return std::unexpected("step[" + step.name + "] blob[" + blob.capture + "]: " + interior.error());
      }
    }

    // 2b. Disposition: how the transformed copy lands in the program
    switch (kDisposition) {
      case EmitBlob::Mode::None:
        break;
      case EmitBlob::Mode::Replace: {
        RewriteAction action;
        action.type = RewriteActionType::ReplaceRange;
        action.range_start = match.range_start_index;
        action.range_end = match.range_end_index;
        action.new_instructions = blob_copy.instructions;
        actions.push_back(std::move(action));
        break;
      }
      case EmitBlob::Mode::Before:
      case EmitBlob::Mode::After:  {
        RewriteAction action;
        action.type = RewriteActionType::InsertBefore;
        action.insert_position = (kDisposition == EmitBlob::Mode::Before) ? match.range_start_index
                                                                          : match.range_end_index + 1;
        action.new_instructions = blob_copy.instructions;
        actions.push_back(std::move(action));
        break;
      }
    }

    // 2c. Store always holds the post-mutation copy (same-step modify + save-for-later compose)
    ctx.captures.blobs[blob.capture] = std::move(blob_copy);
    ++result.applied_count;
  }

  if (!actions.empty()) {
    if (!ApplyRewriteActions(program_instructions, actions)) {
      return std::unexpected("step[" + step.name + "]: failed to apply emit_blob action");
    }
    ctx.MarkProgramMutated();
  }

  return result;
}

/// @brief Applies a scope step: run the step's rule against a stored blob.
auto ExecuteScopeStep(const ApplyRuleStep& step, ExecutionContext& ctx) -> std::expected<dxp::ApplyRuleResults, std::string> {
  auto it = ctx.captures.blobs.find(step.scope);
  if (it == ctx.captures.blobs.end()) {
    return std::unexpected("step[" + step.name + "]: scope references unknown blob '" + step.scope + "'");
  }
  CapturedBlob& blob = it->second;
  const auto& rule = step.rule;

  auto r = ExecuteSingleRuleImpl(blob.instructions, step.name, rule, rule.match_mode, step.required,
                                 rule.rewrite_mode, rule.insert_index, rule.range_start_offset,
                                 rule.range_end_offset, ctx);
  if (!r) {
    return std::unexpected("step[" + step.name + "] scope[" + step.scope + "]: " + r.error());
  }
  return r;
}

}  // namespace

/// @brief Compiles one match entry into its pattern. Shared by RuleData::Compile
/// (rule.match entries) and ApplyRuleData::Compile (match_blob window endpoints).
auto CompileMatchPattern(const InstructionMatchData& match_item) -> std::expected<InstructionPattern, std::string> {
  std::string error;

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
    auto converted = CompileOperandPattern(operand, false);
    if (!converted) {
      return std::unexpected(converted.error());
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
  return pattern;
}

auto RuleData::Compile() const -> std::expected<Rule, std::string> {
  std::string error;
  Rule rule{};

  const std::vector<InstructionMatchData>& effective_match = match;
  // Match patterns are optional when the rule is a scoped interior rule with an
  // emit-only rewrite (e.g. blob steps with no interior rewrite, or plain emits).
  // Compile() does not enforce a minimum here; callers that require a match
  // (plain shader-level rules) check `match` emptiness themselves.
  for (const auto& match_item : effective_match) {
    auto compiled = CompileMatchPattern(match_item);
    if (!compiled) {
      return std::unexpected(compiled.error());
    }
    rule.match_patterns.push_back(std::move(*compiled));
  }

  for (size_t emit_index = 0; emit_index < emit.size(); ++emit_index) {
    const auto& emit_entry = emit[emit_index];
    const int kSourceSpecCount = (emit_entry.opcode.has_value() ? 1 : 0) + (!emit_entry.capture.empty() ? 1 : 0) + (!emit_entry.blob.empty() ? 1 : 0) + (!emit_entry.template_name.empty() ? 1 : 0);
    if (kSourceSpecCount > 1) {
      error = "rule emit entry [" + std::to_string(emit_index) + "] cannot combine opcode, capture, blob, and template on the same entry";
      return std::unexpected(error);
    }
    if (kSourceSpecCount == 0) {
      error = "rule emit entry [" + std::to_string(emit_index) + "] requires one of opcode, capture, blob, or template";
      return std::unexpected(error);
    }
    const std::optional<RepeatConfig> entry_repeat = CompileRepeatConfig(emit_entry.repeat);
    if (entry_repeat.has_value()) {
      if (auto r = ValidateRepeatConfig(*entry_repeat, "rule emit entry [" + std::to_string(emit_index) + "]"); !r) {
        return std::unexpected(r.error());
      }
    }
    if (emit_entry.template_name.empty() && !emit_entry.params.empty()) {
      error = "rule emit entry [" + std::to_string(emit_index) + "] params are only valid on template: entries";
      return std::unexpected(error);
    }
    EmitPattern tpl;
    tpl.opcode = emit_entry.opcode;
    tpl.capture = emit_entry.capture;
    tpl.blob = emit_entry.blob;
    tpl.template_name = emit_entry.template_name;
    tpl.repeat = entry_repeat;
    tpl.params = emit_entry.params;
    tpl.params = emit_entry.params;
    if (emit_entry.saturate.has_value()) {
      tpl.saturate = *emit_entry.saturate;
    }
    tpl.interpolation_mode = emit_entry.interpolation;
    tpl.test_boolean = emit_entry.test_boolean;
    tpl.dimension = emit_entry.dimension;
    tpl.return_type = emit_entry.return_type;
    tpl.structure_stride = emit_entry.structure_stride;
    tpl.access_pattern = emit_entry.access_pattern;
    tpl.mode = emit_entry.mode;
    tpl.uav_flags = emit_entry.uav_flags;
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
      auto operand_pattern_opt = CompileOperandPattern(operand, true);
      if (!operand_pattern_opt) {
        return std::unexpected(operand_pattern_opt.error());
      }
      tpl.operands.push_back(std::move(*operand_pattern_opt));
    }
    for (const auto& ext : emit_entry.extended_opcodes) {
      EmitExtendedOpcode compiled;
      const int kSpecCount = (ext.type.has_value() ? 1 : 0) + (ext.raw.has_value() ? 1 : 0);
      if (kSpecCount != 1) {
        error = "SM5 emit extended_opcodes entries require exactly one of 'type' or 'raw'";
        return std::unexpected(error);
      }
      if (ext.raw.has_value()) {
        compiled.kind = EmitExtendedOpcode::Kind::Raw;
        compiled.raw = *ext.raw;
      } else {
        compiled.kind = EmitExtendedOpcode::Kind::Type;
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
    // Extended opcodes are meaningful on resource-access opcodes and declaration
    // opcodes (dcl_resource, dcl_constant_buffer, dcl_sampler). Everything else
    // must stay bare.
    if (!tpl.extended_opcodes.empty() && tpl.opcode.has_value()) {
      const auto kChain = RequiredExtendedChainForOpcode(*tpl.opcode);
      const bool kIsResourceAccess = kChain.RequiresResourcePair();
      const bool kIsDeclaration =
          *tpl.opcode == Opcode::DclResource || *tpl.opcode == Opcode::DclResourceRaw
          || *tpl.opcode == Opcode::DclResourceStructured
          || *tpl.opcode == Opcode::DclConstantBuffer || *tpl.opcode == Opcode::DclSampler;
      if (!kIsResourceAccess && !kIsDeclaration) {
        error = "SM5 emit extended_opcodes are only supported on resource-access opcodes (ld, sample, gather4 families, resinfo) and declaration opcodes (dcl_resource, dcl_constant_buffer, dcl_sampler)";
        return std::unexpected(error);
      }
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
        if (ext.kind != EmitExtendedOpcode::Kind::Type) {
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

  rule.match_mode = match_mode;
  rule.rewrite_mode = rewrite_mode;
  rule.insert_index = insert_index;
  rule.range_start_offset = range_start_offset;
  rule.range_end_offset = range_end_offset;

  return rule;
}

auto ApplyRuleData::Compile() const -> std::expected<ApplyRuleStep, std::string> {
  auto cond = condition.Compile();
  auto compiled = this->rule.Compile();
  if (!compiled) {
    return std::unexpected(name + ": " + compiled.error());
  }
  const bool kHasMatchBlob = match_blob.has_value();
  const bool kHasPlainMatch = !rule.match.empty();
  const bool kHasScope = !scope.empty();
  // A plain shader-level rule requires at least one match pattern; scoped/blob
  // rules may be emit-only (e.g. capture-only blob steps), and before_last_return
  // takes its anchor from the program (match optional as a guard).
  if (!kHasMatchBlob && !kHasScope && !kHasPlainMatch && rewrite_mode != RewriteKind::BeforeLastReturn) {
    return std::unexpected(name + ": rules require at least one match instruction pattern");
  }
  // match_blob (its rule.match is the interior rule) XOR scope
  if (kHasMatchBlob && kHasScope) {
    return std::unexpected(name + ": match_blob and scope are mutually exclusive — use exactly one");
  }
  if (emit_blob.has_value() && !kHasMatchBlob) {
    return std::unexpected(name + ": emit_blob is only valid alongside match_blob");
  }
  if (kHasMatchBlob && (rewrite_mode != RewriteKind::Replace || insert_index >= 0 || range_start_offset != 0 || range_end_offset != -1)) {
    return std::unexpected(name + ": match_blob steps use emit_blob for window disposition; step-level rewrite_mode/insert_index/range offsets are not applicable");
  }
  ApplyRuleStep step{name, required, rewrite_mode, cond, std::move(*compiled), match_mode};
  int32_t resolved_insert = insert_index;
  if (rewrite_mode == RewriteKind::Before && resolved_insert < 0) {
    resolved_insert = 0;
  }
  step.insert_index = resolved_insert;
  step.range_start_offset = range_start_offset;
  step.range_end_offset = range_end_offset;
  if (kHasMatchBlob) {
    // Window endpoints compile through the same pattern path as rule.match entries.
    auto start = CompileMatchPattern(match_blob->match_start);
    if (!start) {
      return std::unexpected(name + ": match_blob.match_start: " + start.error());
    }
    auto end = CompileMatchPattern(match_blob->match_end);
    if (!end) {
      return std::unexpected(name + ": match_blob.match_end: " + end.error());
    }
    MatchBlob blob;
    blob.match_start = std::move(*start);
    blob.match_end = std::move(*end);
    blob.capture = match_blob->capture;
    step.match_blob = std::move(blob);
  }
  if (emit_blob.has_value()) {
    step.emit_blob = EmitBlob{emit_blob->mode};
  }
  step.scope = scope;
  return step;
}

std::expected<dxp::ApplyRuleResults, std::string> Execute(const ApplyRuleStep& step, ExecutionContext& ctx) {
  if (step.match_blob.has_value()) {
    return ExecuteBlobStep(ctx.program.instructions, step, ctx);
  }
  if (!step.scope.empty()) {
    return ExecuteScopeStep(step, ctx);
  }
  return ExecuteSingleRuleImpl(ctx.program.instructions, step.name, step.rule, step.match_mode, step.required, step.rewrite_mode, step.insert_index, step.range_start_offset, step.range_end_offset, ctx);
}

std::expected<void, std::string> Validate(const ApplyRuleStep& step, dxp::ValidationContext& ctx) {
  if (step.name.empty()) {
    return std::unexpected("apply_rule step requires a name");
  }

  if (!ctx.names.insert(step.name).second) {
    return std::unexpected("duplicate SM5 name '" + step.name + "' reused by step");
  }

  // --- blob/scope structural rules ---
  const bool kHasMatchBlob = step.match_blob.has_value();
  const bool kHasPlainMatch = !step.rule.match_patterns.empty();
  const bool kHasScope = !step.scope.empty();
  // match_blob (its rule.match is the interior rule) XOR scope; a plain match
  // step is the fallback when neither is present.
  if (kHasMatchBlob && kHasScope) {
    return std::unexpected("step '" + step.name + "': match_blob and scope are mutually exclusive — use exactly one");
  }
  if (step.emit_blob.has_value() && !kHasMatchBlob) {
    return std::unexpected("step '" + step.name + "': emit_blob is only valid alongside match_blob");
  }
  if (kHasMatchBlob && (step.rewrite_mode != RewriteKind::Replace || step.insert_index >= 0 || step.range_start_offset != 0 || step.range_end_offset != -1)) {
    return std::unexpected("step '" + step.name + "': match_blob steps use emit_blob for window disposition; step-level rewrite_mode/insert_index/range offsets are not applicable");
  }
  if (kHasMatchBlob) {
    if (step.match_blob->capture.empty()) {
      return std::unexpected("step '" + step.name + "': match_blob requires a capture name");
    }
    if (!ctx.names.insert(step.match_blob->capture).second) {
      return std::unexpected("duplicate SM5 name '" + step.match_blob->capture + "' reused by match_blob capture");
    }
    ctx.blob_names.insert(step.match_blob->capture);
  }
  if (kHasScope) {
    if (step.rule.match_patterns.empty()) {
      return std::unexpected("step '" + step.name + "': scope steps require a rule with match patterns");
    }
    if (ctx.blob_names.find(step.scope) == ctx.blob_names.end()) {
      return std::unexpected("step '" + step.name + "': scope references unknown blob '" + step.scope + "'");
    }
  }
  if (step.rewrite_mode == RewriteKind::BeforeLastReturn && kHasMatchBlob) {
    return std::unexpected("step '" + step.name + "': before_last_return is not compatible with match_blob (use emit_blob for window disposition)");
  }

  // blob: emit references must resolve to a known blob (captured earlier in the recipe)
  auto checkBlobRefs = [&](const std::vector<EmitPattern>& emits) -> std::expected<void, std::string> {
    for (const auto& emit : emits) {
      if (!emit.blob.empty() && ctx.blob_names.find(emit.blob) == ctx.blob_names.end()) {
        return std::unexpected("step '" + step.name + "': emit blob reference '" + emit.blob + "' does not match any earlier match_blob capture");
      }
    }
    return {};
  };
  if (auto r = checkBlobRefs(step.rule.emit_patterns); !r) return r;

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
          return std::unexpected("duplicate export_as key '" + *op.export_as + "' must be unique across all names");
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
      if (!op.handle) {
        continue;
      }
      const bool is_temp_type = op.type == OperandType::Temp || op.type == OperandType::IndexableTemp;
      // Template temps are scoped to their template's expansion (bound only
      // during instantiation); referencing them by name outside a template
      // fails at runtime, so reject at validate time.
      if (is_temp_type && ctx.template_temp_names.contains(op.handle->name)) {
        return std::unexpected("template temp '" + op.handle->name + "' cannot be referenced outside a template instantiation (template temps are scoped to their template's expansion)");
      }
      // Handles are only meaningful for resource-binding operand types.
      // Skip temps (local/temporary register names, including forward
      // add_resource temp references) and cbuffer (existing shader cbuffers).
      if (!is_temp_type && op.type != OperandType::CBuffer && !ctx.handles.contains(op.handle->name)) {
        return std::unexpected("unknown resource declaration handle '" + op.handle->name + "'");
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
    if (emit.template_name.empty() && !emit.params.empty()) {
      return std::unexpected("step '" + step.name + "': params are only valid on template: entries");
    }
    if (!emit.template_name.empty() && !ctx.template_names.contains(emit.template_name)) {
      return std::unexpected("unknown template '" + emit.template_name + "' (referenced template must be declared by a preceding declare_template step)");
    }
    // Template params (declaration contract): every declared param must be
    // provided by the instantiating emit entry; each provided name must be an
    // add_resource temp name declared so far or a capture name producible by
    // a previous or current match step (registers resolved at expansion).
    if (!emit.template_name.empty()) {
      const auto param_it = ctx.template_params.find(emit.template_name);
      if (param_it != ctx.template_params.end()) {
        for (const auto& param_name : param_it->second) {
          if (!emit.params.contains(param_name)) {
            return std::unexpected("step '" + step.name + "': template '" + emit.template_name + "' param '" + param_name + "' is not provided by the instantiating emit entry");
          }
        }
      }
      if (!emit.params.empty()) {
        for (const auto& [param_name, provided] : emit.params) {
          const bool declared = param_it != ctx.template_params.end() && std::find(param_it->second.begin(), param_it->second.end(), param_name) != param_it->second.end();
          if (!declared) {
            return std::unexpected("step '" + step.name + "': template '" + emit.template_name + "' does not declare param '" + param_name + "'");
          }
          const bool is_temp = ctx.handles.contains(provided);
          const bool is_capture =
              ctx.operand_captures.contains(provided) || global_operand_captures.contains(provided)
              || ctx.instruction_captures.contains(provided) || global_instruction_captures.contains(provided)
              || ctx.index_captures.contains(provided) || global_index_captures.contains(provided);
          if (!is_temp && !is_capture) {
            return std::unexpected("step '" + step.name + "': template '" + emit.template_name + "' param '" + param_name + "' provided name '" + provided + "' is neither a known add_resource temp nor a known capture");
          }
        }
      }
    }
    // Enforce the template's required captures (registered at declare_template
    // Validate): each must be producible by a previous or current match step.
    // The cross-step global capture store persists at runtime; a known-but-not
    // produced capture fails at expansion with a named error.
    if (!emit.template_name.empty()) {
      const auto req_it = ctx.template_required_captures.find(emit.template_name);
      if (req_it != ctx.template_required_captures.end()) {
        for (const auto& cap : req_it->second) {
          const bool known =
              ctx.operand_captures.contains(cap) || global_operand_captures.contains(cap)
              || ctx.instruction_captures.contains(cap) || global_instruction_captures.contains(cap)
              || ctx.index_captures.contains(cap) || global_index_captures.contains(cap);
          if (!known) {
            return std::unexpected("step '" + step.name + "': template '" + emit.template_name + "' requires capture '" + cap + "' which no previous or current match step captures");
          }
        }
      }
    }
    if (!emit.capture.empty() && !emit.opcode.has_value()) {
      if (!global_instruction_captures.contains(emit.capture)) {
        return std::unexpected("SM5 emit instruction capture reference '" + emit.capture + "' not found in match captures");
      }
    }
  }

  if ((step.rewrite_mode == RewriteKind::Before || step.rewrite_mode == RewriteKind::After) && step.insert_index < 0 && !kHasMatchBlob && !kHasScope) {
    return std::unexpected("SM5 before/after rewrites require insert_index >= 0");
  }

  // Step-level mode checks only apply to plain shader-level steps; scoped/blob
  // steps carry their modes per-rule inside `rule`.
  if (!kHasMatchBlob && !kHasScope && step.rewrite_mode != RewriteKind::None) {
    if (step.rule.emit_patterns.empty()) {
      return std::unexpected("SM5 rules without emit must use rewrite_mode: None");
    }
  }

  auto validateInterpolation = [&](const std::string& context) -> std::expected<void, std::string> {
    for (const auto& pattern : step.rule.match_patterns) {
      if (pattern.interpolation_mode.has_value()) {
        if (pattern.opcode.has_value()) {
          if (*pattern.opcode != Opcode::DclInputPs && *pattern.opcode != Opcode::DclInputPsSiv && *pattern.opcode != Opcode::DclInputPsSgv) {
            return std::unexpected("interpolation is only valid for dcl_input_ps, dcl_input_ps_siv, and dcl_input_ps_sgv (in " + context + ")");
          }
        }
      }
    }
    for (const auto& emit : step.rule.emit_patterns) {
      if (emit.interpolation_mode.has_value()) {
        if (emit.opcode.has_value()) {
          if (*emit.opcode != Opcode::DclInputPs && *emit.opcode != Opcode::DclInputPsSiv && *emit.opcode != Opcode::DclInputPsSgv) {
            return std::unexpected("interpolation is only valid for dcl_input_ps, dcl_input_ps_siv, and dcl_input_ps_sgv (in " + context + ")");
          }
        }
      }
    }
    return {};
  };
  if (auto r = validateInterpolation("rule '" + step.name + "'"); !r) return r;

  if (step.rule.match_patterns.empty() && !kHasMatchBlob && !kHasScope && step.rewrite_mode != RewriteKind::BeforeLastReturn) {
    return std::unexpected("SM5 rules require at least one match instruction pattern");
  }

  auto validateIndexPatterns = [&](const std::vector<OperandIndexPattern>& indices, const std::string& path) -> std::expected<void, std::string> {
    for (size_t i = 0; i < indices.size(); ++i) {
      const std::string idxPath = path + ".indices[" + std::to_string(i) + "]";
      const auto& idx = indices[i];
      const bool kReprAllowsRelative = idx.representation == OperandIndexRepresentation::Relative || idx.representation == OperandIndexRepresentation::Immediate32PlusRelative || idx.representation == OperandIndexRepresentation::Immediate64PlusRelative;
      const bool kReprRequiresRelative = idx.representation == OperandIndexRepresentation::Immediate32PlusRelative || idx.representation == OperandIndexRepresentation::Immediate64PlusRelative;
      if (idx.relative_operand) {
        if (idx.any) {
          return std::unexpected(idxPath + ": relative_operand is incompatible with any: true");
        }
        if (!kReprAllowsRelative) {
          return std::unexpected(idxPath + ": relative_operand requires representation relative, immediate32_plus_relative, or immediate64_plus_relative");
        }
        // SM5 supports at most one nested relative operand — reject deeper nesting.
        for (const auto& nested_idx : (**idx.relative_operand).IndexPatterns()) {
          if (nested_idx.relative_operand) {
            return std::unexpected(idxPath + ".relative_operand: SM5 relative operands support at most one nesting level");
          }
        }
      } else if (kReprRequiresRelative) {
        return std::unexpected(idxPath + ": immediate32_plus_relative/immediate64_plus_relative requires relative_operand");
      }
    }
    return {};
  };
  auto validateIndexForm = [&](const OperandPattern& op, const std::string& path) -> std::expected<void, std::string> {
    const bool has_typed = !op.immediates_u32.empty() || !op.immediates_u64.empty() || !op.immediates_i32.empty() || !op.immediates_i64.empty() || !op.immediates_f32.empty() || !op.immediates_f64.empty();
    if (!op.indices.empty() && has_typed) {
      return std::unexpected(path + ": cannot use both explicit indices and typed immediates (immediates_u32/etc.)");
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

  // --- Emit operand completeness + opcode layout validation (shared with
  // template emits via ValidateEmitPatterns) ---
  if (auto r = ValidateEmitPatterns(step.rule.emit_patterns, "rule.emit_patterns"); !r) return r;

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
        return std::unexpected(opPath + ": match_capture references unknown captured operand '" + op.match_capture + "'");
      }
      for (const auto& idx : op.IndexPatterns()) {
        if (!idx.match_capture.empty() && !knownIndexCapture(idx.match_capture)) {
          return std::unexpected(opPath + ": index match_capture references unknown captured index '" + idx.match_capture + "'");
        }
      }
      if (op.decl.has_value()) {
        if (!op.decl->AnyFieldSet()) {
          return std::unexpected(opPath + ": decl constraint specifies no fields");
        }
        if (op.type.has_value()) {
          const OperandType kType = *op.type;
          const bool kResourceFields = op.decl->dimension.has_value() || op.decl->structure_stride.has_value()
                                       || std::any_of(op.decl->return_type.begin(), op.decl->return_type.end(),
                                                      [](const auto& t) { return t.has_value(); });
          const bool kResourceType = kType == OperandType::Resource || kType == OperandType::UAV;
          if (kResourceFields && !kResourceType) {
            return std::unexpected(opPath + ": decl dimension/return_type/structure_stride are only valid on resource and unordered_access_view operands");
          }
          if (op.decl->mode.has_value() && kType != OperandType::Sampler) {
            return std::unexpected(opPath + ": decl mode is only valid on sampler operands");
          }
          if (op.decl->access_pattern.has_value() && kType != OperandType::CBuffer) {
            return std::unexpected(opPath + ": decl access_pattern is only valid on constant_buffer operands");
          }
          if ((op.decl->semantic.has_value() || op.decl->interpolation.has_value())
              && kType != OperandType::Input && kType != OperandType::Output) {
            return std::unexpected(opPath + ": decl semantic/interpolation are only valid on input and output operands");
          }
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
    return std::unexpected(r.error());
  }

  return {};
}

void StampResourceAccessControls(ExecutionContext& context, Instruction& instr) {
  StampResourceAccessControls(context.Declarations(), instr);
}

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
