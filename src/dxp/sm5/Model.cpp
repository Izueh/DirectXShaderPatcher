#include "dxp/sm5/Model.h"

#include <algorithm>
#include <cctype>

namespace dxp {
namespace sm5 {

struct OpcodeNameEntry {
  uint32_t value;
  const char *name;
};

struct OpcodeAliasEntry {
  const char *name;
  uint32_t value;
  int32_t testBoolean;
};

static const OpcodeNameEntry kOpcodeTable[] = {
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ADD), "add"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_AND), "and"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_BREAK), "break"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_BREAKC), "breakc"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_CALL), "call"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_CALLC), "callc"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_CASE), "case"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_CONTINUE), "continue"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_CONTINUEC), "continuec"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_CUT), "cut"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DEFAULT), "default"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DERIV_RTX), "deriv_rtx"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DERIV_RTY), "deriv_rty"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DISCARD), "discard"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DIV), "div"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DP2), "dp2"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DP3), "dp3"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DP4), "dp4"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ELSE), "else"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_EMIT), "emit"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_EMITTHENCUT), "emitthencut"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ENDIF), "endif"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ENDLOOP), "endloop"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ENDSWITCH), "endswitch"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_EQ), "eq"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_EXP), "exp"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_FRC), "frc"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_FTOI), "ftoi"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_FTOU), "ftou"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_GE), "ge"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IADD), "iadd"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IF), "if"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IEQ), "ieq"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IGE), "ige"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ILT), "ilt"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IMAD), "imad"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IMAX), "imax"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IMIN), "imin"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IMUL), "imul"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_INE), "ine"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_INEG), "ineg"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ISHL), "ishl"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ISHR), "ishr"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ITOF), "itof"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_LABEL), "label"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_LD), "ld"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_LD_MS), "ld_ms"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_LOG), "log"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_LOOP), "loop"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_LT), "lt"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MAD), "mad"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MIN), "min"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MAX), "max"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_CUSTOMDATA), "customdata"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MOV), "mov"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MOVC), "movc"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MUL), "mul"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_NE), "ne"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_NOP), "nop"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_NOT), "not"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_OR), "or"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_RESINFO), "resinfo"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_RET), "ret"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_RETC), "retc"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ROUND_NE), "round_ne"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ROUND_NI), "round_ni"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ROUND_PI), "round_pi"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ROUND_Z), "round_z"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_RSQ), "rsq"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SAMPLE), "sample"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SAMPLE_C), "sample_c"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SAMPLE_C_LZ), "sample_c_lz"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SAMPLE_L), "sample_l"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SAMPLE_D), "sample_d"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SAMPLE_B), "sample_b"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SQRT), "sqrt"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SWITCH), "switch"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SINCOS), "sincos"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_UDIV), "udiv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ULT), "ult"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_UGE), "uge"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_UMUL), "umul"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_UMAD), "umad"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_UMAX), "umax"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_UMIN), "umin"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_USHR), "ushr"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_UTOF), "utof"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_XOR), "xor"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_RESOURCE), "dcl_resource"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER), "dcl_constant_buffer"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_SAMPLER), "dcl_sampler"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INDEX_RANGE), "dcl_index_range"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY), "dcl_gs_output_primitive_topology"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_GS_INPUT_PRIMITIVE), "dcl_gs_input_primitive"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT), "dcl_max_output_vertex_count"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT), "dcl_input"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT_SGV), "dcl_input_sgv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT_SIV), "dcl_input_siv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT_PS), "dcl_input_ps"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT_PS_SGV), "dcl_input_ps_sgv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT_PS_SIV), "dcl_input_ps_siv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_OUTPUT), "dcl_output"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_OUTPUT_SGV), "dcl_output_sgv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_OUTPUT_SIV), "dcl_output_siv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_TEMPS), "dcl_temps"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INDEXABLE_TEMP), "dcl_indexable_temp"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS), "dcl_global_flags"},
    {static_cast<uint32_t>(D3D10_1_SB_OPCODE_LOD), "lod"},
    {static_cast<uint32_t>(D3D10_1_SB_OPCODE_GATHER4), "gather4"},
    {static_cast<uint32_t>(D3D10_1_SB_OPCODE_SAMPLE_POS), "sample_pos"},
    {static_cast<uint32_t>(D3D10_1_SB_OPCODE_SAMPLE_INFO), "sample_info"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_HS_DECLS), "hs_decls"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_HS_CONTROL_POINT_PHASE), "hs_control_point_phase"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_HS_FORK_PHASE), "hs_fork_phase"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_HS_JOIN_PHASE), "hs_join_phase"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_EMIT_STREAM), "emit_stream"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_CUT_STREAM), "cut_stream"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_EMITTHENCUT_STREAM), "emitthencut_stream"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_INTERFACE_CALL), "interface_call"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_BUFINFO), "bufinfo"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DERIV_RTX_COARSE), "deriv_rtx_coarse"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DERIV_RTX_FINE), "deriv_rtx_fine"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DERIV_RTY_COARSE), "deriv_rty_coarse"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DERIV_RTY_FINE), "deriv_rty_fine"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_GATHER4_C), "gather4_c"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_GATHER4_PO), "gather4_po"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_GATHER4_PO_C), "gather4_po_c"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_RCP), "rcp"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_F32TOF16), "f32tof16"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_F16TOF32), "f16tof32"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_UADDC), "uaddc"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_USUBB), "usubb"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_COUNTBITS), "countbits"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_FIRSTBIT_HI), "firstbit_hi"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_FIRSTBIT_LO), "firstbit_lo"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_FIRSTBIT_SHI), "firstbit_shi"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_UBFE), "ubfe"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IBFE), "ibfe"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_BFI), "bfi"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_BFREV), "bfrev"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_SWAPC), "swapc"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_STREAM), "dcl_stream"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_FUNCTION_BODY), "dcl_function_body"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_FUNCTION_TABLE), "dcl_function_table"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_INTERFACE), "dcl_interface"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_INPUT_CONTROL_POINT_COUNT), "dcl_input_control_point_count"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT), "dcl_output_control_point_count"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_TESS_DOMAIN), "dcl_tess_domain"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_TESS_PARTITIONING), "dcl_tess_partitioning"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_TESS_OUTPUT_PRIMITIVE), "dcl_tess_output_primitive"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_HS_MAX_TESSFACTOR), "dcl_hs_max_tessfactor"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT), "dcl_hs_fork_phase_instance_count"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT), "dcl_hs_join_phase_instance_count"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_THREAD_GROUP), "dcl_thread_group"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED), "dcl_unordered_access_view_typed"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW), "dcl_unordered_access_view_raw"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED), "dcl_unordered_access_view_structured"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW), "dcl_thread_group_shared_memory_raw"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED), "dcl_thread_group_shared_memory_structured"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_RESOURCE_RAW), "dcl_resource_raw"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED), "dcl_resource_structured"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_LD_UAV_TYPED), "ld_uav_typed"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_STORE_UAV_TYPED), "store_uav_typed"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_LD_RAW), "ld_raw"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_STORE_RAW), "store_raw"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_LD_STRUCTURED), "ld_structured"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_STORE_STRUCTURED), "store_structured"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_ATOMIC_AND), "atomic_and"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_ATOMIC_OR), "atomic_or"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_ATOMIC_XOR), "atomic_xor"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_ATOMIC_CMP_STORE), "atomic_cmp_store"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_ATOMIC_IADD), "atomic_iadd"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_ATOMIC_IMAX), "atomic_imax"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_ATOMIC_IMIN), "atomic_imin"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_ATOMIC_UMAX), "atomic_umax"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_ATOMIC_UMIN), "atomic_umin"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_ALLOC), "imm_atomic_alloc"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_CONSUME), "imm_atomic_consume"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_IADD), "imm_atomic_iadd"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_AND), "imm_atomic_and"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_OR), "imm_atomic_or"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_XOR), "imm_atomic_xor"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_EXCH), "imm_atomic_exch"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_CMP_EXCH), "imm_atomic_cmp_exch"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_IMAX), "imm_atomic_imax"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_IMIN), "imm_atomic_imin"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_UMAX), "imm_atomic_umax"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_IMM_ATOMIC_UMIN), "imm_atomic_umin"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_SYNC), "sync"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DADD), "dadd"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DMAX), "dmax"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DMIN), "dmin"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DMUL), "dmul"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DEQ), "deq"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DGE), "dge"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DLT), "dlt"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DNE), "dne"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DMOV), "dmov"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DMOVC), "dmovc"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DTOF), "dtof"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_FTOD), "ftod"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_EVAL_SNAPPED), "eval_snapped"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_EVAL_SAMPLE_INDEX), "eval_sample_index"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_EVAL_CENTROID), "eval_centroid"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_GS_INSTANCE_COUNT), "dcl_gs_instance_count"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_ABORT), "abort"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DEBUG_BREAK), "debug_break"},
    {static_cast<uint32_t>(D3D11_1_SB_OPCODE_DDIV), "ddiv"},
    {static_cast<uint32_t>(D3D11_1_SB_OPCODE_DFMA), "dfma"},
    {static_cast<uint32_t>(D3D11_1_SB_OPCODE_DRCP), "drcp"},
    {static_cast<uint32_t>(D3D11_1_SB_OPCODE_MSAD), "msad"},
    {static_cast<uint32_t>(D3D11_1_SB_OPCODE_DTOI), "dtoi"},
    {static_cast<uint32_t>(D3D11_1_SB_OPCODE_DTOU), "dtou"},
    {static_cast<uint32_t>(D3D11_1_SB_OPCODE_ITOD), "itod"},
    {static_cast<uint32_t>(D3D11_1_SB_OPCODE_UTOD), "utod"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_GATHER4_FEEDBACK), "gather4_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_GATHER4_C_FEEDBACK), "gather4_c_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_GATHER4_PO_FEEDBACK), "gather4_po_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_GATHER4_PO_C_FEEDBACK), "gather4_po_c_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_LD_FEEDBACK), "ld_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_LD_MS_FEEDBACK), "ld_ms_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_LD_UAV_TYPED_FEEDBACK), "ld_uav_typed_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_LD_RAW_FEEDBACK), "ld_raw_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_LD_STRUCTURED_FEEDBACK), "ld_structured_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_SAMPLE_L_FEEDBACK), "sample_l_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_SAMPLE_C_LZ_FEEDBACK), "sample_c_lz_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_SAMPLE_CLAMP_FEEDBACK), "sample_clamp_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_SAMPLE_B_CLAMP_FEEDBACK), "sample_b_clamp_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_SAMPLE_D_CLAMP_FEEDBACK), "sample_d_clamp_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_SAMPLE_C_CLAMP_FEEDBACK), "sample_c_clamp_feedback"},
    {static_cast<uint32_t>(D3DWDDM1_3_SB_OPCODE_CHECK_ACCESS_FULLY_MAPPED), "check_access_fully_mapped"},
};

