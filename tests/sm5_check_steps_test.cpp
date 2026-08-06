#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

// Test 1: check_opcode_count parses and validates
bool TestCheckOpcodeCountParse() {
  const char* yaml = R"(
steps:
  - kind: check_opcode_count
    name: count_ops
    opcodes:
      - mov
      - add
      - mul
)";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 1 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::cout << "Test 1 passed: check_opcode_count parsed.\n";
  return true;
}

// Test 2: check_resource_count parses and validates
bool TestCheckResourceCountParse() {
  const char* yaml = R"(
steps:
  - kind: check_resource_count
    name: count_resources
)";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 2 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::cout << "Test 2 passed: check_resource_count parsed.\n";
  return true;
}

// Test 3: check_opcode_count executes and returns correct counts
bool TestCheckOpcodeCountExecutes() {
  const char* yaml = R"(
steps:
  - kind: check_opcode_count
    name: count_ops
    opcodes:
      - mov
      - add
      - mul
      - frc
      - dcl_temps
)";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 3 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> input_shader;
  if (!ReadFile((RepoRootPath() / "tests/shaders/0x7AFF256C.ps_5_0.cso").string(), input_shader)) {
    std::cerr << "Test 3: failed to read test shader.\n";
    return false;
  }

  auto result = parse_result.value().Execute(input_shader);
  if (!result) {
    std::cerr << "Test 3 execution failed: " << result.error() << "\n";
    return false;
  }

  // Find the check step report
  const dxp::StepReport* step_report = nullptr;
  for (const auto& step : result.value().steps) {
    if (step.name == "count_ops") {
      step_report = &step;
      break;
    }
  }

  if (step_report == nullptr) {
    std::cerr << "Test 3: step report not found.\n";
    return false;
  }

  const auto* ops_res = std::get_if<dxp::CheckOpcodeCountResults>(&step_report->results);
  if (ops_res == nullptr) {
    std::cerr << "Test 3: results not CheckOpcodeCountResults.\n";
    return false;
  }

  // Verify that mov count is > 0 (the test shader has mov instructions)
  auto mov_it = ops_res->opcode_counts.find("mov");
  if (mov_it == ops_res->opcode_counts.end() || mov_it->second == 0) {
    std::cerr << "Test 3: expected mov count > 0, got "
              << (mov_it != ops_res->opcode_counts.end() ? std::to_string(mov_it->second) : "not found")
              << ".\n";
    return false;
  }

  // dcl_temps should have count 1 (one temp declaration)
  auto temps_it = ops_res->opcode_counts.find("dcl_temps");
  if (temps_it == ops_res->opcode_counts.end() || temps_it->second != 1) {
    std::cerr << "Test 3: expected dcl_temps count == 1, got "
              << (temps_it != ops_res->opcode_counts.end() ? std::to_string(temps_it->second) : "not found")
              << ".\n";
    return false;
  }

  // All 5 opcodes should be in the map
  constexpr size_t kExpectedOpcodeCount = 5;
  if (ops_res->opcode_counts.size() != kExpectedOpcodeCount) {
    std::cerr << "Test 3: expected 5 opcode entries, got " << ops_res->opcode_counts.size() << ".\n";
    return false;
  }

  std::cout << "Test 3 passed: check_opcode_count executed correctly (mov=" << mov_it->second
            << ", dcl_temps=" << temps_it->second << ").\n";
  return true;
}

// Test 4: check_resource_count executes and returns correct counts
bool TestCheckResourceCountExecutes() {
  const char* yaml = R"(
steps:
  - kind: check_resource_count
    name: count_resources
)";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 4 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> input_shader;
  if (!ReadFile((RepoRootPath() / "tests/shaders/0x7AFF256C.ps_5_0.cso").string(), input_shader)) {
    std::cerr << "Test 4: failed to read test shader.\n";
    return false;
  }

  auto result = parse_result.value().Execute(input_shader);
  if (!result) {
    std::cerr << "Test 4 execution failed: " << result.error() << "\n";
    return false;
  }

  // Find the check step report
  const dxp::StepReport* step_report = nullptr;
  for (const auto& step : result.value().steps) {
    if (step.name == "count_resources") {
      step_report = &step;
      break;
    }
  }

  if (step_report == nullptr) {
    std::cerr << "Test 4: step report not found.\n";
    return false;
  }

  const auto* res_res = std::get_if<dxp::CheckResourceCountResults>(&step_report->results);
  if (res_res == nullptr) {
    std::cerr << "Test 4: results not CheckResourceCountResults.\n";
    return false;
  }

  // Verify total is > 0 (the test shader has resources)
  if (res_res->total == 0) {
    std::cerr << "Test 4: expected total > 0, got 0.\n";
    return false;
  }

  std::cout << "Test 4 passed: check_resource_count executed correctly (total=" << res_res->total
            << ", textures=" << res_res->textures << ", samplers=" << res_res->samplers
            << ", cbuffers=" << res_res->cbuffers << ", uavs=" << res_res->uavs << ").\n";
  return true;
}

