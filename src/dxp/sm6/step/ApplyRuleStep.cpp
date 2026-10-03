#include "dxp/sm6/step/ApplyRuleStep.hpp"
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/InstrTypes.h>
#include <cmath>
#include <format>
#include <type_traits>
#include <variant>
#include "dxc/DxilValidation/DxilValidation.h"
#include "dxp/Condition_impl.hpp"
#include "dxp/ExportTypes.hpp"
#include "dxp/ResultFieldTraits.hpp"
#include "dxp/sm6/step/ApplyRuleStep_impl.hpp"
#include "dxp/sm6/step/common/Emit.hpp"
#include "dxp/sm6/step/common/Match.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"
#include "llvm/Support/FileSystem.h"
#include "value_types/indirect.h"

#include <bit>

#include <any>
#include <expected>
#include <functional>
#include <memory>
#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResourceBase.h"
#include "dxc/DXIL/DxilResourceBinding.h"
#include "dxc/DXIL/DxilResourceProperties.h"
#include "dxp/sm6/ExecutionContext.hpp"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Use.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Regex.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dxp::sm6::step {
using common::MatchContext;
using common::MatchResult;
using common::CollectAllMatches;
using common::CollectSequenceMatches;
using common::EmitContext;
using common::EmitInstruction;
using common::EmitWritesOutput;
using common::LlvmTypeFor;
using common::MatchesTypePattern;
using common::ResolveConstantValues;
using common::ResolveExtractType;
using common::ResolveOpCode;
using common::ValidateEmitPattern;

