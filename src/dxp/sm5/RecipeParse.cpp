#include "dxp/sm5/RecipeParse.h"

#include "dxp/sm5/Match.h"
#include "dxp/sm5/Model.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "llvm/Support/YAMLTraits.h"

namespace {

static std::string Lowercase(const std::string &value) {
  std::string lowered = value;
  for (char &ch : lowered) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return lowered;
}

static dxp::sm5::RecipeRuleApplicationMode
ParseRuleApplicationMode(const std::string &value) {
  const std::string lowered = Lowercase(value);
  if (lowered == "last") {
    return dxp::sm5::RecipeRuleApplicationMode::Last;
  }
  if (lowered == "matchall" || lowered == "match_all") {
    return dxp::sm5::RecipeRuleApplicationMode::MatchAll;
  }
  return dxp::sm5::RecipeRuleApplicationMode::First;
}

struct YamlMatch {
  std::string opcode;
  std::string capture;
  std::string saturate;
  std::string interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<struct YamlOperand> operands;
  std::vector<struct YamlInstructionMatch> sequence;
};

struct YamlInstructionMatch {
  std::string opcode;
  std::string capture;
  std::string saturate;
  std::string interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<struct YamlOperand> operands;
};

struct YamlComponentSelector {
  std::string kind;
  std::string value;
};

struct YamlOperand {
  std::string type;
  std::vector<uint32_t> indices;
  std::string bind_handle;
  std::string state_temp;
  YamlComponentSelector components;
  std::string mask;
  std::string swizzle;
  std::string select;
  int32_t num_components = -1;
  std::string modifier;
  std::vector<uint32_t> immediates_u32;
  std::vector<float> immediates_f32;
  std::string capture;
  std::string match_capture;
  std::string from_capture;
  std::string scratch;
};

struct YamlEmitInstruction {
  std::string opcode;
  std::string saturate;
  std::string interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<YamlOperand> operands;
};

struct YamlRule {
  YamlMatch match;
  std::vector<YamlEmitInstruction> emit;
  std::string replace;
  std::string application_mode;
};

struct YamlStep {
  std::string name;
  bool required = true;
  std::string application_mode;
  std::vector<YamlRule> rules;
};

struct YamlPrefilter {
  std::string kind;
  std::string name;
  bool required = true;
  uint32_t major = 0;
  uint32_t minor = 0;
  std::string opcode;
  int32_t expected_count = 0;
  int32_t expected_resources = 0;
  YamlMatch match;
};

struct YamlTextureDecl {
  int32_t bind_point = -1;
  std::string dimension = "Texture2D";
  std::string handle;
  bool auto_bind = false;
};

struct YamlTempDecl {
  std::string handle;
};

struct YamlInputDecl {
  int32_t bind_point = -1;
  std::string interpolation_mode = "linear";
  std::string handle;
  bool auto_bind = false;
};

struct YamlOutputDecl {
  int32_t bind_point = -1;
  std::string handle;
  bool auto_bind = false;
};

struct YamlCBufferDecl {
  int32_t bind_point = -1;
  uint32_t elements = 1;
  std::string access_pattern = "immediateIndexed";
  std::string handle;
  bool auto_bind = false;
};

struct YamlSamplerDecl {
  int32_t bind_point = -1;
  std::string mode = "default";
  std::string handle;
  bool auto_bind = false;
};

struct YamlRawResourceDecl {
  int32_t bind_point = -1;
  std::string handle;
  bool auto_bind = false;
};

struct YamlStructuredResourceDecl {
  int32_t bind_point = -1;
  uint32_t stride = 16;
  std::string handle;
  bool auto_bind = false;
};

struct YamlUavDecl {
  int32_t bind_point = -1;
  std::string kind = "typed";
  std::string dimension = "Texture2D";
  uint32_t stride = 16;
  bool globally_coherent = false;
  bool has_counter = false;
  std::string handle;
  bool auto_bind = false;
};

struct YamlRecipeDocument {
  uint32_t version = 1;
  uint32_t reserved_temps = 0;
  std::vector<YamlPrefilter> prefilters;
  std::vector<YamlPrefilter> predicates;
  std::vector<YamlRule> rewrite_rules;
  std::vector<YamlStep> steps;
  std::vector<YamlTempDecl> temp_decls;
  std::vector<YamlTextureDecl> texture_decls;
  std::vector<YamlInputDecl> input_decls;
  std::vector<YamlOutputDecl> output_decls;
  std::vector<YamlRawResourceDecl> raw_resource_decls;
  std::vector<YamlStructuredResourceDecl> structured_resource_decls;
  std::vector<YamlCBufferDecl> cbuffer_decls;
  std::vector<YamlSamplerDecl> sampler_decls;
  std::vector<YamlUavDecl> uav_decls;
};

} // namespace

LLVM_YAML_IS_SEQUENCE_VECTOR(uint32_t)
LLVM_YAML_IS_SEQUENCE_VECTOR(float)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlOperand)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlInstructionMatch)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlEmitInstruction)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRule)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlStep)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlPrefilter)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlTempDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlTextureDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlInputDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlOutputDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRawResourceDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlStructuredResourceDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlCBufferDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlSamplerDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlUavDecl)

