// Extended-opcode emit-chain tests.
//
// Pins the emit-side extended_opcodes support:
//   - explicit entries are verbatim (chain bit 31 assigned by final position);
//   - members of the canonical ResourceDim + ResourceReturnType pair omitted
//     from the explicit chain are synthesized from the resource declaration
//     (or fixed metadata) in canonical order;
//   - an emit that cannot resolve the resource declaration is a hard error
//     (no silent bare emits);
//   - malformed entries fail at parse/compile time.
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

std::string MakeRecipe(const std::string& emit_block) {
  std::string yaml = "version: 1\n";
  yaml += "steps:\n";
  yaml += "  - kind: apply_rule\n";
  yaml += "    name: emit_chain\n";
  yaml += "    required: true\n";
  yaml += "    match_mode: match_all\n";
  yaml += "    rewrite_mode: replace\n";
  yaml += "    rule:\n";
  yaml += "      match:\n";
  yaml += "        - opcode: mov\n";
  yaml += "      emit:\n";
  yaml += emit_block;
  return yaml;
}

/// @brief Executes the recipe and returns the extended-token chain of the first
/// instruction with the given opcode that carries extended tokens (the emitted
/// opcode is unique in the output shaders used here).
bool EmitChain(const char* cso, Opcode target_opcode, const std::string& emit_block,
               std::vector<uint32_t>& out_tokens, std::string& error) {
  auto parse_result = dxp::sm5::Recipe::ParseFromText(MakeRecipe(emit_block), "inline-emit-chain");
  if (!parse_result) {
    error = parse_result.error();
    return false;
  }
  std::vector<uint8_t> bytes;
  if (!ReadFile((RepoRootPath() / cso).string(), bytes)) {
    error = "read failed";
    return false;
  }
  dxp::PatchOptions options;
  options.logger = [](dxp::LogLevel level, const std::string& message) {
    std::cerr << "[LOG " << static_cast<int>(level) << "] " << message << "\n";
  };
  options.log_level = dxp::LogLevel::Trace;
  auto result = parse_result.value().Execute(bytes, options);
  if (!result) {
    error = result.error();
    return false;
  }
  auto program = dxp::sm5::ShaderProgram::FromBytes(result->output_bytes);
  if (!program) {
    error = "reparse failed";
    return false;
  }
  for (const auto& instr : program->instructions) {
    if (instr.opcode != target_opcode || instr.controls.extended_op_codes.empty()) {
      continue;
    }
    for (const auto& token : instr.controls.extended_op_codes) {
      out_tokens.push_back(token.value);
    }
    return true;
  }
  error = "no emitted instruction with extended tokens found";
  return false;
}

bool ExpectChain(const char* cso, Opcode target_opcode, const std::string& emit_block,
                 const std::vector<uint32_t>& expected, const char* label) {
  std::vector<uint32_t> tokens;
  std::string error;
  if (!EmitChain(cso, target_opcode, emit_block, tokens, error)) {
    std::cerr << "  " << label << ": failed: " << error << "\n";
    return false;
  }
  if (tokens != expected) {
    std::cerr << "  " << label << ": chain mismatch\n    got:";
    for (const auto token : tokens) {
      std::cerr << " " << std::hex << token;
    }
    std::cerr << "\n    exp:";
    for (const auto token : expected) {
      std::cerr << " " << std::hex << token;
    }
    std::cerr << std::dec << "\n";
    return false;
  }
  return true;
}

constexpr uint32_t kChainBit = 0x80000000;
constexpr uint32_t kDimTexture2D = 3;
constexpr uint32_t kDimTexture3D = 5;
constexpr uint32_t kFloat4 = 0x5555;

uint32_t DimToken(uint32_t dim) {
  return static_cast<uint32_t>(dxp::sm5::model::ExtendedOpcodeType::ResourceDim) | (dim << 6) | kChainBit;
}
uint32_t RetToken(uint32_t packed) {
  uint32_t token = static_cast<uint32_t>(dxp::sm5::model::ExtendedOpcodeType::ResourceType);
  for (uint32_t component = 0; component < 4; ++component) {
    token |= ((packed >> (4 * component)) & 0xF) << (6 + 4 * component);
  }
  return token;
}

// An ld emitted with no explicit extended_opcodes gets the canonical pair
// synthesized from the declared resource (t0: texture2d float4).
bool TestImplicitSynthesis() {
  const std::string kEmit =
      "        - opcode: ld\n"
      "          operands:\n"
      "            - {type: temp, components: {selection_mode: mask, value: x}}\n"
      "            - {type: temp, components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: resource, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n";
  const std::vector<uint32_t> kExpected = {DimToken(4), RetToken(kFloat4)};  // texture2dms
  return ExpectChain("tests/shaders/sm5_chain_ld2dms.ps_5_0.cso", Opcode::Ld, kEmit, kExpected, "implicit-synthesis");
}

