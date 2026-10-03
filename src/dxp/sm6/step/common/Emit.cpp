#include "dxp/sm6/step/common/Emit.hpp"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <type_traits>
#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

namespace dxp::sm6::step::common {

llvm::Type* ResolveFunctionType(const FunctionType& type, llvm::LLVMContext& context) {
  if (type.kind == FunctionType::Kind::Void) return llvm::Type::getVoidTy(context);
  if (type.kind == FunctionType::Kind::Scalar) return LlvmTypeFor(type.component_type, context);
  return nullptr;
}

static llvm::Value* ResolveEmitCapture(const EmitContext& context, const std::string& name) {
  auto it = context.values.find(name);
  if (it != context.values.end()) return it->second;
  if (context.global_values != nullptr) {
    auto global = context.global_values->find(name);
    if (global != context.global_values->end()) return global->second;
  }
  return nullptr;
}

std::string PrintType(llvm::Type* type) {
  if (type == nullptr) return "null";
  std::string printed;
  llvm::raw_string_ostream ostream(printed);
  type->print(ostream);
  return printed;
}

std::expected<ValueTypePattern, std::string> CompileValueTypePattern(const ValueTypePatternData& data) {
  if (data.struct_name.has_value() && data.struct_name->empty()) {
    return std::unexpected("struct_name cannot be empty");
  }
  if (data.struct_name.has_value() && data.kind.has_value() && *data.kind != ValueTypeKind::Struct) {
    return std::unexpected("struct_name requires kind 'struct'");
  }
  if (data.struct_name.has_value() && data.component_type.has_value()) {
    return std::unexpected("struct_name cannot be combined with component_type");
  }
  if (data.component_type.has_value() && data.kind.has_value() && (*data.kind == ValueTypeKind::Array || *data.kind == ValueTypeKind::Struct)) {
    return std::unexpected("component_type cannot be combined with kind 'array' or 'struct'");
  }
  ValueTypePattern result;
  result.kind = data.kind;
  result.struct_name = data.struct_name;
  result.component_type = data.component_type;
  return result;
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
  return LlvmTypeFor(*pattern.result_component_type, context);
}

/// @brief Formats a shared emission error.
inline std::unexpected<std::string> EmitError(const std::string& emit_name, const std::string& message) {
  return std::unexpected("emit '" + emit_name + "': " + message);
}

template <typename Literal>
std::expected<llvm::Constant*, std::string> ResolveConstantArray(
    const std::vector<std::variant<std::string, Literal>>& entries,
    llvm::Type* type, const dxp::VariableStore* ctx) {
  if (type == nullptr) return std::unexpected("constant requires an operand type");
  auto* element_type = type->getScalarType();
  constexpr bool integer_array = std::is_same_v<Literal, int64_t>;
  if (integer_array ? !element_type->isIntegerTy() : !element_type->isFloatingPointTy()) {
    return std::unexpected(std::string(integer_array ? "integer" : "floating-point") + " constant array is incompatible with " + PrintType(type));
  }
  const size_t width = type->isVectorTy() ? type->getVectorNumElements() : 1;
  if (entries.size() != 1 && entries.size() != width) {
    return std::unexpected("constant array length must be one or match operand width " + std::to_string(width));
  }
  if (entries.empty()) return std::unexpected("constant array requires at least one value");
  std::vector<llvm::Constant*> values;
  values.reserve(width);
  for (const auto& entry : entries) {
    if (const auto* literal = std::get_if<Literal>(&entry)) {
      if constexpr (integer_array) {
        values.push_back(llvm::ConstantInt::get(element_type, *literal));
      } else {
        values.push_back(llvm::ConstantFP::get(element_type, *literal));
      }
      continue;
    }
    const auto& name = std::get<std::string>(entry);
    const auto* variable = ctx != nullptr ? ctx->FindVariable(name) : nullptr;
    if (variable == nullptr) return std::unexpected("unknown environment constant '" + name + "'");
    auto scalar = std::visit([&](auto value) -> std::expected<llvm::Constant*, std::string> {
      if constexpr (integer_array) {
        const unsigned bits = element_type->getIntegerBitWidth();
        bool representable = true;
        if constexpr (std::is_floating_point_v<decltype(value)>) {
          const auto number = static_cast<long double>(value);
          const auto exponent = static_cast<int>(bits);
          representable = std::isfinite(number) && std::trunc(number) == number && number >= -std::ldexp(1.0L, exponent - 1) && number < std::ldexp(1.0L, exponent);
        } else if (bits < std::numeric_limits<uint64_t>::digits) {
          if constexpr (std::is_signed_v<decltype(value)>) {
            if (value < 0) {
              representable = static_cast<int64_t>(value) >= -(int64_t{1} << (bits - 1));
            } else {
              representable = static_cast<uint64_t>(value) < (uint64_t{1} << bits);
            }
          } else {
            representable = static_cast<uint64_t>(value) < (uint64_t{1} << bits);
          }
        }
        if (!representable) return std::unexpected("environment constant '" + name + "' is not an integral value representable by " + PrintType(element_type));
        uint64_t raw = 0;
        if constexpr (std::is_floating_point_v<decltype(value)>) {
          raw = value < 0 ? static_cast<uint64_t>(static_cast<int64_t>(value)) : static_cast<uint64_t>(value);
        } else {
          raw = static_cast<uint64_t>(value);
        }
        return llvm::ConstantInt::get(element_type, raw);
      } else {
        return llvm::ConstantFP::get(element_type, static_cast<double>(value));
      }
    },
                             *variable);
    if (!scalar) return std::unexpected(scalar.error());
    values.push_back(*scalar);
  }
  if (!type->isVectorTy()) return values.front();
  if (values.size() == 1) values.resize(width, values.front());
  return llvm::ConstantVector::get(values);
}

template <typename Operand>
std::expected<llvm::Constant*, std::string> ResolveConstantValuesImpl(
    const Operand& operand, llvm::Type* type, const dxp::VariableStore* ctx) {
  if (!operand.constant_int_values.empty()) return ResolveConstantArray(operand.constant_int_values, type, ctx);
  if (!operand.constant_float_values.empty()) return ResolveConstantArray(operand.constant_float_values, type, ctx);
  return std::unexpected("kind 'constant' requires 'constant_int_values' or 'constant_float_values'");
}

std::expected<llvm::Constant*, std::string> ResolveConstantValues(const EmitOperand& operand, llvm::Type* type, const dxp::VariableStore* variables) {
  return ResolveConstantValuesImpl(operand, type, variables);
}

std::expected<llvm::Constant*, std::string> ResolveConstantValues(const OperandPattern& operand, llvm::Type* type, const dxp::VariableStore* variables) {
  return ResolveConstantValuesImpl(operand, type, variables);
}

std::expected<uint32_t, std::string> ConstantIntToExtractIndex(const std::string& capture_name, size_t depth, llvm::ConstantInt* ci) {
  if (ci == nullptr) {
    return std::unexpected("extract index " + std::to_string(depth) + ": capture '" + capture_name + "' must resolve to an integer ConstantInt");
  }
  // Interpret the ConstantInt bit pattern as an unsigned integer (LLVM
  // integers are signless); a signed-negative conceptual value widens to
  // a large unsigned value and fails the aggregate bounds check.
  if (ci->getValue().getActiveBits() > std::numeric_limits<uint32_t>::digits) {
    return std::unexpected("extract index " + std::to_string(depth) + ": capture '" + capture_name + "' is outside the supported unsigned index range");
  }
  return static_cast<uint32_t>(ci->getZExtValue());
}

bool MatchesTypePattern(llvm::Type* type, const ValueTypePattern& pattern) {
  if (type == nullptr) return false;
  if (!pattern.kind.has_value() && !pattern.struct_name.has_value() && !pattern.component_type.has_value()) {
    return true;
  }
  auto* struct_type = llvm::dyn_cast<llvm::StructType>(type);
  if (pattern.kind.has_value()) {
    switch (*pattern.kind) {
      case ValueTypeKind::Struct:
        if (struct_type == nullptr) return false;
        break;
      case ValueTypeKind::Array:
        if (!llvm::isa<llvm::ArrayType>(type)) return false;
        break;
      case ValueTypeKind::Vector:
        if (!llvm::isa<llvm::VectorType>(type)) return false;
        break;
      case ValueTypeKind::Scalar:
        if (!type->isIntegerTy() && !type->isFloatingPointTy()) return false;
        break;
    }
  }
  if (pattern.struct_name.has_value()) {
    if (struct_type == nullptr || !struct_type->hasName()) return false;
    if (struct_type->getName() != *pattern.struct_name) return false;
  }
  // component_type constrains scalar values or vector element types only;
  // array/struct guards are structural (kind/struct_name).
  if (pattern.component_type.has_value()) {
    auto* want = LlvmTypeFor(*pattern.component_type, type->getContext());
    llvm::Type* actual = nullptr;
    if (auto* vec = llvm::dyn_cast<llvm::VectorType>(type)) {
      actual = vec->getElementType();
    } else if (!llvm::isa<llvm::ArrayType>(type) && !llvm::isa<llvm::StructType>(type)) {
      actual = type;
    }
    if (actual == nullptr || actual != want) return false;
  }
  return true;
}

std::expected<llvm::Type*, std::string> ResolveExtractType(llvm::Type* aggregate_type, std::span<const uint32_t> indices) {
  if (aggregate_type == nullptr) {
    return std::unexpected("extractvalue aggregate type is null");
  }
  if (indices.empty()) {
    return std::unexpected("extractvalue requires at least one index");
  }
  llvm::Type* current = aggregate_type;
  for (size_t depth = 0; depth < indices.size(); ++depth) {
    const uint32_t index = indices[depth];
    if (auto* st = llvm::dyn_cast<llvm::StructType>(current)) {
      if (st->isOpaque()) {
        return std::unexpected("extract path enters opaque struct at index depth " + std::to_string(depth));
      }
      if (index >= st->getNumElements()) {
        return std::unexpected("extract index " + std::to_string(index) + " out of range at depth " + std::to_string(depth) + " for " + PrintType(st));
      }
      current = st->getElementType(index);
      continue;
    }
    if (auto* at = llvm::dyn_cast<llvm::ArrayType>(current)) {
      if (index >= at->getNumElements()) {
        return std::unexpected("extract index " + std::to_string(index) + " out of range at depth " + std::to_string(depth) + " for " + PrintType(at));
      }
      current = at->getElementType();
      continue;
    }
    return std::unexpected("extract path enters non-aggregate type " + PrintType(current) + " at depth " + std::to_string(depth));
  }
  return current;
}

std::expected<llvm::Value*, std::string> ResolveEmitOperand(const EmitOperand& operand, llvm::Type* arg_type, EmitContext& context, const std::string& emit_name,
                                                            std::vector<std::pair<std::string, llvm::Value*>>* consumed_captures) {
  auto& module = context.module;
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
        value = ResolveEmitCapture(context, *operand.capture);
        if (value == nullptr) return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": capture '" + *operand.capture + "' was not produced by any match or earlier emit");
        if (consumed_captures != nullptr) consumed_captures->emplace_back(*operand.capture, value);
      } else if (operand.instruction) {
        value = ResolveEmitCapture(context, (**operand.instruction).capture_name);
        if (value == nullptr) return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": nested instruction pattern resolved to no value");
      } else {
        // Kind-less DXIL operands with no capture/instruction conventionally mean
        // an undefined value (e.g. unused textureLoad offsets) � any other
        // intentional operand kind must be stated explicitly.
        if (arg_type == nullptr || arg_type->isVoidTy()) return EmitError(emit_name, "undefined operand requires a nonvoid operand type");
        return llvm::UndefValue::get(arg_type);
      }
      if (arg_type != nullptr && value->getType() != arg_type) {
        return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + " type mismatch: expected " + type_name(arg_type) + ", captured value is " + type_name(value->getType()));
      }
      auto* owner = context.builder.GetInsertBlock()->getParent();
      if (auto* instruction = llvm::dyn_cast<llvm::Instruction>(value); instruction != nullptr && instruction->getParent()->getParent() != owner) {
        return EmitError(emit_name, "captured instruction belongs to another function");
      }
      if (auto* argument = llvm::dyn_cast<llvm::Argument>(value); argument != nullptr && argument->getParent() != owner) {
        return EmitError(emit_name, "captured argument belongs to another function");
      }
      return value;
    }
    case OperandKind::Constant: {
      if (arg_type == nullptr) return EmitError(emit_name, "constant requires an operand type");
      if (operand.component_type.has_value() && LlvmTypeFor(*operand.component_type, module.getContext()) != arg_type->getScalarType()) {
        return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": component_type disagrees with operand type " + type_name(arg_type));
      }
      auto result = ResolveConstantValues(operand, arg_type, context.variables);
      if (!result) return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": " + result.error());
      return *result;
    }
    case OperandKind::Resource: {
      if (operand.handle.empty()) {
        return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": kind 'resource' requires 'handle'");
      }
      if (!context.resolve_resource) return EmitError(emit_name, "resource resolver is unavailable");
      auto resolved = context.resolve_resource(operand.handle);
      if (!resolved) return EmitError(emit_name, resolved.error());
      llvm::Value* value = *resolved;
      if (value == nullptr) return EmitError(emit_name, "resource handle '" + operand.handle + "' is unavailable");
      if (arg_type != nullptr && value->getType() != arg_type) {
        return EmitError(emit_name, "resource handle '" + operand.handle + "' type mismatch: expected " + type_name(arg_type));
      }
      if (consumed_captures != nullptr) consumed_captures->emplace_back(operand.handle, value);
      return value;
    }
    case OperandKind::Undefined:
      if (arg_type == nullptr || arg_type->isVoidTy()) return EmitError(emit_name, "undefined operand requires a nonvoid operand type");
      return llvm::UndefValue::get(arg_type);
  }
  return EmitError(emit_name, "operand " + std::to_string(operand.operand_index) + ": unknown operand kind");
}

