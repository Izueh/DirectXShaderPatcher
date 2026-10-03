#include <iostream>
#include "dxp/sm5/step/common/Emit.hpp"

namespace {
using namespace dxp::sm5::model;
using namespace dxp::sm5::step;
using dxp::sm5::step::common::EmitContext;
using dxp::sm5::step::common::ResolveEmitEntry;
int failures = 0;

void Check(bool passed, const std::string& message) {
  if (!passed) {
    std::cerr << message << '\n';
    ++failures;
  }
}

CapturedOperand Register(uint32_t index, OperandRole role) {
  CapturedOperand captured;
  captured.role = role;
  captured.operand_data.type = OperandType::Temp;
  captured.operand_data.components = Components{NumComponents::Four, SelectionMode::Mask, "x"};
  captured.operand_data.component_mode = 0x10;  // DXBC mask x.
  Operand::Index slot;
  slot.immediate_lo = index;
  captured.operand_data.index_entries.push_back(slot);
  return captured;
}

uint32_t Index(const Instruction& instruction, size_t operand) {
  return instruction.operands.at(operand).index_entries.at(0).immediate_lo.value();
}
}  // namespace

int main() {
  // Construction requires only emission inputs, without a program or match.
  dxp::sm5::CaptureStore captures;
  dxp::VariableStore variables;
  common::HandleBindings bindings;
  common::TemplateRegistry templates;
  dxp::sm5::DeclarationIndex declarations;
  dxp::LogContext logger;
  std::unordered_map<std::string, CapturedOperand> locals;
  captures.operands["value"] = Register(1, OperandRole::Destination);
  locals["value"] = Register(7, OperandRole::Destination);
  bindings[dxp::BindingClass::Temp]["destination"] = 9;
  EmitContext context{captures, variables, bindings, templates, 30, declarations, logger, &locals, true};

  OperandPattern destination;
  destination.type = OperandType::Temp;
  destination.handle = OperandPattern::Handle{"destination"};
  destination.components = Components{NumComponents::Four, SelectionMode::Mask, "x"};
  EmitPattern emit;
  emit.opcode = Opcode::Mov;
  emit.operands = {destination, OperandPattern{.capture = "value"}};
  std::vector<Instruction> output;
  std::string error;
  if (!ResolveEmitEntry(emit, "local", output, error, context)) {
    std::cerr << error << '\n';
    return 1;
  }
  Check(Index(output.front(), 0) == 9 && Index(output.front(), 1) == 7, "Local capture must override the cross-step capture");
  Check(output.front().operands[1].component_mode != locals.at("value").operand_data.component_mode, "Captured destination must be adapted to a source operand");
  Check(locals.at("value").operand_data.component_mode == 0x10, "Role conversion must preserve stored captures");

  variables.SetVariable("offset", uint32_t{91});
  variables.SetVariable("iteration", uint32_t{99});
  OperandPattern source;
  source.type = OperandType::Temp;
  source.components = Components{NumComponents::Four, SelectionMode::Select, "x"};
  OperandIndexPattern index;
  index.immediate_lo_variable = "offset";
  source.indices = {index};
  emit.operands[1] = source;
  emit.repeat = RepeatConfig{};
  emit.repeat->times = 2;
  emit.repeat->params["offset"].u32 = {21, 22};
  output.clear();
  Check(ResolveEmitEntry(emit, "repeat", output, error, context), "Repeat expansion failed: " + error);
  Check(output.size() == 2 && Index(output[0], 1) == 21 && Index(output[1], 1) == 22, "Each repeat must bind its own environment value");
  Check(std::get<uint32_t>(*variables.FindVariable("offset")) == 91 && std::get<uint32_t>(*variables.FindVariable("iteration")) == 99, "Successful repeats must restore shadowed variables");

  emit.operands[1].indices.front().immediate_lo_variable = "missing";
  output.clear();
  Check(!ResolveEmitEntry(emit, "failure", output, error, context) && error.find("missing variable") != std::string::npos, "Missing repeat operand must report an error");
  Check(std::get<uint32_t>(*variables.FindVariable("offset")) == 91 && std::get<uint32_t>(*variables.FindVariable("iteration")) == 99, "Failed repeats must restore shadowed variables");

  captures.blobs["saved"].instructions = {Instruction{}};
  captures.blobs["saved"].instructions.front().opcode = Opcode::Ret;
  EmitPattern blob;
  blob.blob = "saved";
  output.clear();
  error.clear();
  Check(ResolveEmitEntry(blob, "blob", output, error, context), "Blob expansion failed: " + error);
  if (!output.empty()) output.front().opcode = Opcode::Nop;
  Check(captures.blobs.at("saved").instructions.front().opcode == Opcode::Ret, "Blob emission must preserve the stored copy");
  return failures == 0 ? 0 : 1;
}
