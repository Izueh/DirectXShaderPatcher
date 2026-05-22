#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/RecipeParse.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <vector>

namespace {

static bool WriteFile(const std::string &path, const std::vector<uint8_t> &bytes) {
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

static int CountOpcode(const dxp::sm5::Program &program, dxp::sm5::OpcodeType opcode) {
  int count = 0;
  for (const auto &instruction : program.Instructions) {
    if (static_cast<dxp::sm5::OpcodeType>(instruction.Opcode) == opcode) {
      ++count;
    }
  }
  return count;
}

static bool HasResourceDecl(const dxp::sm5::Program &program, uint32_t bindPoint) {
  for (const auto &instruction : program.Instructions) {
    if (static_cast<dxp::sm5::OpcodeType>(instruction.Opcode) != D3D10_SB_OPCODE_DCL_RESOURCE) {
      continue;
    }
    if (!instruction.Operands.empty() && !instruction.Operands.front().Indices.empty() &&
        instruction.Operands.front().Indices.front() == bindPoint) {
      return true;
    }
  }
  return false;
}

static bool HasConstantBufferDecl(const dxp::sm5::Program &program, uint32_t bindPoint) {
  for (const auto &instruction : program.Instructions) {
    if (static_cast<dxp::sm5::OpcodeType>(instruction.Opcode) != D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) {
      continue;
    }
    if (!instruction.Operands.empty() && !instruction.Operands.front().Indices.empty() &&
        instruction.Operands.front().Indices.front() == bindPoint) {
      return true;
    }
  }
  return false;
}

static bool HasTextureArraySampleUsingResource(const dxp::sm5::Program &program,
                                               uint32_t bindPoint) {
  for (const auto &instruction : program.Instructions) {
    if (static_cast<dxp::sm5::OpcodeType>(instruction.Opcode) != D3D10_SB_OPCODE_SAMPLE_L) {
      continue;
    }
    if (instruction.Operands.size() < 3) {
      continue;
    }
    const auto &resourceOperand = instruction.Operands[2];
    if (resourceOperand.Type == D3D10_SB_OPERAND_TYPE_RESOURCE &&
        !resourceOperand.Indices.empty() && resourceOperand.Indices.front() == bindPoint) {
      return true;
    }
  }
  return false;
}

static bool FindTextureArraySampleUsingResource(const dxp::sm5::Program &program,
                                                uint32_t bindPoint,
                                                const dxp::sm5::Instruction *&instruction) {
  for (const auto &candidate : program.Instructions) {
    if (static_cast<dxp::sm5::OpcodeType>(candidate.Opcode) != D3D10_SB_OPCODE_SAMPLE_L) {
      continue;
    }
    if (candidate.Operands.size() < 3) {
      continue;
    }
    const auto &resourceOperand = candidate.Operands[2];
    if (resourceOperand.Type == D3D10_SB_OPERAND_TYPE_RESOURCE &&
        !resourceOperand.Indices.empty() && resourceOperand.Indices.front() == bindPoint) {
      instruction = &candidate;
      return true;
    }
  }

  instruction = nullptr;
  return false;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) {
    std::cerr << "Usage: sm5_recipe_objective_ign_texture_patch <input.ps_5_0.cso> <recipe.yml> [output.cso]\n";
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

  const size_t initialResourceCount = inputProgram.Resources.size();
  const size_t initialCBufferCount = inputProgram.CBuffers.size();
  const size_t initialSamplerCount = inputProgram.Samplers.size();
  const int initialFrcCount = CountOpcode(inputProgram, D3D10_SB_OPCODE_FRC);
  const uint32_t initialTempCount = inputProgram.TempCount;

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeFile(argv[2], parseResult)) {
    std::cerr << "Failed to parse SM5 recipe file: " << parseResult.Error << "\n";
    return 1;
  }

  const auto patchResult = dxp::sm5::PatchContainerInMemory(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "PatchContainerInMemory failed: " << patchResult.Error << "\n";
    return 1;
  }

  if (argc == 4 && !WriteFile(argv[3], patchResult.OutputBytes)) {
    std::cerr << "Failed to write patched output file: " << argv[3] << "\n";
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

  if (patchedProgram.Resources.size() != initialResourceCount + 1) {
    std::cerr << "Expected one additional resource declaration.\n";
    return 1;
  }

  if (patchedProgram.CBuffers.size() != initialCBufferCount + 1) {
    std::cerr << "Expected one additional cbuffer declaration.\n";
    return 1;
  }

  if (patchedProgram.Samplers.size() != initialSamplerCount + 1) {
    std::cerr << "Expected one additional sampler declaration.\n";
    return 1;
  }

  if (!HasResourceDecl(patchedProgram, 10)) {
    std::cerr << "Expected patched shader to declare fast-noise texture t10.\n";
    return 1;
  }

  if (!HasConstantBufferDecl(patchedProgram, 2)) {
    std::cerr << "Expected patched shader to declare frame constants cbuffer CB2.\n";
    return 1;
  }

  if (!HasTextureArraySampleUsingResource(patchedProgram, 10)) {
    std::cerr << "Expected patched shader to sample the injected texture array resource.\n";
    return 1;
  }

  std::unordered_set<uint32_t> initialSamplerBindPoints;
  for (const auto &sampler : inputProgram.Samplers) {
    initialSamplerBindPoints.insert(sampler.RegisterBindPoint);
  }

  uint32_t injectedSamplerBindPoint = 0;
  bool foundInjectedSampler = false;
  for (const auto &sampler : patchedProgram.Samplers) {
    if (initialSamplerBindPoints.find(sampler.RegisterBindPoint) ==
        initialSamplerBindPoints.end()) {
      injectedSamplerBindPoint = sampler.RegisterBindPoint;
      foundInjectedSampler = true;
      break;
    }
  }

  if (!foundInjectedSampler) {
    std::cerr << "Expected patched shader to include a newly allocated sampler bind point.\n";
    return 1;
  }

  const dxp::sm5::Instruction *sampleInstruction = nullptr;
  if (!FindTextureArraySampleUsingResource(patchedProgram, 10, sampleInstruction) ||
      sampleInstruction == nullptr || sampleInstruction->Operands.size() < 2 ||
      sampleInstruction->Operands[1].Type != D3D10_SB_OPERAND_TYPE_TEMP ||
      sampleInstruction->Operands[1].Indices.empty()) {
    std::cerr << "Expected patched shader to use a temp coordinate operand for the injected sample.\n";
    return 1;
  }

  if (sampleInstruction->Operands.size() < 4 ||
      sampleInstruction->Operands[3].Type != D3D10_SB_OPERAND_TYPE_SAMPLER ||
      sampleInstruction->Operands[3].Indices.empty() ||
      sampleInstruction->Operands[3].Indices.front() != injectedSamplerBindPoint) {
    std::cerr << "Expected injected sample to use the auto-bound sampler declaration.\n";
    return 1;
  }

  if (sampleInstruction->Operands[1].Indices.front() < initialTempCount) {
    std::cerr << "Expected injected sample coordinates to use a newly allocated temp register.\n";
    return 1;
  }

  if (patchedProgram.TempCount <= initialTempCount) {
    std::cerr << "Expected patched shader to increase dcl_temps for injected scratch registers.\n";
    return 1;
  }

  if (CountOpcode(patchedProgram, D3D10_SB_OPCODE_FRC) >= initialFrcCount) {
    std::cerr << "Expected patched shader to reduce FRC count after IGN replacement.\n";
    return 1;
  }

  std::cout << "Objective SM5 YAML recipe patch added CB2, t10, and replaced an IGN chain.\n";
  return 0;
}
