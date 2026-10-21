#pragma once

#include <array>
#include <cstdint>
#include <variant>

#include <dxp/sm5/Model.hpp>
#include <glaze/glaze.hpp>
#include "d3d11TokenizedProgramFormat.hpp"

namespace glz {

template <>
struct meta<dxp::sm5::model::NumComponents> {
  using T = dxp::sm5::model::NumComponents;
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "zero", T::Zero,
      "one", T::One,
      "four", T::Four,
      "n", T::N);
};

template <>
struct meta<dxp::sm5::model::SelectionMode> {
  using T = dxp::sm5::model::SelectionMode;
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "mask", T::Mask,
      "swizzle", T::Swizzle,
      "select", T::Select);
};

template <>
struct meta<dxp::sm5::model::InterpolationMode> {
  using T = dxp::sm5::model::InterpolationMode;  // alias of dxp::InterpolationMode
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "undefined", T::Undefined,
      "constant", T::Constant,
      "linear", T::Linear,
      "linear_centroid", T::LinearCentroid,
      "linear_noperspective", T::LinearNoperspective,
      "linear_noperspective_centroid", T::LinearNoperspectiveCentroid,
      "linear_sample", T::LinearSample,
      "linear_noperspective_sample", T::LinearNoperspectiveSample,
      "invalid", T::Invalid);
};

template <>
struct meta<dxp::sm5::model::CbufferAccessPattern> {
  using T = dxp::sm5::model::CbufferAccessPattern;
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "immediate_indexed", T::ImmediateIndexed,
      "dynamic_indexed", T::DynamicIndexed);
};

template <>
struct meta<dxp::sm5::model::SamplerMode> {
  using T = dxp::sm5::model::SamplerMode;
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "default", T::Default,
      "comparison", T::Comparison,
      "mono", T::Mono);
};

template <>
struct meta<dxp::sm5::model::SignatureSemantic> {
  using T = dxp::sm5::model::SignatureSemantic;
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "undefined", T::Undefined,
      "position", T::Position,
      "clip_distance", T::ClipDistance,
      "cull_distance", T::CullDistance,
      "render_target_array_index", T::RenderTargetArrayIndex,
      "viewport_array_index", T::ViewportArrayIndex,
      "vertex_id", T::VertexId,
      "primitive_id", T::PrimitiveId,
      "instance_id", T::InstanceId,
      "is_front_face", T::IsFrontFace,
      "sample_index", T::SampleIndex,
      "final_quad_u_eq_0_edge_tessfactor", T::FinalQuadUEq0EdgeTessfactor,
      "final_quad_v_eq_0_edge_tessfactor", T::FinalQuadVEq0EdgeTessfactor,
      "final_quad_u_eq_1_edge_tessfactor", T::FinalQuadUEq1EdgeTessfactor,
      "final_quad_v_eq_1_edge_tessfactor", T::FinalQuadVEq1EdgeTessfactor,
      "final_quad_u_inside_tessfactor", T::FinalQuadUInsideTessfactor,
      "final_quad_v_inside_tessfactor", T::FinalQuadVInsideTessfactor,
      "final_tri_u_eq_0_edge_tessfactor", T::FinalTriUEq0EdgeTessfactor,
      "final_tri_v_eq_0_edge_tessfactor", T::FinalTriVEq0EdgeTessfactor,
      "final_tri_w_eq_0_edge_tessfactor", T::FinalTriWEq0EdgeTessfactor,
      "final_tri_inside_tessfactor", T::FinalTriInsideTessfactor,
      "final_line_detail_tessfactor", T::FinalLineDetailTessfactor,
      "final_line_density_tessfactor", T::FinalLineDensityTessfactor);
};

