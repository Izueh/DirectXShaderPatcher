#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/Recipe.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

static int FindTargetInstruction(const dxp::sm5::Program &program) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (static_cast<dxp::sm5::OpcodeType>(instruction.Opcode) ==
            D3D10_SB_OPCODE_MUL &&
        instruction.Operands.size() >= 2) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

static bool HasDiagnosticContaining(const dxp::sm5::RecipeContext &context,
                                    const std::string &needle) {
  for (const std::string &diagnostic : context.Diagnostics) {
    if (diagnostic.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

static dxp::sm5::RecipeRule MakeMovFromMulRule() {
  dxp::sm5::RecipeRule rule;
  rule.Match.Opcode = "mul";

  dxp::sm5::RecipeOperandPattern dstCapture;
  dstCapture.Capture = "dst";
  rule.Match.Operands.push_back(dstCapture);

  dxp::sm5::RecipeOperandPattern srcCapture;
  srcCapture.Capture = "src";
  rule.Match.Operands.push_back(srcCapture);

  dxp::sm5::RecipeInstructionTemplate emitMov;
  emitMov.Opcode = "mov";

  dxp::sm5::RecipeOperandPattern emitDst;
  emitDst.FromCapture = "dst";
  emitMov.Operands.push_back(emitDst);

  dxp::sm5::RecipeOperandPattern emitSrc;
  emitSrc.FromCapture = "src";
  emitMov.Operands.push_back(emitSrc);

  rule.Emit.push_back(emitMov);
  return rule;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_rule_predicate <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> inputBytes;
  if (!ReadFile(argv[1], inputBytes)) {
    std::cerr << "Failed to read input file: " << argv[1] << "\n";
    return 1;
  }

  dxp::sm5::Container inputContainer;
  if (!dxp::sm5::ParseDxbcContainer(inputBytes, inputContainer)) {
    std::cerr << "Failed to parse input DXBC container.\n";
    return 1;
  }

  dxp::sm5::Program inputProgram;
  if (!dxp::sm5::ParseShaderChunk(inputContainer, inputProgram)) {
    std::cerr << "Failed to parse input SM5 program.\n";
    return 1;
  }

  const int targetInstructionIndex = FindTargetInstruction(inputProgram);
  if (targetInstructionIndex < 0) {
    std::cerr << "Failed to locate target MUL instruction.\n";
    return 1;
  }

  {
    dxp::sm5::RecipeRule skipRule = MakeMovFromMulRule();
    skipRule.Predicate = [](dxp::sm5::RecipeContext &, const dxp::sm5::MatchResult &) {
      return false;
    };

    dxp::sm5::Recipe skipRecipe;
    skipRecipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
        "skip_by_predicate", {skipRule},
        dxp::sm5::RecipeRuleApplicationMode::First, true));

    const auto skipResult = dxp::sm5::PatchContainerInMemory(inputBytes, skipRecipe);
    if (!skipResult.Success) {
      std::cerr << "Expected predicate=false recipe to succeed, but patch failed: "
                << skipResult.Error << "\n";
      return 1;
    }

    dxp::sm5::Container patchedContainer;
    if (!dxp::sm5::ParseDxbcContainer(skipResult.OutputBytes, patchedContainer)) {
      std::cerr << "Failed to parse predicate-skip patched DXBC container.\n";
      return 1;
    }

    dxp::sm5::Program patchedProgram;
    if (!dxp::sm5::ParseShaderChunk(patchedContainer, patchedProgram)) {
      std::cerr << "Failed to parse predicate-skip patched SM5 program.\n";
      return 1;
    }

    const auto &instruction =
        patchedProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
    if (static_cast<dxp::sm5::OpcodeType>(instruction.Opcode) != D3D10_SB_OPCODE_MUL) {
      std::cerr << "Expected predicate=false to skip rewrite and keep MUL opcode.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeRule requiredErrorRule = MakeMovFromMulRule();
    requiredErrorRule.Predicate = [](dxp::sm5::RecipeContext &, const dxp::sm5::MatchResult &) -> bool {
      throw std::runtime_error("predicate failed");
    };

    dxp::sm5::Recipe requiredErrorRecipe;
    requiredErrorRecipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
        "required_predicate_error", {requiredErrorRule},
        dxp::sm5::RecipeRuleApplicationMode::First, true));

    const auto requiredErrorResult =
        dxp::sm5::PatchContainerInMemory(inputBytes, requiredErrorRecipe);
    if (requiredErrorResult.Success) {
      std::cerr << "Expected required-step predicate exception to fail patching.\n";
      return 1;
    }

  }

  {
    dxp::sm5::RecipeRule optionalErrorRule = MakeMovFromMulRule();
    optionalErrorRule.Predicate = [](dxp::sm5::RecipeContext &, const dxp::sm5::MatchResult &) -> bool {
      throw std::runtime_error("optional predicate failed");
    };

    dxp::sm5::Recipe optionalErrorRecipe;
    optionalErrorRecipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
        "optional_predicate_error", {optionalErrorRule},
        dxp::sm5::RecipeRuleApplicationMode::First, false));

    const auto optionalErrorResult =
        dxp::sm5::PatchContainerInMemory(inputBytes, optionalErrorRecipe);
    if (!optionalErrorResult.Success) {
      std::cerr << "Expected optional-step predicate exception to continue, but patch failed: "
                << optionalErrorResult.Error << "\n";
      return 1;
    }

    if (!HasDiagnosticContaining(optionalErrorResult.RecipeContext,
                                 "SM5 rule predicate threw exception")) {
      std::cerr << "Expected optional-step predicate exception diagnostic to be recorded.\n";
      return 1;
    }
  }

  std::cout << "SM5 rule predicates skip matches on false and fail only required steps on predicate exceptions.\n";
  return 0;
}