static std::expected<llvm::Value*, std::string> ResolveEmitPatternValue(const EmitPattern& pattern, EmitContext& context,
                                                                        std::vector<std::pair<std::string, llvm::Value*>>* consumed_captures) {
  auto& builder = context.builder;
  auto& module = context.module;
  auto& dxil_module = context.dxil_module;
  const std::string emit_name = pattern.name.empty() ? pattern.opcode.value_or("<unnamed>") : pattern.name;
  if (!pattern.function_name.empty()) {
    auto* function = context.resolve_function ? context.resolve_function(pattern.function_name) : nullptr;
    if (function == nullptr) return EmitError(emit_name, "function '" + pattern.function_name + "' was not executed by an earlier declare_function step");
    if (pattern.operands.size() != function->arg_size()) return EmitError(emit_name, "function argument count mismatch");
    std::vector<llvm::Value*> args;
    for (auto& arg : function->args()) {
      auto operand = std::ranges::find_if(pattern.operands, [&](const auto& op) { return op.operand_index == arg.getArgNo(); });
      if (operand == pattern.operands.end()) return EmitError(emit_name, "missing function argument " + std::to_string(arg.getArgNo()));
      auto value = ResolveEmitOperand(*operand, arg.getType(), context, emit_name, consumed_captures);
      if (!value) return std::unexpected(value.error());
      args.push_back(*value);
    }
    return builder.CreateCall(function, args);
  }
  if (!pattern.capture.empty()) {
    llvm::Value* captured = ResolveEmitCapture(context, pattern.capture);
    if (captured == nullptr) return EmitError(emit_name, "capture '" + pattern.capture + "' was not produced by any match or earlier emit");
    if (consumed_captures != nullptr) consumed_captures->emplace_back(pattern.capture, captured);
    return captured;
  }
  // Extractvalue emission: pull a field (or nested field) out of an aggregate
  // captured by an earlier match or emit pattern (ResRet/CBufRet etc.).
  if (pattern.extract.has_value()) {
    const EmitExtractValue& ex = *pattern.extract;
    llvm::Value* aggregate = ResolveEmitCapture(context, ex.aggregate);
    if (aggregate == nullptr) return EmitError(emit_name, "aggregate '" + ex.aggregate + "' was not produced by any match or earlier emit");
    if (consumed_captures != nullptr) consumed_captures->emplace_back(ex.aggregate, aggregate);
    if (ex.aggregate_type.has_value() && !MatchesTypePattern(aggregate->getType(), *ex.aggregate_type)) {
      return EmitError(emit_name, "aggregate '" + ex.aggregate + "' has type " + PrintType(aggregate->getType()) + ", which does not satisfy the requested aggregate type");
    }
    std::vector<uint32_t> indices;
    for (size_t depth = 0; depth < ex.indices.size(); ++depth) {
      if (const auto* literal = std::get_if<uint32_t>(&ex.indices[depth])) {
        indices.push_back(*literal);
        continue;
      }
      const CaptureExtractIndex& cap = std::get<CaptureExtractIndex>(ex.indices[depth]);
      llvm::Value* index_value = ResolveEmitCapture(context, cap.capture);
      if (index_value == nullptr) {
        return EmitError(emit_name, "extract index " + std::to_string(depth) + ": capture '" + cap.capture + "' was not produced by any match or earlier emit");
      }
      auto* ci = llvm::dyn_cast<llvm::ConstantInt>(index_value);
      auto index_as_uint = ConstantIntToExtractIndex(cap.capture, depth, ci);
      if (!index_as_uint) {
        return EmitError(emit_name, std::move(index_as_uint.error()));
      }
      indices.push_back(*index_as_uint);
    }
    auto extracted_type = ResolveExtractType(aggregate->getType(), indices);
    if (!extracted_type) {
      return EmitError(emit_name, std::move(extracted_type.error()));
    }
    if (ex.result_type.has_value() && !MatchesTypePattern(*extracted_type, *ex.result_type)) {
      return EmitError(emit_name, "extracted type " + PrintType(*extracted_type) + " does not satisfy result_type");
    }
    return builder.CreateExtractValue(aggregate, indices);
  }
  llvm::Type* result_type = GetEmitValueScalarTypeFromPattern(pattern, module.getContext(), llvm::Type::getVoidTy(module.getContext()));
  if (result_type == nullptr) return EmitError(emit_name, "unsupported result_component_type");
  const auto resolve_cast_source = [&]() -> std::expected<llvm::Value*, std::string> {
    const auto& operand = pattern.operands.front();
    const bool undefined_source = operand.kind == OperandKind::Undefined || (operand.kind == OperandKind::Call && !operand.capture.has_value() && !operand.instruction);
    llvm::Type* source_type = nullptr;
    if (operand.kind == OperandKind::Constant || undefined_source) {
      if (!operand.component_type.has_value()) {
        return EmitError(emit_name, operand.kind == OperandKind::Constant ? "cast constant source requires component_type" : "cast undefined source requires component_type");
      }
      source_type = LlvmTypeFor(*operand.component_type, module.getContext());
      if (source_type == nullptr) return EmitError(emit_name, "unsupported cast source component_type");
    }
    return ResolveEmitOperand(operand, source_type, context, emit_name, consumed_captures);
  };
  std::optional<hlsl::OP::OpCode> resolved_dxil_op;
  std::optional<unsigned> resolved_llvm_op;
  if (pattern.opcode.has_value() && !pattern.opcode->empty()) {
    std::tie(resolved_dxil_op, resolved_llvm_op) = ResolveOpCode(*pattern.opcode);
  }
  // Opcode-less emit with operands: pass-through alias of the first captured
  // operand value (re-exposes an earlier capture under pattern.name).
  if (!resolved_dxil_op.has_value() && !resolved_llvm_op.has_value() && !pattern.cast_opcode.has_value() && !pattern.operands.empty()) {
    return ResolveEmitOperand(pattern.operands.front(), nullptr, context, emit_name, consumed_captures);
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
      auto value_result = ResolveEmitOperand(*operand, arg.getType(), context, emit_name, consumed_captures);
      if (!value_result) return std::unexpected(value_result.error());
      args.push_back(*value_result);
    }
    return builder.CreateCall(emitted_function, args);
  }
  if (resolved_llvm_op.has_value()) {
    if (llvm::Instruction::isTerminator(*resolved_llvm_op)) return EmitError(emit_name, "unsupported opcode or invalid operands");
    if (result_type->isVoidTy()) return EmitError(emit_name, "LLVM emission requires result_component_type");

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
        if (pattern.operands.size() != 2 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1) return EmitError(emit_name, "unsupported opcode or invalid operands");
        auto lhs_result = ResolveEmitOperand(pattern.operands[0], result_type, context, emit_name, consumed_captures);
        if (!lhs_result) return std::unexpected(lhs_result.error());
        llvm::Value* lhs = *lhs_result;
        auto rhs_result = ResolveEmitOperand(pattern.operands[1], result_type, context, emit_name, consumed_captures);
        if (!rhs_result) return std::unexpected(rhs_result.error());
        llvm::Value* rhs = *rhs_result;
        if (lhs->getType() != result_type || rhs->getType() != result_type) return EmitError(emit_name, "unsupported opcode or invalid operands");
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
        if (pattern.operands.size() != 1 || pattern.operands[0].operand_index != 0) return EmitError(emit_name, "unsupported opcode or invalid operands");
        auto source_result = resolve_cast_source();
        if (!source_result) return std::unexpected(source_result.error());
        llvm::Value* source = *source_result;
        if (!llvm::CastInst::castIsValid(static_cast<llvm::Instruction::CastOps>(*resolved_llvm_op), source, result_type)) return EmitError(emit_name, "invalid cast source/result types");
        return builder.CreateCast(static_cast<llvm::Instruction::CastOps>(*resolved_llvm_op), source, result_type);
      }
      case llvm::Instruction::ICmp:
        // No comparison predicate is exposed yet; zero is invalid for ICmp.
        return EmitError(emit_name, "icmp requires a comparison predicate, which is not supported yet");
      case llvm::Instruction::FCmp: {
        if (pattern.operands.size() != 2 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1) return EmitError(emit_name, "unsupported opcode or invalid operands");
        auto lhs_result = ResolveEmitOperand(pattern.operands[0], result_type, context, emit_name, consumed_captures);
        if (!lhs_result) return std::unexpected(lhs_result.error());
        llvm::Value* lhs = *lhs_result;
        auto rhs_result = ResolveEmitOperand(pattern.operands[1], result_type, context, emit_name, consumed_captures);
        if (!rhs_result) return std::unexpected(rhs_result.error());
        llvm::Value* rhs = *rhs_result;
        if (lhs->getType() != result_type || rhs->getType() != result_type) return EmitError(emit_name, "unsupported opcode or invalid operands");
        return builder.CreateFCmp(static_cast<llvm::CmpInst::Predicate>(0), lhs, rhs);
      }
      case llvm::Instruction::ExtractElement: {
        if (pattern.operands.size() != 2 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1) return EmitError(emit_name, "unsupported opcode or invalid operands");
        auto vec_result = ResolveEmitOperand(pattern.operands[0], result_type, context, emit_name, consumed_captures);
        if (!vec_result) return std::unexpected(vec_result.error());
        llvm::Value* vec = *vec_result;
        auto idx_result = ResolveEmitOperand(pattern.operands[1], llvm::Type::getInt32Ty(module.getContext()), context, emit_name, consumed_captures);
        if (!idx_result) return std::unexpected(idx_result.error());
        llvm::Value* idx = *idx_result;
        if (!vec->getType()->isVectorTy()) return EmitError(emit_name, "extractelement requires a vector operand");
        return builder.CreateExtractElement(vec, idx);
      }
      case llvm::Instruction::InsertElement: {
        if (pattern.operands.size() != 3 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1 || pattern.operands[2].operand_index != 2) return EmitError(emit_name, "unsupported opcode or invalid operands");
        auto vec_result = ResolveEmitOperand(pattern.operands[0], result_type, context, emit_name, consumed_captures);
        if (!vec_result) return std::unexpected(vec_result.error());
        llvm::Value* vec = *vec_result;
        auto val_result = ResolveEmitOperand(pattern.operands[1], result_type, context, emit_name, consumed_captures);
        if (!val_result) return std::unexpected(val_result.error());
        llvm::Value* val = *val_result;
        auto idx_result = ResolveEmitOperand(pattern.operands[2], llvm::Type::getInt32Ty(module.getContext()), context, emit_name, consumed_captures);
        if (!idx_result) return std::unexpected(idx_result.error());
        llvm::Value* idx = *idx_result;

        if (!vec->getType()->isVectorTy() || vec->getType()->getVectorElementType() != val->getType()) return EmitError(emit_name, "insertelement requires a vector and matching element type");
        return builder.CreateInsertElement(vec, val, idx);
      }
      case llvm::Instruction::Select: {
        if (pattern.operands.size() != 3 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1 || pattern.operands[2].operand_index != 2) return EmitError(emit_name, "unsupported opcode or invalid operands");
        auto cond_result = ResolveEmitOperand(pattern.operands[0], llvm::Type::getInt1Ty(module.getContext()), context, emit_name, consumed_captures);
        if (!cond_result) return std::unexpected(cond_result.error());
        llvm::Value* cond = *cond_result;
        auto s1_result = ResolveEmitOperand(pattern.operands[1], result_type, context, emit_name, consumed_captures);
        if (!s1_result) return std::unexpected(s1_result.error());
        llvm::Value* s1 = *s1_result;
        auto s2_result = ResolveEmitOperand(pattern.operands[2], result_type, context, emit_name, consumed_captures);
        if (!s2_result) return std::unexpected(s2_result.error());
        llvm::Value* s2 = *s2_result;

        return builder.CreateSelect(cond, s1, s2);
      }
      case llvm::Instruction::Load: {
        if (pattern.operands.size() != 1 || pattern.operands[0].operand_index != 0) return EmitError(emit_name, "unsupported opcode or invalid operands");
        auto ptr_result = ResolveEmitOperand(pattern.operands[0], result_type->getPointerTo(), context, emit_name, consumed_captures);
        if (!ptr_result) return std::unexpected(ptr_result.error());
        llvm::Value* ptr = *ptr_result;
        return builder.CreateLoad(result_type, ptr);
      }
      case llvm::Instruction::Store: {
        if (pattern.operands.size() != 2 || pattern.operands[0].operand_index != 0 || pattern.operands[1].operand_index != 1) return EmitError(emit_name, "unsupported opcode or invalid operands");
        auto val_result = ResolveEmitOperand(pattern.operands[0], result_type, context, emit_name, consumed_captures);
        if (!val_result) return std::unexpected(val_result.error());
        llvm::Value* val = *val_result;
        auto ptr_result = ResolveEmitOperand(pattern.operands[1], result_type->getPointerTo(), context, emit_name, consumed_captures);
        if (!ptr_result) return std::unexpected(ptr_result.error());
        llvm::Value* ptr = *ptr_result;
        return builder.CreateStore(val, ptr);
      }
      case llvm::Instruction::GetElementPtr: {
        if (pattern.operands.empty() || pattern.operands[0].operand_index != 0) return EmitError(emit_name, "unsupported opcode or invalid operands");
        auto ptr_result = ResolveEmitOperand(pattern.operands[0], result_type->getPointerTo(), context, emit_name, consumed_captures);
        if (!ptr_result) return std::unexpected(ptr_result.error());
        llvm::Value* ptr = *ptr_result;
        std::vector<llvm::Value*> indices;
        for (size_t i = 1; i < pattern.operands.size(); i++) {
          auto idx_result = ResolveEmitOperand(pattern.operands[i], llvm::Type::getInt32Ty(module.getContext()), context, emit_name, consumed_captures);
          if (!idx_result) return std::unexpected(idx_result.error());
          indices.push_back(*idx_result);
        }
        return builder.CreateGEP(result_type, ptr, indices);
      }
      case llvm::Instruction::Alloca: {
        if (pattern.operands.size() > 1) return EmitError(emit_name, "unsupported opcode or invalid operands");
        llvm::Value* size = nullptr;
        if (pattern.operands.size() == 1) {
          auto size_result = ResolveEmitOperand(pattern.operands[0], llvm::Type::getInt64Ty(module.getContext()), context, emit_name, consumed_captures);
          if (!size_result) return std::unexpected(size_result.error());
          size = *size_result;
        }
        return builder.CreateAlloca(result_type, size);
      }
      default:
        return EmitError(emit_name, "unsupported opcode or invalid operands");
    }
  }
  if (pattern.cast_opcode.has_value() && !pattern.cast_opcode->empty()) {
    auto [dxil_op, llvm_op] = ResolveOpCode(*pattern.cast_opcode);
    if (llvm_op.has_value()) {
      if (pattern.operands.size() != 1 || pattern.operands[0].operand_index != 0) return EmitError(emit_name, "unsupported opcode or invalid operands");
      if (!llvm::Instruction::isCast(*llvm_op)) return EmitError(emit_name, "cast_opcode must name a cast instruction");
      auto source_result = resolve_cast_source();
      if (!source_result) return std::unexpected(source_result.error());
      llvm::Value* source = *source_result;
      if (!llvm::CastInst::castIsValid(static_cast<llvm::Instruction::CastOps>(*llvm_op), source, result_type)) return EmitError(emit_name, "invalid cast source/result types");
      return builder.CreateCast(static_cast<llvm::Instruction::CastOps>(*llvm_op), source, result_type);
    }
  }
  return EmitError(emit_name, "unsupported opcode or invalid operands");
}

