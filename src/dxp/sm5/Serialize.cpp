#include "dxp/sm5/Serialize.h"

#include <algorithm>
#include <cstring>

namespace dxp {
namespace sm5 {

namespace {

/// Encode operand token0 using WDK macros
static uint32_t EncodeOperandToken0(const Operand &operand) {
  uint32_t token0 = 0;
  
  // Bits [01:00] - Number of components
  token0 |= ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(
    static_cast<D3D10_SB_OPERAND_NUM_COMPONENTS>(operand.NumComponents));
  
  // Bits [11:02] - Component selection (swizzle/mask)
  token0 |= operand.ComponentMode;
  
  // Bits [25:20] - Operand type
  token0 |= ENCODE_D3D10_SB_OPERAND_TYPE(operand.Type);

  size_t indexDims = 0;
  if (operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
      operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) {
    indexDims = 0;
  } else {
    indexDims = std::min(operand.Indices.size(), static_cast<size_t>(3));
    if (operand.RelativeOperand) {
      indexDims = std::min(indexDims + 1, static_cast<size_t>(3));
    }
  }

  // Bits [21:20] - Index dimension
  token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(
    static_cast<D3D10_SB_OPERAND_INDEX_DIMENSION>(
      static_cast<uint32_t>(indexDims)));

  // Bits [24:22], [27:25], [30:28] - Index representation for each dimension
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
      D3D10_SB_OPERAND_INDEX_1D,
      D3D10_SB_OPERAND_INDEX_RELATIVE);
  }

  if (operand.Modifier != D3D10_SB_OPERAND_MODIFIER_NONE) {
    token0 |= ENCODE_D3D10_SB_OPERAND_EXTENDED(1);
  }
  
  return token0;
}

/// Encode operand token0 for a simple register operand (no indices)
static uint32_t EncodeOperandToken0Simple(OperandType type, uint32_t numComponents, 
                                           uint32_t componentMode) {
  uint32_t token0 = 0;
  token0 |= ENCODE_D3D10_SB_OPERAND_NUM_COMPONENTS(
    static_cast<D3D10_SB_OPERAND_NUM_COMPONENTS>(numComponents));
  token0 |= componentMode;
  token0 |= ENCODE_D3D10_SB_OPERAND_TYPE(type);
  token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_0D);
  return token0;
}

/// Encode operand token0 for a register with immediate index
static uint32_t EncodeOperandToken0Indexed(OperandType type, uint32_t numComponents,
                                            uint32_t componentMode, uint32_t indexValue) {
  uint32_t token0 = EncodeOperandToken0Simple(type, numComponents, componentMode);
  token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_DIMENSION(D3D10_SB_OPERAND_INDEX_1D);
  token0 |= ENCODE_D3D10_SB_OPERAND_INDEX_REPRESENTATION(
    D3D10_SB_OPERAND_INDEX_1D,
    D3D10_SB_OPERAND_INDEX_IMMEDIATE32);
  (void)indexValue; // Index value is stored separately in RawTokens
  return token0;
}

/// Encode a single operand to dwords
static std::vector<uint32_t> EncodeOperandImpl(const Operand &operand) {
  std::vector<uint32_t> encoded;
  
  if (!operand.RawTokens.empty()) {
    // Use raw tokens if available (for lossless round-trip)
    return operand.RawTokens;
  }
  
  // Build operand token0
  uint32_t token0 = EncodeOperandToken0(operand);
  encoded.push_back(token0);

  // Extended operand token(s) follow token0 directly.
  if (operand.Modifier != D3D10_SB_OPERAND_MODIFIER_NONE) {
    encoded.push_back(ENCODE_D3D10_SB_EXTENDED_OPERAND_MODIFIER(
      static_cast<D3D10_SB_OPERAND_MODIFIER>(operand.Modifier)));
  }

  if (operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32) {
    for (uint32_t value : operand.ImmediateValues) {
      encoded.push_back(value);
    }
  } else {
    for (uint32_t index : operand.Indices) {
      encoded.push_back(index);
    }
  }

  // Encode relative operand
  if (operand.RelativeOperand) {
    auto relTokens = EncodeOperandImpl(*operand.RelativeOperand);
    encoded.insert(encoded.end(), relTokens.begin(), relTokens.end());
  }
  
  return encoded;
}

