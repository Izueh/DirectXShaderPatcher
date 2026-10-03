#pragma once

#include "dxp/sm5/ExecutionContext.hpp"

namespace dxp::sm5::step::common {

using HandleBindings = std::unordered_map<BindingClass, std::unordered_map<std::string, uint32_t>>;
using TemplateRegistry = std::unordered_map<std::string, ExecutionContext::RegisteredTemplate>;

/// @brief Inputs needed to construct instructions, independently of matching or
/// rewrite placement. Mutable bindings and variables support scoped expansion.
struct EmitContext {
  const CaptureStore& captures;
  dxp::VariableStore& variables;
  HandleBindings& bindings;
  const TemplateRegistry& templates;
  uint32_t template_pool_base;
  const DeclarationIndex& declarations;
  const dxp::LogContext& logger;
  const std::unordered_map<std::string, model::CapturedOperand>* local_operands = nullptr;
  bool adapt_operand_roles = false;

  std::unordered_map<std::string, uint32_t>& Bindings(BindingClass kind) { return bindings[kind]; }
};

/// @brief Expands an opcode, capture, blob, template, or repeated emit into an
/// instruction stream. Does not match instructions or mutate the shader.
bool ResolveEmitEntry(const EmitPattern& entry, const std::string& path,
                      std::vector<model::Instruction>& out, std::string& error,
                      EmitContext& context);

void StampResourceAccessControls(const DeclarationIndex& declarations, model::Instruction& instruction);

}  // namespace dxp::sm5::step::common