static const OpcodeAliasEntry kOpcodeAliases[] = {
    {"if_z", static_cast<uint32_t>(D3D10_SB_OPCODE_IF), D3D10_SB_INSTRUCTION_TEST_ZERO},
    {"if_nz", static_cast<uint32_t>(D3D10_SB_OPCODE_IF), D3D10_SB_INSTRUCTION_TEST_NONZERO},
    {"breakc_z", static_cast<uint32_t>(D3D10_SB_OPCODE_BREAKC), D3D10_SB_INSTRUCTION_TEST_ZERO},
    {"breakc_nz", static_cast<uint32_t>(D3D10_SB_OPCODE_BREAKC), D3D10_SB_INSTRUCTION_TEST_NONZERO},
    {"callc_z", static_cast<uint32_t>(D3D10_SB_OPCODE_CALLC), D3D10_SB_INSTRUCTION_TEST_ZERO},
    {"callc_nz", static_cast<uint32_t>(D3D10_SB_OPCODE_CALLC), D3D10_SB_INSTRUCTION_TEST_NONZERO},
    {"continuec_z", static_cast<uint32_t>(D3D10_SB_OPCODE_CONTINUEC), D3D10_SB_INSTRUCTION_TEST_ZERO},
    {"continuec_nz", static_cast<uint32_t>(D3D10_SB_OPCODE_CONTINUEC), D3D10_SB_INSTRUCTION_TEST_NONZERO},
    {"discard_z", static_cast<uint32_t>(D3D10_SB_OPCODE_DISCARD), D3D10_SB_INSTRUCTION_TEST_ZERO},
    {"discard_nz", static_cast<uint32_t>(D3D10_SB_OPCODE_DISCARD), D3D10_SB_INSTRUCTION_TEST_NONZERO},
    {"retc_z", static_cast<uint32_t>(D3D10_SB_OPCODE_RETC), D3D10_SB_INSTRUCTION_TEST_ZERO},
    {"retc_nz", static_cast<uint32_t>(D3D10_SB_OPCODE_RETC), D3D10_SB_INSTRUCTION_TEST_NONZERO},
};

