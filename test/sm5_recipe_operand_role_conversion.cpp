#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Recipe.h"
#include "dxp/sm5/RecipeParse.h"
#include "dxp/sm5/Model.h"

#include <iostream>
#include <vector>





namespace {



static int FindFirstOpcodeWithSwizzleSrc(
    const dxp::sm5::ProgramInspection &program, uint32_t opcode,
    size_t srcIndex) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (instruction.Opcode != opcode ||
        instruction.Operands.size() <= srcIndex) {
      continue;
    }
    const auto &src = instruction.Operands[srcIndex];
    if (src.NumComponents == D3D10_SB_OPERAND_4_COMPONENT &&
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
            src.ComponentMode) ==
            static_cast<uint32_t>(
                D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)) {
      return static_cast<int>(index);
    }
  }
  return -1;
}



static int FindFirstOpcodeWithMaskDst(
    const dxp::sm5::ProgramInspection &program, uint32_t opcode) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (instruction.Opcode != opcode ||
        instruction.Operands.empty()) {
      continue;
    }
    const auto &dst = instruction.Operands[0];
    if (dst.NumComponents != D3D10_SB_OPERAND_4_COMPONENT) {
      continue;
    }
    const uint32_t selMode =
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(dst.ComponentMode);
    const uint32_t mask =
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(dst.ComponentMode);
    if (selMode ==
            static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) &&
        mask != 0) {
      return static_cast<int>(index);
    }
  }
  return -1;
}



static int FindFirstOpcodeWithNOSWIZZLESrc(
    const dxp::sm5::ProgramInspection &program, uint32_t opcode,
    size_t srcIndex) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (instruction.Opcode != opcode ||
        instruction.Operands.size() <= srcIndex) {
      continue;
    }
    const auto &src = instruction.Operands[srcIndex];
    if (src.NumComponents == D3D10_SB_OPERAND_4_COMPONENT &&
        src.ComponentMode == D3D10_SB_OPERAND_4_COMPONENT_NOSWIZZLE) {
      return static_cast<int>(index);
    }
  }
  return -1;
}



static int FindFirstOpcodeWithSelect1Src(
    const dxp::sm5::ProgramInspection &program, uint32_t opcode,
    size_t srcIndex) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (instruction.Opcode != opcode ||
        instruction.Operands.size() <= srcIndex) {
      continue;
    }
    const auto &src = instruction.Operands[srcIndex];
    if (src.NumComponents == D3D10_SB_OPERAND_4_COMPONENT &&
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
            src.ComponentMode) ==
            static_cast<uint32_t>(
                D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE)) {
      return static_cast<int>(index);
    }
  }
  return -1;
}



static int FindFirstOpcodeWithMaskSrc(
    const dxp::sm5::ProgramInspection &program, uint32_t opcode,
    size_t srcIndex) {
  for (size_t index = 0; index < program.Instructions.size(); ++index) {
    const auto &instruction = program.Instructions[index];
    if (instruction.Opcode != opcode ||
        instruction.Operands.size() <= srcIndex) {
      continue;
    }
    const auto &src = instruction.Operands[srcIndex];
    if (src.NumComponents != D3D10_SB_OPERAND_4_COMPONENT) {
      continue;
    }
    const uint32_t selMode =
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(src.ComponentMode);
    const uint32_t mask =
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(src.ComponentMode);
    if (selMode ==
            static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) &&
        mask != 0) {
      return static_cast<int>(index);
    }
  }
  return -1;
}


static int CountSetBits(uint32_t value) {
  int count = 0;
  for (int i = 0; i < 4; ++i) {
    if (value & (1u << i)) {
      ++count;
    }
  }
  return count;
}

}










