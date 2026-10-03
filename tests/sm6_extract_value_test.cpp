// extractvalue integration tests against the compiled rule-pattern sample.
//
// Covers (per the approved plan):
//   * canonical emit extract single-index success + legacy v1 equivalence
//   * canonical emit struct_name guard (positive + emit-time guard failure)
//   * captured ConstantInt index success/missing/non-ConstantInt errors
//   * multi-index on a flat aggregate (per-depth bounds error)
//   * compile-time error contract (exclusivity, empty indices, operand index)
//   * canonical match-side extract (positive + required no-match)

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm6/Recipe.hpp"
#include "dxp/StepResults.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
  if (condition) {
    std::cout << "ok: " << label << "\n";
  } else {
    std::cerr << "FAILED: " << label << "\n";
    ++failures;
  }
}

unsigned RuleMatches(const dxp::RecipeReport& report) {
  unsigned total = 0;
  for (const auto& step : report.steps) {
    if (const auto* ar = std::get_if<dxp::ApplyRuleResults>(&step.results)) total += ar->match_count;
  }
  return total;
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm6_extract_value_test <input.cs_6_6.cso>\n";
    return 1;
  }

  const ScopedCoInitialize coinit;

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  // The sample contains one live texture load at t7 followed by its x extract.
  const std::string base_yaml_head = R"YAML(
