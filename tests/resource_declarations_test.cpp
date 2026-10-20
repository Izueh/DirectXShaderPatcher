// Declared-resource reporting, cbuffer field layouts, opaque `meta:` pass-through and
// load-time resolution of cbuffer field references (SM5 operands, SM6 cbuffer_field).
//
// Usage: resource_declarations_test <sm5 ps_5_0.cso> <sm6 rule-pattern cs_6_6.cso>
//
// Field references are checked for byte-identical output against the equivalent
// hand-written row/component form, so the resolution adds no behavior of its own.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/sm6/Recipe.hpp"

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

std::vector<uint8_t> ReadFile(const char* path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

const dxp::ResourceDeclaration* FindDeclaration(const std::vector<dxp::ResourceDeclaration>& declarations, const std::string& handle) {
  for (const auto& declaration : declarations) {
    if (declaration.handle == handle) return &declaration;
  }
  return nullptr;
}

bool MetaString(const std::optional<dxp::MetaValue>& meta, const char* key, const std::string& expected) {
  if (!meta) return false;
  const dxp::MetaValue* value = meta->Find(key);
  return value != nullptr && value->AsString() != nullptr && *value->AsString() == expected;
}

// ─── SM6 ──────────────────────────────────────────────────────────────────────

const std::string kSm6Declarations = R"YAML(
steps:
  - kind: add_resource
    name: add_inputs
    textures:
      - handle: hdr_scene
        kind: Texture2D
        element_type: F32
        vector_width: 4
        space: 100
        register_index: 0
        meta:
          description: Scene color before tonemapping
          format: r16g16b16a16_float
          size: full
          size_of: render_target
          clear: [0, 0, 0, 1]
    samplers:
      - handle: point_wrap
        space: 100
        register_index: 0
  - kind: add_resource
    name: add_optional_output
    required: false
    uavs:
      - handle: exposure
        kind: Texture2D
        element_type: F32
        vector_width: 1
        meta: { size: fixed, width: 1, height: 1 }
  - kind: add_resource
    name: add_params
    cbuffers:
      - handle: params
        meta: { reshade: { group: Grading } }
        fields:
          - name: scale
            type: F32
            width: 4
          - name: tint
            type: F32
            width: 3
            default: 0.5
            meta: { label: Tint, widget: color }
          - name: uv
            type: F32
            width: 2
          - name: frame_index
            type: U32
          - name: enabled
            type: U32
            default: true
)YAML";

