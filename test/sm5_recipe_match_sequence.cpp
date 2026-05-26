#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <vector>

namespace {

static bool OperandsEqual(const dxp::sm5::ProgramOperand &lhs,
                          const dxp::sm5::ProgramOperand &rhs) {
  if (lhs.Type != rhs.Type || lhs.NumComponents != rhs.NumComponents ||
      lhs.ComponentMode != rhs.ComponentMode || lhs.Modifier != rhs.Modifier ||
      lhs.Indices != rhs.Indices ||
      lhs.ImmediateValues != rhs.ImmediateValues) {
    return false;
  }

  if (lhs.RelativeOperands.size() != rhs.RelativeOperands.size()) {
    return false;
  }

  if (!lhs.RelativeOperands.empty() &&
      !OperandsEqual(lhs.RelativeOperands.front(),
                     rhs.RelativeOperands.front())) {
    return false;
  }

  return true;
}

static int FindFrcMulSequence(const dxp::sm5::ProgramInspection &program) {
  for (size_t index = 0; index + 1 < program.Instructions.size(); ++index) {
    const auto &first = program.Instructions[index];
    const auto &second = program.Instructions[index + 1];
    if (first.Opcode == D3D10_SB_OPCODE_FRC &&
        second.Opcode == D3D10_SB_OPCODE_MUL && second.Operands.size() >= 2) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

static int CountOpcode(const dxp::sm5::ProgramInspection &program,
                       uint32_t opcode) {
  int count = 0;
  for (const auto &instruction : program.Instructions) {
    if (instruction.Opcode == opcode) {
      ++count;
    }
  }
  return count;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_match_sequence <input.ps_5_0.cso>\n";
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

  const int sequenceStartIndex = FindFrcMulSequence(inputProgram);
  if (sequenceStartIndex < 0) {
    std::cerr << "Failed to locate a contiguous FRC/MUL instruction sequence "
                 "in the input program.\n";
    return 1;
  }

  const dxp::sm5::ProgramInstruction originalMulInstruction =
      inputProgram.Instructions[static_cast<size_t>(sequenceStartIndex + 1)];
  const size_t initialInstructionCount = inputProgram.Instructions.size();

  const char *recipeText = R"YAML(version: 1
steps:
  - name: replace_frc_mul_sequence
    rules:
      - match:
          rewrite_mode: ReplaceRange
          sequence:
            - opcode: frc
              capture: ign_frc
            - opcode: mul
              capture: ign_mul
              operands:
                - capture: dst
                - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - capture: src
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-sequence-test")) {
    std::cerr << "Failed to parse inline SM5 sequence recipe: "
              << parseResult.Error << "\n";
    return 1;
  }

  const auto patchResult =
      dxp::sm5::PatchContainer(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with sequence recipe: "
              << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.Report.OutputContainer.Format != "DXBC") {
    std::cerr << "Expected SM5 patch report to identify DXBC output format.\n";
    return 1;
  }

  if (patchResult.Report.Steps.size() != parseResult.Recipe.GetSteps().size()) {
    std::cerr << "Expected SM5 patch report to record one entry per executed "
                 "recipe step.\n";
    return 1;
  }

  if (patchResult.Report.Steps.empty() ||
      patchResult.Report.Steps.front().Name != "replace_frc_mul_sequence" ||
      !patchResult.Report.Steps.front().Executed ||
      !patchResult.Report.Steps.front().Success ||
      !patchResult.Report.Steps.front().Changed ||
      patchResult.Report.Steps.front().MatchCount == 0) {
    std::cerr << "Expected SM5 patch report to describe the executed rewrite "
                 "step.\n";
    return 1;
  }

  if (patchResult.Report.OutputContainer.TotalSizeInBytes !=
      patchResult.OutputBytes.size()) {
    std::cerr << "Expected SM5 patch report to expose final DXBC container "
                 "size.\n";
    return 1;
  }

  if (patchResult.Report.OutputContainer.HashHex.size() != 32) {
    std::cerr << "Expected SM5 patch report to expose a 32-character DXBC "
                 "hash.\n";
    return 1;
  }

  if (patchResult.Report.OutputContainer.Chunks.empty()) {
    std::cerr << "Expected SM5 patch report to enumerate DXBC chunks.\n";
    return 1;
  }

  bool foundShaderChunk = false;
  for (const auto &chunk : patchResult.Report.OutputContainer.Chunks) {
    if (chunk.SizeInBytes == 0) {
      std::cerr << "Expected SM5 patch report chunk sizes to be populated.\n";
      return 1;
    }

    if (chunk.Id == "SHDR" || chunk.Id == "SHEX") {
      foundShaderChunk = true;
    }
  }

  if (!foundShaderChunk) {
    std::cerr << "Expected SM5 patch report to include the shader chunk.\n";
    return 1;
  }

  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Failed to inspect patched SM5 program: " << inspectError
              << "\n";
    return 1;
  }

  if (patchedProgram.Instructions.size() != initialInstructionCount - 1) {
    std::cerr << "Expected sequence replacement to reduce instruction count by "
                 "one.\n";
    return 1;
  }

  const auto &patchedInstruction =
      patchedProgram.Instructions[static_cast<size_t>(sequenceStartIndex)];
  if (patchedInstruction.Opcode != D3D10_SB_OPCODE_MOV) {
    std::cerr << "Expected the matched FRC/MUL sequence to become a MOV "
                 "instruction.\n";
    return 1;
  }

  if (patchedInstruction.Operands.size() != 2) {
    std::cerr
        << "Expected emitted MOV instruction to have exactly two operands.\n";
    return 1;
  }

  if (!OperandsEqual(patchedInstruction.Operands[0],
                     originalMulInstruction.Operands[0])) {
    std::cerr << "Expected sequence replacement to preserve the captured MUL "
                 "destination operand.\n";
    return 1;
  }

  if (!OperandsEqual(patchedInstruction.Operands[1],
                     originalMulInstruction.Operands[1])) {
    std::cerr << "Expected sequence replacement to preserve the captured MUL "
                 "source operand.\n";
    return 1;
  }

  const int initialFrcCount = CountOpcode(inputProgram, D3D10_SB_OPCODE_FRC);
  const char *matchOnlyRecipeText = R"YAML(version: 1
steps:
  - name: match_only_probe
    required: true
    mode: First
    rules:
      - match:
          opcode: frc
          capture: ign_frc
          rewrite_mode: None
)YAML";