static int test_source_to_destination_conversion(
    const dxp::sm5::ProgramInspection &inputProgram,
    const std::vector<uint8_t> &inputBytes) {
  const uint32_t opcodesToTry[] = {
      D3D10_SB_OPCODE_ADD, D3D10_SB_OPCODE_MUL, D3D10_SB_OPCODE_MAD,
      D3D10_SB_OPCODE_ULT, D3D10_SB_OPCODE_EQ, D3D10_SB_OPCODE_MOV,
      D3D10_SB_OPCODE_MIN, D3D10_SB_OPCODE_MAX, D3D10_SB_OPCODE_DP3,
      D3D10_SB_OPCODE_FRC, D3D10_SB_OPCODE_FTOI, D3D10_SB_OPCODE_FTOU,
      D3D10_SB_OPCODE_LOG, D3D10_SB_OPCODE_EXP, D3D10_SB_OPCODE_RSQ,
      D3D10_SB_OPCODE_SQRT, D3D10_SB_OPCODE_DIV,
  };

  int targetIndex = -1;
  uint32_t targetOpcode = 0;
  for (uint32_t op : opcodesToTry) {
    const int idx = FindFirstOpcodeWithSwizzleSrc(inputProgram, op, 1);
    if (idx >= 0) {
      targetIndex = idx;
      targetOpcode = op;
      break;
    }
  }

  if (targetIndex < 0) {
    std::cerr << "Test 7a: Failed to find instruction with SWIZZLE source.\n";
    return 1;
  }

  const auto &originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetIndex)];
  const auto &originalSrc = originalInstruction.Operands[1];

  std::cerr << "Test 7a: Found instruction " << targetIndex
            << " opcode " << targetOpcode << " src NumComp="
            << originalSrc.NumComponents << " ComponentMode=" << originalSrc.ComponentMode << "\n";



  dxp::sm5::Recipe recipe;
  dxp::sm5::RecipeRule rule;
  rule.Named("src_to_dst")
      .WithMatch(dxp::sm5::RecipeMatchPattern{}
                     .WithOpcode(
                         dxp::sm5::GetOpcodeName(
                             dxp::sm5::Opcode{targetOpcode}))
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("dst")
                                     .Build())
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("src")
                                     .Build()))
      .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
                   .WithOpcode("mov")
                   .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                   .CaptureAs("src")
                                   .WithCaptureFieldComponents(true)
                                   .Build()));

  recipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
      "src_to_dst", {std::move(rule)},
      dxp::sm5::RecipeRuleApplicationMode::First, true));

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, recipe);
  if (!patchResult.Success) {
    std::cerr << "Test 7a: Patch failed: " << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.Report.Steps.size() != 1 ||
      patchResult.Report.Steps.front().Rules.size() != 1 ||
      patchResult.Report.Steps.front().Rules.front().AppliedCount == 0) {
    std::cerr << "Test 7a: Rule reported zero matches.\n";
    return 1;
  }

  std::string inspectError;
  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Test 7a: Inspect failed: " << inspectError << "\n";
    return 1;
  }

  const auto &patchedInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetIndex)];
  if (patchedInstruction.Operands.empty()) {
    std::cerr << "Test 7a: Patched instruction has no operands.\n";
    return 1;
  }

  const auto &patchedDst = patchedInstruction.Operands[0];
  const uint32_t selMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
          patchedDst.ComponentMode);
  const uint32_t mask =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(patchedDst.ComponentMode);

  std::cerr << "Test 7a: patchedDst NumComp=" << patchedDst.NumComponents
            << " ComponentMode=" << patchedDst.ComponentMode
            << " selMode=" << selMode << " mask=" << mask << "\n";

  if (selMode !=
      static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)) {
    std::cerr << "Test 7a: Expected MASK selMode, got " << selMode << "\n";
    return 1;
  }


  if (mask == 0) {
    std::cerr << "Test 7a: Expected non-zero mask from SWIZZLE conversion.\n";
    return 1;
  }

  return 0;
}