void TestSm6Declarations() {
  auto parsed = dxp::sm6::Recipe::ParseFromText(kSm6Declarations);
  Check(parsed.has_value(), "SM6 declaration recipe parses" + (parsed ? std::string() : ": " + parsed.error()));
  if (!parsed) return;
  const auto declarations = parsed->GetDeclaredResources();
  Check(declarations.size() == 4, "SM6 reports four declarations");

  const auto* hdr = FindDeclaration(declarations, "hdr_scene");
  Check(hdr != nullptr && hdr->binding_class == dxp::BindingClass::Texture && hdr->step == "add_inputs", "texture class and step");
  Check(hdr != nullptr && hdr->type.dimension == dxp::ResourceDimension::Texture2D && hdr->type.element_type == dxp::ComponentType::F32 &&
            hdr->type.vector_width == 4u,
        "texture type info");
  Check(hdr != nullptr && hdr->register_index == 0u && hdr->space == 100u && !hdr->conditional, "texture binding is explicit");
  Check(hdr != nullptr && MetaString(hdr->meta, "size", "full") && MetaString(hdr->meta, "size_of", "render_target"), "texture meta strings");
  if (hdr != nullptr && hdr->meta) {
    const auto* clear = hdr->meta->Find("clear");
    Check(clear != nullptr && clear->AsArray() != nullptr && clear->AsArray()->size() == 4 && *(*clear->AsArray())[3].AsNumber() == 1.0,
          "texture meta arrays keep numbers");
  }

  const auto* exposure = FindDeclaration(declarations, "exposure");
  Check(exposure != nullptr && exposure->binding_class == dxp::BindingClass::TextureUav && exposure->conditional,
        "required: false marks the UAV conditional");
  Check(exposure != nullptr && !exposure->register_index && !exposure->space, "auto-assigned binding is unset");
  Check(exposure != nullptr && MetaString(exposure->meta, "size", "fixed"), "flow-style meta parses");

  const auto* params = FindDeclaration(declarations, "params");
  Check(params != nullptr && params->type.cbuffer.has_value(), "cbuffer reports its layout");
  if (params == nullptr || !params->type.cbuffer) return;
  const dxp::CBufferLayout& layout = *params->type.cbuffer;
  const auto* scale = layout.FindField("scale");
  const auto* tint = layout.FindField("tint");
  const auto* uv = layout.FindField("uv");
  const auto* frame_index = layout.FindField("frame_index");
  const auto* enabled = layout.FindField("enabled");
  Check(scale && scale->offset == 0 && tint && tint->offset == 16, "fields auto-pack into 16-byte rows");
  Check(uv && uv->offset == 32, "a field that does not fit the rest of a row starts a new row");
  Check(frame_index && frame_index->offset == 40 && enabled && enabled->offset == 44 && layout.size_in_bytes == 48,
        "later fields pack after it; size rounds to 16");
  Check(tint && tint->default_value == std::vector<double>{0.5, 0.5, 0.5}, "scalar default broadcasts to every component");
  Check(enabled && enabled->default_value == std::vector<double>{1.0}, "bool default becomes 1");
  Check(tint && tint->meta && MetaString(tint->meta, "widget", "color"), "field meta passes through");
  Check(params->meta && params->meta->Find("reshade") && params->meta->Find("reshade")->Find("group"), "nested meta objects");
}

void TestLayoutErrors() {
  const auto expect_error = [](const std::string& fields, const std::string& needle, const std::string& label) {
    const std::string yaml = "steps:\n  - kind: add_resource\n    name: add\n    cbuffers:\n      - handle: params\n        fields:\n" + fields;
    auto sm6 = dxp::sm6::Recipe::ParseFromText(yaml);
    auto sm5 = dxp::sm5::Recipe::ParseFromText(yaml);
    Check(!sm6 && sm6.error().find(needle) != std::string::npos, "SM6 rejects " + label + (sm6 ? "" : " (" + sm6.error() + ")"));
    Check(!sm5 && sm5.error().find(needle) != std::string::npos, "SM5 rejects " + label);
  };
  expect_error("          - { name: a, type: F32, width: 3, offset: 8 }\n", "straddles", "a field straddling a row");
  expect_error("          - { name: a, type: F16 }\n", "F32, U32 or I32", "unsupported field types");
  expect_error("          - { name: a, type: F32, width: 2, default: [1, 2, 3] }\n", "default has 3", "a default of the wrong length");
  expect_error("          - { name: a, type: F32 }\n          - { name: a, type: F32 }\n", "duplicate field", "duplicate fields");
  expect_error("          - { name: a, type: F32, offset: 16 }\n          - { name: b, type: F32, offset: 0 }\n", "ascending",
               "out-of-order offsets");
}

// Shared by the SM6 field and manual recipes: identical cbuffer declarations, so the
// only difference is how the emit reads frame_index (row 1, component 3).
const std::string kSm6Head = R"YAML(
steps:
  - kind: add_resource
    name: add_resources
    cbuffers:
      - handle: params
        space: 0
        type: Params
        fields:
          - { name: scale, type: F32, width: 4 }
          - { name: tint, type: F32, width: 3 }
          - { name: frame_index, type: U32 }
  - kind: apply_rule
    name: add_frame_index
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
)YAML";

const std::string kSm6Tail = R"YAML(
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
    match_mode: match_all
    required: false
)YAML";

