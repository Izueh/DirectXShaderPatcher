#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "tests/helper/TestHelper.hpp"

#include "dxp/sm6/Recipe.hpp"
#include "dxp/StepResults.hpp"

namespace {

bool ReadShader(const std::filesystem::path& rel, std::vector<uint8_t>& out) {
  return ReadFile((RepoRootPath() / rel).string(), out);
}

// Typed matching: a constant operand restricted to I32 matches the shader's
// Frc overload constant (an LLVM i32 with value 22); F32 must NOT match.
bool TestTypedMatch() {
  std::vector<uint8_t> shader;
  if (!ReadShader("tests/shaders/0x56C468C3.cs_6_6.cso", shader)) return false;

  const char* positive = R"(
steps:
  - kind: apply_rule
    name: typed_i32
    rewrite_mode: none
    match_mode: match_all
    rule:
      match:
        - opcode: Frc
          operands:
            - index: 0
              kind: constant
              component_type: I32
              capture: c
)";
  auto pr = dxp::sm6::Recipe::ParseFromText(positive);
  if (!pr) {
    std::cerr << "typed match: parse failed: " << pr.error() << "\n";
    return false;
  }
  auto res = pr->Execute(shader);
  if (!res) {
    std::cerr << "typed match: execute failed: " << res.error() << "\n";
    return false;
  }
  const auto* apply_res = std::get_if<dxp::ApplyRuleResults>(&res->steps[0].results);
  if (apply_res == nullptr || apply_res->match_count == 0) {
    std::cerr << "typed match: expected I32 Frc constant to match\n";
    return false;
  }

  const char* negative = R"(
steps:
  - kind: apply_rule
    name: typed_f32
    required: false
    rewrite_mode: none
    match_mode: match_all
    rule:
      match:
        - opcode: Frc
          operands:
            - index: 0
              kind: constant
              component_type: F32
              capture: c
)";
  auto nr = dxp::sm6::Recipe::ParseFromText(negative);
  if (!nr) {
    std::cerr << "typed match negative: parse failed: " << nr.error() << "\n";
    return false;
  }
  auto nres = nr->Execute(shader);
  if (!nres) {
    std::cerr << "typed match negative: execute failed: " << nres.error() << "\n";
    return false;
  }
  const auto* napply = std::get_if<dxp::ApplyRuleResults>(&nres->steps[0].results);
  if (napply == nullptr || napply->match_count != 0) {
    std::cerr << "typed match: expected F32 restriction to reject the i32 constant\n";
    return false;
  }

  std::cout << "  typed match: I32 matches, F32 rejected\n";
  return true;
}

// Typed emitting: an emitted f32 binary op with an explicit F32 constant runs
// and produces a mutated program.
bool TestTypedEmit() {
  std::vector<uint8_t> shader;
  if (!ReadShader("tests/shaders/0x56C468C3.cs_6_6.cso", shader)) return false;

  const char* yaml = R"(
steps:
  - kind: apply_rule
    name: typed_emit
    rewrite_mode: before
    match_mode: first
    rule:
      match:
        - opcode: Frc
      emit:
        - opcode: fadd
          result_component_type: F32
          name: typed_add
          operands:
            - index: 0
              kind: constant
              constant_float_values: [1.5]
              component_type: F32
            - index: 1
              kind: constant
              constant_float_values: [2.5]
              component_type: F32
)";
  auto pr = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!pr) {
    std::cerr << "typed emit: parse failed: " << pr.error() << "\n";
    return false;
  }
  auto res = pr->Execute(shader);
  if (!res) {
    std::cerr << "typed emit: execute failed: " << res.error() << "\n";
    return false;
  }
  if (res->output_bytes == shader) {
    std::cerr << "typed emit: expected the program to be mutated\n";
    return false;
  }
  std::cout << "  typed emit: f32 constants emitted, program mutated\n";
  return true;
}

}  // namespace

int main() {
  const ScopedCoInitialize coinit;

  bool ok = true;
  ok &= TestTypedMatch();
  ok &= TestTypedEmit();

  if (ok) {
    std::cout << "SM6 typed constant match/emit test passed.\n";
  } else {
    std::cerr << "SM6 typed constant match/emit test FAILED.\n";
  }
  std::cout.flush();
  return ok ? 0 : 1;
}