static int test_destination_to_source_conversion(
    const dxp::sm5::ProgramInspection &inputProgram,
    const std::vector<uint8_t> &inputBytes) {

  const uint32_t opcodesToTry[] = {
      D3D10_SB_OPCODE_MOV, D3D10_SB_OPCODE_ADD, D3D10_SB_OPCODE_MUL,
      D3D10_SB_OPCODE_MAD, D3D10_SB_OPCODE_DP3, D3D10_SB_OPCODE_AND,
  };

  int targetIndex = -1;
  for (uint32_t op : opcodesToTry) {
    const int idx = FindFirstOpcodeWithMaskDst(inputProgram, op);
    if (idx >= 0) {
      targetIndex = idx;
      break;
    }
  }

  if (targetIndex < 0) {
    std::cerr << "Test 7b: Failed to find instruction with mask destination.\n";
    return 1;
  }

  const auto &originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetIndex)];
  const auto &originalDst = originalInstruction.Operands[0];

  dxp::sm5::Recipe recipe;
  dxp::sm5::RecipeRule rule;
  rule.Named("dst_to_src")
      .WithMatch(dxp::sm5::RecipeMatchPattern{}
                     .WithOpcode(
                         dxp::sm5::GetOpcodeName(
                             dxp::sm5::Opcode{originalInstruction.Opcode}))
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("dst")
                                     .Build())
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("src")
                                     .Build()))
      .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
                   .WithOpcode("mov")
                   .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                   .WithType("temp")
                                   .Build())
                   .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                   .CaptureAs("dst")
                                   .WithCaptureFieldComponents(true)
                                   .Build()));

  recipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
      "dst_to_src", {std::move(rule)},
      dxp::sm5::RecipeRuleApplicationMode::First, true));

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, recipe);
  if (!patchResult.Success) {
    std::cerr << "Test 7b: Patch failed: " << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.Report.Steps.size() != 1 ||
      patchResult.Report.Steps.front().Rules.size() != 1 ||
      patchResult.Report.Steps.front().Rules.front().AppliedCount == 0) {
    std::cerr << "Test 7b: Rule reported zero matches.\n";
    return 1;
  }

  std::string inspectError;
  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Test 7b: Inspect failed: " << inspectError << "\n";
    return 1;
  }

  const auto &patchedInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetIndex)];
  if (patchedInstruction.Operands.size() < 2) {
    std::cerr << "Test 7b: Patched instruction has insufficient operands.\n";
    return 1;
  }

  const auto &patchedSrc = patchedInstruction.Operands[1];
  const uint32_t selMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
          patchedSrc.ComponentMode);



  if (selMode ==
      static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE)) {

  } else if (selMode ==
      static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE)) {

    const uint32_t swizzle =
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(patchedSrc.ComponentMode);
    if (swizzle == 0) {
      std::cerr << "Test 7b: Expected non-zero swizzle from mask conversion.\n";
      return 1;
    }
  } else {
    std::cerr << "Test 7b: Expected SWIZZLE or SELECT_1 selMode, got "
              << selMode << "\n";
    return 1;
  }

  return 0;
}





