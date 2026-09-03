// Regression test: SM6 patched containers must carry a valid header hash.
//
// SerializeDxilContainerForModule leaves DxilContainerHeader.Hash.Digest
// zeroed; DXC computes the retail hash in a separate validator pass. Patched
// shaders shipped with a zeroed digest crashed the D3D device immediately at
// shader creation (drivers verify the hash; in-process DXIL validation does
// not). This test patches a shader and asserts the output container's header
// hash matches ComputeHashRetail over bytes [Version .. end).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <vector>

#include "dxc/Support/WinIncludes.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilHash/DxilHash.h"
#include "dxp/sm6/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

std::string DigestToHex(const uint8_t* digest) {
  static const char kHex[] = "0123456789ABCDEF";
  std::string hex;
  hex.reserve(32);
  for (unsigned i = 0; i < 16; ++i) {
    hex.push_back(kHex[digest[i] >> 4]);
    hex.push_back(kHex[digest[i] & 0xF]);
  }
  return hex;
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm6_container_hash_test <input.cso>\n";
    return 1;
  }

  const ScopedCoInitialize coinit;

  std::vector<uint8_t> input_shader;
  if (!ReadFile(args[1], input_shader)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  // Minimal no-op recipe: parse/serialize round-trip only.
  const char* yaml = R"(version: 1
steps:
  - kind: apply_rule
    name: identity_pass
    required: false
    rewrite_mode: none
    rule:
      prune: true
      match:
        - opcode: TextureLoad
          capture: texture_load
      emit:
        - capture: texture_load
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Failed to parse recipe: " << parse_result.error() << "\n";
    return 1;
  }

  auto patch_result = parse_result.value().Execute(input_shader);
  if (!patch_result) {
    std::cerr << "Recipe execution failed: " << patch_result.error() << "\n";
    return 1;
  }

  const std::vector<uint8_t>& output = patch_result.value().output_bytes;
  if (output.size() < sizeof(hlsl::DxilContainerHeader)) {
    std::cerr << "Output container too small: " << output.size() << " bytes\n";
    return 1;
  }

  auto* header = hlsl::IsDxilContainerLike(output.data(), output.size());
  if (header == nullptr || !hlsl::IsValidDxilContainer(header, output.size())) {
    std::cerr << "Output is not a valid DXIL container\n";
    return 1;
  }

  // The digest must not be left zeroed (the original bug).
  static const uint8_t kZeroDigest[16] = {};
  if (std::memcmp(header->Hash.Digest, kZeroDigest, sizeof(kZeroDigest)) == 0) {
    std::cerr << "FAIL: container header hash is zeroed (pre-fix behavior)\n";
    return 1;
  }

  // The digest must match ComputeHashRetail over [Version .. end).
  constexpr uint32_t kHashStartOffset = offsetof(hlsl::DxilContainerHeader, Version);
  uint8_t expected[16] = {};
  ComputeHashRetail(output.data() + kHashStartOffset,
                    static_cast<uint32_t>(output.size() - kHashStartOffset), expected);
  if (std::memcmp(expected, header->Hash.Digest, sizeof(expected)) != 0) {
    std::cerr << "FAIL: header hash mismatch\n"
              << "  expected: " << DigestToHex(expected) << "\n"
              << "  stored:   " << DigestToHex(header->Hash.Digest) << "\n";
    return 1;
  }

  std::cout << "SM6 container hash OK: " << DigestToHex(header->Hash.Digest)
            << " (output " << output.size() << " bytes)\n";
  return 0;
}
