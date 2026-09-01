// Phase 2 of SM5 declaration cross-referencing: the declaration index.
//
// Pins:
//   - BuildDeclarationIndex resolves textures (dimension/return types),
//     samplers, cbuffers (elements/access pattern), inputs (interpolation +
//     semantic) and outputs from the parsed dcl_* instructions;
//   - ExecutionContext::Declarations() exposes the index and rebuilds it after
//     MarkProgramMutated() (simulating add_resource / apply_rule mutations);
//   - declarations added at runtime (AddTextureDeclaration) appear in the
//     rebuilt index;
//   - StampResourceAccessControls now also resolves UAV operands via the index.
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "src/dxp/sm5/ExecutionContext.hpp"
#include "src/dxp/sm5/ShaderProgram.hpp"
#include "src/dxp/sm5/step/ApplyRuleStep_impl.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {

using dxp::sm5::model::CbufferAccessPattern;
using dxp::sm5::model::InterpolationMode;
using dxp::sm5::model::OperandType;
using dxp::sm5::model::ResourceDimension;
using dxp::sm5::model::ResourceReturnType;
using dxp::sm5::model::SignatureSemantic;

int g_failures = 0;

void Check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++g_failures;
  }
}

}  // namespace

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_declaration_index_test <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile((RepoRootPath() / args[1]).string(), input_bytes)) {
    std::cerr << "Failed to read file: " << args[1] << "\n";
    return 1;
  }

  auto program = dxp::sm5::ShaderProgram::FromBytes(input_bytes);
  if (!program) {
    std::cerr << "Failed to parse shader: " << program.error() << "\n";
    return 1;
  }

  // --- 1. Index contents from the parsed shader ---
  // Corpus shader (0x7AFF256C): t0..t8 texture2d float, t9 texture2d uint,
  // s0..s7 default samplers, CB0[220] + CB1[8] immediate indexed,
  // v0 linear, v1 position (noperspective), o0 output.
  const auto index = dxp::sm5::BuildDeclarationIndex(*program);

  Check(index.textures.size() == 10, "corpus shader should declare 10 textures");
  const auto* t0 = index.textures.contains(0) ? &index.textures.at(0) : nullptr;
  Check(t0 != nullptr, "t0 should be in the texture index");
  if (t0 != nullptr) {
    Check(t0->dimension == ResourceDimension::Texture2D, "t0 dimension should be texture2d");
    Check(t0->return_types[0] == ResourceReturnType::Float, "t0 return type x should be float");
  }
  const auto* t9 = index.textures.contains(9) ? &index.textures.at(9) : nullptr;
  Check(t9 != nullptr && t9->return_types[0] == ResourceReturnType::UInt, "t9 return type x should be uint");

  Check(index.samplers.size() == 8, "corpus shader should declare 8 samplers");
  Check(index.samplers.contains(3) && index.samplers.at(3) == dxp::sm5::model::SamplerMode::Default, "s3 should be mode_default");

  Check(index.cbuffers.size() == 2, "corpus shader should declare 2 cbuffers");
  const auto* cb0 = index.cbuffers.contains(0) ? &index.cbuffers.at(0) : nullptr;
  Check(cb0 != nullptr, "cb0 should be in the cbuffer index");
  if (cb0 != nullptr) {
    Check(cb0->elements == 220, "cb0 should have 220 elements");
    Check(cb0->access_pattern == CbufferAccessPattern::ImmediateIndexed, "cb0 should be immediate indexed");
  }

  Check(index.inputs.size() == 2, "corpus shader should declare 2 inputs");
  const auto* v1 = index.inputs.contains(1) ? &index.inputs.at(1) : nullptr;
  Check(v1 != nullptr, "v1 should be in the input index");
  if (v1 != nullptr) {
    Check(v1->semantic.has_value() && *v1->semantic == SignatureSemantic::Position, "v1 semantic should be position");
    Check(v1->interpolation.has_value() && *v1->interpolation == InterpolationMode::LinearNoperspective, "v1 interpolation should be linear_noperspective");
  }
  const auto* v0 = index.inputs.contains(0) ? &index.inputs.at(0) : nullptr;
  Check(v0 != nullptr && !v0->semantic.has_value(), "v0 should carry no semantic");

  Check(index.outputs.size() == 1 && index.outputs.contains(0), "corpus shader should declare output o0");
  Check(index.uavs.empty(), "corpus shader declares no UAVs");

  // FindResource disambiguates textures vs UAVs by operand type.
  Check(index.FindResourceDecl(OperandType::Resource, 9) == t9, "FindResourceDecl(resource, 9) should find t9");
  Check(index.FindResourceDecl(OperandType::UAV, 9) == nullptr, "FindResourceDecl(uav, 9) should not find t9");

  // --- 2. Runtime rebuild after mutation ---
  dxp::sm5::ExecutionContext ctx;
  ctx.program = std::move(*program);
  (void)ctx.Declarations();  // build initial index
  Check(!ctx.declaration_index_dirty, "index should be clean after first build");

  dxp::sm5::step::AddResourceStep::TextureDecl decl{};
  decl.handle = "patch_texture";
  decl.dimension = ResourceDimension::Texture2DArray;
  uint32_t bind_point = 0;
  std::string error;
  Check(ctx.program.AddTextureDeclaration(decl, bind_point, 127, error), "texture add should succeed: " + error);

  ctx.MarkProgramMutated();
  const auto& rebuilt = ctx.Declarations();
  Check(rebuilt.textures.contains(bind_point), "patched texture register should appear in rebuilt index");
  if (rebuilt.textures.contains(bind_point)) {
    Check(rebuilt.textures.at(bind_point).dimension == ResourceDimension::Texture2DArray,
          "patched texture dimension should be texture2darray");
  }
  Check(rebuilt.FindResourceDecl(OperandType::Resource, bind_point) != nullptr, "FindResource should resolve the patched texture");

  // --- 3. Resource stamping via the index (extended-chain synthesis input) ---
  // Build an ld access whose resource operand is a UAV declaration: the stamp
  // must resolve dimension/return types from the UAV entry (D3D11 ld can read
  // UAVs, and pre-index the stamp only scanned dcl_resource).
  {
    dxp::sm5::ExecutionContext uav_ctx;
    dxp::sm5::ShaderProgram uav_program;
    dxp::sm5::model::Instruction uav_dcl;
    uav_dcl.opcode = dxp::sm5::model::Opcode::DclUnorderedAccessViewTyped;
    uav_dcl.controls.resource_dimension = ResourceDimension::Texture2D;
    for (uint32_t c = 0; c < 4; ++c) {
      uav_dcl.controls.resource_return_type[c] = ResourceReturnType::UInt;
    }
    uav_dcl.operands.push_back(dxp::sm5::ShaderProgram::MakeUavOperand(2));
    uav_program.instructions.push_back(uav_dcl);
    uav_ctx.program = std::move(uav_program);

    dxp::sm5::model::Instruction ld;
    ld.opcode = dxp::sm5::model::Opcode::Ld;
    dxp::sm5::model::Operand dst;
    dst.type = OperandType::Temp;
    dst.components.num_components = dxp::sm5::model::NumComponents::One;
    dst.component_mode = 0x20;  // mask mode, .x
    dxp::sm5::model::Operand::Index dst_idx;
    dst_idx.representation = dxp::sm5::model::Operand::IndexRepresentation::Immediate32;
    dst_idx.immediate_lo = 0;
    dst.index_entries.push_back(dst_idx);
    ld.operands.push_back(dst);
    dxp::sm5::model::Operand addr;
    addr.type = OperandType::Temp;
    addr.components.num_components = dxp::sm5::model::NumComponents::Four;
    addr.component_mode = 0x30;  // mask mode, .xy
    dxp::sm5::model::Operand::Index addr_idx;
    addr_idx.representation = dxp::sm5::model::Operand::IndexRepresentation::Immediate32;
    addr_idx.immediate_lo = 0;
    addr.index_entries.push_back(addr_idx);
    ld.operands.push_back(addr);
    dxp::sm5::model::Operand uav;
    uav.type = OperandType::UAV;
    dxp::sm5::model::Operand::Index uav_idx;
    uav_idx.representation = dxp::sm5::model::Operand::IndexRepresentation::Immediate32;
    uav_idx.immediate_lo = 2;
    uav.index_entries.push_back(uav_idx);
    ld.operands.push_back(uav);

    dxp::sm5::step::StampResourceAccessControls(uav_ctx, ld);
    Check(ld.controls.resource_dimension.has_value() && *ld.controls.resource_dimension == ResourceDimension::Texture2D,
          "stamp against a UAV declaration should resolve dimension texture2d from the index");
    Check(ld.controls.resource_return_type[0].has_value() && *ld.controls.resource_return_type[0] == ResourceReturnType::UInt,
          "stamp against a UAV declaration should resolve uint return type from the index");
  }

  if (g_failures == 0) {
    std::cout << "sm5_declaration_index_test passed.\n";
    return 0;
  }
  std::cerr << g_failures << " check(s) failed.\n";
  return 1;
}