template <>
struct meta<dxp::sm5::model::OperandType> {
  using T = dxp::sm5::model::OperandType;
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "temp", T::Temp,
      "input", T::Input,
      "output", T::Output,
      "indexable_temp", T::IndexableTemp,
      "immediate32", T::Immediate32,
      "immediate64", T::Immediate64,
      "sampler", T::Sampler,
      "resource", T::Resource,
      "constant_buffer", T::CBuffer,
      "immediate_constant_buffer", T::ImmediateConstantBuffer,
      "label", T::Label,
      "input_primitive_id", T::InputPrimitiveId,
      "output_depth", T::OutputDepth,
      "null", T::Null,
      "rasterizer", T::Rasterizer,
      "output_coverage_mask", T::OutputCoverageMask,
      "stream", T::Stream,
      "function_body", T::FunctionBody,
      "function_table", T::FunctionTable,
      "interface", T::Interface,
      "function_input", T::FunctionInput,
      "function_output", T::FunctionOutput,
      "output_control_point_id", T::OutputControlPointId,
      "input_fork_instance_id", T::InputForkInstanceId,
      "input_join_instance_id", T::InputJoinInstanceId,
      "input_control_point", T::InputControlPoint,
      "output_control_point", T::OutputControlPoint,
      "input_patch_constant", T::InputPatchConstant,
      "input_domain_point", T::InputDomainPoint,
      "this_pointer", T::ThisPointer,
      "unordered_access_view", T::UAV,
      "thread_group_shared_memory", T::ThreadGroupSharedMemory,
      "input_thread_id", T::InputThreadId,
      "input_thread_group_id", T::InputThreadGroupId,
      "input_thread_id_in_group", T::InputThreadIdInGroup,
      "input_coverage_mask", T::InputCoverageMask,
      "input_thread_id_in_group_flattened", T::InputThreadIdInGroupFlattened,
      "input_gs_instance_id", T::InputGsInstanceId,
      "output_depth_greater_equal", T::OutputDepthGreaterEqual,
      "output_depth_less_equal", T::OutputDepthLessEqual,
      "cycle_counter", T::CycleCounter);
};

template <>
struct meta<dxp::sm5::model::OperandModifier> {
  using T = dxp::sm5::model::OperandModifier;
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "none", T::None,
      "neg", T::Neg,
      "abs", T::Abs,
      "abs_neg", T::AbsNeg);
};

template <>
struct meta<dxp::sm5::model::ResourceDimension> {
  using T = dxp::sm5::model::ResourceDimension;
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "unknown", T::Unknown,
      "buffer", T::Buffer,
      "texture1d", T::Texture1D,
      "texture2d", T::Texture2D,
      "texture2dms", T::Texture2DMS,
      "texture3d", T::Texture3D,
      "texturecube", T::TextureCube,
      "texture1darray", T::Texture1DArray,
      "texture2darray", T::Texture2DArray,
      "texture2dmsarray", T::Texture2DMSArray,
      "texturecubearray", T::TextureCubeArray,
      "raw_buffer", T::RawBuffer,
      "structured_buffer", T::StructuredBuffer);
};

template <>
struct meta<dxp::sm5::model::ResourceReturnType> {
  using T = dxp::sm5::model::ResourceReturnType;
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "unorm", T::UNorm,
      "snorm", T::SNorm,
      "sint", T::SInt,
      "uint", T::UInt,
      "float", T::Float,
      "mixed", T::Mixed,
      "double", T::Double,
      "continued", T::Continued,
      "unused", T::Unused);
};