namespace {

/// @brief Maps an LLVM scalar type to the shared ComponentType vocabulary.
/// Falls back to I32/F32 for unknown widths (bfloat16, exotic types) — best effort.
dxp::ComponentType ComponentTypeOf(llvm::Type* type) {
  if (type->isIntegerTy()) {
    switch (type->getIntegerBitWidth()) {
      case 1:  return dxp::ComponentType::I1;
      case 8:  return dxp::ComponentType::I8;
      case 16: return dxp::ComponentType::I16;
      case 32: return dxp::ComponentType::I32;
      case 64: return dxp::ComponentType::I64;
      default: return dxp::ComponentType::I32;
    }
  }
  if (type->isHalfTy()) return dxp::ComponentType::F16;
  if (type->isFloatTy()) return dxp::ComponentType::F32;
  if (type->isDoubleTy()) return dxp::ComponentType::F64;
  return dxp::ComponentType::F32;
}

auto TryGetImmediateValue(llvm::Value* value, std::optional<dxp::ComponentType> component_type,
                          dxp::ImmediateValue& immediate) -> bool {
  auto* constant = llvm::dyn_cast_or_null<llvm::Constant>(value);
  if (constant == nullptr) return false;
  const bool vector = constant->getType()->isVectorTy();
  const unsigned count = vector ? constant->getType()->getVectorNumElements() : 1;
  immediate.type = component_type.value_or(ComponentTypeOf(constant->getType()->getScalarType()));
  immediate.raw_values.reserve(count);
  for (unsigned i = 0; i < count; ++i) {
    auto* element = vector ? constant->getAggregateElement(i) : constant;
    if (auto* integer = llvm::dyn_cast_or_null<llvm::ConstantInt>(element)) {
      if (integer->getBitWidth() > std::numeric_limits<uint64_t>::digits) return false;
      immediate.raw_values.push_back(integer->getZExtValue());
    } else if (auto* floating = llvm::dyn_cast_or_null<llvm::ConstantFP>(element)) {
      const auto bits = floating->getValueAPF().bitcastToAPInt();
      if (bits.getBitWidth() > std::numeric_limits<uint64_t>::digits) return false;
      immediate.raw_values.push_back(bits.getZExtValue());
    } else {
      return false;
    }
  }
  return true;
}

auto ResolveInstructionAtOffset(llvm::Instruction* base, uint32_t offset, llvm::Instruction*& resolved) -> bool {
  resolved = nullptr;
  if (base == nullptr) return false;
  llvm::BasicBlock::iterator instruction_it(base);
  for (uint32_t step = 0; step < offset; ++step) {
    ++instruction_it;
    if (instruction_it == base->getParent()->end()) return false;
  }
  resolved = &*instruction_it;
  return true;
}

auto ResolveReplacementRange(RewriteKind rewrite_mode, int32_t range_start_offset, int32_t range_end_offset, llvm::Instruction* replacement_target,
                             llvm::Instruction*& range_start, llvm::Instruction*& range_end) -> bool {
  range_start = nullptr;
  range_end = nullptr;
  if (replacement_target == nullptr) return false;
  // Replace swaps the entire matched window; only ReplaceRange (custom
  // sub-range via offsets) reaches this function.
  if (rewrite_mode != RewriteKind::ReplaceRange) return false;
  if (range_start_offset < 0 || range_end_offset < -1) return false;
  const auto start_offset = static_cast<uint32_t>(range_start_offset);
  const uint32_t end_offset = range_end_offset < 0 ? start_offset : static_cast<uint32_t>(range_end_offset);
  if (start_offset > end_offset) return false;
  if (!ResolveInstructionAtOffset(replacement_target, start_offset, range_start)) return false;
  if (!ResolveInstructionAtOffset(replacement_target, end_offset, range_end)) return false;
  return true;
}

auto EraseInstructionRange(llvm::Instruction* range_start, llvm::Instruction* range_end,
                           llvm::Instruction* replacement_target) -> bool {
  std::vector<llvm::Instruction*> range_instructions;
  if (range_start != nullptr && range_end != nullptr && range_start->getParent() == range_end->getParent()) {
    bool found_start = false;
    for (llvm::Instruction& instruction : *range_start->getParent()) {
      if (&instruction == range_start) found_start = true;
      if (!found_start) continue;
      range_instructions.push_back(&instruction);
      if (&instruction == range_end) break;
    }
  }
  if (range_instructions.empty()) return false;
  for (llvm::Instruction* instruction : range_instructions) {
    if (instruction == replacement_target) {
      if (!instruction->use_empty()) return false;
      continue;
    }
    if (!instruction->use_empty()) return false;
  }
  for (auto* instruction : std::ranges::reverse_view(range_instructions)) {
    if (instruction->use_empty()) instruction->eraseFromParent();
  }
  return true;
}

/// @brief Resolves a capture name from the match's captures, falling back to the
/// cross-step global capture store (sm5-compatible cross-step captures).
llvm::Value* ResolveCapture(const MatchResult& match, sm6::ExecutionContext* ctx, const std::string& name) {
  auto it = match.captures.find(name);
  if (it != match.captures.end()) return it->second;
  if (ctx != nullptr) {
    auto git = ctx->captures.values.find(name);
    if (git != ctx->captures.values.end()) return git->second;
  }
  return nullptr;
}

std::expected<void, std::string> ApplyMatchRewrite(MatchResult& match, RewriteKind rewrite_mode, const Rule& rule, int32_t insert_index, int32_t range_start_offset, int32_t range_end_offset, llvm::IRBuilder<>& builder,
                                                        llvm::Module& module, hlsl::DxilModule& dxil_module,
                                                        sm6::ExecutionContext* ctx,
                                                        std::vector<llvm::Value*>* rule_emitted);

std::expected<void, std::string> ApplyDxilRewriteRules(llvm::Function& function, llvm::Module& module, hlsl::DxilModule& dxil_module,
                                                       const Rule& rule, MatchKind match_mode, RewriteKind rewrite_mode,
                                                       int32_t insert_index, int32_t range_start_offset, int32_t range_end_offset,
                                                       unsigned* applied_rule_count, unsigned* mutated_rule_count,
                                                       [[maybe_unused]] const std::unordered_map<std::string, llvm::Value*>& captures,
                                                       ::dxp::ApplyRuleResults* export_results,
                                                       sm6::ExecutionContext* ctx) {
  unsigned applied_count = 0;
  unsigned mutation_count = 0;
  const bool rule_mutating = rewrite_mode != RewriteKind::None;
  std::vector<MatchResult> matches;
  MatchContext match_context{ctx, &dxil_module, (ctx != nullptr) ? &ctx->captures.values : nullptr, {}};
  if (rule.match_patterns.size() > 1) {
    // Multiple match patterns form a consecutive (block-local) sequence, mirroring sm5.
    CollectSequenceMatches(function, rule.match_patterns, matches, match_context);
  } else {
    for (const auto& pattern : rule.match_patterns) {
      CollectAllMatches(function, pattern, matches, match_context);
    }
  }
  if (!match_context.error.empty()) return std::unexpected(match_context.error);
  if (matches.empty()) {
    if (applied_rule_count != nullptr) *applied_rule_count = 0;
    if (mutated_rule_count != nullptr) *mutated_rule_count = 0;
    return {};
  }

  if ((export_results != nullptr) && !rule.match_patterns.empty()) {
    const auto& pattern = rule.match_patterns.front();
    for (const auto& match : matches) {
      for (const auto& op_pattern : pattern.operand_patterns) {
        if (!op_pattern.export_as.has_value()) continue;
        const std::string& export_key = *op_pattern.export_as;
        if (ResolveCapture(match, ctx, op_pattern.capture_name) == nullptr) continue;
        auto effective_kind = op_pattern.kind.value_or(OperandKind::Call);
        if (effective_kind == OperandKind::Resource || op_pattern.resource_class.has_value()) {
          dxp::ResourceUsage usage;
          usage.binding_class = dxp::BindingClass::Texture;
          usage.handle = op_pattern.resource_kind.has_value() ? "texture" : "resource";
          usage.register_index = op_pattern.resource_register_index.value_or(0);
          ctx->resource_exports[export_key] = std::move(usage);
        } else if (effective_kind == OperandKind::Constant || effective_kind == OperandKind::Call) {
          dxp::ImmediateValue immediate;
          if (TryGetImmediateValue(ResolveCapture(match, ctx, op_pattern.capture_name), op_pattern.component_type, immediate)) {
            ctx->immediate_exports[export_key] = std::move(immediate);
          }
        }
      }
    }
  }
  std::vector<llvm::Value*> rule_emitted;
  std::string apply_error;
  auto apply_single_match = [&](MatchResult& match) -> bool {
    apply_error.clear();
    // Cross-step captures: persist this match's captures into the global store
    // (sm5-compatible), regardless of rewrite mode.
    if (ctx != nullptr) {
      for (const auto& [name, value] : match.captures) {
        ctx->captures.values[name] = value;
      }
    }
    if (!rule_mutating) {
      ++applied_count;
      return true;
    }
    llvm::IRBuilder<> builder(match.instructions.front());
    if (auto applied = ApplyMatchRewrite(match, rewrite_mode, rule, insert_index, range_start_offset, range_end_offset, builder, module, dxil_module, ctx, &rule_emitted); !applied) {
      apply_error = applied.error();
      return false;
    }
    ++applied_count;
    ++mutation_count;
    return true;
  };
  switch (match_mode) {
    case MatchKind::MatchAll:
      for (auto& match : matches) {
        if (!apply_single_match(match)) return std::unexpected(apply_error);
      }
      break;
    case MatchKind::Last:
      if (!apply_single_match(matches.back())) return std::unexpected(apply_error);
      break;
    case MatchKind::First:
    default:
      if (!apply_single_match(matches.front())) return std::unexpected(apply_error);
      break;
  }
  if (applied_rule_count != nullptr) *applied_rule_count = applied_count;
  if (mutated_rule_count != nullptr) *mutated_rule_count = mutation_count;
  return {};
}

/// @brief Validates that every captured value an emit consumes dominates the
/// emit's insertion point. Cross-block captures are the normal way recipes
/// compose, but SSA requires the definition to dominate the use � a value
/// defined in a later or sibling block cannot feed an emit placed here.
std::expected<void, std::string> ValidateEmitDominance(
    const std::vector<std::pair<std::string, llvm::Value*>>& consumed_captures,
    const llvm::DominatorTree& dom_tree, llvm::Instruction& insertion_point) {
  for (const auto& [name, value] : consumed_captures) {
    auto* def = llvm::dyn_cast<llvm::Instruction>(value);
    if (def == nullptr) continue;  // constants/arguments dominate everywhere
    if (!dom_tree.dominates(def, &insertion_point)) {
      return std::unexpected("capture '" + name + "' does not dominate the emit insertion point " + "(defined in block '" + def->getParent()->getName().str() + "', emit placed in block '" + insertion_point.getParent()->getName().str() + "'); restructure the recipe so the capture is defined before this emit");
    }
  }
  return {};
}
llvm::Instruction* ResolveAnchor(const MatchResult& match, RewriteKind rewrite_mode, int32_t insert_index,
                                              [[maybe_unused]] llvm::Instruction* range_start,
                                              [[maybe_unused]] llvm::Instruction* range_end) {
  if (insert_index < 0 && rewrite_mode == RewriteKind::After) {
    insert_index = static_cast<int32_t>(match.instructions.size()) - 1;
  }
  if (insert_index >= 0 && static_cast<uint32_t>(insert_index) < match.instructions.size()) {
    return match.instructions[static_cast<size_t>(insert_index)];
  }
  switch (rewrite_mode) {
    case RewriteKind::Replace:
    case RewriteKind::ReplaceRange:
      return match.instructions.empty() ? match.rootCall : match.instructions.back();
    case RewriteKind::After:
      return match.instructions.empty() ? match.rootCall : match.instructions.back();
    case RewriteKind::Before:
      return match.instructions.empty() ? match.rootCall : match.instructions.front();
    case RewriteKind::None:
      return match.rootCall;
  }
  return match.rootCall;
}

std::expected<void, std::string> ApplyMatchRewrite(MatchResult& match, RewriteKind rewrite_mode, const Rule& rule, int32_t insert_index, int32_t range_start_offset, int32_t range_end_offset, llvm::IRBuilder<>& builder,
                                                        llvm::Module& module, hlsl::DxilModule& dxil_module,
                                                        sm6::ExecutionContext* ctx,
                                                        std::vector<llvm::Value*>* rule_emitted) {
  if (rewrite_mode == RewriteKind::None) return {};

  // Terminators can anchor Before-insertions (e.g. "insert before Ret"), but a
  // block cannot gain or lose a terminator through erasing, and nothing can be
  // inserted after one.
  if (rewrite_mode == RewriteKind::Replace || rewrite_mode == RewriteKind::ReplaceRange || rewrite_mode == RewriteKind::After) {
    for (llvm::Instruction* inst : match.instructions) {
      if (inst != nullptr && llvm::Instruction::isTerminator(inst->getOpcode())) {
        return std::unexpected("cannot rewrite a terminator instruction (matched by this rule)");
      }
    }
  }

  // Range offsets are relative to the first instruction of the match (for
  // sequences, the first matched instruction — which may not be a call).
  llvm::Instruction* range_target = (match.rootCall != nullptr) ? match.rootCall : match.instructions.front();
  llvm::Instruction* range_start = nullptr;
  llvm::Instruction* range_end = nullptr;
  if (rewrite_mode == RewriteKind::ReplaceRange) {
    if (!ResolveReplacementRange(rewrite_mode, range_start_offset, range_end_offset, range_target, range_start, range_end)) {
      return std::unexpected("replace_range offsets do not resolve to a valid instruction range");
    }
  }
  llvm::Instruction* anchor = ResolveAnchor(match, rewrite_mode, insert_index, range_start, range_end);
  if (anchor == nullptr) return std::unexpected("failed to resolve a rewrite anchor instruction");

  const bool has_replace_captured =
      std::ranges::any_of(rule.emit_patterns, [](const EmitPattern& emit_pattern) {
        return !emit_pattern.replace_captured.empty();
      });
  std::string emit_error;
  std::vector<llvm::Value*> emitted_values;
  emitted_values.reserve(rule.emit_patterns.size());
  if (!rule.emit_patterns.empty()) {
    const bool insert_before = (rewrite_mode == RewriteKind::Before || rewrite_mode == RewriteKind::Replace || rewrite_mode == RewriteKind::ReplaceRange);
    const llvm::DominatorTree dom_tree = llvm::DominatorTreeAnalysis().run(const_cast<llvm::Function&>(*anchor->getParent()->getParent()));
    for (const auto& emit_pattern : rule.emit_patterns) {
      if (insert_before) {
        builder.SetInsertPoint(anchor);
      } else {
        // Insert after the anchor: create before the anchor (a point whose debug
        // metadata is known-good) and move it into place afterwards. Reading the
        // insertion point after the anchor can touch unrelated IR state, so avoid
        // SetInsertPoint on it entirely.
        builder.SetInsertPoint(anchor);
      }
      std::vector<std::pair<std::string, llvm::Value*>> consumed_captures;
      EmitContext emission{builder, module, dxil_module, match.captures, ctx};
      if (ctx != nullptr) {
        emission.global_values = &ctx->captures.values;
        emission.resolve_function = [ctx](const std::string& name) -> llvm::Function* {
          auto it = ctx->functions.find(name);
          return it == ctx->functions.end() ? nullptr : it->second;
        };
      }
      emission.resolve_resource = [&](const std::string& name) -> std::expected<llvm::Value*, std::string> {
        if (ctx != nullptr) {
          auto it = ctx->resource_handle_values.find(name);
          if (it != ctx->resource_handle_values.end()) return it->second;
        }
        auto it = match.captures.find(name);
        if (it != match.captures.end()) return it->second;
        return std::unexpected("resource handle '" + name + "' was not declared by any add_resource step and not captured by this match");
      };
      auto emitted_result = EmitInstruction(emit_pattern, emission, &consumed_captures);
      if (!emitted_result) {
        return std::unexpected(emitted_result.error());
      }
      llvm::Value* emitted = emitted_result->value;
      llvm::Instruction* dominance_point = anchor;
      if (!insert_before) {
        if (llvm::Instruction* next = anchor->getNextNode()) {
          dominance_point = next;
        }
      }
      if (auto dominance = ValidateEmitDominance(consumed_captures, dom_tree, *dominance_point); !dominance) {
        return std::unexpected("'" + (emit_pattern.name.empty() ? emit_pattern.opcode.value_or("emit") : emit_pattern.name) + "': " + dominance.error());
      }
      if (!insert_before) {
        auto* position = anchor;
        for (auto* instruction : emitted_result->instructions) {
          instruction->removeFromParent();
          instruction->insertAfter(position);
          position = instruction;
        }
      }
      for (auto* instruction : emitted_result->instructions) {
        emitted_values.push_back(instruction);
        if (rule_emitted != nullptr) rule_emitted->push_back(instruction);
      }
      if (!emit_pattern.name.empty()) {
        match.captures[emit_pattern.name] = emitted;
      }
      if (!emit_pattern.replace_captured.empty()) {
        llvm::Value* to_replace = ResolveCapture(match, ctx, emit_pattern.replace_captured);
        if (to_replace != nullptr) {
          if (emitted == nullptr || to_replace->getType() != emitted->getType()) {
            return std::unexpected("replace_captured '" + emit_pattern.replace_captured + "': replacement type mismatch");
          }
          // Rewire uses of the captured value except those introduced by THIS
          // rule's own emits � otherwise emits that legitimately consume the
          // captured value get rewritten into a use-cycle among themselves.
          std::unordered_set<llvm::User*> own_uses;
          const std::vector<llvm::Value*>& emitted_list = (rule_emitted != nullptr) ? *rule_emitted : emitted_values;
          for (llvm::Value* emitted_value : emitted_list) {
            if (emitted_value != nullptr && llvm::isa<llvm::Instruction>(emitted_value)) {
              own_uses.insert(llvm::cast<llvm::User>(emitted_value));
            }
          }
          std::vector<llvm::Use*> to_rewire;
          for (llvm::Use& use : to_replace->uses()) {
            if (own_uses.contains(use.getUser())) continue;
            to_rewire.push_back(&use);
          }
          // The emitted value replaces the captured one at each use site, so it
          // must dominate every use � otherwise the rewiring itself would break SSA.
          // In replace mode the emit is created at the matched value's own position,
          // so it dominates everything the matched value did; the check only matters
          // when the emit is placed elsewhere (before/after modes).
          if (rewrite_mode != RewriteKind::Replace && rewrite_mode != RewriteKind::ReplaceRange) {
            // The emitted value replaces the captured one at each use site, so it
            // must dominate every use � otherwise the rewiring itself would break
            // SSA. In replace mode the emit is created at the matched value's own
            // position, so it dominates everything the matched value did; the check
            // only matters when the emit is placed elsewhere (before/after modes).
            const llvm::DominatorTree rewire_dom_tree = llvm::DominatorTreeAnalysis().run(const_cast<llvm::Function&>(*anchor->getParent()->getParent()));
            for (llvm::Use* use : to_rewire) {
              if (auto* emitted_def = llvm::dyn_cast<llvm::Instruction>(emitted)) {
                if (auto* user_inst = llvm::dyn_cast<llvm::Instruction>(use->getUser())) {
                  if (!rewire_dom_tree.dominates(emitted_def, user_inst)) {
                    return std::unexpected("replace_captured '" + emit_pattern.replace_captured + "': the emitted value (block '" + emitted_def->getParent()->getName().str() + "') does not dominate a use being rewired (block '" + user_inst->getParent()->getName().str() + "'); the captured value's consumers must come after the emit");
                  }
                }
              }
            }
          }
          for (llvm::Use* use : to_rewire) {
            use->set(emitted);
          }
        }
      }
      if (!insert_before) {
        if (!emitted_result->instructions.empty()) anchor = emitted_result->instructions.back();
      }
    }
  }

  if (rewrite_mode == RewriteKind::Replace || rewrite_mode == RewriteKind::ReplaceRange) {
    // Replacement is wired exclusively through replace_captured � an emit that
    // leaves the matched value unreplaced would either produce dead code or,
    // worse, erase a value that is still consumed downstream.
    if (!match.instructions.empty() && !has_replace_captured && !match.instructions.front()->use_empty()) {
      const auto& front_type = match.instructions.front()->getType();
      return std::unexpected(
          "replace mode requires a replace_captured emit: the matched value's type (" + (front_type->isStructTy() ? front_type->getStructName().str() : "scalar") + ") cannot be implicitly rewired");
    }
    if (rewrite_mode == RewriteKind::Replace) {
      for (auto* inst : match.instructions) {
        // Erase only instructions with no remaining uses. Any dead dependency
        // trees are intentionally left for the single global DCE pass at serialization.
        if (inst != nullptr && inst->getParent() != nullptr && inst->use_empty()) inst->eraseFromParent();
      }
    } else if (range_start != nullptr && range_end != nullptr) {
      if (!EraseInstructionRange(range_start, range_end, range_target)) {
        return std::unexpected("replace_range failed to erase the matched instruction range (instructions still in use)");
      }
    }
  }
  return {};
}

}  // namespace