static int test_swizzle_to_mask_conversion(
    const dxp::sm5::ProgramInspection &inputProgram,
    const std::vector<uint8_t> &inputBytes) {
  int targetIndex = -1;
  uint32_t targetOpcode = 0;
  size_t srcOperandIndex = 1;

  const uint32_t opcodesToTry[] = {
      D3D10_SB_OPCODE_MOV, D3D10_SB_OPCODE_ADD, D3D10_SB_OPCODE_MUL,
      D3D10_SB_OPCODE_MAD, D3D10_SB_OPCODE_FRC, D3D10_SB_OPCODE_DP3,
      D3D10_SB_OPCODE_MIN, D3D10_SB_OPCODE_MAX, D3D10_SB_OPCODE_FTOI,
      D3D10_SB_OPCODE_FTOU, D3D10_SB_OPCODE_LOG, D3D10_SB_OPCODE_RSQ,
      D3D10_SB_OPCODE_SQRT, D3D10_SB_OPCODE_DIV,
  };

  for (uint32_t op : opcodesToTry) {
    const int idx = FindFirstOpcodeWithSwizzleSrc(inputProgram, op, 1);
    if (idx >= 0) {
      targetIndex = idx;
      targetOpcode = op;
      srcOperandIndex = 1;
      break;
    }
  }

  if (targetIndex < 0) {
    for (uint32_t op : opcodesToTry) {
      const int idx = FindFirstOpcodeWithSwizzleSrc(inputProgram, op, 0);
      if (idx >= 0) {
        targetIndex = idx;
        targetOpcode = op;
        srcOperandIndex = 0;
        break;
      }
    }
  }

  if (targetIndex < 0) {
    return 0;
  }

  const auto &originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetIndex)];
  const auto &originalSrc =
      originalInstruction.Operands[srcOperandIndex];

  dxp::sm5::Recipe recipe;
  dxp::sm5::RecipeRule rule;
  rule.Named("swizzle_to_mask")
      .WithMatch(dxp::sm5::RecipeMatchPattern{}
                     .WithOpcode(
                         dxp::sm5::GetOpcodeName(
                             dxp::sm5::Opcode{targetOpcode}))
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("dst")
                                     .Build())
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("src")
                                     .Build()))
      .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
                   .WithOpcode("mov")
                   .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                   .CaptureAs("src")
                                   .WithCaptureFieldComponents(true)
                                   .Build()));

  recipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
      "swizzle_mask", {std::move(rule)},
      dxp::sm5::RecipeRuleApplicationMode::First, true));

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, recipe);
  if (!patchResult.Success) {
    std::cerr << "Test 7c: Patch failed: " << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.Report.Steps.size() != 1 ||
      patchResult.Report.Steps.front().Rules.size() != 1 ||
      patchResult.Report.Steps.front().Rules.front().AppliedCount == 0) {
    std::cerr << "Test 7c: Rule reported zero matches.\n";
    return 1;
  }

  std::string inspectError;
  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Test 7c: Inspect failed: " << inspectError << "\n";
    return 1;
  }

  const auto &patchedInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetIndex)];
  if (patchedInstruction.Operands.empty()) {
    std::cerr << "Test 7c: Patched instruction has no operands.\n";
    return 1;
  }

  const auto &patchedDst = patchedInstruction.Operands[0];
  const uint32_t patchedSelMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
          patchedDst.ComponentMode);
  const uint32_t mask =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(patchedDst.ComponentMode);

  if (patchedSelMode !=
      static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)) {
    std::cerr << "Test 7c: Expected MASK selMode, got " << patchedSelMode
              << "\n";
    return 1;
  }
  if (mask == 0) {
    std::cerr << "Test 7c: Expected non-zero mask from SWIZZLE conversion.\n";
    return 1;
  }

  return 0;
}