template <>
struct meta<dxp::sm5::model::Opcode> {
  using T = dxp::sm5::model::Opcode;
  static constexpr std::array keys = {
      "add", "and", "break", "breakc", "call", "callc", "case",
      "continue", "continuec", "cut", "default", "deriv_rtx",
      "deriv_rty", "discard", "div", "dp2", "dp3", "dp4", "else",
      "emit", "emitthencut", "endif", "endloop", "endswitch",
      "eq", "exp", "frc", "ftoi", "ftou", "ge", "iadd", "if",
      "ieq", "ige", "ilt", "imad", "imax", "imin", "imul", "ine",
      "ineg", "ishl", "ishr", "itof", "label", "ld", "ld_ms",
      "log", "loop", "lt", "mad", "min", "max", "customdata",
      "mov", "movc", "mul", "ne", "nop", "not", "or", "resinfo",
      "ret", "retc", "round_ne", "round_ni", "round_pi", "round_z",
      "rsq", "sample", "sample_c", "sample_c_lz", "sample_l",
      "sample_d", "sample_b", "sqrt", "switch", "sincos", "udiv",
      "ult", "uge", "umul", "umad", "umax", "umin", "ushr", "utof",
      "xor", "dcl_resource", "dcl_constant_buffer", "dcl_sampler",
      "dcl_index_range", "dcl_gs_output_primitive_topology",
      "dcl_gs_input_primitive", "dcl_max_output_vertex_count",
      "dcl_input", "dcl_input_sgv", "dcl_input_siv", "dcl_input_ps",
      "dcl_input_ps_sgv", "dcl_input_ps_siv", "dcl_output",
      "dcl_output_sgv", "dcl_output_siv", "dcl_temps",
      "dcl_indexable_temp", "dcl_global_flags", "reserved0",
      "lod", "gather4", "sample_pos", "sample_info", "reserved1",
      "hs_decls", "hs_control_point_phase", "hs_fork_phase",
      "hs_join_phase", "emit_stream", "cut_stream", "emitthencut_stream",
      "interface_call", "bufinfo", "deriv_rtx_coarse", "deriv_rtx_fine",
      "deriv_rty_coarse", "deriv_rty_fine", "gather4_c", "gather4_po",
      "gather4_po_c", "rcp", "f32tof16", "f16tof32", "uaddc", "usubb",
      "countbits", "firstbit_hi", "firstbit_lo", "firstbit_shi",
      "ubfe", "ibfe", "bfi", "bfrev", "swapc", "dcl_stream",
      "dcl_function_body", "dcl_function_table", "dcl_interface",
      "dcl_input_control_point_count", "dcl_output_control_point_count",
      "dcl_tess_domain", "dcl_tess_partitioning",
      "dcl_tess_output_primitive", "dcl_hs_max_tessfactor",
      "dcl_hs_fork_phase_instance_count",
      "dcl_hs_join_phase_instance_count", "dcl_thread_group",
      "dcl_unordered_access_view_typed", "dcl_unordered_access_view_raw",
      "dcl_unordered_access_view_structured",
      "dcl_thread_group_shared_memory_raw",
      "dcl_thread_group_shared_memory_structured",
      "dcl_resource_raw", "dcl_resource_structured",
      "ld_uav_typed", "store_uav_typed", "ld_raw", "store_raw",
      "ld_structured", "store_structured", "atomic_and", "atomic_or",
      "atomic_xor", "atomic_cmp_store", "atomic_iadd", "atomic_imax",
      "atomic_imin", "atomic_umax", "atomic_umin", "imm_atomic_alloc",
      "imm_atomic_consume", "imm_atomic_iadd", "imm_atomic_and",
      "imm_atomic_or", "imm_atomic_xor", "imm_atomic_exch",
      "imm_atomic_cmp_exch", "imm_atomic_imax", "imm_atomic_imin",
      "imm_atomic_umax", "imm_atomic_umin", "sync", "dadd", "dmax",
      "dmin", "dmul", "deq", "dge", "dlt", "dne", "dmov", "dmovc",
      "dtof", "ftod", "eval_snapped", "eval_sample_index", "eval_centroid",
      "dcl_gs_instance_count", "abort", "debug_break", "reserved0_209",
      "ddiv", "dfma", "drcp", "msad", "dtoi", "dtou", "itod", "utod",
      "reserved0_218",
      "gather4_feedback", "gather4_c_feedback", "gather4_po_feedback",
      "gather4_po_c_feedback", "ld_feedback", "ld_ms_feedback",
      "ld_uav_typed_feedback", "ld_raw_feedback", "ld_structured_feedback",
      "sample_l_feedback", "sample_c_lz_feedback", "sample_clamp_feedback",
      "sample_b_clamp_feedback", "sample_d_clamp_feedback",
      "sample_c_clamp_feedback", "check_access_fully_mapped",
      "reserved0_235"};
  static constexpr std::array value = {
      T::Add, T::And, T::Break, T::BreakC, T::Call, T::CallC, T::Case,
      T::Continue, T::ContinueC, T::Cut, T::Default, T::DerivRTX,
      T::DerivRTY, T::Discard, T::Div, T::DP2, T::DP3, T::DP4, T::Else,
      T::Emit, T::EmitThenCut, T::EndIf, T::EndLoop, T::EndSwitch,
      T::Eq, T::Exp, T::Frc, T::Ftoi, T::Ftou, T::Ge, T::IAdd, T::If,
      T::IEq, T::IGe, T::ILt, T::IMad, T::IMax, T::IMin, T::IMul, T::INe,
      T::INeg, T::IShl, T::IShr, T::Itof, T::Label, T::Ld, T::LdMs,
      T::Log, T::Loop, T::Lt, T::Mad, T::Min, T::Max, T::CustomData,
      T::Mov, T::MovC, T::Mul, T::Ne, T::Nop, T::Not, T::Or, T::Resinfo,
      T::Ret, T::RetC, T::RoundNe, T::RoundNi, T::RoundPi, T::RoundZ,
      T::Rsq, T::Sample, T::SampleC, T::SampleCLz, T::SampleL, T::SampleD,
      T::SampleB, T::Sqrt, T::Switch, T::Sincos, T::UDiv, T::ULt, T::UGe,
      T::UMul, T::UMad, T::UMax, T::UMin, T::UShr, T::Utof, T::Xor,
      T::DclResource, T::DclConstantBuffer, T::DclSampler,
      T::DclIndexRange, T::DclGsOutputPrimitiveTopology,
      T::DclGsInputPrimitive, T::DclMaxOutputVertexCount,
      T::DclInput, T::DclInputSgv, T::DclInputSiv, T::DclInputPs,
      T::DclInputPsSgv, T::DclInputPsSiv, T::DclOutput,
      T::DclOutputSgv, T::DclOutputSiv, T::DclTemps,
      T::DclIndexableTemp, T::DclGlobalFlags, T::Reserved0,
      T::Lod, T::Gather4, T::SamplePos, T::SampleInfo, T::Reserved1,
      T::HsDecls, T::HsControlPointPhase, T::HsForkPhase, T::HsJoinPhase,
      T::EmitStream, T::CutStream, T::EmitThenCutStream,
      T::InterfaceCall, T::Bufinfo, T::DerivRTXCoarse, T::DerivRTXFine,
      T::DerivRTYCoarse, T::DerivRTYFine, T::Gather4C, T::Gather4PO,
      T::Gather4POC, T::Rcp, T::F32ToF16, T::F16ToF32, T::UAddC, T::USubb,
      T::CountBits, T::FirstBitHi, T::FirstBitLo, T::FirstBitSHI,
      T::UBFE, T::IBFE, T::BFI, T::BFRev, T::SwapC, T::DclStream,
      T::DclFunctionBody, T::DclFunctionTable, T::DclInterface,
      T::DclInputControlPointCount,
      T::DclOutputControlPointCount, T::DclTessDomain,
      T::DclTessPartitioning, T::DclTessOutputPrimitive,
      T::DclHsMaxTessfactor,
      T::DclHsForkPhaseInstanceCount,
      T::DclHsJoinPhaseInstanceCount, T::DclThreadGroup,
      T::DclUnorderedAccessViewTyped, T::DclUnorderedAccessViewRaw,
      T::DclUnorderedAccessViewStructured,
      T::DclThreadGroupSharedMemoryRaw,
      T::DclThreadGroupSharedMemoryStructured,
      T::DclResourceRaw, T::DclResourceStructured,
      T::LdUavTyped, T::StoreUavTyped, T::LdRaw, T::StoreRaw,
      T::LdStructured, T::StoreStructured, T::AtomicAnd, T::AtomicOr,
      T::AtomicXor, T::AtomicCmpStore, T::AtomicIAdd, T::AtomicIMax,
      T::AtomicIMin, T::AtomicUMax, T::AtomicUMin, T::ImmAtomicAlloc,
      T::ImmAtomicConsume, T::ImmAtomicIAdd, T::ImmAtomicAnd,
      T::ImmAtomicOr, T::ImmAtomicXor, T::ImmAtomicExch,
      T::ImmAtomicCmpExch, T::ImmAtomicIMax, T::ImmAtomicIMin,
      T::ImmAtomicUMax, T::ImmAtomicUMin, T::Sync, T::DAdd, T::DMax,
      T::DMin, T::DMul, T::DEq, T::DGe, T::DLt, T::DNe, T::DMov,
      T::DMovC, T::DToF, T::FToD, T::EvalSnapped, T::EvalSampleIndex,
      T::EvalCentroid, T::DclGsInstanceCount, T::Abort, T::DebugBreak,
      T::Reserved0209,
      T::DDiv, T::DFma, T::DRcp, T::MSAD, T::DToI, T::DToU, T::IToD,
      T::UToD, T::Reserved0218,
      T::Gather4Feedback, T::Gather4CFeedback, T::Gather4POFeedback,
      T::Gather4POCFeedback, T::LdFeedback, T::LdMsFeedback,
      T::LdUavTypedFeedback, T::LdRawFeedback, T::LdStructuredFeedback,
      T::SampleLFeedback, T::SampleCLzFeedback, T::SampleClampFeedback,
      T::SampleBClampFeedback, T::SampleDClampFeedback,
      T::SampleCClampFeedback, T::CheckAccessFullyMapped,
      T::Reserved0235};
};

}  // namespace glz

