#include <iostream>
#include <string>

#include "tests/helper/TestHelper.hpp"

#include "dxp/sm5/Recipe.hpp"

namespace {

bool Contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

/// Expects the recipe to be rejected: either at parse time, or at validation
/// time (cross-step reference checks like scope/blob run in ValidateRecipe).
int ExpectRejected(const char* recipe_text, const char* name, const std::string& expected_substring) {
  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, name);
  if (parse_result) {
    auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
    if (validate_result) {
      std::cerr << "Expected recipe '" << name << "' to be rejected, but it parsed and validated.\n";
      return 1;
    }
    if (!Contains(validate_result.error(), expected_substring)) {
      std::cerr << "Expected validation error for '" << name << "' to mention '" << expected_substring << "', got: "
                << validate_result.error() << "\n";
      return 1;
    }
    return 0;
  }
  if (!Contains(parse_result.error(), expected_substring)) {
    std::cerr << "Expected parse error for '" << name << "' to mention '" << expected_substring << "', got: "
              << parse_result.error() << "\n";
    return 1;
  }
  return 0;
}

}  // namespace

int main() {
  // 1. match_blob XOR scope (rule.match inside a match_blob step IS the interior rule — legal)
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: blob_and_scope
    match_blob:
      match_start: {opcode: mov}
      match_end: {opcode: sample_l}
      capture: w
    scope: w
    rule:
      match:
        - opcode: frc
)YAML";
    if (int rc = ExpectRejected(recipe, "blob_and_scope", "mutually exclusive")) return rc;
  }

  // 2. emit_blob without match_blob
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: emit_blob_alone
    emit_blob:
      mode: replace
    rule:
      match:
        - opcode: frc
)YAML";
    if (int rc = ExpectRejected(recipe, "emit_blob_alone", "only valid alongside match_blob")) return rc;
  }

  // 3. step-level rewrite_mode on a match_blob step
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: blob_with_rewrite
    match_blob:
      match_start: {opcode: mov}
      match_end: {opcode: sample_l}
      capture: w
    rewrite_mode: before
    rule:
      match:
        - opcode: frc
)YAML";
    if (int rc = ExpectRejected(recipe, "blob_with_rewrite", "not applicable")) return rc;
  }

  // 4. scope with no match_blob capture anywhere — unknown blob reference
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: scope_and_match
    scope: some_blob
    rule:
      match:
        - opcode: frc
)YAML";
    if (int rc = ExpectRejected(recipe, "scope_and_match", "unknown blob")) return rc;
  }

  // 5. duplicate blob capture name
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: blob_one
    match_blob:
      match_start: {opcode: mov}
      match_end: {opcode: sample_l}
      capture: dup_name
  - kind: apply_rule
    name: blob_two
    match_blob:
      match_start: {opcode: dp2}
      match_end: {opcode: sample_l}
      capture: dup_name
)YAML";
    if (int rc = ExpectRejected(recipe, "blob_dup_name", "duplicate SM5 name")) return rc;
  }

  // 6. before_last_return WITH match patterns must parse (match is an optional guard)
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: blr_with_guard
    rewrite_mode: before_last_return
    rule:
      match:
        - opcode: mul
      emit:
        - opcode: mov
)YAML";
    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe, "blr_with_guard");
    if (!parse_result) {
      std::cerr << "Expected before_last_return with a guard match to parse: " << parse_result.error() << "\n";
      return 1;
    }
  }

  // 7. before_last_return without match patterns must also parse (anchor comes from the program)
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: blr_no_guard
    rewrite_mode: before_last_return
    rule:
      emit:
        - opcode: mov
)YAML";
    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe, "blr_no_guard");
    if (!parse_result) {
      std::cerr << "Expected before_last_return without match patterns to parse: " << parse_result.error() << "\n";
      return 1;
    }
  }

  // 8. emit_blob: before/after modes parse alongside match_blob
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: blob_before_mode
    match_blob:
      match_start: {opcode: mov}
      match_end: {opcode: sample_l}
      capture: w
    emit_blob:
      mode: before
)YAML";
    auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe, "blob_before_mode");
    if (!parse_result) {
      std::cerr << "Expected match_blob with emit_blob: before to parse: " << parse_result.error() << "\n";
      return 1;
    }
  }

  std::cout << "SM5 blob validation tests passed.\n";
  return 0;
}
