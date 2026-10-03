#include <atomic>
#include <iostream>
#include <thread>
#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/sm6/Recipe.hpp"
#include "dxp/sm6/step/common/Emit.hpp"
#include "dxp/sm6/step/DeclareFunctionStep_impl.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {
using namespace dxp::sm6::step;
using dxp::sm6::step::common::LlvmTypeFor;
int failures = 0;

bool Check(bool value, const std::string& label) {
  if (!value) {
    std::cerr << label << '\n';
    ++failures;
  }
  return value;
}

const std::string kScale = R"(
  - kind: declare_function
    name: scale
    params: [{name: value, type: F32}]
    return_type: F32
    emit:
      - opcode: fmul
        name: scaled
        result_component_type: F32
        operands:
          - {index: 1, kind: constant, constant_float_values: [factor]}
          - {index: 0, capture: value}
    return: {capture: scaled}
)";

std::string Rewrite(const std::string& name, const std::string& mode = "replace", const std::string& opcode = "Frc") {
  return "  - kind: apply_rule\n    name: patch\n    rewrite_mode: " + mode + R"(
    rule:
      name: patch_rule
      match:
        - opcode: )"
         + opcode + R"(
          capture: original
          operands: [{index: 1, capture: input}]
      emit:
        - function: )"
         + name + R"(
          name: result
          operands: [{index: 0, capture: input}]
          replace_captured: original
)";
}

std::expected<dxp::RecipeReport, std::string> Run(const std::string& yaml, const std::vector<uint8_t>& shader) {
  auto recipe = dxp::sm6::Recipe::ParseFromText(yaml, "function-test");
  if (!recipe) return std::unexpected(recipe.error());
  return recipe->Execute(shader);
}

void ExpectError(const std::string& yaml, const std::vector<uint8_t>& shader, const std::string& expected) {
  auto result = Run(yaml, shader);
  Check(!result && result.error().find(expected) != std::string::npos,
        "Expected '" + expected + "', got " + (result ? "success" : result.error()));
}

bool Load(const std::vector<uint8_t>& bytes, dxp::sm6::ExecutionContext& context) {
  auto loaded = dxp::sm6::ShaderProgram::FromBytes(bytes, context.program);
  return Check(loaded.has_value(), loaded ? "" : loaded.error());
}

unsigned CountCalls(llvm::Function& function, const std::string& callee) {
  unsigned count = 0;
  for (auto& block : function) {
    for (auto& instruction : block) {
      auto* call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (call != nullptr && call->getCalledFunction() != nullptr && call->getCalledFunction()->getName() == callee) ++count;
    }
  }
  return count;
}

