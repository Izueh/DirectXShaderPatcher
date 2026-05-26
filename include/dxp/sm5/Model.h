#pragma once

#include "d3d11TokenizedProgramFormat.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dxp {
namespace sm5 {

/// @brief Alias for the operand kind enum used by SM5 bytecode.
using OperandType = D3D10_SB_OPERAND_TYPE;
/// @brief Alias for the operand modifier enum used by SM5 bytecode.
using OperandModifier = D3D10_SB_OPERAND_MODIFIER;
/// @brief Alias for the opcode enum used by SM5 bytecode.
using OpcodeType = D3D10_SB_OPCODE_TYPE;
/// @brief Alias for the extended opcode enum used by SM5 bytecode.
using ExtendedOpcodeType = D3D10_SB_EXTENDED_OPCODE_TYPE;
/// @brief Alias for the shader program type enum used by SM5 bytecode.
using ProgramType = D3D10_SB_TOKENIZED_PROGRAM_TYPE;

/// @brief Wraps an SM5 opcode value with typed conversions.
struct Opcode {
  OpcodeType Value = D3D10_SB_OPCODE_ADD;

  Opcode() = default;
  explicit Opcode(OpcodeType v) : Value(v) {}
  explicit Opcode(uint32_t v)
      : Value(static_cast<OpcodeType>(v & D3D10_SB_OPCODE_TYPE_MASK)) {}

  explicit operator OpcodeType() const { return Value; }
  explicit operator uint32_t() const { return static_cast<uint32_t>(Value); }

  bool operator==(const Opcode &rhs) const { return Value == rhs.Value; }
  bool operator!=(const Opcode &rhs) const { return Value != rhs.Value; }

  static Opcode Unknown() { return Opcode{static_cast<uint32_t>(0xFFFFFFFFu)}; }
  static Opcode CustomData() { return Opcode{D3D10_SB_OPCODE_CUSTOMDATA}; }
};

/// @brief Wraps an SM5 extended opcode value with typed conversions.
struct ExtendedOpcode {
  ExtendedOpcodeType Value = D3D10_SB_EXTENDED_OPCODE_EMPTY;

  ExtendedOpcode() = default;
  explicit ExtendedOpcode(ExtendedOpcodeType v) : Value(v) {}
  explicit ExtendedOpcode(uint32_t v)
      : Value(static_cast<ExtendedOpcodeType>(
            v & D3D10_SB_EXTENDED_OPCODE_TYPE_MASK)) {}

  explicit operator ExtendedOpcodeType() const { return Value; }
  explicit operator uint32_t() const { return static_cast<uint32_t>(Value); }