namespace llvm {
namespace yaml {

template <> struct MappingTraits<YamlComponentSelector> {
  static void mapping(IO &io, YamlComponentSelector &selector) {
    io.mapOptional("kind", selector.kind);
    io.mapOptional("value", selector.value);
  }
};

template <> struct MappingTraits<YamlOperand> {
  static void mapping(IO &io, YamlOperand &operand) {
    io.mapOptional("type", operand.type);
    io.mapOptional("indices", operand.indices);
    io.mapOptional("bind_handle", operand.bind_handle);
    io.mapOptional("state_temp", operand.state_temp);
    io.mapOptional("components", operand.components);
    io.mapOptional("mask", operand.mask);
    io.mapOptional("swizzle", operand.swizzle);
    io.mapOptional("select", operand.select);
    io.mapOptional("num_components", operand.num_components, -1);
    io.mapOptional("modifier", operand.modifier);
    io.mapOptional("immediates_u32", operand.immediates_u32);
    io.mapOptional("immediates_f32", operand.immediates_f32);
    io.mapOptional("capture", operand.capture);
    io.mapOptional("match_capture", operand.match_capture);
    io.mapOptional("from_capture", operand.from_capture);
    io.mapOptional("scratch", operand.scratch);
  }
};

template <> struct MappingTraits<YamlInstructionMatch> {
  static void mapping(IO &io, YamlInstructionMatch &match) {
    io.mapRequired("opcode", match.opcode);
    io.mapOptional("capture", match.capture);
    io.mapOptional("saturate", match.saturate);
    io.mapOptional("interpolation_mode", match.interpolation_mode);
    io.mapOptional("test_boolean", match.test_boolean, -1);
    io.mapOptional("operands", match.operands);
  }
};

template <> struct MappingTraits<YamlMatch> {
  static void mapping(IO &io, YamlMatch &match) {
    io.mapOptional("opcode", match.opcode);
    io.mapOptional("capture", match.capture);
    io.mapOptional("saturate", match.saturate);
    io.mapOptional("interpolation_mode", match.interpolation_mode);
    io.mapOptional("test_boolean", match.test_boolean, -1);
    io.mapOptional("operands", match.operands);
    io.mapOptional("sequence", match.sequence);
  }
};

template <> struct MappingTraits<YamlEmitInstruction> {
  static void mapping(IO &io, YamlEmitInstruction &emit) {
    io.mapRequired("opcode", emit.opcode);
    io.mapOptional("saturate", emit.saturate);
    io.mapOptional("interpolation_mode", emit.interpolation_mode);
    io.mapOptional("test_boolean", emit.test_boolean, -1);
    io.mapOptional("operands", emit.operands);
  }
};

template <> struct MappingTraits<YamlRule> {
  static void mapping(IO &io, YamlRule &rule) {
    io.mapOptional("match", rule.match);
    io.mapOptional("emit", rule.emit);
    io.mapOptional("replace", rule.replace);
    io.mapOptional("application_mode", rule.application_mode);
  }
};

template <> struct MappingTraits<YamlStep> {
  static void mapping(IO &io, YamlStep &step) {
    io.mapOptional("name", step.name);
    io.mapOptional("required", step.required, true);
    io.mapOptional("application_mode", step.application_mode);
    io.mapOptional("rules", step.rules);
  }
};

template <> struct MappingTraits<YamlPrefilter> {
  static void mapping(IO &io, YamlPrefilter &prefilter) {
    io.mapRequired("kind", prefilter.kind);
    io.mapOptional("name", prefilter.name);
    io.mapOptional("required", prefilter.required, true);
    io.mapOptional("major", prefilter.major, 0u);
    io.mapOptional("minor", prefilter.minor, 0u);
    io.mapOptional("opcode", prefilter.opcode);
    io.mapOptional("expected_count", prefilter.expected_count, 0);
    io.mapOptional("expected_resources", prefilter.expected_resources, 0);
    io.mapOptional("match", prefilter.match);
  }
};

template <> struct MappingTraits<YamlTextureDecl> {
  static void mapping(IO &io, YamlTextureDecl &decl) {
    io.mapOptional("bind_point", decl.bind_point, -1);
    io.mapOptional("dimension", decl.dimension, std::string("Texture2D"));
    io.mapOptional("handle", decl.handle);
    io.mapOptional("auto_bind", decl.auto_bind, false);
  }
};

template <> struct MappingTraits<YamlTempDecl> {
  static void mapping(IO &io, YamlTempDecl &decl) {
    io.mapOptional("handle", decl.handle);
  }
};

template <> struct MappingTraits<YamlInputDecl> {
  static void mapping(IO &io, YamlInputDecl &decl) {
    io.mapOptional("bind_point", decl.bind_point, -1);
    io.mapOptional("interpolation_mode", decl.interpolation_mode,
                   std::string("linear"));
    io.mapOptional("handle", decl.handle);
    io.mapOptional("auto_bind", decl.auto_bind, false);
  }
};

template <> struct MappingTraits<YamlOutputDecl> {
  static void mapping(IO &io, YamlOutputDecl &decl) {
    io.mapOptional("bind_point", decl.bind_point, -1);
    io.mapOptional("handle", decl.handle);
    io.mapOptional("auto_bind", decl.auto_bind, false);
  }
};

template <> struct MappingTraits<YamlCBufferDecl> {
  static void mapping(IO &io, YamlCBufferDecl &decl) {
    io.mapOptional("bind_point", decl.bind_point, -1);
    io.mapOptional("elements", decl.elements, 1u);
    io.mapOptional("access_pattern", decl.access_pattern,
                   std::string("immediateIndexed"));
    io.mapOptional("handle", decl.handle);
    io.mapOptional("auto_bind", decl.auto_bind, false);
  }
};

template <> struct MappingTraits<YamlSamplerDecl> {
  static void mapping(IO &io, YamlSamplerDecl &decl) {
    io.mapOptional("bind_point", decl.bind_point, -1);
    io.mapOptional("mode", decl.mode, std::string("default"));
    io.mapOptional("handle", decl.handle);
    io.mapOptional("auto_bind", decl.auto_bind, false);
  }
};

template <> struct MappingTraits<YamlRawResourceDecl> {
  static void mapping(IO &io, YamlRawResourceDecl &decl) {
    io.mapOptional("bind_point", decl.bind_point, -1);
    io.mapOptional("handle", decl.handle);
    io.mapOptional("auto_bind", decl.auto_bind, false);
  }
};

template <> struct MappingTraits<YamlStructuredResourceDecl> {
  static void mapping(IO &io, YamlStructuredResourceDecl &decl) {
    io.mapOptional("bind_point", decl.bind_point, -1);
    io.mapOptional("stride", decl.stride, 16u);
    io.mapOptional("handle", decl.handle);
    io.mapOptional("auto_bind", decl.auto_bind, false);
  }
};

template <> struct MappingTraits<YamlUavDecl> {
  static void mapping(IO &io, YamlUavDecl &decl) {
    io.mapOptional("bind_point", decl.bind_point, -1);
    io.mapOptional("kind", decl.kind, std::string("typed"));
    io.mapOptional("dimension", decl.dimension, std::string("Texture2D"));
    io.mapOptional("stride", decl.stride, 16u);
    io.mapOptional("globally_coherent", decl.globally_coherent, false);
    io.mapOptional("has_counter", decl.has_counter, false);
    io.mapOptional("handle", decl.handle);
    io.mapOptional("auto_bind", decl.auto_bind, false);
  }
};

template <> struct MappingTraits<YamlRecipeDocument> {
  static void mapping(IO &io, YamlRecipeDocument &document) {
    io.mapOptional("version", document.version, 1u);
    io.mapOptional("reserved_temps", document.reserved_temps, 0u);
    io.mapOptional("prefilters", document.prefilters);
    io.mapOptional("predicates", document.predicates);
    io.mapOptional("rewrite_rules", document.rewrite_rules);
    io.mapOptional("steps", document.steps);
    io.mapOptional("temp_decls", document.temp_decls);
    io.mapOptional("texture_decls", document.texture_decls);
    io.mapOptional("input_decls", document.input_decls);
    io.mapOptional("output_decls", document.output_decls);
    io.mapOptional("raw_resource_decls", document.raw_resource_decls);
    io.mapOptional("structured_resource_decls", document.structured_resource_decls);
    io.mapOptional("cbuffer_decls", document.cbuffer_decls);
    io.mapOptional("sampler_decls", document.sampler_decls);
    io.mapOptional("uav_decls", document.uav_decls);
  }
};

} // namespace yaml
} // namespace llvm

namespace dxp {
namespace sm5 {

namespace {

static bool ParseTextureDimensionToken(const std::string &value,
                                       uint32_t &dimension,
                                       std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered == "texture1d") {
    dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE1D;
    return true;
  }
  if (lowered == "texture2d") {
    dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE2D;
    return true;
  }
  if (lowered == "texture2darray") {
    dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE2DARRAY;
    return true;
  }
  if (lowered == "texture3d") {
    dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURE3D;
    return true;
  }
  if (lowered == "texturecube") {
    dimension = D3D10_SB_RESOURCE_DIMENSION_TEXTURECUBE;
    return true;
  }

  error = "unsupported SM5 texture dimension: " + value;
  return false;
}

static bool ParseCBufferAccessPatternToken(const std::string &value,
                                           uint32_t &accessPattern,
                                           std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered == "immediateindexed" || lowered == "immediate_indexed") {
    accessPattern = D3D10_SB_CONSTANT_BUFFER_IMMEDIATE_INDEXED;
    return true;
  }
  if (lowered == "dynamicindexed" || lowered == "dynamic_indexed") {
    accessPattern = D3D10_SB_CONSTANT_BUFFER_DYNAMIC_INDEXED;
    return true;
  }

  error = "unsupported SM5 cbuffer access pattern: " + value;
  return false;
}

static bool ParseSamplerModeToken(const std::string &value,
                                  uint32_t &mode,
                                  std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered == "default") {
    mode = D3D10_SB_SAMPLER_MODE_DEFAULT;
    return true;
  }
  if (lowered == "comparison") {
    mode = D3D10_SB_SAMPLER_MODE_COMPARISON;
    return true;
  }
  if (lowered == "mono") {
    mode = D3D10_SB_SAMPLER_MODE_MONO;
    return true;
  }

  error = "unsupported SM5 sampler mode: " + value;
  return false;
}

static bool ParseUavKindToken(const std::string &value,
                              RecipeUavKind &kind,
                              std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered == "typed") {
    kind = RecipeUavKind::Typed;
    return true;
  }
  if (lowered == "raw") {
    kind = RecipeUavKind::Raw;
    return true;
  }
  if (lowered == "structured") {
    kind = RecipeUavKind::Structured;
    return true;
  }

  error = "unsupported SM5 uav kind: " + value;
  return false;
}

static bool ParsePrefilterKindToken(const std::string &value,
                                    PrefilterKind &kind,
                                    std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered == "check_shader_version") {
    kind = PrefilterKind::CheckShaderVersion;
    return true;
  }
  if (lowered == "check_opcode_count") {
    kind = PrefilterKind::CheckOpcodeCount;
    return true;
  }
  if (lowered == "check_resource_count") {
    kind = PrefilterKind::CheckResourceCount;
    return true;
  }
  if (lowered == "check_pattern_match") {
    kind = PrefilterKind::CheckPatternMatch;
    return true;
  }

  error = "unsupported SM5 prefilter kind: " + value;
  return false;
}

static bool ParseBoolToken(const std::string &value,
                           bool &parsedValue,
                           std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered == "true") {
    parsedValue = true;
    return true;
  }
  if (lowered == "false") {
    parsedValue = false;
    return true;
  }

  error = "expected boolean token, got '" + value + "'";
  return false;
}

