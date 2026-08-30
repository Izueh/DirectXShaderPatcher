#include "dxp/sm6/step/ApplyRuleStep.hpp"
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/InstrTypes.h>
#include <format>
#include "dxc/DxilValidation/DxilValidation.h"
#include "dxp/Condition_impl.hpp"
#include "dxp/ExportTypes.hpp"
#include "dxp/ResultFieldTraits.hpp"
#include "dxp/sm6/ShaderProgram.hpp"
#include "dxp/sm6/step/ApplyRuleStep_impl.hpp"
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
#include "llvm/IR/ValueHandle.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Regex.h"
#include "llvm/Transforms/Utils/Local.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dxp::sm6::step {

struct MatchResult {
  llvm::CallInst* rootCall = nullptr;
  std::unordered_map<std::string, llvm::Value*> captures;
  std::vector<llvm::Instruction*> instructions;

  llvm::Instruction* ResolveAnchor(RewriteKind rewrite_mode, int32_t insert_index,
                                   llvm::Instruction* range_start,
                                   llvm::Instruction* range_end) const;
  std::expected<void, std::string> ApplyRule(RewriteKind rewrite_mode, const Rule& rule, int32_t insert_index, int32_t range_start_offset, int32_t range_end_offset, llvm::IRBuilder<>& builder,
                                             llvm::Module& module, hlsl::DxilModule& dxil_module,
                                             sm6::ExecutionContext* ctx = nullptr,
                                             std::vector<llvm::Value*>* rule_emitted = nullptr);

  [[nodiscard]] llvm::Value* GetCapture(const std::string& name) const {
    auto it = captures.find(name);
    return it != captures.end() ? it->second : nullptr;
  }

  [[nodiscard]] llvm::CallInst* GetCallCapture(const std::string& name) const {
    return llvm::dyn_cast_or_null<llvm::CallInst>(GetCapture(name));
  }
};

// Forward declarations for namespace-scope helpers defined after the matcher
// (the matcher calls them from within the anonymous namespace).
unsigned CollectSequenceMatches(llvm::Function& function,
                                const std::vector<InstructionPattern>& patterns,
                                std::vector<MatchResult>& results,
                                hlsl::DxilModule* dxil_module,
                                const std::unordered_map<std::string, llvm::Value*>* global_captures);
void PruneCandidateInstructions(const std::vector<llvm::WeakTrackingVH>& candidates,
                                const std::unordered_map<std::string, llvm::Value*>* protected_values);

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

/// @brief Maps the shared ComponentType vocabulary to an LLVM scalar type.
/// Returns nullptr for types with no scalar LLVM representation (SNorm/UNorm,
/// packed formats, F8, Invalid, LastEntry).
llvm::Type* LlvmTypeFor(dxp::ComponentType type, llvm::LLVMContext& context) {
  switch (type) {
    case dxp::ComponentType::I1:  return llvm::Type::getInt1Ty(context);
    case dxp::ComponentType::I8:
    case dxp::ComponentType::U8:  return llvm::Type::getInt8Ty(context);
    case dxp::ComponentType::I16:
    case dxp::ComponentType::U16: return llvm::Type::getInt16Ty(context);
    case dxp::ComponentType::I32:
    case dxp::ComponentType::U32: return llvm::Type::getInt32Ty(context);
    case dxp::ComponentType::I64:
    case dxp::ComponentType::U64: return llvm::Type::getInt64Ty(context);
    case dxp::ComponentType::F16: return llvm::Type::getHalfTy(context);
    case dxp::ComponentType::F32: return llvm::Type::getFloatTy(context);
    case dxp::ComponentType::F64: return llvm::Type::getDoubleTy(context);
    default:                      return nullptr;
  }
}

std::pair<std::optional<hlsl::OP::OpCode>, std::optional<unsigned>>
ResolveOpCode(const std::string& opcode) {
  std::optional<hlsl::OP::OpCode> dxil_op_code;
  std::optional<unsigned> instruction_opcode;

  for (unsigned i = 0; i < static_cast<unsigned>(hlsl::OP::OpCode::NumOpCodes); ++i) {
    auto dxil_op = static_cast<hlsl::OP::OpCode>(i);
    const char* name = hlsl::OP::GetOpCodeName(dxil_op);
    if ((name != nullptr) && std::string(name) == opcode) {
      dxil_op_code = dxil_op;
      break;
    }
  }

  for (unsigned i = 1; i < llvm::Instruction::OtherOpsEnd; ++i) {
    const char* name = llvm::Instruction::getOpcodeName(i);
    if ((name != nullptr) && std::string(name) == opcode) {
      instruction_opcode = i;
      break;
    }
  }

  return {dxil_op_code, instruction_opcode};
}

auto GetEmitValueScalarTypeFromPattern(const EmitPattern& pattern, llvm::LLVMContext& context,
                                       llvm::Type* fallback_type) -> llvm::Type* {
  if (!pattern.result_component_type.has_value()) {
    return fallback_type;
  }
  auto comp = static_cast<hlsl::DXIL::ComponentType>(*pattern.result_component_type);
  switch (comp) {
    case hlsl::DXIL::ComponentType::F32:
      return llvm::Type::getFloatTy(context);
    case hlsl::DXIL::ComponentType::U32:
    case hlsl::DXIL::ComponentType::I32:
      return llvm::Type::getInt32Ty(context);
    default:
      return nullptr;
  }
}

/// @brief Formats and stores an emit error; always returns nullptr so callers can
/// `return EmitError(emit_name, message);` from any resolve function.
inline std::unexpected<std::string> EmitError(const std::string& emit_name, const std::string& message) {
  return std::unexpected("emit '" + emit_name + "': " + message);
}

auto IsDxOpCall(const llvm::Instruction& instruction, llvm::StringRef function_name) -> bool {
  const auto* call = llvm::dyn_cast<llvm::CallInst>(&instruction);
  const llvm::Function* callee = call != nullptr ? call->getCalledFunction() : nullptr;
  return callee != nullptr && callee->getName() == function_name;
}

auto IsDxOpCall(const llvm::Instruction& instruction, hlsl::OP::OpCode op_code) -> bool {
  if (!hlsl::OP::IsDxilOpFuncCallInst(&instruction)) return false;
  return hlsl::OP::GetDxilOpFuncCallInst(&instruction) == op_code;
}

auto IsConstantIntValue(const llvm::Value* value, int64_t expected_value) -> bool {
  const auto* constant_int = llvm::dyn_cast<llvm::ConstantInt>(value);
  return constant_int != nullptr && constant_int->getSExtValue() == expected_value;
}

auto IsConstantFloatValue(const llvm::Value* value, double expected_value) -> bool {
  const auto* constant_fp = llvm::dyn_cast<llvm::ConstantFP>(value);
  return constant_fp != nullptr && constant_fp->isExactlyValue(expected_value);
}

auto TryGetConstantStructIntField(const llvm::Value* value, unsigned field_index, uint64_t& field_value) -> bool {
  const auto* constant_value = llvm::dyn_cast<llvm::Constant>(value);
  if (constant_value == nullptr) {
    return false;
  }
  if (llvm::isa<llvm::ConstantAggregateZero>(constant_value)) {
    field_value = 0;
    return true;
  }
  const auto* constant_struct = llvm::dyn_cast<llvm::ConstantStruct>(constant_value);
  if (constant_struct == nullptr || field_index >= constant_struct->getNumOperands()) {
    return false;
  }
  const llvm::ConstantInt* field_constant = llvm::dyn_cast<llvm::ConstantInt>(constant_struct->getOperand(field_index));
  if (field_constant == nullptr) {
    return false;
  }
  field_value = field_constant->getZExtValue();
  return true;
}

auto FindResourceByOrdinal(hlsl::DxilModule& dxil_module,
                           hlsl::DXIL::ResourceClass resource_class,
                           unsigned resource_index) -> const hlsl::DxilResourceBase* {
  switch (resource_class) {
    case hlsl::DXIL::ResourceClass::SRV: {
      const auto& resources = dxil_module.GetSRVs();
      return resource_index < resources.size() ? resources[resource_index].get() : nullptr;
    }
    case hlsl::DXIL::ResourceClass::UAV: {
      const auto& resources = dxil_module.GetUAVs();
      return resource_index < resources.size() ? resources[resource_index].get() : nullptr;
    }
    case hlsl::DXIL::ResourceClass::CBuffer: {
      const auto& resources = dxil_module.GetCBuffers();
      return resource_index < resources.size() ? resources[resource_index].get() : nullptr;
    }
    case hlsl::DXIL::ResourceClass::Sampler: {
      const auto& resources = dxil_module.GetSamplers();
      return resource_index < resources.size() ? resources[resource_index].get() : nullptr;
    }
    default:
      return nullptr;
  }
}

