#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dxp {
namespace sm5 {

using OperandType = uint32_t;
using OperandModifier = uint32_t;
using OpcodeType = uint32_t;
using ExtendedOpcodeType = uint32_t;
using ProgramType = uint32_t;

// Mirrored SM5 token values used by public API defaults.
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

/// @brief Wraps an SM5 opcode value with typed conversions.
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

/// @brief Wraps an SM5 extended opcode value with typed conversions.
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

/// @brief Stores optional opcode control bits decoded from an instruction.
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

/// @brief Represents one decoded operand from the instruction stream.
struct Operand {
  /// @brief Encodes how an operand index is represented in the token stream.
  enum class IndexRepresentation : uint32_t {
    Immediate32 = 0,             ///< 32-bit immediate value
    Immediate64 = 1,             ///< 64-bit immediate value (two DWORDs)
    Relative = 2,                ///< Relative addressing via a sub-operand
    Immediate32PlusRelative = 3, ///< 32-bit immediate plus relative
    Immediate64PlusRelative = 4, ///< 64-bit immediate plus relative
  };

  /// @brief One ordered index slot of a decoded operand.
  ///
  /// Operands can carry zero, one, or two index slots depending on operand type
  /// (e.g., a `temp` has one index for the register number; an
  /// `indexable_temp` has two). `IndexEntries` holds the authoritative ordered
  /// list; `Indices` is a flattened immediate-value view used by
  /// serialization and binding helpers.
  struct Index {
    /// Encoding used for this index in the token stream.
    IndexRepresentation Representation = IndexRepresentation::Immediate32;
    bool HasImmediateLo = false;
    uint32_t ImmediateLo = 0; ///< Low 32-bit immediate (e.g., register number)
    bool HasImmediateHi = false;
    uint32_t ImmediateHi = 0; ///< High 32-bit immediate for 64-bit indices
    /// Sub-operand used for relative addressing; null when not relative.
    std::shared_ptr<Operand> RelativeOperand;
    /// Non-empty on match templates: stores the matched immediate under this
    /// name in `MatchResult::CapturedOperandIndexValues`. Corresponds to the
    /// `capture` field in YAML index objects.
    std::string CaptureName;
    /// Non-empty on emit templates: resolves the emitted immediate from a
    /// previously captured index value of this name. Corresponds to the
    /// `match_capture` field on emit-side index objects in YAML.
    std::string MatchCaptureName;
  };

  OperandType Type = kOperandTypeTemp;
  uint32_t NumComponents = kOperandNumComponents4;
  uint32_t ComponentMode = kOperandComponentNoSwizzle;
  std::vector<Index> IndexEntries;
  std::vector<uint32_t> Indices;
  std::string BindHandle;
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
