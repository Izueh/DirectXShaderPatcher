// Phase 3 of SM5 declaration cross-referencing: `decl:` operand constraints.
//
// Pins:
//   - `decl: {dimension: ...}` on a resource match operand matches only when the
//     operand's t# is declared with that dimension (corpus: t0..t8 are 2d, no 2darray);
//   - `decl: {semantic: position}` on an input match operand matches only the
//     SV_Position register (v1), regardless of its register number;
//   - a decl constraint with no resolvable declaration is a no-match (not an error);
//   - decl on emit operands is rejected at parse time;
//   - decl fields invalid for the operand type are rejected at parse time;
//   - decl constraints see declarations added by earlier add_resource steps.
#include <cstdint>
#include <expected>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

using dxp::sm5::model::Opcode;

int g_failures = 0;

void Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
  }
}

/// @brief Runs an inline recipe, returns the apply_rule step's success + match count.
struct RunOutcome {
  bool ran = false;
  bool step_success = false;
  uint32_t match_count = 0;
};

RunOutcome RunRecipe(const std::string& yaml, const std::vector<uint8_t>& shader) {
  RunOutcome out;
  auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml, "decl-test");
  if (!parse_result) {
    std::cerr << "parse failed: " << parse_result.error() << "\n";
    return out;
  }
  auto patch_result = parse_result->Execute(shader);
  if (!patch_result) {
    std::cerr << "execute failed: " << patch_result.error() << "\n";
    return out;
  }
  out.ran = true;
  for (const auto& step : patch_result->steps) {
    if (step.name == "decl_probe" || step.name == "self_probe") {
      out.step_success = step.success;
      if (const auto* results = std::get_if<dxp::ApplyRuleResults>(&step.results)) {
        out.match_count = results->match_count;
      }
    }
  }
  return out;
}