namespace dxp::sm5::model {

/// Extended-opcode token layout (D3D11 tokenized format).
constexpr uint32_t kExtendedOpcodeTypeMask = 0x3FU;  ///< type bits [5:0].
constexpr uint32_t kSampleControlOffsetBits = 4U;    ///< sample offsets are 4-bit 2's complement.

// ld_raw/ld_structured declarations carry no return types; canonical is MIXED x4.
constexpr uint32_t kPackedMixedReturnTypes = ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_MIXED, 0)
                                             | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_MIXED, 1)
                                             | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_MIXED, 2)
                                             | ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_MIXED, 3);

/// @brief Canonical extended-opcode chain per resource-access opcode
/// (verified against the fxc corpus, see tests/sm5_extended_chain_table_test.cpp):
/// ld/ld2dms/resinfo/sample/gather4 families carry ResourceDim + ResourceReturnType
/// (sample/gather4 additionally SampleControls when offsets != 0); ld_raw/ld_structured
/// carry a fixed RAW/STRUCTURED_BUFFER dim + MIXED return; everything else has none.
enum class ExtendedChainKind : std::uint8_t {
  None = 0,                   ///< no extended opcode tokens.
  ResourcePair,               ///< ResourceDim + ResourceReturnType, both from the declaration.
  ResourcePairFixed,          ///< ResourceDim + ResourceReturnType; dim/return types are fixed (ld_raw, ld_structured).
  ResourcePairControls,       ///< SampleControls (when offsets != 0) + ResourceDim + ResourceReturnType.
  ResourcePairControlsFixed,  ///< SampleControls (when offsets != 0) + ResourceDim + ResourceReturnType (fixed).
};

