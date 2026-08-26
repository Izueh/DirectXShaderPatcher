#include "dxp/sm5/ShaderProgram.hpp"

#include "value_types/indirect.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/ExportTypes.hpp"
#include "dxp/sm5/Model.hpp"
#include "dxp/sm5/step/AddResourceStep.hpp"
#include "dxp/utils/Hash.hpp"

namespace {

constexpr uint32_t kDxbcContainerSignature = 0x43425844;
constexpr uint32_t kDxbcChunkShdr = 0x52444853;
constexpr uint32_t kDxbcChunkShex = 0x58454853;
constexpr uint32_t kDxbcChunkRdef = 0x46454452;
constexpr uint32_t kDxbcChunkPsio = 0x4F495350;
constexpr uint32_t kDxbcChunkVsio = 0x4F495356;
constexpr uint32_t kDxbcChunkGsio = 0x4F495347;
constexpr uint32_t kDxbcChunkDsio = 0x4F495344;
constexpr uint32_t kDxbcChunkHsio = 0x4F495348;
constexpr uint32_t kDxbcChunkCsio = 0x4F495343;
constexpr uint32_t kDxbcChunkSbio = 0x4F494253;
constexpr uint32_t kDxbcChunkStat = 0x54415453;
constexpr uint32_t kDxbcChunkInfo = 0x4F464E49;
constexpr uint32_t kDxbcChunkFlags = 0x47414C46;
constexpr uint32_t kDxbcChunkType = 0x45505954;

auto FourCCToChunkKind(uint32_t four_cc) -> dxp::sm5::ShaderProgram::ChunkKind {
  using ChunkKind = dxp::sm5::ShaderProgram::ChunkKind;
  switch (four_cc) {
    case kDxbcChunkShdr:
    case kDxbcChunkShex:
      return ChunkKind::Shader;
    case kDxbcChunkRdef:
      return ChunkKind::RDEF;
    case kDxbcChunkPsio:
      return ChunkKind::PSIO;
    case kDxbcChunkVsio:
      return ChunkKind::VSIO;
    case kDxbcChunkGsio:
      return ChunkKind::GSIO;
    case kDxbcChunkDsio:
      return ChunkKind::DSIO;
    case kDxbcChunkHsio:
      return ChunkKind::HSIO;
    case kDxbcChunkCsio:
      return ChunkKind::CSIO;
    case kDxbcChunkSbio:
      return ChunkKind::SBIO;
    case kDxbcChunkStat:
      return ChunkKind::STAT;
    case kDxbcChunkInfo:
      return ChunkKind::INFO;
    case kDxbcChunkFlags:
      return ChunkKind::Flags;
    case kDxbcChunkType:
      return ChunkKind::Type;
    default:
      return ChunkKind::Unknown;
  }
}

auto AlignToDword(uint32_t byte_count) -> uint32_t {
  return (byte_count + 3) & ~3U;
}

auto FourCCToString(uint32_t four_cc) -> std::string {
  constexpr uint32_t kByteMask = 0xffU;
  constexpr uint32_t kByteShift1 = 8U;
  constexpr uint32_t kByteShift2 = 16U;
  constexpr uint32_t kByteShift3 = 24U;
  std::string text(4, '\0');
  text[0] = static_cast<char>(four_cc & kByteMask);
  text[1] = static_cast<char>((four_cc >> kByteShift1) & kByteMask);
  text[2] = static_cast<char>((four_cc >> kByteShift2) & kByteMask);
  text[3] = static_cast<char>((four_cc >> kByteShift3) & kByteMask);
  return text;
}

void ComputeDXBCHash(const uint8_t* data, uint32_t byte_count, uint8_t* out_hash) {
  using dxp::utils::hash::MD5Digest;
  using dxp::utils::hash::MD5Hasher;
  constexpr size_t kBlockSize = 64U;
  const std::span<const uint8_t> bytes(data, byte_count);
  auto size = static_cast<size_t>(byte_count);
  const uint32_t kANum = static_cast<uint32_t>(size) * 8U;
  const uint32_t kBNum = (kANum >> 2U) | 1U;
  std::array<uint8_t, sizeof(uint32_t)> a_bytes = {};
  std::array<uint8_t, sizeof(uint32_t)> b_bytes = {};
  for (uint32_t byte_idx = 0; byte_idx < sizeof(uint32_t); ++byte_idx) {
    a_bytes.at(byte_idx) = static_cast<uint8_t>((kANum >> (8U * byte_idx)) & 0xffU);
    b_bytes.at(byte_idx) = static_cast<uint8_t>((kBNum >> (8U * byte_idx)) & 0xffU);
  }
  const size_t remainder = size % kBlockSize;
  const size_t padding_size = kBlockSize - remainder;
  MD5Hasher md5;
  if (size > remainder) {
    md5.Update(bytes.data(), size - remainder);
  }
  static const std::array<uint8_t, kBlockSize> kSPadding = {0x80};
  if (remainder >= 56U) {
    if (remainder != 0U) {
      md5.Update(&bytes[size - remainder], remainder);
    }
    md5.Update(kSPadding.data(), padding_size);
    md5.Update(a_bytes.data(), a_bytes.size());
    md5.Update(&kSPadding[a_bytes.size()], kSPadding.size() - a_bytes.size() - b_bytes.size());
    md5.Update(b_bytes.data(), b_bytes.size());
  } else {
    md5.Update(a_bytes.data(), a_bytes.size());
    if (remainder != 0U) {
      md5.Update(&bytes[size - remainder], remainder);
    }
    md5.Update(kSPadding.data(), padding_size - a_bytes.size() - b_bytes.size());
    md5.Update(b_bytes.data(), b_bytes.size());
  }
  // DXBC hashing does not finalize the last block properly: the custom a/b
  // padding blocks above ARE the final block, so the digest is the raw MD5
  // state (getDigest in dxbc-spirv's hashDxbcBinary). Using Finalize() would
  // append the standard MD5 padding and produce a mismatched hash.
  MD5Digest result = md5.GetDigest();
  std::memcpy(out_hash, result.data.data(), 16);
}

auto DxbcHashToHex(const dxp::sm5::ShaderProgram::DxbcContainerHeader& header) -> std::string {
  static constexpr std::array<char, 16> kHexDigits = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  std::string hex;
  hex.reserve(32);
  for (const uint32_t word : header.hash) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
      const auto kByte = static_cast<uint8_t>((word >> shift) & 0xffU);
      hex.push_back(kHexDigits.at((kByte >> 4U) & 0xfU));
      hex.push_back(kHexDigits.at(kByte & 0xfU));
    }
  }
  return hex;
}

}  // anonymous namespace