static bool ParseInterpolationModeToken(const std::string &value,
                                        uint32_t &mode,
                                        std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered == "undefined") {
    mode = D3D10_SB_INTERPOLATION_UNDEFINED;
    return true;
  }
  if (lowered == "constant") {
    mode = D3D10_SB_INTERPOLATION_CONSTANT;
    return true;
  }
  if (lowered == "linear") {
    mode = D3D10_SB_INTERPOLATION_LINEAR;
    return true;
  }
  if (lowered == "linear_centroid" || lowered == "linearcentroid") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_CENTROID;
    return true;
  }
  if (lowered == "linear_noperspective" || lowered == "linearnoperspective") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE;
    return true;
  }
  if (lowered == "linear_noperspective_centroid" ||
      lowered == "linearnoperspectivecentroid") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_CENTROID;
    return true;
  }
  if (lowered == "linear_sample" || lowered == "linearsample") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_SAMPLE;
    return true;
  }
  if (lowered == "linear_noperspective_sample" ||
      lowered == "linearnoperspectivesample") {
    mode = D3D10_SB_INTERPOLATION_LINEAR_NOPERSPECTIVE_SAMPLE;
    return true;
  }

  error = "unsupported SM5 interpolation mode: " + value;
  return false;
}

static uint32_t FloatAsUint(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

static bool ParseOperandType(const std::string &value,
                             OperandType &type,
                             std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered == "temp") {
    type = D3D10_SB_OPERAND_TYPE_TEMP;
    return true;
  }
  if (lowered == "input") {
    type = D3D10_SB_OPERAND_TYPE_INPUT;
    return true;
  }
  if (lowered == "output") {
    type = D3D10_SB_OPERAND_TYPE_OUTPUT;
    return true;
  }
  if (lowered == "indexable_temp") {
    type = D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP;
    return true;
  }
  if (lowered == "immediate32") {
    type = D3D10_SB_OPERAND_TYPE_IMMEDIATE32;
    return true;
  }
  if (lowered == "immediate64") {
    type = D3D10_SB_OPERAND_TYPE_IMMEDIATE64;
    return true;
  }
  if (lowered == "sampler") {
    type = D3D10_SB_OPERAND_TYPE_SAMPLER;
    return true;
  }
  if (lowered == "resource") {
    type = D3D10_SB_OPERAND_TYPE_RESOURCE;
    return true;
  }
  if (lowered == "unordered_access_view" || lowered == "uav") {
    type = D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW;
    return true;
  }
  if (lowered == "constant_buffer" || lowered == "cbuffer") {
    type = D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER;
    return true;
  }
  if (lowered == "output_depth") {
    type = D3D10_SB_OPERAND_TYPE_OUTPUT_DEPTH;
    return true;
  }

  char *end = nullptr;
  const unsigned long parsed = std::strtoul(value.c_str(), &end, 0);
  if (end != nullptr && *end == '\0') {
    type = static_cast<OperandType>(parsed);
    return true;
  }

  error = "unsupported SM5 operand type: " + value;
  return false;
}

static bool ParseOperandModifier(const std::string &value,
                                 OperandModifier &modifier,
                                 std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered.empty() || lowered == "none") {
    modifier = D3D10_SB_OPERAND_MODIFIER_NONE;
    return true;
  }
  if (lowered == "neg" || lowered == "minus") {
    modifier = D3D10_SB_OPERAND_MODIFIER_NEG;
    return true;
  }
  if (lowered == "abs") {
    modifier = D3D10_SB_OPERAND_MODIFIER_ABS;
    return true;
  }
  if (lowered == "absneg" || lowered == "abs_neg") {
    modifier = D3D10_SB_OPERAND_MODIFIER_ABSNEG;
    return true;
  }

  error = "unsupported SM5 operand modifier: " + value;
  return false;
}

static bool TryParseComponentChar(char ch,
                                  D3D10_SB_4_COMPONENT_NAME &component,
                                  std::string &error) {
  switch (static_cast<char>(std::tolower(static_cast<unsigned char>(ch)))) {
    case 'x':
      component = D3D10_SB_4_COMPONENT_X;
      return true;
    case 'y':
      component = D3D10_SB_4_COMPONENT_Y;
      return true;
    case 'z':
      component = D3D10_SB_4_COMPONENT_Z;
      return true;
    case 'w':
      component = D3D10_SB_4_COMPONENT_W;
      return true;
    default:
      error = std::string("unsupported SM5 component selector: ") + ch;
      return false;
  }
}

static bool ParseOperandComponentMode(const YamlOperand &operandModel,
                                      OperandType operandType,
                                      bool portableV2,
                                      uint32_t &numComponents,
                                      uint32_t &componentMode,
                                      std::string &error) {
  const bool hasSelectorObject = !operandModel.components.kind.empty() ||
                                 !operandModel.components.value.empty();
  const bool hasLegacySelectors = !operandModel.select.empty() ||
                                  !operandModel.mask.empty() ||
                                  !operandModel.swizzle.empty();

  if (portableV2) {
    if (hasLegacySelectors) {
      error = "portable operands require components.kind/components.value instead of mask/swizzle/select";
      return false;
    }
  }

  if (hasSelectorObject && hasLegacySelectors) {
    error = "operand components cannot mix components with mask/swizzle/select";
    return false;
  }

  std::string selectToken = operandModel.select;
  std::string maskToken = operandModel.mask;
  std::string swizzleToken = operandModel.swizzle;
  if (hasSelectorObject) {
    const std::string kind = Lowercase(operandModel.components.kind);
    if (kind.empty()) {
      error = "operand components.kind is required when components is present";
      return false;
    }
    if (operandModel.components.value.empty()) {
      error = "operand components.value is required when components is present";
      return false;
    }

    if (kind == "select") {
      selectToken = operandModel.components.value;
    } else if (kind == "mask") {
      maskToken = operandModel.components.value;
    } else if (kind == "swizzle") {
      swizzleToken = operandModel.components.value;
    } else {
      error = "unsupported operand components.kind: " + operandModel.components.kind;
      return false;
    }
  }

  if (!selectToken.empty()) {
    D3D10_SB_4_COMPONENT_NAME component = D3D10_SB_4_COMPONENT_X;
    if (selectToken.size() != 1 ||
        !TryParseComponentChar(selectToken.front(), component,
                               error)) {
      return false;
    }
    numComponents = D3D10_SB_OPERAND_4_COMPONENT;
    componentMode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                        D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) |
                    ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(component);
    return true;
  }

  if (!maskToken.empty()) {
    uint32_t mask = 0;
    for (char ch : maskToken) {
      switch (static_cast<char>(std::tolower(static_cast<unsigned char>(ch)))) {
        case 'x':
          mask |= D3D10_SB_OPERAND_4_COMPONENT_MASK_X;
          break;
        case 'y':
          mask |= D3D10_SB_OPERAND_4_COMPONENT_MASK_Y;
          break;
        case 'z':
          mask |= D3D10_SB_OPERAND_4_COMPONENT_MASK_Z;
          break;
        case 'w':
          mask |= D3D10_SB_OPERAND_4_COMPONENT_MASK_W;
          break;
        default:
          error = std::string("unsupported SM5 mask component: ") + ch;
          return false;
      }
    }
    numComponents = D3D10_SB_OPERAND_4_COMPONENT;
    componentMode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                        D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
                    ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(mask);
    return true;
  }

  if (!swizzleToken.empty()) {
    if (swizzleToken.size() != 4) {
      error = "SM5 swizzle requires exactly four components";
      return false;
    }
    D3D10_SB_4_COMPONENT_NAME components[4] = {};
    for (size_t index = 0; index < 4; ++index) {
      if (!TryParseComponentChar(swizzleToken[index],
                                 components[index], error)) {
        return false;
      }
    }
    numComponents = D3D10_SB_OPERAND_4_COMPONENT;
    componentMode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                        D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) |
                    ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(
                        components[0], components[1], components[2],
                        components[3]);
    return true;
  }

  if (operandModel.num_components >= 0) {
    numComponents = static_cast<uint32_t>(operandModel.num_components);
  } else if (operandType == D3D10_SB_OPERAND_TYPE_SAMPLER ||
             operandType == D3D10_SB_OPERAND_TYPE_RESOURCE ||
             operandType == D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW) {
    numComponents = D3D10_SB_OPERAND_0_COMPONENT;
  } else if (operandType == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
             operandType == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) {
    numComponents = operandModel.immediates_u32.size() > 1 ||
                            operandModel.immediates_f32.size() > 1
                        ? D3D10_SB_OPERAND_4_COMPONENT
                        : D3D10_SB_OPERAND_1_COMPONENT;
  } else {
    numComponents = D3D10_SB_OPERAND_4_COMPONENT;
  }

  componentMode = 0;
  if (numComponents == D3D10_SB_OPERAND_4_COMPONENT) {
    componentMode = D3D10_SB_OPERAND_4_COMPONENT_NOSWIZZLE;
  }
  return true;
}