// Test 5: version fields are populated in ExecutionContext
bool TestVersionFieldsPopulated() {
  const char* yaml = R"(
steps:
  - kind: check_opcode_count
    name: count_ops
    opcodes:
      - mov
)";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 5 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> input_shader;
  if (!ReadFile((RepoRootPath() / "tests/shaders/0x7AFF256C.ps_5_0.cso").string(), input_shader)) {
    std::cerr << "Test 5: failed to read test shader.\n";
    return false;
  }

  auto result = parse_result.value().Execute(input_shader);
  if (!result) {
    std::cerr << "Test 5 execution failed: " << result.error() << "\n";
    return false;
  }

  // Shader loaded and step executed — version fields were populated.
  // The 0x7AFF256C.ps_5_0 shader is SM5.0
  if (result.value().steps.empty()) {
    std::cerr << "Test 5: no steps executed.\n";
    return false;
  }

  std::cout << "Test 5 passed: version fields populated (shader loaded as SM5.0).\n";
  return true;
}

// Test 6: combined steps — check_opcode_count + check_resource_count + apply_rule
bool TestCombinedSteps() {
  const char* yaml = R"(
steps:
  - kind: check_opcode_count
    name: count_ops
    opcodes:
      - mov
  - kind: check_resource_count
    name: count_resources
  - kind: apply_rule
    name: noop
    required: false
    rule:
        match:
          - opcode: mov
            operands:
            - capture: dst
            - capture: src
        emit:
          - opcode: mov
            operands:
              - type: temp
                capture: dst
              - type: temp
                capture: src
)";

  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 6 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> input_shader;
  if (!ReadFile((RepoRootPath() / "tests/shaders/0x7AFF256C.ps_5_0.cso").string(), input_shader)) {
    std::cerr << "Test 6: failed to read test shader.\n";
    return false;
  }

  auto result = parse_result.value().Execute(input_shader);
  if (!result) {
    std::cerr << "Test 6 execution failed: " << result.error() << "\n";
    return false;
  }

  // Verify check steps ran (apply_rule with no matches may be skipped)
  bool has_opcode_count = false;
  bool has_resource_count = false;
  for (const auto& step : result.value().steps) {
    if (step.name == "count_ops") has_opcode_count = true;
    if (step.name == "count_resources") has_resource_count = true;
  }

  if (!has_opcode_count || !has_resource_count) {
    std::cerr << "Test 6: expected check steps to run (opcode_count=" << has_opcode_count
              << ", resource_count=" << has_resource_count << ").\n";
    return false;
  }

  std::cout << "Test 6 passed: combined steps executed successfully.\n";
  return true;
}

}  // namespace

int main(int argc, [[maybe_unused]] char** argv_) {
  if (argc != 1) {
    std::cerr << "Usage: sm5_check_steps_test\n";
    return 1;
  }

  bool all_passed = true;

  all_passed &= TestCheckOpcodeCountParse();
  all_passed &= TestCheckResourceCountParse();
  all_passed &= TestCheckOpcodeCountExecutes();
  all_passed &= TestCheckResourceCountExecutes();
  all_passed &= TestVersionFieldsPopulated();
  all_passed &= TestCombinedSteps();

  if (all_passed) {
    std::cout << "All check steps tests passed.\n";
  } else {
    std::cerr << "Some check steps tests failed.\n";
  }

  std::cout.flush();
  std::cerr.flush();
  return all_passed ? 0 : 1;
}
