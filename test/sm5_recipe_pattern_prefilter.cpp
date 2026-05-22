#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/Recipe.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

static bool Contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

static bool ContainsDiagnostic(const dxp::sm5::RecipeContext &context,
                               const std::string &needle) {
  for (const auto &message : context.Diagnostics) {
    if (Contains(message, needle)) {
      return true;
    }
  }
  return false;
}

static dxp::sm5::RecipeMatchPattern MakeSingleMulPattern() {
  dxp::sm5::RecipeMatchPattern pattern;
  pattern.Opcode = "mul";
  return pattern;
}

static dxp::sm5::RecipeMatchPattern MakeFrcMulSequencePattern() {
  dxp::sm5::RecipeMatchPattern pattern;

  dxp::sm5::RecipeInstructionPattern first;
  first.Opcode = "frc";
  pattern.Sequence.push_back(first);

  dxp::sm5::RecipeInstructionPattern second;
  second.Opcode = "mul";
  pattern.Sequence.push_back(second);

  return pattern;
}

static dxp::sm5::RecipeMatchPattern MakeImpossibleMulPattern() {
  dxp::sm5::RecipeMatchPattern pattern;
  pattern.Opcode = "mul";

  dxp::sm5::RecipeOperandPattern impossibleOperand;
  impossibleOperand.Type = "sampler";
  pattern.Operands.push_back(impossibleOperand);

  return pattern;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_pattern_prefilter <input.ps_5_0.cso>\n";
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

  dxp::sm5::Recipe optionalFailRecipe;
  optionalFailRecipe.AddPrefilter(
      dxp::sm5::MakePatternPrefilter(MakeSingleMulPattern(),
                                     "required_single_mul", true));
  optionalFailRecipe.AddPrefilter(
      dxp::sm5::MakePatternPrefilter(MakeFrcMulSequencePattern(),
                                     "required_frc_mul_sequence", true));
  optionalFailRecipe.AddPrefilter(
      dxp::sm5::MakePatternPrefilter(MakeImpossibleMulPattern(),
                                     "optional_impossible", false));

  const auto optionalFailResult =
      dxp::sm5::PatchContainerInMemory(inputBytes, optionalFailRecipe);
  if (!optionalFailResult.Success) {
    std::cerr << "Expected optional-failure recipe to succeed, but patch failed: "
              << optionalFailResult.Error << "\n";
    return 1;
  }

  if (!ContainsDiagnostic(optionalFailResult.RecipeContext,
                          "optional SM5 prefilter did not match: optional_impossible")) {
    std::cerr << "Expected optional pattern prefilter failure to record a diagnostic.\n";
    return 1;
  }

  dxp::sm5::Recipe requiredFailRecipe;
  requiredFailRecipe.AddPrefilter(
      dxp::sm5::MakePatternPrefilter(MakeImpossibleMulPattern(),
                                     "required_impossible", true));

  const auto requiredFailResult =
      dxp::sm5::PatchContainerInMemory(inputBytes, requiredFailRecipe);
  if (requiredFailResult.Success) {
    std::cerr << "Expected required-failure recipe to fail, but patch succeeded.\n";
    return 1;
  }

  std::cout << "SM5 pattern prefilters support single and sequence matching; "
               "optional mismatches are diagnostic-only and required mismatches fail.\n";
  return 0;
}