static bool ParseEmitOperand(const YamlOperand &operandModel,
                             bool portableV2,
                             Operand &operand,
                             std::string &error) {
  operand = Operand{};

  const std::string captureRef = !operandModel.from_capture.empty()
                                     ? operandModel.from_capture
                                     : operandModel.capture;

  if (portableV2 && !operandModel.from_capture.empty()) {
    error = "portable emit operands use capture instead of from_capture";
    return false;
  }

  if (!captureRef.empty() && !operandModel.scratch.empty()) {
    error = "SM5 emit operand cannot use both capture and scratch";
    return false;
  }

  if (!captureRef.empty() && !operandModel.state_temp.empty()) {
    error = "SM5 emit operand cannot use both capture and state_temp";
    return false;
  }

  if (!captureRef.empty()) {
    operand.CaptureName = captureRef;
    return true;
  }

  if (operandModel.type.empty() && operandModel.scratch.empty() &&
      operandModel.state_temp.empty()) {
    error = "literal SM5 emit operands require type, capture, scratch, or state_temp";
    return false;
  }

  if (!operandModel.bind_handle.empty() && !operandModel.scratch.empty()) {
    error = "SM5 emit operand cannot use both bind_handle and scratch";
    return false;
  }

  if (!operandModel.bind_handle.empty() && !operandModel.type.empty()) {
    OperandType parsedType = D3D10_SB_OPERAND_TYPE_TEMP;
    if (!ParseOperandType(operandModel.type, parsedType, error)) {
      return false;
    }
  }

  if (!operandModel.state_temp.empty() && !operandModel.scratch.empty()) {
    error = "SM5 emit operand cannot use both state_temp and scratch";
    return false;
  }

  if (!operandModel.state_temp.empty() && !operandModel.bind_handle.empty()) {
    error = "SM5 emit operand cannot use both state_temp and bind_handle";
    return false;
  }

  if (!operandModel.scratch.empty() || !operandModel.state_temp.empty()) {
    operand.Type = D3D10_SB_OPERAND_TYPE_TEMP;
  } else if (!ParseOperandType(operandModel.type, operand.Type, error)) {
    return false;
  }

  if (!operandModel.scratch.empty() && !operandModel.type.empty()) {
    OperandType parsedType = D3D10_SB_OPERAND_TYPE_TEMP;
    if (!ParseOperandType(operandModel.type, parsedType, error)) {
      return false;
    }
    if (parsedType != D3D10_SB_OPERAND_TYPE_TEMP) {
      error = "SM5 scratch operands must use temp type";
      return false;
    }
  }

  if (!operandModel.state_temp.empty() && !operandModel.type.empty()) {
    OperandType parsedType = D3D10_SB_OPERAND_TYPE_TEMP;
    if (!ParseOperandType(operandModel.type, parsedType, error)) {
      return false;
    }
    if (parsedType != D3D10_SB_OPERAND_TYPE_TEMP) {
      error = "SM5 state_temp operands must use temp type";
      return false;
    }
  }

  if (!ParseOperandComponentMode(operandModel, operand.Type,
                                 portableV2,
                                 operand.NumComponents,
                                 operand.ComponentMode, error)) {
    return false;
  }

  operand.Indices = operandModel.indices;
  operand.BindHandle = operandModel.bind_handle;
  operand.StateTempName = operandModel.state_temp;
  operand.ScratchName = operandModel.scratch;
  operand.ImmediateValues = operandModel.immediates_u32;
  for (float immediateValue : operandModel.immediates_f32) {
    operand.ImmediateValues.push_back(FloatAsUint(immediateValue));
  }

  if (!operandModel.modifier.empty() &&
      !ParseOperandModifier(operandModel.modifier, operand.Modifier, error)) {
    return false;
  }

  return true;
}

static bool ParseMatchOperand(const YamlOperand &operandModel,
                              bool portableV2,
                              OperandMatch &operandMatch,
                              std::string &error) {
  operandMatch = OperandMatch{};

  if (!operandModel.type.empty()) {
    if (!ParseOperandType(operandModel.type, operandMatch.MatchType, error)) {
      return false;
    }
    operandMatch.HasTypeMatch = true;
  }

  if (!operandModel.indices.empty()) {
    operandMatch.MatchIndices.assign(operandModel.indices.begin(),
                                     operandModel.indices.end());
    operandMatch.HasIndexMatch = true;
  }

  OperandType componentType = operandMatch.HasTypeMatch ? operandMatch.MatchType
                                                        : D3D10_SB_OPERAND_TYPE_TEMP;
  uint32_t numComponents = 0;
  uint32_t componentMode = 0;
  if (!operandModel.mask.empty() || !operandModel.swizzle.empty() ||
      !operandModel.components.kind.empty() ||
      !operandModel.components.value.empty() ||
      !operandModel.select.empty() || operandModel.num_components >= 0) {
    if (!ParseOperandComponentMode(operandModel, componentType, portableV2, numComponents,
                     componentMode, error)) {
      return false;
    }
    operandMatch.MatchNumComponents = numComponents;
    operandMatch.HasNumComponentsMatch = true;
    operandMatch.MatchComponentMode = componentMode;
    operandMatch.HasComponentMatch = true;
  }

  if (!operandModel.modifier.empty()) {
    if (!ParseOperandModifier(operandModel.modifier,
                  operandMatch.MatchModifier, error)) {
      return false;
    }
    operandMatch.HasModifierMatch = true;
  }

  if (!operandModel.immediates_u32.empty() ||
      !operandModel.immediates_f32.empty()) {
    operandMatch.MatchImmediates = operandModel.immediates_u32;
    for (float immediateValue : operandModel.immediates_f32) {
      operandMatch.MatchImmediates.push_back(FloatAsUint(immediateValue));
    }
    operandMatch.HasImmediateMatch = true;
  }

  operandMatch.CaptureName = operandModel.capture;
  operandMatch.MatchAgainstCapture = operandModel.match_capture;
  return true;
}

static bool FillRecipeOperandPattern(const YamlOperand &operandModel,
                                     bool portableV2,
                                     bool forEmit,
                                     RecipeOperandPattern &operandPattern,
                                     std::string &error) {
  operandPattern = RecipeOperandPattern{};
  operandPattern.Type = operandModel.type;
  operandPattern.Indices = operandModel.indices;
  operandPattern.BindHandle = operandModel.bind_handle;
  operandPattern.StateTemp = operandModel.state_temp;
  operandPattern.NumComponents = operandModel.num_components;
  operandPattern.Modifier = operandModel.modifier;
  operandPattern.ImmediateU32 = operandModel.immediates_u32;
  operandPattern.ImmediateF32 = operandModel.immediates_f32;
  operandPattern.Capture = operandModel.capture;
  operandPattern.MatchCapture = operandModel.match_capture;
  operandPattern.Scratch = operandModel.scratch;

  if (!operandModel.components.kind.empty() || !operandModel.components.value.empty()) {
    const std::string kind = Lowercase(operandModel.components.kind);
    if (kind == "mask") {
      operandPattern.Mask = operandModel.components.value;
    } else if (kind == "swizzle") {
      operandPattern.Swizzle = operandModel.components.value;
    } else if (kind == "select") {
      operandPattern.Select = operandModel.components.value;
    } else {
      error = "unsupported operand components.kind: " + operandModel.components.kind;
      return false;
    }
  } else {
    operandPattern.Mask = operandModel.mask;
    operandPattern.Swizzle = operandModel.swizzle;
    operandPattern.Select = operandModel.select;
  }

  if (forEmit) {
    if (portableV2) {
      if (!operandModel.from_capture.empty()) {
        error = "portable emit operands use capture instead of from_capture";
        return false;
      }
      operandPattern.FromCapture = operandModel.capture;
      operandPattern.Capture.clear();
    } else {
      operandPattern.FromCapture = operandModel.from_capture;
    }
  } else {
    operandPattern.FromCapture.clear();
  }

  return true;
}