void TestNestedAndCleanup(const std::vector<uint8_t>& shader) {
  const std::string declarations = kScale + R"(
  - kind: declare_function
    name: wrapper
    params: [{name: value, type: F32}]
    return_type: F32
    emit:
      - function: scale
        name: wrapped
        operands: [{capture: value}]
    return: {capture: wrapped}
  - kind: declare_function
    name: unused
    emit:
      - opcode: WaveGetLaneCount
)";
  const std::string yaml = "env: {factor: 2.0}\nsteps:\n" + declarations + R"(
  - kind: apply_rule
    name: patch
    rule:
      name: patch_rule
      match:
        - opcode: Frc
          capture: original
          operands: [{index: 1, capture: input}]
      emit:
        - function: wrapper
          name: first
          operands: [{capture: input}]
        - function: wrapper
          name: second
          operands: [{capture: first}]
          replace_captured: original
)";
  auto result = Run(yaml, shader);
  if (!Check(result.has_value(), result ? "" : result.error())) return;
  Check(result->modified, "Function calls must modify the shader");
  auto* declared = std::get_if<dxp::DeclareFunctionResults>(&result->steps.front().results);
  Check(declared != nullptr && declared->functions_added == 1 && declared->instruction_count == 2, "Declaration report must describe the function body");
  dxp::sm6::ExecutionContext output;
  if (!Load(result->output_bytes, output)) return;
  auto* module = output.program.GetModule();
  auto* scale = module->getFunction("__dxp_fn.scale");
  auto* wrapper = module->getFunction("__dxp_fn.wrapper");
  if (!Check(scale != nullptr && wrapper != nullptr, "Serialized DXIL must retain both reachable helper definitions")) return;
  Check(scale->hasInternalLinkage() && scale->arg_size() == 1 && scale->getReturnType()->isFloatTy(), "Helper must have the declared internal scalar signature");
  Check(CountCalls(*output.program.GetEntryFunction(), "__dxp_fn.wrapper") == 2, "Multiple calls must reuse one definition");
  Check(CountCalls(*wrapper, "__dxp_fn.scale") == 1, "Nested direct call must survive serialization");
  Check(module->getFunction("__dxp_fn.unused") == nullptr && module->getFunction("dx.op.waveGetLaneCount") == nullptr, "Unused helpers and their intrinsics must be removed");
  Check(output.program.Verify().has_value(), "Reloaded module must verify");

  auto declarations_only = Run("env: {factor: 2.0}\nsteps:\n" + declarations, shader);
  Check(declarations_only && !declarations_only->modified && declarations_only->output_bytes == shader, "Declaration-only recipe must pass through unchanged");
  auto no_match = Run("env: {factor: 2.0}\nsteps:\n" + declarations + Rewrite("scale", "replace", "Sin"), shader);
  Check(no_match && !no_match->modified && no_match->output_bytes == shader, "Unused declarations after no match must pass through unchanged");

  for (const auto& mode : {"before", "after"}) {
    auto inserted = Run("env: {factor: 2.0}\nsteps:\n" + kScale + Rewrite("scale", mode), shader);
    Check(inserted && inserted->modified, std::string("Function call insertion failed in ") + mode + ": " + (inserted ? "" : inserted.error()));
  }
}

void TestResourceHelpers(const std::vector<uint8_t>& shader) {
  const std::string yaml = R"(
steps:
  - kind: add_resource
    name: constants
    cbuffers:
      - handle: frame
        size: 16
        space: 7
        register_index: 3
        fields: [{name: values, type: F32, width: 4, offset: 0}]
  - kind: declare_function
    name: read_frame
    return_type: F32
    emit:
      - opcode: CBufferLoadLegacy
        result_component_type: F32
        name: a
        operands:
          - {index: 1, kind: resource, handle: frame}
          - {index: 2, kind: constant, constant_int_values: [0]}
      - name: ax
        extract: {aggregate: a, indices: [0]}
      - opcode: CBufferLoadLegacy
        result_component_type: F32
        name: b
        operands:
          - {index: 1, kind: resource, handle: frame}
          - {index: 2, kind: constant, constant_int_values: [0]}
      - name: by
        extract: {aggregate: b, indices: [1]}
      - opcode: fadd
        name: total
        result_component_type: F32
        operands: [{index: 0, capture: ax}, {index: 1, capture: by}]
    return: {capture: total}
  - kind: apply_rule
    name: patch
    rule:
      name: patch_rule
      match: [{opcode: Frc, capture: original}]
      emit: [{function: read_frame, replace_captured: original}]
)";
  auto result = Run(yaml, shader);
  if (!Check(result.has_value(), result ? "" : result.error())) return;
  dxp::sm6::ExecutionContext output;
  if (!Load(result->output_bytes, output)) return;
  auto* helper = output.program.GetModule()->getFunction("__dxp_fn.read_frame");
  if (!Check(helper != nullptr, "Resource helper must survive serialization")) return;
  Check(CountCalls(*helper, "dx.op.createHandleFromBinding") == 1 && CountCalls(*helper, "dx.op.annotateHandle") == 1, "Resource handles must be created and cached inside the helper");
  for (auto& block : *helper) {
    for (auto& instruction : block) {
      for (auto& operand : instruction.operands()) {
        auto* value = llvm::dyn_cast<llvm::Instruction>(operand.get());
        Check(value == nullptr || value->getParent()->getParent() == helper, "Helper must not consume entry-function SSA instructions");
      }
    }
  }
}