std::expected<EmitResult, std::string> EmitInstruction(const EmitPattern& pattern, EmitContext& context,
                                                       std::vector<std::pair<std::string, llvm::Value*>>* consumed) {
  auto* block = context.builder.GetInsertBlock();
  if (block == nullptr) return std::unexpected("emit requires an insertion block");
  const auto insertion = context.builder.GetInsertPoint();
  llvm::Instruction* previous = insertion == block->begin() ? nullptr : &*std::prev(insertion);
  llvm::Instruction* next = insertion == block->end() ? nullptr : &*insertion;
  std::optional<EmitPattern> normalized;
  if (!std::ranges::is_sorted(pattern.operands, {}, &EmitOperand::operand_index)) {
    normalized = pattern;
    std::ranges::sort(normalized->operands, {}, &EmitOperand::operand_index);
  }
  auto value = ResolveEmitPatternValue(normalized.has_value() ? *normalized : pattern, context, consumed);
  if (!value) return std::unexpected(value.error());
  EmitResult result;
  if (*value != nullptr && !(*value)->getType()->isVoidTy()) result.value = *value;
  for (auto* instruction = previous != nullptr ? previous->getNextNode() : (block->empty() ? nullptr : &block->front());
       instruction != next && instruction != nullptr; instruction = instruction->getNextNode()) {
    result.instructions.push_back(instruction);
  }
  if ((!pattern.name.empty() || !pattern.replace_captured.empty()) && result.value == nullptr) {
    return std::unexpected("emit cannot name or replace a void result");
  }
  return result;
}