  dxp::sm5::RecipeParseResult matchOnlyParseResult;
  if (!dxp::sm5::ParseRecipeText(matchOnlyRecipeText, matchOnlyParseResult,
                                 "inline-sm5-match-only-test")) {
    std::cerr << "Failed to parse inline SM5 match-only recipe: "
              << matchOnlyParseResult.Error << "\n";
    return 1;
  }

  const auto matchOnlyPatchResult =
      dxp::sm5::PatchContainer(inputBytes, matchOnlyParseResult.Recipe);
  if (!matchOnlyPatchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with match-only recipe: "
              << matchOnlyPatchResult.Error << "\n";
    return 1;
  }

  if (matchOnlyPatchResult.RecipeContext.TotalRuleMatches == 0) {
    std::cerr
        << "Expected SM5 match-only recipe to report at least one match.\n";
    return 1;
  }

  if (matchOnlyPatchResult.RecipeContext.ProgramModified) {
    std::cerr
        << "Expected SM5 match-only recipe to avoid modifying the program.\n";
    return 1;
  }

  dxp::sm5::ProgramInspection matchOnlyProgram;
  if (!dxp::sm5::InspectProgram(matchOnlyPatchResult.OutputBytes,
                                matchOnlyProgram, &inspectError)) {
    std::cerr << "Failed to inspect SM5 match-only patched program: "
              << inspectError << "\n";
    return 1;
  }

  if (matchOnlyProgram.Instructions.size() !=
      inputProgram.Instructions.size()) {
    std::cerr
        << "Expected SM5 match-only recipe to preserve instruction count.\n";
    return 1;
  }

  if (CountOpcode(matchOnlyProgram, D3D10_SB_OPCODE_FRC) != initialFrcCount) {
    std::cerr
        << "Expected SM5 match-only recipe to preserve Frc opcode count.\n";
    return 1;
  }

  std::cout << "SM5 sequence replacement succeeded and SM5 match-only rules "
               "reported matches without mutating the program.\n";
  return 0;
}