auto ApplyRuleData::Compile() const -> std::expected<ApplyRuleStep, std::string> {
  auto rule = this->rule.Compile();
  if (!rule) return std::unexpected(std::move(rule.error()));
  // The condition must be carried into the compiled step: RecipeEngine gates step
  // execution on step.condition, so discarding it here silently makes `condition`
  // a no-op for every SM6 apply_rule. Mirrors the SM5 backend.
  auto condition = this->condition.Compile();
  ApplyRuleStep step{name, required, rewrite_mode, std::move(condition), std::move(*rule), match_mode};
  step.insert_index = insert_index;
  step.range_start_offset = range_start_offset;
  step.range_end_offset = range_end_offset;
  return step;
}

std::expected<::dxp::ApplyRuleResults, std::string> Execute(const ApplyRuleStep& step, sm6::ExecutionContext& ctx) {
  for (size_t i = 0; i < step.rule.match_patterns.size(); ++i) {
    const auto& pattern = step.rule.match_patterns[i];
    bool has_valid_opcode = false;
    if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
      auto [dxil_op, llvm_op] = ResolveOpCode(*pattern.opcode);
      if (dxil_op.has_value() || llvm_op.has_value()) {
        has_valid_opcode = true;
      }
    }
    if (!has_valid_opcode && (!pattern.callee_name.has_value() || pattern.callee_name->empty())) {
      return std::unexpected("'" + step.name + "': match pattern " + std::to_string(i) + " has no opcode and no callee_name");
    }
    if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
      auto [dxil_op, llvm_op] = ResolveOpCode(*pattern.opcode);
      // Terminators can anchor Before insertions but cannot be erased or inserted after.
      if (llvm_op.has_value() && llvm::Instruction::isTerminator(*llvm_op) && (step.rewrite_mode == RewriteKind::Replace || step.rewrite_mode == RewriteKind::ReplaceRange || step.rewrite_mode == RewriteKind::After)) {
        return std::unexpected("'" + step.name + "': cannot rewrite control flow instructions (Br, Ret, PHI, etc.) with this rewrite_mode");
      }
    }
  }
  for (size_t i = 0; i < step.rule.emit_patterns.size(); ++i) {
    const auto& pattern = step.rule.emit_patterns[i];
    bool has_valid_opcode = false;
    if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
      auto [dxil_op, llvm_op] = ResolveOpCode(*pattern.opcode);
      if (dxil_op.has_value() || llvm_op.has_value()) {
        has_valid_opcode = true;
      }
    }
    if (!has_valid_opcode && (!pattern.cast_opcode.has_value() || pattern.cast_opcode->empty())
        && pattern.capture.empty() && pattern.operands.empty() && !pattern.extract.has_value() && pattern.function_name.empty()) {
      return std::unexpected("'" + step.name + "': emit pattern " + std::to_string(i) + " has no opcode");
    }
  }

  auto* mod = ctx.program.GetModule();
  auto* dxil = ctx.program.GetDxilModule();
  auto* entry = ctx.program.GetEntryFunction();
  if ((mod == nullptr) || (dxil == nullptr)) {
    return std::unexpected(step.name + ": missing module state");
  }
  if (entry == nullptr) {
    return std::unexpected(step.name + ": failed to locate entry function");
  }
  unsigned total_matches = 0;
  unsigned total_mutations = 0;
  ::dxp::ApplyRuleResults result;
  auto applied = ApplyDxilRewriteRules(*entry, *mod, *dxil, step.rule, step.match_mode, step.rewrite_mode, step.insert_index, step.range_start_offset, step.range_end_offset, &total_matches, &total_mutations,
                                       ctx.captures.values, &result, &ctx);
  if (!applied) {
    return std::unexpected("'" + step.name + "': " + applied.error());
  }
  result.match_count = total_matches;
  result.applied_count = total_mutations;

  ctx.program_modified = ctx.program_modified || (total_mutations != 0);
  // Publish match state under both the step name and the rule name, matching SM5,
  // so a later step can gate on either (`is: <step>` / `is: <rule>`).
  ctx.SetState<bool>(step.name, total_matches > 0);
  if (!step.rule.name.empty()) {
    ctx.SetState<bool>(step.rule.name, total_matches > 0);
  }
  return result;
}

