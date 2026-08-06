#pragma once

#include <dxp/sm5/Model.hpp>
#include <glaze/glaze.hpp>

namespace glz {

template <>
struct meta<dxp::sm5::NumComponents> {
  using T = dxp::sm5::NumComponents;
  static constexpr std::array keys = {"zero", "one", "four", "n"};
  static constexpr std::array value = {T::Zero, T::One, T::Four, T::N};
};

template <>
struct meta<dxp::sm5::SelectionMode> {
  using T = dxp::sm5::SelectionMode;
  static constexpr std::array keys = {"mask", "swizzle", "select"};
  static constexpr std::array value = {T::Mask, T::Swizzle, T::Select};
};

template <>
struct meta<dxp::sm5::InterpolationMode> {
  using T = dxp::sm5::InterpolationMode;
  static constexpr std::array keys = {"undefined",
                                      "constant",
                                      "linear",
                                      "linear_centroid",
                                      "linear_noperspective",
                                      "linear_noperspective_centroid",
                                      "linear_sample",
                                      "linear_noperspective_sample"};
  static constexpr std::array value = {T::Undefined,
                                       T::Constant,
                                       T::Linear,
                                       T::LinearCentroid,
                                       T::LinearNoperspective,
                                       T::LinearNoperspectiveCentroid,
                                       T::LinearSample,
                                       T::LinearNoperspectiveSample};
};

template <>
struct meta<dxp::sm5::CbufferAccessPattern> {
  using T = dxp::sm5::CbufferAccessPattern;
  static constexpr std::array keys = {"immediate_indexed", "dynamic_indexed"};
  static constexpr std::array value = {T::ImmediateIndexed, T::DynamicIndexed};
};

template <>
struct meta<dxp::sm5::SamplerMode> {
  using T = dxp::sm5::SamplerMode;
  static constexpr std::array keys = {"default", "comparison", "mono"};
  static constexpr std::array value = {T::Default, T::Comparison, T::Mono};
};

template <>
struct meta<dxp::sm5::OperandType> {
  using T = dxp::sm5::OperandType;
  static constexpr std::array keys = {
      "temp", "input", "output", "indexable_temp",
      "immediate32", "immediate64", "sampler", "resource",
      "constant_buffer", "immediate_constant_buffer", "label",
      "input_primitive_id", "output_depth", "null",
      "rasterizer", "output_coverage_mask",
      "stream", "function_body", "function_table", "interface",
      "function_input", "function_output",
      "output_control_point_id", "input_fork_instance_id",
      "input_join_instance_id", "input_control_point",
      "output_control_point", "input_patch_constant",
      "input_domain_point", "this_pointer",
      "unordered_access_view", "thread_group_shared_memory",
      "input_thread_id", "input_thread_group_id",
      "input_thread_id_in_group", "input_coverage_mask",
      "input_thread_id_in_group_flattened",
      "input_gs_instance_id",
      "output_depth_greater_equal", "output_depth_less_equal",
      "cycle_counter"};
  static constexpr std::array value = {
      T::Temp, T::Input, T::Output, T::IndexableTemp,
      T::Immediate32, T::Immediate64, T::Sampler, T::Resource,
      T::CBuffer, T::ImmediateConstantBuffer, T::Label,
      T::InputPrimitiveId, T::OutputDepth, T::Null,
      T::Rasterizer, T::OutputCoverageMask,
      T::Stream, T::FunctionBody, T::FunctionTable, T::Interface,
      T::FunctionInput, T::FunctionOutput,
      T::OutputControlPointId, T::InputForkInstanceId,
      T::InputJoinInstanceId, T::InputControlPoint,
      T::OutputControlPoint, T::InputPatchConstant,
      T::InputDomainPoint, T::ThisPointer,
      T::UAV, T::ThreadGroupSharedMemory,
      T::InputThreadId, T::InputThreadGroupId,
      T::InputThreadIdInGroup, T::InputCoverageMask,
      T::InputThreadIdInGroupFlattened,
      T::InputGsInstanceId,
      T::OutputDepthGreaterEqual, T::OutputDepthLessEqual,
      T::CycleCounter};
};

template <>
struct meta<dxp::sm5::OperandModifier> {
  using T = dxp::sm5::OperandModifier;
  static constexpr std::array keys = {"none", "neg", "abs", "abs_neg"};
  static constexpr std::array value = {T::None, T::Neg, T::Abs, T::AbsNeg};
};

template <>
struct meta<dxp::sm5::Opcode> {
  using T = dxp::sm5::Opcode;
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
