#include "dxp/sm6/step/AddResourceStep_impl.hpp"
#include "dxp/sm6/step/common/Emit.hpp"
#include "dxp/sm6/step/DeclareFunctionStep_impl.hpp"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

namespace dxp::sm6::step {
using common::EmitContext;
using common::EmitInstruction;
using common::EmitWritesOutput;
using common::ResolveEmitOperand;
using common::ResolveFunctionType;
using common::ValidateEmitPattern;

namespace {

bool IsScalarType(const FunctionType& type) {
  if (type.kind != FunctionType::Kind::Scalar) return false;
  switch (type.component_type) {
    case dxp::ComponentType::I1:
    case dxp::ComponentType::I8:
    case dxp::ComponentType::U8:
    case dxp::ComponentType::I16:
    case dxp::ComponentType::U16:
    case dxp::ComponentType::I32:
    case dxp::ComponentType::U32:
    case dxp::ComponentType::I64:
    case dxp::ComponentType::U64:
    case dxp::ComponentType::F16:
    case dxp::ComponentType::F32:
    case dxp::ComponentType::F64: return true;
    default:                      return false;
  }
}

}  // namespace

std::expected<void, std::string> Validate(const DeclareFunctionStep& step, dxp::ValidationContext& context) {
  const auto fail = [&](const std::string& message) -> std::expected<void, std::string> {
    return std::unexpected("declare_function '" + step.name + "': " + message);
  };
  if (step.name.empty()) return fail("requires a name");
  if (!context.names.insert(step.name).second) return fail("duplicate name");
  std::unordered_set<std::string> locals;
  for (const auto& param : step.signature.params) {
    if (param.name.empty() || !locals.insert(param.name).second) return fail("empty or duplicate parameter name '" + param.name + "'");
    if (!IsScalarType(param.type)) return fail("parameter '" + param.name + "' requires a supported scalar type");
  }
  const bool returns_void = step.signature.return_type.kind == FunctionType::Kind::Void;
  if (!returns_void && !IsScalarType(step.signature.return_type)) return fail("unsupported return_type");
  if (returns_void == step.return_value.has_value()) return fail(returns_void ? "void function cannot have a return operand" : "scalar function requires a return operand");
  const auto check_value = [&](const std::string& name) -> std::expected<void, std::string> {
    if (!locals.contains(name)) return fail("value '" + name + "' is not a parameter or earlier body result");
    return {};
  };
  const auto check_operand = [&](const EmitOperand& operand) -> std::expected<void, std::string> {
    if (operand.capture.has_value()) {
      if (auto checked = check_value(*operand.capture); !checked) return checked;
    }
    if (operand.instruction) {
      if (auto checked = check_value((**operand.instruction).capture_name); !checked) return checked;
    }
    if (operand.kind == OperandKind::Resource && !context.handles.contains(operand.handle)) return fail("resource '" + operand.handle + "' must be declared earlier");
    return {};
  };
  bool writes_output = false;
  for (const auto& emit : step.emits) {
    if (!emit.replace_captured.empty()) return fail("replace_captured is only valid in apply_rule");
    if (auto checked = ValidateEmitPattern(emit, context); !checked) return fail(checked.error());
    if (!emit.capture.empty()) {
      if (auto checked = check_value(emit.capture); !checked) return checked;
    }
    if (emit.extract.has_value()) {
      if (auto checked = check_value(emit.extract->aggregate); !checked) return checked;
      for (const auto& index : emit.extract->indices) {
        if (const auto* captured = std::get_if<CaptureExtractIndex>(&index)) {
          if (auto checked = check_value(captured->capture); !checked) return checked;
        }
      }
    }
    for (const auto& operand : emit.operands) {
      if (auto checked = check_operand(operand); !checked) return checked;
    }
    if (!emit.name.empty() && !locals.insert(emit.name).second) return fail("duplicate local name '" + emit.name + "'");
    writes_output = writes_output || EmitWritesOutput(emit, context);
  }
  if (step.return_value.has_value()) {
    if (step.return_value->operand_index != 0) return fail("return operand index must be zero");
    if (auto checked = check_operand(*step.return_value); !checked) return checked;
  }
  if (auto checked = ValidateCondition<DeclareFunctionStep::Results>(step.condition, context); !checked) return checked;
  context.function_signatures.emplace(step.name, step.signature);
  if (writes_output) context.output_writing_functions.insert(step.name);
  return {};
}

std::expected<DeclareFunctionStep::Results, std::string> Execute(const DeclareFunctionStep& step, ExecutionContext& context) {
  auto* module = context.program.GetModule();
  auto* dxil = context.program.GetDxilModule();
  if (module == nullptr || dxil == nullptr) return std::unexpected("declare_function: missing module state");
  auto* return_type = ResolveFunctionType(step.signature.return_type, module->getContext());
  if (return_type == nullptr) return std::unexpected("declare_function: unsupported return type");
  if (return_type->isVoidTy() == step.return_value.has_value()) return std::unexpected("declare_function: return operand does not agree with return type");
  std::vector<llvm::Type*> params;
  for (const auto& param : step.signature.params) {
    auto* type = ResolveFunctionType(param.type, module->getContext());
    if (type == nullptr || type->isVoidTy()) return std::unexpected("declare_function: unsupported parameter type");
    params.push_back(type);
  }
  const std::string symbol = "__dxp_fn." + step.name;
  if (module->getNamedValue(symbol) != nullptr || context.functions.contains(step.name)) return std::unexpected("declare_function '" + step.name + "': symbol collision at '" + symbol + "'");
  auto* function = llvm::Function::Create(llvm::FunctionType::get(return_type, params, false), llvm::GlobalValue::InternalLinkage, symbol, module);
  if (auto* entry = context.program.GetEntryFunction()) {
    // Preserve shader arithmetic and wave semantics without adding purity or
    // inlining assumptions. These are the DXIL-supported string attributes.
    for (const auto* name : {"fp32-denorm-mode", "waveops-include-helper-lanes"}) {
      if (entry->hasFnAttribute(name)) function->addFnAttr(name, entry->getFnAttribute(name).getValueAsString());
    }
  }
  const auto fail = [&](const std::string& error) -> std::expected<DeclareFunctionStep::Results, std::string> {
    function->eraseFromParent();
    return std::unexpected("declare_function '" + step.name + "': " + error);
  };
  auto* block = llvm::BasicBlock::Create(module->getContext(), "entry", function);
  llvm::IRBuilder<> builder(block);
  std::unordered_map<std::string, llvm::Value*> locals;
  for (auto& param : function->args()) {
    const auto& name = step.signature.params[param.getArgNo()].name;
    param.setName(name);
    locals.emplace(name, &param);
  }
  std::unordered_map<std::string, llvm::Value*> handles;
  EmitContext emission{builder, *module, *dxil, locals, &context};
  emission.resolve_function = [&](const std::string& name) -> llvm::Function* {
    auto it = context.functions.find(name);
    return it == context.functions.end() ? nullptr : it->second;
  };
  emission.resolve_resource = [&](const std::string& name) -> std::expected<llvm::Value*, std::string> {
    auto cached = handles.find(name);
    if (cached != handles.end()) return cached->second;
    auto resource = context.resource_handles.find(name);
    auto binding = context.resource_bindings.find(name);
    if (resource == context.resource_handles.end() || binding == context.resource_bindings.end()) return std::unexpected("resource '" + name + "' was not executed by an earlier add_resource step");
    const auto* desc = resource->second;
    hlsl::DxilResourceBinding dxil_binding{};
    dxil_binding.resourceClass = static_cast<uint8_t>(desc->GetClass());
    dxil_binding.rangeLowerBound = desc->GetLowerBound();
    dxil_binding.rangeUpperBound = desc->GetLowerBound() + desc->GetRangeSize() - 1;
    dxil_binding.spaceID = desc->GetSpaceID();
    auto* value = context.program.CreateResourceHandle(*desc, dxil_binding, builder);
    if (value == nullptr) return std::unexpected("cannot create function-local handle for resource '" + name + "'");
    handles.emplace(name, value);
    return value;
  };
  for (const auto& emit : step.emits) {
    auto emitted = EmitInstruction(emit, emission);
    if (!emitted) return fail(emitted.error());
    if (!emit.name.empty()) locals.emplace(emit.name, emitted->value);
  }
  if (return_type->isVoidTy()) {
    builder.CreateRetVoid();
  } else {
    auto value = ResolveEmitOperand(*step.return_value, return_type, emission, "return");
    if (!value) return fail(value.error());
    builder.CreateRet(*value);
  }
  std::string diagnostics;
  llvm::raw_string_ostream stream(diagnostics);
  if (llvm::verifyFunction(*function, &stream)) {
    stream.flush();
    return fail(diagnostics);
  }
  DeclareFunctionStep::Results result;
  result.functions_added = 1;
  for (const auto& body : *function) result.instruction_count += static_cast<uint32_t>(body.size());
  context.functions.emplace(step.name, function);
  context.program.TrackInjectedFunction(function);
  context.program_modified = true;
  context.SetState<bool>(step.name, true);
  return result;
}

std::expected<DeclareFunctionStep, std::string> DeclareFunctionData::Compile() const {
  DeclareFunctionStep step;
  step.name = name;
  step.required = required;
  step.condition = condition.Compile();
  for (const auto& param : params) step.signature.params.push_back({param.name, FunctionType{param.type}});
  if (return_type != "void") {
    auto type = std::ranges::find(glz::meta<dxp::ComponentType>::keys, return_type);
    if (type == std::end(glz::meta<dxp::ComponentType>::keys)) return std::unexpected("declare_function '" + name + "': unknown return_type '" + return_type + "'");
    const auto index = std::distance(std::begin(glz::meta<dxp::ComponentType>::keys), type);
    step.signature.return_type = FunctionType{glz::meta<dxp::ComponentType>::value[index]};
  }
  for (const auto& pattern : emit) {
    auto compiled = pattern.Compile();
    if (!compiled) return std::unexpected(compiled.error());
    step.emits.push_back(std::move(*compiled));
  }
  if (return_value.has_value()) {
    auto compiled = return_value->Compile();
    if (!compiled) return std::unexpected(compiled.error());
    step.return_value = std::move(*compiled);
  }
  return step;
}

std::string DescribeOutcome(const DeclareFunctionStep&, const DeclareFunctionStep::Results& results, const ExecutionContext&) {
  return "declared function with " + std::to_string(results.instruction_count) + " instruction(s)";
}

static_assert(RecipeStep<DeclareFunctionStep>);
static_assert(ExecutableStep<DeclareFunctionStep, ExecutionContext>);

}  // namespace dxp::sm6::step
