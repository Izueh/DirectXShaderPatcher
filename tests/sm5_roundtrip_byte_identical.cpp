#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "src/dxp/sm5/ShaderProgram.hpp"
#include "tests/helper/TestHelper.hpp"

// Parses a real D3DCompiler-produced shader, re-serializes it, and asserts the
// result is byte-identical — including the DXBC container hash. Guards the
// serialization-fidelity fixes (extended opcode tokens, dcl_global_flags bits,
// dcl_resource dimension, dcl_cb operand form, discard test_boolean, and the
// getDigest()-style container hash).
int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_roundtrip_byte_identical <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  auto program = dxp::sm5::ShaderProgram::FromBytes(input_bytes);
  if (!program) {
    std::cerr << "Failed to parse SM5 shader: " << program.error() << "\n";
    return 1;
  }

  auto serialized = program->Serialize();
  if (!serialized) {
    std::cerr << "Failed to re-serialize SM5 shader: " << serialized.error() << "\n";
    return 1;
  }

  if (serialized->size() != input_bytes.size() || *serialized != input_bytes) {
    std::cerr << "Round-trip is not byte-identical: input " << input_bytes.size() << " bytes, output "
              << serialized->size() << " bytes\n";
    return 1;
  }

  std::cout << "SM5 round-trip is byte-identical (" << input_bytes.size() << " bytes).\n";
  return 0;
}
