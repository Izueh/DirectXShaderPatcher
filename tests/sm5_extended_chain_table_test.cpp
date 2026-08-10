// Extended-opcode chain table compliance test.
//
// The corpus under tests/shaders/sm5_chain_*.cso is compiled with fxc
// (D3DCompiler, Windows SDK) from HLSL exercising one resource-access opcode
// each. This test asserts that (a) RequiredExtendedChainForOpcode() encodes
// the empirically-verified chain for every opcode, and (b) every corpus
// shader's actual extended tokens — parsed with dxp's own parser — match the
// table (presence, order, chaining bits, dimension, return types, and
// sample-control offsets). This is the regression gate behind the
// Flugan-80004005 bare-`ld` bug class: any opcode whose canonical chain the
// table gets wrong (or that stops being synthesized) will fail here.
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <vector>

#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/sm5/Model.hpp"
#include "src/dxp/sm5/ShaderProgram.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

using dxp::sm5::model::ExtendedChainKind;
using dxp::sm5::model::ExtendedOpcodeType;
using dxp::sm5::model::Opcode;
using dxp::sm5::model::ParseExtendedOpcodeToken;
using dxp::sm5::model::RequiredExtendedChainForOpcode;
using dxp::sm5::model::ResourceDimPayload;
using dxp::sm5::model::ResourceReturnTypePayload;
using dxp::sm5::model::SampleControlsPayload;

constexpr uint32_t kTypeMask = 0x3F;
constexpr uint32_t kChainBit = 0x80000000;

uint32_t TokenType(uint32_t token) {
  return token & kTypeMask;
}

uint32_t TokenDimension(uint32_t token) {
  return static_cast<uint32_t>(DECODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION(token));
}

uint32_t TokenStride(uint32_t token) {
  return static_cast<uint32_t>(DECODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION_STRUCTURE_STRIDE(token));
}

uint32_t TokenReturnType(uint32_t token, uint32_t component) {
  return static_cast<uint32_t>(DECODE_D3D11_SB_EXTENDED_RESOURCE_RETURN_TYPE(token, component));
}

/// @brief 4-bit 2's-complement sample-control offset (payload bit `shift`).
int32_t SampleControlOffset(uint32_t token, uint32_t shift) {
  const uint32_t kRaw = ((token >> 6) >> shift) & 0xF;
  return static_cast<int32_t>(kRaw | -(kRaw & 0x8));
}

struct ChainCase {
  const char* shader;                                // repo-relative path
  Opcode opcode;                                     // opcode to inspect in the shader
  bool expect_controls;                              // sample_controls expected first
  int32_t expect_u = 0, expect_v = 0, expect_w = 0;  // offsets (controls case)
  uint32_t expect_dim = 0;                           // resource dimension in the dim token
  uint32_t expect_stride = 0;                        // structured-buffer stride (dim token)
  uint32_t expect_return = 0;                        // packed 4x4 return types
  bool expect_none = false;                          // expect NO extended tokens at all
};