static int test_noswizzle_to_mask_conversion(
    const dxp::sm5::ProgramInspection &inputProgram,
    const std::vector<uint8_t> &inputBytes) {
  const uint32_t opcodesToTry[] = {
      D3D10_SB_OPCODE_ADD, D3D10_SB_OPCODE_MUL, D3D10_SB_OPCODE_MAD,
      D3D10_SB_OPCODE_ULT, D3D10_SB_OPCODE_EQ, D3D10_SB_OPCODE_MOV,
      D3D10_SB_OPCODE_MIN, D3D10_SB_OPCODE_MAX, D3D10_SB_OPCODE_DP3,
      D3D10_SB_OPCODE_FRC, D3D10_SB_OPCODE_FTOI, D3D10_SB_OPCODE_FTOU,
      D3D10_SB_OPCODE_LOG, D3D10_SB_OPCODE_EXP, D3D10_SB_OPCODE_RSQ,
      D3D10_SB_OPCODE_SQRT, D3D10_SB_OPCODE_DIV,
  };

  int targetIndex = -1;
  uint32_t targetOpcode = 0;
  for (uint32_t op : opcodesToTry) {
    const int idx = FindFirstOpcodeWithNOSWIZZLESrc(inputProgram, op, 1);
    if (idx >= 0) {
      targetIndex = idx;
      targetOpcode = op;
      break;
    }
  }

  if (targetIndex < 0) {
    std::cerr << "Test 7d: Skipped, no NOSWIZZLE source in this shader.\n";
    return 0;
  }

  const auto &originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetIndex)];
  const auto &originalSrc = originalInstruction.Operands[1];

  std::cerr << "Test 7d: Found instruction " << targetIndex
            << " opcode " << targetOpcode << " src NumComp="
            << originalSrc.NumComponents << " ComponentMode=" << originalSrc.ComponentMode << "\n";

  dxp::sm5::Recipe recipe;
  dxp::sm5::RecipeRule rule;
  rule.Named("noswizzle_to_mask")
      .WithMatch(dxp::sm5::RecipeMatchPattern{}
                     .WithOpcode(
                         dxp::sm5::GetOpcodeName(
                             dxp::sm5::Opcode{targetOpcode}))
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("dst")
                                     .Build())
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("src")
                                     .Build()))
      .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
                   .WithOpcode("mov")
                   .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                   .CaptureAs("src")
                                   .WithCaptureFieldComponents(true)
                                   .Build()));

  recipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
      "noswizzle_to_mask", {std::move(rule)},
      dxp::sm5::RecipeRuleApplicationMode::First, true));

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, recipe);
  if (!patchResult.Success) {
    std::cerr << "Test 7d: Patch failed: " << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.Report.Steps.size() != 1 ||
      patchResult.Report.Steps.front().Rules.size() != 1 ||
      patchResult.Report.Steps.front().Rules.front().AppliedCount == 0) {
    std::cerr << "Test 7d: Rule reported zero matches.\n";
    return 1;
  }

  std::string inspectError;
  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Test 7d: Inspect failed: " << inspectError << "\n";
    return 1;
  }

  const auto &patchedInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetIndex)];
  if (patchedInstruction.Operands.empty()) {
    std::cerr << "Test 7d: Patched instruction has no operands.\n";
    return 1;
  }

  const auto &patchedDst = patchedInstruction.Operands[0];
  const uint32_t selMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
          patchedDst.ComponentMode);
  const uint32_t mask =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(patchedDst.ComponentMode);

  std::cerr << "Test 7d: patchedDst NumComp=" << patchedDst.NumComponents
            << " ComponentMode=" << patchedDst.ComponentMode
            << " selMode=" << selMode << " mask=" << mask << "\n";

  if (selMode !=
      static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)) {
    std::cerr << "Test 7d: Expected MASK selMode, got " << selMode << "\n";
    return 1;
  }

  if (mask != D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL) {
    std::cerr << "Test 7d: Expected full mask (xyzw = 0xF0), got mask=" << mask << "\n";
    return 1;
  }

  return 0;
}









