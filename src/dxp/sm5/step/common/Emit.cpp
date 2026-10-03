#include "dxp/sm5/step/common/Emit.hpp"
#include <cstring>
#include <iterator>
#include <type_traits>
#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/sm5/step/common/Rule_impl.hpp"

namespace dxp::sm5::step::common {
using namespace dxp::sm5::model;
namespace {
constexpr uint32_t kBitsPerDword = 32U;
constexpr uint64_t kU32Mask = 0xFFFFFFFFULL;
auto ResolveEmit(EmitContext& context, const EmitPattern& emit, const std::string& path, std::string& error) -> Instruction;
auto ResolveEmitBlobEntry(EmitContext& context, const std::string& blob_name, const std::string& path, std::string& error) -> std::vector<Instruction>;
auto ResolveOperand(EmitContext& context, const OperandPattern& op, const std::string& path, std::string& error, size_t emit_operand_index) -> Operand;
auto ResolveOperandIndex(EmitContext& context, const OperandIndexPattern& pattern, const std::string& path, std::string& error) -> Operand::Index;
bool BuildExtendedOpcodeChain(EmitContext& context, Instruction& instr, const std::vector<EmitExtendedOpcode>& entries, const std::string& path, std::string& error);
bool ValidateOperandRole(const Operand& operand, OperandRole expected_role, const std::string& path, std::string& error);

/// @brief Bind iteration-scoped variables (params + implicit `iteration`).
/// Shadow semantics per the template plan: binding a name that shadows a
/// global saves the global so it is restored when the scope ends.
struct IterationScopeGuard {
  std::unordered_map<std::string, dxp::PrimitiveValue> saved_globals;
  EmitContext* ctx = nullptr;

  explicit IterationScopeGuard(EmitContext& ctx_val) : ctx(&ctx_val) {}

  void Bind(const std::string& name, dxp::PrimitiveValue value) {
    if (saved_globals.count(name) == 0 && ctx->variables.HasVariable(name)) {
      saved_globals[name] = *ctx->variables.FindVariable(name);
    }
    ctx->variables.SetVariable(name, std::move(value));
  }