namespace dxp::sm5 {
using namespace dxp::sm5::model;

namespace {

auto ParseReadDword(std::span<const uint8_t> data, uint32_t byte_offset) -> uint32_t {
  uint32_t value = 0;
  std::memcpy(&value, &data[byte_offset], sizeof(value));
  return value;
}

auto ParseDecodeOpcode(uint32_t token0) -> Opcode {
  return static_cast<Opcode>(DECODE_D3D10_SB_OPCODE_TYPE(token0));
}

auto ParseDecodeInstructionLength(uint32_t token0) -> uint32_t {
  const uint32_t kLen = DECODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(token0);
  return kLen == 0 ? 1U : kLen;
}

auto ParseDecodeProgramType(uint32_t version_token) -> ProgramType {
  return static_cast<ProgramType>(DECODE_D3D10_SB_TOKENIZED_PROGRAM_TYPE(version_token));
}

auto ParseDecodeMajorVersion(uint32_t version_token) -> uint32_t {
  return DECODE_D3D10_SB_TOKENIZED_PROGRAM_MAJOR_VERSION(version_token);
}

auto ParseDecodeMinorVersion(uint32_t version_token) -> uint32_t {
  return DECODE_D3D10_SB_TOKENIZED_PROGRAM_MINOR_VERSION(version_token);
}

auto ParseDecodeOperandType(uint32_t token0) -> OperandType {
  return static_cast<OperandType>(DECODE_D3D10_SB_OPERAND_TYPE(token0));
}

auto ParseDecodeNumComponents(uint32_t token0) -> uint32_t {
  return DECODE_D3D10_SB_OPERAND_NUM_COMPONENTS(token0);
}

auto ParseDecodeComponentMode(uint32_t token0) -> uint32_t {
  // Ground truth: the operand token's component bits (selection mode at bits 2-3,
  // mask/swizzle/select value at bits 4-11) round-trip losslessly. Mask mode
  // only defines bits 4-7; swizzle mode defines bits 4-11; select-one mode
  // defines bits 4-5; well-formed tokens leave the rest zero.
  return token0 & kComponentModeBits;
}

auto ParseIsOpcodeExtended(uint32_t token0) -> bool {
  return DECODE_IS_D3D10_SB_OPCODE_EXTENDED(token0) != 0;
}

auto ParseIsOperandExtended(uint32_t token0) -> bool {
  return DECODE_IS_D3D10_SB_OPERAND_EXTENDED(token0) != 0;
}

auto ParseOpcodeControls(std::span<const uint8_t> data, uint32_t instruction_start, uint32_t instruction_length,
                         uint32_t token0) -> OpcodeControls {
  OpcodeControls controls;
  const auto kOpcode = DECODE_D3D10_SB_OPCODE_TYPE(token0);
  controls.saturate = DECODE_IS_D3D10_SB_INSTRUCTION_SATURATE_ENABLED(token0) != 0;

  if (OpcodeUsesTestBoolean(static_cast<dxp::sm5::Opcode>(kOpcode))) {
    controls.test_boolean = DECODE_D3D10_SB_INSTRUCTION_TEST_BOOLEAN(token0);
  }
  controls.precise_values = DECODE_D3D11_SB_INSTRUCTION_PRECISE_VALUES(token0);
  if (kOpcode == D3D10_SB_OPCODE_RESINFO) {
    controls.resinfo_return_type = DECODE_D3D10_SB_RESINFO_INSTRUCTION_RETURN_TYPE(token0);
  }
  if (kOpcode == D3D11_SB_OPCODE_SYNC) {
    controls.sync_flags = DECODE_D3D11_SB_SYNC_FLAGS(token0);
  }
  if (kOpcode == D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS) {
    controls.sync_flags = DECODE_D3D10_SB_GLOBAL_FLAGS(token0);
  }
  if (kOpcode == D3D10_SB_OPCODE_DCL_RESOURCE) {
    controls.resource_dimension = static_cast<ResourceDimension>(DECODE_D3D10_SB_RESOURCE_DIMENSION(token0));
    // Parse the four per-component resource return types (4 bits each) the same
    // way the extended RESOURCE_RETURN_TYPE token and the DCL encode path do,
    // so resource-access synthesis can reproduce the declaration's real types
    // (not a silent float default).
    for (uint32_t component = 0; component < 4; ++component) {
      controls.resource_return_type[component] = static_cast<ResourceReturnType>(DECODE_D3D10_SB_RESOURCE_RETURN_TYPE(token0, component));
    }
  }
  if (kOpcode == D3D10_SB_OPCODE_DCL_INPUT_PS || kOpcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV) {
    controls.input_interpolation_mode = static_cast<uint32_t>(DECODE_D3D10_SB_INPUT_INTERPOLATION_MODE(token0));
  }
  if (!ParseIsOpcodeExtended(token0)) {
    return controls;
  }
  uint32_t cursor = instruction_start + 1;
  const uint32_t kEnd = instruction_start + instruction_length;
  while (cursor < kEnd) {
    const uint32_t kExtToken = ParseReadDword(data, cursor * 4);
    controls.extended_op_codes.emplace_back(kExtToken);
    ++cursor;
    if (!DECODE_IS_D3D10_SB_OPCODE_EXTENDED(kExtToken)) {
      break;
    }
  }
  return controls;
}

auto ParseOperand(std::span<const uint8_t> data, uint32_t total_dwords, uint32_t instruction_end, uint32_t& cursor) -> Operand {
  Operand operand;
  if (cursor >= total_dwords || cursor >= instruction_end) {
    return operand;
  }
  const uint32_t kStart = cursor;
  const uint32_t kOperandToken = ParseReadDword(data, cursor * 4);
  operand.type = ParseDecodeOperandType(kOperandToken);
  operand.components.num_components = static_cast<NumComponents>(ParseDecodeNumComponents(kOperandToken));
  operand.component_mode = ParseDecodeComponentMode(kOperandToken);
  ++cursor;
  if (ParseIsOperandExtended(kOperandToken) && cursor < instruction_end) {
    uint32_t ext_token = ParseReadDword(data, cursor * 4);
    if (DECODE_D3D10_SB_EXTENDED_OPERAND_TYPE(ext_token) == D3D10_SB_EXTENDED_OPERAND_MODIFIER) {
      operand.modifier = static_cast<OperandModifier>(DECODE_D3D10_SB_OPERAND_MODIFIER(ext_token));
    }
    ++cursor;
    while (DECODE_IS_D3D10_SB_OPERAND_DOUBLE_EXTENDED(ext_token) && cursor < instruction_end) {
      ext_token = ParseReadDword(data, cursor * 4);
      ++cursor;
    }
  }
  const uint32_t kIndexDim = DECODE_D3D10_SB_OPERAND_INDEX_DIMENSION(kOperandToken);
  for (uint32_t dim = 0; dim < kIndexDim && dim < 3 && cursor < instruction_end; ++dim) {
    const auto kIndexRep = DECODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(
        static_cast<D3D10_SB_OPERAND_INDEX_DIMENSION>(dim), kOperandToken);
    Operand::Index index_entry;
    if (kIndexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE32) {
      const uint32_t kImm = ParseReadDword(data, cursor * 4);
      index_entry.representation = Operand::IndexRepresentation::Immediate32;
      index_entry.immediate_lo = kImm;
      operand.index_entries.push_back(std::move(index_entry));
      ++cursor;
      continue;
    }
    if (kIndexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE64 && (cursor + 1) < instruction_end) {
      const uint32_t kLo = ParseReadDword(data, cursor * 4);
      const uint32_t kHi = ParseReadDword(data, (cursor + 1) * 4);
      index_entry.representation = Operand::IndexRepresentation::Immediate64;
      index_entry.immediate_lo = kLo;
      index_entry.immediate_hi = kHi;
      operand.index_entries.push_back(std::move(index_entry));
      cursor += 2;
      continue;
    }
    if (kIndexRep == D3D10_SB_OPERAND_INDEX_RELATIVE || kIndexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE || kIndexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE) {
      if (kIndexRep == D3D10_SB_OPERAND_INDEX_RELATIVE) {
        index_entry.representation = Operand::IndexRepresentation::Relative;
      } else if (kIndexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE) {
        index_entry.representation = Operand::IndexRepresentation::Immediate32PlusRelative;
      } else {
        index_entry.representation = Operand::IndexRepresentation::Immediate64PlusRelative;
      }
      if (kIndexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE && cursor < instruction_end) {
        const uint32_t kImm = ParseReadDword(data, cursor * 4);
        index_entry.immediate_lo = kImm;
        ++cursor;
      } else if (kIndexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE && (cursor + 1) < instruction_end) {
        const uint32_t kLo = ParseReadDword(data, cursor * 4);
        const uint32_t kHi = ParseReadDword(data, (cursor + 1) * 4);
        index_entry.immediate_lo = kLo;
        index_entry.immediate_hi = kHi;
        cursor += 2;
      }
      Operand rel = ParseOperand(data, total_dwords, instruction_end, cursor);
      index_entry.relative_operand = xyz::indirect<Operand>(std::move(rel));
      operand.index_entries.push_back(std::move(index_entry));
      continue;
    }
    operand.index_entries.push_back(std::move(index_entry));
  }
  if (operand.type == OperandType::Immediate32) {
    const uint32_t immediate_count = (operand.components.num_components == NumComponents::One) ? 1 : 4;
    for (uint32_t i = 0; i < immediate_count && cursor < instruction_end; ++i) {
      const uint32_t kImm = ParseReadDword(data, cursor * 4);
      Operand::Index idx;
      idx.representation = Operand::IndexRepresentation::Immediate32;
      idx.immediate_lo = kImm;
      operand.index_entries.push_back(std::move(idx));
      ++cursor;
    }
  }
  operand.source_offset = kStart;
  operand.source_length = cursor - kStart;
  return operand;
}

void UpdateDeclarationOverlay(ShaderProgram& program, const Instruction& instruction) {
  const auto kOpcode = instruction.opcode;
  if (kOpcode == Opcode::DclTemps && !instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
    program.temp_count = *instruction.operands.front().index_entries[0].immediate_lo;
  }
  if ((kOpcode == Opcode::DclResource || kOpcode == Opcode::DclResourceRaw || kOpcode == Opcode::DclResourceStructured) && !instruction.operands.empty()) {
    ResourceDecl decl;
    if (!instruction.operands.front().index_entries.empty()) {
      decl.register_bind_point = *instruction.operands.front().index_entries[0].immediate_lo;
    }
    program.resources.push_back(decl);
  }
  if (kOpcode == D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER && !instruction.operands.empty()) {
    CBufferDecl decl;
    if (!instruction.operands.front().index_entries.empty()) {
      decl.register_bind_point = *instruction.operands.front().index_entries[0].immediate_lo;
    }
    program.cbuffers.push_back(decl);
  }
  if (kOpcode == D3D10_SB_OPCODE_DCL_SAMPLER && !instruction.operands.empty()) {
    SamplerDecl decl;
    if (!instruction.operands.front().index_entries.empty()) {
      decl.register_bind_point = *instruction.operands.front().index_entries[0].immediate_lo;
    }
    program.samplers.push_back(decl);
  }
  if (kOpcode == D3D11_SB_OPCODE_DCL_THREAD_GROUP && instruction.operands.size() >= 3) {
    ThreadGroupDecl decl;
    if (!instruction.operands[0].index_entries.empty()) {
      decl.group_size_x = *instruction.operands[0].index_entries[0].immediate_lo;
    }
    if (!instruction.operands[1].index_entries.empty()) {
      decl.group_size_y = *instruction.operands[1].index_entries[0].immediate_lo;
    }
    if (!instruction.operands[2].index_entries.empty()) {
      decl.group_size_z = *instruction.operands[2].index_entries[0].immediate_lo;
    }
    program.thread_groups.push_back(decl);
  }
  if (kOpcode == D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS) {
    program.global_flags.flags = instruction.controls.sync_flags;
  }
}

auto ParseProgramImpl(std::span<const uint8_t> data, uint32_t size, ShaderProgram& program) -> bool {
  if (data.empty() || size < 8) {
    return false;
  }
  program.program_type = ProgramType::PixelShader;
  program.major_version = 0;
  program.minor_version = 0;
  program.total_length_in_dwords = 0;
  program.instructions.clear();
  program.resources.clear();
  program.samplers.clear();
  program.cbuffers.clear();
  program.thread_groups.clear();
  program.global_flags = {};
  program.temp_count = 0;
  program.indexable_temps.clear();
  const uint32_t kVersionToken = ParseReadDword(data, 0);
  const uint32_t kLengthToken = ParseReadDword(data, 4);
  program.program_type = ParseDecodeProgramType(kVersionToken);
  program.major_version = ParseDecodeMajorVersion(kVersionToken);
  program.minor_version = ParseDecodeMinorVersion(kVersionToken);
  program.total_length_in_dwords = kLengthToken;
  const uint32_t kTotalDwords = std::min(kLengthToken, size / 4);
  uint32_t cursor = 2;
  while (cursor < kTotalDwords) {
    const uint32_t kInstructionStart = cursor;
    const uint32_t kToken0 = ParseReadDword(data, cursor * 4);
    const Opcode kOpcode = ParseDecodeOpcode(kToken0);
    const uint32_t kLength = ParseDecodeInstructionLength(kToken0);
    if (kLength == 0 || (kInstructionStart + kLength) > kTotalDwords) {
      return false;
    }
    Instruction instruction;
    instruction.opcode = kOpcode;
    instruction.length_in_dwords = kLength;
    instruction.source_offset = kInstructionStart;
    instruction.source_length = kLength;
    instruction.controls = ParseOpcodeControls(data, kInstructionStart, kLength, kToken0);
    if (kOpcode == D3D10_SB_OPCODE_DCL_SAMPLER) {
      instruction.sampler_mode = static_cast<SamplerMode>(DECODE_D3D10_SB_SAMPLER_MODE(kToken0));
      instruction.controls.mode = static_cast<SamplerMode>(DECODE_D3D10_SB_SAMPLER_MODE(kToken0));
    }
    if (kOpcode == D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) {
      instruction.controls.access_pattern = static_cast<CbufferAccessPattern>(DECODE_D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN(kToken0));
    }
    if (kOpcode == D3D10_SB_OPCODE_DCL_RESOURCE && kLength >= 4) {
      // The last dword of a dcl_resource instruction is the 4-component
      // return-type token; capture it so re-serialization preserves non-float
      // return types (the token is otherwise parsed as a trailing operand).
      const uint32_t kReturnTypeToken = ParseReadDword(data, (kInstructionStart + kLength - 1) * 4);
      for (uint32_t component = 0; component < 4; ++component) {
        instruction.controls.resource_return_type[component] = static_cast<ResourceReturnType>((kReturnTypeToken >> (component * D3D10_SB_RESOURCE_RETURN_TYPE_NUMBITS)) & D3D10_SB_RESOURCE_RETURN_TYPE_MASK);
      }
    }
    if (kOpcode == D3D10_SB_OPCODE_CUSTOMDATA) {
      // DCL_CUSTOMDATA stores opaque bytes that must round-trip exactly.
      instruction.custom_data.reserve(kLength - 1);
      for (uint32_t i = 1; i < kLength; ++i) {
        instruction.custom_data.push_back(ParseReadDword(data, (kInstructionStart + i) * 4));
      }
      program.instructions.push_back(std::move(instruction));
      cursor += kLength;
      continue;
    }
    if (kOpcode == D3D10_SB_OPCODE_DCL_TEMPS) {
      if (kLength < 2) {
        return false;
      }
      Operand operand;
      operand.type = OperandType::Temp;
      operand.components.num_components = NumComponents::Zero;
      operand.component_mode = 0;
      Operand::Index idx;
      idx.representation = Operand::IndexRepresentation::Immediate32;
      idx.immediate_lo = ParseReadDword(data, (kInstructionStart + 1) * 4);
      operand.index_entries.push_back(std::move(idx));
      operand.source_offset = kInstructionStart + 1;
      operand.source_length = 1;
      instruction.operands.push_back(std::move(operand));
      UpdateDeclarationOverlay(program, instruction);
      program.instructions.push_back(std::move(instruction));
      cursor += kLength;
      continue;
    }
    const uint32_t kExtendedOpcodeCount = [&]() -> uint32_t {
      if (!ParseIsOpcodeExtended(kToken0)) {
        return 0;
      }
      uint32_t count = 0;
      uint32_t ext_count = kInstructionStart + 1;
      const uint32_t kEnd = kInstructionStart + kLength;
      while (ext_count < kEnd) {
        const uint32_t kExt = ParseReadDword(data, ext_count * 4);
        ++count;
        ++ext_count;
        if (!DECODE_IS_D3D10_SB_OPCODE_EXTENDED(kExt)) {
          break;
        }
      }
      return count;
    }();
    uint32_t operand_cursor = kInstructionStart + 1 + kExtendedOpcodeCount;
    const uint32_t kInstructionEnd = kInstructionStart + kLength;
    while (operand_cursor < kInstructionEnd) {
      const uint32_t kBefore = operand_cursor;
      Operand operand = ParseOperand(data, kTotalDwords, kInstructionEnd, operand_cursor);
      if (operand.source_length == 0 || operand_cursor <= kBefore) {
        break;
      }
      instruction.operands.push_back(std::move(operand));
    }
    UpdateDeclarationOverlay(program, instruction);
    program.instructions.push_back(std::move(instruction));
    cursor += kLength;
  }
  return true;
}

}  // anonymous namespace

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : program_type(other.program_type),
      major_version(other.major_version),
      minor_version(other.minor_version),
      total_length_in_dwords(other.total_length_in_dwords),
      instructions(std::move(other.instructions)),
      resources(std::move(other.resources)),
      samplers(std::move(other.samplers)),
      cbuffers(std::move(other.cbuffers)),
      thread_groups(std::move(other.thread_groups)),
      global_flags(other.global_flags),
      temp_count(other.temp_count),
      indexable_temps(std::move(other.indexable_temps)),
      header(other.header),
      chunks(std::move(other.chunks)) {
  other.program_type = ProgramType::PixelShader;
  other.major_version = 0;
  other.minor_version = 0;
  other.total_length_in_dwords = 0;
  other.temp_count = 0;
  other.global_flags = {};
  other.header = {};
}

auto ShaderProgram::operator=(ShaderProgram&& other) noexcept -> ShaderProgram& {
  if (this == &other) {
    return *this;
  }
  program_type = other.program_type;
  major_version = other.major_version;
  minor_version = other.minor_version;
  total_length_in_dwords = other.total_length_in_dwords;
  instructions = std::move(other.instructions);
  resources = std::move(other.resources);
  samplers = std::move(other.samplers);
  cbuffers = std::move(other.cbuffers);
  thread_groups = std::move(other.thread_groups);
  global_flags = other.global_flags;
  temp_count = other.temp_count;
  indexable_temps = std::move(other.indexable_temps);
  header = other.header;
  chunks = std::move(other.chunks);
  other.program_type = ProgramType::PixelShader;
  other.major_version = 0;
  other.minor_version = 0;
  other.total_length_in_dwords = 0;
  other.temp_count = 0;
  other.global_flags = {};
  other.header = {};
  return *this;
}

auto ShaderProgram::ParseProgram(std::span<const uint8_t> data, ShaderProgram& program) -> bool {
  return ParseProgramImpl(data, static_cast<uint32_t>(data.size()), program);
}

auto ShaderProgram::SerializeBitcode() const -> std::vector<uint8_t> {
  std::vector<uint32_t> words;
  words.push_back(ENCODE_D3D10_SB_TOKENIZED_PROGRAM_VERSION_TOKEN(
      static_cast<D3D10_SB_TOKENIZED_PROGRAM_TYPE>(static_cast<uint32_t>(program_type)), major_version, minor_version));
  words.push_back(0);
  for (const auto& instruction : instructions) {
    auto encoded = instruction.Encode();
    words.insert(words.end(), encoded.begin(), encoded.end());
  }
  words[1] = static_cast<uint32_t>(words.size());
  std::vector<uint8_t> out_bytes(words.size() * sizeof(uint32_t));
  std::memcpy(out_bytes.data(), words.data(), out_bytes.size());
  return out_bytes;
}

auto ShaderProgram::FromBytes(std::span<const uint8_t> bytes) -> std::expected<ShaderProgram, std::string> {
  ShaderProgram prog;
  if (bytes.size() < 32) {
    return std::unexpected("[sm5] container too small to be a DXBC container");
  }

  size_t offset = 0;
  std::memcpy(&prog.header.signature, &bytes[offset], 4);
  offset += 4;
  for (unsigned int& idx : prog.header.hash) {
    std::memcpy(&idx, &bytes[offset], 4);
    offset += 4;
  }
  std::memcpy(&prog.header.one, &bytes[offset], 4);
  offset += 4;
  std::memcpy(&prog.header.total_size_in_bytes, &bytes[offset], 4);
  offset += 4;
  std::memcpy(&prog.header.chunk_count, &bytes[offset], 4);
  offset += 4;

  if (prog.header.signature != kDxbcContainerSignature) {
    std::ostringstream message;
    message << "[sm5] invalid DXBC signature: 0x" << std::hex << prog.header.signature;
    return std::unexpected(message.str());
  }

  if (prog.header.total_size_in_bytes != bytes.size()) {
    std::ostringstream message;
    message << "[sm5] DXBC byte count mismatch: header says " << prog.header.total_size_in_bytes << ", actual "
            << bytes.size();
    return std::unexpected(message.str());
  }

  const uint32_t chunk_count = prog.header.chunk_count;
  if (offset + static_cast<size_t>(chunk_count) * 4 > bytes.size()) {
    return std::unexpected("[sm5] chunk table extends beyond container");
  }

  std::vector<uint32_t> chunk_offsets(chunk_count);
  for (uint32_t idx = 0; idx < chunk_count; ++idx) {
    std::memcpy(&chunk_offsets[idx], &bytes[offset + (static_cast<size_t>(idx) * 4)], 4);
  }

  std::vector<uint32_t> sorted_offsets = chunk_offsets;
  std::ranges::sort(sorted_offsets);

  prog.chunks.reserve(sorted_offsets.size());
  for (const unsigned int chunk_offset : sorted_offsets) {
    if (chunk_offset + 8 > bytes.size()) {
      continue;
    }

    uint32_t four_cc = 0;
    uint32_t chunk_size = 0;
    std::memcpy(&four_cc, &bytes[chunk_offset], 4);
    std::memcpy(&chunk_size, &bytes[chunk_offset + 4], 4);

    if (static_cast<size_t>(chunk_offset) + 8U + chunk_size > bytes.size()) {
      continue;
    }

    ShaderProgram::DxbcChunk chunk;
    chunk.four_cc = four_cc;
    chunk.kind = FourCCToChunkKind(four_cc);
    chunk.offset_in_container = chunk_offset;

    if (chunk_size > 0) {
      chunk.data.resize(chunk_size);
      std::memcpy(chunk.data.data(), &bytes[chunk_offset + 8], chunk_size);
    }

    prog.chunks.push_back(std::move(chunk));
  }

  const auto* shader_chunk = prog.GetShaderChunk();
  if (shader_chunk == nullptr || shader_chunk->data.empty()) {
    return prog;
  }
  ParseProgramImpl(std::span<const uint8_t>(shader_chunk->data.data(), shader_chunk->data.size()),
                   static_cast<uint32_t>(shader_chunk->data.size()), prog);
  return prog;
}

auto ShaderProgram::Serialize() -> std::expected<std::vector<uint8_t>, std::string> {
  if (chunks.empty()) {
    return std::unexpected("[sm5] cannot serialize empty container");
  }

  auto shader_bytecode = SerializeBitcode();

  auto* shader_chunk = GetShaderChunk();
  if (shader_chunk != nullptr) {
    shader_chunk->data = shader_bytecode;
  }

  auto chunk_count = static_cast<uint32_t>(chunks.size());
  const uint32_t header_size = 32 + (chunk_count * 4);

  uint32_t total_chunk_data_size = 0;
  for (const auto& chunk : chunks) {
    total_chunk_data_size += AlignToDword(8 + static_cast<uint32_t>(chunk.data.size()));
  }

  const uint32_t total_size = AlignToDword(header_size + total_chunk_data_size);
  std::vector<uint8_t> out_bytes(total_size, 0);

  size_t out_offset = 0;
  std::memcpy(&out_bytes[out_offset], &header.signature, 4);
  out_offset += 4;

  uint32_t zero = 0;
  for (int i = 0; i < 4; ++i) {
    std::memcpy(&out_bytes[out_offset], &zero, 4);
    out_offset += 4;
  }

  uint32_t one = header.one == 0 ? 1U : header.one;
  std::memcpy(&out_bytes[out_offset], &one, 4);
  out_offset += 4;

  auto total_size_u32 = total_size;
  std::memcpy(&out_bytes[out_offset], &total_size_u32, 4);
  out_offset += 4;
  std::memcpy(&out_bytes[out_offset], &chunk_count, 4);
  out_offset += 4;

  std::vector<uint32_t> chunk_offsets;
  chunk_offsets.reserve(chunks.size());
  auto current_offset = header_size;
  for (const auto& chunk : chunks) {
    chunk_offsets.push_back(current_offset);
    current_offset += AlignToDword(8 + static_cast<uint32_t>(chunk.data.size()));
  }

  for (uint32_t i = 0; i < chunk_count; ++i) {
    std::memcpy(&out_bytes[out_offset], &chunk_offsets[i], 4);
    out_offset += 4;
  }

  for (size_t i = 0; i < chunks.size(); ++i) {
    const auto& chunk = chunks[i];
    auto chunk_data_size = static_cast<uint32_t>(chunk.data.size());
    std::memcpy(&out_bytes[chunk_offsets[i]], &chunk.four_cc, 4);
    std::memcpy(&out_bytes[chunk_offsets[i] + 4], &chunk_data_size, 4);
    if (chunk_data_size > 0) {
      std::memcpy(&out_bytes[chunk_offsets[i] + 8], chunk.data.data(), chunk_data_size);
    }
  }

  constexpr uint32_t kHashStartOffset = offsetof(DxbcContainerHeader, one);
  ComputeDXBCHash(&out_bytes[kHashStartOffset], static_cast<uint32_t>(out_bytes.size() - kHashStartOffset),
                  &out_bytes[offsetof(DxbcContainerHeader, hash)]);

  return out_bytes;
}

auto ShaderProgram::BuildReport() -> dxp::PatchContainerReport {
  auto serialized_result = Serialize();
  dxp::PatchContainerReport report;
  report.format = "DXBC";

  if (!serialized_result) {
    return report;
  }
  const auto& serialized = *serialized_result;
  if (serialized.size() < sizeof(DxbcContainerHeader)) {
    return report;
  }
  DxbcContainerHeader header{};
  std::memcpy(&header, serialized.data(), sizeof(header));
  report.total_size_in_bytes = header.total_size_in_bytes;
  report.hash_hex = DxbcHashToHex(header);

  report.chunks.reserve(chunks.size());
  for (const auto& chunk : chunks) {
    dxp::PatchChunkReport chunk_report;
    chunk_report.id = FourCCToString(chunk.four_cc);
    chunk_report.four_cc = chunk.four_cc;
    chunk_report.offset_in_container = chunk.offset_in_container;
    chunk_report.size_in_bytes = static_cast<uint32_t>(chunk.data.size());
    report.chunks.push_back(std::move(chunk_report));
  }

  return report;
}

auto ShaderProgram::FindChunk(ChunkKind kind) const -> const ShaderProgram::DxbcChunk* {
  for (const auto& chunk : chunks) {
    if (chunk.kind == kind) {
      return &chunk;
    }
  }
  return nullptr;
}

auto ShaderProgram::FindChunk(ChunkKind kind) -> ShaderProgram::DxbcChunk* {
  for (auto& chunk : chunks) {
    if (chunk.kind == kind) {
      return &chunk;
    }
  }
  return nullptr;
}

auto ShaderProgram::FindChunkByFourCC(uint32_t four_cc) const -> const ShaderProgram::DxbcChunk* {
  for (const auto& chunk : chunks) {
    if (chunk.four_cc == four_cc) {
      return &chunk;
    }
  }
  return nullptr;
}

auto ShaderProgram::FindChunkByFourCC(uint32_t four_cc) -> ShaderProgram::DxbcChunk* {
  for (auto& chunk : chunks) {
    if (chunk.four_cc == four_cc) {
      return &chunk;
    }
  }
  return nullptr;
}

auto ShaderProgram::GetShaderChunk() const -> const ShaderProgram::DxbcChunk* {
  for (const auto& chunk : chunks) {
    if (chunk.kind == ChunkKind::Shader) {
      return &chunk;
    }
  }
  return nullptr;
}

auto ShaderProgram::GetShaderChunk() -> ShaderProgram::DxbcChunk* {
  for (auto& chunk : chunks) {
    if (chunk.kind == ChunkKind::Shader) {
      return &chunk;
    }
  }
  return nullptr;
}

auto ShaderProgram::GetOpcodeCounts() const -> std::unordered_map<std::string, int32_t> {
  std::unordered_map<std::string, int32_t> counts;
  for (const auto& instr : instructions) {
    const auto val = static_cast<uint32_t>(instr.opcode);
    if (val < glz::meta<Opcode>::keys.size()) {
      counts[glz::meta<Opcode>::keys.at(val)]++;
    }
  }
  return counts;
}

auto ShaderProgram::GetInstructionOpcodes() const -> std::vector<Opcode> {
  std::vector<Opcode> opcodes;
  opcodes.reserve(instructions.size());
  for (const auto& instr : instructions) {
    opcodes.push_back(instr.opcode);
  }
  return opcodes;
}

auto ShaderProgram::FindNextAvailableTexture(unsigned preferred, bool from_high) const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  for (const auto& r : resources) occupied.insert(r.register_bind_point);
  unsigned bp = preferred;
  if (from_high) {
    while (occupied.contains(bp)) --bp;
  } else {
    while (occupied.contains(bp)) ++bp;
  }
  return bp;
}

