#pragma once

#include "d3d11TokenizedProgramFormat.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dxp {
namespace sm5 {

using OperandType = D3D10_SB_OPERAND_TYPE;
using OperandModifier = D3D10_SB_OPERAND_MODIFIER;
using OpcodeType = D3D10_SB_OPCODE_TYPE;
using ExtendedOpcodeType = D3D10_SB_EXTENDED_OPCODE_TYPE;
using ProgramType = D3D10_SB_TOKENIZED_PROGRAM_TYPE;

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

struct SamplerDecl {
  uint32_t RegisterBindPoint = 0;
  uint32_t RegisterSpace = 0;
  uint32_t Mode = 0;
  bool Indexed = false;
  std::string CaptureName;
};

struct CBufferDecl {
  uint32_t RegisterBindPoint = 0;
  uint32_t RegisterSpace = 0;
  uint32_t Elements = 1;
  uint32_t AccessPattern = 0;
  std::string Name;
  std::string CaptureName;
};

struct ThreadGroupDecl {
  uint32_t GroupSizeX = 1;
  uint32_t GroupSizeY = 1;
  uint32_t GroupSizeZ = 1;
};

struct GlobalFlags {
  uint32_t Flags = 0;
};

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

const char *GetOpcodeName(Opcode opcode);
bool OpcodeUsesTestBoolean(Opcode opcode);
bool ParseOpcode(const std::string &name, Opcode &opcode);
bool ParseOpcodeWithImplicitTestBoolean(const std::string &name,
                                        Opcode &opcode,
                                        int32_t &implicitTestBoolean);

} // namespace sm5
} // namespace dxp
