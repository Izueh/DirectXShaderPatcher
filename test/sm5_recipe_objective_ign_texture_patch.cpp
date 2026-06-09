#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <vector>

namespace {

static bool WriteFile(const std::string &path,
                      const std::vector<uint8_t> &bytes) {
  const std::filesystem::path outputPath(path);
  const std::filesystem::path parentPath = outputPath.parent_path();
  if (!parentPath.empty()) {
    std::error_code error;
    if (!std::filesystem::create_directories(parentPath, error) && error) {
      return false;
    }
  }

  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }

  if (!bytes.empty()) {
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }

  return static_cast<bool>(out);
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

static bool HasResourceDecl(const dxp::sm5::ProgramInspection &program,
                            uint32_t bindPoint) {
  for (const auto &instruction : program.Instructions) {
    if (instruction.Opcode != D3D10_SB_OPCODE_DCL_RESOURCE) {
      continue;
    }
    if (!instruction.Operands.empty() &&
        !instruction.Operands.front().Indices.empty() &&
        instruction.Operands.front().Indices.front() == bindPoint) {
      return true;
    }
  }
  return false;
}

static bool HasConstantBufferDecl(const dxp::sm5::ProgramInspection &program,
                                  uint32_t bindPoint) {
  for (const auto &instruction : program.Instructions) {
    if (instruction.Opcode != D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) {
      continue;
    }
    if (!instruction.Operands.empty() &&
        !instruction.Operands.front().Indices.empty() &&
        instruction.Operands.front().Indices.front() == bindPoint) {
      return true;
    }
  }
  return false;
}

static bool
HasTextureArraySampleUsingResource(const dxp::sm5::ProgramInspection &program,
                                   uint32_t bindPoint) {
  for (const auto &instruction : program.Instructions) {
    if (instruction.Opcode != D3D10_SB_OPCODE_SAMPLE_L) {
      continue;
    }
    if (instruction.Operands.size() < 3) {
      continue;
    }
    const auto &resourceOperand = instruction.Operands[2];
    if (resourceOperand.Type == D3D10_SB_OPERAND_TYPE_RESOURCE &&
        !resourceOperand.Indices.empty() &&
        resourceOperand.Indices.front() == bindPoint) {
      return true;
    }
  }
  return false;
}

static bool FindTextureArraySampleUsingResource(
    const dxp::sm5::ProgramInspection &program, uint32_t bindPoint,
    const dxp::sm5::ProgramInstruction *&instruction) {
  for (const auto &candidate : program.Instructions) {
    if (candidate.Opcode != D3D10_SB_OPCODE_SAMPLE_L) {
      continue;
    }
    if (candidate.Operands.size() < 3) {
      continue;
    }
    const auto &resourceOperand = candidate.Operands[2];
    if (resourceOperand.Type == D3D10_SB_OPERAND_TYPE_RESOURCE &&
        !resourceOperand.Indices.empty() &&
        resourceOperand.Indices.front() == bindPoint) {
      instruction = &candidate;
      return true;
    }
  }

  instruction = nullptr;
  return false;
}

}