auto CaptureMatchedValue(const std::string& capture_name, llvm::Value* value,
                         std::unordered_map<std::string, llvm::Value*>& captures) -> void {
  if (capture_name.empty()) return;
  captures[capture_name] = value;
}

// match_capture: the matched value must equal a previously captured value
// (cross-step global store first, then same-match local captures). A name that
// was never captured anywhere fails the match.
auto MatchesCapturedValue(const std::string& match_capture_name, llvm::Value* value,
                          const std::unordered_map<std::string, llvm::Value*>& captures,
                          const std::unordered_map<std::string, llvm::Value*>* global_captures) -> bool {
  if (match_capture_name.empty()) return true;
  if (global_captures != nullptr) {
    auto global_it = global_captures->find(match_capture_name);
    if (global_it != global_captures->end()) return global_it->second == value;
  }
  auto capture_it = captures.find(match_capture_name);
  if (capture_it == captures.end()) return false;
  return capture_it->second == value;
}

auto IsPrunableDxilInstruction(const llvm::Instruction& instruction) -> bool {
  if (const auto* call = llvm::dyn_cast<llvm::CallInst>(&instruction)) {
    const llvm::Function* callee = call->getCalledFunction();
    if (callee == nullptr) return false;
    if (call->doesNotAccessMemory() || call->onlyReadsMemory()) return true;
    const llvm::StringRef callee_name = callee->getName();
    return callee_name == "dx.op.annotateHandle" || callee_name == "dx.op.createHandleFromBinding";
  }
  return !instruction.mayHaveSideEffects();
}

void CollectPrunableOperands(llvm::Instruction* instruction, std::unordered_set<llvm::Instruction*>& visited,
                             std::vector<llvm::WeakTrackingVH>& post_order) {
  if (instruction == nullptr || !visited.insert(instruction).second) return;
  post_order.emplace_back(instruction);
  for (const llvm::Use& operand_use : instruction->operands()) {
    auto* operand_instruction = llvm::dyn_cast<llvm::Instruction>(operand_use.get());
    if (operand_instruction == nullptr || !operand_instruction->use_empty()) continue;
    CollectPrunableOperands(operand_instruction, visited, post_order);
  }
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

auto TryResolveResourceFromHandle(llvm::Value* value, hlsl::DxilModule& dxil_module,
                                  hlsl::DXIL::ResourceClass preferred_resource_class,
                                  const hlsl::DxilResourceBase*& resource) -> bool {
  const llvm::CallInst* const call_init = llvm::dyn_cast<llvm::CallInst>(value);
  const llvm::CallInst* call = call_init;
  if (call == nullptr) return false;
  if (IsDxOpCall(*call, hlsl::OP::OpCode::AnnotateHandle)) {
    if (call->getNumArgOperands() < 2) return false;
    call = llvm::dyn_cast<llvm::CallInst>(call->getArgOperand(1));
    if (call == nullptr) return false;
  }
  if (!IsDxOpCall(*call, hlsl::OP::OpCode::CreateHandleFromBinding) || call->getNumArgOperands() < 4) {
    return false;
  }
  uint64_t bind_point = 0;
  if (!TryGetConstantStructIntField(call->getArgOperand(1), 0, bind_point) && !TryGetConstantStructIntField(call->getArgOperand(1), 1, bind_point)) {
    return false;
  }
  uint64_t space = 0;
  if (!TryGetConstantStructIntField(call->getArgOperand(1), 2, space)) return false;
  uint64_t resource_class_value = 0;
  if (!TryGetConstantStructIntField(call->getArgOperand(1), 3, resource_class_value)) return false;
  uint64_t handle_index = bind_point;
  if (const llvm::ConstantInt* handle_index_constant = llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(2))) {
    handle_index = handle_index_constant->getZExtValue();
  }
  auto resolved_resource_class = static_cast<hlsl::DXIL::ResourceClass>(resource_class_value);
  if (resolved_resource_class == hlsl::DXIL::ResourceClass::Invalid && preferred_resource_class != hlsl::DXIL::ResourceClass::Invalid) {
    resolved_resource_class = preferred_resource_class;
  }
  resource = FindResourceByRegisterIndex(dxil_module, resolved_resource_class, static_cast<unsigned>(handle_index),
                                         static_cast<unsigned>(space));
  if (resource == nullptr && resolved_resource_class != hlsl::DXIL::ResourceClass::Invalid) {
    resource = FindResourceByOrdinal(dxil_module, resolved_resource_class, static_cast<unsigned>(handle_index));
  }
  return resource != nullptr;
}

auto MatchOperandPattern(llvm::Value* value, const OperandPattern& pattern,
                         std::unordered_map<std::string, llvm::Value*>& captures,
                         hlsl::DxilModule* dxil_module,
                         const std::unordered_map<std::string, llvm::Value*>* global_captures) -> bool;

auto CheckMatchInstructionPattern(llvm::Value* value, const InstructionPattern& pattern,
                                  std::unordered_map<std::string, llvm::Value*>& captures,
                                  hlsl::DxilModule* dxil_module,
                                  const std::unordered_map<std::string, llvm::Value*>* global_captures) -> bool {
  if (value == nullptr) return false;
  const llvm::CallInst* call = llvm::dyn_cast<llvm::CallInst>(value);
  if (call != nullptr) {
    if (pattern.callee_name.has_value() && !IsDxOpCall(*call, *pattern.callee_name)) return false;
    if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
      auto [dxil_op, llvm_op] = ResolveOpCode(*pattern.opcode);
      if (dxil_op.has_value() && !IsDxOpCall(*call, *dxil_op)) return false;
      if (llvm_op.has_value()) return false;
    }
  } else {
    const llvm::Instruction* instruction = llvm::dyn_cast<llvm::Instruction>(value);
    if (instruction == nullptr) return false;
    if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
      auto [dxil_op, llvm_op] = ResolveOpCode(*pattern.opcode);
      if (llvm_op.has_value() && instruction->getOpcode() != *llvm_op) return false;
      if (dxil_op.has_value()) return false;
    }
  }
  if (call != nullptr) {
    for (const OperandPattern& operand_pattern : pattern.operand_patterns) {
      if (operand_pattern.operand_index >= call->getNumArgOperands() || !MatchOperandPattern(call->getArgOperand(operand_pattern.operand_index), operand_pattern, captures, dxil_module, global_captures)) {
        return false;
      }
    }
  } else {
    const auto* instruction = llvm::dyn_cast<llvm::Instruction>(value);
    if (instruction != nullptr) {
      for (const OperandPattern& operand_pattern : pattern.operand_patterns) {
        if (operand_pattern.operand_index >= instruction->getNumOperands() || !MatchOperandPattern(instruction->getOperand(operand_pattern.operand_index), operand_pattern, captures, dxil_module, global_captures)) {
          return false;
        }
      }
    }
  }
  CaptureMatchedValue(pattern.capture_name, value, captures);
  return MatchesCapturedValue(pattern.match_capture, value, captures, global_captures);
}