auto ShaderProgram::FindNextAvailableSampler(unsigned preferred, bool from_high) const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  for (const auto& s : samplers) occupied.insert(s.register_bind_point);
  unsigned bp = preferred;
  if (from_high) {
    while (occupied.contains(bp)) --bp;
  } else {
    while (occupied.contains(bp)) ++bp;
  }
  return bp;
}

auto ShaderProgram::FindNextAvailableCBuffer(unsigned preferred, bool from_high) const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  for (const auto& c : cbuffers) occupied.insert(c.register_bind_point);
  unsigned bp = preferred;
  if (from_high) {
    while (occupied.contains(bp)) --bp;
  } else {
    while (occupied.contains(bp)) ++bp;
  }
  return bp;
}

auto ShaderProgram::FindNextAvailableUAV(unsigned preferred, bool from_high) const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  for (const auto& r : resources) occupied.insert(r.register_bind_point);
  unsigned bp = preferred;
  if (from_high) {
    while (occupied.contains(bp)) --bp;
  } else {
    while (occupied.contains(bp)) ++bp;
  }
  return bp;
}

namespace {

/// @brief Collects input/output signature register indices from DCL instructions.
void CollectSignatureRegisters(const std::vector<Instruction>& instructions,
                               std::unordered_set<uint32_t>& occupied, bool inputs) {
  for (const auto& instr : instructions) {
    const auto op = instr.opcode;
    const bool is_input = op == Opcode::DclInput || op == Opcode::DclInputPs || op == Opcode::DclInputPsSiv || op == Opcode::DclInputSgv || op == Opcode::DclInputSiv;
    const bool is_output = op == Opcode::DclOutput || op == Opcode::DclOutputSgv || op == Opcode::DclOutputSiv;
    if ((inputs && is_input) || (!inputs && is_output)) {
      if (!instr.operands.empty() && !instr.operands.front().index_entries.empty()) {
        const auto& idx = instr.operands.front().index_entries.front();
        if (idx.immediate_lo.has_value()) occupied.insert(*idx.immediate_lo);
      }
    }
  }
}

}  // namespace

