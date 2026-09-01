#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "value_types/indirect.h"

namespace dxp::sm5::model {

/// @brief Number of components an operand uses.
/// Mirrors @c D3D10_SB_OPERAND_NUM_COMPONENTS from the DXBC token format.
enum class NumComponents : std::uint8_t {
  Zero = 0,  ///< @c D3D10_SB_OPERAND_NUM_COMPONENTS_ZERO
  One = 1,   ///< @c D3D10_SB_OPERAND_NUM_COMPONENTS_ONE
  Four = 2,  ///< @c D3D10_SB_OPERAND_NUM_COMPONENTS_FOUR
  N = 3,     ///< @c D3D10_SB_OPERAND_NUM_COMPONENTS_N
};

/// @brief Selection mode for operand component selection.
/// Mirrors @c D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE from the DXBC token format.
enum class SelectionMode : std::uint8_t {
  Mask = 0,     ///< @c D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE
  Swizzle = 1,  ///< @c D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE
  Select = 2,   ///< @c D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE
};

/// @brief Operand component selector configuration.
struct Components {
  NumComponents num_components = NumComponents::Four;
  SelectionMode selection_mode = SelectionMode::Mask;
  std::string value;

  bool operator==(const Components& rhs) const {
    return num_components == rhs.num_components && selection_mode == rhs.selection_mode && value == rhs.value;
  }
};

/// @brief Operand register type in DXBC instructions.
/// Mirrors @c D3D10_SB_OPERAND_TYPE / @c D3D11_SB_OPERAND_TYPE from the DXBC token format.
enum class OperandType : std::uint8_t {
  Temp = 0,                            ///< @c D3D10_SB_OPERAND_TYPE_TEMP
  Input = 1,                           ///< @c D3D10_SB_OPERAND_TYPE_INPUT
  Output = 2,                          ///< @c D3D10_SB_OPERAND_TYPE_OUTPUT
  IndexableTemp = 3,                   ///< @c D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP
  Immediate32 = 4,                     ///< @c D3D10_SB_OPERAND_TYPE_IMMEDIATE32
  Immediate64 = 5,                     ///< @c D3D10_SB_OPERAND_TYPE_IMMEDIATE64
  Sampler = 6,                         ///< @c D3D10_SB_OPERAND_TYPE_SAMPLER
  Resource = 7,                        ///< @c D3D10_SB_OPERAND_TYPE_RESOURCE
  CBuffer = 8,                         ///< @c D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER
  ImmediateConstantBuffer = 9,         ///< @c D3D10_SB_OPERAND_TYPE_IMMEDIATE_CONSTANT_BUFFER
  Label = 10,                          ///< @c D3D10_SB_OPERAND_TYPE_LABEL
  InputPrimitiveId = 11,               ///< @c D3D10_SB_OPERAND_TYPE_INPUT_PRIMITIVEID
  OutputDepth = 12,                    ///< @c D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH
  Null = 13,                           ///< @c D3D10_SB_OPERAND_TYPE_NULL
  Rasterizer = 14,                     ///< @c D3D10_SB_OPERAND_TYPE_RASTERIZER
  OutputCoverageMask = 15,             ///< @c D3D10_SB_OPERAND_TYPE_OUTPUT_COVERAGE_MASK
  Stream = 16,                         ///< @c D3D11_SB_OPERAND_TYPE_STREAM
  FunctionBody = 17,                   ///< @c D3D11_SB_OPERAND_TYPE_FUNCTION_BODY
  FunctionTable = 18,                  ///< @c D3D11_SB_OPERAND_TYPE_FUNCTION_TABLE
  Interface = 19,                      ///< @c D3D11_SB_OPERAND_TYPE_INTERFACE
  FunctionInput = 20,                  ///< @c D3D11_SB_OPERAND_TYPE_FUNCTION_INPUT
  FunctionOutput = 21,                 ///< @c D3D11_SB_OPERAND_TYPE_FUNCTION_OUTPUT
  OutputControlPointId = 22,           ///< @c D3D11_SB_OPERAND_TYPE_OUTPUT_CONTROL_POINT_ID
  InputForkInstanceId = 23,            ///< @c D3D11_SB_OPERAND_TYPE_INPUT_FORK_INSTANCE_ID
  InputJoinInstanceId = 24,            ///< @c D3D11_SB_OPERAND_TYPE_INPUT_JOIN_INSTANCE_ID
  InputControlPoint = 25,              ///< @c D3D11_SB_OPERAND_TYPE_INPUT_CONTROL_POINT
  OutputControlPoint = 26,             ///< @c D3D11_SB_OPERAND_TYPE_OUTPUT_CONTROL_POINT
  InputPatchConstant = 27,             ///< @c D3D11_SB_OPERAND_TYPE_INPUT_PATCH_CONSTANT
  InputDomainPoint = 28,               ///< @c D3D11_SB_OPERAND_TYPE_INPUT_DOMAIN_POINT
  ThisPointer = 29,                    ///< @c D3D11_SB_OPERAND_TYPE_THIS_POINTER
  UAV = 30,                            ///< @c D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW
  ThreadGroupSharedMemory = 31,        ///< @c D3D11_SB_OPERAND_TYPE_THREAD_GROUP_SHARED_MEMORY
  InputThreadId = 32,                  ///< @c D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID
  InputThreadGroupId = 33,             ///< @c D3D11_SB_OPERAND_TYPE_INPUT_THREAD_GROUP_ID
  InputThreadIdInGroup = 34,           ///< @c D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP
  InputCoverageMask = 35,              ///< @c D3D11_SB_OPERAND_TYPE_INPUT_COVERAGE_MASK
  InputThreadIdInGroupFlattened = 36,  ///< @c D3D11_SB_OPERAND_TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED
  InputGsInstanceId = 37,              ///< @c D3D11_SB_OPERAND_TYPE_INPUT_GS_INSTANCE_ID
  OutputDepthGreaterEqual = 38,        ///< @c D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_GREATER_EQUAL
  OutputDepthLessEqual = 39,           ///< @c D3D11_SB_OPERAND_TYPE_OUTPUT_DEPTH_LESS_EQUAL
  CycleCounter = 40,                   ///< @c D3D11_SB_OPERAND_TYPE_CYCLE_COUNTER
};

/// @brief Operand modifier flag in DXBC instructions.
/// Mirrors @c D3D10_SB_OPERAND_MODIFIER from the DXBC token format.
enum class OperandModifier : std::uint8_t {
  None = 0,    ///< @c D3D10_SB_OPERAND_MODIFIER_NONE
  Neg = 1,     ///< @c D3D10_SB_OPERAND_MODIFIER_NEG
  Abs = 2,     ///< @c D3D10_SB_OPERAND_MODIFIER_ABS
  AbsNeg = 3,  ///< @c D3D10_SB_OPERAND_MODIFIER_ABSNEG
};

/// @brief Resource shape declared for a texture/UAV register.
/// Mirrors @c D3D10_SB_RESOURCE_DIMENSION from the DXBC token format.
enum class ResourceDimension : std::uint8_t {
  Unknown = 0,
  Buffer = 1,
  Texture1D = 2,
  Texture2D = 3,
  Texture2DMS = 4,
  Texture3D = 5,
  TextureCube = 6,
  Texture1DArray = 7,
  Texture2DArray = 8,
  Texture2DMSArray = 9,
  TextureCubeArray = 10,
  RawBuffer = 11,
  StructuredBuffer = 12,
};

/// @brief Data type a resource read returns.
/// Mirrors @c D3D10_SB_RESOURCE_RETURN_TYPE from the DXBC token format.
enum class ResourceReturnType : std::uint8_t {
  UNorm = 1,
  SNorm = 2,
  SInt = 3,
  UInt = 4,
  Float = 5,
  Mixed = 6,
  Double = 7,
  Continued = 8,
  Unused = 9,
};

/// @brief Extended opcode type in DXBC instructions.
/// Mirrors @c D3D10_SB_EXTENDED_OPCODE_TYPE from the DXBC token format.
enum class ExtendedOpcodeType : std::uint8_t {
  Empty = 0,           ///< @c D3D10_SB_EXTENDED_OPCODE_EMPTY
  SampleControls = 1,  ///< @c D3D10_SB_EXTENDED_OPCODE_SAMPLE_CONTROLS
  ResourceDim = 2,     ///< @c D3D11_SB_EXTENDED_OPCODE_RESOURCE_DIM
  ResourceType = 3,    ///< @c D3D11_SB_EXTENDED_OPCODE_RESOURCE_RETURN_TYPE
};

/// Bits of the DXBC operand token that select the component mode and its value:
/// selection mode (bits 2-3) plus the mask/swizzle/select value (bits 4-11),
/// exactly as they appear in the token (ground truth per
/// d3d11TokenizedProgramFormat.hpp). See ParseDecodeComponentMode/Encode.
constexpr uint32_t kComponentModeBits = 0xFFC;

enum class OperandRole : std::uint8_t {
  Source = 0,
  Destination = 1,
};

/// @brief Interpolation mode for input-signature declarations.
enum class InterpolationMode : std::uint8_t {
  Undefined = 0,
  Constant = 1,
  Linear = 2,
  LinearCentroid = 3,
  LinearNoperspective = 4,
  LinearNoperspectiveCentroid = 5,
  LinearSample = 6,
  LinearNoperspectiveSample = 7,
  Invalid = 8,
};

/// @brief Sampler mode for DCL_SAMPLER declarations.
/// Mirrors @c D3D10_SB_SAMPLER_MODE from the DXBC token format.
enum class SamplerMode : std::uint8_t {
  Default = 0,     ///< @c D3D10_SB_SAMPLER_DEFAULT
  Comparison = 1,  ///< @c D3D10_SB_SAMPLER_COMPARISON
  Mono = 2,        ///< @c D3D10_SB_SAMPLER_MONO
};

/// @brief Semantic name of SIV/SGV signature declarations (the NameToken — what
/// HLSL spells SV_Position, SV_PrimitiveID, etc.). Mirrors @c D3D10_SB_NAME.
enum class SignatureSemantic : std::uint8_t {
  Undefined = 0,
  Position = 1,
  ClipDistance = 2,
  CullDistance = 3,
  RenderTargetArrayIndex = 4,
  ViewportArrayIndex = 5,
  VertexId = 6,
  PrimitiveId = 7,
  InstanceId = 8,
  IsFrontFace = 9,
  SampleIndex = 10,
  FinalQuadUEq0EdgeTessfactor = 11,
  FinalQuadVEq0EdgeTessfactor = 12,
  FinalQuadUEq1EdgeTessfactor = 13,
  FinalQuadVEq1EdgeTessfactor = 14,
  FinalQuadUInsideTessfactor = 15,
  FinalQuadVInsideTessfactor = 16,
  FinalTriUEq0EdgeTessfactor = 17,
  FinalTriVEq0EdgeTessfactor = 18,
  FinalTriWEq0EdgeTessfactor = 19,
  FinalTriInsideTessfactor = 20,
  FinalLineDetailTessfactor = 21,
  FinalLineDensityTessfactor = 22,
};

/// @brief Constant buffer access pattern.
/// Mirrors @c D3D11_SB_CONSTANT_BUFFER_ACCESS_PATTERN from the DXBC token format
/// (value-identical; verified by static_assert in Model.cpp).
enum class CbufferAccessPattern : std::uint8_t {
  ImmediateIndexed = 0,  ///< @c D3D10_SB_CONSTANT_BUFFER_IMMEDIATE_INDEXED
  DynamicIndexed = 1,    ///< @c D3D10_SB_CONSTANT_BUFFER_DYNAMIC_INDEXED
};

constexpr size_t MaxInstructionOperands = 8;

/// Number of tokenized opcodes in the @c Opcode enum (values 0..235).
constexpr size_t kOpcodeCount = 236;

enum class Opcode : uint32_t {