auto MatchOperandPattern(llvm::Value* value, const OperandPattern& pattern,
                         std::unordered_map<std::string, llvm::Value*>& captures,
                         hlsl::DxilModule* dxil_module,
                         const std::unordered_map<std::string, llvm::Value*>* global_captures) -> bool {
  if (value == nullptr) return false;
  if (!pattern.capture_name.empty()) {
    captures[pattern.capture_name] = value;
  }
  if (!MatchesCapturedValue(pattern.match_capture, value, captures, global_captures)) {
    return false;
  }
  if (pattern.kind.has_value()) {
    switch (*pattern.kind) {
      case OperandKind::Constant:
        // Optional type restriction: the constant's scalar type must match.
        if (pattern.component_type.has_value()) {
          auto* want = LlvmTypeFor(*pattern.component_type, value->getContext());
          if (want == nullptr || value->getType()->getScalarType() != want) return false;
        }
        if (!pattern.constant_int_values.empty()) {
          if (!IsConstantIntValue(value, pattern.constant_int_values[0])) return false;
        } else if (!pattern.constant_float_values.empty()) {
          if (pattern.constant_float_values.size() == 1) {
            const auto* cf = llvm::dyn_cast<llvm::ConstantFP>(value);
            if ((cf == nullptr) || !IsConstantFloatValue(cf, pattern.constant_float_values[0])) return false;
          } else {
            const auto* cv = llvm::dyn_cast<llvm::ConstantVector>(value);
            if ((cv == nullptr) || cv->getNumOperands() != pattern.constant_float_values.size()) return false;
            for (size_t i = 0; i < pattern.constant_float_values.size(); i++) {
              const auto* ce = llvm::dyn_cast<llvm::ConstantFP>(cv->getOperand(i));
              if ((ce == nullptr) || !IsConstantFloatValue(ce, pattern.constant_float_values[i])) return false;
            }
          }
        } else if (!pattern.component_type.has_value()) {
          // No value spec and no type spec — nothing to match.
          return false;
        }
        break;
      case OperandKind::Call: {
        if (pattern.instruction) {
          if (!CheckMatchInstructionPattern(value, **pattern.instruction, captures, dxil_module, global_captures)) {
            return false;
          }
        }
        break;
      }
      case OperandKind::Resource: {
        if (dxil_module == nullptr) return false;
        const hlsl::DxilResourceBase* resource = nullptr;
        const hlsl::DXIL::ResourceClass preferred_resource_class =
            pattern.resource_class.has_value() ? static_cast<hlsl::DXIL::ResourceClass>(*pattern.resource_class) : hlsl::DXIL::ResourceClass::Invalid;
        if (!TryResolveResourceFromHandle(value, *dxil_module, preferred_resource_class, resource)) {
          return false;
        }
        if (pattern.resource_class.has_value() && resource->GetClass() != static_cast<hlsl::DXIL::ResourceClass>(*pattern.resource_class)) return false;
        if (pattern.resource_kind.has_value() && resource->GetKind() != static_cast<hlsl::DXIL::ResourceKind>(*pattern.resource_kind)) return false;
        if (pattern.resource_name.has_value() && resource->GetGlobalName() != *pattern.resource_name) return false;
        if (pattern.resource_name_like_pattern.has_value()) {
          llvm::Regex resource_name_regex(*pattern.resource_name_like_pattern);
          if (!resource_name_regex.match(resource->GetGlobalName())) return false;
        }
        if (pattern.resource_register_index.has_value() && resource->GetLowerBound() != static_cast<unsigned>(*pattern.resource_register_index)) return false;
        if (pattern.resource_space.has_value() && resource->GetSpaceID() != static_cast<unsigned>(*pattern.resource_space)) return false;
        break;
      }
      case OperandKind::Undefined:
        break;
    }
  }
  CaptureMatchedValue(pattern.capture_name, value, captures);
  return MatchesCapturedValue(pattern.match_capture, value, captures, global_captures);
}

auto MatchInstructionPattern(llvm::CallInst* call, const InstructionPattern& pattern,
                             std::unordered_map<std::string, llvm::Value*>& captures,
                             hlsl::DxilModule* dxil_module,
                             const std::unordered_map<std::string, llvm::Value*>* global_captures) -> bool {
  if (call == nullptr) return false;
  if (pattern.callee_name.has_value() && !IsDxOpCall(*call, *pattern.callee_name)) return false;
  if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
    auto [dxil_op, llvm_op] = ResolveOpCode(*pattern.opcode);
    if (dxil_op.has_value() && !IsDxOpCall(*call, *dxil_op)) return false;
    if (llvm_op.has_value()) return false;
  }
  CaptureMatchedValue(pattern.capture_name, call, captures);
  if (!MatchesCapturedValue(pattern.match_capture, call, captures, global_captures)) return false;
  for (const OperandPattern& operand_pattern : pattern.operand_patterns) {
    if (operand_pattern.operand_index >= call->getNumArgOperands() || !MatchOperandPattern(call->getArgOperand(operand_pattern.operand_index), operand_pattern, captures, dxil_module, global_captures)) {
      return false;
    }
  }
  return true;
}

auto MatchInstructionPattern(llvm::Instruction* instr, const InstructionPattern& pattern,
                             std::unordered_map<std::string, llvm::Value*>& captures,
                             hlsl::DxilModule* dxil_module,
                             const std::unordered_map<std::string, llvm::Value*>* global_captures) -> bool {
  if (instr == nullptr) return false;
  if (pattern.callee_name.has_value()) return false;
  if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
    auto [dxil_op, llvm_op] = ResolveOpCode(*pattern.opcode);
    // A DXIL-opcode pattern can never match a non-call instruction (mirrors the
    // call-side rule that a binary-op pattern can never match a call).
    if (dxil_op.has_value()) return false;
    if (llvm_op.has_value() && instr->getOpcode() != *llvm_op) return false;
  }
  CaptureMatchedValue(pattern.capture_name, instr, captures);
  if (!MatchesCapturedValue(pattern.match_capture, instr, captures, global_captures)) return false;
  for (const OperandPattern& operand_pattern : pattern.operand_patterns) {
    if (operand_pattern.operand_index >= instr->getNumOperands() || !MatchOperandPattern(instr->getOperand(operand_pattern.operand_index), operand_pattern, captures, dxil_module, global_captures)) {
      return false;
    }
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

llvm::Value* ResolveInstructionPattern(const InstructionPattern& pattern, const MatchResult& match,
                                       sm6::ExecutionContext* ctx) {
  if (!pattern.capture_name.empty()) {
    return ResolveCapture(match, ctx, pattern.capture_name);
  }
  return nullptr;
}

std::expected<llvm::Value*, std::string> ResolveEmitOperand(const EmitOperand& operand, llvm::Type* arg_type, llvm::IRBuilder<>& builder,
                                                            llvm::Module& module, hlsl::DxilModule& dxil_module, const MatchResult& match,
                                                            sm6::ExecutionContext* ctx, const std::string& emit_name,
                                                            std::vector<std::pair<std::string, llvm::Value*>>* consumed_captures = nullptr) {
  const auto type_name = [](llvm::Type* type) {
    std::string name;
    llvm::raw_string_ostream stream(name);
    stream << *type;
    return stream.str();
  };
  switch (operand.kind) {
    case OperandKind::Call: {
      llvm::Value* value = nullptr;
      if (operand.capture.has_value()) {
        value = ResolveCapture(match, ctx, *operand.capture);
        if (value == nullptr) return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": capture '" + *operand.capture + "' was not produced by any match or earlier emit");
        if (consumed_captures != nullptr) consumed_captures->emplace_back(*operand.capture, value);
      } else if (operand.instruction) {
        value = ResolveInstructionPattern(**operand.instruction, match, ctx);
        if (value == nullptr) return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": nested instruction pattern resolved to no value");
      } else {
        // Kind-less DXIL operands with no capture/instruction conventionally mean
        // an undefined value (e.g. unused textureLoad offsets) � any other
        // intentional operand kind must be stated explicitly.
        return llvm::UndefValue::get(arg_type);
      }
      if (arg_type != nullptr && value->getType() != arg_type) {
        return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + " type mismatch: expected " + type_name(arg_type) + ", captured value is " + type_name(value->getType()));
      }
      return value;
    }
    case OperandKind::Constant: {
      if (!operand.constant_int_values.empty() || !operand.constant_float_values.empty()) {
        const bool arg_is_vector = arg_type->isVectorTy();
        llvm::Type* elem_type = arg_is_vector ? arg_type->getVectorElementType() : arg_type;
        if (operand.component_type.has_value()) {
          if (auto* t = LlvmTypeFor(*operand.component_type, module.getContext()); t != nullptr) elem_type = t;
        }
        if (!operand.constant_int_values.empty()) {
          if (arg_is_vector) {
            std::vector<llvm::Constant*> elems(operand.constant_int_values.size());
            for (size_t i = 0; i < operand.constant_int_values.size(); i++) {
              elems[i] = llvm::ConstantInt::get(elem_type, operand.constant_int_values[i]);
            }
            if (elems.size() == 1) {
              for (unsigned j = 0; j < arg_type->getVectorNumElements(); j++) elems.push_back(elems[0]);
            }
            return llvm::ConstantVector::get(llvm::ArrayRef<llvm::Constant*>(elems));
          }
          if (!elem_type->isIntegerTy()) {
            return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": integer constant but argument type is " + type_name(arg_type));
          }
          return llvm::ConstantInt::get(elem_type, operand.constant_int_values[0]);
        }
        // Float constants: honor a float element type (explicit or signature),
        // otherwise default to F32 (previously always created F64).
        if (!elem_type->isHalfTy() && !elem_type->isFloatTy() && !elem_type->isDoubleTy()) {
          elem_type = llvm::Type::getFloatTy(module.getContext());
        }
        const auto make_float = [&](double value) -> llvm::Constant* {
          if (elem_type->isHalfTy()) {
            return llvm::ConstantFP::get(module.getContext(), llvm::APFloat(llvm::APFloat::IEEEhalf, value));
          }
          if (elem_type->isFloatTy()) {
            return llvm::ConstantFP::get(module.getContext(), llvm::APFloat(static_cast<float>(value)));
          }
          return llvm::ConstantFP::get(module.getContext(), llvm::APFloat(value));
        };
        if (arg_is_vector) {
          std::vector<llvm::Constant*> elems(operand.constant_float_values.size());
          for (size_t i = 0; i < operand.constant_float_values.size(); i++) {
            elems[i] = make_float(operand.constant_float_values[i]);
          }
          if (elems.size() == 1) {
            for (unsigned j = 0; j < arg_type->getVectorNumElements(); j++) elems.push_back(elems[0]);
          }
          return llvm::ConstantVector::get(llvm::ArrayRef<llvm::Constant*>(elems));
        }
        return make_float(operand.constant_float_values[0]);
      }
      if (ctx != nullptr && operand.capture.has_value()) {
        auto it = ctx->variables.find(*operand.capture);
        if (it != ctx->variables.end()) {
          const std::any& val = it->second;
          if (const auto* ip = std::any_cast<int64_t>(&val)) return builder.getInt64(*ip);
          if (const auto* fp = std::any_cast<double>(&val)) return llvm::ConstantFP::get(module.getContext(), llvm::APFloat(*fp));
          if (const auto* ip32 = std::any_cast<int32_t>(&val)) return builder.getInt32(*ip32);
          if (const auto* fp32 = std::any_cast<float>(&val)) return llvm::ConstantFP::get(module.getContext(), llvm::APFloat(static_cast<double>(*fp32)));
        }
      }
      return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": kind 'constant' requires 'constant_int_values', 'constant_float_values', or a resolvable variable capture");
    }
    case OperandKind::Resource: {
      if (operand.handle.empty()) {
        return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": kind 'resource' requires 'handle'");
      }
      // Resolve add_resource-declared handles to their LLVM createHandle value
      // (mirrors SM5's from_handle -> bind-point resolution via the context).
      if (ctx != nullptr) {
        auto it = ctx->resource_handle_values.find(operand.handle);
        if (it != ctx->resource_handle_values.end()) {
          llvm::Value* handle_value = it->second;
          if (arg_type != nullptr && handle_value->getType() != arg_type) {
            return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": resource handle '" + operand.handle + "' type mismatch: expected " + type_name(arg_type) + ", handle is " + type_name(handle_value->getType()));
          }
          return handle_value;
        }
      }
      auto it = match.captures.find(operand.handle);
      if (it != match.captures.end()) {
        llvm::Value* handle_value = it->second;
        if (arg_type != nullptr && handle_value->getType() != arg_type) {
          return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": captured handle '" + operand.handle + "' type mismatch: expected " + type_name(arg_type) + ", captured value is " + type_name(handle_value->getType()));
        }
        return handle_value;
      }
      return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": resource handle '" + operand.handle + "' was not declared by any add_resource step and not captured by this match");
    }
    case OperandKind::Undefined:
      return llvm::UndefValue::get(arg_type);
  }
  return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": unknown operand kind");
}