auto ShaderProgram::FindNextAvailableInput(unsigned preferred, bool from_high) const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  CollectSignatureRegisters(instructions, occupied, /*inputs=*/true);
  unsigned bp = preferred;
  if (from_high) {
    while (occupied.contains(bp)) --bp;
  } else {
    while (occupied.contains(bp)) ++bp;
  }
  return bp;
}

auto ShaderProgram::FindNextAvailableOutput(unsigned preferred, bool from_high) const -> unsigned {
  std::unordered_set<uint32_t> occupied;
  CollectSignatureRegisters(instructions, occupied, /*inputs=*/false);
  unsigned bp = preferred;
  if (from_high) {
    while (occupied.contains(bp)) --bp;
  } else {
    while (occupied.contains(bp)) ++bp;
  }
  return bp;
}

auto ShaderProgram::EnsureTempDeclaration() -> void {
  if (temp_count == 0) return;
  bool found_dcl_temps = false;
  for (auto& instruction : instructions) {
    if (instruction.opcode == Opcode::DclTemps) {
      instruction = BuildTempDeclaration(temp_count);
      found_dcl_temps = true;
      break;
    }
  }
  if (!found_dcl_temps) {
    uint32_t insert_index = 0;
    for (uint32_t i = 0; i < instructions.size(); ++i) {
      if (OpcodeIsDeclaration(instructions[i].opcode)) insert_index = i + 1;
    }
    instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                        BuildTempDeclaration(temp_count));
  }
}