static int test_select1_to_mask_conversion(
    const dxp::sm5::ProgramInspection &inputProgram,
    const std::vector<uint8_t> &inputBytes) {
  const uint32_t opcodesToTry[] = {
      D3D10_SB_OPCODE_ADD, D3D10_SB_OPCODE_MUL, D3D10_SB_OPCODE_MAD,
      D3D10_SB_OPCODE_ULT, D3D10_SB_OPCODE_EQ, D3D10_SB_OPCODE_MOV,
      D3D10_SB_OPCODE_MIN, D3D10_SB_OPCODE_MAX, D3D10_SB_OPCODE_DP3,
      D3D10_SB_OPCODE_FRC, D3D10_SB_OPCODE_FTOI, D3D10_SB_OPCODE_FTOU,
      D3D10_SB_OPCODE_LOG, D3D10_SB_OPCODE_EXP, D3D10_SB_OPCODE_RSQ,
      D3D10_SB_OPCODE_SQRT, D3D10_SB_OPCODE_DIV,
  };

  int targetIndex = -1;
  uint32_t targetOpcode = 0;
  size_t srcOperandIndex = 1;
  for (uint32_t op : opcodesToTry) {
    const int idx = FindFirstOpcodeWithSelect1Src(inputProgram, op, 1);
    if (idx >= 0) {
      targetIndex = idx;
      targetOpcode = op;
      break;
    }
  }

  if (targetIndex < 0) {
    for (uint32_t op : opcodesToTry) {
      const int idx = FindFirstOpcodeWithSelect1Src(inputProgram, op, 0);
      if (idx >= 0) {
        targetIndex = idx;
        targetOpcode = op;
        srcOperandIndex = 0;
        break;
      }
    }
  }

  if (targetIndex < 0) {
    std::cerr << "Test 7e: Failed to find instruction with SELECT_1 source.\n";
    return 1;
  }

  const auto &originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetIndex)];
  const auto &originalSrc = originalInstruction.Operands[srcOperandIndex];

  std::cerr << "Test 7e: Found instruction " << targetIndex
            << " opcode " << targetOpcode << " src NumComp="
            << originalSrc.NumComponents << " ComponentMode=" << originalSrc.ComponentMode << "\n";

  dxp::sm5::Recipe recipe;
  dxp::sm5::RecipeRule rule;
  rule.Named("select1_to_mask")
      .WithMatch(dxp::sm5::RecipeMatchPattern{}
                     .WithOpcode(
                         dxp::sm5::GetOpcodeName(
                             dxp::sm5::Opcode{targetOpcode}))
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("dst")
                                     .Build())
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("src")
                                     .Build()))
      .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
                   .WithOpcode("mov")
                   .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                   .CaptureAs("src")
                                   .WithCaptureFieldComponents(true)
                                   .Build()));

  recipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
      "select1_to_mask", {std::move(rule)},
      dxp::sm5::RecipeRuleApplicationMode::First, true));

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, recipe);
  if (!patchResult.Success) {
    std::cerr << "Test 7e: Patch failed: " << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.Report.Steps.size() != 1 ||
      patchResult.Report.Steps.front().Rules.size() != 1 ||
      patchResult.Report.Steps.front().Rules.front().AppliedCount == 0) {
    std::cerr << "Test 7e: Rule reported zero matches.\n";
    return 1;
  }

  std::string inspectError;
  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Test 7e: Inspect failed: " << inspectError << "\n";
    return 1;
  }

  const auto &patchedInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetIndex)];
  if (patchedInstruction.Operands.empty()) {
    std::cerr << "Test 7e: Patched instruction has no operands.\n";
    return 1;
  }

  const auto &patchedDst = patchedInstruction.Operands[0];
  const uint32_t selMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
          patchedDst.ComponentMode);
  const uint32_t mask =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(patchedDst.ComponentMode);

  std::cerr << "Test 7e: patchedDst NumComp=" << patchedDst.NumComponents
            << " ComponentMode=" << patchedDst.ComponentMode
            << " selMode=" << selMode << " mask=" << mask << "\n";

  if (selMode !=
      static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)) {
    std::cerr << "Test 7e: Expected MASK selMode, got " << selMode << "\n";
    return 1;
  }

  const uint32_t maskBits = mask >> 4;
  if (CountSetBits(maskBits) != 1) {
    std::cerr << "Test 7e: Expected single-bit mask, got maskBits=" << maskBits << "\n";
    return 1;
  }

  return 0;
}









