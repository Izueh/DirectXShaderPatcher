#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "dxp/sm5/Types.h"

namespace dxp {
namespace sm5 {

struct InstructionCaptureFields {
  bool Opcode = false;
  bool Saturate = false;
  bool TestBoolean = false;
  bool Operands = false;
  bool Immediates = false;

  bool AnySelected() const {
    return Opcode || Saturate || TestBoolean || Operands || Immediates;
  }
};

using OperandType = uint32_t;
using OperandModifier = uint32_t;
using OpcodeType = uint32_t;
using ExtendedOpcodeType = uint32_t;
using ProgramType = uint32_t;

constexpr OperandType kOperandTypeTemp = 0u;
constexpr OperandType kOperandTypeImmediate32 = 4u;
constexpr OperandType kOperandTypeImmediate64 = 5u;
constexpr uint32_t kOperandNumComponents0 = 0u;
constexpr uint32_t kOperandNumComponents1 = 1u;
constexpr uint32_t kOperandNumComponents4 = 2u;
constexpr uint32_t kOperandComponentNoSwizzle = 0xE4u << 4;
constexpr OperandModifier kOperandModifierNone = 0u;
constexpr uint32_t kInterpolationModeUndefined = 0u;
constexpr ProgramType kProgramTypePixelShader = 0u;
constexpr OpcodeType kOpcodeCustomData = 54u;

enum class OperandRole : uint32_t {
  Source = 0,
  Destination = 1,
};

enum class InterpolationMode : uint32_t {
  Undefined = 0,
  Constant = 1,
  Linear = 2,
  LinearCentroid = 3,
  LinearNoperspective = 4,
  LinearNoperspectiveCentroid = 5,
  LinearSample = 6,
  LinearNoperspectiveSample = 7,
};

enum class ResourceDimension : uint32_t {
  Texture1D = 0,
  Texture2D = 1,
  Texture2DMS = 2,
  TextureCube = 3,
  Texture3D = 4,
  Texture2DArray = 5,
  Texture2DMSArray = 6,
  TextureCubeArray = 7,
};

enum class CbufferAccessPattern : uint32_t {
  ImmediateIndexed = 0,
  DynamicIndexed = 1,
};

enum class SamplerMode : uint32_t {
  Default = 0,
  Comparison = 1,
  Mono = 2,
};

constexpr size_t kMaxInstructionOperands = 5;

struct InstructionLayout {
  OpcodeType Opcode = 0;
  OperandRole Roles[kMaxInstructionOperands];
  uint8_t RoleCount = 0;
};

struct Opcode {
  OpcodeType Value = 0u;

  Opcode() = default;
  explicit Opcode(uint32_t v) : Value(v) {}

  explicit operator uint32_t() const { return static_cast<uint32_t>(Value); }

  bool operator==(const Opcode &rhs) const { return Value == rhs.Value; }
  bool operator!=(const Opcode &rhs) const { return Value != rhs.Value; }

  static Opcode Unknown() { return Opcode{static_cast<uint32_t>(0xFFFFFFFFu)}; }
  static Opcode CustomData() { return Opcode{kOpcodeCustomData}; }
};

struct ExtendedOpcode {
  ExtendedOpcodeType Value = 0u;

  ExtendedOpcode() = default;
  explicit ExtendedOpcode(uint32_t v) : Value(v) {}

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
  uint32_t InputInterpolationMode = kInterpolationModeUndefined;
  std::vector<ExtendedOpcode> ExtendedOpCodes;
};

struct Operand {
  enum class IndexRepresentation : uint32_t {
    Immediate32 = 0,
    Immediate64 = 1,
    Relative = 2,
    Immediate32PlusRelative = 3,
    Immediate64PlusRelative = 4,
  };

  struct Index {
    IndexRepresentation Representation = IndexRepresentation::Immediate32;
    bool HasImmediateLo = false;
    uint32_t ImmediateLo = 0;
    bool HasImmediateHi = false;
    uint32_t ImmediateHi = 0;
    std::shared_ptr<Operand> RelativeOperand;
    std::string CaptureName;
    std::string MatchCaptureName;
    std::string ImmediateLoVariableName;
    std::string ImmediateHiVariableName;
    uint32_t ImmediateVariableFamily = 0;
  };

  OperandType Type = kOperandTypeTemp;
  uint32_t NumComponents = kOperandNumComponents4;
  uint32_t ComponentMode = kOperandComponentNoSwizzle;
  std::vector<Index> IndexEntries;
  std::vector<uint32_t> Indices;
  std::string FromHandle;
  OperandModifier Modifier = kOperandModifierNone;
  std::vector<uint32_t> ImmediateValues;
  std::shared_ptr<Operand> RelativeOperand;
  std::vector<uint32_t> RawTokens;
  uint32_t SourceOffset = 0;
  uint32_t SourceLength = 0;
  std::string CaptureName;
  bool CaptureType = false;
  bool CaptureComponents = false;
  bool CaptureModifier = false;
  bool CaptureIndices = false;
  bool CaptureImmediates = false;

  bool HasCaptureFieldProjection() const {
    return CaptureType || CaptureComponents || CaptureModifier ||
           CaptureIndices || CaptureImmediates;
  }

  bool Equals(const CapturedOperand &other) const;
  bool Equals(const Operand &other) const;
  CapturedOperand ToCaptured() const;
  void FromCaptured(const CapturedOperand &cap);
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
  std::string Capture;
  InstructionCaptureFields CaptureFields;

  CapturedInstruction ToCaptured() const;
  void FromCaptured(const CapturedInstruction &cap);
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
  ProgramType ProgramType = kProgramTypePixelShader;
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
bool IsDataOpcode(OpcodeType opcode);
OperandRole GetOperandRole(OpcodeType opcode, size_t operandIndex);
bool ParseOpcodeWithImplicitTestBoolean(const std::string &name,
                                        Opcode &opcode,
                                        int32_t &implicitTestBoolean);

} // namespace sm5
} // namespace dxp