std::expected<llvm::Value*, std::string> ResolveEmitPattern(const EmitPattern& pattern, llvm::IRBuilder<>& builder, llvm::Module& module,
                                                            hlsl::DxilModule& dxil_module, const MatchResult& match, sm6::ExecutionContext* ctx,
                                                            std::vector<std::pair<std::string, llvm::Value*>>* consumed_captures = nullptr) {
  const std::string emit_name = pattern.name.empty() ? pattern.opcode.value_or("<unnamed>") : pattern.name;
  if (!pattern.capture.empty()) {
    llvm::Value* captured = ResolveCapture(match, ctx, pattern.capture);
    if (captured == nullptr) return EmitError(emit_name, "capture '" + pattern.capture + "' was not produced by any match or earlier emit");
    if (consumed_captures != nullptr) consumed_captures->emplace_back(pattern.capture, captured);
    return captured;
  }
  // Aggregate extraction: pull one field out of a struct-producing emit
  // (ResRet/CBufRet) captured by an earlier emit pattern in the same rule.
  if (!pattern.aggregate.empty()) {
    llvm::Value* aggregate = ResolveCapture(match, ctx, pattern.aggregate);
    if (aggregate == nullptr) return EmitError(emit_name, "aggregate '" + pattern.aggregate + "' was not produced by any earlier emit");
    if (consumed_captures != nullptr) consumed_captures->emplace_back(pattern.aggregate, aggregate);
    auto* aggregate_type = llvm::dyn_cast<llvm::StructType>(aggregate->getType());
    if (aggregate_type == nullptr) return EmitError(emit_name, "aggregate '" + pattern.aggregate + "' is " + aggregate->getType()->getStructName().str() + ", expected a struct (ResRet/CBufRet)");
    if (pattern.extract_index >= aggregate_type->getNumElements()) {
      return EmitError(emit_name, "extract_index " + std::to_string(pattern.extract_index) + " out of range for " + aggregate_type->getStructName().str() + " (" + std::to_string(aggregate_type->getNumElements()) + " fields)");
    }
    return builder.CreateExtractValue(aggregate, pattern.extract_index);
  }
  llvm::Type* result_type = GetEmitValueScalarTypeFromPattern(pattern, module.getContext(), llvm::Type::getVoidTy(module.getContext()));
  if (result_type == nullptr) {
    // Unsupported component type � fall back to void, matching the previous inline behavior.
    result_type = llvm::Type::getVoidTy(module.getContext());
  }
  std::optional<hlsl::OP::OpCode> resolved_dxil_op;
  std::optional<unsigned> resolved_llvm_op;
  if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
    std::tie(resolved_dxil_op, resolved_llvm_op) = ResolveOpCode(*pattern.opcode);
  }
  // Opcode-less emit with operands: pass-through alias of the first captured
  // operand value (re-exposes an earlier capture under pattern.name).
  if (!resolved_dxil_op.has_value() && !resolved_llvm_op.has_value() && !pattern.operands.empty()) {
    return ResolveEmitOperand(pattern.operands.front(), nullptr, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
  }
  if (resolved_dxil_op.has_value()) {
    hlsl::OP* dxil_op = dxil_module.GetOP();
    if (dxil_op == nullptr) return EmitError(emit_name, "module has no DXIL OP table");
    // GetOpFunc expects the scalar overload type (e.g. float) and creates the
    // function with the correct signature: resource ops expand the return type
    // to ResRet/CBufRet internally (see DxilOperations.cpp RRT/CBRT).
    if (!hlsl::OP::IsOverloadLegal(*resolved_dxil_op, result_type)) {
      const auto comp_name = pattern.result_component_type.has_value() ? std::to_string(static_cast<int>(*pattern.result_component_type)) : std::string("F32 (default)");
      return EmitError(emit_name, "opcode '" + *pattern.opcode + "' does not support overload component type " + comp_name);
    }
    llvm::Function* emitted_function = dxil_op->GetOpFunc(*resolved_dxil_op, result_type);
    if (emitted_function == nullptr) return EmitError(emit_name, "opcode '" + *pattern.opcode + "' could not be resolved to a DXIL function for the requested overload type");
    // Operand index N maps directly to DXIL argument N (argument 0 is the opcode
    // constant). DXIL signatures have no padding � Dot2/Dot3 repeat their
    // operands, and each repeated component has its own argument index.
    std::vector<llvm::Value*> args;
    args.reserve(emitted_function->arg_size());
    for (auto& arg : emitted_function->args()) {
      if (arg.getArgNo() == 0) {
        args.push_back(llvm::ConstantInt::get(arg.getType(), static_cast<uint64_t>(*resolved_dxil_op)));
        continue;
      }
      const EmitOperand* operand = nullptr;
      for (const auto& op : pattern.operands) {
        if (op.operand_index == arg.getArgNo()) {
          operand = &op;
          break;
        }
      }
      if (operand == nullptr) {
        return EmitError(emit_name, "opcode '" + *pattern.opcode + "' argument " + std::to_string(arg.getArgNo()) + " (" + hlsl::OP::GetOpCodeName(*resolved_dxil_op) + ") has no matching operand with index " + std::to_string(arg.getArgNo()));
      }
      auto value_result = ResolveEmitOperand(*operand, arg.getType(), builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
      if (!value_result) return std::unexpected(value_result.error());
      args.push_back(*value_result);
    }
    return builder.CreateCall(emitted_function, args);
  }
  if (resolved_llvm_op.has_value()) {
    if (llvm::Instruction::isTerminator(*resolved_llvm_op)) return nullptr;

    switch (*resolved_llvm_op) {
      case llvm::Instruction::Add:
      case llvm::Instruction::FAdd:
      case llvm::Instruction::Sub:
      case llvm::Instruction::FSub:
      case llvm::Instruction::Mul:
      case llvm::Instruction::FMul:
      case llvm::Instruction::UDiv:
      case llvm::Instruction::SDiv:
      case llvm::Instruction::FDiv:
      case llvm::Instruction::URem:
      case llvm::Instruction::SRem:
      case llvm::Instruction::FRem:
      case llvm::Instruction::Shl:
      case llvm::Instruction::LShr:
      case llvm::Instruction::AShr:
      case llvm::Instruction::And:
      case llvm::Instruction::Or:
      case llvm::Instruction::Xor:  {
        if (pattern.operands.size() != 2 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1) return nullptr;
        auto lhs_result = ResolveEmitOperand(pattern.operands[0], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!lhs_result) return std::unexpected(lhs_result.error());
        llvm::Value* lhs = *lhs_result;
        auto rhs_result = ResolveEmitOperand(pattern.operands[1], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!rhs_result) return std::unexpected(rhs_result.error());
        llvm::Value* rhs = *rhs_result;
        if (lhs->getType() != result_type || rhs->getType() != result_type) return nullptr;
        return builder.CreateBinOp(static_cast<llvm::Instruction::BinaryOps>(*resolved_llvm_op), lhs, rhs);
      }
      case llvm::Instruction::Trunc:
      case llvm::Instruction::ZExt:
      case llvm::Instruction::SExt:
      case llvm::Instruction::FPTrunc:
      case llvm::Instruction::FPExt:
      case llvm::Instruction::UIToFP:
      case llvm::Instruction::SIToFP:
      case llvm::Instruction::FPToUI:
      case llvm::Instruction::FPToSI:
      case llvm::Instruction::IntToPtr:
      case llvm::Instruction::PtrToInt:
      case llvm::Instruction::BitCast:
      case llvm::Instruction::AddrSpaceCast: {
        if (pattern.operands.size() != 1 || pattern.operands[0].operand_index != 0) return nullptr;
        auto source_result = ResolveEmitOperand(pattern.operands[0], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!source_result) return std::unexpected(source_result.error());
        llvm::Value* source = *source_result;
        return builder.CreateCast(static_cast<llvm::Instruction::CastOps>(*resolved_llvm_op), source, result_type);
      }
      case llvm::Instruction::ICmp: {
        if (pattern.operands.size() != 2 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1) return nullptr;
        auto lhs_result = ResolveEmitOperand(pattern.operands[0], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!lhs_result) return std::unexpected(lhs_result.error());
        llvm::Value* lhs = *lhs_result;
        auto rhs_result = ResolveEmitOperand(pattern.operands[1], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!rhs_result) return std::unexpected(rhs_result.error());
        llvm::Value* rhs = *rhs_result;
        if (lhs->getType() != result_type || rhs->getType() != result_type) return nullptr;
        return builder.CreateICmp(static_cast<llvm::CmpInst::Predicate>(0), lhs, rhs);
      }
      case llvm::Instruction::FCmp: {
        if (pattern.operands.size() != 2 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1) return nullptr;
        auto lhs_result = ResolveEmitOperand(pattern.operands[0], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!lhs_result) return std::unexpected(lhs_result.error());
        llvm::Value* lhs = *lhs_result;
        auto rhs_result = ResolveEmitOperand(pattern.operands[1], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!rhs_result) return std::unexpected(rhs_result.error());
        llvm::Value* rhs = *rhs_result;
        if (lhs->getType() != result_type || rhs->getType() != result_type) return nullptr;
        return builder.CreateFCmp(static_cast<llvm::CmpInst::Predicate>(0), lhs, rhs);
      }
      case llvm::Instruction::ExtractElement: {
        if (pattern.operands.size() != 2 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1) return nullptr;
        auto vec_result = ResolveEmitOperand(pattern.operands[0], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!vec_result) return std::unexpected(vec_result.error());
        llvm::Value* vec = *vec_result;
        auto idx_result = ResolveEmitOperand(pattern.operands[1], llvm::Type::getInt32Ty(module.getContext()), builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!idx_result) return std::unexpected(idx_result.error());
        llvm::Value* idx = *idx_result;
        return builder.CreateExtractElement(vec, idx);
      }
      case llvm::Instruction::InsertElement: {
        if (pattern.operands.size() != 3 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1 || pattern.operands[2].operand_index != 2) return nullptr;
        auto vec_result = ResolveEmitOperand(pattern.operands[0], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!vec_result) return std::unexpected(vec_result.error());
        llvm::Value* vec = *vec_result;
        auto val_result = ResolveEmitOperand(pattern.operands[1], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!val_result) return std::unexpected(val_result.error());
        llvm::Value* val = *val_result;
        auto idx_result = ResolveEmitOperand(pattern.operands[2], llvm::Type::getInt32Ty(module.getContext()), builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!idx_result) return std::unexpected(idx_result.error());
        llvm::Value* idx = *idx_result;

        return builder.CreateInsertElement(vec, val, idx);
      }
      case llvm::Instruction::Select: {
        if (pattern.operands.size() != 3 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1 || pattern.operands[2].operand_index != 2) return nullptr;
        auto cond_result = ResolveEmitOperand(pattern.operands[0], llvm::Type::getInt1Ty(module.getContext()), builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!cond_result) return std::unexpected(cond_result.error());
        llvm::Value* cond = *cond_result;
        auto s1_result = ResolveEmitOperand(pattern.operands[1], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!s1_result) return std::unexpected(s1_result.error());
        llvm::Value* s1 = *s1_result;
        auto s2_result = ResolveEmitOperand(pattern.operands[2], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!s2_result) return std::unexpected(s2_result.error());
        llvm::Value* s2 = *s2_result;

        return builder.CreateSelect(cond, s1, s2);
      }
      case llvm::Instruction::Load: {
        if (pattern.operands.size() != 1 || pattern.operands[0].operand_index != 0) return nullptr;
        auto ptr_result = ResolveEmitOperand(pattern.operands[0], llvm::Type::getInt8PtrTy(module.getContext()), builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!ptr_result) return std::unexpected(ptr_result.error());
        llvm::Value* ptr = *ptr_result;
        return builder.CreateLoad(result_type, ptr);
      }
      case llvm::Instruction::Store: {
        if (pattern.operands.size() != 2 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1) return nullptr;
        auto val_result = ResolveEmitOperand(pattern.operands[0], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!val_result) return std::unexpected(val_result.error());
        llvm::Value* val = *val_result;
        auto ptr_result = ResolveEmitOperand(pattern.operands[1], llvm::Type::getInt8PtrTy(module.getContext()), builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!ptr_result) return std::unexpected(ptr_result.error());
        llvm::Value* ptr = *ptr_result;
        builder.CreateStore(val, ptr);
        return nullptr;
      }
      case llvm::Instruction::GetElementPtr: {
        if (pattern.operands.empty() || pattern.operands[0].operand_index != 0) return nullptr;
        auto ptr_result = ResolveEmitOperand(pattern.operands[0], llvm::Type::getInt8PtrTy(module.getContext()), builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
        if (!ptr_result) return std::unexpected(ptr_result.error());
        llvm::Value* ptr = *ptr_result;
        std::vector<llvm::Value*> indices;
        for (size_t i = 1; i < pattern.operands.size(); i++) {
          auto idx_result = ResolveEmitOperand(pattern.operands[i], llvm::Type::getInt32Ty(module.getContext()), builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
          if (!idx_result) return std::unexpected(idx_result.error());
          indices.push_back(*idx_result);
        }
        return builder.CreateGEP(result_type, ptr, indices);
      }
      case llvm::Instruction::Alloca: {
        if (pattern.operands.size() > 1) return nullptr;
        llvm::Value* size = nullptr;
        if (pattern.operands.size() == 1) {
          auto size_result = ResolveEmitOperand(pattern.operands[0], llvm::Type::getInt64Ty(module.getContext()), builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
          if (!size_result) return std::unexpected(size_result.error());
          size = *size_result;
        }
        return builder.CreateAlloca(result_type, size);
      }
      default:
        return nullptr;
    }
  }
  if (pattern.cast_opcode.has_value() && !pattern.cast_opcode->empty()) {
    auto [dxil_op, llvm_op] = ResolveOpCode(*pattern.cast_opcode);
    if (llvm_op.has_value()) {
      if (pattern.operands.size() != 1 || pattern.operands[0].operand_index != 0) return nullptr;
      auto source_result = ResolveEmitOperand(pattern.operands[0], result_type, builder, module, dxil_module, match, ctx, emit_name, consumed_captures);
      if (!source_result) return std::unexpected(source_result.error());
      llvm::Value* source = *source_result;
      return builder.CreateCast(static_cast<llvm::Instruction::CastOps>(*llvm_op), source, result_type);
    }
  }
  return nullptr;
}

unsigned CollectDxilCallMatches(llvm::Function& function, const InstructionPattern& pattern,
                                std::vector<MatchResult>& results, hlsl::DxilModule* dxil_module,
                                const std::unordered_map<std::string, llvm::Value*>* global_captures);
unsigned CollectBinaryOpMatches(llvm::Function& function, const InstructionPattern& pattern,
                                std::vector<MatchResult>& results, hlsl::DxilModule* dxil_module,
                                const std::unordered_map<std::string, llvm::Value*>* global_captures);

/// @brief Collects all matches for one pattern (call-based + binary-op based).
void CollectAllMatches(llvm::Function& function, const InstructionPattern& pattern,
                       std::vector<MatchResult>& matches, hlsl::DxilModule* dxil_module,
                       const std::unordered_map<std::string, llvm::Value*>* global_captures) {
  std::vector<MatchResult> call_matches;
  CollectDxilCallMatches(function, pattern, call_matches, dxil_module, global_captures);
  std::vector<MatchResult> binary_op_matches;
  CollectBinaryOpMatches(function, pattern, binary_op_matches, dxil_module, global_captures);
  matches.insert(matches.end(), std::make_move_iterator(call_matches.begin()),
                 std::make_move_iterator(call_matches.end()));
  matches.insert(matches.end(), std::make_move_iterator(binary_op_matches.begin()),
                 std::make_move_iterator(binary_op_matches.end()));
}

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
  if (rule.match_patterns.size() > 1) {
    // Multiple match patterns form a consecutive (block-local) sequence, mirroring sm5.
    CollectSequenceMatches(function, rule.match_patterns, matches, &dxil_module,
                           (ctx != nullptr) ? &ctx->captures.values : nullptr);
  } else {
    for (const auto& pattern : rule.match_patterns) {
      CollectAllMatches(function, pattern, matches, &dxil_module,
                        (ctx != nullptr) ? &ctx->captures.values : nullptr);
    }
  }
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
        } else if (effective_kind == OperandKind::Constant && !op_pattern.constant_int_values.empty()) {
          // Recipe-specified integer constant shorthand — conventionally i32 (matches the emit default).
          dxp::ImmediateValue imm;
          imm.type = dxp::ComponentType::I32;
          for (auto v : op_pattern.constant_int_values) {
            imm.raw_values.push_back(static_cast<uint64_t>(v));
          }
          ctx->immediate_exports[export_key] = std::move(imm);
        } else if (effective_kind == OperandKind::Call) {
          llvm::Value* val = ResolveCapture(match, ctx, op_pattern.capture_name);
          if (val == nullptr) continue;
          if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(val)) {
            dxp::ImmediateValue imm;
            imm.type = ComponentTypeOf(ci->getType());
            imm.raw_values.push_back(ci->getZExtValue());
            ctx->immediate_exports[export_key] = std::move(imm);
          } else if (auto* cf = llvm::dyn_cast<llvm::ConstantFP>(val)) {
            dxp::ImmediateValue imm;
            imm.type = ComponentTypeOf(cf->getType());
            imm.raw_values.push_back(cf->getValueAPF().bitcastToAPInt().getZExtValue());
            ctx->immediate_exports[export_key] = std::move(imm);
          } else if (auto* cv = llvm::dyn_cast<llvm::ConstantVector>(val)) {
            dxp::ImmediateValue imm;
            imm.type = ComponentTypeOf(cv->getType()->getScalarType());
            for (unsigned i = 0; i < cv->getNumOperands(); ++i) {
              if (auto* ce = llvm::dyn_cast<llvm::ConstantFP>(cv->getOperand(i))) {
                imm.raw_values.push_back(ce->getValueAPF().bitcastToAPInt().getZExtValue());
              }
            }
            if (!imm.raw_values.empty()) {
              ctx->immediate_exports[export_key] = std::move(imm);
            }
          }
        }
      }
    }
  }
  std::vector<llvm::WeakTrackingVH> prune_roots;
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
    // Collect prune candidates for the end-of-step pass (never prune mid-loop):
    // for Before/After the matched instructions may have become dead; for
    // Replace/ReplaceRange their operands may be dead after erasure.
    if (rule.prune_dead_instructions) {
      for (llvm::Instruction* inst : match.instructions) {
        if (inst == nullptr) continue;
        if (rewrite_mode == RewriteKind::Before || rewrite_mode == RewriteKind::After) {
          prune_roots.emplace_back(inst);
        } else if (rewrite_mode == RewriteKind::Replace || rewrite_mode == RewriteKind::ReplaceRange) {
          for (llvm::Use& use : inst->operands()) {
            if (auto* op_inst = llvm::dyn_cast<llvm::Instruction>(use.get())) {
              prune_roots.emplace_back(op_inst);
            }
          }
        }
      }
    }
    llvm::IRBuilder<> builder(match.instructions.front());
    if (auto applied = match.ApplyRule(rewrite_mode, rule, insert_index, range_start_offset, range_end_offset, builder, module, dxil_module, ctx, &rule_emitted); !applied) {
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
  // End-of-step prune: one pass after all matches applied, capture-aware.
  if (rule.prune_dead_instructions && !prune_roots.empty()) {
    PruneCandidateInstructions(prune_roots, (ctx != nullptr) ? &ctx->captures.values : nullptr);
  }
  if (applied_rule_count != nullptr) *applied_rule_count = applied_count;
  if (mutated_rule_count != nullptr) *mutated_rule_count = mutation_count;
  return {};
}