static bool ParseInstructionMatch(
    const YamlInstructionMatch &matchModel,
    bool portableV2,
    InstructionMatch &instructionMatch,
    std::string &error) {
  instructionMatch = InstructionMatch{};

  if (!ParseOpcode(matchModel.opcode, instructionMatch.Opcode)) {
    error = "Unknown SM5 opcode in match: " + matchModel.opcode;
    return false;
  }
  instructionMatch.HasOpcode = true;
  instructionMatch.CaptureName = matchModel.capture;
  if (!matchModel.saturate.empty()) {
    bool saturate = false;
    if (!ParseBoolToken(matchModel.saturate, saturate, error)) {
      error = "invalid SM5 saturate value: " + error;
      return false;
    }
    instructionMatch.HasSaturateMatch = true;
    instructionMatch.SaturateValue = saturate;
  }
  if (matchModel.test_boolean >= 0) {
    instructionMatch.HasTestBooleanMatch = true;
    instructionMatch.MatchTestBoolean =
        static_cast<uint32_t>(matchModel.test_boolean);
  }
  if (!matchModel.interpolation_mode.empty()) {
    uint32_t interpolationMode = 0;
    if (!ParseInterpolationModeToken(matchModel.interpolation_mode,
                                     interpolationMode, error)) {
      return false;
    }
    if (instructionMatch.Opcode != Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS} &&
        instructionMatch.Opcode != Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS_SIV}) {
      error =
          "SM5 interpolation_mode is only valid for dcl_input_ps and dcl_input_ps_siv";
      return false;
    }
    instructionMatch.HasInputInterpolationModeMatch = true;
    instructionMatch.MatchInputInterpolationMode = interpolationMode;
  }
  for (const YamlOperand &operandModel : matchModel.operands) {
    OperandMatch operandMatch;
    if (!ParseMatchOperand(operandModel, portableV2, operandMatch, error)) {
      return false;
    }
    instructionMatch.OperandPatterns.push_back(std::move(operandMatch));
  }

  return true;
}

static bool ParseRule(const YamlRule &ruleModel,
                      bool portableV2,
                      RecipeRuleApplicationMode inheritedMode,
                      RecipeRule &rule,
                      std::string &error) {
  rule = RecipeRule{};
  rule.ApplicationMode = inheritedMode;

  if (!ruleModel.application_mode.empty()) {
    rule.ApplicationMode = ParseRuleApplicationMode(ruleModel.application_mode);
  }

  if (!ruleModel.match.sequence.empty()) {
    if (!ruleModel.match.opcode.empty() || !ruleModel.match.capture.empty() ||
      !ruleModel.match.saturate.empty() || !ruleModel.match.interpolation_mode.empty() ||
        ruleModel.match.test_boolean >= 0 || !ruleModel.match.operands.empty()) {
      error =
          "SM5 match.sequence cannot be combined with single-instruction match fields";
      return false;
    }

    for (const YamlInstructionMatch &matchModel : ruleModel.match.sequence) {
      InstructionMatch instructionMatch;
      if (!ParseInstructionMatch(matchModel, portableV2, instructionMatch, error)) {
        return false;
      }
      RecipeInstructionPattern pattern;
      pattern.Opcode = matchModel.opcode;
      pattern.Capture = matchModel.capture;
      pattern.Saturate = matchModel.saturate;
      pattern.InterpolationMode = matchModel.interpolation_mode;
      pattern.TestBoolean = matchModel.test_boolean;
      for (const YamlOperand &operandModel : matchModel.operands) {
        RecipeOperandPattern operand;
        if (!FillRecipeOperandPattern(operandModel, portableV2, false, operand, error)) {
          return false;
        }
        pattern.Operands.push_back(std::move(operand));
      }
      rule.Match.Sequence.push_back(std::move(pattern));
    }
  } else if (!ruleModel.match.opcode.empty()) {
    Opcode parsedOpcode;
    if (!ParseOpcode(ruleModel.match.opcode, parsedOpcode)) {
      error = "Unknown SM5 opcode in match: " + ruleModel.match.opcode;
      return false;
    }
    rule.Match.Opcode = ruleModel.match.opcode;
    rule.Match.Capture = ruleModel.match.capture;
    if (!ruleModel.match.saturate.empty()) {
      bool saturate = false;
      if (!ParseBoolToken(ruleModel.match.saturate, saturate, error)) {
        error = "invalid SM5 saturate value: " + error;
        return false;
      }
    }
    rule.Match.Saturate = ruleModel.match.saturate;
    rule.Match.InterpolationMode = ruleModel.match.interpolation_mode;
    rule.Match.TestBoolean = ruleModel.match.test_boolean;
    for (const YamlOperand &operandModel : ruleModel.match.operands) {
      OperandMatch operandMatch;
      if (!ParseMatchOperand(operandModel, portableV2, operandMatch, error)) {
        return false;
      }
      RecipeOperandPattern operand;
      if (!FillRecipeOperandPattern(operandModel, portableV2, false, operand, error)) {
        return false;
      }
      rule.Match.Operands.push_back(std::move(operand));
    }
  } else {
    error = "SM5 rules require match.opcode or match.sequence";
    return false;
  }

  rule.Replace = ruleModel.replace;

  for (const YamlEmitInstruction &emitModel : ruleModel.emit) {
    if (emitModel.opcode.empty()) {
      error = "SM5 emit entries require opcode";
      return false;
    }

    Instruction instruction;
    if (!ParseOpcode(emitModel.opcode, instruction.Opcode)) {
      error = "Unknown SM5 opcode in emit: " + emitModel.opcode;
      return false;
    }
    if (!emitModel.saturate.empty()) {
      bool saturate = false;
      if (!ParseBoolToken(emitModel.saturate, saturate, error)) {
        error = "invalid SM5 emit saturate value: " + error;
        return false;
      }
      instruction.Controls.Saturate = saturate;
    }
    if (emitModel.test_boolean >= 0) {
      instruction.Controls.HasTestBoolean = true;
      instruction.Controls.TestBoolean =
          static_cast<uint32_t>(emitModel.test_boolean);
    }
    if (!emitModel.interpolation_mode.empty()) {
      uint32_t interpolationMode = 0;
      if (!ParseInterpolationModeToken(emitModel.interpolation_mode,
                                       interpolationMode, error)) {
        return false;
      }

      const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
      if (opcode != D3D10_SB_OPCODE_DCL_INPUT_PS &&
          opcode != D3D10_SB_OPCODE_DCL_INPUT_PS_SIV) {
        error =
            "SM5 interpolation_mode is only valid for dcl_input_ps and dcl_input_ps_siv";
        return false;
      }

      instruction.Controls.HasInputInterpolationMode = true;
      instruction.Controls.InputInterpolationMode = interpolationMode;
    }
    for (const YamlOperand &operandModel : emitModel.operands) {
      Operand operand;
      if (!ParseEmitOperand(operandModel, portableV2, operand, error)) {
        return false;
      }
      instruction.Operands.push_back(std::move(operand));
    }
    RecipeInstructionTemplate emitInstruction;
    emitInstruction.Opcode = emitModel.opcode;
    emitInstruction.Saturate = emitModel.saturate;
    emitInstruction.InterpolationMode = emitModel.interpolation_mode;
    emitInstruction.TestBoolean = emitModel.test_boolean;
    for (const YamlOperand &operandModel : emitModel.operands) {
      RecipeOperandPattern operandPattern;
      if (!FillRecipeOperandPattern(operandModel, portableV2, true,
                                    operandPattern, error)) {
        return false;
      }
      emitInstruction.Operands.push_back(std::move(operandPattern));
    }
    rule.Emit.push_back(std::move(emitInstruction));
  }

  return true;
}

static bool IsEmptyYamlMatch(const YamlMatch &match) {
  return match.opcode.empty() && match.capture.empty() && match.saturate.empty() &&
         match.interpolation_mode.empty() &&
         match.test_boolean < 0 && match.operands.empty() && match.sequence.empty();
}

static bool ValidatePrefilterModel(const YamlPrefilter &prefilter,
                                   PrefilterKind kind,
                                   std::string &error) {
  switch (kind) {
    case PrefilterKind::CheckShaderVersion:
    case PrefilterKind::CheckResourceCount:
      return true;
    case PrefilterKind::CheckOpcodeCount:
      if (prefilter.opcode.empty()) {
        error = "SM5 check_opcode_count prefilter requires opcode";
        return false;
      }
      return true;
    case PrefilterKind::CheckPatternMatch:
      if (IsEmptyYamlMatch(prefilter.match)) {
        error = "SM5 check_pattern_match prefilter requires match.opcode or match.sequence";
        return false;
      }
      if (!prefilter.match.sequence.empty() &&
          (!prefilter.match.opcode.empty() || !prefilter.match.capture.empty() ||
           !prefilter.match.saturate.empty() || !prefilter.match.interpolation_mode.empty() ||
           prefilter.match.test_boolean >= 0 ||
           !prefilter.match.operands.empty())) {
        error =
            "SM5 prefilter match.sequence cannot be combined with single-instruction match fields";
        return false;
      }
      if (prefilter.match.sequence.empty() && prefilter.match.opcode.empty()) {
        error = "SM5 check_pattern_match prefilter requires match.opcode when match.sequence is omitted";
        return false;
      }
      return true;
  }

  return false;
}