// Explicit sample_controls + synthesized pair: 3-token canonical chain with
// correct chaining bits.
bool TestControlsPlusSynthesis() {
  const std::string kEmit =
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - type: sample_controls\n"
      "              sample_controls: { u: 1, v: -2, w: 0 }\n"
      "          operands:\n"
      "            - {type: temp, components: {selection_mode: mask, value: xyzw}}\n"
      "            - {type: temp, components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: resource, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: sampler, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n";
  const uint32_t kControls = static_cast<uint32_t>(dxp::sm5::model::ExtendedOpcodeType::SampleControls)
                             | (1u << (6 + 3)) | (static_cast<uint32_t>(-2) & 0xF) << (6 + 7) | kChainBit;
  const std::vector<uint32_t> kExpected = {kControls, DimToken(kDimTexture2D), RetToken(kFloat4)};
  return ExpectChain("tests/shaders/sm5_chain_ld2d.ps_5_0.cso", Opcode::Sample, kEmit, kExpected,
                     "controls-plus-synthesis");
}

// Explicit full chain is verbatim: texture3d + uint return, no declaration
// lookup needed.
bool TestExplicitFullChain() {
  const std::string kEmit =
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - type: resource_dim\n"
      "              resource_dim: { dimension: 5 }\n"
      "            - type: resource_type\n"
      "              resource_return_type: [4, 4, 4, 4]\n"
      "          operands:\n"
      "            - {type: temp, components: {selection_mode: mask, value: xyzw}}\n"
      "            - {type: temp, components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: resource, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: sampler, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n";
  const std::vector<uint32_t> kExpected = {DimToken(kDimTexture3D), RetToken(0x4444)};
  return ExpectChain("tests/shaders/sm5_chain_ld2d.ps_5_0.cso", Opcode::Sample, kEmit, kExpected,
                     "explicit-full-chain");
}

// Raw tokens are verbatim (engine assigns the chaining bit).
bool TestRawChain() {
  const std::string kEmit =
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - raw: 2147483842\n"  // 0x800000C2 (ResourceDim texture2d)
      "            - raw: 1398083\n"     // 0x00155543 (ReturnType float4)
      "          operands:\n"
      "            - {type: temp, components: {selection_mode: mask, value: xyzw}}\n"
      "            - {type: temp, components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: resource, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: sampler, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n";
  const std::vector<uint32_t> kExpected = {DimToken(kDimTexture2D), RetToken(kFloat4)};
  return ExpectChain("tests/shaders/sm5_chain_ld2d.ps_5_0.cso", Opcode::Sample, kEmit, kExpected, "raw-chain");
}

// Partial: explicit dim, synthesized return (canonical order preserved).
bool TestPartialChain() {
  const std::string kEmit =
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - type: resource_dim\n"
      "              resource_dim: { dimension: 5 }\n"
      "          operands:\n"
      "            - {type: temp, components: {selection_mode: mask, value: xyzw}}\n"
      "            - {type: temp, components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: resource, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: sampler, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n";
  // dim is verbatim (5), return is synthesized from t0 (float4).
  const std::vector<uint32_t> kExpected = {DimToken(kDimTexture3D), RetToken(kFloat4)};
  return ExpectChain("tests/shaders/sm5_chain_ld2d.ps_5_0.cso", Opcode::Sample, kEmit, kExpected, "partial-chain");
}

// Ret-only explicit chain: the missing dim is INSERTED before it (canonical
// order), not appended after.
bool TestRetOnlyChain() {
  const std::string kEmit =
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - type: resource_type\n"
      "              resource_return_type: [4, 4, 4, 4]\n"
      "          operands:\n"
      "            - {type: temp, components: {selection_mode: mask, value: xyzw}}\n"
      "            - {type: temp, components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: resource, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: sampler, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n";
  // dim synthesized from t0 (texture2d) inserted BEFORE the explicit return.
  const std::vector<uint32_t> kExpected = {DimToken(kDimTexture2D), RetToken(0x4444)};
  return ExpectChain("tests/shaders/sm5_chain_ld2d.ps_5_0.cso", Opcode::Sample, kEmit, kExpected, "ret-only-chain");
}