auto ShaderProgram::MakeSelectComponentMode(uint32_t component) -> uint32_t {
  return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) | ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(static_cast<D3D10_SB_4_COMPONENT_NAME>(component));
}

auto ShaderProgram::MakeConstantBufferDeclarationOperand(uint32_t register_index, uint32_t element_count) -> Operand {
  Operand operand;
  operand.type = OperandType::CBuffer;
  operand.components.num_components = NumComponents::Four;
  operand.component_mode =
      ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) | (ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(D3D10_SB_4_COMPONENT_X, D3D10_SB_4_COMPONENT_Y, D3D10_SB_4_COMPONENT_Z, D3D10_SB_4_COMPONENT_W) << 4);
  Operand::Index bind_index;
  bind_index.representation = Operand::IndexRepresentation::Immediate32;
  bind_index.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(bind_index));
  Operand::Index count_index;
  count_index.representation = Operand::IndexRepresentation::Immediate32;
  count_index.immediate_lo = element_count;
  operand.index_entries.push_back(std::move(count_index));
  return operand;
}

auto ShaderProgram::MakeSamplerOperand(uint32_t register_index) -> Operand {
  Operand operand;
  operand.type = OperandType::Sampler;
  operand.components.num_components = NumComponents::Zero;
  operand.component_mode = 0;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(idx));
  return operand;
}