steps:
  - kind: add_resource
    name: add_resources
    cbuffers:
      - handle: frame_constants
        space: 0
        size: 16
        type: ISFastFrameConstants
        fields:
          - name: FrameIndex
            type: U32
            width: 1
            offset: 0
  - kind: apply_rule
    name: blue_noise_scalar_slice
    rewrite_mode: after
    rule:
        match:
          - opcode: TextureLoad
            capture: texture_load
            operands:
              - index: 1
                kind: resource
                resource_class: SRV
                resource_kind: Texture2D
                register_index: 7
                space: 0
              - index: 3
                kind: call
                capture: coord_x
          - opcode: extractvalue
            capture: shader_x
            extract:
              indices: [0]
        emit:
          - opcode: CBufferLoadLegacy
            result_component_type: I32
            name: frame_load
            operands:
              - index: 1
                kind: resource
                handle: frame_constants
              - index: 2
                kind: constant
                constant_int_values: [0]
)YAML";

  // Connect the extracted register value to the shader's buffer write, so
  // canonical/legacy equivalence checks observable Execute output.
  const std::string output_emit = R"YAML(
          - opcode: sitofp
            name: frame_value
            result_component_type: F32
            operands:
              - index: 0
                capture: frame_index
          - opcode: fadd
            name: adjusted_x
            result_component_type: F32
            replace_captured: shader_x
            operands:
              - index: 0
                capture: shader_x
              - index: 1
                capture: frame_value
)YAML";

  // ─── 1. Canonical emit single-index vs legacy v1 equivalence ──────────────
  {
    const std::string legacy = base_yaml_head + R"YAML(
          - aggregate: frame_load
            extract_index: 0
            name: frame_index
)YAML" + output_emit + R"YAML(
    match_mode: match_all
    required: false
)YAML";
    const std::string canonical = base_yaml_head + R"YAML(
          - name: frame_index
            extract:
              aggregate: frame_load
              indices: [0]
              result_type:
                component_type: I32
)YAML" + output_emit + R"YAML(
    match_mode: match_all
    required: false
)YAML";
    auto legacy_parse = dxp::sm6::Recipe::ParseFromText(legacy);
    Check(legacy_parse.has_value(), "legacy v1 recipe parses");
    auto canonical_parse = dxp::sm6::Recipe::ParseFromText(canonical);
    Check(canonical_parse.has_value(), "canonical recipe parses");
    unsigned legacy_matches = 0, canonical_matches = 0;
    if (legacy_parse && canonical_parse) {
      auto legacy_run = legacy_parse.value().Execute(input_bytes);
      auto canonical_run = canonical_parse.value().Execute(input_bytes);
      Check(legacy_run.has_value(), "legacy v1 recipe executes");
      Check(canonical_run.has_value(), "canonical recipe executes");
      if (legacy_run) legacy_matches = RuleMatches(*legacy_run);
      if (canonical_run) canonical_matches = RuleMatches(*canonical_run);
      Check(legacy_matches > 0, "legacy recipe has matches");
      Check(canonical_matches == legacy_matches, "canonical match count equals legacy");
      if (legacy_run && canonical_run) {
        Check(canonical_run->output_bytes == legacy_run->output_bytes, "canonical and legacy Execute output are identical");
        Check(canonical_run->output_bytes != input_bytes, "extraction replacement changes shader output");
      }
    }
  }

  // ─── 2. struct_name guard (positive) ───────────────────────────────────────
  {
    const std::string yaml = base_yaml_head + R"YAML(
          - name: frame_index
            extract:
              aggregate: frame_load
              indices: [0]
              aggregate_type:
                struct_name: dx.types.CBufRet.i32
              result_type:
                component_type: I32
    match_mode: match_all
    required: false
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(parse.has_value(), "struct_name guard recipe parses");
    if (parse) {
      auto result = parse.value().Execute(input_bytes);
      Check(result.has_value() && RuleMatches(*result) > 0, "struct_name guard matches");
    }
  }

  // ─── 3. struct_name guard (emit-time failure) ──────────────────────────────
  {
    const std::string yaml = base_yaml_head + R"YAML(
          - name: frame_index
            extract:
              aggregate: frame_load
              indices: [0]
              aggregate_type:
                struct_name: dx.types.CBufRet.f32
    match_mode: match_all
    required: false
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(parse.has_value(), "wrong struct_name guard recipe parses");
    if (parse) {
      auto result = parse.value().Execute(input_bytes);
      Check(!result && result.error().find("does not satisfy the requested aggregate type") != std::string::npos,
            "emit-time aggregate_type guard failure error");
    }
  }

  // ─── 4. Captured-index runtime ───────────────────────────────────────────────
  // 4.0 Positive: captured ConstantInt index (constant operand capture).
  {
    const std::string yaml = R"YAML(
steps:
  - kind: add_resource
    name: add_resources
    cbuffers:
      - handle: frame_constants
        space: 0
        size: 16
        type: ISFastFrameConstants
        fields:
          - name: FrameIndex
            type: U32
            width: 1
            offset: 0
  - kind: apply_rule
    name: captured_index_positive
    rewrite_mode: after
    rule:
        match:
          - opcode: TextureLoad
            capture: texture_load
            operands:
              - index: 1
                kind: resource
                resource_class: SRV
                resource_kind: Texture2D
                register_index: 7
                space: 0
              - index: 2
                kind: constant
                constant_int_values: [0]
                capture: idx_zero
              - index: 3
                kind: call
                capture: coord_x
        emit:
          - opcode: CBufferLoadLegacy
            result_component_type: I32
            name: frame_load
            operands:
              - index: 1
                kind: resource
                handle: frame_constants
              - index: 2
                kind: constant
                constant_int_values: [0]
          - name: frame_index_pos
            extract:
              aggregate: frame_load
              indices: [{capture: idx_zero}]
    match_mode: match_all
    required: false
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(parse.has_value(), "positive captured-index recipe parses");
    if (parse) {
      auto result = parse.value().Execute(input_bytes);
      Check(result.has_value() && RuleMatches(*result) > 0, "positive captured ConstantInt index succeeds");
    }
  }
  {
    const std::string yaml = base_yaml_head + R"YAML(
          - name: frame_index
            extract:
              aggregate: frame_load
              indices: [{capture: no_such_index}]
    match_mode: match_all
    required: false
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(parse.has_value(), "missing captured index recipe parses");
    if (parse) {
      auto result = parse.value().Execute(input_bytes);
      Check(!result && result.error().find("extract index 0: capture 'no_such_index' was not produced by any match or earlier emit") != std::string::npos,
            "missing captured index error");
    }
  }
  {
    const std::string yaml = base_yaml_head + R"YAML(
          - opcode: fadd
            result_component_type: F32
            name: float_capture
            operands:
              - index: 0
                kind: constant
                constant_float_values: [1.0]
              - index: 1
                kind: constant
                constant_float_values: [2.0]
          - name: frame_index
            extract:
              aggregate: frame_load
              indices: [{capture: float_capture}]
    match_mode: match_all
    required: false
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(parse.has_value(), "non-ConstantInt captured index recipe parses");
    if (parse) {
      auto result = parse.value().Execute(input_bytes);
      Check(!result && result.error().find("extract index 0: capture 'float_capture' must resolve to an integer ConstantInt") != std::string::npos,
            "non-ConstantInt captured index error");
    }
  }

  // ─── 5. Flat-aggregate multi-index + out-of-range + missing aggregate ───────
  {
    const std::string yaml = base_yaml_head + R"YAML(
          - name: frame_index
            extract:
              aggregate: frame_load
              indices: [0, 1]
    match_mode: match_all
    required: false
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(parse.has_value(), "multi-index recipe parses");
    if (parse) {
      auto result = parse.value().Execute(input_bytes);
      Check(!result && result.error().find("extract path enters non-aggregate type i32 at depth 1") != std::string::npos,
            "multi-index on flat aggregate error");
    }
  }
  {
    const std::string yaml = base_yaml_head + R"YAML(
          - name: frame_index
            extract:
              aggregate: frame_load
              indices: [8]
    match_mode: match_all
    required: false
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(parse.has_value(), "out-of-range index recipe parses");
    if (parse) {
      auto result = parse.value().Execute(input_bytes);
      Check(!result && result.error().find("extract index 8 out of range at depth 0 for %dx.types.CBufRet.i32") != std::string::npos,
            "out-of-range index error");
    }
  }
  {
    const std::string yaml = base_yaml_head + R"YAML(
          - name: frame_index
            extract:
              aggregate: no_such_aggregate
              indices: [0]
    match_mode: match_all
    required: false
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(parse.has_value(), "missing aggregate recipe parses");
    if (parse) {
      auto result = parse.value().Execute(input_bytes);
      Check(!result && result.error().find("aggregate 'no_such_aggregate' was not produced by any match or earlier emit") != std::string::npos,
            "missing aggregate error");
    }
  }

  // ─── 6. Compile-time error contract ─────────────────────────────────────────
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: add
          extract:
            indices: [0]
      emit: []
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("extract is only valid with opcode 'extractvalue'") != std::string::npos,
          "extract without extractvalue opcode rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          extract:
            indices: [0]
          extract_index: 1
      emit: []
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("extract and extract_index cannot both be specified") != std::string::npos,
          "extract and legacy extract_index rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: add
          extract_index: 0
      emit: []
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("extract_index is only valid with opcode 'extractvalue'") != std::string::npos,
          "legacy extract_index without extractvalue rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          extract:
            indices: []
      emit: []
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("extract.indices requires at least one index") != std::string::npos,
          "empty match indices rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          operands:
            - index: 1
      emit: []
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("extractvalue has only aggregate operand 0; operand index 1 is invalid") != std::string::npos,
          "extractvalue operand index > 0 rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          extract:
            indices: [0]
      emit:
        - name: ex
          extract:
            indices: [0]
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("extract.aggregate is required") != std::string::npos,
          "emit extract without aggregate rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          extract:
            indices: [0]
      emit:
        - name: ex
          opcode: fadd
          extract:
            aggregate: a
            indices: [0]
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("extract cannot be combined with opcode, cast_opcode, capture, or operands") != std::string::npos,
          "emit extract combined with opcode rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          extract:
            indices: [0]
      emit:
        - name: ex
          extract_index: 0
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("legacy extract_index requires aggregate") != std::string::npos,
          "dangling legacy extract_index rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          extract:
            indices: [0]
      emit:
        - name: ex
          aggregate: a
          opcode: fadd
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("aggregate cannot be combined with opcode, cast_opcode, capture, or operands") != std::string::npos,
          "legacy aggregate combined with opcode rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          extract:
            indices: [0]
      emit:
        - name: ex
          result_component_type: I32
          extract:
            aggregate: a
            indices: [0]
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("extract cannot be combined with result_component_type") != std::string::npos,
          "emit extract combined with result_component_type rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          extract:
            indices: [0]
            aggregate_type:
              struct_name: X
              component_type: F32
      emit: []
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("struct_name cannot be combined with component_type") != std::string::npos,
          "struct_name + component_type rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          extract:
            indices: [0]
            aggregate_type:
              struct_name: ""
      emit: []
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("struct_name cannot be empty") != std::string::npos,
          "empty struct_name rejected");
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: r
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          extract:
            indices: [0]
            aggregate_type:
              kind: array
              component_type: F32
      emit: []
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parse && parse.error().find("component_type cannot be combined with kind 'array' or 'struct'") != std::string::npos,
          "component_type + kind array rejected");
  }

  // ─── 7. Canonical match-side extract ───────────────────────────────────────
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: canonical_match
    required: true
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          capture: cb_extract
          extract:
            indices: [0]
            aggregate_type:
              struct_name: dx.types.CBufRet.i32
            result_type:
              component_type: I32
      emit: []
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(parse.has_value(), "canonical match recipe parses");
    if (parse) {
      auto result = parse.value().Execute(input_bytes);
      Check(result.has_value() && RuleMatches(*result) > 0, "canonical match-side extract matches");
      if (result && !result.value().steps.empty()) {
        Check(result.value().steps[0].success, "canonical match step success");
      }
    }
  }
  {
    const std::string yaml = R"YAML(
steps:
  - kind: apply_rule
    name: canonical_match_nomatch
    required: true
    rewrite_mode: none
    rule:
      match:
        - opcode: extractvalue
          extract:
            indices: [0]
            aggregate_type:
              struct_name: dx.types.CBufRet.u32
      emit: []
)YAML";
    auto parse = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(parse.has_value(), "canonical match no-match recipe parses");
    if (parse) {
      auto result = parse.value().Execute(input_bytes);
      Check(result.has_value() && !result.value().steps.empty() && !result.value().steps[0].success,
            "required no-match step reported failed");
      if (result) {
        Check(!result.value().modified, "no-match recipe leaves output unmodified");
      }
    }
  }

  if (failures != 0) {
    std::cerr << failures << " extract_value test(s) failed\n";
    return 1;
  }
  std::cout << "sm6_extract_value_test passed\n";
  std::cout.flush();
  std::cerr.flush();
  return 0;
}
