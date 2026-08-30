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

bool ReadShader(std::vector<uint8_t>& out) {
  return ReadFile((RepoRootPath() / "tests/shaders/0x56C468C3.cs_6_6.cso").string(), out);
}

uint32_t MatchCount(const dxp::RecipeReport& report) {
  for (const auto& step : report.steps) {
    if (const auto* ar = std::get_if<dxp::ApplyRuleResults>(&step.results)) {
      return ar->match_count;
    }
  }
  return 0;
}

// Sequence matching: [FMax, FMin] consecutive pairs (3 in this shader), and the
// OR-union of independent patterns must NOT be reported as a sequence.
bool TestSequence() {
  std::vector<uint8_t> shader;
  if (!ReadShader(shader)) return false;

  const char* probe = R"(
steps:
  - kind: apply_rule
    name: seq_probe
    required: false
    rewrite_mode: none
    match_mode: match_all
    rule:
      match:
        - opcode: FMax
        - opcode: FMin
)";
  auto pr = dxp::sm6::Recipe::ParseFromText(probe);
  if (!pr) {
    std::cerr << "sequence: parse failed: " << pr.error() << "\n";
    return false;
  }
  auto res = pr->Execute(shader);
  if (!res) {
    std::cerr << "sequence: execute failed: " << res.error() << "\n";
    return false;
  }
  if (MatchCount(*res) != 3) {
    std::cerr << "sequence: expected 3 consecutive FMax-FMin pairs, got " << MatchCount(*res) << "\n";
    return false;
  }

  // A sequence rewrite mutates the program.
  const char* rewrite = R"(
steps:
  - kind: apply_rule
    name: seq_rewrite
    rewrite_mode: before
    match_mode: match_all
    rule:
      match:
        - opcode: FMax
        - opcode: FMin
      emit:
        - opcode: fadd
          result_component_type: F32
          name: seq_marker
          operands:
            - index: 0
              kind: constant
              constant_float_values: [0.5]
              component_type: F32
            - index: 1
              kind: constant
              constant_float_values: [0.25]
              component_type: F32
)";
  auto rr = dxp::sm6::Recipe::ParseFromText(rewrite);
  if (!rr) {
    std::cerr << "sequence rewrite: parse failed: " << rr.error() << "\n";
    return false;
  }
  auto rres = rr->Execute(shader);
  if (!rres) {
    std::cerr << "sequence rewrite: execute failed: " << rres.error() << "\n";
    return false;
  }
  if (rres->output_bytes == shader) {
    std::cerr << "sequence rewrite: expected mutated output\n";
    return false;
  }
  std::cout << "  sequence: [FMax, FMin] matched 3 pairs and rewrote them\n";
  return true;
}

// Terminator handling: 'ret' (LLVM) can anchor a Before insertion; Replace on a
// terminator is refused cleanly.
bool TestTerminator() {
  std::vector<uint8_t> shader;
  if (!ReadShader(shader)) return false;

  const char* before = R"(
steps:
  - kind: apply_rule
    name: before_ret
    rewrite_mode: before
    match_mode: match_all
    rule:
      match:
        - opcode: ret
      emit:
        - opcode: fadd
          result_component_type: F32
          name: pre_ret_marker
          operands:
            - index: 0
              kind: constant
              constant_float_values: [1.0]
              component_type: F32
            - index: 1
              kind: constant
              constant_float_values: [2.0]
              component_type: F32
)";
  auto pr = dxp::sm6::Recipe::ParseFromText(before);
  if (!pr) {
    std::cerr << "terminator: parse failed: " << pr.error() << "\n";
    return false;
  }
  auto res = pr->Execute(shader);
  if (!res) {
    std::cerr << "terminator before: execute failed: " << res.error() << "\n";
    return false;
  }
  if (res->output_bytes == shader) {
    std::cerr << "terminator before: expected mutated output (insert before ret)\n";
    return false;
  }

  const char* replace = R"(
steps:
  - kind: apply_rule
    name: replace_ret
    rewrite_mode: replace
    match_mode: match_all
    rule:
      match:
        - opcode: ret
      emit:
        - opcode: fadd
          result_component_type: F32
          operands:
            - index: 0
              kind: constant
              constant_float_values: [1.0]
              component_type: F32
            - index: 1
              kind: constant
              constant_float_values: [2.0]
              component_type: F32
)";
  auto rp = dxp::sm6::Recipe::ParseFromText(replace);
  if (!rp) {
    std::cerr << "terminator replace: parse failed: " << rp.error() << "\n";
    return false;
  }
  auto rres = rp->Execute(shader);
  if (rres) {
    std::cerr << "terminator replace: expected refusal (cannot erase a terminator)\n";
    return false;
  }
  std::cout << "  terminator: before-ret works, replace-ret refused cleanly\n";
  return true;
}