unsigned CollectDxilCallMatches(llvm::Function& function, const InstructionPattern& pattern,
                                std::vector<MatchResult>& results, hlsl::DxilModule* dxil_module,
                                const std::unordered_map<std::string, llvm::Value*>* global_captures) {
  results.clear();
  // Debug: log the pattern
  if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
  }
  for (llvm::BasicBlock& basic_block : function) {
    for (llvm::Instruction& instruction : basic_block) {
      auto* const call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (call == nullptr) continue;
      std::unordered_map<std::string, llvm::Value*> captures;
      if (!MatchInstructionPattern(call, pattern, captures, dxil_module, global_captures)) continue;
      MatchResult result;
      result.rootCall = call;
      result.instructions.push_back(call);
      result.captures = std::move(captures);
      results.push_back(std::move(result));
    }
  }
  return static_cast<unsigned>(results.size());
}

unsigned CollectBinaryOpMatches(llvm::Function& function, const InstructionPattern& pattern,
                                std::vector<MatchResult>& results, hlsl::DxilModule* dxil_module,
                                const std::unordered_map<std::string, llvm::Value*>* global_captures) {
  results.clear();
  for (const auto& op : pattern.operand_patterns) {
    if (!op.kind.has_value()) {
      if (!op.capture_name.empty()) return 0;
      continue;
    }
    if (*op.kind == OperandKind::Call) return 0;
  }
  for (llvm::BasicBlock& basic_block : function) {
    for (llvm::Instruction& instruction : basic_block) {
      if (llvm::isa<llvm::CallInst>(&instruction)) continue;
      std::unordered_map<std::string, llvm::Value*> captures;
      if (!MatchInstructionPattern(&instruction, pattern, captures, dxil_module, global_captures)) continue;
      MatchResult result;
      result.rootCall = nullptr;
      result.instructions.push_back(&instruction);
      result.captures = std::move(captures);
      results.push_back(std::move(result));
    }
  }
  return static_cast<unsigned>(results.size());
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
}  // anonymous namespace

