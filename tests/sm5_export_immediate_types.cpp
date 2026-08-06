#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "tests/helper/TestHelper.hpp"

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"

namespace {

bool CaptureWith(const std::filesystem::path& shader_rel, const char* yaml,
                 dxp::ComponentType expected_type, const char* label) {
  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << label << ": recipe parse failed: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> shader;
  if (!ReadFile((RepoRootPath() / shader_rel).string(), shader)) {
    std::cerr << label << ": failed to read shader\n";
    return false;
  }

  auto result = parse_result.value().Execute(shader);
  if (!result) {
    std::cerr << label << ": execute failed: " << result.error() << "\n";
    return false;
  }

  bool found = false;
  for (const auto& [key, imm] : result->immediate_values) {
    if (imm.type != expected_type) {
      std::cerr << label << ": '" << key << "' type " << static_cast<int>(imm.type)
                << " != expected " << static_cast<int>(expected_type) << "\n";
      return false;
    }
    if (imm.raw_values.empty()) {
      std::cerr << label << ": '" << key << "' has no raw values\n";
      return false;
    }
    found = true;
    std::cout << "  " << label << ": '" << key << "' type=" << static_cast<int>(imm.type)
              << " raw[0]=" << imm.raw_values[0] << "\n";
  }
  if (!found) {
    std::cerr << label << ": no immediate exports produced\n";
    return false;
  }
  return true;
}

// Integer-opcode immediate (and mask) must be labeled I32.
bool TestIntegerImmediate() {
  const char* yaml = R"(
steps:
  - kind: apply_rule
    name: capture_int_imm
    rewrite_mode: none
    match_mode: match_all
    rule:
      match:
        - opcode: and
          operands:
            - type: temp
            - type: temp
            - capture: mask
              export_as: int_mask
)";
  return CaptureWith("tests/shaders/0xCDF14206.ps_5_0.cso", yaml, dxp::ComponentType::I32,
                     "integer-immediate");
}

// Ambiguous float-default immediate (mov literal) must be labeled F32.
bool TestFloatImmediate() {
  const char* yaml = R"(
steps:
  - kind: apply_rule
    name: capture_float_imm
    rewrite_mode: none
    match_mode: match_all
    rule:
      match:
        - opcode: mov
          operands:
            - type: temp
            - capture: lit
              export_as: float_lit
)";
  return CaptureWith("tests/shaders/0x7AFF256C.ps_5_0.cso", yaml, dxp::ComponentType::F32,
                     "float-immediate");
}

}  // namespace

int main() {
  bool ok = true;
  ok &= TestIntegerImmediate();
  ok &= TestFloatImmediate();

  if (ok) {
    std::cout << "SM5 immediate export type inference test passed.\n";
  } else {
    std::cerr << "SM5 immediate export type inference test FAILED.\n";
  }
  std::cout.flush();
  return ok ? 0 : 1;
}
