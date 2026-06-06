#include "Serialize.h"

#include "d3d11TokenizedProgramFormat.hpp"

#include <algorithm>
#include <cstring>

namespace dxp {
namespace sm5 {

namespace {

static uint32_t EncodeOperandToken0(const Operand &operand) {
  uint32_t token0 = 0;

  token0 |= ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(
      static_cast<D3D10_SB_OPERAND_NUM_COMPONENTS>(operand.NumComponents));

  token0 |= operand.ComponentMode;

  token0 |= ENCODE_D3D10_SB_OPERAND_TYPE(operand.Type);

  size_t indexDims = 0;
  if (operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
      operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) {
    indexDims = 0;
  } else if (!operand.IndexEntries.empty()) {
    indexDims = std::min(operand.IndexEntries.size(), static_cast<size_t>(3));
  } else {
    indexDims = std::min(operand.Indices.size(), static_cast<size_t>(3));
    if (operand.RelativeOperand) {
      indexDims = std::min(indexDims + 1, static_cast<size_t>(3));
    }
  }

  token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(
      static_cast<D3D10_SB_OPERAND_INDEX_DIMENSION>(
          static_cast<uint32_t>(indexDims)));

  if (!operand.IndexEntries.empty()) {
    for (size_t dim = 0; dim < indexDims; ++dim) {
      uint32_t rep = D3D10_SB_OPERAND_INDEX_IMMEDIATE32;
      switch (operand.IndexEntries[dim].Representation) {
      case Operand::IndexRepresentation::Immediate32:
        rep = D3D10_SB_OPERAND_INDEX_IMMEDIATE32;
        break;
      case Operand::IndexRepresentation::Immediate64:
        rep = D3D10_SB_OPERAND_INDEX_IMMEDIATE64;
        break;
      case Operand::IndexRepresentation::Relative:
        rep = D3D10_SB_OPERAND_INDEX_RELATIVE;
        break;
      case Operand::IndexRepresentation::Immediate32PlusRelative:
        rep = D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE;
        break;
      case Operand::IndexRepresentation::Immediate64PlusRelative:
        rep = D3D10_SB_OPERAND_INDEX_IMMEDIATE64_PLUS_RELATIVE;
        break;
      }
      token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(
          static_cast<D3D10_SB_OPERAND_INDEX_DIMENSION>(dim),
          static_cast<D3D10_SB_OPERAND_INDEX_REPRESENTATION>(rep));
    }
  } else {
    for (size_t dim = 0; dim < operand.Indices.size() && dim < 3; ++dim) {
      uint32_t rep = static_cast<uint32_t>(
          (dim + 1 == operand.Indices.size() && operand.RelativeOperand)
              ? D3D10_SB_OPERAND_INDEX_IMMEDIATE32_PLUS_RELATIVE
              : D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
      token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(
          static_cast<D3D10_SB_OPERAND_INDEX_DIMENSION>(dim),
          static_cast<D3D10_SB_OPERAND_INDEX_REPRESENTATION>(rep));
    }

    if (operand.RelativeOperand && operand.ImmediateValues.empty()) {
      token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(
          D3D10_SB_OPERAND_INDEX_1D, D3D10_SB_OPERAND_INDEX_RELATIVE);
    }
  }

  if (operand.Modifier != D3D10_SB_OPERAND_MODIFIER_NONE) {
    token0 |= ENCODE_D3D10_SB_OPERAND_EXTENDED(1);
  }

  return token0;
}

static uint32_t EncodeOperandToken0Simple(OperandType type,
                                          uint32_t numComponents,
                                          uint32_t componentMode) {
  uint32_t token0 = 0;
  token0 |= ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(
      static_cast<D3D10_SB_OPERAND_NUM_COMPONENTS>(numComponents));
  token0 |= componentMode;
  token0 |= ENCODE_D3D10_SB_OPERAND_TYPE(type);
  token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_0D);
  return token0;
}

static uint32_t EncodeOperandToken0Indexed(OperandType type,
                                           uint32_t numComponents,
                                           uint32_t componentMode,
                                           uint32_t indexValue) {
  uint32_t token0 =
      EncodeOperandToken0Simple(type, numComponents, componentMode);
  token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_1D);
  token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(
      D3D10_SB_OPERAND_INDEX_1D, D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
  (void)indexValue;
  return token0;
}

static std::vector<uint32_t> EncodeOperandImpl(const Operand &operand) {
  std::vector<uint32_t> encoded;

  if (!operand.RawTokens.empty()) {

    return operand.RawTokens;
  }

  uint32_t token0 = EncodeOperandToken0(operand);
  encoded.push_back(token0);

  if (operand.Modifier != D3D10_SB_OPERAND_MODIFIER_NONE) {
    encoded.push_back(ENCODE_D3D10_SB_EXTENDED_OPERAND_MODIFIER(
        static_cast<D3D10_SB_OPERAND_MODIFIER>(operand.Modifier)));
  }

  if (operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32) {
    for (uint32_t value : operand.ImmediateValues) {
      encoded.push_back(value);
    }
  } else if (!operand.IndexEntries.empty()) {
    for (const Operand::Index &index : operand.IndexEntries) {
      switch (index.Representation) {
      case Operand::IndexRepresentation::Immediate32:
        if (index.HasImmediateLo) {
          encoded.push_back(index.ImmediateLo);
        }
        break;
      case Operand::IndexRepresentation::Immediate64:
        if (index.HasImmediateLo) {
          encoded.push_back(index.ImmediateLo);
        }
        if (index.HasImmediateHi) {
          encoded.push_back(index.ImmediateHi);
        }
        break;
      case Operand::IndexRepresentation::Relative:
        break;
      case Operand::IndexRepresentation::Immediate32PlusRelative:
        if (index.HasImmediateLo) {
          encoded.push_back(index.ImmediateLo);
        }
        break;
      case Operand::IndexRepresentation::Immediate64PlusRelative:
        if (index.HasImmediateLo) {
          encoded.push_back(index.ImmediateLo);
        }
        if (index.HasImmediateHi) {
          encoded.push_back(index.ImmediateHi);
        }
        break;
      }

      if (index.RelativeOperand) {
        auto relTokens = EncodeOperandImpl(*index.RelativeOperand);
        encoded.insert(encoded.end(), relTokens.begin(), relTokens.end());
      }
    }
  } else {
    for (uint32_t index : operand.Indices) {
      encoded.push_back(index);
    }
  }

  if (operand.RelativeOperand && operand.IndexEntries.empty()) {
    auto relTokens = EncodeOperandImpl(*operand.RelativeOperand);
    encoded.insert(encoded.end(), relTokens.begin(), relTokens.end());
  }

  return encoded;
}

static std::vector<uint32_t>
EncodeDeclarationOperand(OperandType type,
                         const std::vector<uint32_t> &indices) {
  Operand operand;
  operand.Type = type;
  operand.NumComponents = D3D10_SB_OPERAND_0_COMPONENT;
  operand.ComponentMode = 0;
  operand.Indices = indices;
  return EncodeOperandImpl(operand);
}

static uint32_t EncodeFloatResourceReturnTypeToken() {
  return ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 0) |
         ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 1) |
         ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 2) |
         ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 3);
}