/// @brief Extended-opcode chain spec for one opcode.
struct ExtendedChainSpec {
  ExtendedChainKind kind = ExtendedChainKind::None;
  uint32_t fixed_dimension = 0;    ///< D3D10_SB_RESOURCE_DIMENSION to emit for Fixed kinds.
  uint32_t fixed_return_type = 0;  ///< Packed 4x4-bit return types to emit for Fixed kinds (e.g. MIXED x4).

  /// @brief Whether the canonical chain includes the ResourceDim + ReturnType pair.
  [[nodiscard]] bool RequiresResourcePair() const {
    return kind == ExtendedChainKind::ResourcePair || kind == ExtendedChainKind::ResourcePairFixed
           || kind == ExtendedChainKind::ResourcePairControls || kind == ExtendedChainKind::ResourcePairControlsFixed;
  }

  /// @brief Whether the return type is fixed rather than derived from a declaration.
  [[nodiscard]] bool HasFixedMetadata() const {
    return kind == ExtendedChainKind::ResourcePairFixed || kind == ExtendedChainKind::ResourcePairControlsFixed;
  }
};

/// @brief Canonical extended-opcode chain for an SM5 opcode (see ExtendedChainKind).
inline ExtendedChainSpec RequiredExtendedChainForOpcode(Opcode opcode) {
  switch (opcode) {
    case Opcode::Ld:
    case Opcode::LdMs:
    case Opcode::Resinfo:
      return {.kind = ExtendedChainKind::ResourcePair};
    case Opcode::LdRaw:
      return {.kind = ExtendedChainKind::ResourcePairFixed,
              .fixed_dimension = D3D11_SB_RESOURCE_DIMENSION_RAW_BUFFER,
              .fixed_return_type = kPackedMixedReturnTypes};
    case Opcode::LdStructured:
      return {.kind = ExtendedChainKind::ResourcePairFixed,
              .fixed_dimension = D3D11_SB_RESOURCE_DIMENSION_STRUCTURED_BUFFER,
              .fixed_return_type = kPackedMixedReturnTypes};
    case Opcode::Sample:
    case Opcode::SampleB:
    case Opcode::SampleC:
    case Opcode::SampleCLz:
    case Opcode::SampleD:
    case Opcode::SampleL:
    case Opcode::Gather4:
    case Opcode::Gather4C:
    case Opcode::Gather4PO:
    case Opcode::Gather4POC:
      return {.kind = ExtendedChainKind::ResourcePairControls};
    default:
      return {};
  }
}