  void Unbind(const std::string& name) {
    ctx->variables.UnsetVariable(name);
    auto it = saved_globals.find(name);
    if (it != saved_globals.end()) {
      ctx->variables.SetVariable(name, it->second);
    }
  }
};

/// @brief Bind per-iteration param values from arrays (through the scope).
static void BindRepeatParams(const RepeatConfig& repeat, uint32_t iteration, IterationScopeGuard& scope) {
  for (const auto& [name, arr] : repeat.params) {
    if (!arr.u32.empty() && iteration < arr.u32.size())
      scope.Bind(name, arr.u32[iteration]);
    else if (!arr.i32.empty() && iteration < arr.i32.size())
      scope.Bind(name, arr.i32[iteration]);
    else if (!arr.u64.empty() && iteration < arr.u64.size())
      scope.Bind(name, arr.u64[iteration]);
    else if (!arr.i64.empty() && iteration < arr.i64.size())
      scope.Bind(name, arr.i64[iteration]);
    else if (!arr.f32.empty() && iteration < arr.f32.size())
      scope.Bind(name, static_cast<double>(arr.f32[iteration]));
    else if (!arr.f64.empty() && iteration < arr.f64.size())
      scope.Bind(name, arr.f64[iteration]);
  }
}

/// @brief Bind template temps to the pool allocation.
static void BindTemplateTemps(const std::vector<std::string>& temps, uint32_t pool_base, EmitContext& ctx) {
  auto& temp_bindings = ctx.Bindings(BindingClass::Temp);
  for (size_t i = 0; i < temps.size(); ++i) {
    temp_bindings[temps[i]] = pool_base + static_cast<uint32_t>(i);
  }
}

/// @brief Unbind template temps from the pool allocation.
static void UnbindTemplateTemps(const std::vector<std::string>& temps, EmitContext& ctx) {
  auto& temp_bindings = ctx.Bindings(BindingClass::Temp);
  for (const auto& t : temps) {
    temp_bindings.erase(t);
  }
}

/// @brief Resolve one emit pattern into instructions, handling blob expansion.
/// Reuses the same logic that EvaluateRuleRewriteCallback uses for rule-level
/// emits — no duplication.
static bool ResolveEmitPattern(const EmitPattern& emit,
                               const std::string& path, std::vector<Instruction>& out,
                               std::string& error, EmitContext& ctx) {
  if (!emit.blob.empty()) {
    auto blob_instrs = ResolveEmitBlobEntry(ctx, emit.blob, path, error);
    if (!error.empty()) return false;
    for (auto& bi : blob_instrs) out.push_back(std::move(bi));
  } else {
    Instruction resolved = ResolveEmit(ctx, emit, path, error);
    if (!error.empty()) return false;
    out.push_back(std::move(resolved));
  }
  return true;
}

static bool ExpandTemplate(const std::string& template_name,
                           const ExecutionContext::RegisteredTemplate& tpl,
                           std::vector<Instruction>& out_instructions,
                           std::string& error,
                           EmitContext& ctx,
                           const std::optional<RepeatConfig>* override_repeat = nullptr,
                           const std::map<std::string, std::string>* entry_params = nullptr) {
  const std::vector<std::string>& template_temps = tpl.temps;
  const std::vector<EmitPattern>& template_emits = tpl.emits;
  // Entry-level repeat (on the template: emit entry): repeats the whole body
  // N times with caller params. Per-emit repeat (on the template's emits)
  // is template-set and expands inside each body iteration.
  static const std::optional<RepeatConfig> kNoRepeat;
  const std::optional<RepeatConfig>* effective_repeat = &kNoRepeat;
  if (override_repeat != nullptr && override_repeat->has_value()) {
    effective_repeat = override_repeat;
  }
  std::vector<std::string> bound_params;
  if (entry_params != nullptr && !entry_params->empty()) {
    auto& temp_bindings = ctx.Bindings(BindingClass::Temp);
    for (const auto& [param_name, provided] : *entry_params) {
      uint32_t register_index = 0;
      bool resolved = false;
      if (auto it = temp_bindings.find(provided); it != temp_bindings.end()) {
        register_index = it->second;
        resolved = true;
      } else if (auto it = ctx.captures.operands.find(provided); it != ctx.captures.operands.end()) {
        const auto& captured = it->second.operand_data;
        if (!captured.index_entries.empty() && captured.index_entries[0].immediate_lo.has_value()) {
          register_index = captured.index_entries[0].immediate_lo.value();
          resolved = true;
        }
      }
      if (!resolved) {
        error = "template '" + template_name + "' param '" + param_name + "': provided name '" + provided + "' is not a known add_resource temp or capture";
        return false;
      }
      temp_bindings[param_name] = register_index;
      bound_params.push_back(param_name);
    }
  }
  // Reuse pool model: every instantiation binds its temps to the same fixed
  // block (safe — temp names are unbound after the expansion, so nothing
  // outside it can reference those registers by name).
  BindTemplateTemps(template_temps, ctx.template_pool_base, ctx);

  auto cleanup = [&]() {
    UnbindTemplateTemps(template_temps, ctx);
    for (const auto& param_name : bound_params) {
      ctx.Bindings(BindingClass::Temp).erase(param_name);
    }
  };

  EmitContext body_context = ctx;
  body_context.local_operands = nullptr;
  body_context.adapt_operand_roles = false;
  auto expand_once = [&]() -> bool {
    for (size_t i = 0; i < template_emits.size(); ++i) {
      const std::string path = "template '" + template_name + "'.emits[" + std::to_string(i) + "]";
      const auto& emit = template_emits[i];
      if (emit.repeat.has_value()) {
        const auto& repeat = *emit.repeat;
        IterationScopeGuard scope(ctx);
        for (uint32_t iter = 0; iter < repeat.times; ++iter) {
          BindRepeatParams(repeat, iter, scope);
          scope.Bind("iteration", static_cast<dxp::PrimitiveValue>(iter));
          const bool ok = ResolveEmitPattern(emit, path, out_instructions, error, body_context);
          for (const auto& [name, arr] : repeat.params) scope.Unbind(name);
          scope.Unbind("iteration");
          if (!ok) {
            return false;
          }
        }
      } else if (!ResolveEmitPattern(emit, path, out_instructions, error, body_context)) {
        return false;
      }
    }
    return true;
  };

  // Template repeat: N iterations over the same pool block. Iteration-scoped
  // variables (params + 0-based `iteration`) exist only during each iteration
  // and shadow same-named globals with save/restore.
  const bool has_repeat = effective_repeat != nullptr && effective_repeat->has_value();
  if (has_repeat) {
    const auto& repeat = **effective_repeat;
    IterationScopeGuard scope(ctx);
    for (uint32_t iter = 0; iter < repeat.times; ++iter) {
      BindRepeatParams(repeat, iter, scope);
      scope.Bind("iteration", static_cast<dxp::PrimitiveValue>(iter));
      const bool ok = expand_once();
      for (const auto& [name, arr] : repeat.params) scope.Unbind(name);
      scope.Unbind("iteration");
      if (!ok) {
        cleanup();
        return false;
      }
    }
  } else {
    if (!expand_once()) {
      cleanup();
      return false;
    }
  }
  cleanup();
  return true;
}

// Builds the emitted extended-opcode chain: explicit entries verbatim, then the
// canonical ResourceDim/ResourceReturnType pair completed from the declaration.
// Unresolvable declarations are a hard error (no silent bare emits).
bool BuildExtendedOpcodeChain(EmitContext& context, Instruction& instr,
                              const std::vector<EmitExtendedOpcode>& entries,
                              const std::string& path, std::string& error) {
  const auto kChain = RequiredExtendedChainForOpcode(instr.opcode);
  std::vector<uint32_t> tokens;
  tokens.reserve(entries.size() + 2);
  for (const auto& entry : entries) {
    if (entry.kind == EmitExtendedOpcode::Kind::Raw) {
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
        StampResourceAccessControls(context.declarations, instr);
        if (!instr.controls.resource_dimension.has_value()) {
          error = path
                  +
                  ": resource-access emit requires a declared resource to synthesize the canonical "
                  "ResourceDim/ResourceReturnType extended pair (no silent bare emit)";
          return false;
        }
        dimension = static_cast<uint32_t>(static_cast<uint8_t>(*instr.controls.resource_dimension));
        for (uint32_t component = 0; component < 4; ++component) {
          if (instr.controls.resource_return_type[component].has_value()) {
            packed_return |= static_cast<uint32_t>(static_cast<uint8_t>(*instr.controls.resource_return_type[component])) << (component * D3D10_SB_RESOURCE_RETURN_TYPE_NUMBITS);
          }
        }
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

/// @brief Expands a stored blob into an emit stream (deep copy; the stored copy
/// is never aliased — each emit produces an independent duplicate).
auto ResolveEmitBlobEntry(EmitContext& context, const std::string& blob_name,
                          const std::string& path, std::string& error) -> std::vector<Instruction> {
  auto it = context.captures.blobs.find(blob_name);
  if (it == context.captures.blobs.end()) {
    error = path + ": unknown blob '" + blob_name + "'";
    return {};
  }
  if (it->second.instructions.empty()) {
    error = path + ": blob '" + blob_name + "' is empty";
    return {};
  }
  std::vector<Instruction> expanded;
  expanded.reserve(it->second.instructions.size());
  for (const Instruction& instr : it->second.instructions) {
    // FinalizeInstruction recomputes dword length; the copy is independent by value
    expanded.push_back(ShaderProgram::FinalizeInstruction(instr));
  }
  return expanded;
}

auto ResolveEmit(EmitContext& context, const EmitPattern& emit, const std::string& path, std::string& error) -> Instruction {
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
      Operand ro = ResolveOperand(context, emit.operands[oi], op, error, oi);
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
  if (emit.dimension.has_value()) {
    instr.controls.resource_dimension = emit.dimension;
  }
  for (uint32_t component = 0; component < 4; ++component) {
    if (emit.return_type[component].has_value()) {
      instr.controls.resource_return_type[component] = emit.return_type[component];
    }
  }
  if (emit.structure_stride != 0) {
    instr.controls.structure_stride = emit.structure_stride;
  }
  if (emit.access_pattern.has_value()) {
    instr.controls.access_pattern = emit.access_pattern;
    instr.controls.access_pattern_raw = static_cast<uint32_t>(static_cast<uint8_t>(*emit.access_pattern));
  }
  if (emit.mode.has_value()) {
    instr.controls.mode = emit.mode;
  }
  if (emit.uav_flags != 0) {
    instr.controls.uav_flags = emit.uav_flags;
  }
  instr.length_in_dwords = 0;
  for (size_t oi = 0; oi < emit.operands.size(); ++oi) {
    const std::string op = path + ".operands[" + std::to_string(oi) + "]";
    Operand ro = ResolveOperand(context, emit.operands[oi], op, error, oi);
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

auto ResolveOperand(EmitContext& context, const OperandPattern& op, const std::string& path, std::string& error, size_t emit_operand_index) -> Operand {
  Operand operand;
  const CapturedOperand* co = nullptr;
  OperandRole cr = OperandRole::Source;
  if (!op.capture.empty()) {
    // Prefer the current match's local captures (per-match correctness for
    // match_all rewrites); fall back to the cross-step global store.
    if (context.local_operands != nullptr) {
      auto lit = context.local_operands->find(op.capture);
      if (lit != context.local_operands->end()) co = &lit->second;
    }
    if (co == nullptr) {
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
  if (co != nullptr && context.adapt_operand_roles) {
    const OperandRole er = (emit_operand_index == 0) ? OperandRole::Destination : OperandRole::Source;
    if (!ValidateOperandRole(co->operand_data, cr, path, error)) return {};
    if (cr != er) operand = co->ResolveForRole(er);
    if (!ValidateOperandRole(operand, er, path, error)) return {};
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
      Operand::Index idx = ResolveOperandIndex(context, patterns[i], ip, error);
      if (!error.empty()) return {};
      operand.index_entries.push_back(std::move(idx));
    }
  } else if (!op.indices.empty()) {
    if (!op.capture.empty()) {
      operand.index_entries.clear();
    }
    for (size_t i = 0; i < op.indices.size(); ++i) {
      const std::string ip = path + ".indices[" + std::to_string(i) + "]";
      Operand::Index idx;
      idx.representation = static_cast<Operand::IndexRepresentation>(op.indices[i].representation);
      idx.immediate_lo = op.indices[i].immediate_lo.value_or(0);
      idx.immediate_hi = op.indices[i].immediate_hi.value_or(0);
      operand.index_entries.push_back(std::move(idx));
    }
  }
  if (op.handle && (!op.indices.empty() || !op.immediates_u32.empty() || !op.immediates_u64.empty() || !op.immediates_i32.empty() || !op.immediates_i64.empty() || !op.immediates_f32.empty() || !op.immediates_f64.empty())) {
    error = path + ": handle cannot be combined with explicit indices or typed immediates";
    return {};
  }
  if (!op.indices.empty() && (!op.immediates_u32.empty() || !op.immediates_u64.empty() || !op.immediates_i32.empty() || !op.immediates_i64.empty() || !op.immediates_f32.empty() || !op.immediates_f64.empty())) {
    error = path + ": explicit indices and typed immediates cannot be combined";
    return {};
  }
  if (op.handle) {
    const auto lookup = [&](BindingClass kind) -> const uint32_t* {
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
      case OperandType::Temp:   rbp = lookup(BindingClass::Temp); break;
      case OperandType::Input:  rbp = lookup(BindingClass::Input); break;
      case OperandType::Output: rbp = lookup(BindingClass::Output); break;
      case OperandType::Resource:
        rbp = lookup(BindingClass::Texture);
        if (rbp == nullptr) rbp = lookup(BindingClass::RawResource);
        if (rbp == nullptr) rbp = lookup(BindingClass::StructuredResource);
        break;
      case OperandType::Sampler: rbp = lookup(BindingClass::Sampler); break;
      case OperandType::CBuffer: rbp = lookup(BindingClass::CBuffer); break;
      case OperandType::UAV:     rbp = lookup(BindingClass::Uav); break;
      default:
        error = path + ": SM5 handle operand type is unsupported for resource binding";
        return {};
    }
    if (rbp == nullptr) {
      error = path + ": missing SM5 declaration handle binding '" + op.handle->name + "'";
      return {};
    }
    // Handle overrides the register index (first entry) but preserves any
    // subsequent entries (e.g. cbuffer element_index) from a captured operand.
    if (!operand.index_entries.empty()) {
      operand.index_entries[0].immediate_lo = *rbp;
    } else {
      Operand::Index operand_index;
      operand_index.representation = Operand::IndexRepresentation::Immediate32;
      operand_index.immediate_lo = *rbp;
      operand.index_entries.push_back(std::move(operand_index));
    }
    if (op.handle->element_index.has_value()) {
      uint32_t resolved_element = 0;
      const auto& elem_idx = *op.handle->element_index;
      if (std::holds_alternative<std::string>(elem_idx)) {
        if (const auto* var = context.variables.FindVariable(std::get<std::string>(elem_idx))) {
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
      if (*op.type == OperandType::Temp || *op.type == OperandType::IndexableTemp) {
        // Temp array element is a 1-D register: base + element. A second index
        // entry would encode 2-D indexing ([row][col]), which the register
        // space interprets as a flattened row — not base + element.
        if (const auto base = operand.index_entries[0].immediate_lo) {
          operand.index_entries[0].immediate_lo = *base + resolved_element;
        }
      } else {
        Operand::Index element_index;
        element_index.representation = Operand::IndexRepresentation::Immediate32;
        element_index.immediate_lo = resolved_element;
        operand.index_entries.push_back(std::move(element_index));
      }
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

auto ResolveOperandIndex(EmitContext& context, const OperandIndexPattern& pattern, const std::string& path, std::string& error) -> Operand::Index {
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
  // Variable-backed immediates: resolve from context variables (repeat params, env, etc.).
  if (!pattern.immediate_lo_variable.empty()) {
    if (const auto* v = context.variables.FindVariable(pattern.immediate_lo_variable)) {
      bool found = false;
      std::visit(
          [&found, &idx](const auto& pv) {
            using T = std::decay_t<decltype(pv)>;
            if constexpr (std::is_same_v<T, bool>) {
              idx.immediate_lo = pv ? 1u : 0u;
              found = true;
            } else if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t>) {
              idx.immediate_lo = static_cast<uint32_t>(pv);
              found = true;
            } else if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>) {
              idx.immediate_lo = static_cast<uint32_t>(pv);
              idx.immediate_hi = static_cast<uint32_t>(pv >> 32);
              found = true;
            } else if constexpr (std::is_same_v<T, double>) {
              const auto bits = static_cast<uint64_t>(pv);
              idx.immediate_lo = static_cast<uint32_t>(bits);
              idx.immediate_hi = static_cast<uint32_t>(bits >> 32);
              found = true;
            }
          },
          *v);
      if (!found) {
        error = path + ": variable '" + pattern.immediate_lo_variable + "' has unsupported type for immediate";
        return {};
      }
    } else {
      error = path + ": missing variable '" + pattern.immediate_lo_variable + "' for immediate";
      return {};
    }
  }
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
    Operand ro = ResolveOperand(context, **pattern.relative_operand, path + ".relative_operand", error, 0);
    if (!error.empty()) return {};
    idx.relative_operand = xyz::indirect<Operand>(std::move(ro));
  }
  return idx;
}

bool ValidateOperandRole(const Operand& operand, OperandRole expected_role, const std::string& path, std::string& error) {
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

bool ResolveImmediateFromVariable(const std::string& path, const std::string& vn, const EmitContext& ctx, ImmediateFamily family, uint32_t& ol, uint32_t& oh, bool& hh, std::string& error) {
  const auto* v = ctx.variables.FindVariable(vn);
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

/// @brief Resolve one emit entry (opcode / capture / blob / template),
/// expanding an optional repeat (N copies with per-iteration param binding
/// and 0-based `iteration`, scoped with save/restore) into instructions.
/// Template entries instantiate from the context's template registry; the
/// entry-level repeat overrides the template's declared repeat.
bool ResolveEmitEntry(const EmitPattern& entry,
                      const std::string& path, std::vector<Instruction>& out,
                      std::string& error, EmitContext& ctx) {
  if (!entry.template_name.empty()) {
    auto it = ctx.templates.find(entry.template_name);
    if (it == ctx.templates.end()) {
      error = path + ": unknown template '" + entry.template_name + "'";
      return false;
    }
    return ExpandTemplate(entry.template_name, it->second, out, error, ctx, &entry.repeat, &entry.params);
  }
  if (entry.repeat.has_value()) {
    const auto& repeat = *entry.repeat;
    IterationScopeGuard scope(ctx);
    for (uint32_t iter = 0; iter < repeat.times; ++iter) {
      BindRepeatParams(repeat, iter, scope);
      scope.Bind("iteration", static_cast<dxp::PrimitiveValue>(iter));
      const bool ok = ResolveEmitPattern(entry, path, out, error, ctx);
      for (const auto& [name, arr] : repeat.params) scope.Unbind(name);
      scope.Unbind("iteration");
      if (!ok) return false;
    }
    return true;
  }
  return ResolveEmitPattern(entry, path, out, error, ctx);
}

// Stamps resource dimension/return type from the operand's declaration so the
// canonical extended pair can be synthesized. Parsed instructions are untouched.
void StampResourceAccessControls(const DeclarationIndex& declarations, Instruction& instr) {
  if (!instr.controls.extended_op_codes.empty() || !RequiredExtendedChainForOpcode(instr.opcode).RequiresResourcePair()) {
    return;
  }
  for (const auto& operand : instr.operands) {
    if (operand.type != OperandType::Resource && operand.type != OperandType::UAV) {
      continue;
    }
    if (operand.index_entries.empty() || !operand.index_entries[0].immediate_lo.has_value()) {
      continue;
    }
    const uint32_t register_index = *operand.index_entries[0].immediate_lo;
    const ResourceDeclInfo* decl = declarations.FindResourceDecl(operand.type, register_index);
    if (decl == nullptr) {
      continue;
    }
    instr.controls.resource_dimension = decl->dimension;
    for (uint32_t component = 0; component < 4; ++component) {
      instr.controls.resource_return_type[component] = decl->return_types[component];
    }
    return;
  }
}

}  // namespace dxp::sm5::step::common