void TestSm6FieldEmit(std::span<const uint8_t> shader) {
  const std::string field_form = kSm6Head + R"YAML(
          - name: frame_index
            cbuffer_field: { handle: params, field: frame_index }
)YAML" + kSm6Tail;
  const std::string manual_form = kSm6Head + R"YAML(
          - opcode: CBufferLoadLegacy
            result_component_type: I32
            name: frame_index__cbuffer_row
            operands:
              - { index: 1, kind: resource, handle: params }
              - { index: 2, kind: constant, constant_int_values: [1] }
          - name: frame_index
            extract:
              aggregate: frame_index__cbuffer_row
              indices: [3]
)YAML" + kSm6Tail;

  auto field = dxp::sm6::Recipe::ParseFromText(field_form);
  auto manual = dxp::sm6::Recipe::ParseFromText(manual_form);
  Check(field.has_value(), "SM6 cbuffer_field recipe parses" + (field ? std::string() : ": " + field.error()));
  Check(manual.has_value(), "SM6 manual row/component recipe parses" + (manual ? std::string() : ": " + manual.error()));
  if (!field || !manual) return;
  auto field_run = field->Execute(shader);
  auto manual_run = manual->Execute(shader);
  Check(field_run.has_value(), "SM6 cbuffer_field recipe executes" + (field_run ? std::string() : ": " + field_run.error()));
  Check(manual_run.has_value(), "SM6 manual recipe executes" + (manual_run ? std::string() : ": " + manual_run.error()));
  if (!field_run || !manual_run) return;
  Check(field_run->modified && field_run->output_bytes == manual_run->output_bytes,
        "SM6 cbuffer_field output is identical to the manual CBufferLoadLegacy + extractvalue");

  const auto binding = field_run->new_bindings.find("params");
  Check(binding != field_run->new_bindings.end() && binding->second.type.cbuffer &&
            binding->second.type.cbuffer->FindField("frame_index") != nullptr,
        "SM6 new_bindings carries the cbuffer layout");

  const auto expect_error = [](const std::string& yaml, const std::string& needle, const std::string& label) {
    auto parsed = dxp::sm6::Recipe::ParseFromText(yaml);
    Check(!parsed && parsed.error().find(needle) != std::string::npos, label + (parsed ? "" : " (" + parsed.error() + ")"));
  };
  expect_error(kSm6Head + "          - name: x\n            cbuffer_field: { handle: params, field: missing }\n" + kSm6Tail,
               "has no field 'missing'", "SM6 rejects unknown fields at load");
  expect_error(kSm6Head + "          - name: x\n            cbuffer_field: { handle: params, field: tint, component: 3 }\n" + kSm6Tail,
               "out of range", "SM6 rejects out-of-range components");
  expect_error(kSm6Head + "          - name: x\n            cbuffer_field: { handle: other, field: tint }\n" + kSm6Tail,
               "no fields declared", "SM6 rejects undeclared cbuffers");
}

// ─── SM5 ──────────────────────────────────────────────────────────────────────

// Replaces interleaved gradient noise with a cbuffer read. `strength` sits at row 1
// component x; `bias` (width 2) at row 1 components yz.
const std::string kSm5Head = R"YAML(
steps:
  - kind: add_resource
    name: add_params
    cbuffers:
      - handle: params
        meta: { description: Grading parameters }
        fields:
          - { name: scale, type: F32, width: 4 }
          - { name: strength, type: F32, default: 0.25 }
          - { name: bias, type: F32, width: 2 }
  - kind: apply_rule
    name: replace_ign
    rewrite_mode: replace_range
    rule:
      match:
        - opcode: dp2
          operands:
            - type: temp
              capture: ign_dst
            - capture: ign_coord
        - opcode: frc
        - opcode: mul
        - opcode: frc
      emit:
        - opcode: mov
          operands:
            - capture: ign_dst
)YAML";