void TestVoidHelpers(const std::vector<uint8_t>& shader) {
  const std::string definitions = R"(
  - kind: add_resource
    name: resources
    uavs: [{handle: writes, kind: RawBuffer, element_type: U32, vector_width: 1, space: 7, register_index: 4}]
  - kind: declare_function
    name: write_value
    params: [{name: value, type: U32}]
    emit:
      - opcode: RawBufferStore
        result_component_type: U32
        operands:
          - {index: 1, kind: resource, handle: writes}
          - {index: 2, kind: constant, constant_int_values: [0]}
          - {index: 3, kind: undefined}
          - {index: 4, capture: value}
          - {index: 5, kind: undefined}
          - {index: 6, kind: undefined}
          - {index: 7, kind: undefined}
          - {index: 8, kind: constant, constant_int_values: [1]}
          - {index: 9, kind: constant, constant_int_values: [4]}
  - kind: declare_function
    name: write_wrapper
    params: [{name: value, type: U32}]
    emit: [{function: write_value, operands: [{capture: value}]}]
)";
  const std::string insertion = R"(
  - kind: apply_rule
    name: patch
    rewrite_mode: before
    rule:
      name: patch_rule
      match: [{opcode: ret}]
      emit: [{function: write_wrapper, operands: [{kind: constant, constant_int_values: [7]}]}]
)";
  auto result = Run("steps:\n" + definitions + insertion, shader);
  if (!Check(result.has_value(), result ? "" : result.error())) return;
  dxp::sm6::ExecutionContext output;
  if (!Load(result->output_bytes, output)) return;
  auto* helper = output.program.GetModule()->getFunction("__dxp_fn.write_wrapper");
  Check(helper != nullptr && helper->getReturnType()->isVoidTy() && CountCalls(*helper, "__dxp_fn.write_value") == 1, "Void helper must retain its nested side-effect call");

  dxp::sm6::ExecutionContext input;
  if (!Load(shader, input)) return;
  std::string store_opcode;
  for (auto& block : *input.program.GetEntryFunction()) {
    for (auto& instruction : block) {
      if (hlsl::OP::IsDxilOpFuncCallInst(&instruction)) {
        auto op = hlsl::OP::GetDxilOpFuncCallInst(&instruction);
        if (op == hlsl::OP::OpCode::RawBufferStore || op == hlsl::OP::OpCode::BufferStore) store_opcode = hlsl::OP::GetOpCodeName(op);
      }
    }
  }
  if (!Check(!store_opcode.empty(), "Fixture must have a live buffer store")) return;
  std::string replace = insertion;
  replace.replace(replace.find("rewrite_mode: before"), std::string("rewrite_mode: before").size(), "rewrite_mode: replace");
  replace.replace(replace.find("opcode: ret"), std::string("opcode: ret").size(), "opcode: " + store_opcode);
  auto replaced = Run("steps:\n" + definitions + replace, shader);
  Check(replaced && replaced->modified, "Output-writing nested helper must satisfy replacement validation: " + (replaced ? "" : replaced.error()));
}