static int test_mask_to_mask_conversion(
    const dxp::sm5::ProgramInspection &inputProgram,
    const std::vector<uint8_t> &inputBytes) {
  const uint32_t opcodesToTry[] = {
      D3D10_SB_OPCODE_ADD, D3D10_SB_OPCODE_MUL, D3D10_SB_OPCODE_MAD,
      D3D10_SB_OPCODE_ULT, D3D10_SB_OPCODE_EQ, D3D10_SB_OPCODE_MOV,
      D3D10_SB_OPCODE_MIN, D3D10_SB_OPCODE_MAX, D3D10_SB_OPCODE_DP3,
      D3D10_SB_OPCODE_FRC, D3D10_SB_OPCODE_FTOI, D3D10_SB_OPCODE_FTOU,
      D3D10_SB_OPCODE_LOG, D3D10_SB_OPCODE_EXP, D3D10_SB_OPCODE_RSQ,
      D3D10_SB_OPCODE_SQRT, D3D10_SB_OPCODE_DIV,
  };

  int targetIndex = -1;
  uint32_t targetOpcode = 0;
  for (uint32_t op : opcodesToTry) {
    const int idx = FindFirstOpcodeWithMaskSrc(inputProgram, op, 1);
    if (idx >= 0) {
      targetIndex = idx;
      targetOpcode = op;
      break;
    }
  }

  if (targetIndex < 0) {
    std::cerr << "Test 7f: Skipped, no MASK source in this shader.\n";
    return 0;
  }

  const auto &originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetIndex)];
  const auto &originalSrc = originalInstruction.Operands[1];

  std::cerr << "Test 7f: Found instruction " << targetIndex
            << " opcode " << targetOpcode << " src NumComp="
            << originalSrc.NumComponents << " ComponentMode=" << originalSrc.ComponentMode << "\n";

  dxp::sm5::Recipe recipe;
  dxp::sm5::RecipeRule rule;
  rule.Named("mask_to_mask")
      .WithMatch(dxp::sm5::RecipeMatchPattern{}
                     .WithOpcode(
                         dxp::sm5::GetOpcodeName(
                             dxp::sm5::Opcode{targetOpcode}))
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("dst")
                                     .Build())
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("src")
                                     .Build()))
      .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
                   .WithOpcode("mov")
                   .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                   .CaptureAs("src")
                                   .WithCaptureFieldComponents(true)
                                   .Build()));

  recipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
      "mask_to_mask", {std::move(rule)},
      dxp::sm5::RecipeRuleApplicationMode::First, true));

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, recipe);
  if (!patchResult.Success) {
    std::cerr << "Test 7f: Patch failed: " << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.Report.Steps.size() != 1 ||
      patchResult.Report.Steps.front().Rules.size() != 1 ||
      patchResult.Report.Steps.front().Rules.front().AppliedCount == 0) {
    std::cerr << "Test 7f: Rule reported zero matches.\n";
    return 1;
  }

  std::string inspectError;
  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Test 7f: Inspect failed: " << inspectError << "\n";
    return 1;
  }

  const auto &patchedInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetIndex)];
  if (patchedInstruction.Operands.empty()) {
    std::cerr << "Test 7f: Patched instruction has no operands.\n";
    return 1;
  }

  const auto &patchedDst = patchedInstruction.Operands[0];
  const uint32_t selMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
          patchedDst.ComponentMode);
  const uint32_t mask =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(patchedDst.ComponentMode);

  std::cerr << "Test 7f: patchedDst NumComp=" << patchedDst.NumComponents
            << " ComponentMode=" << patchedDst.ComponentMode
            << " selMode=" << selMode << " mask=" << mask << "\n";

  if (selMode !=
      static_cast<uint32_t>(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE)) {
    std::cerr << "Test 7f: Expected MASK selMode, got " << selMode << "\n";
    return 1;
  }

  if (mask == 0) {
    std::cerr << "Test 7f: Expected non-zero mask.\n";
    return 1;
  }

  return 0;
}