std::expected<void, std::string> Validate(const ApplyRuleStep& step, ValidationContext& ctx) {
  if (step.name.empty()) {
    return std::unexpected("apply_rule step requires a name");
  }

  if (!ctx.names.insert(step.name).second) {
    return std::unexpected("duplicate SM6 name '" + step.name + "' reused by step");
  }

  for (const auto& pattern : step.rule.match_patterns) {
    for (const auto& op_pattern : pattern.operand_patterns) {
      if (op_pattern.export_as.has_value()) {
        if (!ctx.names.insert(*op_pattern.export_as).second) {
          return std::unexpected("duplicate export_as key '" + *op_pattern.export_as + "' must be unique across all names");
        }
      }
    }
  }

  if (step.rewrite_mode == RewriteKind::Replace || step.rewrite_mode == RewriteKind::ReplaceRange) {
    if (step.rule.emit_patterns.empty()) {
      return std::unexpected("'" + step.name + "': Replace mode requires at least one emit value");
    }
    // Replacing a matched value is only observable through replace_captured (the
    // emitted value takes over the matched value's uses) or an output write.
    // Without one, the emit block is either dead code or erases a live value.
    const bool wires_replacement =
        std::ranges::any_of(step.rule.emit_patterns, [](const auto& emit) { return !emit.replace_captured.empty(); });
    const bool writes_output = std::ranges::any_of(step.rule.emit_patterns, [&](const auto& emit) {
      return EmitWritesOutput(emit, ctx);
    });
    if (!wires_replacement && !writes_output) {
      return std::unexpected("'" + step.name + "': Replace mode requires at least one emit with 'replace_captured' " + "(or an output write such as StoreOutput) so the emitted code replaces existing values");
    }
  }

  for (const auto& emit : step.rule.emit_patterns) {
    if (auto validated = ValidateEmitPattern(emit, ctx); !validated) {
      return std::unexpected("'" + step.name + "': " + validated.error());
    }
  }

  if (auto r = ValidateCondition<ApplyRuleStep::Results>(step.condition, ctx); !r) {
    return std::unexpected(r.error());
  }

  return {};
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

}  // namespace dxp::sm6::step