void TestScalarCpp(const std::vector<uint8_t>& shader) {
  dxp::sm6::ExecutionContext context;
  if (!Load(shader, context)) return;
  const std::vector<dxp::ComponentType> types{dxp::ComponentType::I1, dxp::ComponentType::I8, dxp::ComponentType::U8, dxp::ComponentType::I16, dxp::ComponentType::U16, dxp::ComponentType::I32, dxp::ComponentType::U32, dxp::ComponentType::I64, dxp::ComponentType::U64, dxp::ComponentType::F16, dxp::ComponentType::F32, dxp::ComponentType::F64};
  const std::vector<std::string> type_names{"I1", "I8", "U8", "I16", "U16", "I32", "U32", "I64", "U64", "F16", "F32", "F64"};
  for (size_t i = 0; i < types.size(); ++i) {
    auto yaml_identity = Run("steps: [{kind: declare_function, name: identity, params: [{name: value, type: " + type_names[i] + "}], return_type: " + type_names[i] + ", return: {capture: value}}]", shader);
    Check(yaml_identity && yaml_identity->output_bytes == shader, "YAML scalar identity must construct and pass through: " + type_names[i]);
    DeclareFunctionStep declaration;
    declaration.name = "identity_" + std::to_string(i);
    declaration.signature.params = {{"value", FunctionType{types[i]}}};
    declaration.signature.return_type = FunctionType{types[i]};
    declaration.return_value = EmitOperand{.capture = "value"};
    auto defined = Execute(declaration, context);
    if (!Check(defined.has_value(), defined ? "" : defined.error())) continue;
    auto* function = context.functions.at(declaration.name);
    auto* ret = llvm::dyn_cast<llvm::ReturnInst>(function->getEntryBlock().getTerminator());
    Check(ret != nullptr && ret->getReturnValue() == &*function->arg_begin(), "C++ identity return must reference its own parameter");
    Check(function->getReturnType() == LlvmTypeFor(types[i], function->getContext()), "C++ scalar signature must map to its LLVM type");
    if (i == 0) {
      auto collision = Execute(declaration, context);
      Check(!collision && collision.error().find("symbol collision") != std::string::npos, "Duplicate module symbol must report an error");
    }
  }
  Check(context.program.Verify().has_value(), "All scalar C++ definitions must pass LLVM verification");

  DeclareFunctionStep constant;
  constant.name = "constant";
  constant.signature.return_type = FunctionType{dxp::ComponentType::F32};
  constant.return_value = EmitOperand{.kind = OperandKind::Constant, .constant_float_values = {0.5}};
  Check(Execute(constant, context).has_value(), "Empty body must support a constant return");
  context.program.PruneDeadCode();
  context.program.PruneDeadCode();
  Check(context.program.GetModule()->getFunction("__dxp_fn.constant") == nullptr, "Repeated cleanup must safely remove uncalled C++ helpers");

  DeclareFunctionStep identity;
  identity.name = "identity";
  identity.signature.params = {{"value", FunctionType{dxp::ComponentType::F32}}};
  identity.signature.return_type = FunctionType{dxp::ComponentType::F32};
  identity.return_value = EmitOperand{.capture = "value"};
  Rule rule;
  rule.name = "cpp_rule";
  rule.match_patterns = {InstructionPattern{.opcode = "Frc", .capture_name = "old", .operand_patterns = {OperandPattern{.operand_index = 1, .capture_name = "input"}}}};
  rule.emit_patterns = {EmitPattern{.operands = {EmitOperand{.capture = "input"}}, .replace_captured = "old", .function_name = "identity"}};
  dxp::sm6::Recipe recipe;
  recipe.AddStep(identity).AddStep(ApplyRuleStep{"cpp_patch", true, RewriteKind::Replace, std::nullopt, rule});
  auto result = recipe.Execute(shader);
  Check(result && result->modified, "Public C++ recipe must serialize real function calls: " + (result ? "" : result.error()));
}