static std::vector<uint32_t>
EncodeResourceDeclaration(const Instruction &instruction) {
  if (instruction.Operands.empty() ||
      instruction.Operands.front().Indices.empty()) {
    return {};
  }

  const ResourceDecl *decl = nullptr;
  if (!instruction.Name.empty()) {
    (void)decl;
  }

  uint32_t dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D;
  uint32_t returnTypeToken = EncodeFloatResourceReturnTypeToken();
  if (instruction.RawTokens.size() >= 4) {
    dimension = DECODE_D3D10_SB_RESOURCE_DIMENSION(instruction.RawTokens[0]);
    returnTypeToken = instruction.RawTokens.back();
  }

  const auto resourceOperand =
      EncodeDeclarationOperand(D3D10_SB_OPERAND_TYPE_RESOURCE,
                               {instruction.Operands.front().Indices.front()});
  const uint32_t length =
      1u + static_cast<uint32_t>(resourceOperand.size()) + 1u;

  std::vector<uint32_t> encoded;
  encoded.reserve(length);
  encoded.push_back(ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_RESOURCE) |
                    ENCODE_D3D10_SB_RESOURCE_DIMENSION(
                        static_cast<D3D10_SB_RESOURCE_DIMENSION>(dimension)) |
                    ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(length));
  encoded.insert(encoded.end(), resourceOperand.begin(), resourceOperand.end());
  encoded.push_back(returnTypeToken);
  return encoded;
}

static std::vector<uint32_t>
EncodeConstantBufferDeclaration(const Instruction &instruction) {
  if (instruction.Operands.empty() ||
      instruction.Operands.front().Indices.size() < 2) {
    return {};
  }

  uint32_t accessPattern = D3D10_SB_CONSTANT_BUFFER_IMMEDIATE_INDEXED;
  if (!instruction.RawTokens.empty()) {
    accessPattern = DECODE_D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN(
        instruction.RawTokens[0]);
  }

  const auto cbufferOperand =
      EncodeDeclarationOperand(D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER,
                               {instruction.Operands.front().Indices[0],
                                instruction.Operands.front().Indices[1]});
  const uint32_t length = 1u + static_cast<uint32_t>(cbufferOperand.size());

  std::vector<uint32_t> encoded;
  encoded.reserve(length);
  encoded.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) |
      ENCODE_D3D10_SB_D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN(accessPattern) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(length));
  encoded.insert(encoded.end(), cbufferOperand.begin(), cbufferOperand.end());
  return encoded;
}