static bool ValidateUniqueDeclarationHandles(
  const std::vector<YamlTempDecl> &tempDecls,
  const std::vector<YamlInputDecl> &inputDecls,
  const std::vector<YamlOutputDecl> &outputDecls,
    const std::vector<YamlTextureDecl> &textureDecls,
    const std::vector<YamlRawResourceDecl> &rawResourceDecls,
    const std::vector<YamlStructuredResourceDecl> &structuredResourceDecls,
    const std::vector<YamlCBufferDecl> &cbufferDecls,
    const std::vector<YamlSamplerDecl> &samplerDecls,
    const std::vector<YamlUavDecl> &uavDecls,
    std::string &error) {
  std::unordered_map<std::string, uint32_t> resourceHandles;
  std::unordered_map<std::string, uint32_t> tempHandles;
  std::unordered_map<std::string, uint32_t> inputHandles;
  std::unordered_map<std::string, uint32_t> outputHandles;
  std::unordered_map<std::string, uint32_t> cbufferHandles;
  std::unordered_map<std::string, uint32_t> samplerHandles;
  std::unordered_map<std::string, uint32_t> uavHandles;

  for (uint32_t i = 0; i < tempDecls.size(); ++i) {
    if (tempDecls[i].handle.empty()) {
      continue;
    }
    if (!tempHandles.emplace(tempDecls[i].handle, i).second) {
      error = "duplicate SM5 temp declaration handle: '" + tempDecls[i].handle + "'";
      return false;
    }
  }

  for (uint32_t i = 0; i < inputDecls.size(); ++i) {
    if (inputDecls[i].handle.empty()) {
      continue;
    }
    if (!inputHandles.emplace(inputDecls[i].handle, i).second) {
      error = "duplicate SM5 input declaration handle: '" + inputDecls[i].handle + "'";
      return false;
    }
  }

  for (uint32_t i = 0; i < outputDecls.size(); ++i) {
    if (outputDecls[i].handle.empty()) {
      continue;
    }
    if (!outputHandles.emplace(outputDecls[i].handle, i).second) {
      error = "duplicate SM5 output declaration handle: '" + outputDecls[i].handle + "'";
      return false;
    }
  }

  for (uint32_t i = 0; i < textureDecls.size(); ++i) {
    if (textureDecls[i].handle.empty()) {
      continue;
    }
    if (!resourceHandles.emplace(textureDecls[i].handle, i).second) {
      error = "duplicate SM5 resource declaration handle: '" + textureDecls[i].handle + "'";
      return false;
    }
  }

  for (uint32_t i = 0; i < rawResourceDecls.size(); ++i) {
    if (rawResourceDecls[i].handle.empty()) {
      continue;
    }
    if (!resourceHandles.emplace(rawResourceDecls[i].handle,
                                 textureDecls.size() + i).second) {
      error = "duplicate SM5 resource declaration handle: '" + rawResourceDecls[i].handle + "'";
      return false;
    }
  }

  for (uint32_t i = 0; i < structuredResourceDecls.size(); ++i) {
    if (structuredResourceDecls[i].handle.empty()) {
      continue;
    }
    if (!resourceHandles.emplace(structuredResourceDecls[i].handle,
                                 textureDecls.size() + rawResourceDecls.size() + i).second) {
      error = "duplicate SM5 resource declaration handle: '" + structuredResourceDecls[i].handle + "'";
      return false;
    }
  }

  for (uint32_t i = 0; i < cbufferDecls.size(); ++i) {
    if (cbufferDecls[i].handle.empty()) {
      continue;
    }
    if (!cbufferHandles.emplace(cbufferDecls[i].handle, i).second) {
      error = "duplicate SM5 cbuffer declaration handle: '" + cbufferDecls[i].handle + "'";
      return false;
    }
  }

  for (uint32_t i = 0; i < samplerDecls.size(); ++i) {
    if (samplerDecls[i].handle.empty()) {
      continue;
    }
    if (!samplerHandles.emplace(samplerDecls[i].handle, i).second) {
      error = "duplicate SM5 sampler declaration handle: '" + samplerDecls[i].handle + "'";
      return false;
    }
  }

  for (uint32_t i = 0; i < uavDecls.size(); ++i) {
    if (uavDecls[i].handle.empty()) {
      continue;
    }
    if (!uavHandles.emplace(uavDecls[i].handle, i).second) {
      error = "duplicate SM5 uav declaration handle: '" + uavDecls[i].handle + "'";
      return false;
    }
  }

  return true;
}

static bool ValidateEmitOperandHandleReference(
    const YamlOperand &operand,
  const std::unordered_map<std::string, uint32_t> &tempHandles,
  const std::unordered_map<std::string, uint32_t> &inputHandles,
  const std::unordered_map<std::string, uint32_t> &outputHandles,
  const std::unordered_map<std::string, uint32_t> &resourceHandles,
    const std::unordered_map<std::string, uint32_t> &cbufferHandles,
    const std::unordered_map<std::string, uint32_t> &samplerHandles,
  const std::unordered_map<std::string, uint32_t> &uavHandles,
    std::string &error) {
  if (operand.bind_handle.empty()) {
    return true;
  }

  if (operand.type.empty()) {
    error = "SM5 bind_handle emit operands require explicit operand type";
    return false;
  }

  OperandType type = D3D10_SB_OPERAND_TYPE_TEMP;
  if (!ParseOperandType(operand.type, type, error)) {
    return false;
  }

  switch (type) {
    case D3D10_SB_OPERAND_TYPE_TEMP:
      if (tempHandles.find(operand.bind_handle) == tempHandles.end()) {
        error = "SM5 bind_handle references unknown temp declaration handle '" +
                operand.bind_handle + "'";
        return false;
      }
      return true;
    case D3D10_SB_OPERAND_TYPE_INPUT:
      if (inputHandles.find(operand.bind_handle) == inputHandles.end()) {
        error = "SM5 bind_handle references unknown input declaration handle '" +
                operand.bind_handle + "'";
        return false;
      }
      return true;
    case D3D10_SB_OPERAND_TYPE_OUTPUT:
      if (outputHandles.find(operand.bind_handle) == outputHandles.end()) {
        error = "SM5 bind_handle references unknown output declaration handle '" +
                operand.bind_handle + "'";
        return false;
      }
      return true;
    case D3D10_SB_OPERAND_TYPE_RESOURCE:
      if (resourceHandles.find(operand.bind_handle) == resourceHandles.end()) {
        error = "SM5 bind_handle references unknown resource declaration handle '" +
                operand.bind_handle + "'";
        return false;
      }
      return true;
    case D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW:
      if (uavHandles.find(operand.bind_handle) == uavHandles.end()) {
        error = "SM5 bind_handle references unknown uav declaration handle '" +
                operand.bind_handle + "'";
        return false;
      }
      return true;
    case D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER:
      if (cbufferHandles.find(operand.bind_handle) == cbufferHandles.end()) {
        error = "SM5 bind_handle references unknown cbuffer declaration handle '" +
                operand.bind_handle + "'";
        return false;
      }
      return true;
    case D3D10_SB_OPERAND_TYPE_SAMPLER:
      if (samplerHandles.find(operand.bind_handle) == samplerHandles.end()) {
        error = "SM5 bind_handle references unknown sampler declaration handle '" +
                operand.bind_handle + "'";
        return false;
      }
      return true;
    default:
      error = "SM5 bind_handle operand type is unsupported for resource binding";
      return false;
  }
}