  Add = 0,
  And = 1,
  Break = 2,
  BreakC = 3,
  Call = 4,
  CallC = 5,
  Case = 6,
  Continue = 7,
  ContinueC = 8,
  Cut = 9,
  Default = 10,
  DerivRTX = 11,
  DerivRTY = 12,
  Discard = 13,
  Div = 14,
  DP2 = 15,
  DP3 = 16,
  DP4 = 17,
  Else = 18,
  Emit = 19,
  EmitThenCut = 20,
  EndIf = 21,
  EndLoop = 22,
  EndSwitch = 23,
  Eq = 24,
  Exp = 25,
  Frc = 26,
  Ftoi = 27,
  Ftou = 28,
  Ge = 29,
  IAdd = 30,
  If = 31,
  IEq = 32,
  IGe = 33,
  ILt = 34,
  IMad = 35,
  IMax = 36,
  IMin = 37,
  IMul = 38,
  INe = 39,
  INeg = 40,
  IShl = 41,
  IShr = 42,
  Itof = 43,
  Label = 44,
  Ld = 45,
  LdMs = 46,
  Log = 47,
  Loop = 48,
  Lt = 49,
  Mad = 50,
  Min = 51,
  Max = 52,
  CustomData = 53,
  Mov = 54,
  MovC = 55,
  Mul = 56,
  Ne = 57,
  Nop = 58,
  Not = 59,
  Or = 60,
  Resinfo = 61,
  Ret = 62,
  RetC = 63,
  RoundNe = 64,
  RoundNi = 65,
  RoundPi = 66,
  RoundZ = 67,
  Rsq = 68,
  Sample = 69,
  SampleC = 70,
  SampleCLz = 71,
  SampleL = 72,
  SampleD = 73,
  SampleB = 74,
  Sqrt = 75,
  Switch = 76,
  Sincos = 77,
  UDiv = 78,
  ULt = 79,
  UGe = 80,
  UMul = 81,
  UMad = 82,
  UMax = 83,
  UMin = 84,
  UShr = 85,
  Utof = 86,
  Xor = 87,
  DclResource = 88,
  DclConstantBuffer = 89,
  DclSampler = 90,
  DclIndexRange = 91,
  DclGsOutputPrimitiveTopology = 92,
  DclGsInputPrimitive = 93,
  DclMaxOutputVertexCount = 94,
  DclInput = 95,
  DclInputSgv = 96,
  DclInputSiv = 97,
  DclInputPs = 98,
  DclInputPsSgv = 99,
  DclInputPsSiv = 100,
  DclOutput = 101,
  DclOutputSgv = 102,
  DclOutputSiv = 103,
  DclTemps = 104,
  DclIndexableTemp = 105,
  DclGlobalFlags = 106,
  Reserved0 = 107,

