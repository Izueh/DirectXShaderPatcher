#include "TestSupport.h"
#include "dxp/sm5/Model.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Recipe.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

static int FindTargetInstruction(const dxp::sm5::ProgramInspection &program) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (instruction.Opcode == D3D10_SB_OPCODE_MUL &&
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
  return dxp::sm5::RecipeRule{}
    .WithMatch(dxp::sm5::RecipeMatchPattern{}
           .WithOpcode("mul")
           .AddOperand(
             dxp::sm5::RecipeOperandPattern{}.CaptureAs("dst"))
           .AddOperand(
             dxp::sm5::RecipeOperandPattern{}.CaptureAs("src")))
    .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
           .WithOpcode("mov")
           .AddOperand(
             dxp::sm5::RecipeOperandPattern{}.CaptureAs("dst"))
           .AddOperand(
             dxp::sm5::RecipeOperandPattern{}.CaptureAs("src")));
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

  dxp::sm5::ProgramInspection inputProgram;
  std::string inspectError;
  if (!dxp::sm5::InspectProgram(inputBytes, inputProgram, &inspectError)) {
    std::cerr << "Failed to inspect input SM5 program: " << inspectError
              << "\n";
    return 1;
  }

  const int targetInstructionIndex = FindTargetInstruction(inputProgram);
  if (targetInstructionIndex < 0) {
    std::cerr << "Failed to locate target MUL instruction.\n";
    return 1;
  }

  {
    dxp::sm5::RecipeRule skipRule =
        MakeMovFromMulRule().When([](dxp::sm5::RecipeContext &) {
          return false;
        });

    dxp::sm5::Recipe skipRecipe;
    skipRecipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
        "skip_by_predicate", {skipRule},
        dxp::sm5::RecipeRuleApplicationMode::First, true));

    const auto skipResult = dxp::sm5::PatchContainer(inputBytes, skipRecipe);
    if (!skipResult.Success) {
      std::cerr
          << "Expected predicate=false recipe to succeed, but patch failed: "
          << skipResult.Error << "\n";
      return 1;
    }

    dxp::sm5::ProgramInspection patchedProgram;
    if (!dxp::sm5::InspectProgram(skipResult.OutputBytes, patchedProgram,
                                  &inspectError)) {
      std::cerr << "Failed to inspect predicate-skip patched SM5 program: "
                << inspectError << "\n";
      return 1;
    }

    const auto &instruction =
        patchedProgram
            .Instructions[static_cast<size_t>(targetInstructionIndex)];
    if (instruction.Opcode != D3D10_SB_OPCODE_MUL) {
      std::cerr
          << "Expected predicate=false to skip rewrite and keep MUL opcode.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeRule requiredErrorRule =
        MakeMovFromMulRule().When([](dxp::sm5::RecipeContext &) -> bool {
          throw std::runtime_error("predicate failed");
        });

    dxp::sm5::Recipe requiredErrorRecipe;
    requiredErrorRecipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
        "required_predicate_error", {requiredErrorRule},
        dxp::sm5::RecipeRuleApplicationMode::First, true));

    const auto requiredErrorResult =
        dxp::sm5::PatchContainer(inputBytes, requiredErrorRecipe);
    if (requiredErrorResult.Success) {
      std::cerr
          << "Expected required-step predicate exception to fail patching.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeRule optionalErrorRule =
        MakeMovFromMulRule().When([](dxp::sm5::RecipeContext &) -> bool {
          throw std::runtime_error("optional predicate failed");
        });

    dxp::sm5::Recipe optionalErrorRecipe;
    optionalErrorRecipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
        "optional_predicate_error", {optionalErrorRule},
        dxp::sm5::RecipeRuleApplicationMode::First, false));

    const auto optionalErrorResult =
        dxp::sm5::PatchContainer(inputBytes, optionalErrorRecipe);
    if (!optionalErrorResult.Success) {
      std::cerr << "Expected optional-step predicate exception to continue, "
                   "but patch failed: "
                << optionalErrorResult.Error << "\n";
      return 1;
    }

    if (!HasDiagnosticContaining(optionalErrorResult.RecipeContext,
                                 "SM5 rule predicate threw exception")) {
      std::cerr << "Expected optional-step predicate exception diagnostic to "
                   "be recorded.\n";
      return 1;
    }
  }

  {
    dxp::sm5::RecipeRule callbackRule = dxp::sm5::RecipeRule{}
      .WithMatch([](const dxp::sm5::Program &program,
                    dxp::sm5::RecipeContext &) {
        std::vector<dxp::sm5::RecipeRuleMatch> matches;
        for (uint32_t index = 0; index < program.Instructions.size(); ++index) {
          const auto &instruction = program.Instructions[index];
          if (instruction.Opcode != dxp::sm5::Opcode{D3D10_SB_OPCODE_MUL} ||
              instruction.Operands.size() < 2) {
            continue;
          }

          dxp::sm5::RecipeRuleMatch match;
          match.InstructionIndex = index;
          match.InstructionHandle = &instruction;
          match.RangeStartIndex = index;
          match.RangeEndIndex = index;
          match.CapturedOperands["dst"] = &instruction.Operands[0];
          match.CapturedOperands["src"] = &instruction.Operands[1];
          matches.push_back(std::move(match));
        }
        return matches;
      })
      .Rewrite([](const dxp::sm5::Program &,
                  const dxp::sm5::RecipeRuleMatch &match,
                  dxp::sm5::RecipeContext &) {
        std::vector<dxp::sm5::RecipeRewriteAction> actions;

        dxp::sm5::RecipeRewriteAction action;
        action.Kind = dxp::sm5::RecipeRewriteActionKind::ReplaceOne;
        action.ReplaceIndex = match.InstructionIndex;
        action.AddEmit(dxp::sm5::RecipeInstructionTemplate{}
                           .WithOpcode("mov")
                           .AddOperand(
                               dxp::sm5::RecipeOperandPattern{}.CaptureAs("dst"))
                           .AddOperand(
                               dxp::sm5::RecipeOperandPattern{}.CaptureAs("src")));
        actions.push_back(std::move(action));
        return actions;
      });

    dxp::sm5::Recipe callbackRecipe;
    callbackRecipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
        "callback_match_and_rewrite", {callbackRule},
        dxp::sm5::RecipeRuleApplicationMode::First, true));

    const auto callbackResult = dxp::sm5::PatchContainer(inputBytes, callbackRecipe);
    if (!callbackResult.Success) {
      std::cerr << "Expected callback match/rewrite recipe to succeed, but patch failed: "
                << callbackResult.Error << "\n";
      return 1;
    }

    dxp::sm5::ProgramInspection patchedProgram;
    if (!dxp::sm5::InspectProgram(callbackResult.OutputBytes, patchedProgram,
                                  &inspectError)) {
      std::cerr << "Failed to inspect callback-rewrite patched SM5 program: "
                << inspectError << "\n";
      return 1;
    }

    const auto &instruction =
        patchedProgram.Instructions[static_cast<size_t>(targetInstructionIndex)];
    if (instruction.Opcode != D3D10_SB_OPCODE_MOV) {
      std::cerr << "Expected callback rewrite to replace MUL with MOV.\n";
      return 1;
    }
  }

  std::cout << "SM5 rule predicates skip matches on false and fail only "
               "required steps on predicate exceptions, and callback match/rewrite works.\n";
  return 0;
}
