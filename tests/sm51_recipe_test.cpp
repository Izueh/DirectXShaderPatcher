// Recipes and SM5.1: resources in another register space, the `upgrade_to_sm51` option, env variable spaces and
// recipes chained on each other's SM5.1 output.
#include <d3dcompiler.h>
#include <windows.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/sm5/ShaderProgram.hpp"

namespace {

int failures = 0;

void Check(bool ok, const std::string& what) {
  if (!ok) {
    std::cerr << "FAIL: " << what << "\n";
    ++failures;
  }
}

std::vector<uint8_t> ReadAll(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::string Disassemble(const std::vector<uint8_t>& bytes) {
  Microsoft::WRL::ComPtr<ID3DBlob> blob;
  if (FAILED(D3DDisassemble(bytes.data(), bytes.size(), D3D_DISASM_DISABLE_DEBUG_INFO, nullptr, &blob)) || !blob) return {};
  return std::string(static_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize());
}

bool Contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

size_t Count(const std::string& text, const std::string& part) {
  size_t count = 0;
  for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size())) ++count;
  return count;
}

// Minor version of the shader chunk
uint32_t ShaderMinor(const std::vector<uint8_t>& bytes) {
  uint32_t count = 0;
  std::memcpy(&count, &bytes[28], 4);
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t offset = 0;
    std::memcpy(&offset, &bytes[32 + (i * 4)], 4);
    if (std::memcmp(&bytes[offset], "SHEX", 4) == 0 || std::memcmp(&bytes[offset], "SHDR", 4) == 0) {
      uint32_t version = 0;
      std::memcpy(&version, &bytes[offset + 8], 4);
      return version & 0xF;
    }
  }
  return 99;
}

dxp::sm5::Recipe Parse(const std::string& yaml) {
  auto recipe = dxp::sm5::Recipe::ParseFromText(yaml, "inline");
  if (!recipe) {
    std::cerr << "recipe didn't parse: " << recipe.error() << "\n";
    std::exit(1);
  }
  return std::move(*recipe);
}

// A recipe that only declares resources passes the shader through, so those recipes add this rule to change something
constexpr const char* kNopRule = R"YAML(
  - kind: apply_rule
    name: touch
    rewrite_mode: before_last_return
    rule:
      emit:
        - opcode: nop
)YAML";