void TestSm5FieldEmit(std::span<const uint8_t> shader) {
  const auto run = [&](const std::string& operand, const std::string& label) -> std::optional<dxp::RecipeReport> {
    auto parsed = dxp::sm5::Recipe::ParseFromText(kSm5Head + operand);
    Check(parsed.has_value(), label + " parses" + (parsed ? std::string() : ": " + parsed.error()));
    if (!parsed) return std::nullopt;
    auto result = parsed->Execute(shader);
    Check(result.has_value() && result->modified, label + " executes and modifies" + (result ? std::string() : ": " + result.error()));
    if (!result) return std::nullopt;
    return std::move(*result);
  };

  auto scalar_field = run("            - { type: constant_buffer, handle: { name: params, field: strength } }\n", "SM5 scalar field");
  auto scalar_manual = run(
      "            - { type: constant_buffer, handle: { name: params, element_index: 1 }, components: { selection_mode: select, value: x } }\n",
      "SM5 scalar manual");
  Check(scalar_field && scalar_manual && scalar_field->output_bytes == scalar_manual->output_bytes,
        "SM5 scalar field output is identical to element_index + select");

  auto vector_field = run("            - { type: constant_buffer, handle: { name: params, field: bias } }\n", "SM5 vector field");
  auto vector_manual = run(
      "            - { type: constant_buffer, handle: { name: params, element_index: 1 }, components: { selection_mode: swizzle, value: yzzz } }\n",
      "SM5 vector manual");
  Check(vector_field && vector_manual && vector_field->output_bytes == vector_manual->output_bytes,
        "SM5 vector field output is identical to element_index + swizzle");

  if (scalar_field) {
    const auto binding = scalar_field->new_bindings.find("params");
    Check(binding != scalar_field->new_bindings.end() && binding->second.type.cbuffer &&
              binding->second.type.cbuffer->size_in_bytes == 32 && MetaString(binding->second.meta, "description", "Grading parameters"),
          "SM5 new_bindings carries layout and meta");
  }

  auto parsed = dxp::sm5::Recipe::ParseFromText(kSm5Head);
  if (parsed) {
    const auto declarations = parsed->GetDeclaredResources();
    const auto* params = FindDeclaration(declarations, "params");
    Check(params && params->space == 0u && params->type.cbuffer && params->type.cbuffer->FindField("strength")->offset == 16 &&
              params->type.cbuffer->FindField("strength")->default_value == std::vector<double>{0.25},
          "SM5 declarations report the field layout");
  }

  const auto expect_error = [](const std::string& operand, const std::string& needle, const std::string& label) {
    auto result = dxp::sm5::Recipe::ParseFromText(kSm5Head + operand);
    Check(!result && result.error().find(needle) != std::string::npos, label + (result ? "" : " (" + result.error() + ")"));
  };
  expect_error("            - { type: constant_buffer, handle: { name: params, field: nope } }\n", "has no field 'nope'",
               "SM5 rejects unknown fields at load");
  expect_error("            - { type: constant_buffer, handle: { name: params, field: strength, element_index: 0 } }\n",
               "cannot be combined", "SM5 rejects field with element_index");
  expect_error("            - { type: resource, handle: { name: params, field: strength } }\n", "requires type constant_buffer",
               "SM5 rejects field on non-cbuffer operands");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: resource_declarations_test <sm5.cso> <sm6.cso>\n";
    return 1;
  }
  const auto sm5_shader = ReadFile(argv[1]);
  const auto sm6_shader = ReadFile(argv[2]);
  if (sm5_shader.empty() || sm6_shader.empty()) {
    std::cerr << "Failed to read input shaders\n";
    return 1;
  }

  TestSm6Declarations();
  TestLayoutErrors();
  TestSm6FieldEmit(sm6_shader);
  TestSm5FieldEmit(sm5_shader);

  std::cout << (failures == 0 ? "All checks passed\n" : std::to_string(failures) + " check(s) failed\n");
  return failures == 0 ? 0 : 1;
}