/// @brief Matches a consecutive sequence of instruction patterns within a single
/// basic block (block-local: a sequence cannot span a terminator/block boundary).
/// Overlapping sequences are reported (mirrors sm5); match_mode selects among them.
/// Captures are shared across the whole sequence, so later patterns can constrain
/// values captured by earlier ones.
unsigned CollectSequenceMatches(llvm::Function& function,
                                const std::vector<InstructionPattern>& patterns,
                                std::vector<MatchResult>& results,
                                hlsl::DxilModule* dxil_module,
                                const std::unordered_map<std::string, llvm::Value*>* global_captures) {
  results.clear();
  const size_t sequence_length = patterns.size();
  if (sequence_length == 0) return 0;
  for (llvm::BasicBlock& basic_block : function) {
    std::vector<llvm::Instruction*> block_instructions;
    block_instructions.reserve(basic_block.size());
    for (llvm::Instruction& instruction : basic_block) {
      block_instructions.push_back(&instruction);
    }
    if (block_instructions.size() < sequence_length) continue;
    for (size_t start = 0; start + sequence_length <= block_instructions.size(); ++start) {
      std::unordered_map<std::string, llvm::Value*> captures;
      std::vector<llvm::Instruction*> matched;
      matched.reserve(sequence_length);
      bool ok = true;
      for (size_t i = 0; i < sequence_length; ++i) {
        llvm::Instruction* const instruction = block_instructions[start + i];
        const bool matched_this =
            llvm::isa<llvm::CallInst>(instruction)
                ? MatchInstructionPattern(llvm::cast<llvm::CallInst>(instruction), patterns[i], captures, dxil_module, global_captures)
                : MatchInstructionPattern(instruction, patterns[i], captures, dxil_module, global_captures);
        if (!matched_this) {
          ok = false;
          break;
        }
        matched.push_back(instruction);
      }
      if (!ok) continue;
      MatchResult result;
      result.rootCall = llvm::dyn_cast<llvm::CallInst>(matched.front());
      result.instructions = std::move(matched);
      result.captures = std::move(captures);
      results.push_back(std::move(result));
    }
  }
  return static_cast<unsigned>(results.size());
}

