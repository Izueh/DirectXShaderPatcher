#include "dxp/sm5/Model.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace dxp {
namespace sm5 {

struct OpcodeNameEntry {
  uint32_t value;
  const char *name;
};

static const OpcodeNameEntry kOpcodeTable[] = {
    {static_cast<uint32_t>(D3D10_SB_OPCODE_NOP), "nop"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MOV), "mov"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ADD), "add"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MUL), "mul"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DIV), "div"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MAD), "mad"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MIN), "min"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MAX), "max"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_FRC), "frc"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DP2), "dp2"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DP3), "dp3"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DP4), "dp4"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_MOVC), "movc"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_LT), "lt"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_GE), "ge"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_EQ), "eq"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_NE), "ne"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_AND), "and"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_OR), "or"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_XOR), "xor"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_NOT), "not"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IADD), "iadd"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ISHL), "ishl"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ISHR), "ishr"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ILT), "ilt"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IMAD), "imad"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IMUL), "imul"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_UTOF), "utof"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ITOF), "itof"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_FTOU), "ftou"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_FTOI), "ftoi"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ROUND_NE), "round_ne"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ROUND_NI), "round_ni"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ROUND_PI), "round_pi"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ROUND_Z), "round_z"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SQRT), "sqrt"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_RSQ), "rsq"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_EXP), "exp"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_LOG), "log"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SINCOS), "sincos"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ULT), "ult"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SAMPLE), "sample"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SAMPLE_B), "sample_b"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SAMPLE_C), "sample_c"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SAMPLE_D), "sample_d"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_SAMPLE_L), "sample_l"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_LD), "ld"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DISCARD), "discard"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_IF), "if"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ELSE), "else"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ENDIF), "endif"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_LOOP), "loop"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_ENDLOOP), "endloop"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_BREAK), "break"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_BREAKC), "breakc"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_CONTINUE), "continue"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_CONTINUEC), "continuec"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_CUSTOMDATA), "customdata"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_RET), "ret"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_RETC), "retc"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_RESOURCE), "dcl_resource"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER),
     "dcl_constant_buffer"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_SAMPLER), "dcl_sampler"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INDEX_RANGE), "dcl_index_range"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_GS_OUTPUT_PRIMITIVE_TOPOLOGY),
     "dcl_gs_output_primitive_topology"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_GS_INPUT_PRIMITIVE),
     "dcl_gs_input_primitive"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_MAX_OUTPUT_VERTEX_COUNT),
     "dcl_max_output_vertex_count"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT), "dcl_input"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT_SGV), "dcl_input_sgv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT_SIV), "dcl_input_siv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT_PS), "dcl_input_ps"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT_PS_SGV),
     "dcl_input_ps_sgv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INPUT_PS_SIV),
     "dcl_input_ps_siv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_OUTPUT), "dcl_output"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_OUTPUT_SGV), "dcl_output_sgv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_OUTPUT_SIV), "dcl_output_siv"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_TEMPS), "dcl_temps"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_INDEXABLE_TEMP),
     "dcl_indexable_temp"},
    {static_cast<uint32_t>(D3D10_SB_OPCODE_DCL_GLOBAL_FLAGS),
     "dcl_global_flags"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_STREAM), "dcl_stream"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_FUNCTION_BODY),
     "dcl_function_body"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_FUNCTION_TABLE),
     "dcl_function_table"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_INTERFACE), "dcl_interface"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_INPUT_CONTROL_POINT_COUNT),
     "dcl_input_control_point_count"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_OUTPUT_CONTROL_POINT_COUNT),
     "dcl_output_control_point_count"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_TESS_DOMAIN), "dcl_tess_domain"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_TESS_PARTITIONING),
     "dcl_tess_partitioning"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_TESS_OUTPUT_PRIMITIVE),
     "dcl_tess_output_primitive"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_HS_MAX_TESSFACTOR),
     "dcl_hs_max_tessfactor"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_HS_FORK_PHASE_INSTANCE_COUNT),
     "dcl_hs_fork_phase_instance_count"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_HS_JOIN_PHASE_INSTANCE_COUNT),
     "dcl_hs_join_phase_instance_count"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_THREAD_GROUP),
     "dcl_thread_group"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED),
     "dcl_unordered_access_view_typed"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW),
     "dcl_unordered_access_view_raw"},
    {static_cast<uint32_t>(
         D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED),
     "dcl_unordered_access_view_structured"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_RAW),
     "dcl_thread_group_shared_memory_raw"},
    {static_cast<uint32_t>(
         D3D11_SB_OPCODE_DCL_THREAD_GROUP_SHARED_MEMORY_STRUCTURED),
     "dcl_thread_group_shared_memory_structured"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_RESOURCE_RAW),
     "dcl_resource_raw"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED),
     "dcl_resource_structured"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_DCL_GS_INSTANCE_COUNT),
     "dcl_gs_instance_count"},
    {static_cast<uint32_t>(D3D11_SB_OPCODE_SYNC), "sync"},
};

static constexpr size_t kOpcodeTableSize =
    sizeof(kOpcodeTable) / sizeof(kOpcodeTable[0]);

const char *GetOpcodeName(Opcode opcode) {
  uint32_t val = static_cast<uint32_t>(opcode);
  for (size_t i = 0; i < kOpcodeTableSize; ++i) {
    if (kOpcodeTable[i].value == val)
      return kOpcodeTable[i].name;
  }
  return "unknown";
}

bool ParseOpcode(const std::string &name, Opcode &opcode) {
  std::string lowered = name;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(std::tolower(value));
                 });

  for (size_t i = 0; i < kOpcodeTableSize; ++i) {
    if (lowered == kOpcodeTable[i].name) {
      opcode = Opcode{kOpcodeTable[i].value};
      return true;
    }
  }
  return false;
}

} // namespace sm5
} // namespace dxp