static int test_same_role_no_op(
    const dxp::sm5::ProgramInspection &inputProgram,
    const std::vector<uint8_t> &inputBytes) {
  const uint32_t opcodesToTry[] = {
      D3D10_SB_OPCODE_ADD, D3D10_SB_OPCODE_MUL, D3D10_SB_OPCODE_MAD,
      D3D10_SB_OPCODE_ULT, D3D10_SB_OPCODE_EQ, D3D10_SB_OPCODE_MOV,
      D3D10_SB_OPCODE_MIN, D3D10_SB_OPCODE_MAX, D3D10_SB_OPCODE_DP3,
      D3D10_SB_OPCODE_FRC, D3D10_SB_OPCODE_FTOI, D3D10_SB_OPCODE_FTOU,
      D3D10_SB_OPCODE_LOG, D3D10_SB_OPCODE_EXP, D3D10_SB_OPCODE_RSQ,
      D3D10_SB_OPCODE_SQRT, D3D10_SB_OPCODE_DIV,
  };

  int targetIndex = -1;
  uint32_t targetOpcode = 0;
  for (uint32_t op : opcodesToTry) {
    const int idx = FindFirstOpcodeWithSwizzleSrc(inputProgram, op, 1);
    if (idx >= 0) {
      targetIndex = idx;
      targetOpcode = op;
      break;
    }
  }

  if (targetIndex < 0) {
    std::cerr << "Test 7g: Failed to find instruction with SWIZZLE source.\n";
    return 1;
  }

  const auto &originalInstruction =
      inputProgram.Instructions[static_cast<size_t>(targetIndex)];
  const auto &originalSrc = originalInstruction.Operands[1];

  std::cerr << "Test 7g: Found instruction " << targetIndex
            << " opcode " << targetOpcode << " src NumComp="
            << originalSrc.NumComponents << " ComponentMode=" << originalSrc.ComponentMode << "\n";


  dxp::sm5::Recipe recipe;
  dxp::sm5::RecipeRule rule;
  rule.Named("same_role_no_op")
      .WithMatch(dxp::sm5::RecipeMatchPattern{}
                     .WithOpcode(
                         dxp::sm5::GetOpcodeName(
                             dxp::sm5::Opcode{targetOpcode}))
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("dst")
                                     .Build())
                     .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                     .CaptureAs("src")
                                     .Build()))
      .AddEmit(dxp::sm5::RecipeInstructionTemplate{}
                   .WithOpcode("mov")
                   .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                   .WithType("temp")
                                   .Build())
                   .AddOperand(dxp::sm5::RecipeOperandPatternBuilder{}
                                   .CaptureAs("src")
                                   .WithCaptureFieldComponents(true)
                                   .Build()));

  recipe.AddStep(dxp::sm5::MakeRewriteRulesStep(
      "same_role_no_op", {std::move(rule)},
      dxp::sm5::RecipeRuleApplicationMode::First, true));

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, recipe);
  if (!patchResult.Success) {
    std::cerr << "Test 7g: Patch failed: " << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.Report.Steps.size() != 1 ||
      patchResult.Report.Steps.front().Rules.size() != 1 ||
      patchResult.Report.Steps.front().Rules.front().AppliedCount == 0) {
    std::cerr << "Test 7g: Rule reported zero matches.\n";
    return 1;
  }

  std::string inspectError;
  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Test 7g: Inspect failed: " << inspectError << "\n";
    return 1;
  }

  const auto &patchedInstruction =
      patchedProgram.Instructions[static_cast<size_t>(targetIndex)];
  if (patchedInstruction.Operands.size() < 2) {
    std::cerr << "Test 7g: Patched instruction has insufficient operands.\n";
    return 1;
  }

  const auto &patchedSrc = patchedInstruction.Operands[1];
  const uint32_t patchedSelMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
          patchedSrc.ComponentMode);
  const uint32_t originalSelMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
          originalSrc.ComponentMode);

  std::cerr << "Test 7g: patchedSrc NumComp=" << patchedSrc.NumComponents
            << " ComponentMode=" << patchedSrc.ComponentMode
            << " originalSelMode=" << originalSelMode
            << " patchedSelMode=" << patchedSelMode << "\n";


  if (patchedSrc.ComponentMode != originalSrc.ComponentMode) {
    std::cerr << "Test 7g: Expected no conversion (same role), but ComponentMode changed from "
              << originalSrc.ComponentMode << " to " << patchedSrc.ComponentMode << "\n";
    return 1;
  }

  return 0;
}





int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_operand_role_conversion <input.ps_5_0.cso>\n";
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

  int failureCount = 0;
  failureCount += test_source_to_destination_conversion(inputProgram, inputBytes);
  failureCount += test_destination_to_source_conversion(inputProgram, inputBytes);
  failureCount += test_swizzle_to_mask_conversion(inputProgram, inputBytes);
  failureCount += test_noswizzle_to_mask_conversion(inputProgram, inputBytes);
  failureCount += test_select1_to_mask_conversion(inputProgram, inputBytes);
  failureCount += test_mask_to_mask_conversion(inputProgram, inputBytes);
  failureCount += test_same_role_no_op(inputProgram, inputBytes);

  if (failureCount > 0) {
    std::cerr << "\n=== " << failureCount << " TEST(S) FAILED ===\n";
    return 1;
  }

  std::cerr << "All operand role conversion integration tests passed.\n";
  return 0;
}
