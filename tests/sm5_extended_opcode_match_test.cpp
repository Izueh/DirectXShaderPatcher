// Extended-opcode match-side tests.
//
// Pins the match semantics of `extended_opcodes` on match entries:
//   - absent -> wildcard (matches any chain, including none);
//   - empty list -> the instruction must carry NO extended tokens;
//   - present -> exact full-chain match (count + per-entry rules), with
//     `any` position wildcards, exact `raw` tokens, and structured payload
//     expectations (sample_controls offsets, resource dimension, per-component
//     return types) compared against the decoded token.
// Also pins the validate-time errors for malformed entries.
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "src/dxp/sm5/ShaderProgram.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

using dxp::sm5::model::Opcode;

std::string MakeRecipe(const char* opcode, const std::string& match_extra) {
  std::string yaml = "version: 1\n";
  yaml += "steps:\n";
  yaml += "  - kind: apply_rule\n";
  yaml += "    name: match_target\n";
  yaml += "    required: false\n";
  yaml += "    match_mode: match_all\n";
  yaml += "    rewrite_mode: replace\n";
  yaml += "    rule:\n";
  yaml += "      match:\n";
  yaml += "        - opcode: ";
  yaml += opcode;
  yaml += "\n";
  yaml += match_extra;
  yaml += "      emit:\n";
  yaml += "        - opcode: mov\n";
  yaml += "          operands:\n";
  yaml += "            - {type: temp, components: {selection_mode: mask, value: x}}\n";
  yaml += "            - {type: immediate32, immediates_u32: [1]}\n";
  return yaml;
}

/// @brief Runs the recipe against the shader and returns the number of matched
/// instructions as the delta of emitted `mov` markers (output movs minus input
/// movs — the corpus shaders themselves may contain movs). Returns false on
/// recipe/parse/execute failure.
bool CountMatches(const char* cso, const char* opcode, const std::string& match_extra, size_t& out_markers,
                  std::string& error) {
  auto parse_result = dxp::sm5::Recipe::ParseFromText(MakeRecipe(opcode, match_extra), "inline-match-test");
  if (!parse_result) {
    error = parse_result.error();
    return false;
  }
  std::vector<uint8_t> bytes;
  if (!ReadFile((RepoRootPath() / cso).string(), bytes)) {
    error = "read failed";
    return false;
  }
  auto count_movs = [](std::span<const uint8_t> data) -> std::expected<size_t, std::string> {
    auto program = dxp::sm5::ShaderProgram::FromBytes(data);
    if (!program) {
      return std::unexpected(program.error());
    }
    size_t markers = 0;
    for (const auto& instr : program->instructions) {
      if (instr.opcode == Opcode::Mov) {
        ++markers;
      }
    }
    return markers;
  };
  const auto input_movs = count_movs(bytes);
  if (!input_movs) {
    error = "input parse failed: " + input_movs.error();
    return false;
  }
  auto result = parse_result.value().Execute(bytes);
  if (!result) {
    error = result.error();
    return false;
  }
  const auto output_movs = count_movs(result->output_bytes);
  if (!output_movs) {
    error = "output parse failed: " + output_movs.error();
    return false;
  }
  out_markers = *output_movs - *input_movs;
  return true;
}

bool ExpectCount(const char* cso, const char* opcode, const std::string& match_extra, size_t expected,
                 const char* label) {
  size_t markers = 0;
  std::string error;
  if (!CountMatches(cso, opcode, match_extra, markers, error)) {
    std::cerr << "  " << label << ": recipe/execute failed: " << error << "\n";
    return false;
  }
  if (markers != expected) {
    std::cerr << "  " << label << ": expected " << expected << " match(es), got " << markers << "\n";
    return false;
  }
  return true;
}

// Absent extended_opcodes -> wildcard: matches with AND without extended tokens.
bool TestWildcardDefault() {
  const std::string kNone;
  bool ok = true;
  ok &= ExpectCount("tests/shaders/sm5_chain_sample.ps_5_0.cso", "sample", kNone, 1, "wildcard-plain-sample");
  ok &= ExpectCount("tests/shaders/sm5_chain_sample_aoff.ps_5_0.cso", "sample", kNone, 1, "wildcard-offset-sample");
  return ok;
}