bool EmitWritesOutput(const EmitPattern& pattern, const dxp::ValidationContext& context) {
  if (!pattern.function_name.empty()) return context.output_writing_functions.contains(pattern.function_name);
  if (!pattern.opcode.has_value()) return false;
  auto [op, llvm_op] = ResolveOpCode(*pattern.opcode);
  return op == hlsl::OP::OpCode::StoreOutput || op == hlsl::OP::OpCode::StoreVertexOutput || op == hlsl::OP::OpCode::StorePrimitiveOutput || op == hlsl::OP::OpCode::RawBufferStore || op == hlsl::OP::OpCode::BufferStore || op == hlsl::OP::OpCode::TextureStore;
}

std::expected<void, std::string> ValidateEmitPattern(const EmitPattern& pattern, const dxp::ValidationContext& context) {
  const auto fail = [&](const std::string& message) -> std::expected<void, std::string> {
    return std::unexpected("emit '" + pattern.name + "': " + message);
  };
  if (pattern.opcode.has_value() && pattern.cast_opcode.has_value()) return fail("opcode and cast_opcode cannot be combined");
  std::unordered_set<uint32_t> indices;
  for (const auto& operand : pattern.operands) {
    if (!indices.insert(operand.operand_index).second) return fail("duplicate operand index " + std::to_string(operand.operand_index));
  }
  if (!pattern.function_name.empty()) {
    if (pattern.opcode.has_value() || pattern.cast_opcode.has_value() || !pattern.capture.empty() || pattern.extract.has_value() || pattern.result_component_type.has_value()) {
      return fail("function cannot be combined with opcode, cast_opcode, capture, extract, or result_component_type");
    }
    auto signature = context.function_signatures.find(pattern.function_name);
    if (signature == context.function_signatures.end()) return fail("unknown function '" + pattern.function_name + "'; functions must be declared earlier");
    const auto& params = signature->second.params;
    if (pattern.operands.size() != params.size()) return fail("function argument count mismatch");
    for (const auto& operand : pattern.operands) {
      if (operand.operand_index >= params.size()) return fail("function argument index out of range");
    }
    if (signature->second.return_type.kind == FunctionType::Kind::Void && (!pattern.name.empty() || !pattern.replace_captured.empty())) {
      return fail("cannot name or replace a void function result");
    }
    return {};
  }
  if (pattern.extract.has_value()) {
    if (pattern.opcode.has_value() || pattern.cast_opcode.has_value() || !pattern.capture.empty() || !pattern.operands.empty() || pattern.result_component_type.has_value()) {
      return fail("extract cannot be combined with other emit modes");
    }
    if (pattern.extract->aggregate.empty() || pattern.extract->indices.empty()) return fail("extract requires aggregate and indices");
  }
  if (!pattern.capture.empty() && (pattern.opcode.has_value() || pattern.cast_opcode.has_value() || !pattern.operands.empty())) {
    return fail("capture cannot be combined with other emit modes");
  }
  if (!pattern.opcode.has_value() && !pattern.cast_opcode.has_value() && pattern.capture.empty() && !pattern.extract.has_value() && pattern.operands.empty()) {
    return fail("no opcode or value specified");
  }
  const auto is_float = [](dxp::ComponentType type) {
    return type == dxp::ComponentType::F16 || type == dxp::ComponentType::F32 || type == dxp::ComponentType::F64;
  };
  const auto is_int = [](dxp::ComponentType type) {
    return type == dxp::ComponentType::I1 || type == dxp::ComponentType::I8 || type == dxp::ComponentType::U8 || type == dxp::ComponentType::I16 || type == dxp::ComponentType::U16 || type == dxp::ComponentType::I32 || type == dxp::ComponentType::U32 || type == dxp::ComponentType::I64 || type == dxp::ComponentType::U64;
  };
  if (pattern.result_component_type.has_value() && !is_float(*pattern.result_component_type) && !is_int(*pattern.result_component_type)) return fail("unsupported result_component_type");
  const auto opcode = pattern.opcode.has_value() ? pattern.opcode : pattern.cast_opcode;
  if (opcode.has_value()) {
    auto [dxil_op, llvm_op] = ResolveOpCode(*opcode);
    if (!dxil_op.has_value() && !llvm_op.has_value()) return fail("opcode '" + *opcode + "' is not a known DXIL or LLVM opcode");
    if (llvm_op.has_value() && llvm::Instruction::isTerminator(*llvm_op)) return fail("terminators cannot be emitted in an instruction stream");
    if (pattern.cast_opcode.has_value() && (!llvm_op.has_value() || !llvm::Instruction::isCast(*llvm_op))) return fail("cast_opcode must name a cast instruction");
    if (llvm_op.has_value()) {
      const bool floating = llvm_op == llvm::Instruction::FAdd || llvm_op == llvm::Instruction::FSub || llvm_op == llvm::Instruction::FMul || llvm_op == llvm::Instruction::FDiv || llvm_op == llvm::Instruction::FRem;
      const bool integer = llvm_op == llvm::Instruction::Add || llvm_op == llvm::Instruction::Sub || llvm_op == llvm::Instruction::Mul || llvm_op == llvm::Instruction::UDiv || llvm_op == llvm::Instruction::SDiv || llvm_op == llvm::Instruction::URem || llvm_op == llvm::Instruction::SRem || llvm_op == llvm::Instruction::Shl || llvm_op == llvm::Instruction::LShr || llvm_op == llvm::Instruction::AShr || llvm_op == llvm::Instruction::And || llvm_op == llvm::Instruction::Or || llvm_op == llvm::Instruction::Xor;
      const auto valid = [&](std::optional<dxp::ComponentType> type) { return !type.has_value() || (!floating && !integer) || (floating ? is_float(*type) : is_int(*type)); };
      if (!valid(pattern.result_component_type)) return fail("opcode type mismatch for result_component_type");
      for (const auto& operand : pattern.operands) {
        if (!valid(operand.component_type)) return fail("opcode type mismatch for operand component_type");
      }
    }
  }
  return {};
}

}  // namespace dxp::sm6::step::common