static std::vector<uint32_t>
EncodeTempDeclaration(const Instruction &instruction) {
  uint32_t tempCount = 0;
  if (!instruction.Operands.empty() &&
      !instruction.Operands.front().Indices.empty()) {
    tempCount = instruction.Operands.front().Indices.front();
  } else if (instruction.RawTokens.size() >= 2) {
    tempCount = instruction.RawTokens[1];
  }

  return {
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_TEMPS) |
          ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(2),
      tempCount,
  };
}

static uint32_t EncodeInstructionToken0(const Instruction &instruction,
                                        uint32_t totalDwords) {
  uint32_t token0 = 0;
  const auto opcode = static_cast<OpcodeType>(instruction.Opcode);

  token0 |= ENCODE_D3D10_SB_OPCODE_TYPE(opcode);

  token0 |= ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(totalDwords);

  if (instruction.Controls.Saturate) {
    token0 |= ENCODE_D3D10_SB_INSTRUCTION_SATURATE(1);
  }

  if (instruction.Controls.HasTestBoolean) {
    token0 |= ENCODE_D3D10_SB_INSTRUCTION_TEST_BOOLEAN(
        static_cast<D3D10_SB_INSTRUCTION_TEST_BOOLEAN>(
            instruction.Controls.TestBoolean));
  }

  if (instruction.Controls.PreciseValues) {
    token0 |= ENCODE_D3D11_SB_INSTRUCTION_PRECISE_VALUES(
        instruction.Controls.PreciseValues);
  }

  if (instruction.Controls.ResinfoReturnType) {
    token0 |= ENCODE_D3D10_SB_RESINFO_INSTRUCTION_RETURN_TYPE(
        static_cast<D3D10_SB_RESINFO_INSTRUCTION_RETURN_TYPE>(
            instruction.Controls.ResinfoReturnType));
  }

  if ((opcode == D3D10_SB_OPCODE_DCL_INPUT_PS ||
       opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV) &&
      instruction.Controls.HasInputInterpolationMode) {
    token0 |= ENCODE_D3D10_SB_INPUT_INTERPOLATION_MODE(
        static_cast<D3D10_SB_INTERPOLATION_MODE>(
            instruction.Controls.InputInterpolationMode));
  }

  return token0;
}

} // namespace

std::vector<uint32_t> EncodeOperand(const Operand &operand) {
  return EncodeOperandImpl(operand);
}

std::vector<uint32_t> EncodeInstruction(const Instruction &instruction) {

  if (!instruction.RawTokens.empty()) {
    return instruction.RawTokens;
  }

  if (static_cast<OpcodeType>(instruction.Opcode) ==
      D3D10_SB_OPCODE_DCL_RESOURCE) {
    return EncodeResourceDeclaration(instruction);
  }

  if (static_cast<OpcodeType>(instruction.Opcode) ==
      D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) {
    return EncodeConstantBufferDeclaration(instruction);
  }

  if (static_cast<OpcodeType>(instruction.Opcode) ==
      D3D10_SB_OPCODE_DCL_TEMPS) {
    return EncodeTempDeclaration(instruction);
  }

  uint32_t totalLength = 1;

  for (const auto &operand : instruction.Operands) {
    if (!operand.RawTokens.empty()) {
      totalLength += static_cast<uint32_t>(operand.RawTokens.size());
    } else {

      totalLength += 1;
      if (operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32) {
        totalLength += static_cast<uint32_t>(operand.ImmediateValues.size());
      } else {
        totalLength += static_cast<uint32_t>(operand.Indices.size());
      }
      if (operand.RelativeOperand) {
        totalLength += 2;
      }
      if (operand.Modifier != D3D10_SB_OPERAND_MODIFIER_NONE) {
        totalLength += 1;
      }
    }
  }

  uint32_t token0 = EncodeInstructionToken0(instruction, totalLength);

  std::vector<uint32_t> encoded;
  encoded.push_back(token0);

  for (const auto &operand : instruction.Operands) {
    auto operandTokens = EncodeOperandImpl(operand);
    encoded.insert(encoded.end(), operandTokens.begin(), operandTokens.end());
  }

  return encoded;
}

static bool SerializeProgram(const Program &program,
                             std::vector<uint8_t> &outBytes) {
  std::vector<uint32_t> words;

  words.push_back(ENCODE_D3D10_SB_TOKENIZED_PROGRAM_VERSION_TOKEN(
      static_cast<D3D10_SB_TOKENIZED_PROGRAM_TYPE>(program.ProgramType),
      program.MajorVersion, program.MinorVersion));
  words.push_back(0);

  for (const auto &instruction : program.Instructions) {
    auto encoded = EncodeInstruction(instruction);
    words.insert(words.end(), encoded.begin(), encoded.end());
  }

  words[1] = static_cast<uint32_t>(words.size());

  outBytes.resize(words.size() * sizeof(uint32_t));
  std::memcpy(outBytes.data(), words.data(), outBytes.size());
  return true;
}

bool RebuildShaderChunk(const Program &program, std::vector<uint8_t> &outData) {
  return SerializeProgram(program, outData);
}

} // namespace sm5
} // namespace dxp