// Type-only expectations and structured payload expectations. Specified chains
// are EXACT: the whole instruction chain must match (count + per-entry rules).
bool TestTypeAndStructured() {
  bool ok = true;
  // Full-chain type-only: sample_controls + resource_dim + resource_type.
  const std::string kControlsChain =
      "          extended_opcodes:\n"
      "            - type: sample_controls\n"
      "            - type: resource_dim\n"
      "            - type: resource_type\n";
  ok &= ExpectCount("tests/shaders/sm5_chain_sample_aoff.ps_5_0.cso", "sample", kControlsChain, 1,
                    "type-only-controls-aoff");
  ok &= ExpectCount("tests/shaders/sm5_chain_sample.ps_5_0.cso", "sample", kControlsChain, 0,
                    "type-only-controls-plain");

  const std::string kControlsExact =
      "          extended_opcodes:\n"
      "            - type: sample_controls\n"
      "              sample_controls: { u: 1, v: -2, w: 0 }\n"
      "            - type: resource_dim\n"
      "            - type: resource_type\n";
  ok &= ExpectCount("tests/shaders/sm5_chain_sample_aoff.ps_5_0.cso", "sample", kControlsExact, 1,
                    "controls-u1v-2-aoff");
  const std::string kControlsZero =
      "          extended_opcodes:\n"
      "            - type: sample_controls\n"
      "              sample_controls: { u: 0, v: 0, w: 0 }\n"
      "            - type: resource_dim\n"
      "            - type: resource_type\n";
  ok &= ExpectCount("tests/shaders/sm5_chain_sample_aoff.ps_5_0.cso", "sample", kControlsZero, 0,
                    "controls-zero-aoff");

  const std::string kDim3 =
      "          extended_opcodes:\n"
      "            - type: resource_dim\n"
      "              resource_dim: { dimension: 3 }\n"
      "            - type: resource_type\n";
  ok &= ExpectCount("tests/shaders/sm5_chain_sample.ps_5_0.cso", "sample", kDim3, 1, "dim3-sample");
  ok &= ExpectCount("tests/shaders/sm5_chain_sample_3d.ps_5_0.cso", "sample", kDim3, 0, "dim3-sample3d");
  const std::string kDim5 =
      "          extended_opcodes:\n"
      "            - type: resource_dim\n"
      "              resource_dim: { dimension: 5 }\n"
      "            - type: resource_type\n";
  ok &= ExpectCount("tests/shaders/sm5_chain_sample_3d.ps_5_0.cso", "sample", kDim5, 1, "dim5-sample3d");

  const std::string kRetFloat =
      "          extended_opcodes:\n"
      "            - type: resource_dim\n"
      "            - type: resource_type\n"
      "              resource_return_type: [5, 5, 5, 5]\n";
  ok &= ExpectCount("tests/shaders/sm5_chain_ld2d.ps_5_0.cso", "ld", kRetFloat, 1, "ret-float-ld");
  ok &= ExpectCount("tests/shaders/sm5_chain_ld_uint.ps_5_0.cso", "ld", kRetFloat, 0, "ret-float-lduint");
  const std::string kRetUint =
      "          extended_opcodes:\n"
      "            - type: resource_dim\n"
      "            - type: resource_type\n"
      "              resource_return_type: [4, 4, 4, 4]\n";
  ok &= ExpectCount("tests/shaders/sm5_chain_ld_uint.ps_5_0.cso", "ld", kRetUint, 1, "ret-uint-lduint");
  return ok;
}

// Raw exact-token matching and `any` position wildcards.
bool TestRawAndAny() {
  bool ok = true;
  // sample.cso chain: 0x800000C2 (ResourceDim) + 0x00155543 (ReturnType float4).
  const std::string kRaw =
      "          extended_opcodes:\n"
      "            - raw: 2147483842\n"
      "            - raw: 1398083\n";
  ok &= ExpectCount("tests/shaders/sm5_chain_sample.ps_5_0.cso", "sample", kRaw, 1, "raw-exact");
  const std::string kRawWrong =
      "          extended_opcodes:\n"
      "            - raw: 2147483843\n"
      "            - raw: 1398083\n";
  ok &= ExpectCount("tests/shaders/sm5_chain_sample.ps_5_0.cso", "sample", kRawWrong, 0, "raw-wrong");

  const std::string kAny2 =
      "          extended_opcodes:\n"
      "            - any: true\n"
      "            - any: true\n";
  ok &= ExpectCount("tests/shaders/sm5_chain_sample.ps_5_0.cso", "sample", kAny2, 1, "any2-plain");
  ok &= ExpectCount("tests/shaders/sm5_chain_sample_aoff.ps_5_0.cso", "sample", kAny2, 0, "any2-aoff-chain3");

  // An explicit empty list requires the instruction to carry NO extended tokens.
  const std::string kEmpty = "          extended_opcodes: []\n";
  ok &= ExpectCount("tests/shaders/sm5_chain_sample.ps_5_0.cso", "sample", kEmpty, 0, "empty-list-sample");
  ok &= ExpectCount("tests/shaders/sm5_chain_store_raw.cs_5_0.cso", "store_raw", kEmpty, 1, "empty-list-store");
  return ok;
}

// Malformed entries must fail at parse/compile time.
bool TestMalformedEntries() {
  const char* kBad[] = {
      "          extended_opcodes:\n"
      "            - {type: sample_controls, raw: 1}\n",
      "          extended_opcodes:\n"
      "            - {any: true, type: sample_controls}\n",
      "          extended_opcodes:\n"
      "            - {type: resource_dim, sample_controls: {u: 1, v: 0, w: 0}}\n",
      "          extended_opcodes:\n"
      "            - {type: sample_controls, resource_return_type: [5, 5, 5, 5]}\n",
  };
  bool ok = true;
  for (const auto& extra : kBad) {
    auto parse_result = dxp::sm5::Recipe::ParseFromText(MakeRecipe("sample", extra), "inline-bad-ext");
    if (parse_result) {
      std::cerr << "  malformed extended_opcodes entry was accepted:\n"
                << extra << "\n";
      ok = false;
    }
  }
  return ok;
}

}  // namespace

int main() {
  bool ok = true;
  ok &= TestWildcardDefault();
  ok &= TestTypeAndStructured();
  ok &= TestRawAndAny();
  ok &= TestMalformedEntries();

  if (ok) {
    std::cout << "SM5 extended-opcode match tests passed.\n";
  } else {
    std::cerr << "SM5 extended-opcode match tests FAILED.\n";
  }
  std::cout.flush();
  return ok ? 0 : 1;
}