void PruneDeadDxilTree(llvm::Instruction* root, const std::unordered_set<llvm::Instruction*>* protected_set) {
  if (root == nullptr) return;
  std::unordered_set<llvm::Instruction*> visited;
  std::vector<llvm::WeakTrackingVH> post_order;
  CollectPrunableOperands(root, visited, post_order);
  for (auto& it : std::ranges::reverse_view(post_order)) {
    auto* candidate = llvm::dyn_cast_or_null<llvm::Instruction>(static_cast<llvm::Value*>(it));
    if (candidate == nullptr || !candidate->use_empty() || !IsPrunableDxilInstruction(*candidate)) continue;
    if (protected_set != nullptr && protected_set->contains(candidate)) continue;
    candidate->eraseFromParent();
  }
}

void PruneCandidateInstructions(const std::vector<llvm::WeakTrackingVH>& candidates,
                                const std::unordered_map<std::string, llvm::Value*>* protected_values) {
  // Instructions referenced by the cross-step capture store are never pruned
  // (a later step may still emit them).
  std::unordered_set<llvm::Instruction*> protected_set;
  if (protected_values != nullptr) {
    for (const auto& [name, value] : *protected_values) {
      if (auto* inst = llvm::dyn_cast<llvm::Instruction>(value)) protected_set.insert(inst);
    }
  }
  for (const llvm::WeakTrackingVH& candidate_handle : candidates) {
    auto* candidate = llvm::dyn_cast_or_null<llvm::Instruction>(static_cast<llvm::Value*>(candidate_handle));
    if (candidate == nullptr || protected_set.contains(candidate)) continue;
    PruneDeadDxilTree(candidate, &protected_set);
  }
}

llvm::Instruction* MatchResult::ResolveAnchor(RewriteKind rewrite_mode, int32_t insert_index,
                                              [[maybe_unused]] llvm::Instruction* range_start,
                                              [[maybe_unused]] llvm::Instruction* range_end) const {
  if (insert_index < 0 && rewrite_mode == RewriteKind::After) {
    insert_index = static_cast<int32_t>(instructions.size()) - 1;
  }
  if (insert_index >= 0 && static_cast<uint32_t>(insert_index) < instructions.size()) {
    return instructions[static_cast<size_t>(insert_index)];
  }
  switch (rewrite_mode) {
    case RewriteKind::Replace:
    case RewriteKind::ReplaceRange:
      return instructions.empty() ? rootCall : instructions.back();
    case RewriteKind::After:
      return instructions.empty() ? rootCall : instructions.back();
    case RewriteKind::Before:
      return instructions.empty() ? rootCall : instructions.front();
    case RewriteKind::None:
      return rootCall;
  }
  return rootCall;
}