static bool ValidateEmitHandleReferences(const YamlRecipeDocument &document,
                                         std::string &error) {
  std::unordered_map<std::string, uint32_t> tempHandles;
  std::unordered_map<std::string, uint32_t> inputHandles;
  std::unordered_map<std::string, uint32_t> outputHandles;
  std::unordered_map<std::string, uint32_t> resourceHandles;
  std::unordered_map<std::string, uint32_t> cbufferHandles;
  std::unordered_map<std::string, uint32_t> samplerHandles;
  std::unordered_map<std::string, uint32_t> uavHandles;

  for (uint32_t i = 0; i < document.temp_decls.size(); ++i) {
    if (!document.temp_decls[i].handle.empty()) {
      tempHandles.emplace(document.temp_decls[i].handle, i);
    }
  }

  for (uint32_t i = 0; i < document.input_decls.size(); ++i) {
    if (!document.input_decls[i].handle.empty()) {
      inputHandles.emplace(document.input_decls[i].handle, i);
    }
  }
  for (uint32_t i = 0; i < document.output_decls.size(); ++i) {
    if (!document.output_decls[i].handle.empty()) {
      outputHandles.emplace(document.output_decls[i].handle, i);
    }
  }

  for (uint32_t i = 0; i < document.texture_decls.size(); ++i) {
    if (!document.texture_decls[i].handle.empty()) {
      resourceHandles.emplace(document.texture_decls[i].handle, i);
    }
  }
  for (uint32_t i = 0; i < document.raw_resource_decls.size(); ++i) {
    if (!document.raw_resource_decls[i].handle.empty()) {
      resourceHandles.emplace(document.raw_resource_decls[i].handle,
                              document.texture_decls.size() + i);
    }
  }
  for (uint32_t i = 0; i < document.structured_resource_decls.size(); ++i) {
    if (!document.structured_resource_decls[i].handle.empty()) {
      resourceHandles.emplace(document.structured_resource_decls[i].handle,
                              document.texture_decls.size() +
                                  document.raw_resource_decls.size() + i);
    }
  }
  for (uint32_t i = 0; i < document.cbuffer_decls.size(); ++i) {
    if (!document.cbuffer_decls[i].handle.empty()) {
      cbufferHandles.emplace(document.cbuffer_decls[i].handle, i);
    }
  }
  for (uint32_t i = 0; i < document.sampler_decls.size(); ++i) {
    if (!document.sampler_decls[i].handle.empty()) {
      samplerHandles.emplace(document.sampler_decls[i].handle, i);
    }
  }
  for (uint32_t i = 0; i < document.uav_decls.size(); ++i) {
    if (!document.uav_decls[i].handle.empty()) {
      uavHandles.emplace(document.uav_decls[i].handle, i);
    }
  }

  for (const YamlStep &step : document.steps) {
    for (const YamlRule &rule : step.rules) {
      for (const YamlEmitInstruction &emit : rule.emit) {
        for (const YamlOperand &operand : emit.operands) {
          if (!ValidateEmitOperandHandleReference(operand,
                                                  tempHandles,
                                                  inputHandles,
                                                  outputHandles,
                                                  resourceHandles,
                                                  cbufferHandles,
                                                  samplerHandles,
                                                  uavHandles,
                                                  error)) {
            return false;
          }
        }
      }
    }
  }

  return true;
}

static bool ValidatePortableDeclarationModel(const YamlRecipeDocument &document,
                                             std::string &error) {
  for (const YamlTempDecl &decl : document.temp_decls) {
    if (decl.handle.empty()) {
      error = "portable schema temp_decls require handle";
      return false;
    }
  }

  for (const YamlInputDecl &decl : document.input_decls) {
    if (decl.handle.empty()) {
      error = "portable schema input_decls require handle";
      return false;
    }
    if (!decl.auto_bind) {
      error = "portable schema input_decls require auto_bind: true";
      return false;
    }
    if (decl.bind_point >= 0) {
      error = "portable schema input_decls do not allow bind_point";
      return false;
    }
    uint32_t parsedInterpolationMode = D3D10_SB_INTERPOLATION_LINEAR;
    if (!ParseInterpolationModeToken(decl.interpolation_mode,
                                     parsedInterpolationMode, error)) {
      return false;
    }
  }

  for (const YamlOutputDecl &decl : document.output_decls) {
    if (decl.handle.empty()) {
      error = "portable schema output_decls require handle";
      return false;
    }
    if (!decl.auto_bind) {
      error = "portable schema output_decls require auto_bind: true";
      return false;
    }
    if (decl.bind_point >= 0) {
      error = "portable schema output_decls do not allow bind_point";
      return false;
    }
  }

  auto validateTexture = [&](const YamlTextureDecl &decl) -> bool {
    if (decl.handle.empty()) {
      error = "portable schema texture_decls require handle";
      return false;
    }
    if (!decl.auto_bind) {
      error = "portable schema texture_decls require auto_bind: true";
      return false;
    }
    if (decl.bind_point >= 0) {
      error = "portable schema texture_decls do not allow bind_point";
      return false;
    }
    return true;
  };

  auto validateCBuffer = [&](const YamlCBufferDecl &decl) -> bool {
    if (decl.handle.empty()) {
      error = "portable schema cbuffer_decls require handle";
      return false;
    }
    if (!decl.auto_bind) {
      error = "portable schema cbuffer_decls require auto_bind: true";
      return false;
    }
    if (decl.bind_point >= 0) {
      error = "portable schema cbuffer_decls do not allow bind_point";
      return false;
    }
    return true;
  };

  auto validateSampler = [&](const YamlSamplerDecl &decl) -> bool {
    if (decl.handle.empty()) {
      error = "portable schema sampler_decls require handle";
      return false;
    }
    if (!decl.auto_bind) {
      error = "portable schema sampler_decls require auto_bind: true";
      return false;
    }
    if (decl.bind_point >= 0) {
      error = "portable schema sampler_decls do not allow bind_point";
      return false;
    }
    return true;
  };

  for (const YamlTextureDecl &decl : document.texture_decls) {
    if (!validateTexture(decl)) {
      return false;
    }
  }

  for (const YamlRawResourceDecl &decl : document.raw_resource_decls) {
    if (decl.handle.empty()) {
      error = "portable schema raw_resource_decls require handle";
      return false;
    }
    if (!decl.auto_bind) {
      error = "portable schema raw_resource_decls require auto_bind: true";
      return false;
    }
    if (decl.bind_point >= 0) {
      error = "portable schema raw_resource_decls do not allow bind_point";
      return false;
    }
  }

  for (const YamlStructuredResourceDecl &decl : document.structured_resource_decls) {
    if (decl.handle.empty()) {
      error = "portable schema structured_resource_decls require handle";
      return false;
    }
    if (!decl.auto_bind) {
      error = "portable schema structured_resource_decls require auto_bind: true";
      return false;
    }
    if (decl.bind_point >= 0) {
      error = "portable schema structured_resource_decls do not allow bind_point";
      return false;
    }
  }
  for (const YamlCBufferDecl &decl : document.cbuffer_decls) {
    if (!validateCBuffer(decl)) {
      return false;
    }
  }
  for (const YamlSamplerDecl &decl : document.sampler_decls) {
    if (!validateSampler(decl)) {
      return false;
    }
  }

  for (const YamlUavDecl &decl : document.uav_decls) {
    if (decl.handle.empty()) {
      error = "portable schema uav_decls require handle";
      return false;
    }
    if (!decl.auto_bind) {
      error = "portable schema uav_decls require auto_bind: true";
      return false;
    }
    if (decl.bind_point >= 0) {
      error = "portable schema uav_decls do not allow bind_point";
      return false;
    }
  }

  return true;
}

} // namespace