  Lod = 108,
  Gather4 = 109,
  SamplePos = 110,
  SampleInfo = 111,
  Reserved1 = 112,

  HsDecls = 113,
  HsControlPointPhase = 114,
  HsForkPhase = 115,
  HsJoinPhase = 116,
  EmitStream = 117,
  CutStream = 118,
  EmitThenCutStream = 119,
  InterfaceCall = 120,
  Bufinfo = 121,
  DerivRTXCoarse = 122,
  DerivRTXFine = 123,
  DerivRTYCoarse = 124,
  DerivRTYFine = 125,
  Gather4C = 126,
  Gather4PO = 127,
  Gather4POC = 128,
  Rcp = 129,
  F32ToF16 = 130,
  F16ToF32 = 131,
  UAddC = 132,
  USubb = 133,
  CountBits = 134,
  FirstBitHi = 135,
  FirstBitLo = 136,
  FirstBitSHI = 137,
  UBFE = 138,
  IBFE = 139,
  BFI = 140,
  BFRev = 141,
  SwapC = 142,
  DclStream = 143,
  DclFunctionBody = 144,
  DclFunctionTable = 145,
  DclInterface = 146,
  DclInputControlPointCount = 147,
  DclOutputControlPointCount = 148,
  DclTessDomain = 149,
  DclTessPartitioning = 150,
  DclTessOutputPrimitive = 151,
  DclHsMaxTessfactor = 152,
  DclHsForkPhaseInstanceCount = 153,
  DclHsJoinPhaseInstanceCount = 154,
  DclThreadGroup = 155,
  DclUnorderedAccessViewTyped = 156,
  DclUnorderedAccessViewRaw = 157,
  DclUnorderedAccessViewStructured = 158,
  DclThreadGroupSharedMemoryRaw = 159,
  DclThreadGroupSharedMemoryStructured = 160,
  DclResourceRaw = 161,
  DclResourceStructured = 162,
  LdUavTyped = 163,
  StoreUavTyped = 164,
  LdRaw = 165,
  StoreRaw = 166,
  LdStructured = 167,
  StoreStructured = 168,
  AtomicAnd = 169,
  AtomicOr = 170,
  AtomicXor = 171,
  AtomicCmpStore = 172,
  AtomicIAdd = 173,
  AtomicIMax = 174,
  AtomicIMin = 175,
  AtomicUMax = 176,
  AtomicUMin = 177,
  ImmAtomicAlloc = 178,
  ImmAtomicConsume = 179,
  ImmAtomicIAdd = 180,
  ImmAtomicAnd = 181,
  ImmAtomicOr = 182,
  ImmAtomicXor = 183,
  ImmAtomicExch = 184,
  ImmAtomicCmpExch = 185,
  ImmAtomicIMax = 186,
  ImmAtomicIMin = 187,
  ImmAtomicUMax = 188,
  ImmAtomicUMin = 189,
  Sync = 190,
  DAdd = 191,
  DMax = 192,
  DMin = 193,
  DMul = 194,
  DEq = 195,
  DGe = 196,
  DLt = 197,
  DNe = 198,
  DMov = 199,
  DMovC = 200,
  DToF = 201,
  FToD = 202,
  EvalSnapped = 203,
  EvalSampleIndex = 204,
  EvalCentroid = 205,
  DclGsInstanceCount = 206,
  Abort = 207,
  DebugBreak = 208,
  Reserved0209 = 209,

