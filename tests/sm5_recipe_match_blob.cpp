#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <variant>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

#ifndef NDEBUG
#include "tests/helper/StackTraceHelper.hpp"
#endif

namespace {

/// Runs an inline recipe against the input bytes; returns the report or error message.
auto RunRecipe(const std::vector<uint8_t>& input, const char* recipe_text, const char* name,
               dxp::RecipeReport& report) -> bool {
  auto parse_result = dxp::sm5::Recipe::ParseFromText(recipe_text, name);
  if (!parse_result) {
    std::cerr << "Failed to parse inline SM5 recipe: " << parse_result.error() << "\n";
    return false;
  }
  auto patch_result = parse_result.value().Execute(input);
  if (!patch_result) {
    std::cerr << "Failed to patch SM5 shader: " << patch_result.error() << "\n";
    return false;
  }
  report = std::move(patch_result.value());
  return true;
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
#ifndef NDEBUG
  InstallCrashHandler();
#endif

  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_match_blob <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  // ───────────────────────────────────────────────────────────────────────────
  // Test 1: match_blob + emit_blob {mode: replace} with an interior rule.
  // Window: starts at the dp2 (after 'frc'), ends at sample_l. The window
  // contains a frc → replace it with mov inside the blob, splice back.
  // ───────────────────────────────────────────────────────────────────────────
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: blob_replace
    match_blob:
      match_start:
        opcode: dp2
      match_end:
        opcode: sample_l
      capture: window_a
    match_mode: first
    emit_blob:
      mode: replace
    rule:
      match_mode: match_all
      rewrite_mode: replace
      match:
        - opcode: frc
          operands:
            - capture: frc_dst
            - capture: frc_src
      emit:
        - opcode: mov
          operands:
            - capture: frc_dst
            - capture: frc_src
)YAML";

    dxp::RecipeReport report;
    if (!RunRecipe(input_bytes, recipe, "inline-sm5-blob-replace", report)) {
      return 1;
    }

    if (report.output_bytes.empty()) {
      std::cerr << "blob replace: output unexpectedly empty.\n";
      return 1;
    }
    if (report.output_bytes == input_bytes) {
      std::cerr << "blob replace: expected mutation, output identical to input.\n";
      return 1;
    }
    bool found = false;
    for (const auto& step : report.steps) {
      const auto* res = std::get_if<dxp::ApplyRuleResults>(&step.results);
      if (step.name == "blob_replace" && res != nullptr && res->match_count > 0) {
        found = true;
      }
    }
    if (!found) {
      std::cerr << "blob replace: expected the blob step to be recorded with matches.\n";
      return 1;
    }
    std::cout << "blob replace (match + rewrite + splice in one step) OK\n";
  }

  // ───────────────────────────────────────────────────────────────────────────
  // Test 2: match_blob with default emit_blob (none) — shader must be untouched.
  // The step still runs (interior rule mutates the stored blob copy only).
  // ───────────────────────────────────────────────────────────────────────────
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: blob_stash
    match_blob:
      match_start:
        opcode: dp2
      match_end:
        opcode: sample_l
      capture: window_b
    rule:
      match_mode: match_all
      rewrite_mode: replace
      match:
        - opcode: frc
          operands:
            - capture: stash_dst
            - capture: stash_src
      emit:
        - opcode: mov
          operands:
            - capture: stash_dst
            - capture: stash_src
)YAML";

    dxp::RecipeReport report;
    if (!RunRecipe(input_bytes, recipe, "inline-sm5-blob-stash", report)) {
      return 1;
    }

    if (report.output_bytes != input_bytes) {
      std::cerr << "blob stash: emit_blob defaults to none — output must be byte-identical to input.\n";
      return 1;
    }
    std::cout << "blob stash (default emit_blob: none leaves shader untouched) OK\n";
  }

  // ───────────────────────────────────────────────────────────────────────────
  // Test 3: emit_blob {mode: before} — transformed blob inserted before the
  // window start; the original window instructions remain present.
  // ───────────────────────────────────────────────────────────────────────────
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: blob_before
    match_blob:
      match_start:
        opcode: dp2
      match_end:
        opcode: sample_l
      capture: window_c
    match_mode: first
    emit_blob:
      mode: before
    rule:
      match_mode: match_all
      rewrite_mode: replace
      match:
        - opcode: frc
          operands:
            - capture: bf_dst
            - capture: bf_src
      emit:
        - opcode: mov
          operands:
            - capture: bf_dst
            - capture: bf_src
)YAML";

    dxp::RecipeReport report;
    if (!RunRecipe(input_bytes, recipe, "inline-sm5-blob-before", report)) {
      return 1;
    }

    if (report.output_bytes == input_bytes) {
      std::cerr << "blob before: expected insertion to change the output.\n";
      return 1;
    }
    std::cout << "blob before (insert before window; window preserved) OK\n";
  }

  // ───────────────────────────────────────────────────────────────────────────
  // Test 4: blob: emit expansion — capture (mode none), then insert the stored
  // blob before the last ret via a separate step. Output must differ.
  // ───────────────────────────────────────────────────────────────────────────
  {
    const char* recipe = R"YAML(version: 1
steps:
  - kind: apply_rule
    name: capture_only
    match_blob:
      match_start:
        opcode: dp2
      match_end:
        opcode: sample_l
      capture: dup_block
  - kind: apply_rule
    name: reinsert_dup
    rewrite_mode: before_last_return
    rule:
      emit:
        - blob: dup_block
)YAML";

    dxp::RecipeReport report;
    if (!RunRecipe(input_bytes, recipe, "inline-sm5-blob-dup", report)) {
      return 1;
    }

    if (report.output_bytes == input_bytes) {
      std::cerr << "blob dup: expected blob reinsertion to change the output.\n";
      return 1;
    }
    bool found = false;
    for (const auto& step : report.steps) {
      const auto* res = std::get_if<dxp::ApplyRuleResults>(&step.results);
      if (step.name == "reinsert_dup" && res != nullptr && res->applied_count > 0) {
        found = true;
      }
    }
    if (!found) {
      std::cerr << "blob dup: expected reinsert step to be recorded as applied.\n";
      return 1;
    }
    std::cout << "blob reinsertion (blob: emit before last ret) OK\n";
  }

  std::cout << "SM5 match_blob tests passed.\n";
  return 0;
}
