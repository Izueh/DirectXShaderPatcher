#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/RecipeParse.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {

static uint32_t FloatAsUint(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

static int CountOpcode(const dxp::sm5::Program &program,
                       dxp::sm5::OpcodeType opcode) {
  int count = 0;
  for (const auto &instruction : program.Instructions) {
    if (static_cast<dxp::sm5::OpcodeType>(instruction.Opcode) == opcode) {
      ++count;
    }
  }
  return count;
}

static bool IsIndexedDeclOpcode(dxp::sm5::OpcodeType opcode) {
  return opcode == D3D10_SB_OPCODE_DCL_INPUT ||
         opcode == D3D10_SB_OPCODE_DCL_INPUT_PS ||
         opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SGV ||
         opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV;
}

static int CountInputPsWithInterpolationMode(const dxp::sm5::Program &program,
                                             uint32_t interpolationMode) {
  int count = 0;
  for (const auto &instruction : program.Instructions) {
    if (static_cast<dxp::sm5::OpcodeType>(instruction.Opcode) !=
        D3D10_SB_OPCODE_DCL_INPUT_PS) {
      continue;
    }
    if (!instruction.Controls.HasInputInterpolationMode) {
      continue;
    }
    if (instruction.Controls.InputInterpolationMode == interpolationMode) {
      ++count;
    }
  }
  return count;
}

static int CountInputDeclOpcodes(const dxp::sm5::Program &program) {
  int count = 0;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = static_cast<dxp::sm5::OpcodeType>(instruction.Opcode);
    if (IsIndexedDeclOpcode(opcode)) {
      ++count;
    }
  }
  return count;
}

static std::filesystem::path RepoRootPath() {
  return std::filesystem::path(__FILE__).parent_path().parent_path();
}

static bool IsReg(const dxp::sm5::Operand &operand,
                  dxp::sm5::OperandType type,
                  uint32_t index) {
  return operand.Type == type && !operand.Indices.empty() &&
         operand.Indices.front() == index;
}