static std::vector<uint32_t> EncodeDeclarationOperand(OperandType type,
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

static std::vector<uint32_t> EncodeResourceDeclaration(const Instruction &instruction) {
  if (instruction.Operands.empty() || instruction.Operands.front().Indices.empty()) {
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

  const auto resourceOperand = EncodeDeclarationOperand(
      D3D10_SB_OPERAND_TYPE_RESOURCE, {instruction.Operands.front().Indices.front()});
  const uint32_t length = 1u + static_cast<uint32_t>(resourceOperand.size()) + 1u;

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

static std::vector<uint32_t> EncodeConstantBufferDeclaration(const Instruction &instruction) {
  if (instruction.Operands.empty() || instruction.Operands.front().Indices.size() < 2) {
    return {};
  }

  uint32_t accessPattern = D3D10_SB_CONSTANT_BUFFER_IMMEDIATE_INDEXED;
  if (!instruction.RawTokens.empty()) {
    accessPattern = DECODE_D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN(instruction.RawTokens[0]);
  }

  const auto cbufferOperand = EncodeDeclarationOperand(
      D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER,
      {instruction.Operands.front().Indices[0], instruction.Operands.front().Indices[1]});
  const uint32_t length = 1u + static_cast<uint32_t>(cbufferOperand.size());

  std::vector<uint32_t> encoded;
  encoded.reserve(length);
  encoded.push_back(ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) |
                    ENCODE_D3D10_SB_D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN(accessPattern) |
                    ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(length));
  encoded.insert(encoded.end(), cbufferOperand.begin(), cbufferOperand.end());
  return encoded;
}

static std::vector<uint32_t> EncodeTempDeclaration(const Instruction &instruction) {
  uint32_t tempCount = 0;
  if (!instruction.Operands.empty() && !instruction.Operands.front().Indices.empty()) {
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

/// Encode instruction token0 using WDK macros
static uint32_t EncodeInstructionToken0(const Instruction &instruction, uint32_t totalDwords) {
  uint32_t token0 = 0;
  const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
  
  // Bits [10:00] - Opcode type
  token0 |= ENCODE_D3D10_SB_OPCODE_TYPE(opcode);
  
  // Bits [30:24] - Instruction length
  token0 |= ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(totalDwords);
  
  // Bits [13] - Saturate
  if (instruction.Controls.Saturate) {
    token0 |= ENCODE_D3D10_SB_INSTRUCTION_SATURATE(1);
  }
  
  // Bits [18] - Test boolean
  if (instruction.Controls.HasTestBoolean) {
    token0 |= ENCODE_D3D10_SB_INSTRUCTION_TEST_BOOLEAN(
      static_cast<D3D10_SB_INSTRUCTION_TEST_BOOLEAN>(instruction.Controls.TestBoolean));
  }
  
  // Bits [19:22] - Precise values
  if (instruction.Controls.PreciseValues) {
    token0 |= ENCODE_D3D11_SB_INSTRUCTION_PRECISE_VALUES(instruction.Controls.PreciseValues);
  }
  
  // Bits [11:14] - Resinfo return type
  if (instruction.Controls.ResinfoReturnType) {
    token0 |= ENCODE_D3D10_SB_RESINFO_INSTRUCTION_RETURN_TYPE(
      static_cast<D3D10_SB_RESINFO_INSTRUCTION_RETURN_TYPE>(instruction.Controls.ResinfoReturnType));
  }

  if ((opcode == D3D10_SB_OPCODE_DCL_INPUT_PS ||
       opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV) &&
      instruction.Controls.HasInputInterpolationMode) {
    token0 |= ENCODE_D3D10_SB_INPUT_INTERPOLATION_MODE(
        static_cast<D3D10_SB_INTERPOLATION_MODE>(instruction.Controls.InputInterpolationMode));
  }
  
  return token0;
}

} // namespace

std::vector<uint32_t> EncodeOperand(const Operand &operand) {
  return EncodeOperandImpl(operand);
}

std::vector<uint32_t> EncodeInstruction(const Instruction &instruction) {
  // If raw tokens are available, use them for lossless round-trip
  if (!instruction.RawTokens.empty()) {
    return instruction.RawTokens;
  }

  if (static_cast<OpcodeType>(instruction.Opcode) == D3D10_SB_OPCODE_DCL_RESOURCE) {
    return EncodeResourceDeclaration(instruction);
  }

  if (static_cast<OpcodeType>(instruction.Opcode) == D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) {
    return EncodeConstantBufferDeclaration(instruction);
  }

  if (static_cast<OpcodeType>(instruction.Opcode) == D3D10_SB_OPCODE_DCL_TEMPS) {
    return EncodeTempDeclaration(instruction);
  }
  
  // Calculate total length
  uint32_t totalLength = 1; // Opcode token
  
  // Count operand dwords
  for (const auto &operand : instruction.Operands) {
    if (!operand.RawTokens.empty()) {
      totalLength += static_cast<uint32_t>(operand.RawTokens.size());
    } else {
      // Estimate operand size
      totalLength += 1; // Base operand token
      if (operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32) {
        totalLength += static_cast<uint32_t>(operand.ImmediateValues.size());
      } else {
        totalLength += static_cast<uint32_t>(operand.Indices.size());
      }
      if (operand.RelativeOperand) {
        totalLength += 2; // Rough estimate for relative operand
      }
      if (operand.Modifier != D3D10_SB_OPERAND_MODIFIER_NONE) {
        totalLength += 1; // Extended operand token
      }
    }
  }
  
  // Encode instruction token0
  uint32_t token0 = EncodeInstructionToken0(instruction, totalLength);
  
  std::vector<uint32_t> encoded;
  encoded.push_back(token0);
  
  // Encode operands
  for (const auto &operand : instruction.Operands) {
    auto operandTokens = EncodeOperandImpl(operand);
    encoded.insert(encoded.end(), operandTokens.begin(), operandTokens.end());
  }
  
  return encoded;
}

bool SerializeProgram(const Program &program, std::vector<uint8_t> &outBytes) {
  std::vector<uint32_t> words;
  
  // Encode version token
  words.push_back(ENCODE_D3D10_SB_TOKENIZED_PROGRAM_VERSION_TOKEN(
    static_cast<D3D10_SB_TOKENIZED_PROGRAM_TYPE>(program.ProgramType),
    program.MajorVersion,
    program.MinorVersion));
  words.push_back(0); // Length placeholder - will be filled in
  
  // Encode instructions
  for (const auto &instruction : program.Instructions) {
    auto encoded = EncodeInstruction(instruction);
    words.insert(words.end(), encoded.begin(), encoded.end());
  }
  
  // Set length
  words[1] = static_cast<uint32_t>(words.size());
  
  // Convert to bytes
  outBytes.resize(words.size() * sizeof(uint32_t));
  std::memcpy(outBytes.data(), words.data(), outBytes.size());
  return true;
}

bool RebuildShaderChunk(const Program &program, std::vector<uint8_t> &outData) {
  return SerializeProgram(program, outData);
}

} // namespace sm5
} // namespace dxp