constexpr ChainCase kCases[] = {
    {"tests/shaders/sm5_chain_ld2d.ps_5_0.cso", Opcode::Ld, false, 0, 0, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_ld_uint.ps_5_0.cso", Opcode::Ld, false, 0, 0, 0, 3, 0, 0x4444},
    {"tests/shaders/sm5_chain_ld2dms.ps_5_0.cso", Opcode::LdMs, false, 0, 0, 0, 4, 0, 0x5555},
    {"tests/shaders/sm5_chain_ld_raw.ps_5_0.cso", Opcode::LdRaw, false, 0, 0, 0, 11, 0, 0x6666},
    {"tests/shaders/sm5_chain_ld_structured.ps_5_0.cso", Opcode::LdStructured, false, 0, 0, 0, 12, 16, 0x6666},
    {"tests/shaders/sm5_chain_resinfo.ps_5_0.cso", Opcode::Resinfo, false, 0, 0, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_sample.ps_5_0.cso", Opcode::Sample, false, 0, 0, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_sample_3d.ps_5_0.cso", Opcode::Sample, false, 0, 0, 0, 5, 0, 0x5555},
    {"tests/shaders/sm5_chain_sample_aoff.ps_5_0.cso", Opcode::Sample, true, 1, -2, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_sample_b.ps_5_0.cso", Opcode::SampleB, false, 0, 0, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_sample_c.ps_5_0.cso", Opcode::SampleC, false, 0, 0, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_sample_c_lz.ps_5_0.cso", Opcode::SampleCLz, false, 0, 0, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_sample_d.ps_5_0.cso", Opcode::SampleD, false, 0, 0, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_sample_l.ps_5_0.cso", Opcode::SampleL, false, 0, 0, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_gather4.ps_5_0.cso", Opcode::Gather4, false, 0, 0, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_gather4_c.ps_5_0.cso", Opcode::Gather4C, false, 0, 0, 0, 3, 0, 0x5555},
    // fxc lowers Gather-with-offset to GATHER4 + sample_controls and
    // GatherCmp-with-offset to GATHER4_C + sample_controls (SM5 has no
    // gather4_po emission): the offsets ride on the controls token.
    {"tests/shaders/sm5_chain_gather4_po.ps_5_0.cso", Opcode::Gather4, true, 1, 0, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_gather4_po_c.ps_5_0.cso", Opcode::Gather4C, true, 0, -1, 0, 3, 0, 0x5555},
    {"tests/shaders/sm5_chain_store_raw.cs_5_0.cso", Opcode::StoreRaw, false, 0, 0, 0, 0, 0, 0, true},
    {"tests/shaders/sm5_chain_store_structured.cs_5_0.cso", Opcode::StoreStructured, false, 0, 0, 0, 0, 0, 0, true},
};

bool RunCase(const ChainCase& c) {
  std::vector<uint8_t> bytes;
  if (!ReadFile((RepoRootPath() / c.shader).string(), bytes)) {
    std::cerr << "  failed to read shader: " << c.shader << "\n";
    return false;
  }
  auto program = dxp::sm5::ShaderProgram::FromBytes(bytes);
  if (!program) {
    std::cerr << "  " << c.shader << " failed to parse: " << program.error() << "\n";
    return false;
  }

  bool found = false;
  for (const auto& instr : program->instructions) {
    if (instr.opcode != c.opcode) {
      continue;
    }
    found = true;
    const auto& ext = instr.controls.extended_op_codes;
    if (c.expect_none) {
      if (!ext.empty()) {
        std::cerr << "  " << c.shader << ": opcode " << static_cast<uint32_t>(c.opcode)
                  << " unexpectedly carries " << ext.size() << " extended token(s)\n";
        return false;
      }
      continue;
    }

    size_t index = 0;
    if (c.expect_controls) {
      if (ext.size() != 3) {
        std::cerr << "  " << c.shader << ": expected 3 extended tokens, got " << ext.size() << "\n";
        return false;
      }
      const uint32_t kControls = ext[0].value;
      if (TokenType(kControls) != 1 || (kControls & kChainBit) == 0
          || SampleControlOffset(kControls, 3) != c.expect_u
          || SampleControlOffset(kControls, 7) != c.expect_v
          || SampleControlOffset(kControls, 11) != c.expect_w) {
        std::cerr << "  " << c.shader << ": sample_controls token mismatch (0x" << std::hex << kControls
                  << std::dec << ", u=" << SampleControlOffset(kControls, 3) << " v="
                  << SampleControlOffset(kControls, 7) << " w=" << SampleControlOffset(kControls, 11) << ")\n";
        return false;
      }
      index = 1;
    } else {
      if (ext.size() != 2) {
        std::cerr << "  " << c.shader << ": expected 2 extended tokens, got " << ext.size() << "\n";
        return false;
      }
    }

    const uint32_t kDim = ext[index].value;
    const uint32_t kReturn = ext[index + 1].value;
    if (TokenType(kDim) != 2 || (kDim & kChainBit) == 0 || TokenDimension(kDim) != c.expect_dim
        || TokenStride(kDim) != c.expect_stride) {
      std::cerr << "  " << c.shader << ": resource_dim token mismatch (0x" << std::hex << kDim << std::dec
                << ", dim=" << TokenDimension(kDim) << " stride=" << TokenStride(kDim) << ")\n";
      return false;
    }
    if (TokenType(kReturn) != 3 || (kReturn & kChainBit) != 0) {
      std::cerr << "  " << c.shader << ": resource_return_type token mismatch (0x" << std::hex << kReturn
                << std::dec << ")\n";
      return false;
    }
    for (uint32_t component = 0; component < 4; ++component) {
      const uint32_t kExpected = (c.expect_return >> (component * 4)) & 0xF;
      if (TokenReturnType(kReturn, component) != kExpected) {
        std::cerr << "  " << c.shader << ": return type component " << component << " is "
                  << TokenReturnType(kReturn, component) << ", expected " << kExpected << "\n";
        return false;
      }
    }
  }
  if (!found) {
    std::cerr << "  " << c.shader << ": target opcode " << static_cast<uint32_t>(c.opcode) << " not found\n";
    return false;
  }
  return true;
}

bool TestTable() {
  using K = ExtendedChainKind;
  const auto spec = [](Opcode opcode) { return RequiredExtendedChainForOpcode(opcode); };

  if (spec(Opcode::Ld).kind != K::ResourcePair || spec(Opcode::LdMs).kind != K::ResourcePair
      || spec(Opcode::Resinfo).kind != K::ResourcePair) {
    std::cerr << "  ld/ld2dms/resinfo must be ResourcePair\n";
    return false;
  }
  if (spec(Opcode::LdRaw).kind != K::ResourcePairFixed || spec(Opcode::LdRaw).fixed_dimension != 11
      || spec(Opcode::LdRaw).fixed_return_type != 0x6666) {
    std::cerr << "  ld_raw must be ResourcePairFixed (RAW_BUFFER, MIXED)\n";
    return false;
  }
  if (spec(Opcode::LdStructured).kind != K::ResourcePairFixed || spec(Opcode::LdStructured).fixed_dimension != 12
      || spec(Opcode::LdStructured).fixed_return_type != 0x6666) {
    std::cerr << "  ld_structured must be ResourcePairFixed (STRUCTURED_BUFFER, MIXED)\n";
    return false;
  }
  for (const Opcode opcode : {Opcode::Sample, Opcode::SampleB, Opcode::SampleC, Opcode::SampleCLz, Opcode::SampleD,
                              Opcode::SampleL, Opcode::Gather4, Opcode::Gather4C, Opcode::Gather4PO,
                              Opcode::Gather4POC}) {
    if (spec(opcode).kind != K::ResourcePairControls) {
      std::cerr << "  sample/gather4 family opcode " << static_cast<uint32_t>(opcode)
                << " must be ResourcePairControls\n";
      return false;
    }
  }
  if (spec(Opcode::Mov).kind != K::None || spec(Opcode::StoreRaw).kind != K::None
      || spec(Opcode::StoreStructured).kind != K::None) {
    std::cerr << "  mov/store_raw/store_structured must be None\n";
    return false;
  }
  return true;
}

/// @brief Pins the structured decode: known tokens decode to typed payloads,
/// unknown/reserved types fall back to the raw token (the escape hatch).
bool TestDecode() {
  const auto dim_token = ParseExtendedOpcodeToken(0x800000C2);  // ResourceDim, texture2d, chained
  if (dim_token.type != ExtendedOpcodeType::ResourceDim || !dim_token.chained) return false;
  const auto* dim = std::get_if<ResourceDimPayload>(&dim_token.payload);
  if (dim == nullptr || dim->dimension != 3 || dim->structure_stride != 0) return false;

  const auto stride_token = ParseExtendedOpcodeToken(0x80008302);  // ResourceDim, structured buffer, stride 16
  const auto* stride_dim = std::get_if<ResourceDimPayload>(&stride_token.payload);
  if (stride_dim == nullptr || stride_dim->dimension != 12 || stride_dim->structure_stride != 16) return false;

  const auto ret_token = ParseExtendedOpcodeToken(0x00155543);  // ResourceReturnType, float x4, not chained
  if (ret_token.type != ExtendedOpcodeType::ResourceType || ret_token.chained) return false;
  const auto* ret = std::get_if<ResourceReturnTypePayload>(&ret_token.payload);
  if (ret == nullptr || ret->component_types != std::array<uint32_t, 4>{5, 5, 5, 5}) return false;

  const auto controls_token = ParseExtendedOpcodeToken(0x8001C201);  // SampleControls, u=1 v=-2 w=0, chained
  if (controls_token.type != ExtendedOpcodeType::SampleControls || !controls_token.chained) return false;
  const auto* controls = std::get_if<SampleControlsPayload>(&controls_token.payload);
  if (controls == nullptr || controls->u != 1 || controls->v != -2 || controls->w != 0) return false;

  const auto raw_token = ParseExtendedOpcodeToken(0x80000100U | 0x3FU);  // unknown type 63: raw fallback
  const auto* raw = std::get_if<uint32_t>(&raw_token.payload);
  if (raw == nullptr || *raw != (0x80000100U | 0x3FU)) return false;

  return true;
}

}  // namespace

int main() {
  bool ok = true;
  ok &= TestTable();
  ok &= TestDecode();
  for (const auto& c : kCases) {
    const bool pass = RunCase(c);
    if (pass) {
      std::cout << "  chain OK: " << c.shader << "\n";
    }
    ok &= pass;
  }

  if (ok) {
    std::cout << "SM5 extended-opcode chain table test passed.\n";
  } else {
    std::cerr << "SM5 extended-opcode chain table test FAILED.\n";
  }
  std::cout.flush();
  return ok ? 0 : 1;
}
