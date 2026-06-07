#include "benchmark_helpers.h"

#include "d3d11TokenizedProgramFormat.hpp"

#include <fstream>
#include <stdexcept>

std::vector<uint8_t> LoadShaderBytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open shader file: " + path.string());
  }

  const std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(static_cast<size_t>(size));
  if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
    throw std::runtime_error("Failed to read shader file: " + path.string());
  }

  return buffer;
}

bool ParseShaderToProgram(const std::vector<uint8_t>& containerBytes,
                          dxp::sm5::Program& program, std::string& error) {
  dxp::sm5::Container container;
  if (!dxp::sm5::ParseDxbcContainer(containerBytes, container)) {
    error = "failed to parse DXBC container";
    return false;
  }

  if (!dxp::sm5::ParseShaderChunk(container, program)) {
    error = "failed to parse shader chunk";
    return false;
  }

  return true;
}

dxp::sm5::Recipe BuildNoopMovRecipe() {
  dxp::sm5::Recipe recipe;

  // Build a rule that matches MOV instructions and re-emits them unchanged.
  // This exercises the match + rewrite pipeline without modifying the shader.
  dxp::sm5::RecipeRule rule;
  rule.Match.Opcode = "mov";
  rule.Emit.emplace_back();
  rule.Emit.front().Opcode = "mov";
  rule.Emit.front().Operands.emplace_back();
  // Destination operand: capture from matched instruction
  rule.Emit.front().Operands[0].Capture = "dst";
  rule.Emit.front().Operands[0].CaptureFields =
      dxp::sm5::RecipeOperandCaptureFields{true, true, true, true, true};
  // Source operand: capture from matched instruction
  rule.Emit.front().Operands.emplace_back();
  rule.Emit.front().Operands[1].Capture = "src";
  rule.Emit.front().Operands[1].CaptureFields =
      dxp::sm5::RecipeOperandCaptureFields{true, true, true, true, true};

  // Add the rule to a step
  dxp::sm5::RecipeStep step;
  step.Name = "noop_mov";
  step.Rules.push_back(std::move(rule));
  step.ApplicationMode = dxp::sm5::RecipeRuleApplicationMode::MatchAll;

  recipe.AddStep(std::move(step));
  return recipe;
}

dxp::sm5::Recipe BuildSequenceMatchRecipe() {
  dxp::sm5::Recipe recipe;

  // Build a rule that matches a two-instruction sequence: MOV followed by ADD.
  dxp::sm5::RecipeRule rule;
  rule.Match.Sequence.emplace_back();
  rule.Match.Sequence[0].Opcode = "mov";
  rule.Match.Sequence[0].Operands.emplace_back();
  rule.Match.Sequence[0].Operands[0].Capture = "dst";
  rule.Match.Sequence[0].Operands[0].CaptureFields =
      dxp::sm5::RecipeOperandCaptureFields{true, true, true, true, true};

  rule.Match.Sequence.emplace_back();
  rule.Match.Sequence[1].Opcode = "add";
  rule.Match.Sequence[1].Operands.emplace_back();
  rule.Match.Sequence[1].Operands[0].Capture = "dst2";
  rule.Match.Sequence[1].Operands[0].CaptureFields =
      dxp::sm5::RecipeOperandCaptureFields{true, true, true, true, true};

  // Re-emit both instructions
  rule.Emit.emplace_back();
  rule.Emit[0].Opcode = "mov";
  rule.Emit[0].Operands.emplace_back();
  rule.Emit[0].Operands[0].Capture = "dst";
  rule.Emit[0].Operands[0].CaptureFields =
      dxp::sm5::RecipeOperandCaptureFields{true, true, true, true, true};
  rule.Emit[0].Operands.emplace_back();
  rule.Emit[0].Operands[1].Capture = "src";
  rule.Emit[0].Operands[1].CaptureFields =
      dxp::sm5::RecipeOperandCaptureFields{true, true, true, true, true};

  rule.Emit.emplace_back();
  rule.Emit[1].Opcode = "add";
  rule.Emit[1].Operands.emplace_back();
  rule.Emit[1].Operands[0].Capture = "dst2";
  rule.Emit[1].Operands[0].CaptureFields =
      dxp::sm5::RecipeOperandCaptureFields{true, true, true, true, true};
  rule.Emit[1].Operands.emplace_back();
  rule.Emit[1].Operands[1].Capture = "src";
  rule.Emit[1].Operands[1].CaptureFields =
      dxp::sm5::RecipeOperandCaptureFields{true, true, true, true, true};

  dxp::sm5::RecipeStep step;
  step.Name = "sequence_mov_add";
  step.Rules.push_back(std::move(rule));
  step.ApplicationMode = dxp::sm5::RecipeRuleApplicationMode::MatchAll;

  recipe.AddStep(std::move(step));
  return recipe;
}

std::filesystem::path DefaultTestShaderPath() {
  // Use the same shader path as the existing SM5 tests.
  // This resolves relative to the source directory.
  return "test/shaders/0x7AFF256C.ps_5_0.cso";
}