auto ShaderProgram::MakeResourceOperand(uint32_t register_index) -> Operand {
  Operand operand;
  operand.type = OperandType::Resource;
  operand.components.num_components = NumComponents::Zero;
  operand.component_mode = 0;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(idx));
  return operand;
}

auto ShaderProgram::MakeInputOperand(uint32_t register_index) -> Operand {
  Operand operand;
  operand.type = OperandType::Input;
  operand.components.num_components = NumComponents::Four;
  operand.component_mode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(idx));
  return operand;
}

auto ShaderProgram::MakeOutputOperand(uint32_t register_index) -> Operand {
  Operand operand;
  operand.type = OperandType::Output;
  operand.components.num_components = NumComponents::Four;
  operand.component_mode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) | D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(idx));
  return operand;
}

auto ShaderProgram::MakeUavOperand(uint32_t register_index) -> Operand {
  Operand operand;
  operand.type = OperandType::UAV;
  operand.components.num_components = NumComponents::Zero;
  operand.component_mode = 0;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = register_index;
  operand.index_entries.push_back(std::move(idx));
  return operand;
}

auto ShaderProgram::FindInsertAfterLastDeclaration(Opcode opcode) -> uint32_t {
  uint32_t insert_index = 0;
  for (uint32_t i = 0; i < instructions.size(); ++i) {
    if (instructions[i].opcode == opcode) {
      insert_index = i + 1;
    }
  }
  return insert_index;
}