  DDiv = 210,
  DFma = 211,
  DRcp = 212,
  MSAD = 213,
  DToI = 214,
  DToU = 215,
  IToD = 216,
  UToD = 217,
  Reserved0218 = 218,

  Gather4Feedback = 219,
  Gather4CFeedback = 220,
  Gather4POFeedback = 221,
  Gather4POCFeedback = 222,
  LdFeedback = 223,
  LdMsFeedback = 224,
  LdUavTypedFeedback = 225,
  LdRawFeedback = 226,
  LdStructuredFeedback = 227,
  SampleLFeedback = 228,
  SampleCLzFeedback = 229,
  SampleClampFeedback = 230,
  SampleBClampFeedback = 231,
  SampleDClampFeedback = 232,
  SampleCClampFeedback = 233,
  CheckAccessFullyMapped = 234,
  Reserved0235 = 235,

  /// DXBC unknown-opcode sentinel (0xFFFFFFFF).
  Unknown = 0xFFFFFFFFU,
};

/// @brief Structured payloads for the three known extended-opcode types
/// (mirrors the D3D11 tokenized-format layout, same bit semantics as
/// dxbc-spirv's SampleControlToken / ResourceDimToken / ResourceTypeToken,
/// MIT, Copyright (c) 2025 Philip Rebohle).
struct SampleControlsPayload {
  int32_t u = 0;  ///< Immediate U offset (4-bit 2's complement).
  int32_t v = 0;  ///< Immediate V offset (4-bit 2's complement).
  int32_t w = 0;  ///< Immediate W offset (4-bit 2's complement).
};

struct ResourceDimPayload {
  uint32_t dimension = 0;         ///< D3D10_SB_RESOURCE_DIMENSION.
  uint32_t structure_stride = 0;  ///< Byte stride (structured buffer only).
};

struct ResourceReturnTypePayload {
  std::array<uint32_t, 4> component_types{};  ///< D3D10_SB_RESOURCE_RETURN_TYPE per component.
};

inline Opcode OpcodeUnknown() {
  return Opcode::Unknown;
}

inline bool operator==(const Opcode& lhs, uint32_t rhs) {
  return static_cast<uint32_t>(lhs) == rhs;
}
inline bool operator==(uint32_t lhs, const Opcode& rhs) {
  return lhs == static_cast<uint32_t>(rhs);
}

/// @brief Returns true if this opcode is a declaration (dcl_*): the base
/// D3D10 declaration range (DclResource..DclGlobalFlags) or the D3D11
/// declaration range (DclStream..DclResourceStructured).
inline bool OpcodeIsDeclaration(Opcode opcode) noexcept {
  const auto val = static_cast<uint32_t>(opcode);
  return (val >= static_cast<uint32_t>(Opcode::DclResource) && val <= static_cast<uint32_t>(Opcode::DclGlobalFlags)) || (val >= static_cast<uint32_t>(Opcode::DclStream) && val <= static_cast<uint32_t>(Opcode::DclResourceStructured));
}

/// @brief Whether this opcode's declaration carries a trailing NameToken
/// (dcl_input_sgv/siv, dcl_input_ps_sgv/siv, dcl_output_sgv/siv).
inline bool OpcodeUsesSemanticName(Opcode opcode) noexcept {
  const auto val = static_cast<uint32_t>(opcode);
  return val == static_cast<uint32_t>(Opcode::DclInputSgv) || val == static_cast<uint32_t>(Opcode::DclInputSiv)
         || val == static_cast<uint32_t>(Opcode::DclInputPsSgv) || val == static_cast<uint32_t>(Opcode::DclInputPsSiv)
         || val == static_cast<uint32_t>(Opcode::DclOutputSgv) || val == static_cast<uint32_t>(Opcode::DclOutputSiv);
}

/// @brief Returns true if this opcode supports the test-boolean flag.
inline bool OpcodeUsesTestBoolean(Opcode opcode) noexcept {
  const auto val = static_cast<uint32_t>(opcode);
  return val == static_cast<uint32_t>(Opcode::BreakC) || val == static_cast<uint32_t>(Opcode::CallC) || val == static_cast<uint32_t>(Opcode::ContinueC) || val == static_cast<uint32_t>(Opcode::Discard) || val == static_cast<uint32_t>(Opcode::If) || val == static_cast<uint32_t>(Opcode::RetC);
}

/// Scalar/operand data type an opcode expects in an operand slot.
/// Mirrors dxbc-spirv's ir::ScalarType usage in its instruction layout table.
enum class OperandScalarType : std::uint8_t {
  Unknown = 0,  ///< no type constraint (typeless operand)
  F32 = 1,      ///< 32-bit float
  U32 = 2,      ///< 32-bit unsigned integer
  I32 = 3,      ///< 32-bit signed integer
  F64 = 4,      ///< 64-bit float (double)
  Bool = 5,     ///< boolean predicate
  Texture = 6,  ///< shader resource view operand
  Sampler = 7,  ///< sampler operand
  Uav = 8,      ///< unordered access view operand
  CBuffer = 9,  ///< constant buffer operand
};

struct InstructionLayout {
  std::array<OperandRole, MaxInstructionOperands> roles{};
  std::array<OperandScalarType, MaxInstructionOperands> types{};
  uint8_t role_count = 0;
};

/// @brief Extended opcode token.
struct ExtendedOpcode {
  uint32_t value = 0U;