static constexpr size_t kOpcodeTableSize = sizeof(kOpcodeTable) / sizeof(kOpcodeTable[0]);
static constexpr size_t kOpcodeAliasCount = sizeof(kOpcodeAliases) / sizeof(kOpcodeAliases[0]);

static std::string Lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

const char *GetOpcodeName(Opcode opcode) {
  const uint32_t value = static_cast<uint32_t>(opcode);
  for (size_t index = 0; index < kOpcodeTableSize; ++index) {
    if (kOpcodeTable[index].value == value) {
      return kOpcodeTable[index].name;
    }
  }
  return "unknown";
}

bool OpcodeUsesTestBoolean(Opcode opcode) {
  switch (static_cast<OpcodeType>(opcode)) {
  case D3D10_SB_OPCODE_BREAKC:
  case D3D10_SB_OPCODE_CALLC:
  case D3D10_SB_OPCODE_CONTINUEC:
  case D3D10_SB_OPCODE_DISCARD:
  case D3D10_SB_OPCODE_IF:
  case D3D10_SB_OPCODE_RETC:
    return true;
  default:
    return false;
  }
}

bool ParseOpcodeWithImplicitTestBoolean(const std::string &name,
                                        Opcode &opcode,
                                        int32_t &implicitTestBoolean) {
  const std::string lowered = Lowercase(name);
  implicitTestBoolean = -1;

  for (size_t index = 0; index < kOpcodeAliasCount; ++index) {
    if (lowered == kOpcodeAliases[index].name) {
      opcode = Opcode{kOpcodeAliases[index].value};
      implicitTestBoolean = kOpcodeAliases[index].testBoolean;
      return true;
    }
  }

  for (size_t index = 0; index < kOpcodeTableSize; ++index) {
    if (lowered == kOpcodeTable[index].name) {
      opcode = Opcode{kOpcodeTable[index].value};
      return true;
    }
  }

  return false;
}

bool ParseOpcode(const std::string &name, Opcode &opcode) {
  int32_t implicitTestBoolean = -1;
  return ParseOpcodeWithImplicitTestBoolean(name, opcode,
                                            implicitTestBoolean);
}

} // namespace sm5
} // namespace dxp