void TestLocalStorageAndPatchRoot(const std::vector<uint8_t>& shader) {
  const std::string body = R"(
  - kind: declare_function
    name: memory
    params: [{name: value, type: F32}]
    return_type: F32
    emit:
      - {opcode: alloca, name: slot, result_component_type: F32}
      - opcode: store
        result_component_type: F32
        operands: [{index: 0, capture: value}, {index: 1, capture: slot}]
      - opcode: load
        name: loaded
        result_component_type: F32
        operands: [{capture: slot}]
    return: {capture: loaded}
)";
  auto result = Run("steps:\n" + body + Rewrite("memory"), shader);
  Check(result && result->modified, "Helper-local store must emit successfully without an SSA result: " + (result ? "" : result.error()));

  // A helper reached only from the hull patch-constant phase must survive.
  std::vector<uint8_t> hull;
  if (!Check(ReadFile((RepoRootPath() / "tests/shaders/sm6_function_roots.hs_6_6.cso").string(), hull), "Hull fixture must load")) return;
  dxp::sm6::ExecutionContext context;
  if (!Load(hull, context)) return;
  DeclareFunctionStep declaration;
  declaration.name = "patch_helper";
  if (!Check(Execute(declaration, context).has_value(), "Patch root helper declaration failed")) return;
  auto* module = context.program.GetModule();
  auto* root = context.program.GetDxilModule()->GetPatchConstantFunction();
  if (!Check(root != nullptr, "Hull fixture must contain a patch-constant root")) return;
  llvm::IRBuilder<> builder(root->getEntryBlock().getTerminator());
  builder.CreateCall(context.functions.at("patch_helper"));
  context.program.PruneDeadCode();
  Check(module->getFunction("__dxp_fn.patch_helper") != nullptr, "Patch-constant entry root must retain its reachable helpers");
  Check(context.program.Verify().has_value(), "Patch root helper must retain valid LLVM IR");
  auto serialized = context.program.Serialize();
  if (!Check(serialized.has_value(), "Hull helper must pass container validation: " + (serialized ? "" : serialized.error()))) return;
  dxp::sm6::ExecutionContext reloaded;
  if (!Load(*serialized, reloaded)) return;
  Check(reloaded.program.GetModule()->getFunction("__dxp_fn.patch_helper") != nullptr, "Serialized hull shader must retain the patch-only helper");
}

