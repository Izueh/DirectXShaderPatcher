#include "dxp/sm6/step/common/Match.hpp"
#include <iterator>
#include <utility>
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResourceBase.h"
#include "dxp/sm6/ShaderProgram.hpp"
#include "dxp/sm6/step/common/Emit.hpp"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Regex.h"

namespace dxp::sm6::step::common {
namespace {
// Forward declarations for helpers defined after the matcher.
unsigned CollectSequenceMatchesImpl(llvm::Function& function,
                                    const std::vector<InstructionPattern>& patterns,
                                    std::vector<MatchResult>& results,
                                    hlsl::DxilModule* dxil_module,
                                    const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context);

bool MatchesExtractValue(llvm::Value* value, const ExtractValuePattern& pattern);

auto IsDxOpCall(const llvm::Instruction& instruction, llvm::StringRef function_name) -> bool {
  const auto* call = llvm::dyn_cast<llvm::CallInst>(&instruction);
  const llvm::Function* callee = call != nullptr ? call->getCalledFunction() : nullptr;
  return callee != nullptr && callee->getName() == function_name;
}

auto IsDxOpCall(const llvm::Instruction& instruction, hlsl::OP::OpCode op_code) -> bool {
  if (!hlsl::OP::IsDxilOpFuncCallInst(&instruction)) return false;
  return hlsl::OP::GetDxilOpFuncCallInst(&instruction) == op_code;
}

auto IsConstantIntValue(const llvm::ConstantInt& value, int64_t expected_value,
                        std::optional<dxp::ComponentType> component_type) -> bool {
  const bool unsigned_value = value.getBitWidth() == 1 || component_type == dxp::ComponentType::U8 || component_type == dxp::ComponentType::U16 || component_type == dxp::ComponentType::U32 || component_type == dxp::ComponentType::U64;
  if (unsigned_value) return expected_value >= 0 && value.getZExtValue() == static_cast<uint64_t>(expected_value);
  return value.getSExtValue() == expected_value;
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
                         const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context) -> bool;

// Nested instruction patterns require recursive operand matching.
// NOLINTNEXTLINE(misc-no-recursion)
auto CheckMatchInstructionPattern(llvm::Value* value, const InstructionPattern& pattern,
                                  std::unordered_map<std::string, llvm::Value*>& captures,
                                  hlsl::DxilModule* dxil_module,
                                  const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context) -> bool {
  if (value == nullptr || !constant_context.error.empty()) return false;
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
  if (pattern.extract.has_value() && !MatchesExtractValue(value, *pattern.extract)) return false;
  if (call != nullptr) {
    for (const OperandPattern& operand_pattern : pattern.operand_patterns) {
      if (operand_pattern.operand_index >= call->getNumArgOperands() || !MatchOperandPattern(call->getArgOperand(operand_pattern.operand_index), operand_pattern, captures, dxil_module, global_captures, constant_context)) {
        return false;
      }
    }
  } else {
    const auto* instruction = llvm::dyn_cast<llvm::Instruction>(value);
    if (instruction != nullptr) {
      for (const OperandPattern& operand_pattern : pattern.operand_patterns) {
        if (operand_pattern.operand_index >= instruction->getNumOperands() || !MatchOperandPattern(instruction->getOperand(operand_pattern.operand_index), operand_pattern, captures, dxil_module, global_captures, constant_context)) {
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
                         const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context) -> bool {
  if (value == nullptr || !constant_context.error.empty()) return false;
  if (!pattern.capture_name.empty()) {
    captures[pattern.capture_name] = value;
  }
  if (!MatchesCapturedValue(pattern.match_capture, value, captures, global_captures)) {
    return false;
  }
  if (pattern.kind.has_value()) {
    switch (*pattern.kind) {
      case OperandKind::Constant: {
        // A constant matcher must not accept a dynamic SSA value.
        if (!llvm::isa<llvm::Constant>(value)) return false;
        // Optional type restriction: the constant's scalar type must match.
        if (pattern.component_type.has_value()) {
          auto* want = LlvmTypeFor(*pattern.component_type, value->getContext());
          if (want == nullptr || value->getType()->getScalarType() != want) return false;
        }
        if (!pattern.constant_int_values.empty() || !pattern.constant_float_values.empty()) {
          auto* element_type = value->getType()->getScalarType();
          const bool integer_array = !pattern.constant_int_values.empty();
          if (integer_array ? !element_type->isIntegerTy() : !element_type->isFloatingPointTy()) return false;
          const unsigned count = value->getType()->isVectorTy() ? value->getType()->getVectorNumElements() : 1;
          const size_t entries = integer_array ? pattern.constant_int_values.size() : pattern.constant_float_values.size();
          if (entries != 1 && entries != count) return false;
          auto expected = ResolveConstantValues(pattern, value->getType(), constant_context.variables);
          if (!expected) {
            constant_context.error = "match operand " + std::to_string(pattern.operand_index) + ": " + expected.error();
            return false;
          }
          // LLVM uniquifies constants, including aggregate-zero and data vectors.
          if (*expected != value) return false;
          // Integer literals also enforce signedness after conversion.
          auto* constant = llvm::cast<llvm::Constant>(value);
          for (unsigned i = 0; i < count; ++i) {
            auto* element = count == 1 && !value->getType()->isVectorTy() ? constant : constant->getAggregateElement(i);
            if (integer_array) {
              const auto& entry = pattern.constant_int_values[pattern.constant_int_values.size() == 1 ? 0 : i];
              if (const auto* literal = std::get_if<int64_t>(&entry);
                  literal != nullptr && !IsConstantIntValue(*llvm::cast<llvm::ConstantInt>(element), *literal, pattern.component_type)) return false;
            } else {
              const auto& entry = pattern.constant_float_values[pattern.constant_float_values.size() == 1 ? 0 : i];
              if (const auto* literal = std::get_if<double>(&entry);
                  literal != nullptr && !llvm::cast<llvm::ConstantFP>(element)->isExactlyValue(*literal)) return false;
            }
          }
        }
        break;
      }
      case OperandKind::Call: {
        if (pattern.instruction) {
          if (!CheckMatchInstructionPattern(value, **pattern.instruction, captures, dxil_module, global_captures, constant_context)) {
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
                             const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context) -> bool {
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
    if (operand_pattern.operand_index >= call->getNumArgOperands() || !MatchOperandPattern(call->getArgOperand(operand_pattern.operand_index), operand_pattern, captures, dxil_module, global_captures, constant_context)) {
      return false;
    }
  }
  return true;
}

auto MatchInstructionPattern(llvm::Instruction* instr, const InstructionPattern& pattern,
                             std::unordered_map<std::string, llvm::Value*>& captures,
                             hlsl::DxilModule* dxil_module,
                             const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context) -> bool {
  if (instr == nullptr) return false;
  if (pattern.callee_name.has_value()) return false;
  if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
    auto [dxil_op, llvm_op] = ResolveOpCode(*pattern.opcode);
    // A DXIL-opcode pattern can never match a non-call instruction (mirrors the
    // call-side rule that a binary-op pattern can never match a call).
    if (dxil_op.has_value()) return false;
    if (llvm_op.has_value() && instr->getOpcode() != *llvm_op) return false;
  }
  if (pattern.extract.has_value() && !MatchesExtractValue(instr, *pattern.extract)) return false;
  CaptureMatchedValue(pattern.capture_name, instr, captures);
  if (!MatchesCapturedValue(pattern.match_capture, instr, captures, global_captures)) return false;
  for (const OperandPattern& operand_pattern : pattern.operand_patterns) {
    if (operand_pattern.operand_index >= instr->getNumOperands() || !MatchOperandPattern(instr->getOperand(operand_pattern.operand_index), operand_pattern, captures, dxil_module, global_captures, constant_context)) {
      return false;
    }
  }
  return true;
}

unsigned CollectDxilCallMatches(llvm::Function& function, const InstructionPattern& pattern,
                                std::vector<MatchResult>& results, hlsl::DxilModule* dxil_module,
                                const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context);
unsigned CollectBinaryOpMatches(llvm::Function& function, const InstructionPattern& pattern,
                                std::vector<MatchResult>& results, hlsl::DxilModule* dxil_module,
                                const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context);

/// @brief Collects all matches for one pattern (call-based + binary-op based).
void CollectAllMatchesImpl(llvm::Function& function, const InstructionPattern& pattern,
                           std::vector<MatchResult>& matches, hlsl::DxilModule* dxil_module,
                           const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context) {
  std::vector<MatchResult> call_matches;
  CollectDxilCallMatches(function, pattern, call_matches, dxil_module, global_captures, constant_context);
  std::vector<MatchResult> binary_op_matches;
  CollectBinaryOpMatches(function, pattern, binary_op_matches, dxil_module, global_captures, constant_context);
  matches.insert(matches.end(), std::make_move_iterator(call_matches.begin()),
                 std::make_move_iterator(call_matches.end()));
  matches.insert(matches.end(), std::make_move_iterator(binary_op_matches.begin()),
                 std::make_move_iterator(binary_op_matches.end()));
}

unsigned CollectDxilCallMatches(llvm::Function& function, const InstructionPattern& pattern,
                                std::vector<MatchResult>& results, hlsl::DxilModule* dxil_module,
                                const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context) {
  results.clear();
  // Debug: log the pattern
  if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
  }
  for (llvm::BasicBlock& basic_block : function) {
    for (llvm::Instruction& instruction : basic_block) {
      auto* const call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (call == nullptr) continue;
      std::unordered_map<std::string, llvm::Value*> captures;
      const bool matched = MatchInstructionPattern(call, pattern, captures, dxil_module, global_captures, constant_context);
      if (!constant_context.error.empty()) return 0;
      if (!matched) continue;
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
                                const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context) {
  results.clear();
  for (llvm::BasicBlock& basic_block : function) {
    for (llvm::Instruction& instruction : basic_block) {
      if (llvm::isa<llvm::CallInst>(&instruction)) continue;
      std::unordered_map<std::string, llvm::Value*> captures;
      const bool matched = MatchInstructionPattern(&instruction, pattern, captures, dxil_module, global_captures, constant_context);
      if (!constant_context.error.empty()) return 0;
      if (!matched) continue;
      MatchResult result;
      result.rootCall = nullptr;
      result.instructions.push_back(&instruction);
      result.captures = std::move(captures);
      results.push_back(std::move(result));
    }
  }
  return static_cast<unsigned>(results.size());
}

bool MatchesExtractValue(llvm::Value* value, const ExtractValuePattern& pattern) {
  if (value == nullptr) return false;
  const auto* extract = llvm::dyn_cast<llvm::ExtractValueInst>(value);
  if (extract == nullptr) return false;
  if (pattern.indices.empty()) return false;
  const auto actual = extract->getIndices();
  if (actual.size() != pattern.indices.size()) return false;
  for (size_t i = 0; i < actual.size(); ++i) {
    if (actual[i] != pattern.indices[i]) return false;
  }
  if (pattern.aggregate_type.has_value() && !MatchesTypePattern(extract->getAggregateOperand()->getType(), *pattern.aggregate_type)) {
    return false;
  }
  auto resolved_type = ResolveExtractType(extract->getAggregateOperand()->getType(), pattern.indices);
  if (!resolved_type) {
    return false;
  }
  if (resolved_type.value() != extract->getType()) {
    return false;
  }
  if (pattern.result_type.has_value() && !MatchesTypePattern(extract->getType(), *pattern.result_type)) {
    return false;
  }
  return true;
}

/// @brief Matches a consecutive sequence of instruction patterns within a single
/// basic block (block-local: a sequence cannot span a terminator/block boundary).
/// Overlapping sequences are reported (mirrors sm5); match_mode selects among them.
/// Captures are shared across the whole sequence, so later patterns can constrain
/// values captured by earlier ones.
unsigned CollectSequenceMatchesImpl(llvm::Function& function,
                                    const std::vector<InstructionPattern>& patterns,
                                    std::vector<MatchResult>& results,
                                    hlsl::DxilModule* dxil_module,
                                    const std::unordered_map<std::string, llvm::Value*>* global_captures, MatchContext& constant_context) {
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
                ? MatchInstructionPattern(llvm::cast<llvm::CallInst>(instruction), patterns[i], captures, dxil_module, global_captures, constant_context)
                : MatchInstructionPattern(instruction, patterns[i], captures, dxil_module, global_captures, constant_context);
        if (!constant_context.error.empty()) return 0;
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

}  // namespace

void CollectAllMatches(llvm::Function& function, const InstructionPattern& pattern,
                       std::vector<MatchResult>& matches, MatchContext& context) {
  CollectAllMatchesImpl(function, pattern, matches, context.dxil_module, context.global_captures, context);
}

unsigned CollectSequenceMatches(llvm::Function& function, const std::vector<InstructionPattern>& patterns,
                                std::vector<MatchResult>& matches, MatchContext& context) {
  return CollectSequenceMatchesImpl(function, patterns, matches, context.dxil_module, context.global_captures, context);
}

}  // namespace dxp::sm6::step::common