  ExtendedOpcode() = default;
  explicit ExtendedOpcode(uint32_t value_in) : value(value_in) {}

  explicit operator uint32_t() const {
    return value;
  }

  bool operator==(const ExtendedOpcode& rhs) const {
    return value == rhs.value;
  }
  bool operator!=(const ExtendedOpcode& rhs) const {
    return value != rhs.value;
  }
};

struct OpcodeControls {
  bool saturate = false;
  std::optional<uint32_t> test_boolean;
  uint32_t precise_values = 0;
  uint32_t resinfo_return_type = 0;
  uint32_t sync_flags = 0;
  std::optional<uint32_t> input_interpolation_mode;
  std::optional<CbufferAccessPattern> access_pattern;
  uint32_t access_pattern_raw = 0;
  std::optional<SamplerMode> mode;
  std::optional<ResourceDimension> resource_dimension;
  std::array<std::optional<ResourceReturnType>, 4> resource_return_type = {std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  uint32_t uav_flags = 0;
  uint32_t structure_stride = 0;
  /// @brief Decoded NameToken of SIV/SGV signature declarations (dcl_input_sgv/siv,
  /// dcl_input_ps_sgv/siv, dcl_output_sgv/siv). Absent for other opcodes.
  std::optional<SignatureSemantic> semantic_name;
  std::vector<ExtendedOpcode> extended_op_codes;
};

struct Operand {
  enum class IndexRepresentation : std::uint8_t {
    Immediate32 = 0,
    Immediate64 = 1,
    Relative = 2,
    Immediate32PlusRelative = 3,
    Immediate64PlusRelative = 4,
  };