  bool operator==(const ExtendedOpcode &rhs) const {
    return Value == rhs.Value;
  }
  bool operator!=(const ExtendedOpcode &rhs) const {
    return Value != rhs.Value;
  }
};

/// @brief Stores optional opcode control bits decoded from an instruction.
struct OpcodeControls {
  bool Saturate = false;
  bool HasTestBoolean = false;
  uint32_t TestBoolean = 0;
  uint32_t PreciseValues = 0;
  uint32_t ResinfoReturnType = 0;
  uint32_t SyncFlags = 0;
  bool HasInputInterpolationMode = false;
  uint32_t InputInterpolationMode = D3D10_SB_INTERPOLATION_UNDEFINED;
  std::vector<ExtendedOpcode> ExtendedOpCodes;
};

/// @brief Represents one decoded operand from the instruction stream.
struct Operand {
  OperandType Type = D3D10_SB_OPERAND_TYPE_TEMP;
  uint32_t NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
  uint32_t ComponentMode = D3D10_SB_OPERAND_4_COMPONENT_NOSWIZZLE;
  std::vector<uint32_t> Indices;
  std::string BindHandle;
  std::string StateTempName;
  OperandModifier Modifier = D3D10_SB_OPERAND_MODIFIER_NONE;
  std::vector<uint32_t> ImmediateValues;
  std::shared_ptr<Operand> RelativeOperand;
  std::vector<uint32_t> RawTokens;
  uint32_t SourceOffset = 0;
  uint32_t SourceLength = 0;
  std::string CaptureName;
  std::string ScratchName;
};

/// @brief Represents one decoded instruction from the instruction stream.
struct Instruction {
  dxp::sm5::Opcode Opcode = dxp::sm5::Opcode::Unknown();
  OpcodeControls Controls;
  uint32_t LengthInDwords = 0;
  std::vector<Operand> Operands;
  std::vector<uint32_t> CustomData;
  uint32_t SourceOffset = 0;
  uint32_t SourceLength = 0;
  std::vector<uint32_t> RawTokens;
  std::string Name;
};

/// @brief Describes a declared shader resource binding.
struct ResourceDecl {
  uint32_t RegisterBindPoint = 0;
  uint32_t RegisterSpace = 0;
  uint32_t Dimension = 0;
  uint32_t NumElements = 1;
  uint32_t ReturnFieldType = 0;
  uint32_t SampleCount = 1;
  bool Indexed = false;
  std::string CaptureName;
};

/// @brief Describes a declared sampler binding.
struct SamplerDecl {
  uint32_t RegisterBindPoint = 0;
  uint32_t RegisterSpace = 0;
  uint32_t Mode = 0;
  bool Indexed = false;
  std::string CaptureName;
};

/// @brief Describes a declared constant buffer binding.
struct CBufferDecl {
  uint32_t RegisterBindPoint = 0;
  uint32_t RegisterSpace = 0;
  uint32_t Elements = 1;
  uint32_t AccessPattern = 0;
  std::string Name;
  std::string CaptureName;
};

/// @brief Describes a compute shader thread-group declaration.
struct ThreadGroupDecl {
  uint32_t GroupSizeX = 1;
  uint32_t GroupSizeY = 1;
  uint32_t GroupSizeZ = 1;
};

/// @brief Stores decoded global shader flags.
struct GlobalFlags {
  uint32_t Flags = 0;
};

/// @brief Owns the decoded representation of an SM5 program.
struct Program {
  ProgramType ProgramType = D3D10_SB_PIXEL_SHADER;
  uint32_t MajorVersion = 0;
  uint32_t MinorVersion = 0;
  uint32_t TotalLengthInDwords = 0;

  std::vector<Instruction> Instructions;

  std::vector<ResourceDecl> Resources;
  std::vector<SamplerDecl> Samplers;
  std::vector<CBufferDecl> CBuffers;
  std::vector<ThreadGroupDecl> ThreadGroups;
  GlobalFlags GlobalFlags;

  uint32_t TempCount = 0;
  uint32_t TempSize = 0;
  std::vector<uint32_t> IndexableTemps;
};

/// @brief Returns a human-readable name for an opcode.
/// @param opcode Opcode to format.
/// @return A static opcode name string.
const char *GetOpcodeName(Opcode opcode);

/// @brief Returns whether an opcode carries the test_boolean control bit.
/// @param opcode Opcode to inspect.
/// @return `true` when the opcode uses the zero/nonzero test control.
bool OpcodeUsesTestBoolean(Opcode opcode);

/// @brief Parses an opcode name into its typed representation.
/// @param name Opcode name to parse.
/// @param opcode Receives the parsed opcode on success.
/// @return `true` when the name maps to a known opcode.
bool ParseOpcode(const std::string &name, Opcode &opcode);

/// @brief Parses an opcode name and resolves any implicit test_boolean alias.
/// @param name Opcode name or assembly-style alias to parse.
/// @param opcode Receives the parsed opcode on success.
/// @param implicitTestBoolean Receives `-1` when no alias implied a test,
/// otherwise the decoded zero/nonzero control.
/// @return `true` when the name maps to a known opcode or supported alias.
bool ParseOpcodeWithImplicitTestBoolean(const std::string &name,
                                        Opcode &opcode,
                                        int32_t &implicitTestBoolean);

} // namespace sm5
} // namespace dxp