std::expected<void, std::string> MatchResult::ApplyRule(RewriteKind rewrite_mode, const Rule& rule, int32_t insert_index, int32_t range_start_offset, int32_t range_end_offset, llvm::IRBuilder<>& builder,
                                                        llvm::Module& module, hlsl::DxilModule& dxil_module,
                                                        sm6::ExecutionContext* ctx,
                                                        std::vector<llvm::Value*>* rule_emitted) {
  if (rewrite_mode == RewriteKind::None) return {};

  // Terminators can anchor Before-insertions (e.g. "insert before Ret"), but a
  // block cannot gain or lose a terminator through erasing, and nothing can be
  // inserted after one.
  if (rewrite_mode == RewriteKind::Replace || rewrite_mode == RewriteKind::ReplaceRange || rewrite_mode == RewriteKind::After) {
    for (llvm::Instruction* inst : instructions) {
      if (inst != nullptr && llvm::Instruction::isTerminator(inst->getOpcode())) {
        return std::unexpected("cannot rewrite a terminator instruction (matched by this rule)");
      }
    }
  }

  // Range offsets are relative to the first instruction of the match (for
  // sequences, the first matched instruction — which may not be a call).
  llvm::Instruction* range_target = (rootCall != nullptr) ? rootCall : instructions.front();
  llvm::Instruction* range_start = nullptr;
  llvm::Instruction* range_end = nullptr;
  if (rewrite_mode == RewriteKind::ReplaceRange) {
    if (!ResolveReplacementRange(rewrite_mode, range_start_offset, range_end_offset, range_target, range_start, range_end)) {
      return std::unexpected("replace_range offsets do not resolve to a valid instruction range");
    }
  }
  llvm::Instruction* anchor = this->ResolveAnchor(rewrite_mode, insert_index, range_start, range_end);
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
      auto emitted_result = ResolveEmitPattern(emit_pattern, builder, module, dxil_module, *this, ctx, &consumed_captures);
      if (!emitted_result) {
        return std::unexpected(emitted_result.error());
      }
      llvm::Value* emitted = *emitted_result;
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
        if (auto* emitted_inst = llvm::dyn_cast<llvm::Instruction>(emitted)) {
          emitted_inst->removeFromParent();
          emitted_inst->insertAfter(anchor);
        }
      }
      emitted_values.push_back(emitted);
      if (rule_emitted != nullptr) rule_emitted->push_back(emitted);
      if (!emit_pattern.name.empty()) {
        this->captures[emit_pattern.name] = emitted;
      }
      if (!emit_pattern.replace_captured.empty()) {
        llvm::Value* to_replace = ResolveCapture(*this, ctx, emit_pattern.replace_captured);
        if (to_replace != nullptr) {
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
        anchor = llvm::cast<llvm::Instruction>(emitted);
      }
    }
  }

  if (rewrite_mode == RewriteKind::Replace || rewrite_mode == RewriteKind::ReplaceRange) {
    // Replacement is wired exclusively through replace_captured � an emit that
    // leaves the matched value unreplaced would either produce dead code or,
    // worse, erase a value that is still consumed downstream.
    if (!instructions.empty() && !has_replace_captured && !instructions.front()->use_empty()) {
      const auto& front_type = instructions.front()->getType();
      return std::unexpected(
          "replace mode requires a replace_captured emit: the matched value's type (" + (front_type->isStructTy() ? front_type->getStructName().str() : "scalar") + ") cannot be implicitly rewired");
    }
    if (rewrite_mode == RewriteKind::Replace) {
      for (auto* inst : instructions) {
        // Erase only instructions with no remaining uses; anything still used is
        // left for the end-of-step prune.
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

void PruneFunctionDeadCode(llvm::Function& function) {
  bool changed = true;
  while (changed) {
    changed = false;
    std::vector<llvm::WeakTrackingVH> candidates;
    for (llvm::BasicBlock& basic_block : function) {
      for (llvm::Instruction& instruction : basic_block) {
        if (!instruction.use_empty()) continue;
        if (instruction.isTerminator()) continue;
        candidates.emplace_back(&instruction);
      }
    }
    for (const llvm::WeakTrackingVH& candidate_handle : candidates) {
      auto* candidate = llvm::dyn_cast_or_null<llvm::Instruction>(static_cast<llvm::Value*>(candidate_handle));
      if (candidate == nullptr || !candidate->use_empty()) continue;
      if (llvm::isInstructionTriviallyDead(candidate)) {
        llvm::RecursivelyDeleteTriviallyDeadInstructions(candidate);
        changed = true;
        continue;
      }
      const llvm::WeakTrackingVH kPruneProbe(candidate);
      PruneDeadDxilTree(candidate, nullptr);
      if (static_cast<llvm::Value*>(kPruneProbe) == nullptr) changed = true;
    }
  }
}

auto OperandPatternData::Compile() const -> std::expected<OperandPattern, std::string> {
  OperandPattern result;
  result.operand_index = index;
  result.capture_name = capture;
  result.match_capture = match_capture;
  result.export_as = export_as;
  if (kind.has_value()) {
    result.kind = *kind;
  }
  if (result.kind.has_value()) {
    switch (*result.kind) {
      case OperandKind::Call:
        if (instruction) {
          auto instr = instruction->Compile();
          if (!instr) return std::unexpected(std::move(instr.error()));
          result.instruction = xyz::indirect<InstructionPattern>(std::move(*instr));
        }
        break;
      case OperandKind::Constant:
        result.constant_int_values = constant_int_values;
        result.constant_float_values = constant_float_values;
        result.component_type = component_type;
        break;
      case OperandKind::Resource:
        result.resource_class = resource_class;
        result.resource_kind = resource_kind;
        if (!resource_name.empty()) result.resource_name = resource_name;
        if (!resource_name_like.empty()) result.resource_name_like_pattern = resource_name_like;
        result.resource_register_index = register_index;
        result.resource_space = space;
        break;
      case OperandKind::Undefined:
        break;
    }
  }
  return result;
}

auto EmitOperandPatternData::Compile() const -> std::expected<EmitOperand, std::string> {
  EmitOperand result;
  result.operand_index = index;
  if (kind.has_value()) result.kind = *kind;
  if (!capture.empty()) result.capture = capture;
  result.constant_int_values = constant_int_values;
  result.constant_float_values = constant_float_values;
  result.component_type = component_type;
  if (!handle.empty()) result.handle = handle;
  if (instruction) {
    auto compiled = instruction->Compile();
    if (!compiled) return std::unexpected(std::move(compiled.error()));
    result.instruction = xyz::indirect<InstructionPattern>(std::move(*compiled));
  }
  return result;
}

auto RuleData::Compile() const -> std::expected<Rule, std::string> {
  Rule result;
  result.prune_dead_instructions = prune;
  for (const auto& m : match) {
    auto compiled = m.Compile();
    if (!compiled) return std::unexpected(std::move(compiled.error()));
    result.match_patterns.push_back(std::move(*compiled));
  }
  for (const auto& e : emit) {
    auto compiled = e.Compile();
    if (!compiled) return std::unexpected(std::move(compiled.error()));
    result.emit_patterns.push_back(std::move(*compiled));
  }
  return result;
}

auto ApplyRuleData::Compile() const -> std::expected<ApplyRuleStep, std::string> {
  auto rule = this->rule.Compile();
  if (!rule) return std::unexpected(std::move(rule.error()));
  ApplyRuleStep step{name, required, rewrite_mode, {}, std::move(*rule), match_mode};
  step.insert_index = insert_index;
  step.range_start_offset = range_start_offset;
  step.range_end_offset = range_end_offset;
  return step;
}

auto MatchInstructionPatternData::Compile() const -> std::expected<InstructionPattern, std::string> {
  InstructionPattern result;
  result.capture_name = capture;
  result.match_capture = match_capture;
  if (!opcode.empty()) result.opcode = opcode;
  for (const auto& op : operands) {
    auto compiled = op.Compile();
    if (!compiled) return std::unexpected(std::move(compiled.error()));
    result.operand_patterns.push_back(std::move(*compiled));
  }
  return result;
}

auto EmitPatternData::Compile() const -> std::expected<EmitPattern, std::string> {
  EmitPattern result;
  result.name = name;
  result.extract_index = extract_index;
  result.aggregate = aggregate;
  result.result_component_type = result_component_type;
  result.capture = capture;
  result.replace_captured = replace_captured;
  if (!opcode.empty()) result.opcode = opcode;
  if (cast_opcode.has_value()) result.cast_opcode = *cast_opcode;
  for (const auto& op : operands) {
    auto compiled = op.Compile();
    if (!compiled) return std::unexpected(std::move(compiled.error()));
    result.operands.push_back(std::move(*compiled));
  }
  return result;
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
        && pattern.capture.empty() && pattern.operands.empty() && pattern.aggregate.empty()) {
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
  ctx.SetState<bool>(step.name, total_matches > 0);
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
    const bool writes_output =
        std::ranges::any_of(step.rule.emit_patterns, [](const auto& emit) {
          if (!emit.opcode.has_value() || emit.opcode->empty()) return false;
          auto [dxil_op, llvm_op] = ResolveOpCode(*emit.opcode);
          return dxil_op.has_value() && (dxil_op == hlsl::OP::OpCode::StoreOutput || dxil_op == hlsl::OP::OpCode::StoreVertexOutput || dxil_op == hlsl::OP::OpCode::StorePrimitiveOutput || dxil_op == hlsl::OP::OpCode::RawBufferStore || dxil_op == hlsl::OP::OpCode::BufferStore || dxil_op == hlsl::OP::OpCode::TextureStore);
        });
    if (!wires_replacement && !writes_output) {
      return std::unexpected("'" + step.name + "': Replace mode requires at least one emit with 'replace_captured' " + "(or an output write such as StoreOutput) so the emitted code replaces existing values");
    }
  }

  // Op/type consistency: for LLVM binary-op emits, the result and constant
  // operand types must match the opcode's integer/float family. DXIL ops use
  // overloads and accept any component type, so they are not constrained.
  const auto type_is_float = [](dxp::ComponentType type) {
    return type == dxp::ComponentType::F16 || type == dxp::ComponentType::F32 || type == dxp::ComponentType::F64;
  };
  const auto type_is_int = [](dxp::ComponentType type) {
    return type == dxp::ComponentType::I1 || type == dxp::ComponentType::I8 || type == dxp::ComponentType::U8 || type == dxp::ComponentType::I16 || type == dxp::ComponentType::U16 || type == dxp::ComponentType::I32 || type == dxp::ComponentType::U32 || type == dxp::ComponentType::I64 || type == dxp::ComponentType::U64;
  };
  for (const auto& emit_pattern : step.rule.emit_patterns) {
    std::optional<unsigned> llvm_op;
    if (emit_pattern.opcode.has_value() && !emit_pattern.opcode->empty()) {
      auto [dxil_op, resolved_llvm_op] = ResolveOpCode(*emit_pattern.opcode);
      if (!dxil_op.has_value() && !resolved_llvm_op.has_value()) {
        return std::unexpected("'" + step.name + "': emit opcode '" + *emit_pattern.opcode + "' is not a known DXIL or LLVM opcode");
      }
      llvm_op = resolved_llvm_op;
    }
    if (llvm_op.has_value()) {
      const bool is_float_op = llvm_op == llvm::Instruction::FAdd || llvm_op == llvm::Instruction::FSub || llvm_op == llvm::Instruction::FMul || llvm_op == llvm::Instruction::FDiv || llvm_op == llvm::Instruction::FRem;
      const bool is_int_op = llvm_op == llvm::Instruction::Add || llvm_op == llvm::Instruction::Sub || llvm_op == llvm::Instruction::Mul || llvm_op == llvm::Instruction::UDiv || llvm_op == llvm::Instruction::SDiv || llvm_op == llvm::Instruction::URem || llvm_op == llvm::Instruction::SRem || llvm_op == llvm::Instruction::Shl || llvm_op == llvm::Instruction::LShr || llvm_op == llvm::Instruction::AShr || llvm_op == llvm::Instruction::And || llvm_op == llvm::Instruction::Or || llvm_op == llvm::Instruction::Xor;
      if (is_float_op || is_int_op) {
        const auto check_type = [&](std::optional<dxp::ComponentType> type, const char* what) -> bool {
          if (!type.has_value()) return true;
          const bool mismatch = is_float_op ? !type_is_float(*type) : !type_is_int(*type);
          if (mismatch) {
            return false;
          }
          return true;
        };
        if (!check_type(emit_pattern.result_component_type, "result_component_type")) {
          return std::unexpected("'" + step.name + "': emit opcode '" + *emit_pattern.opcode + "' type mismatch for result_component_type");
        }
        for (const auto& operand : emit_pattern.operands) {
          if (!check_type(operand.component_type, "operand component_type")) {
            return std::unexpected("'" + step.name + "': emit opcode '" + *emit_pattern.opcode + "' type mismatch for operand component_type");
          }
        }
      }
    }
  }

  // Emit operands are sorted by operand_index in the ApplyRuleStep constructor,
  // so the recipe is immutable during Validate — no const_cast needed here.

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