  struct Index {
    IndexRepresentation representation = IndexRepresentation::Immediate32;
    std::optional<uint32_t> immediate_lo;
    std::optional<uint32_t> immediate_hi;
    std::optional<xyz::indirect<Operand>> relative_operand;  ///< Deep-copying, uniquely-owning relative operand (std::indirect-style).

    Index() = default;
    bool operator==(const Index& rhs) const;
    [[nodiscard]] std::vector<uint32_t> Encode() const;
  };

  OperandType type = OperandType::Temp;
  Components components;
  uint32_t component_mode = 0;  ///< Token bits 2-11 (selection mode + value); see kComponentModeBits.
  std::vector<Index> index_entries;
  OperandModifier modifier = OperandModifier::None;
  std::optional<xyz::indirect<Operand>> relative_operand;  ///< Deep-copying, uniquely-owning relative operand (std::indirect-style).
  uint32_t source_offset = 0;                              ///< Parse provenance (dword offset in the shader chunk); not recipe state.
  uint32_t source_length = 0;                              ///< Parse provenance (dword length); not recipe state.

  bool operator==(const Operand& rhs) const;
  [[nodiscard]] std::vector<uint32_t> Encode() const;

  /// Validates component selection mode consistency with the expected role.
  bool ValidateForRole(OperandRole expected_role, const std::string& path, std::string& error) const;
};

struct Instruction {
  Opcode opcode = OpcodeUnknown();
  OpcodeControls controls;
  uint32_t length_in_dwords = 0;
  std::vector<Operand> operands;
  SamplerMode sampler_mode = SamplerMode::Default;
  std::vector<uint32_t> custom_data;
  uint32_t custom_data_opcode_token = 0;
  uint32_t source_offset = 0;  ///< Parse provenance (dword offset in the shader chunk); not recipe state.
  uint32_t source_length = 0;  ///< Parse provenance (dword length); not recipe state.

