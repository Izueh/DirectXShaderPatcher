#include "TestSupport.h"
#include "dxp/sm5/Container.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/Serialize.h"

#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_container_roundtrip <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> inputBytes;
  if (!ReadFile(argv[1], inputBytes)) {
    std::cerr << "Failed to read file: " << argv[1] << "\n";
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

  std::vector<uint8_t> rebuiltShader;
  if (!dxp::sm5::RebuildShaderChunk(program, rebuiltShader)) {
    std::cerr << "Failed to rebuild shader chunk.\n";
    return 1;
  }

  dxp::sm5::DxbcChunk *shaderChunk = container.GetShaderChunk();
  if (shaderChunk == nullptr) {
    std::cerr << "Shader chunk missing from parsed container.\n";
    return 1;
  }
  shaderChunk->Data = rebuiltShader;

  std::vector<uint8_t> outputBytes;
  if (!dxp::sm5::SerializeDxbcContainer(container, outputBytes)) {
    std::cerr << "Failed to serialize DXBC container.\n";
    return 1;
  }

  if (!dxp::sm5::RecomputeDxbcHash(outputBytes)) {
    std::cerr << "Failed to recompute DXBC hash.\n";
    return 1;
  }

  if (outputBytes.empty()) {
    std::cerr << "Round-trip output was unexpectedly empty.\n";
    return 1;
  }

  std::cout << "SM5 round-trip succeeded. Input bytes: " << inputBytes.size()
            << ", output bytes: " << outputBytes.size() << "\n";
  return 0;
}
