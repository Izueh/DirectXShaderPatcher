#include "Parse.h"

#include "d3d11TokenizedProgramFormat.hpp"

#include <algorithm>
#include <cstring>

namespace dxp {
namespace sm5 {

namespace {

static uint32_t ReadDword(const uint8_t *data, uint32_t byteOffset) {
  uint32_t value = 0;
  std::memcpy(&value, data + byteOffset, sizeof(value));
  return value;
}

static OpcodeType DecodeOpcode(uint32_t token0) {
  return static_cast<OpcodeType>(DECODE_D3D10_SB_OPCODE_TYPE(token0));
}

static uint32_t DecodeInstructionLength(uint32_t token0) {
  const uint32_t len = DECODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(token0);
  return len == 0 ? 1u : len;
}

static ProgramType DecodeProgramType(uint32_t versionToken) {
  return static_cast<ProgramType>(
      DECODE_D3D10_SB_TOKENIZED_PROGRAM_TYPE(versionToken));
}

static uint32_t DecodeMajorVersion(uint32_t versionToken) {
  return DECODE_D3D10_SB_TOKENIZED_PROGRAM_MAJOR_VERSION(versionToken);
}

static uint32_t DecodeMinorVersion(uint32_t versionToken) {
  return DECODE_D3D10_SB_TOKENIZED_PROGRAM_MINOR_VERSION(versionToken);
}

static OperandType DecodeOperandType(uint32_t token0) {
  return static_cast<OperandType>(DECODE_D3D10_SB_OPERAND_TYPE(token0));
}

static uint32_t DecodeNumComponents(uint32_t token0) {
  return DECODE_D3D10_SB_OPERAND_NUM_COMPONENTS(token0);
}

static uint32_t DecodeComponentMode(uint32_t token0) {
  const uint32_t selectionMode =
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(token0);

  switch (static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(
      selectionMode)) {
  case D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE:
    return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
               D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
           DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(token0);
  case D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE: {
    const uint32_t swizzle =
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(token0);
    // NOSWIZZLE is SWIZZLE_MODE with X-X-X-X pattern.
    if (swizzle == D3D10_SB_OPERAND_4_COMPONENT_NOSWIZZLE) {
      return D3D10_SB_OPERAND_4_COMPONENT_NOSWIZZLE;
    }
    return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
               D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) |
        swizzle;
  }
  case D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE:
    return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
               D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) |
           DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(token0);
  default:
    return 0;
  }
}

static bool IsOpcodeExtended(uint32_t token0) {
  return DECODE_IS_D3D10_SB_OPCODE_EXTENDED(token0) != 0;
}

static bool IsOperandExtended(uint32_t token0) {
  return DECODE_IS_D3D10_SB_OPERAND_EXTENDED(token0) != 0;
}

static OpcodeControls ParseOpcodeControls(const uint8_t *data,
                                          uint32_t instructionStart,
                                          uint32_t instructionLength,
                                          uint32_t token0) {
  OpcodeControls controls;
  const auto opcode =
      static_cast<OpcodeType>(DECODE_D3D10_SB_OPCODE_TYPE(token0));
  controls.Saturate =
      DECODE_IS_D3D10_SB_INSTRUCTION_SATURATE_ENABLED(token0) != 0;
  controls.HasTestBoolean = OpcodeUsesTestBoolean(Opcode{opcode});
  if (controls.HasTestBoolean) {
    controls.TestBoolean = DECODE_D3D10_SB_INSTRUCTION_TEST_BOOLEAN(token0);
  }
  controls.PreciseValues = DECODE_D3D11_SB_INSTRUCTION_PRECISE_VALUES(token0);
  if (opcode == D3D10_SB_OPCODE_RESINFO) {
    controls.ResinfoReturnType =
        DECODE_D3D10_SB_RESINFO_INSTRUCTION_RETURN_TYPE(token0);
  }
  if (opcode == D3D11_SB_OPCODE_SYNC) {
    controls.SyncFlags = DECODE_D3D11_SB_SYNC_FLAGS(token0);
  }
  if (opcode == D3D10_SB_OPCODE_DCL_INPUT_PS ||
      opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV) {
    controls.HasInputInterpolationMode = true;
    controls.InputInterpolationMode =
        static_cast<uint32_t>(DECODE_D3D10_SB_INPUT_INTERPOLATION_MODE(token0));
  }

  if (!IsOpcodeExtended(token0)) {
    return controls;
  }

  uint32_t cursor = instructionStart + 1;
  const uint32_t end = instructionStart + instructionLength;
  while (cursor < end) {
    const uint32_t extToken = ReadDword(data, cursor * 4);
    controls.ExtendedOpCodes.emplace_back(static_cast<ExtendedOpcodeType>(
        DECODE_D3D10_SB_EXTENDED_OPCODE_TYPE(extToken)));
    ++cursor;
    if (!DECODE_IS_D3D10_SB_OPCODE_EXTENDED(extToken)) {
      break;
    }
  }

  return controls;
}

static Operand ParseOperand(const uint8_t *data, uint32_t totalDwords,
                            uint32_t instructionEnd, uint32_t &cursor) {
  Operand operand;
  if (cursor >= totalDwords || cursor >= instructionEnd) {
    return operand;
  }

  const uint32_t start = cursor;
  const uint32_t token0 = ReadDword(data, cursor * 4);
  operand.Type = DecodeOperandType(token0);
  operand.NumComponents = DecodeNumComponents(token0);
  operand.ComponentMode = DecodeComponentMode(token0);
  operand.RawTokens.push_back(token0);
  ++cursor;

  if (IsOperandExtended(token0) && cursor < instructionEnd) {
    uint32_t extToken = ReadDword(data, cursor * 4);
    operand.RawTokens.push_back(extToken);
    if (DECODE_D3D10_SB_EXTENDED_OPERAND_TYPE(extToken) ==
        D3D10_SB_EXTENDED_OPERAND_MODIFIER) {
      operand.Modifier = static_cast<OperandModifier>(
          DECODE_D3D10_SB_OPERAND_MODIFIER(extToken));
    }
    ++cursor;
    while (DECODE_IS_D3D10_SB_OPERAND_DOUBLE_EXTENDED(extToken) &&
           cursor < instructionEnd) {
      extToken = ReadDword(data, cursor * 4);
      operand.RawTokens.push_back(extToken);
      ++cursor;
    }
  }

  const uint32_t indexDim = DECODE_D3D10_SB_OPERAND_INDEX_DIMENSION(token0);
  for (uint32_t dim = 0; dim < indexDim && dim < 3 && cursor < instructionEnd;
       ++dim) {
    const auto indexRep = static_cast<D3D10_SB_OPERAND_INDEX_REPRESENTATION>(
        DECODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(
            static_cast<D3D10_SB_OPERAND_INDEX_DIMENSION>(dim), token0));

    Operand::Index indexEntry;

    if (indexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE32) {
      const uint32_t imm = ReadDword(data, cursor * 4);
      indexEntry.Representation = Operand::IndexRepresentation::Immediate32;
      indexEntry.HasImmediateLo = true;
      indexEntry.ImmediateLo = imm;
      operand.Indices.push_back(imm);
      operand.RawTokens.push_back(imm);
      operand.IndexEntries.push_back(std::move(indexEntry));
      ++cursor;
      continue;
    }

    if (indexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE64 &&
        (cursor + 1) < instructionEnd) {
      const uint32_t lo = ReadDword(data, cursor * 4);
      const uint32_t hi = ReadDword(data, (cursor + 1) * 4);
      indexEntry.Representation = Operand::IndexRepresentation::Immediate64;
      indexEntry.HasImmediateLo = true;
      indexEntry.ImmediateLo = lo;
      indexEntry.HasImmediateHi = true;
      indexEntry.ImmediateHi = hi;
      operand.Indices.push_back(lo);
      operand.Indices.push_back(hi);
      operand.RawTokens.push_back(lo);
      operand.RawTokens.push_back(hi);
      operand.IndexEntries.push_back(std::move(indexEntry));
      cursor += 2;
      continue;
    }

    if (indexRep == D3D10_SB_OPERAND_INDEX_RELATIVE ||
        indexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE ||
        indexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE) {
      if (indexRep == D3D10_SB_OPERAND_INDEX_RELATIVE) {
        indexEntry.Representation = Operand::IndexRepresentation::Relative;
      } else if (indexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE) {
        indexEntry.Representation =
            Operand::IndexRepresentation::Immediate32PlusRelative;
      } else {
        indexEntry.Representation =
            Operand::IndexRepresentation::Immediate64PlusRelative;
      }

      if (indexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE &&
          cursor < instructionEnd) {
        const uint32_t imm = ReadDword(data, cursor * 4);
        indexEntry.HasImmediateLo = true;
        indexEntry.ImmediateLo = imm;
        operand.Indices.push_back(imm);
        operand.RawTokens.push_back(imm);
        ++cursor;
      } else if (indexRep == D3D10_SB_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE &&
                 (cursor + 1) < instructionEnd) {
        const uint32_t lo = ReadDword(data, cursor * 4);
        const uint32_t hi = ReadDword(data, (cursor + 1) * 4);
        indexEntry.HasImmediateLo = true;
        indexEntry.ImmediateLo = lo;
        indexEntry.HasImmediateHi = true;
        indexEntry.ImmediateHi = hi;
        operand.Indices.push_back(lo);
        operand.Indices.push_back(hi);
        operand.RawTokens.push_back(lo);
        operand.RawTokens.push_back(hi);
        cursor += 2;
      }

      Operand rel = ParseOperand(data, totalDwords, instructionEnd, cursor);
      auto relativePtr = std::make_shared<Operand>(std::move(rel));
      indexEntry.RelativeOperand = relativePtr;
      operand.RelativeOperand = relativePtr;
      operand.IndexEntries.push_back(std::move(indexEntry));
      continue;
    }

    operand.IndexEntries.push_back(std::move(indexEntry));
  }

  if (operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32) {
    uint32_t immediateCount = 0;
    if (operand.NumComponents == D3D10_SB_OPERAND_1_COMPONENT) {
      immediateCount = 1;
    } else if (operand.NumComponents == D3D10_SB_OPERAND_4_COMPONENT) {
      immediateCount = 4;
    }
    for (uint32_t i = 0; i < immediateCount && cursor < instructionEnd; ++i) {
      const uint32_t imm = ReadDword(data, cursor * 4);
      operand.ImmediateValues.push_back(imm);
      operand.RawTokens.push_back(imm);
      ++cursor;
    }
  }

  operand.SourceOffset = start;
  operand.SourceLength = cursor - start;
  return operand;
}

static uint32_t CountExtendedOpcodeTokens(const uint8_t *data,
                                          uint32_t instructionStart,
                                          uint32_t instructionLength,
                                          uint32_t token0) {
  if (!IsOpcodeExtended(token0)) {
    return 0;
  }

  uint32_t count = 0;
  uint32_t cursor = instructionStart + 1;
  const uint32_t end = instructionStart + instructionLength;
  while (cursor < end) {
    const uint32_t extToken = ReadDword(data, cursor * 4);
    ++count;
    ++cursor;
    if (!DECODE_IS_D3D10_SB_OPCODE_EXTENDED(extToken)) {
      break;
    }
  }

  return count;
}

static void UpdateDeclarationOverlay(Program &program,
                                     const Instruction &instruction) {
  const auto opcode = static_cast<OpcodeType>(instruction.Opcode);

  if (opcode == D3D10_SB_OPCODE_DCL_TEMPS && !instruction.Operands.empty()) {
    if (!instruction.Operands.front().Indices.empty()) {
      program.TempCount = instruction.Operands.front().Indices.front();
      program.TempSize = program.TempCount * 4;
    }
  }

  if ((opcode == D3D10_SB_OPCODE_DCL_RESOURCE ||
       opcode == D3D11_SB_OPCODE_DCL_RESOURCE_RAW ||
       opcode == D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED) &&
      !instruction.Operands.empty()) {
    ResourceDecl decl;
    const auto &op = instruction.Operands.front();
    if (!op.Indices.empty()) {
      decl.RegisterBindPoint = op.Indices.front();
    }
    program.Resources.push_back(decl);
  }

  if (opcode == D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER &&
      !instruction.Operands.empty()) {
    CBufferDecl decl;
    const auto &op = instruction.Operands.front();
    if (!op.Indices.empty()) {
      decl.RegisterBindPoint = op.Indices.front();
    }
    program.CBuffers.push_back(decl);
  }

  if (opcode == D3D10_SB_OPCODE_DCL_SAMPLER && !instruction.Operands.empty()) {
    SamplerDecl decl;
    const auto &op = instruction.Operands.front();
    if (!op.Indices.empty()) {
      decl.RegisterBindPoint = op.Indices.front();
    }
    program.Samplers.push_back(decl);
  }

  if (opcode == D3D11_SB_OPCODE_DCL_THREAD_GROUP &&
      instruction.Operands.size() >= 3) {
    ThreadGroupDecl decl;
    if (!instruction.Operands[0].ImmediateValues.empty()) {
      decl.GroupSizeX = instruction.Operands[0].ImmediateValues[0];
    }
    if (!instruction.Operands[1].ImmediateValues.empty()) {
      decl.GroupSizeY = instruction.Operands[1].ImmediateValues[0];
    }
    if (!instruction.Operands[2].ImmediateValues.empty()) {
      decl.GroupSizeZ = instruction.Operands[2].ImmediateValues[0];
    }
    program.ThreadGroups.push_back(decl);
  }

  if (opcode == D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS) {
    program.GlobalFlags.Flags = instruction.Controls.SyncFlags;
  }
}

} // namespace