  [[nodiscard]] std::vector<uint32_t> Encode() const;
};

/// @brief Captured operand with role and destination_mask.
struct CapturedOperand {
  Operand operand_data;
  OperandRole role = OperandRole::Source;
  uint32_t destination_mask = 0;
  uint32_t component_mask = 0;
  std::optional<std::string> export_as;

  /// Resolves into the representation needed for a new role.
  [[nodiscard]] Operand ResolveForRole(OperandRole new_role) const;
};

/// @brief Captured instruction.
struct CapturedInstruction {
  Instruction instruction_data;
};

/// @brief Captured instruction blob (match_blob window / scoped-edit target).
/// Stored by value: cross-step, a blob is an independent instruction sequence,
/// never a live view into the program (positions would go stale after splices).
struct CapturedBlob {
  std::vector<Instruction> instructions;
};

struct ResourceDecl {
  uint32_t register_bind_point = 0;
  uint32_t register_space = 0;
  uint32_t dimension = 0;
  uint32_t num_elements = 1;
  uint32_t return_field_type = 0;
  uint32_t sample_count = 1;
  bool indexed = false;
};

struct SamplerDecl {
  uint32_t register_bind_point = 0;
  uint32_t register_space = 0;
  uint32_t mode = 0;
  bool indexed = false;
};

struct CBufferDecl {
  uint32_t register_bind_point = 0;
  uint32_t register_space = 0;
  uint32_t elements = 1;
  uint32_t access_pattern = 0;
  std::string name;
};

struct ThreadGroupDecl {
  uint32_t group_size_x = 1;
  uint32_t group_size_y = 1;
  uint32_t group_size_z = 1;
};

struct GlobalFlags {
  uint32_t flags = 0;
};

OperandRole GetOperandRole(Opcode opcode, size_t operand_index);

/// @brief Expected operand count for an opcode per the dxbc-spirv layout table.
/// @return the count, or 0 for opcodes with no defined operand layout.
uint32_t GetExpectedOperandCount(Opcode opcode);

/// @brief Expected scalar/operand data type for an opcode's operand slot.
/// @return the type, or @c OperandScalarType::Unknown when unconstrained/undefined.
OperandScalarType GetExpectedOperandType(Opcode opcode, size_t operand_index);

}  // namespace dxp::sm5::model