static bool HasInjectedBlockBeforeRet(const dxp::sm5::Program &program,
                                      uint32_t inputX,
                                      uint32_t inputY,
                                      uint32_t outputX) {
  if (program.Instructions.size() < 5) {
    return false;
  }

  for (size_t i = 4; i < program.Instructions.size(); ++i) {
    const auto retOp = static_cast<dxp::sm5::OpcodeType>(program.Instructions[i].Opcode);
    if (retOp != D3D10_SB_OPCODE_RET) {
      continue;
    }

    const auto div0Op = static_cast<dxp::sm5::OpcodeType>(program.Instructions[i - 4].Opcode);
    const auto div1Op = static_cast<dxp::sm5::OpcodeType>(program.Instructions[i - 3].Opcode);
    const auto addOp = static_cast<dxp::sm5::OpcodeType>(program.Instructions[i - 2].Opcode);
    const auto mulOp = static_cast<dxp::sm5::OpcodeType>(program.Instructions[i - 1].Opcode);
    if (div0Op != D3D10_SB_OPCODE_DIV || div1Op != D3D10_SB_OPCODE_DIV ||
        addOp != D3D10_SB_OPCODE_ADD || mulOp != D3D10_SB_OPCODE_MUL) {
      continue;
    }

    const auto &div0 = program.Instructions[i - 4];
    const auto &div1 = program.Instructions[i - 3];
    const auto &add = program.Instructions[i - 2];
    const auto &mul = program.Instructions[i - 1];
    if (div0.Operands.size() < 3 || div1.Operands.size() < 3 ||
        add.Operands.size() < 3 || mul.Operands.size() < 3) {
      continue;
    }

    if (!IsReg(div0.Operands[1], D3D10_SB_OPERAND_TYPE_INPUT, inputX) ||
        !IsReg(div0.Operands[2], D3D10_SB_OPERAND_TYPE_INPUT, inputX)) {
      continue;
    }

    if (!IsReg(div1.Operands[1], D3D10_SB_OPERAND_TYPE_INPUT, inputY) ||
        !IsReg(div1.Operands[2], D3D10_SB_OPERAND_TYPE_INPUT, inputY)) {
      continue;
    }

    if (div0.Operands[0].Type != D3D10_SB_OPERAND_TYPE_TEMP || div0.Operands[0].Indices.empty()) {
      continue;
    }

    const uint32_t tempIndex = div0.Operands[0].Indices.front();
    if (!IsReg(div1.Operands[0], D3D10_SB_OPERAND_TYPE_TEMP, tempIndex) ||
        !IsReg(add.Operands[0], D3D10_SB_OPERAND_TYPE_TEMP, tempIndex) ||
        !IsReg(mul.Operands[1], D3D10_SB_OPERAND_TYPE_TEMP, tempIndex)) {
      continue;
    }

    if (!IsReg(mul.Operands[0], D3D10_SB_OPERAND_TYPE_OUTPUT, outputX)) {
      continue;
    }

    if (mul.Operands[2].Type != D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
        mul.Operands[2].ImmediateValues.size() != 4) {
      continue;
    }

    const auto &imm = mul.Operands[2].ImmediateValues;
    if (imm[0] != FloatAsUint(0.5f) || imm[1] != FloatAsUint(-0.5f) ||
        imm[2] != FloatAsUint(0.0f) || imm[3] != FloatAsUint(0.0f)) {
      continue;
    }

    return true;
  }

  return false;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_screen_uv_insert <input.ps_5_0.cso>\n";
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

  const int initialInputDecls = CountInputDeclOpcodes(inputProgram);
  const int initialInputPsDecls = CountOpcode(inputProgram, D3D10_SB_OPCODE_DCL_INPUT_PS);
  const int initialOutputDecls = CountOpcode(inputProgram, D3D10_SB_OPCODE_DCL_OUTPUT);
  const uint32_t initialTempCount = inputProgram.TempCount;

  dxp::sm5::RecipeParseResult parseResult;
  const std::filesystem::path recipePath =
      RepoRootPath() / "recipes" / "physically_based_standard_screen_uv.recipe.yml";
  if (!dxp::sm5::ParseRecipeFile(recipePath.string(), parseResult)) {
    std::cerr << "Failed to parse SM5 recipe file: " << parseResult.Error << "\n";
    return 1;
  }

  const auto patchResult = dxp::sm5::PatchContainerInMemory(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader: " << patchResult.Error << "\n";
    return 1;
  }

  dxp::sm5::Container patchedContainer;
  if (!dxp::sm5::ParseDxbcContainer(patchResult.OutputBytes, patchedContainer)) {
    std::cerr << "Failed to parse patched DXBC container.\n";
    return 1;
  }

  dxp::sm5::Program patchedProgram;
  if (!dxp::sm5::ParseShaderChunk(patchedContainer, patchedProgram)) {
    std::cerr << "Failed to parse patched SM5 program.\n";
    return 1;
  }

  const int patchedInputDecls = CountInputDeclOpcodes(patchedProgram);
  const int patchedInputPsDecls = CountOpcode(patchedProgram, D3D10_SB_OPCODE_DCL_INPUT_PS);
  const int patchedLinearInputPsDecls =
      CountInputPsWithInterpolationMode(patchedProgram, D3D10_SB_INTERPOLATION_LINEAR);
  const int patchedOutputDecls = CountOpcode(patchedProgram, D3D10_SB_OPCODE_DCL_OUTPUT);

  if (patchedInputDecls < initialInputDecls + 2) {
    std::cerr << "Expected two additional input declarations (initial_total="
              << initialInputDecls << ", patched_total=" << patchedInputDecls
              << ", initial_dcl_input_ps=" << initialInputPsDecls
              << ", patched_dcl_input_ps=" << patchedInputPsDecls
              << ").\n";
    return 1;
  }

  if (patchedOutputDecls < initialOutputDecls + 1) {
    std::cerr << "Expected one additional dcl_output declaration (initial="
              << initialOutputDecls << ", patched=" << patchedOutputDecls
              << ").\n";
    return 1;
  }

  if (patchedLinearInputPsDecls < initialInputPsDecls + 2) {
    std::cerr << "Expected injected dcl_input_ps declarations to use explicit linear interpolation mode.\n";
    return 1;
  }

  if (patchedProgram.TempCount <= initialTempCount) {
    std::cerr << "Expected injected math to allocate a new temp register.\n";
    return 1;
  }

  constexpr uint32_t kInjectedInputX = 13;
  constexpr uint32_t kInjectedInputY = 14;
  constexpr uint32_t kInjectedOutputX = 1;
  if (!HasInjectedBlockBeforeRet(patchedProgram,
                                 kInjectedInputX,
                                 kInjectedInputY,
                                 kInjectedOutputX)) {
    std::cerr << "Failed to locate the injected div/div/add/mul block before ret.\n";
    return 1;
  }

  std::cout << "SM5 declarative recipe inserted IO declarations and screen-UV block with "
            << "v" << kInjectedInputX << ", v" << kInjectedInputY
            << ", and o" << kInjectedOutputX << ".\n";
  return 0;
}