int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) {
    std::cerr << "Usage: sm5_recipe_objective_ign_texture_patch "
                 "<input.ps_5_0.cso> <recipe.yml> [output.cso]\n";
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

  const size_t initialResourceCount = inputProgram.ResourceBindPoints.size();
  const size_t initialCBufferCount = inputProgram.CBufferBindPoints.size();
  const size_t initialSamplerCount = inputProgram.SamplerBindPoints.size();
  const int initialFrcCount = CountOpcode(inputProgram, D3D10_SB_OPCODE_FRC);
  const uint32_t initialTempCount = inputProgram.TempCount;

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeFile(argv[2], parseResult)) {
    std::cerr << "Failed to parse SM5 recipe file: " << parseResult.Error
              << "\n";
    return 1;
  }

  const auto patchResult =
      dxp::sm5::PatchContainer(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "PatchContainer failed: " << patchResult.Error << "\n";
    return 1;
  }

  if (argc == 4 && !WriteFile(argv[3], patchResult.OutputBytes)) {
    std::cerr << "Failed to write patched output file: " << argv[3] << "\n";
    return 1;
  }

  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Failed to inspect patched SM5 program: " << inspectError
              << "\n";
    return 1;
  }

  if (patchedProgram.ResourceBindPoints.size() != initialResourceCount + 1) {
    std::cerr << "Expected one additional resource declaration.\n";
    return 1;
  }

  if (patchedProgram.CBufferBindPoints.size() != initialCBufferCount + 1) {
    std::cerr << "Expected one additional cbuffer declaration.\n";
    return 1;
  }

  if (patchedProgram.SamplerBindPoints.size() != initialSamplerCount + 1) {
    std::cerr << "Expected one additional sampler declaration.\n";
    return 1;
  }

  if (!HasResourceDecl(patchedProgram, 10)) {
    std::cerr << "Expected patched shader to declare fast-noise texture t10.\n";
    return 1;
  }

  if (!HasConstantBufferDecl(patchedProgram, 2)) {
    std::cerr
        << "Expected patched shader to declare frame constants cbuffer CB2.\n";
    return 1;
  }

  if (!HasTextureArraySampleUsingResource(patchedProgram, 10)) {
    std::cerr << "Expected patched shader to sample the injected texture array "
                 "resource.\n";
    return 1;
  }

  std::unordered_set<uint32_t> initialSamplerBindPoints;
  for (const auto bindPoint : inputProgram.SamplerBindPoints) {
    initialSamplerBindPoints.insert(bindPoint);
  }

  uint32_t injectedSamplerBindPoint = 0;
  bool foundInjectedSampler = false;
  for (const auto bindPoint : patchedProgram.SamplerBindPoints) {
    if (initialSamplerBindPoints.find(bindPoint) ==
        initialSamplerBindPoints.end()) {
      injectedSamplerBindPoint = bindPoint;
      foundInjectedSampler = true;
      break;
    }
  }

  if (!foundInjectedSampler) {
    std::cerr << "Expected patched shader to include a newly allocated sampler "
                 "bind point.\n";
    return 1;
  }

  const dxp::sm5::ProgramInstruction *sampleInstruction = nullptr;
  if (!FindTextureArraySampleUsingResource(patchedProgram, 10,
                                           sampleInstruction) ||
      sampleInstruction == nullptr || sampleInstruction->Operands.size() < 2 ||
      sampleInstruction->Operands[1].Type != D3D10_SB_OPERAND_TYPE_TEMP ||
      sampleInstruction->Operands[1].Indices.empty()) {
    std::cerr << "Expected patched shader to use a temp coordinate operand for "
                 "the injected sample.\n";
    return 1;
  }

  if (sampleInstruction->Operands.size() < 4 ||
      sampleInstruction->Operands[3].Type != D3D10_SB_OPERAND_TYPE_SAMPLER ||
      sampleInstruction->Operands[3].Indices.empty() ||
      sampleInstruction->Operands[3].Indices.front() !=
          injectedSamplerBindPoint) {
    std::cerr << "Expected injected sample to use the auto-bound sampler "
                 "declaration.\n";
    return 1;
  }

  if (sampleInstruction->Operands[1].Indices.front() < initialTempCount) {
    std::cerr << "Expected injected sample coordinates to use a newly "
                 "allocated temp register.\n";
    return 1;
  }

  if (patchedProgram.TempCount <= initialTempCount) {
    std::cerr << "Expected patched shader to increase dcl_temps for injected "
                 "temporary registers.\n";
    return 1;
  }

  if (CountOpcode(patchedProgram, D3D10_SB_OPCODE_FRC) >= initialFrcCount) {
    std::cerr << "Expected patched shader to reduce FRC count after IGN "
                 "replacement.\n";
    return 1;
  }

  std::cout << "Objective SM5 YAML recipe patch added CB2, t10, and replaced "
               "an IGN chain.\n";
  return 0;
}