bool ParseRecipeText(llvm::StringRef recipeText,
                     RecipeParseResult &result,
                     llvm::StringRef sourceName) {
  result = RecipeParseResult{};

  YamlRecipeDocument document;
  llvm::yaml::Input input(recipeText);
  input >> document;
  if (input.error()) {
    result.Error = sourceName.str() + ": " + input.error().message();
    return false;
  }

  if (document.version != 1) {
    result.Error = sourceName.str() + ": unsupported SM5 recipe schema version";
    return false;
  }

  std::string parseError;

  const bool strictPortable = true;

  if (!document.rewrite_rules.empty()) {
    result.Error = sourceName.str() +
                   ": schema version 1 requires steps and does not allow top-level rewrite_rules";
    return false;
  }
  if (document.steps.empty()) {
    result.Error = sourceName.str() + ": schema version 1 requires at least one step";
    return false;
  }
  if (!document.prefilters.empty()) {
    result.Error = sourceName.str() +
                   ": schema version 1 uses predicates instead of prefilters";
    return false;
  }
  if (!ValidatePortableDeclarationModel(document, parseError)) {
    result.Error = sourceName.str() + ": " + parseError;
    return false;
  }

  if (document.reserved_temps > 0) {
    result.Recipe.ReserveTemps(document.reserved_temps);
  }

  for (const YamlTempDecl &declModel : document.temp_decls) {
    RecipeTempDecl decl;
    decl.Handle = declModel.handle;
    result.Recipe.AddTempDecl(std::move(decl));
  }

  if (!ValidateUniqueDeclarationHandles(document.temp_decls,
                                        document.input_decls,
                                        document.output_decls,
                                        document.texture_decls,
                                        document.raw_resource_decls,
                                        document.structured_resource_decls,
                                        document.cbuffer_decls,
                                        document.sampler_decls,
                                        document.uav_decls,
                                        parseError)) {
    result.Error = sourceName.str() + ": " + parseError;
    return false;
  }

  if (!ValidateEmitHandleReferences(document, parseError)) {
    result.Error = sourceName.str() + ": " + parseError;
    return false;
  }

  for (const YamlPrefilter &prefilterModel : document.predicates) {
    RecipePrefilter prefilter;
    if (!ParsePrefilterKindToken(prefilterModel.kind, prefilter.Kind,
                                 parseError)) {
      result.Error = sourceName.str() + ": " + parseError;
      return false;
    }
    if (!ValidatePrefilterModel(prefilterModel, prefilter.Kind, parseError)) {
      result.Error = sourceName.str() + ": " + parseError;
      return false;
    }
    prefilter.Name = prefilterModel.name;
    prefilter.Required = prefilterModel.required;
    prefilter.ExpectedMajorVersion = prefilterModel.major;
    prefilter.ExpectedMinorVersion = prefilterModel.minor;
    prefilter.ExpectedCount = prefilterModel.expected_count;
    prefilter.ExpectedResourceCount = prefilterModel.expected_resources;
    prefilter.Match.Opcode = prefilterModel.match.opcode;
    prefilter.Match.Capture = prefilterModel.match.capture;
    prefilter.Match.Saturate = prefilterModel.match.saturate;
    prefilter.Match.TestBoolean = prefilterModel.match.test_boolean;
    for (const YamlOperand &operandModel : prefilterModel.match.operands) {
      RecipeOperandPattern operand;
      if (!FillRecipeOperandPattern(operandModel, strictPortable, false,
                                    operand, parseError)) {
        result.Error = sourceName.str() + ": " + parseError;
        return false;
      }
      prefilter.Match.Operands.push_back(std::move(operand));
    }
    for (const YamlInstructionMatch &matchModel : prefilterModel.match.sequence) {
      RecipeInstructionPattern pattern;
      pattern.Opcode = matchModel.opcode;
      pattern.Capture = matchModel.capture;
      pattern.Saturate = matchModel.saturate;
      pattern.TestBoolean = matchModel.test_boolean;
      for (const YamlOperand &operandModel : matchModel.operands) {
        RecipeOperandPattern operand;
        if (!FillRecipeOperandPattern(operandModel, strictPortable, false,
                                      operand, parseError)) {
          result.Error = sourceName.str() + ": " + parseError;
          return false;
        }
        pattern.Operands.push_back(std::move(operand));
      }
      prefilter.Match.Sequence.push_back(std::move(pattern));
    }
    if (!prefilterModel.opcode.empty()) {
      Opcode parsedOpcode;
      if (!ParseOpcode(prefilterModel.opcode, parsedOpcode)) {
        result.Error = sourceName.str() +
                       ": Unknown SM5 opcode in prefilter: " +
                       prefilterModel.opcode;
        return false;
      }
      prefilter.Opcode = prefilterModel.opcode;
    }
    result.Recipe.AddPrefilter(std::move(prefilter));
  }

  for (const YamlInputDecl &declModel : document.input_decls) {
    RecipeInputDecl decl;
    decl.BindPoint = declModel.bind_point >= 0
                         ? static_cast<uint32_t>(declModel.bind_point)
                         : 0u;
    decl.Handle = declModel.handle;
    decl.AutoBind = declModel.auto_bind;
    if (!ParseInterpolationModeToken(declModel.interpolation_mode,
                                     decl.InterpolationMode, parseError)) {
      result.Error = sourceName.str() + ": " + parseError;
      return false;
    }
    result.Recipe.AddInputDecl(std::move(decl));
  }

  for (const YamlOutputDecl &declModel : document.output_decls) {
    RecipeOutputDecl decl;
    decl.BindPoint = declModel.bind_point >= 0
                         ? static_cast<uint32_t>(declModel.bind_point)
                         : 0u;
    decl.Handle = declModel.handle;
    decl.AutoBind = declModel.auto_bind;
    result.Recipe.AddOutputDecl(std::move(decl));
  }

  for (const YamlTextureDecl &declModel : document.texture_decls) {
    RecipeTextureDecl decl;
    decl.BindPoint = declModel.bind_point >= 0
                         ? static_cast<uint32_t>(declModel.bind_point)
                         : 0u;
    decl.Handle = declModel.handle;
    decl.AutoBind = declModel.auto_bind;
    if (!ParseTextureDimensionToken(declModel.dimension, decl.Dimension,
                                    parseError)) {
      result.Error = sourceName.str() + ": " + parseError;
      return false;
    }
    result.Recipe.AddTextureDecl(std::move(decl));
  }

  for (const YamlCBufferDecl &declModel : document.cbuffer_decls) {
    RecipeCBufferDecl decl;
    decl.BindPoint = declModel.bind_point >= 0
                         ? static_cast<uint32_t>(declModel.bind_point)
                         : 0u;
    decl.Elements = declModel.elements;
    decl.Handle = declModel.handle;
    decl.AutoBind = declModel.auto_bind;
    if (!ParseCBufferAccessPatternToken(declModel.access_pattern,
                                        decl.AccessPattern, parseError)) {
      result.Error = sourceName.str() + ": " + parseError;
      return false;
    }
    result.Recipe.AddCBufferDecl(std::move(decl));
  }

  for (const YamlRawResourceDecl &declModel : document.raw_resource_decls) {
    RecipeRawResourceDecl decl;
    decl.BindPoint = declModel.bind_point >= 0
                         ? static_cast<uint32_t>(declModel.bind_point)
                         : 0u;
    decl.Handle = declModel.handle;
    decl.AutoBind = declModel.auto_bind;
    result.Recipe.AddRawResourceDecl(std::move(decl));
  }

  for (const YamlStructuredResourceDecl &declModel : document.structured_resource_decls) {
    RecipeStructuredResourceDecl decl;
    decl.BindPoint = declModel.bind_point >= 0
                         ? static_cast<uint32_t>(declModel.bind_point)
                         : 0u;
    decl.StructureStride = declModel.stride;
    decl.Handle = declModel.handle;
    decl.AutoBind = declModel.auto_bind;
    result.Recipe.AddStructuredResourceDecl(std::move(decl));
  }

  for (const YamlSamplerDecl &declModel : document.sampler_decls) {
    RecipeSamplerDecl decl;
    decl.BindPoint = declModel.bind_point >= 0
                         ? static_cast<uint32_t>(declModel.bind_point)
                         : 0u;
    decl.Handle = declModel.handle;
    decl.AutoBind = declModel.auto_bind;
    if (!ParseSamplerModeToken(declModel.mode, decl.Mode, parseError)) {
      result.Error = sourceName.str() + ": " + parseError;
      return false;
    }
    result.Recipe.AddSamplerDecl(std::move(decl));
  }

  for (const YamlUavDecl &declModel : document.uav_decls) {
    RecipeUavDecl decl;
    decl.BindPoint = declModel.bind_point >= 0
                         ? static_cast<uint32_t>(declModel.bind_point)
                         : 0u;
    decl.Handle = declModel.handle;
    decl.AutoBind = declModel.auto_bind;
    decl.StructureStride = declModel.stride;
    decl.GloballyCoherent = declModel.globally_coherent;
    decl.HasOrderPreservingCounter = declModel.has_counter;
    if (!ParseUavKindToken(declModel.kind, decl.Kind, parseError)) {
      result.Error = sourceName.str() + ": " + parseError;
      return false;
    }
    if (!ParseTextureDimensionToken(declModel.dimension, decl.Dimension,
                                    parseError)) {
      result.Error = sourceName.str() + ": " + parseError;
      return false;
    }
    result.Recipe.AddUavDecl(std::move(decl));
  }

  auto appendRuleToStep = [&](const YamlRule &ruleModel,
                              RecipeStep &step) -> bool {
    RecipeRule rule;
    if (!ParseRule(ruleModel, strictPortable, step.ApplicationMode, rule, parseError)) {
      result.Error = sourceName.str() + ": " + parseError;
      return false;
    }
    step.Rules.push_back(std::move(rule));
    return true;
  };

  for (size_t stepIndex = 0; stepIndex < document.steps.size(); ++stepIndex) {
    const YamlStep &stepModel = document.steps[stepIndex];
    RecipeStep step;
    step.Name = stepModel.name.empty()
                    ? "step_" + std::to_string(result.Recipe.GetSteps().size() + 1)
                    : stepModel.name;
    step.Required = stepModel.required;
    if (!stepModel.application_mode.empty()) {
      step.ApplicationMode = ParseRuleApplicationMode(stepModel.application_mode);
    }

    for (const YamlRule &ruleModel : stepModel.rules) {
      if (!appendRuleToStep(ruleModel, step)) {
        return false;
      }
    }

    result.Recipe.AddStep(std::move(step));
  }

  return true;
}

bool ParseRecipeFile(const std::string &recipePath,
                     RecipeParseResult &result) {
  std::ifstream file(recipePath);
  if (!file) {
    result.Error = "failed to open recipe file: " + recipePath;
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return ParseRecipeText(buffer.str(), result, recipePath);
}

} // namespace sm5
} // namespace dxp