#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/Serialize.h"

#include <filesystem>
#include <fstream>
#include <iostream>
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

static std::string DefaultOutPath(const std::string &inPath) {
  const size_t dot = inPath.rfind(".cso");
  if (dot == std::string::npos) {
    return inPath + ".sm5.mov.patched.cso";
  }
  return inPath.substr(0, dot) + ".sm5.mov.patched.cso";
}

static int FindNthOpcodeIndex(const dxp::sm5::Program &program,
                              dxp::sm5::OpcodeType opcode,
                              int ordinal) {
  int seen = 0;
  for (size_t i = 0; i < program.Instructions.size(); ++i) {
    const auto &inst = program.Instructions[i];
    if (static_cast<dxp::sm5::OpcodeType>(inst.Opcode) != opcode) {
      continue;
    }
    if (seen == ordinal) {
      return static_cast<int>(i);
    }
    ++seen;
  }
  return -1;
}

static bool ValidatePatchedProgram(const dxp::sm5::Program &inputProgram,
                                   const dxp::sm5::Program &patchedProgram,
                                   std::string &error) {
  const int firstInputFrc = FindNthOpcodeIndex(inputProgram, D3D10_SB_OPCODE_FRC, 0);
  const int secondInputFrc = FindNthOpcodeIndex(inputProgram, D3D10_SB_OPCODE_FRC, 1);
  if (firstInputFrc < 0 || secondInputFrc < 0) {
    error = "Expected at least two FRC instructions in input program.";
    return false;
  }

  if (static_cast<int>(inputProgram.Instructions.size()) !=
      static_cast<int>(patchedProgram.Instructions.size())) {
    error = "Patched instruction count changed unexpectedly.";
    return false;
  }

  const auto firstPatchedOpcode = static_cast<dxp::sm5::OpcodeType>(
      patchedProgram.Instructions[static_cast<size_t>(firstInputFrc)].Opcode);
  if (firstPatchedOpcode != D3D10_SB_OPCODE_MOV) {
    error = "First FRC instruction was not rewritten to MOV after round-trip.";
    return false;
  }

  const auto secondPatchedOpcode = static_cast<dxp::sm5::OpcodeType>(
      patchedProgram.Instructions[static_cast<size_t>(secondInputFrc)].Opcode);
  if (secondPatchedOpcode != D3D10_SB_OPCODE_FRC) {
    error = "Second FRC instruction changed unexpectedly after round-trip.";
    return false;
  }

  return true;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2 && argc != 3) {
    std::cerr << "Usage: sm5_replace_frc_with_mov <input.ps_5_0.cso> [output.cso]\n";
    return 1;
  }

  const std::string inputPath = argv[1];
  const std::string outputPath = argc == 3 ? argv[2] : DefaultOutPath(inputPath);

  std::vector<uint8_t> inputBytes;
  if (!ReadFile(inputPath, inputBytes)) {
    std::cerr << "Failed to read input file: " << inputPath << "\n";
    return 1;
  }

  dxp::sm5::Container container;
  if (!dxp::sm5::ParseDxbcContainer(inputBytes, container)) {
    std::cerr << "Failed to parse DXBC container.\n";
    return 1;
  }

  dxp::sm5::Program program;
  if (!dxp::sm5::ParseShaderChunk(container, program)) {
    std::cerr << "Failed to parse SM5 shader chunk.\n";
    return 1;
  }
  const dxp::sm5::Program originalProgram = program;

  bool replaced = false;
  uint32_t replacedInstructionIndex = 0;
  for (uint32_t i = 0; i < program.Instructions.size(); ++i) {
    auto &inst = program.Instructions[i];
    if (static_cast<dxp::sm5::OpcodeType>(inst.Opcode) != D3D10_SB_OPCODE_FRC) {
      continue;
    }
    if (inst.RawTokens.empty()) {
      continue;
    }

    uint32_t token0 = inst.RawTokens[0];
    token0 &= ~D3D10_SB_OPCODE_TYPE_MASK;
    token0 |= ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_MOV);
    inst.RawTokens[0] = token0;
    inst.Opcode = dxp::sm5::Opcode{D3D10_SB_OPCODE_MOV};

    replaced = true;
    replacedInstructionIndex = i;
    break;
  }

  if (!replaced) {
    std::cerr << "No FRC instruction found to replace.\n";
    return 1;
  }

  std::vector<uint8_t> shaderBytes;
  if (!dxp::sm5::RebuildShaderChunk(program, shaderBytes)) {
    std::cerr << "Failed to rebuild SM5 shader chunk.\n";
    return 1;
  }

  dxp::sm5::DxbcChunk *shaderChunk = container.GetShaderChunk();
  if (shaderChunk == nullptr) {
    std::cerr << "Shader chunk missing from container.\n";
    return 1;
  }
  shaderChunk->Data = std::move(shaderBytes);

  std::vector<uint8_t> outputBytes;
  if (!dxp::sm5::SerializeDxbcContainer(container, outputBytes)) {
    std::cerr << "Failed to serialize patched container.\n";
    return 1;
  }
  if (!dxp::sm5::RecomputeDxbcHash(outputBytes)) {
    std::cerr << "Failed to recompute DXBC hash.\n";
    return 1;
  }

  dxp::sm5::Container verifiedContainer;
  if (!dxp::sm5::ParseDxbcContainer(outputBytes, verifiedContainer)) {
    std::cerr << "Failed to reparse patched DXBC container for verification.\n";
    return 1;
  }

  dxp::sm5::Program verifiedProgram;
  if (!dxp::sm5::ParseShaderChunk(verifiedContainer, verifiedProgram)) {
    std::cerr << "Failed to reparse patched SM5 shader chunk for verification.\n";
    return 1;
  }

  std::string validationError;
  if (!ValidatePatchedProgram(originalProgram, verifiedProgram, validationError)) {
    std::cerr << validationError << "\n";
    return 1;
  }

  if (!WriteFile(outputPath, outputBytes)) {
    std::cerr << "Failed to write output file: " << outputPath << "\n";
    return 1;
  }

  std::cout << "Patched SM5 shader written to: " << outputPath << "\n";
  std::cout << "Replaced first FRC with MOV at instruction index: " << replacedInstructionIndex << "\n";
  return 0;
}