auto ShaderProgram::AllocateBindPoint(const std::unordered_set<uint32_t>& occupied, bool auto_bind,
                                      uint32_t requested_register_index, bool reverse,
                                      uint32_t& resolved_register_index, std::string& error) -> bool {
  if (!auto_bind) {
    if (occupied.contains(requested_register_index)) {
      error = "SM5 declaration register index already occupied: " + std::to_string(requested_register_index);
      return false;
    }
    resolved_register_index = requested_register_index;
    return true;
  }

  // Reverse order: take the HIGHEST free slot (requested = the max) so the
  // patched resource lands away from the game's low-register bindings.
  uint32_t candidate = requested_register_index;
  while (occupied.contains(candidate)) {
    if (reverse) {
      if (candidate == 0) {
        error = "SM5 declaration auto_bind exhausted available bind points";
        return false;
      }
      --candidate;
    } else {
      if (candidate == std::numeric_limits<uint32_t>::max()) {
        error = "SM5 declaration auto_bind exhausted available bind points";
        return false;
      }
      ++candidate;
    }
  }
  resolved_register_index = candidate;
  return true;
}

auto ShaderProgram::RecordNamedBinding(std::unordered_map<std::string, uint32_t>& bindings, const std::string& handle,
                                       uint32_t register_index, const char* resource_kind, std::string& error) -> bool {
  if (handle.empty()) {
    return true;
  }
  if (bindings.contains(handle)) {
    error = std::string("duplicate SM5 ") + resource_kind + " declaration handle: " + handle;
    return false;
  }
  bindings[handle] = register_index;
  return true;
}

auto ShaderProgram::FinalizeInstruction(Instruction instruction) -> Instruction {
  instruction.length_in_dwords = static_cast<uint32_t>(instruction.Encode().size());
  return instruction;
}

auto ShaderProgram::BuildTempDeclaration(uint32_t temp_count) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclTemps;
  Operand operand;
  operand.type = OperandType::Temp;
  operand.components.num_components = NumComponents::Zero;
  operand.component_mode = 0;
  Operand::Index idx;
  idx.representation = Operand::IndexRepresentation::Immediate32;
  idx.immediate_lo = temp_count;
  operand.index_entries.push_back(std::move(idx));
  instruction.operands.push_back(std::move(operand));
  return FinalizeInstruction(std::move(instruction));
}

auto ShaderProgram::BuildConstantBufferDeclaration(const dxp::sm5::step::AddResourceStep::CBufferDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclConstantBuffer;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeConstantBufferDeclarationOperand(register_index, decl.elements));
  instruction.controls.access_pattern = decl.access_pattern;
  instruction.controls.access_pattern_raw = static_cast<uint32_t>(decl.access_pattern);
  return instruction;
}

auto ShaderProgram::BuildTextureDeclaration(const dxp::sm5::step::AddResourceStep::TextureDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclResource;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeResourceOperand(register_index));
  instruction.controls.resource_dimension = static_cast<ResourceDimension>(decl.dimension);
  for (uint32_t component = 0; component < 4; ++component) {
    instruction.controls.resource_return_type[component] = ResourceReturnType::Float;
  }
  return instruction;
}

auto ShaderProgram::BuildInputDeclaration(const dxp::sm5::step::AddResourceStep::InputDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclInputPs;
  instruction.controls.input_interpolation_mode = static_cast<uint32_t>(decl.interpolation_mode);
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeInputOperand(register_index));
  return instruction;
}

auto ShaderProgram::BuildOutputDeclaration(const dxp::sm5::step::AddResourceStep::OutputDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclOutput;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeOutputOperand(register_index));
  return instruction;
}

auto ShaderProgram::BuildSamplerDeclaration(const dxp::sm5::step::AddResourceStep::SamplerDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclSampler;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeSamplerOperand(register_index));
  instruction.sampler_mode = decl.mode;
  return instruction;
}

auto ShaderProgram::BuildRawResourceDeclaration(const dxp::sm5::step::AddResourceStep::RawResourceDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclResourceRaw;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeResourceOperand(register_index));
  return instruction;
}

auto ShaderProgram::BuildStructuredResourceDeclaration(const dxp::sm5::step::AddResourceStep::StructuredResourceDecl& decl) -> Instruction {
  Instruction instruction;
  instruction.opcode = Opcode::DclResourceStructured;
  const uint32_t register_index = decl.register_index.value_or(0U);
  instruction.operands.push_back(MakeResourceOperand(register_index));
  instruction.controls.structure_stride = decl.structure_stride;
  return instruction;
}

