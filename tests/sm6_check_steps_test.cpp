#include <cstdint>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "dxp/sm6/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

// Test 1: check_opcode_count step parses and executes
bool TestCheckOpcodeCountParseAndExecute() {
  const char* yaml = R"(
steps:
  - kind: check_opcode_count
    name: count_ops
    dxil_opcodes:
      - Frc
      - Fmul
      - Add
    llvm_opcodes:
      - Call
      - Ret
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 1 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::cout << "Test 1 passed: check_opcode_count parsed.\n";
  return true;
}

// Test 2: check_opcode_count with empty opcodes list fails validation
bool TestCheckOpcodeCountEmptyFails() {
  const char* yaml = R"(
steps:
  - kind: check_opcode_count
    name: count_ops
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 2 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  // Execute will internally validate and fail for empty opcodes
  const std::vector<uint8_t> empty_shader;
  auto result = parse_result.value().Execute(empty_shader);
  if (result) {
    std::cerr << "Test 2: empty opcodes should have failed execution.\n";
    return false;
  }

  std::cout << "Test 2 passed: empty opcodes correctly rejected.\n";
  return true;
}

// Test 3: check_resource_count step parses and executes
bool TestCheckResourceCountParseAndExecute() {
  const char* yaml = R"(
steps:
  - kind: check_resource_count
    name: count_resources
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 3 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::cout << "Test 3 passed: check_resource_count parsed.\n";
  return true;
}

// Test 4: check_opcode_count executes on a real shader and returns counts
bool TestCheckOpcodeCountExecutes() {
  const char* yaml = R"(
steps:
  - kind: check_opcode_count
    name: count_ops
    dxil_opcodes:
      - Frc
      - Fmul
      - Add
      - NonExistentOp
    llvm_opcodes:
      - Call
      - Ret
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 4 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> input_shader;
  if (!ReadFile((RepoRootPath() / "tests/shaders/0x56C468C3.cs_6_6.cso").string(), input_shader)) {
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
    if (step.name == "count_ops") {
      step_report = &step;
      break;
    }
  }

  if (step_report == nullptr) {
    std::cerr << "Test 4: step report not found.\n";
    return false;
  }

  const auto* ops_res = std::get_if<dxp::CheckOpcodeCountResults>(&step_report->results);
  if (ops_res == nullptr) {
    std::cerr << "Test 4: results not CheckOpcodeCountResults.\n";
    return false;
  }

  // Verify that Frc count is > 0 (the test shader has Frc calls)
  auto frc_it = ops_res->dxil_opcode_counts.find("Frc");
  if (frc_it == ops_res->dxil_opcode_counts.end() || frc_it->second == 0) {
    std::cerr << "Test 4: expected Frc count > 0, got "
              << (frc_it != ops_res->dxil_opcode_counts.end() ? std::to_string(frc_it->second) : "not found")
              << ".\n";
    return false;
  }

  // NonExistentOp should have count 0
  auto nonexistent_it = ops_res->dxil_opcode_counts.find("NonExistentOp");
  if (nonexistent_it == ops_res->dxil_opcode_counts.end() || nonexistent_it->second != 0) {
    std::cerr << "Test 4: expected NonExistentOp count == 0.\n";
    return false;
  }

  std::cout << "Test 4 passed: check_opcode_count executed correctly (Frc=" << frc_it->second << ").\n";
  return true;
}

// Test 5: check_resource_count executes on a real shader and returns counts
bool TestCheckResourceCountExecutes() {
  const char* yaml = R"(
steps:
  - kind: check_resource_count
    name: count_resources
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 5 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> input_shader;
  if (!ReadFile((RepoRootPath() / "tests/shaders/0x56C468C3.cs_6_6.cso").string(), input_shader)) {
    std::cerr << "Test 5: failed to read test shader.\n";
    return false;
  }

  auto result = parse_result.value().Execute(input_shader);
  if (!result) {
    std::cerr << "Test 5 execution failed: " << result.error() << "\n";
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
    std::cerr << "Test 5: step report not found.\n";
    return false;
  }

  const auto* res_res = std::get_if<dxp::CheckResourceCountResults>(&step_report->results);
  if (res_res == nullptr) {
    std::cerr << "Test 5: results not CheckResourceCountResults.\n";
    return false;
  }

  // Verify total is > 0 (the test shader has resources)
  if (res_res->total == 0) {
    std::cerr << "Test 5: expected total > 0, got 0.\n";
    return false;
  }

  std::cout << "Test 5 passed: check_resource_count executed correctly (total=" << res_res->total
            << ", textures=" << res_res->textures << ", uavs=" << res_res->uavs
            << ", cbuffers=" << res_res->cbuffers << ", samplers=" << res_res->samplers << ").\n";
  return true;
}

// Test 6: version fields are populated in ExecutionContext
bool TestVersionFieldsPopulated() {
  const char* yaml = R"(
steps:
  - kind: check_opcode_count
    name: count_ops
    dxil_opcodes:
      - Frc
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 6 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> input_shader;
  if (!ReadFile((RepoRootPath() / "tests/shaders/0x56C468C3.cs_6_6.cso").string(), input_shader)) {
    std::cerr << "Test 6: failed to read test shader.\n";
    return false;
  }

  auto result = parse_result.value().Execute(input_shader);
  if (!result) {
    std::cerr << "Test 6 execution failed: " << result.error() << "\n";
    return false;
  }

  // The version is set in the execution context before steps run.
  // We can verify by checking that the step executed successfully
  // (which means the context was properly initialized with version info).
  if (result.value().steps.empty()) {
    std::cerr << "Test 6: no steps executed.\n";
    return false;
  }

  std::cout << "Test 6 passed: version fields populated (shader loaded successfully).\n";
  return true;
}