static bool ParseProgram(const uint8_t *data, uint32_t size, Program &program);

static std::pair<const uint8_t *, uint32_t>
GetShaderBytecode(const Container &container) {
  const DxbcChunk *chunk = container.GetShaderChunk();
  if (chunk == nullptr || chunk->Data.empty()) {
    return {nullptr, 0};
  }
  return {chunk->Data.data(), static_cast<uint32_t>(chunk->Data.size())};
}

bool ParseShaderChunk(const Container &container, Program &program) {
  const auto [bytecode, byteCount] = GetShaderBytecode(container);
  if (bytecode == nullptr || byteCount == 0) {
    return false;
  }
  return ParseProgram(bytecode, byteCount, program);
}

static bool ParseProgram(const uint8_t *data, uint32_t size, Program &program) {
  if (data == nullptr || size < 8) {
    return false;
  }

  program = Program{};

  const uint32_t versionToken = ReadDword(data, 0);
  const uint32_t lengthToken = ReadDword(data, 4);

  program.ProgramType = DecodeProgramType(versionToken);
  program.MajorVersion = DecodeMajorVersion(versionToken);
  program.MinorVersion = DecodeMinorVersion(versionToken);
  program.TotalLengthInDwords = lengthToken;

  const uint32_t totalDwords = std::min(lengthToken, size / 4);
  uint32_t cursor = 2;

  while (cursor < totalDwords) {
    const uint32_t instructionStart = cursor;
    const uint32_t token0 = ReadDword(data, cursor * 4);
    const OpcodeType opcode = DecodeOpcode(token0);
    const uint32_t length = DecodeInstructionLength(token0);

    if (length == 0 || (instructionStart + length) > totalDwords) {
      return false;
    }

    Instruction instruction;
    instruction.Opcode = Opcode{opcode};
    instruction.LengthInDwords = length;
    instruction.SourceOffset = instructionStart;
    instruction.SourceLength = length;
    instruction.Controls =
        ParseOpcodeControls(data, instructionStart, length, token0);

    instruction.RawTokens.reserve(length);
    for (uint32_t i = 0; i < length; ++i) {
      instruction.RawTokens.push_back(
          ReadDword(data, (instructionStart + i) * 4));
    }

    if (opcode == D3D10_SB_OPCODE_CUSTOMDATA) {
      instruction.CustomData = instruction.RawTokens;
      program.Instructions.push_back(std::move(instruction));
      cursor += length;
      continue;
    }

    if (opcode == D3D10_SB_OPCODE_DCL_TEMPS) {
      if (length < 2) {
        return false;
      }

      Operand operand;
      operand.Type = D3D10_SB_OPERAND_TYPE_TEMP;
      operand.NumComponents = D3D10_SB_OPERAND_0_COMPONENT;
      operand.ComponentMode = 0;
      operand.Indices.push_back(ReadDword(data, (instructionStart + 1) * 4));
      operand.SourceOffset = instructionStart + 1;
      operand.SourceLength = 1;
      instruction.Operands.push_back(std::move(operand));

      UpdateDeclarationOverlay(program, instruction);
      program.Instructions.push_back(std::move(instruction));
      cursor += length;
      continue;
    }

    const uint32_t extendedOpcodeCount =
        CountExtendedOpcodeTokens(data, instructionStart, length, token0);

    uint32_t operandCursor = instructionStart + 1 + extendedOpcodeCount;
    const uint32_t instructionEnd = instructionStart + length;
    while (operandCursor < instructionEnd) {
      const uint32_t before = operandCursor;
      Operand operand =
          ParseOperand(data, totalDwords, instructionEnd, operandCursor);
      if (operand.SourceLength == 0 || operandCursor <= before) {
        break;
      }
      instruction.Operands.push_back(std::move(operand));
    }

    UpdateDeclarationOverlay(program, instruction);
    program.Instructions.push_back(std::move(instruction));
    cursor += length;
  }

  return true;
}

} // namespace sm5
} // namespace dxp