// SSA replace: replacing FMax rewires its result's uses to the emitted value.
bool TestSsaReplace() {
  std::vector<uint8_t> shader;
  if (!ReadShader(shader)) return false;

  const char* yaml = R"(
steps:
  - kind: apply_rule
    name: replace_fmax
    rewrite_mode: replace
    match_mode: match_all
    rule:
      match:
        - opcode: FMax
          capture: matched_fmax
          operands:
            - index: 1
              capture: a
      emit:
        - opcode: fadd
          result_component_type: F32
          name: fmax_replacement
          replace_captured: matched_fmax
          operands:
            - index: 0
              kind: call
              capture: a
            - index: 1
              kind: constant
              constant_float_values: [0.0]
              component_type: F32
)";
  auto pr = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!pr) {
    std::cerr << "ssa replace: parse failed: " << pr.error() << "\n";
    return false;
  }
  auto res = pr->Execute(shader);
  if (!res) {
    std::cerr << "ssa replace: execute failed: " << res.error() << "\n";
    return false;
  }
  if (res->output_bytes == shader) {
    std::cerr << "ssa replace: expected mutated output (FMax replaced with fadd)\n";
    return false;
  }
  std::cout << "  ssa replace: FMax replaced, uses rewired\n";
  return true;
}

// Cross-step captures: step 1 captures an FMax result; step 2 emits code using it
// and rewires its uses (resolved from the global capture store).
bool TestCrossStep() {
  std::vector<uint8_t> shader;
  if (!ReadShader(shader)) return false;

  const char* yaml = R"(
steps:
  - kind: apply_rule
    name: capture_step
    rewrite_mode: none
    match_mode: match_all
    rule:
      match:
        - opcode: FMax
          capture: shared_fmax
  - kind: apply_rule
    name: use_step
    rewrite_mode: after
    match_mode: match_all
    rule:
      match:
        - opcode: FMax
          match_capture: shared_fmax
      emit:
        - opcode: fadd
          result_component_type: F32
          name: shared_use
          operands:
            - index: 0
              kind: call
              capture: shared_fmax
            - index: 1
              kind: constant
              constant_float_values: [0.5]
              component_type: F32
        - opcode: fadd
          result_component_type: F32
          name: shared_use2
          replace_captured: shared_fmax
          operands:
            - index: 0
              kind: call
              capture: shared_use
            - index: 1
              kind: constant
              constant_float_values: [0.5]
              component_type: F32
)";
  auto pr = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!pr) {
    std::cerr << "cross-step: parse failed: " << pr.error() << "\n";
    return false;
  }
  auto res = pr->Execute(shader);
  if (!res) {
    std::cerr << "cross-step: execute failed: " << res.error() << "\n";
    return false;
  }
  if (res->output_bytes == shader) {
    std::cerr << "cross-step: expected mutated output\n";
    return false;
  }
  std::cout << "  cross-step: step 2 used step 1's capture\n";
  return true;
}

// Validation: integer opcode + float component type is rejected before Execute.
bool TestValidation() {
  const char* bad = R"(
steps:
  - kind: apply_rule
    name: bad_types
    rewrite_mode: replace
    rule:
      match:
        - opcode: Frc
          capture: matched_frc
      emit:
        - opcode: add
          result_component_type: F32
          replace_captured: matched_frc
          operands:
            - index: 0
              kind: constant
              constant_float_values: [1.0]
              component_type: F32
            - index: 1
              kind: constant
              constant_float_values: [2.0]
              component_type: F32
)";
  auto pr = dxp::sm6::Recipe::ParseFromText(bad);
  if (!pr) {
    std::cerr << "validation: parse failed: " << pr.error() << "\n";
    return false;
  }
  auto vr = dxp::sm6::ValidateRecipe(pr.value());
  if (vr) {
    std::cerr << "validation: expected integer-opcode/float-type recipe to be rejected\n";
    return false;
  }
  std::cout << "  validation: int opcode + float type rejected: " << vr.error() << "\n";
  return true;
}

}  // namespace

int main() {
  const ScopedCoInitialize coinit;

  bool ok = true;
  ok &= TestSequence();
  ok &= TestTerminator();
  ok &= TestSsaReplace();
  ok &= TestCrossStep();
  ok &= TestValidation();

  if (ok) {
    std::cout << "SM6 rewrite semantics test passed.\n";
  } else {
    std::cerr << "SM6 rewrite semantics test FAILED.\n";
  }
  std::cout.flush();
  return ok ? 0 : 1;
}