// Controls + ret explicit: the missing dim is inserted between them.
bool TestControlsRetChain() {
  const std::string kEmit =
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - type: sample_controls\n"
      "              sample_controls: { u: 1, v: -2, w: 0 }\n"
      "            - type: resource_type\n"
      "              resource_return_type: [4, 4, 4, 4]\n"
      "          operands:\n"
      "            - {type: temp, components: {selection_mode: mask, value: xyzw}}\n"
      "            - {type: temp, components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: resource, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: sampler, indices: [{representation: immediate32, immediate_lo: 0}], components: {selection_mode: swizzle, value: xyzw}}\n";
  const uint32_t kControls = static_cast<uint32_t>(dxp::sm5::model::ExtendedOpcodeType::SampleControls)
                             | (1u << (6 + 3)) | (static_cast<uint32_t>(-2) & 0xF) << (6 + 7) | kChainBit;
  const std::vector<uint32_t> kExpected = {kControls, DimToken(kDimTexture2D), RetToken(0x4444)};
  return ExpectChain("tests/shaders/sm5_chain_ld2d.ps_5_0.cso", Opcode::Sample, kEmit, kExpected, "controls-ret-chain");
}

// An ld referencing an undeclared resource register is a hard error, not a
// silent bare emit.
bool TestUnresolvableDeclaration() {
  const std::string kEmit =
      "        - opcode: ld\n"
      "          operands:\n"
      "            - {type: temp, components: {selection_mode: mask, value: x}}\n"
      "            - {type: temp, components: {selection_mode: swizzle, value: xyzw}}\n"
      "            - {type: resource, indices: [{representation: immediate32, immediate_lo: 63}], components: {selection_mode: swizzle, value: xyzw}}\n";
  auto parse_result = dxp::sm5::Recipe::ParseFromText(MakeRecipe(kEmit), "inline-emit-chain");
  if (!parse_result) {
    std::cerr << "  unresolvable-declaration: recipe failed to parse\n";
    return false;
  }
  std::vector<uint8_t> bytes;
  if (!ReadFile((RepoRootPath() / "tests/shaders/sm5_chain_ld2dms.ps_5_0.cso").string(), bytes)) {
    return false;
  }
  auto result = parse_result.value().Execute(bytes);
  if (result) {
    std::cerr << "  unresolvable-declaration: emit succeeded without a declared resource\n";
    return false;
  }
  if (result.error().find("requires a declared resource") == std::string::npos) {
    std::cerr << "  unresolvable-declaration: unexpected error: " << result.error() << "\n";
    return false;
  }
  return true;
}

// Malformed emit entries must fail at parse/compile time.
bool TestMalformedEntries() {
  const char* kBad[] = {
      // type without payload
      "        - opcode: ld\n"
      "          extended_opcodes:\n"
      "            - type: resource_dim\n",
      // sample_controls offsets out of 4-bit range
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - type: sample_controls\n"
      "              sample_controls: { u: 8, v: 0, w: 0 }\n",
      // invalid dimension
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - type: resource_dim\n"
      "              resource_dim: { dimension: 13 }\n",
      // invalid return type component
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - type: resource_type\n"
      "              resource_return_type: [7, 7, 7, 7]\n",
      // type + raw conflict
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - {type: resource_dim, raw: 5}\n",
      // extended_opcodes on a non-resource opcode
      "        - opcode: mov\n"
      "          extended_opcodes:\n"
      "            - type: resource_dim\n"
      "              resource_dim: { dimension: 3 }\n",
      // sample_controls on an opcode that cannot carry them
      "        - opcode: ld\n"
      "          extended_opcodes:\n"
      "            - type: sample_controls\n"
      "              sample_controls: { u: 1, v: 0, w: 0 }\n",
      // duplicate chain member
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - type: resource_dim\n"
      "              resource_dim: { dimension: 3 }\n"
      "            - type: resource_dim\n"
      "              resource_dim: { dimension: 5 }\n",
      // out-of-order chain (return before dim)
      "        - opcode: sample\n"
      "          extended_opcodes:\n"
      "            - type: resource_type\n"
      "              resource_return_type: [4, 4, 4, 4]\n"
      "            - type: resource_dim\n"
      "              resource_dim: { dimension: 3 }\n",
  };
  bool ok = true;
  for (const auto& emit_block : kBad) {
    auto parse_result = dxp::sm5::Recipe::ParseFromText(MakeRecipe(emit_block), "inline-bad-emit");
    if (parse_result) {
      std::cerr << "  malformed emit entry was accepted:\n"
                << emit_block << "\n";
      ok = false;
    }
  }
  return ok;
}

}  // namespace

int main() {
  bool ok = true;
  ok &= TestImplicitSynthesis();
  ok &= TestControlsPlusSynthesis();
  ok &= TestExplicitFullChain();
  ok &= TestRawChain();
  ok &= TestPartialChain();
  ok &= TestRetOnlyChain();
  ok &= TestControlsRetChain();
  ok &= TestUnresolvableDeclaration();
  ok &= TestMalformedEntries();

  if (ok) {
    std::cout << "SM5 extended-opcode emit-chain tests passed.\n";
  } else {
    std::cerr << "SM5 extended-opcode emit-chain tests FAILED.\n";
  }
  std::cout.flush();
  return ok ? 0 : 1;
}