// An add_resource step for a cbuffer plus a rule that reads it, before the shader returns
std::string CBufferRecipe(const std::string& header, const std::string& cbuffer_fields) {
  return "version: 1\n" + header + R"YAML(steps:
  - kind: add_resource
    name: add_luma
    temps: [scratch]
    cbuffers:
      - handle: luma
        elements: 1
)YAML" + cbuffer_fields + R"YAML(
  - kind: apply_rule
    name: read_luma
    rule:
      match:
        - opcode: add
          operands:
            - type: output
              capture: dst
            - type: temp
              capture: a
            - type: constant_buffer
              capture: c
      emit:
        - opcode: add
          operands:
            - type: temp
              handle: { name: scratch }
              components: {selection_mode: mask, value: xyzw}
            - capture: a
            - capture: c
        - opcode: add
          operands:
            - capture: dst
            - type: temp
              handle: { name: scratch }
              components: {selection_mode: swizzle, value: xyzw}
            - type: constant_buffer
              handle: { name: luma, element_index: 0 }
              components: {selection_mode: swizzle, value: xyzw}
)YAML";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: sm51_recipe_test <sm51_probe_game.ps_5_0.cso> <sm51_probe.ps_5_0.cso>\n";
    return 1;
  }
  const std::vector<uint8_t> game = ReadAll(argv[1]);
  const std::vector<uint8_t> probe = ReadAll(argv[2]);
  if (game.empty() || probe.empty()) {
    std::cerr << "fixtures not found\n";
    return 1;
  }

  // 1. A resource in space 100 upgrades the output by itself
  {
    const auto recipe = Parse(CBufferRecipe("", "        register_index: 13\n        space: 100\n"));
    const auto report = recipe.Execute(game);
    Check(report.has_value(), "space 100 cbuffer: " + (report ? std::string() : report.error()));
    if (report) {
      Check(report->modified, "space 100 cbuffer modifies the shader");
      Check(report->output_container.shader_model == "5.1", "auto upgrade reports 5.1");
      Check(ShaderMinor(report->output_bytes) == 1, "auto upgrade writes the 5.1 version token");
      const auto binding = report->new_bindings.find("luma");
      Check(binding != report->new_bindings.end() && binding->second.register_index == 13 && binding->second.space == 100,
            "binding is reported at b13 space 100");
      const std::string text = Disassemble(report->output_bytes);
      Check(!text.empty(), "5.1 output disassembles");
      Check(Contains(text, "CB1[13:13][1], immediateIndexed, space=100"), "the new cbuffer is declared in space 100");
      Check(Contains(text, "CB0[0:0][1], immediateIndexed, space=0"), "the game's cbuffer stays in space 0");
      Check(Contains(text, "CB1[13][0]"), "the instruction reads the new cbuffer through its range");
      Check(Contains(text, "T0[0:0], space=0") && Contains(text, "S0[0:0], mode_default, space=0"), "the game's texture and sampler stay in space 0");
      auto program = dxp::sm5::ShaderProgram::FromBytes(report->output_bytes);
      Check(program.has_value() && program->input_sm51 && program->placements.size() == 1, "the output reads back as 5.1 with one placement");
    }
  }

  // 2. Space 0 stays 5.0 by default
  {
    const auto recipe = Parse(CBufferRecipe("", "        register_index: 12\n"));
    const auto report = recipe.Execute(game);
    Check(report.has_value(), "space 0 cbuffer");
    if (report) {
      Check(report->output_container.shader_model == "5.0" && ShaderMinor(report->output_bytes) == 0, "space 0 stays SM5.0");
      Check(report->new_bindings.at("luma").space == 0, "space 0 binding");
    }
  }

  // 3. upgrade_to_sm51: always upgrades even then; a shader the recipe leaves alone is not touched
  {
    const auto recipe = Parse(CBufferRecipe("upgrade_to_sm51: always\n", "        register_index: 12\n"));
    const auto report = recipe.Execute(game);
    Check(report.has_value() && report->output_container.shader_model == "5.1", "always upgrades a space 0 patch");
    if (report) {
      const std::string text = Disassemble(report->output_bytes);
      Check(Contains(text, "CB1[12:12][1], immediateIndexed, space=0"), "always: the cbuffer is declared in space 0");
      Check(!Contains(text, "space=100"), "always: nothing moves to another space");
    }
    const auto untouched = Parse("version: 1\nupgrade_to_sm51: always\nsteps:\n  - kind: check_shader_version\n    name: v\n    major: 5\n    minor: 0\n").Execute(game);
    Check(untouched.has_value() && !untouched->modified && untouched->output_bytes == game, "a recipe that changes nothing leaves the bytes alone");
  }

  // 4. upgrade_to_sm51: never refuses a space
  {
    const auto recipe = Parse(CBufferRecipe("upgrade_to_sm51: never\n", "        register_index: 13\n        space: 100\n"));
    const auto report = recipe.Execute(game);
    Check(!report.has_value() && Contains(report.error(), "upgrade_to_sm51: never"), "never refuses space 100");
    const auto fine = Parse(CBufferRecipe("upgrade_to_sm51: never\n", "        register_index: 13\n")).Execute(game);
    Check(fine.has_value() && fine->output_container.shader_model == "5.0", "never is fine without a space");
  }

  // 5. The space from an env variable
  {
    const auto recipe = Parse(CBufferRecipe("", "        register_index: 13\n        space: LUMA_SPACE\n"));
    dxp::PatchOptions options;
    options.SetEnv("LUMA_SPACE", uint32_t{100});
    const auto report = recipe.Execute(game, options);
    Check(report.has_value() && report->new_bindings.at("luma").space == 100, "space from an env variable");
    const auto missing = recipe.Execute(game);
    Check(!missing.has_value() && Contains(missing.error(), "LUMA_SPACE"), "a missing space variable names itself");
    dxp::PatchOptions bad;
    bad.SetEnv("LUMA_SPACE", true);
    const auto wrong = recipe.Execute(game, bad);
    Check(!wrong.has_value() && Contains(wrong.error(), "register space"), "a non-number space variable is refused");
    const auto declared = recipe.GetDeclaredResources();
    Check(declared.size() == 1 && !declared[0].space.has_value(), "an env space isn't known before execution");
    const auto literal = Parse(CBufferRecipe("", "        space: 100\n")).GetDeclaredResources();
    Check(literal.size() == 1 && literal[0].space == 100u, "a literal space is declared");
  }

  // 6. Recipes chained on the 5.1 output
  {
    const auto first = Parse(CBufferRecipe("", "        register_index: 13\n        space: 100\n")).Execute(game);
    Check(first.has_value(), "chain: first recipe");
    if (first) {
      const auto second = Parse(R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_more
    textures:
      - handle: noise
        register_index: 5
        space: 100
        dimension: texture2d
    samplers:
      - handle: linear
        space: 100
)YAML" + std::string(kNopRule)).Execute(first->output_bytes);
      Check(second.has_value(), "chain: second recipe on 5.1 input: " + (second ? std::string() : second.error()));
      if (second) {
        Check(second->output_container.shader_model == "5.1", "chain keeps 5.1");
        Check(second->new_bindings.at("noise").space == 100 && second->new_bindings.at("noise").register_index == 5, "chain: texture placed");
        Check(second->new_bindings.at("linear").space == 100 && second->new_bindings.at("linear").register_index == 0, "chain: sampler takes the first free register of the space");
        const std::string text = Disassemble(second->output_bytes);
        Check(Contains(text, "CB1[13:13][1], immediateIndexed, space=100"), "chain: the first recipe's cbuffer survives");
        Check(Contains(text, "T1[5:5], space=100") && Contains(text, "S1[0:0], mode_default, space=100"), "chain: the second recipe's resources are declared");
        Check(Contains(text, "CB1[13][0]"), "chain: the first recipe's read survives");
      }
      const auto clash = Parse(CBufferRecipe("", "        register_index: 13\n        space: 100\n")).Execute(first->output_bytes);
      Check(!clash.has_value() && Contains(clash.error(), "space 100"), "chain: b13 space 100 is taken");
      const auto other_space = Parse(CBufferRecipe("", "        register_index: 13\n        space: 101\n")).Execute(first->output_bytes);
      Check(other_space.has_value(), "chain: b13 in another space is free");
    }
  }

  // 7. The same register in two spaces, auto-binding inside a space
  {
    const auto recipe = Parse(R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_many
    cbuffers:
      - handle: a
        register_index: 0
        space: 100
      - handle: b
        space: 100
      - handle: c
        space: 100
        reverse_bind: true
    textures:
      - handle: t_in_100
        space: 100
        dimension: texture2d
)YAML" + std::string(kNopRule));
    const auto report = recipe.Execute(game);
    Check(report.has_value(), "two spaces: " + (report ? std::string() : report.error()));
    if (report) {
      Check(report->new_bindings.at("a").register_index == 0 && report->new_bindings.at("a").space == 100, "b0 space 100 next to the game's b0");
      Check(report->new_bindings.at("b").register_index == 1 && report->new_bindings.at("b").space == 100, "auto takes the first free register of the space");
      Check(report->new_bindings.at("c").register_index == 14 && report->new_bindings.at("c").space == 100, "reverse_bind takes the highest free register of the space");
      Check(report->new_bindings.at("t_in_100").register_index == 0, "t0 space 100 next to the game's t0");
      const std::string text = Disassemble(report->output_bytes);
      Check(Count(text, "space=100") == 4 && Count(text, "space=0") == 3, "four resources in space 100 beside the game's three");
    }
    const auto in_space_zero = Parse("version: 1\nsteps:\n  - kind: add_resource\n    name: clash\n    cbuffers:\n      - handle: a\n        register_index: 0\n").Execute(game);
    Check(!in_space_zero.has_value(), "b0 in space 0 is still the game's");
  }

  // 8. A typed UAV in another space keeps its dimension and return type
  {
    const auto recipe = Parse(R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_uav
    uavs:
      - handle: out
        space: 100
        dimension: texture2d
    raw_resources:
      - handle: raw
        space: 100
    structured_resources:
      - handle: structured
        space: 100
        stride: 16
)YAML" + std::string(kNopRule));
    const auto report = recipe.Execute(game);
    Check(report.has_value(), "uav: " + (report ? std::string() : report.error()));
    if (report) {
      const std::string text = Disassemble(report->output_bytes);
      Check(Contains(text, "dcl_uav_typed_texture2d (float,float,float,float) U0[0:0], space=100"), "the typed UAV is declared with its dimension in space 100");
      Check(Contains(text, "dcl_resource_raw T1[1:1], space=100") || Contains(text, "dcl_resource_raw T1[0:0], space=100"), "the raw resource is declared in space 100: " + text);
      Check(Contains(text, "dcl_resource_structured") && Contains(text, ", 16, space=100"), "the structured resource is declared in space 100");
    }
  }

  // 9. A 5.0 recipe's own bytes: nothing changes for a recipe without a space (guards the 0.10 output)
  {
    const auto first = Parse(CBufferRecipe("", "        register_index: 12\n")).Execute(probe);
    const auto second = Parse(CBufferRecipe("", "        register_index: 12\n")).Execute(probe);
    Check(first.has_value() && second.has_value() && first->output_bytes == second->output_bytes, "deterministic output");
    Check(first.has_value() && ShaderMinor(first->output_bytes) == 0, "0.10 behavior: SM5.0 stays SM5.0");
  }

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "SM5.1 recipes ok\n";
  return 0;
}