auto ShaderProgram::BuildUavDeclaration(const dxp::sm5::step::AddResourceStep::UavDecl& decl) -> Instruction {
  Instruction instruction;
  const uint32_t register_index = decl.register_index.value_or(0U);
  if (decl.kind == step::AddResourceStep::UavKind::Raw) {
    instruction.opcode = Opcode::DclUnorderedAccessViewRaw;
    instruction.operands.push_back(MakeUavOperand(register_index));
    instruction.controls.uav_flags = decl.globally_coherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0;
    return instruction;
  }

  if (decl.kind == step::AddResourceStep::UavKind::Structured) {
    instruction.opcode = Opcode::DclUnorderedAccessViewStructured;
    instruction.operands.push_back(MakeUavOperand(register_index));
    uint32_t flags = decl.globally_coherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0;
    if (decl.has_order_preserving_counter) {
      flags |= D3D11_SB_UAV_HAS_ORDER_PRESERVING_COUNTER;
    }
    instruction.controls.uav_flags = flags;
    instruction.controls.structure_stride = decl.structure_stride;
    return instruction;
  }

  instruction.opcode = Opcode::DclUnorderedAccessViewTyped;
  instruction.operands.push_back(MakeUavOperand(register_index));
  instruction.controls.resource_dimension = static_cast<ResourceDimension>(decl.dimension);
  for (uint32_t component = 0; component < 4; ++component) {
    instruction.controls.resource_return_type[component] = ResourceReturnType::Float;
  }
  instruction.controls.uav_flags = decl.globally_coherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0;
  return instruction;
}

auto ShaderProgram::AddInputDeclaration(const dxp::sm5::step::AddResourceStep::InputDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  uint32_t insert_index = 0;
  for (uint32_t i = 0; i < instructions.size(); ++i) {
    const auto opcode = instructions[i].opcode;
    if ((opcode == Opcode::DclInput || opcode == Opcode::DclInputPs || opcode == Opcode::DclInputPsSiv || opcode == Opcode::DclInputPsSgv) && !instructions[i].operands.empty() && !instructions[i].operands.front().index_entries.empty()) {
      occupied.insert(*instructions[i].operands.front().index_entries[0].immediate_lo);
      insert_index = i + 1;
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const bool reverse = decl.reverse_bind.value_or(false);
  const uint32_t requested = is_auto ? (reverse ? max_bind_point : 0U) : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, reverse, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::InputDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildInputDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddOutputDeclaration(const dxp::sm5::step::AddResourceStep::OutputDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  uint32_t insert_index = 0;
  for (uint32_t i = 0; i < instructions.size(); ++i) {
    const auto opcode = instructions[i].opcode;
    if ((opcode == Opcode::DclOutput || opcode == Opcode::DclOutputSiv || opcode == Opcode::DclOutputSgv) && !instructions[i].operands.empty() && !instructions[i].operands.front().index_entries.empty()) {
      occupied.insert(*instructions[i].operands.front().index_entries[0].immediate_lo);
      insert_index = i + 1;
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const bool reverse = decl.reverse_bind.value_or(false);
  const uint32_t requested = is_auto ? (reverse ? max_bind_point : 0U) : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, reverse, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::OutputDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildOutputDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddTextureDeclaration(const dxp::sm5::step::AddResourceStep::TextureDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& instruction : instructions) {
    const auto opcode = instruction.opcode;
    if ((opcode == Opcode::DclResource || opcode == Opcode::DclResourceRaw || opcode == Opcode::DclResourceStructured) && !instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
      occupied.insert(*instruction.operands.front().index_entries[0].immediate_lo);
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const bool reverse = decl.reverse_bind.value_or(false);
  const uint32_t requested = is_auto ? (reverse ? max_bind_point : 0U) : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, reverse, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::TextureDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D10_SB_OPCODE_DCL_RESOURCE));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildTextureDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddRawResourceDeclaration(const dxp::sm5::step::AddResourceStep::RawResourceDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& instruction : instructions) {
    const auto opcode = instruction.opcode;
    if ((opcode == Opcode::DclResource || opcode == Opcode::DclResourceRaw || opcode == Opcode::DclResourceStructured) && !instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
      occupied.insert(*instruction.operands.front().index_entries.front().immediate_lo);
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const bool reverse = decl.reverse_bind.value_or(false);
  const uint32_t requested = is_auto ? (reverse ? max_bind_point : 0U) : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, reverse, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::RawResourceDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D11_SB_OPCODE_DCL_RESOURCE_RAW));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildRawResourceDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddStructuredResourceDeclaration(const dxp::sm5::step::AddResourceStep::StructuredResourceDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& instruction : instructions) {
    const auto opcode = instruction.opcode;
    if ((opcode == Opcode::DclResource || opcode == Opcode::DclResourceRaw || opcode == Opcode::DclResourceStructured) && !instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
      occupied.insert(*instruction.operands.front().index_entries.front().immediate_lo);
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const bool reverse = decl.reverse_bind.value_or(false);
  const uint32_t requested = is_auto ? (reverse ? max_bind_point : 0U) : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, reverse, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::StructuredResourceDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildStructuredResourceDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddCBufferDeclaration(const dxp::sm5::step::AddResourceStep::CBufferDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& cbuffer : cbuffers) {
    occupied.insert(cbuffer.register_bind_point);
  }

  const bool is_auto = !decl.register_index.has_value();
  const bool reverse = decl.reverse_bind.value_or(false);
  const uint32_t requested = is_auto ? (reverse ? max_bind_point : 0U) : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, reverse, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::CBufferDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildConstantBufferDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddSamplerDeclaration(const dxp::sm5::step::AddResourceStep::SamplerDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& instruction : instructions) {
    const auto opcode = instruction.opcode;
    if (opcode == Opcode::DclSampler && !instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
      occupied.insert(*instruction.operands.front().index_entries.front().immediate_lo);
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const bool reverse = decl.reverse_bind.value_or(false);
  const uint32_t requested = is_auto ? (reverse ? max_bind_point : 0U) : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, reverse, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::SamplerDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D10_SB_OPCODE_DCL_SAMPLER));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index),
                      BuildSamplerDeclaration(resolved_decl));
  return true;
}

auto ShaderProgram::AddUavDeclaration(const dxp::sm5::step::AddResourceStep::UavDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error) -> bool {
  std::unordered_set<uint32_t> occupied;
  for (const auto& instruction : instructions) {
    const auto opcode = instruction.opcode;
    if ((opcode == Opcode::DclUnorderedAccessViewTyped || opcode == Opcode::DclUnorderedAccessViewRaw || opcode == Opcode::DclUnorderedAccessViewStructured) && !instruction.operands.empty() && !instruction.operands.front().index_entries.empty()) {
      occupied.insert(*instruction.operands.front().index_entries.front().immediate_lo);
    }
  }

  const bool is_auto = !decl.register_index.has_value();
  const bool reverse = decl.reverse_bind.value_or(false);
  const uint32_t requested = is_auto ? (reverse ? max_bind_point : 0U) : *decl.register_index;
  if (!AllocateBindPoint(occupied, is_auto, requested, reverse, out_register_index, error)) {
    return false;
  }

  step::AddResourceStep::UavDecl resolved_decl = decl;
  resolved_decl.register_index = out_register_index;
  const uint32_t insert_index = FindInsertAfterLastDeclaration(static_cast<Opcode>(D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED));
  instructions.insert(instructions.begin() + static_cast<ptrdiff_t>(insert_index), BuildUavDeclaration(resolved_decl));
  return true;
}
}  // namespace dxp::sm5