/// @brief Decoded extended-opcode payload; the uint32_t alternative is the raw
/// token for unknown/future types (multi-dword payloads, reserved type codes)
/// that we cannot model.
using ExtendedOpcodePayload = std::variant<SampleControlsPayload, ResourceDimPayload,
                                           ResourceReturnTypePayload, uint32_t>;

struct DecodedExtendedOpcode {
  ExtendedOpcodeType type = ExtendedOpcodeType::Empty;
  ExtendedOpcodePayload payload;
  bool chained = false;  ///< Bit 31: another extended opcode follows.
};

/// @brief 4-bit 2's-complement sign extension (the WDK's
/// DECODE_IMMEDIATE_D3D10_SB_ADDRESS_OFFSET claims it but does not apply it).
inline int32_t SignExtend4(uint32_t value) {
  return static_cast<int32_t>(value | -(value & 0x8U));
}

/// @brief Decodes one extended-opcode token using the WDK layout macros.
inline DecodedExtendedOpcode ParseExtendedOpcodeToken(uint32_t token) {
  DecodedExtendedOpcode out;
  out.type = static_cast<ExtendedOpcodeType>(token & kExtendedOpcodeTypeMask);
  out.chained = (token & D3D10_SB_OPCODE_EXTENDED_MASK) != 0;
  switch (out.type) {
    case ExtendedOpcodeType::SampleControls:
      out.payload = SampleControlsPayload{
          .u = SignExtend4(DECODE_IMMEDIATE_D3D10_SB_ADDRESS_OFFSET(0, token)),
          .v = SignExtend4(DECODE_IMMEDIATE_D3D10_SB_ADDRESS_OFFSET(1, token)),
          .w = SignExtend4(DECODE_IMMEDIATE_D3D10_SB_ADDRESS_OFFSET(2, token))};
      break;
    case ExtendedOpcodeType::ResourceDim:
      out.payload = ResourceDimPayload{
          .dimension = static_cast<uint32_t>(DECODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION(token)),
          .structure_stride =
              static_cast<uint32_t>(DECODE_D3D11_SB_EXTENDED_RESOURCE_DIMENSION_STRUCTURE_STRIDE(token))};
      break;
    case ExtendedOpcodeType::ResourceType:
      out.payload = ResourceReturnTypePayload{.component_types = {
                                                  static_cast<uint32_t>(DECODE_D3D11_SB_EXTENDED_RESOURCE_RETURN_TYPE(token, 0)),
                                                  static_cast<uint32_t>(DECODE_D3D11_SB_EXTENDED_RESOURCE_RETURN_TYPE(token, 1)),
                                                  static_cast<uint32_t>(DECODE_D3D11_SB_EXTENDED_RESOURCE_RETURN_TYPE(token, 2)),
                                                  static_cast<uint32_t>(DECODE_D3D11_SB_EXTENDED_RESOURCE_RETURN_TYPE(token, 3))}};
      break;
    default:
      out.payload = token;  // Raw fallback: unknown/reserved type, keep exact bits.
      break;
  }
  return out;
}

}  // namespace dxp::sm5::model