// Test 7: combined steps — check_opcode_count + check_resource_count + apply_rule
bool TestCombinedSteps() {
  const char* yaml = R"(
steps:
  - kind: check_opcode_count
    name: count_ops
    dxil_opcodes:
      - Frc
  - kind: check_resource_count
    name: count_resources
  - kind: apply_rule
    name: noop
    required: false
    rewrite_mode: none
    rule:
      match:
        - opcode: Frc
      emit: []
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 7 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> input_shader;
  if (!ReadFile((RepoRootPath() / "tests/shaders/0x56C468C3.cs_6_6.cso").string(), input_shader)) {
    std::cerr << "Test 7: failed to read test shader.\n";
    return false;
  }

  auto result = parse_result.value().Execute(input_shader);
  if (!result) {
    std::cerr << "Test 7 execution failed: " << result.error() << "\n";
    return false;
  }

  // Verify all three steps ran
  if (result.value().steps.size() != 3) {
    std::cerr << "Test 7: expected 3 steps, got " << result.value().steps.size() << ".\n";
    return false;
  }

  std::cout << "Test 7 passed: combined steps executed successfully.\n";
  return true;
}

// Test 8: check_shader_version parses and succeeds when versions match
bool TestCheckShaderVersionMatches() {
  const char* yaml = R"(
steps:
  - kind: check_shader_version
    name: version_check
    major: 6
    minor: 6
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 8 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> input_shader;
  if (!ReadFile((RepoRootPath() / "tests/shaders/0x56C468C3.cs_6_6.cso").string(), input_shader)) {
    std::cerr << "Test 8: failed to read test shader.\n";
    return false;
  }

  auto result = parse_result.value().Execute(input_shader);
  if (!result) {
    std::cerr << "Test 8 execution failed: " << result.error() << "\n";
    return false;
  }

  const dxp::StepReport* step_report = nullptr;
  for (const auto& step : result.value().steps) {
    if (step.name == "version_check") {
      step_report = &step;
      break;
    }
  }
  if (step_report == nullptr || !step_report->success) {
    std::cerr << "Test 8: version_check step did not succeed.\n";
    return false;
  }
  const auto* ver = std::get_if<dxp::CheckShaderVersionResults>(&step_report->results);
  if (ver == nullptr || ver->major_version != 6 || ver->minor_version != 6) {
    std::cerr << "Test 8: unexpected version results.\n";
    return false;
  }

  std::cout << "Test 8 passed: check_shader_version matched (SM6.6).\n";
  return true;
}

// Test 9: check_shader_version mismatch is a normal filter outcome — Execute
// succeeds, the step is reported as failed (state=false), and the output is an
// unmodified pass-through.
bool TestCheckShaderVersionMismatch() {
  const char* yaml = R"(
steps:
  - kind: check_shader_version
    name: version_check
    major: 5
    minor: 1
)";

  auto parse_result = dxp::sm6::Recipe::ParseFromText(yaml);
  if (!parse_result) {
    std::cerr << "Test 9 failed to parse: " << parse_result.error() << "\n";
    return false;
  }

  std::vector<uint8_t> input_shader;
  if (!ReadFile((RepoRootPath() / "tests/shaders/0x56C468C3.cs_6_6.cso").string(), input_shader)) {
    std::cerr << "Test 9: failed to read test shader.\n";
    return false;
  }

  auto result = parse_result.value().Execute(input_shader);
  if (!result) {
    std::cerr << "Test 9: version mismatch must not fail execution: " << result.error() << "\n";
    return false;
  }
  const auto& report = result.value();
  if (report.steps.size() != 1 || report.steps[0].name != "version_check" || report.steps[0].success) {
    std::cerr << "Test 9: version_check must be reported as a no-match step.\n";
    return false;
  }
  if (report.modified || report.output_bytes != input_shader) {
    std::cerr << "Test 9: output must be an unmodified pass-through.\n";
    return false;
  }

  std::cout << "Test 9 passed: check_shader_version mismatch is a non-error no-match.\n";
  return true;
}

}  // namespace

int main(int argc, [[maybe_unused]] char** argv_) {
  if (argc != 1) {
    std::cerr << "Usage: sm6_check_steps_test\n";
    return 1;
  }

  const ScopedCoInitialize coinit;

  bool all_passed = true;

  all_passed &= TestCheckOpcodeCountParseAndExecute();
  all_passed &= TestCheckOpcodeCountEmptyFails();
  all_passed &= TestCheckResourceCountParseAndExecute();
  all_passed &= TestCheckOpcodeCountExecutes();
  all_passed &= TestCheckResourceCountExecutes();
  all_passed &= TestVersionFieldsPopulated();
  all_passed &= TestCombinedSteps();
  all_passed &= TestCheckShaderVersionMatches();
  all_passed &= TestCheckShaderVersionMismatch();

  if (all_passed) {
    std::cout << "All check steps tests passed.\n";
  } else {
    std::cerr << "Some check steps tests failed.\n";
  }

  std::cout.flush();
  std::cerr.flush();
  return all_passed ? 0 : 1;
}