void TestErrors(const std::vector<uint8_t>& shader) {
  ExpectError("steps: [{kind: declare_function, name: bad, return_type: F32}]", shader, "requires a return operand");
  ExpectError("steps: [{kind: declare_function, name: bad, return: {kind: constant, constant_int_values: [1]}}]", shader, "void function");
  ExpectError("steps: [{kind: declare_function, name: bad, return_type: Invalid, return: {capture: x}}]", shader, "unsupported return_type");
  ExpectError("steps: [{kind: declare_function, name: bad, params: [{name: x, type: Invalid}]}]", shader, "supported scalar type");
  ExpectError("steps: [{kind: declare_function, name: bad, params: [{name: x, type: F32}, {name: x, type: F32}]}]", shader, "duplicate parameter");
  ExpectError("steps: [{kind: declare_function, name: bad, return_type: F32, return: {capture: shader_capture}}]", shader, "not a parameter");
  ExpectError("steps: [{kind: declare_function, name: bad, emit: [{function: bad}]}]", shader, "declared earlier");
  ExpectError("steps: [{kind: declare_function, name: a, emit: [{function: b}]}, {kind: declare_function, name: b}]", shader, "declared earlier");
  ExpectError("steps: [{kind: declare_function, name: bad, emit: [{capture: x, replace_captured: x}]}]", shader, "only valid in apply_rule");
  ExpectError("steps: [{kind: declare_function, name: bad, params: [{name: x, type: F32}], emit: [{capture: x, name: x}]}]", shader, "duplicate local name");
  ExpectError("steps: [{kind: declare_function, name: bad, emit: [{opcode: ret}]}]", shader, "terminators");
  ExpectError("steps: [{kind: declare_function, name: bad, emit: [{opcode: fmul, cast_opcode: fptrunc}]}]", shader, "cannot be combined");
  ExpectError("steps: [{kind: declare_function, name: bad, emit: [{opcode: fmul, operands: [{index: 0}, {index: 1}]}]}]", shader, "requires result_component_type");
  ExpectError("steps: [{kind: declare_function, name: bad, return_type: F32, return: {kind: resource, handle: missing}}]", shader, "resource 'missing'");
  ExpectError("env: {factor: 2.0}\nsteps:\n" + kScale + Rewrite("missing"), shader, "declared earlier");
  const std::string prefix = "env: {factor: 2.0}\nsteps:\n" + kScale;
  std::string call = Rewrite("scale");
  call.replace(call.find("operands: [{index: 0, capture: input}]"), std::string("operands: [{index: 0, capture: input}]").size(), "operands: []");
  ExpectError(prefix + call, shader, "argument count mismatch");
  call = Rewrite("scale");
  call.replace(call.find("index: 0, capture: input"), std::string("index: 0").size(), "index: 1");
  ExpectError(prefix + call, shader, "argument index out of range");
  ExpectError("env: {factor: 2.0}\nsteps:\n" + kScale + R"(
  - kind: declare_function
    name: mismatch
    params: [{name: value, type: I32}]
    return_type: F32
    emit: [{function: scale, name: result, operands: [{capture: value}]}]
    return: {capture: result}
)",
              shader, "type mismatch");
  ExpectError("steps: [{kind: declare_function, name: bad, params: [{name: value, type: I32}], return_type: F32, return: {capture: value}}]", shader, "type mismatch");
  ExpectError("steps: [{kind: declare_function, name: empty}, {kind: declare_function, name: bad, emit: [{function: empty, name: result}]}]", shader, "void function result");
  ExpectError("steps: [{kind: declare_function, name: empty}, {kind: declare_function, name: bad, emit: [{function: empty, opcode: Frc}]}]", shader, "cannot be combined");
  std::string skipped = kScale;
  skipped.insert(skipped.find("    params:"), "    condition: {is: enabled}\n");
  ExpectError("env: {enabled: false, factor: 2.0}\nsteps:\n" + skipped + Rewrite("scale"), shader, "was not executed");
  std::string gated = Rewrite("scale");
  gated.insert(gated.find("    rule:"), "    condition: {is: scale}\n");
  auto gate_result = Run("env: {enabled: false, factor: 2.0}\nsteps:\n" + skipped + gated, shader);
  Check(gate_result && !gate_result->modified && gate_result->output_bytes == shader, "Gating callers on declaration state must skip unavailable functions");
  ExpectError("steps: [{kind: declare_template, name: obsolete}]", shader, "");
}

void TestConcurrent(const std::vector<uint8_t>& shader) {
  const std::string yaml = "env: {factor: 2.0}\nsteps:\n" + kScale + Rewrite("scale");
  auto baseline = Run(yaml, shader);
  auto recipe = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!Check(baseline && recipe, "Concurrency setup failed")) return;
  std::atomic<bool> start{false};
  std::atomic<bool> failed{false};
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([&] {
      while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
      for (int iteration = 0; iteration < 3; ++iteration) {
        auto result = recipe->Execute(shader);
        if (!result || result->output_bytes != baseline->output_bytes) failed.store(true);
      }
    });
  }
  start.store(true, std::memory_order_release);
  for (auto& thread : threads) thread.join();
  Check(!failed.load(), "Concurrent execution of an unvalidated function recipe must be deterministic");
}

}  // namespace

int main() {
  std::vector<uint8_t> shader;
  if (!ReadFile((RepoRootPath() / "tests/shaders/sm6_rule_patterns.cs_6_6.cso").string(), shader)) return 1;
  TestNestedAndCleanup(shader);
  TestResourceHelpers(shader);
  TestVoidHelpers(shader);
  TestScalarCpp(shader);
  TestLocalStorageAndPatchRoot(shader);
  TestErrors(shader);
  TestConcurrent(shader);
  return failures == 0 ? 0 : 1;
}