/// @brief Standard match recipe body: match `match_body`, emit a single mov marker.
std::string MatchMovRecipe(const std::string& match_body) {
  std::string yaml = "version: 1\nsteps:\n";
  yaml += "  - kind: apply_rule\n";
  yaml += "    name: decl_probe\n";
  yaml += "    required: false\n";
  yaml += "    match_mode: match_all\n";
  yaml += "    rewrite_mode: replace\n";
  yaml += "    rule:\n";
  yaml += "      match:\n";
  yaml += match_body;
  yaml += "      emit:\n";
  yaml += "        - opcode: mov\n";
  yaml += "          operands:\n";
  yaml += "            - {type: temp, components: {selection_mode: mask, value: x}}\n";
  yaml += "            - {type: immediate32, immediates_u32: [1]}\n";
  return yaml;
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_decl_constraint_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> shader;
  if (!ReadFile((RepoRootPath() / args[1]).string(), shader)) {
    std::cerr << "Failed to read file: " << args[1] << "\n";
    return 1;
  }

  // --- 1. dimension constraint: texture2d matches, texture2darray does not ---
  // Corpus: t0..t8 texture2d, t9 texture2d(uint) — no texture2darray anywhere.
  {
    auto match_2d = RunRecipe(MatchMovRecipe(
                                  "        - opcode: sample_l\n"
                                  "          operands:\n"
                                  "            - any: true\n"
                                  "            - any: true\n"
                                  "            - {type: resource, decl: {dimension: texture2d}}\n"
                                  "            - any: true\n"),
                              shader);
    Check(match_2d.ran, "texture2d decl recipe should run");
    Check(match_2d.ran && match_2d.match_count >= 1, "texture2d decl constraint should match at least one sample_l");

    auto match_2darray = RunRecipe(MatchMovRecipe(
                                       "        - opcode: sample_l\n"
                                       "          operands:\n"
                                       "            - any: true\n"
                                       "            - any: true\n"
                                       "            - {type: resource, decl: {dimension: texture2darray}}\n"
                                       "            - any: true\n"),
                                   shader);
    Check(match_2darray.ran, "texture2darray decl recipe should run");
    Check(match_2darray.ran && match_2darray.match_count == 0, "texture2darray decl constraint must not match (no 2darray declared)");
  }

  // --- 2. semantic constraint: input with position semantic ---
  // Corpus reads v1 (dcl_input_ps_siv position) via `mad r7.xy, r0, l(...), v1.xyxx` —
  // v1 is the mad's 4th operand.
  {
    auto match_pos = RunRecipe(MatchMovRecipe(
                                   "        - opcode: mad\n"
                                   "          operands:\n"
                                   "            - any: true\n"
                                   "            - any: true\n"
                                   "            - any: true\n"
                                   "            - {type: input, decl: {semantic: position}}\n"),
                               shader);
    Check(match_pos.ran, "semantic decl recipe should run");
    Check(match_pos.ran && match_pos.match_count >= 1, "semantic: position decl constraint should match the mad reading v1");

    // Negative control: the same pattern against semantic vertex_id must not match.
    auto match_wrong = RunRecipe(MatchMovRecipe(
                                     "        - opcode: mad\n"
                                     "          operands:\n"
                                     "            - any: true\n"
                                     "            - any: true\n"
                                     "            - any: true\n"
                                     "            - {type: input, decl: {semantic: vertex_id}}\n"),
                                 shader);
    Check(match_wrong.ran && match_wrong.match_count == 0, "semantic: vertex_id decl constraint must not match");
  }

  // --- 3. missing declaration is a no-match, not an error ---
  {
    auto match_missing = RunRecipe(MatchMovRecipe(
                                       "        - opcode: sample_l\n"
                                       "          operands:\n"
                                       "            - any: true\n"
                                       "            - any: true\n"
                                       "            - {type: resource, decl: {dimension: texture3d}}\n"
                                       "            - any: true\n"),
                                   shader);
    Check(match_missing.ran, "missing-decl recipe must run without error");
    Check(match_missing.ran && match_missing.match_count == 0, "texture3d decl constraint must not match (no texture3d declared)");
  }

  // --- 4. decl on emit operands is rejected at validation time ---
  // (Step validation runs lazily on the first Execute call.)
  {
    std::string yaml = "version: 1\nsteps:\n";
    yaml += "  - kind: apply_rule\n";
    yaml += "    name: bad_emit_decl\n";
    yaml += "    required: false\n";
    yaml += "    rule:\n";
    yaml += "      match:\n";
    yaml += "        - opcode: mov\n";
    yaml += "      emit:\n";
    yaml += "        - opcode: mov\n";
    yaml += "          operands:\n";
    yaml += "            - {type: temp, decl: {semantic: position}}\n";
    yaml += "            - {type: immediate32, immediates_u32: [1]}\n";
    auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml, "emit-decl");
    Check(parse_result.has_value(), "recipe with emit decl should parse (validation is lazy)");
    bool rejected = false;
    if (parse_result) {
      auto run = parse_result->Execute(shader);
      rejected = !run.has_value();
      if (!rejected) {
        std::cerr << "emit-decl error was: (execute unexpectedly succeeded)\n";
      }
    }
    Check(rejected, "decl on emit operand must be rejected at validation time");
  }

  // --- 5. decl field invalid for operand type is rejected at validation time ---
  {
    std::string yaml = "version: 1\nsteps:\n";
    yaml += "  - kind: apply_rule\n";
    yaml += "    name: bad_field\n";
    yaml += "    required: false\n";
    yaml += "    rule:\n";
    yaml += "      match:\n";
    yaml += "        - opcode: mov\n";
    yaml += "          operands:\n";
    yaml += "            - {type: temp, decl: {semantic: position}}\n";
    yaml += "      emit:\n";
    yaml += "        - opcode: mov\n";
    yaml += "          operands:\n";
    yaml += "            - {type: temp, components: {selection_mode: mask, value: x}}\n";
    yaml += "            - {type: immediate32, immediates_u32: [1]}\n";
    auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml, "field-mismatch");
    Check(parse_result.has_value(), "recipe with field/type mismatch should parse (validation is lazy)");
    bool rejected = false;
    if (parse_result) {
      auto run = parse_result->Execute(shader);
      rejected = !run.has_value();
    }
    Check(rejected, "decl semantic on a temp operand must be rejected at validation time");
  }

  // --- 6. decl sees declarations added by a prior add_resource step ---
  {
    std::string yaml = "version: 1\nsteps:\n";
    yaml += "  - kind: add_resource\n";
    yaml += "    name: add_2darray\n";
    yaml += "    textures:\n";
    yaml += "      - handle: patch_tex\n";
    yaml += "        dimension: texture2darray\n";
    yaml += "  - kind: apply_rule\n";
    yaml += "    name: self_probe\n";
    yaml += "    required: false\n";
    yaml += "    match_mode: match_all\n";
    yaml += "    rewrite_mode: none\n";
    yaml += "    rule:\n";
    yaml += "      match:\n";
    yaml += "        - opcode: dcl_resource\n";
    yaml += "          operands:\n";
    yaml += "            - {type: resource, decl: {dimension: texture2darray}}\n";
    auto parse_result = dxp::sm5::Recipe::ParseFromText(yaml, "cross-step");
    Check(parse_result.has_value(), "cross-step decl recipe should parse");
    if (parse_result) {
      auto run = parse_result->Execute(shader);
      Check(run.has_value(), "cross-step decl recipe should run");
      bool step_success = false;
      uint32_t match_count = 0;
      if (run) {
        for (const auto& step : run->steps) {
          if (step.name == "self_probe") {
            step_success = step.success;
            if (const auto* results = std::get_if<dxp::ApplyRuleResults>(&step.results)) {
              match_count = results->match_count;
            }
          }
        }
      }
      Check(run && step_success && match_count >= 1, "decl constraint must resolve the add_resource-declared texture2darray");
    }
  }

  if (g_failures == 0) {
    std::cout << "sm5_decl_constraint_test passed.\n";
    return 0;
  }
  std::cerr << g_failures << " check(s) failed.\n";
  return 1;
}
