#include "dxp/sm5/RecipeParse.h"

#include "YamlTraits.h"

#include "d3d11TokenizedProgramFormat.hpp"

#include "Model.h"
#include "Transforms.h"

#include <cctype>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/YAMLTraits.h"

namespace {



struct YamlMatch {
  std::string opcode;
  std::string capture;
  dxp::sm5::RecipeRuleRewriteMode rewrite_mode = dxp::sm5::RecipeRuleRewriteMode::Replace;
  int32_t range_start_offset = 0;
  int32_t range_end_offset = -1;
  int32_t insert_relative_index = -1;
  bool saturate = false;
  dxp::sm5::InterpolationMode interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<struct YamlOperand> operands;
  std::vector<struct YamlInstructionMatch> sequence;
};

struct YamlInstructionMatch {
  std::string opcode;
  std::string capture;
  bool saturate = false;
  dxp::sm5::InterpolationMode interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<struct YamlOperand> operands;
};

struct YamlComponentSelector {
  std::string kind;
  std::string value;
};

struct YamlOperandIndex {
  bool any = false;
  dxp::sm5::RecipeOperandIndexRepresentation representation = dxp::sm5::RecipeOperandIndexRepresentation::Immediate32;
  std::string immediate_lo;
  std::string immediate_hi;
  std::string capture;
  std::string match_capture;
};

struct YamlImmediateScalar {
  std::string value;
};

struct YamlOperandCaptureFields {
  bool type = false;
  bool components = false;
  bool modifier = false;
  bool indices = false;
  bool immediates = false;
};

struct YamlOperand {
  bool any = false;
  std::string type;
  std::vector<YamlOperandIndex> indices;
  std::vector<YamlImmediateScalar> immediates_u32;
  std::vector<YamlImmediateScalar> immediates_u64;
  std::vector<YamlImmediateScalar> immediates_i32;
  std::vector<YamlImmediateScalar> immediates_i64;
  std::vector<YamlImmediateScalar> immediates_f32;
  std::vector<YamlImmediateScalar> immediates_f64;
  std::string from_handle;
  YamlComponentSelector components;
  int32_t num_components = -1;
  std::string modifier;
  std::string capture;
  std::string match_capture;
  YamlOperandCaptureFields capture_fields;
  YamlOperandCaptureFields match_capture_fields;
};

struct YamlEmitInstruction {
  std::string opcode;
  bool saturate = false;
  dxp::sm5::InterpolationMode interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<YamlOperand> operands;
};

struct YamlRule {
  std::string name;
  YamlMatch match;
  std::string replace;
  std::vector<YamlEmitInstruction> emit;
  dxp::sm5::RecipeRuleApplicationMode mode = dxp::sm5::RecipeRuleApplicationMode::First;
  bool required_match = false;
};

struct YamlStepCondition {
  struct Comparison {
    std::string state;
    std::string input;
    std::string value;
  };

  std::string state;
  std::string input;
  std::vector<YamlStepCondition> and_conditions;
  std::vector<YamlStepCondition> or_conditions;
  Comparison eq;
  Comparison ne;
  Comparison gt;
  Comparison gte;
  Comparison lt;
  Comparison lte;
  bool not_condition = false;
};

struct YamlStep {
  std::string kind;
  std::string name;
  bool abort_on_failure = true;
  dxp::sm5::RecipeRuleApplicationMode mode = dxp::sm5::RecipeRuleApplicationMode::First;
  YamlStepCondition if_condition;
  std::vector<YamlRule> rules;
  int32_t major = INT_MIN;
  int32_t minor = INT_MIN;
  std::string opcode;
  int32_t expected_count = INT_MIN;
  int32_t expected_resources = INT_MIN;

  int32_t bind_point = -1;
  std::string handle;
  std::vector<std::string> handles;
  bool auto_bind = false;
  dxp::sm5::ResourceDimension dimension = dxp::sm5::ResourceDimension::Texture2D;
  dxp::sm5::InterpolationMode interpolation_mode;
  uint32_t elements = 1;
  dxp::sm5::CbufferAccessPattern access_pattern = dxp::sm5::CbufferAccessPattern::ImmediateIndexed;
  dxp::sm5::SamplerMode sampler_mode = dxp::sm5::SamplerMode::Default;
  dxp::sm5::RecipeUavKind uav_kind = dxp::sm5::RecipeUavKind::Typed;
  uint32_t stride = 16;
  bool globally_coherent = false;
  bool has_counter = false;
};

struct YamlTextureDecl {
  int32_t bind_point = -1;
  dxp::sm5::ResourceDimension dimension = dxp::sm5::ResourceDimension::Texture2D;
  std::string handle;
  bool auto_bind = false;
};

struct YamlInputDecl {
  int32_t bind_point = -1;
  dxp::sm5::InterpolationMode interpolation_mode;
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
  dxp::sm5::CbufferAccessPattern access_pattern = dxp::sm5::CbufferAccessPattern::ImmediateIndexed;
  std::string handle;
  bool auto_bind = false;
};

struct YamlSamplerDecl {
  int32_t bind_point = -1;
  dxp::sm5::SamplerMode mode = dxp::sm5::SamplerMode::Default;
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
  dxp::sm5::RecipeUavKind kind = dxp::sm5::RecipeUavKind::Typed;
  dxp::sm5::ResourceDimension dimension = dxp::sm5::ResourceDimension::Texture2D;
  uint32_t stride = 16;
  bool globally_coherent = false;
  bool has_counter = false;
  std::string handle;
  bool auto_bind = false;
};

struct YamlRecipeDocument {
  uint32_t version = 1;
  std::vector<YamlRule> rewrite_rules;
  std::vector<YamlStep> steps;

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

LLVM_YAML_IS_SEQUENCE_VECTOR(uint64_t)
LLVM_YAML_IS_SEQUENCE_VECTOR(int32_t)
LLVM_YAML_IS_SEQUENCE_VECTOR(int64_t)
LLVM_YAML_IS_SEQUENCE_VECTOR(float)
LLVM_YAML_IS_SEQUENCE_VECTOR(double)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlImmediateScalar)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlOperandIndex)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlOperand)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlInstructionMatch)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlEmitInstruction)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRule)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlStep)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlStepCondition)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlStepCondition::Comparison)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlTextureDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlInputDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlOutputDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRawResourceDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlStructuredResourceDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlCBufferDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlSamplerDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlUavDecl)
LLVM_YAML_IS_SEQUENCE_VECTOR(std::string)

namespace llvm {
namespace yaml {

template <> struct ScalarTraits<YamlImmediateScalar> {
  static void output(const YamlImmediateScalar &value, void *ctxt,
                     raw_ostream &out) {
    (void)ctxt;
    out << value.value;
  }

  static StringRef input(StringRef scalar, void *ctxt,
                         YamlImmediateScalar &value) {
    (void)ctxt;
    value.value = scalar.str();
    return StringRef();
  }

  static bool mustQuote(StringRef) { return false; }
};

template <> struct MappingTraits<YamlComponentSelector> {
  static void mapping(IO &io, YamlComponentSelector &selector) {
    io.mapOptional("kind", selector.kind);
    io.mapOptional("value", selector.value);
  }
};

template <> struct MappingTraits<YamlOperandCaptureFields> {
  static void mapping(IO &io, YamlOperandCaptureFields &fields) {
    io.mapOptional("type", fields.type, false);
    io.mapOptional("components", fields.components, false);
    io.mapOptional("modifier", fields.modifier, false);
    io.mapOptional("indices", fields.indices, false);
    io.mapOptional("immediates", fields.immediates, false);
  }
};

template <> struct MappingTraits<YamlOperand> {
  static void mapping(IO &io, YamlOperand &operand) {
    io.mapOptional("any", operand.any, false);
    io.mapOptional("type", operand.type);
    io.mapOptional("indices", operand.indices);
    io.mapOptional("immediates_u32", operand.immediates_u32);
    io.mapOptional("immediates_u64", operand.immediates_u64);
    io.mapOptional("immediates_i32", operand.immediates_i32);
    io.mapOptional("immediates_i64", operand.immediates_i64);
    io.mapOptional("immediates_f32", operand.immediates_f32);
    io.mapOptional("immediates_f64", operand.immediates_f64);
    io.mapOptional("from_handle", operand.from_handle);
    io.mapOptional("components", operand.components);
    io.mapOptional("num_components", operand.num_components, -1);
    io.mapOptional("modifier", operand.modifier);
    io.mapOptional("capture", operand.capture);
    io.mapOptional("match_capture", operand.match_capture);
    io.mapOptional("capture_fields", operand.capture_fields);
    io.mapOptional("match_capture_fields", operand.match_capture_fields);
  }
};

template <> struct MappingTraits<YamlOperandIndex> {
  static void mapping(IO &io, YamlOperandIndex &index) {
    io.mapOptional("any", index.any, false);
    io.mapOptional("representation", index.representation);
    io.mapOptional("immediate_lo", index.immediate_lo);
    io.mapOptional("immediate_hi", index.immediate_hi);
    io.mapOptional("capture", index.capture);
    io.mapOptional("match_capture", index.match_capture);
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
    io.mapOptional("rewrite_mode", match.rewrite_mode);
    io.mapOptional("range_start_offset", match.range_start_offset, 0);
    io.mapOptional("range_end_offset", match.range_end_offset, -1);
    io.mapOptional("insert_relative_index", match.insert_relative_index, -1);
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
    io.mapOptional("name", rule.name);
    io.mapOptional("match", rule.match);
    io.mapOptional("replace", rule.replace);
    io.mapOptional("emit", rule.emit);
    io.mapOptional("mode", rule.mode);
    io.mapOptional("required_match", rule.required_match, false);
  }
};

template <> struct MappingTraits<YamlStepCondition> {
  static void mapComparison(IO &io, const char *key,
                            YamlStepCondition::Comparison &comparison) {
    io.mapOptional(key, comparison);
  }

  static void mapping(IO &io, YamlStepCondition &condition) {
    io.mapOptional("state", condition.state);
    io.mapOptional("input", condition.input);
    io.mapOptional("and", condition.and_conditions);
    io.mapOptional("or", condition.or_conditions);
    mapComparison(io, "eq", condition.eq);
    mapComparison(io, "ne", condition.ne);
    mapComparison(io, "gt", condition.gt);
    mapComparison(io, "gte", condition.gte);
    mapComparison(io, "lt", condition.lt);
    mapComparison(io, "lte", condition.lte);
    io.mapOptional("not", condition.not_condition, false);
  }
};

template <> struct MappingTraits<YamlStepCondition::Comparison> {
  static void mapping(IO &io, YamlStepCondition::Comparison &comparison) {
    io.mapOptional("state", comparison.state);
    io.mapOptional("input", comparison.input);
    io.mapRequired("value", comparison.value);
  }
};

template <> struct MappingTraits<YamlStep> {
  static void mapping(IO &io, YamlStep &step) {
    io.mapOptional("kind", step.kind);
    io.mapOptional("name", step.name);
    io.mapOptional("abort_on_failure", step.abort_on_failure, true);
    io.mapOptional("mode", step.mode);
    io.mapOptional("if", step.if_condition);
    io.mapOptional("rules", step.rules);
    io.mapOptional("major", step.major, INT_MIN);
    io.mapOptional("minor", step.minor, INT_MIN);
    io.mapOptional("opcode", step.opcode);
    io.mapOptional("expected_count", step.expected_count, INT_MIN);
    io.mapOptional("expected_resources", step.expected_resources, INT_MIN);
    io.mapOptional("bind_point", step.bind_point, -1);
    io.mapOptional("handle", step.handle);
    io.mapOptional("handles", step.handles);
    io.mapOptional("auto_bind", step.auto_bind, false);
    io.mapOptional("dimension", step.dimension, dxp::sm5::ResourceDimension::Texture2D);
    io.mapOptional("interpolation_mode", step.interpolation_mode, dxp::sm5::InterpolationMode::Undefined);
    io.mapOptional("elements", step.elements, 1u);
    io.mapOptional("access_pattern", step.access_pattern, dxp::sm5::CbufferAccessPattern::ImmediateIndexed);
    io.mapOptional("sampler_mode", step.sampler_mode, dxp::sm5::SamplerMode::Default);
    io.mapOptional("uav_kind", step.uav_kind, dxp::sm5::RecipeUavKind::Typed);
    io.mapOptional("stride", step.stride, 16u);
    io.mapOptional("globally_coherent", step.globally_coherent, false);
    io.mapOptional("has_counter", step.has_counter, false);
  }
};

template <> struct MappingTraits<YamlTextureDecl> {
  static void mapping(IO &io, YamlTextureDecl &decl) {
    io.mapOptional("bind_point", decl.bind_point, -1);
    io.mapOptional("dimension", decl.dimension, dxp::sm5::ResourceDimension::Texture2D);
    io.mapOptional("handle", decl.handle);
    io.mapOptional("auto_bind", decl.auto_bind, false);
  }
};

template <> struct MappingTraits<YamlInputDecl> {
  static void mapping(IO &io, YamlInputDecl &decl) {
    io.mapOptional("bind_point", decl.bind_point, -1);
    io.mapOptional("interpolation_mode", decl.interpolation_mode);
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
    io.mapOptional("access_pattern", decl.access_pattern);
    io.mapOptional("handle", decl.handle);
    io.mapOptional("auto_bind", decl.auto_bind, false);
  }
};

template <> struct MappingTraits<YamlSamplerDecl> {
  static void mapping(IO &io, YamlSamplerDecl &decl) {
    io.mapOptional("bind_point", decl.bind_point, -1);
    io.mapOptional("mode", decl.mode, dxp::sm5::SamplerMode::Default);
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
    io.mapOptional("kind", decl.kind, dxp::sm5::RecipeUavKind::Typed);
    io.mapOptional("dimension", decl.dimension, dxp::sm5::ResourceDimension::Texture2D);
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
    io.mapOptional("rewrite_rules", document.rewrite_rules);
    io.mapOptional("steps", document.steps);
    io.mapOptional("texture_decls", document.texture_decls);
    io.mapOptional("input_decls", document.input_decls);
    io.mapOptional("output_decls", document.output_decls);
    io.mapOptional("raw_resource_decls", document.raw_resource_decls);
    io.mapOptional("structured_resource_decls",
                   document.structured_resource_decls);
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




static bool ParseUavKindToken(const std::string &value, RecipeUavKind &kind,
                              std::string &error) {
  if (value == "typed") {
    kind = RecipeUavKind::Typed;
    return true;
  }
  if (value == "raw") {
    kind = RecipeUavKind::Raw;
    return true;
  }
  if (value == "structured") {
    kind = RecipeUavKind::Structured;
    return true;
  }

  error = "unsupported SM5 uav kind: " + value;
  return false;
}


static std::string interpolationModeToString(
    dxp::sm5::InterpolationMode mode) {
  switch (mode) {
  case dxp::sm5::InterpolationMode::Undefined:
    return {};  // not set — let Recipe.cpp skip validation
  case dxp::sm5::InterpolationMode::Constant:
    return "constant";
  case dxp::sm5::InterpolationMode::Linear:
    return "linear";
  case dxp::sm5::InterpolationMode::LinearCentroid:
    return "linear_centroid";
  case dxp::sm5::InterpolationMode::LinearNoperspective:
    return "linear_noperspective";
  case dxp::sm5::InterpolationMode::LinearNoperspectiveCentroid:
    return "linear_noperspective_centroid";
  case dxp::sm5::InterpolationMode::LinearSample:
    return "linear_sample";
  case dxp::sm5::InterpolationMode::LinearNoperspectiveSample:
    return "linear_noperspective_sample";
  }
  return {};
}


static bool ParseOperandType(const std::string &value, OperandType &type,
                             std::string &error) {
  if (value == "temp") {
    type = D3D10_SB_OPERAND_TYPE_TEMP;
    return true;
  }
  if (value == "input") {
    type = D3D10_SB_OPERAND_TYPE_INPUT;
    return true;
  }
  if (value == "output") {
    type = D3D10_SB_OPERAND_TYPE_OUTPUT;
    return true;
  }
  if (value == "indexable_temp") {
    type = D3D10_SB_OPERAND_TYPE_INDEXABLE_TEMP;
    return true;
  }
  if (value == "immediate32") {
    type = D3D10_SB_OPERAND_TYPE_IMMEDIATE32;
    return true;
  }
  if (value == "immediate64") {
    type = D3D10_SB_OPERAND_TYPE_IMMEDIATE64;
    return true;
  }
  if (value == "sampler") {
    type = D3D10_SB_OPERAND_TYPE_SAMPLER;
    return true;
  }
  if (value == "resource") {
    type = D3D10_SB_OPERAND_TYPE_RESOURCE;
    return true;
  }
  if (value == "unordered_access_view" || value == "uav") {
    type = D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW;
    return true;
  }
  if (value == "constant_buffer" || value == "cbuffer") {
    type = D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER;
    return true;
  }
  if (value == "output_depth") {
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
  if (value.empty() || value == "none") {
    modifier = D3D10_SB_OPERAND_MODIFIER_NONE;
    return true;
  }
  if (value == "neg" || value == "minus") {
    modifier = D3D10_SB_OPERAND_MODIFIER_NEG;
    return true;
  }
  if (value == "abs") {
    modifier = D3D10_SB_OPERAND_MODIFIER_ABS;
    return true;
  }
  if (value == "abs_neg") {
    modifier = D3D10_SB_OPERAND_MODIFIER_ABSNEG;
    return true;
  }

  error = "unsupported SM5 operand modifier: " + value;
  return false;
}


static bool ParseIndexImmediateScalar(const std::string &token,
                                      const char *fieldName,
                                      bool &hasImmediate,
                                      uint32_t &immediate,
                                      std::string &error) {
  hasImmediate = false;
  immediate = 0;
  if (token.empty()) {
    return true;
  }

  char *end = nullptr;
  const unsigned long long parsed = std::strtoull(token.c_str(), &end, 0);
  if (end != nullptr && *end == '\0') {
    if (parsed > 0xFFFFFFFFull) {
      error = std::string("SM5 ") + fieldName + " is out of uint32 range";
      return false;
    }
    hasImmediate = true;
    immediate = static_cast<uint32_t>(parsed);
    return true;
  }

  error = std::string("SM5 ") + fieldName +
          " only accepts integer literals";
  return false;
}

static bool IsValidVariableKey(const std::string &name) {
  if (name.empty()) {
    return false;
  }

  const unsigned char first = static_cast<unsigned char>(name.front());
  if (!(std::isalpha(first) || first == '_')) {
    return false;
  }

  for (char ch : name) {
    const unsigned char value = static_cast<unsigned char>(ch);
    if (!(std::isalnum(value) || value == '_')) {
      return false;
    }
  }

  return true;
}

template <typename TDest, typename TSource>
static TDest BitCastImmediate(TSource value) {
  static_assert(sizeof(TDest) == sizeof(TSource),
                "bit-cast source/destination sizes must match");
  TDest result{};
  std::memcpy(&result, &value, sizeof(result));
  return result;
}

static bool DecodeOperandIndexPatterns(
    const std::vector<YamlOperandIndex> &yamlIndices,
    std::vector<RecipeOperandIndexPattern> &indexPatterns, bool allowAny,
    std::string &error);

static bool DecodeEmitImmediateShorthands(
    const YamlOperand &operandModel,
    std::vector<RecipeOperandIndexPattern> &indexPatterns,
    std::string &error) {
  indexPatterns.clear();
  indexPatterns.reserve(operandModel.immediates_u32.size() +
                        operandModel.immediates_u64.size() +
                        operandModel.immediates_i32.size() +
                        operandModel.immediates_i64.size() +
                        operandModel.immediates_f32.size() +
                        operandModel.immediates_f64.size());

  auto appendImmediate32 = [&](uint32_t immediate,
                               RecipeImmediateFamily family) {
    RecipeOperandIndexPattern indexPattern;
    indexPattern.Representation = RecipeOperandIndexRepresentation::Immediate32;
    indexPattern.HasImmediateLo = true;
    indexPattern.ImmediateLo = immediate;
    indexPattern.ImmediateFamily = family;
    indexPatterns.push_back(std::move(indexPattern));
  };

  auto appendImmediate64 = [&](uint64_t immediate,
                               RecipeImmediateFamily family) {
    RecipeOperandIndexPattern indexPattern;
    indexPattern.Representation = RecipeOperandIndexRepresentation::Immediate64;
    indexPattern.HasImmediateLo = true;
    indexPattern.ImmediateLo = static_cast<uint32_t>(immediate & 0xFFFFFFFFull);
    indexPattern.HasImmediateHi = true;
    indexPattern.ImmediateHi = static_cast<uint32_t>(immediate >> 32);
    indexPattern.ImmediateFamily = family;
    indexPatterns.push_back(std::move(indexPattern));
  };

  auto appendVariable32 = [&](const std::string &variable,
                              RecipeImmediateFamily family,
                              const char *fieldName) -> bool {
    if (!IsValidVariableKey(variable)) {
      error = std::string("SM5 ") + fieldName +
              " entries must be numeric literals or non-empty identifier "
              "variable names";
      return false;
    }
    RecipeOperandIndexPattern indexPattern;
    indexPattern.Representation = RecipeOperandIndexRepresentation::Immediate32;
    indexPattern.HasImmediateLo = true;
    indexPattern.ImmediateLoVariable = variable;
    indexPattern.ImmediateFamily = family;
    indexPatterns.push_back(std::move(indexPattern));
    return true;
  };

  auto appendVariable64 = [&](const std::string &variable,
                              RecipeImmediateFamily family,
                              const char *fieldName) -> bool {
    if (!IsValidVariableKey(variable)) {
      error = std::string("SM5 ") + fieldName +
              " entries must be numeric literals or non-empty identifier "
              "variable names";
      return false;
    }
    RecipeOperandIndexPattern indexPattern;
    indexPattern.Representation = RecipeOperandIndexRepresentation::Immediate64;
    indexPattern.HasImmediateLo = true;
    indexPattern.HasImmediateHi = true;
    indexPattern.ImmediateLoVariable = variable;
    indexPattern.ImmediateFamily = family;
    indexPatterns.push_back(std::move(indexPattern));
    return true;
  };

  auto parseUnsigned = [&](const std::string &token,
                           uint64_t &value) -> bool {
    char *end = nullptr;
    const unsigned long long parsed = std::strtoull(token.c_str(), &end, 0);
    if (end != nullptr && *end == '\0') {
      value = static_cast<uint64_t>(parsed);
      return true;
    }
    return false;
  };

  auto parseSigned = [&](const std::string &token, int64_t &value) -> bool {
    char *end = nullptr;
    const long long parsed = std::strtoll(token.c_str(), &end, 0);
    if (end != nullptr && *end == '\0') {
      value = static_cast<int64_t>(parsed);
      return true;
    }
    return false;
  };

  auto parseFloat32 = [&](const std::string &token, float &value) -> bool {
    char *end = nullptr;
    const float parsed = std::strtof(token.c_str(), &end);
    if (end != nullptr && *end == '\0') {
      value = parsed;
      return true;
    }
    return false;
  };

  auto parseFloat64 = [&](const std::string &token, double &value) -> bool {
    char *end = nullptr;
    const double parsed = std::strtod(token.c_str(), &end);
    if (end != nullptr && *end == '\0') {
      value = parsed;
      return true;
    }
    return false;
  };

  for (const YamlImmediateScalar &token : operandModel.immediates_u32) {
    uint64_t parsed = 0;
    if (parseUnsigned(token.value, parsed)) {
      if (parsed > 0xFFFFFFFFull) {
        error = "SM5 immediates_u32 literal is out of uint32 range";
        return false;
      }
      appendImmediate32(static_cast<uint32_t>(parsed), RecipeImmediateFamily::U32);
      continue;
    }
    if (!appendVariable32(token.value, RecipeImmediateFamily::U32,
                          "immediates_u32")) {
      return false;
    }
  }

  for (const YamlImmediateScalar &token : operandModel.immediates_u64) {
    uint64_t parsed = 0;
    if (parseUnsigned(token.value, parsed)) {
      appendImmediate64(parsed, RecipeImmediateFamily::U64);
      continue;
    }
    if (!appendVariable64(token.value, RecipeImmediateFamily::U64,
                          "immediates_u64")) {
      return false;
    }
  }

  for (const YamlImmediateScalar &token : operandModel.immediates_i32) {
    int64_t parsed = 0;
    if (parseSigned(token.value, parsed)) {
      if (parsed < static_cast<int64_t>(INT32_MIN) ||
          parsed > static_cast<int64_t>(INT32_MAX)) {
        error = "SM5 immediates_i32 literal is out of int32 range";
        return false;
      }
      appendImmediate32(BitCastImmediate<uint32_t>(static_cast<int32_t>(parsed)),
                        RecipeImmediateFamily::I32);
      continue;
    }
    if (!appendVariable32(token.value, RecipeImmediateFamily::I32,
                          "immediates_i32")) {
      return false;
    }
  }

  for (const YamlImmediateScalar &token : operandModel.immediates_i64) {
    int64_t parsed = 0;
    if (parseSigned(token.value, parsed)) {
      appendImmediate64(BitCastImmediate<uint64_t>(parsed),
                        RecipeImmediateFamily::I64);
      continue;
    }
    if (!appendVariable64(token.value, RecipeImmediateFamily::I64,
                          "immediates_i64")) {
      return false;
    }
  }

  for (const YamlImmediateScalar &token : operandModel.immediates_f32) {
    float parsed = 0.0f;
    if (parseFloat32(token.value, parsed)) {
      appendImmediate32(BitCastImmediate<uint32_t>(parsed),
                        RecipeImmediateFamily::F32);
      continue;
    }
    if (!appendVariable32(token.value, RecipeImmediateFamily::F32,
                          "immediates_f32")) {
      return false;
    }
  }

  for (const YamlImmediateScalar &token : operandModel.immediates_f64) {
    double parsed = 0.0;
    if (parseFloat64(token.value, parsed)) {
      appendImmediate64(BitCastImmediate<uint64_t>(parsed),
                        RecipeImmediateFamily::F64);
      continue;
    }
    if (!appendVariable64(token.value, RecipeImmediateFamily::F64,
                          "immediates_f64")) {
      return false;
    }
  }

  return true;
}

static bool DecodeOperandIndexPatternsWithShorthands(
    const YamlOperand &operandModel,
    std::vector<RecipeOperandIndexPattern> &indexPatterns, bool allowAny,
    bool allowEmitImmediateShorthands, std::string &error) {
  const bool hasExplicitIndices = !operandModel.indices.empty();
  const bool hasImmediateShorthands = !operandModel.immediates_u32.empty() ||
                                      !operandModel.immediates_u64.empty() ||
                                      !operandModel.immediates_i32.empty() ||
                                      !operandModel.immediates_i64.empty() ||
                                      !operandModel.immediates_f32.empty() ||
                                      !operandModel.immediates_f64.empty();

  if (!allowEmitImmediateShorthands && hasImmediateShorthands) {
    error = "SM5 immediates_u32/immediates_u64/immediates_i32/immediates_i64/immediates_f32/immediates_f64 are only valid on emit operands";
    return false;
  }

  if (hasExplicitIndices && hasImmediateShorthands) {
    error = "SM5 emit operands may use explicit indices or immediate shorthand arrays (immediates_u32/immediates_u64/immediates_i32/immediates_i64/immediates_f32/immediates_f64), but not both";
    return false;
  }

  if (hasExplicitIndices) {
    return DecodeOperandIndexPatterns(operandModel.indices, indexPatterns,
                                      allowAny, error);
  }

  if (hasImmediateShorthands) {
    return DecodeEmitImmediateShorthands(operandModel, indexPatterns, error);
  }

  indexPatterns.clear();
  return true;
}

static bool DecodeOperandIndexPatterns(
    const std::vector<YamlOperandIndex> &yamlIndices,
  std::vector<RecipeOperandIndexPattern> &indexPatterns, bool allowAny,
  std::string &error) {
  indexPatterns.clear();
  indexPatterns.reserve(yamlIndices.size());

  for (const YamlOperandIndex &yamlIndex : yamlIndices) {
    RecipeOperandIndexPattern indexPattern;
    indexPattern.Any = yamlIndex.any;

    if (yamlIndex.any && !allowAny) {
      error = "SM5 emit operand indices cannot use any wildcard";
      return false;
    }

    indexPattern.Representation = yamlIndex.representation;

    if (!ParseIndexImmediateScalar(yamlIndex.immediate_lo,
                                   "index immediate_lo",
                                   indexPattern.HasImmediateLo,
                                   indexPattern.ImmediateLo, error)) {
      return false;
    }
    if (!ParseIndexImmediateScalar(yamlIndex.immediate_hi,
                                   "index immediate_hi",
                                   indexPattern.HasImmediateHi,
                                   indexPattern.ImmediateHi, error)) {
      return false;
    }

    indexPattern.Capture = yamlIndex.capture;
    indexPattern.MatchCapture = yamlIndex.match_capture;

    indexPatterns.push_back(std::move(indexPattern));
  }

  return true;
}

struct CaptureNameTables {
  std::unordered_set<std::string> Instructions;
  std::unordered_set<std::string> Operands;
  std::unordered_set<std::string> OperandIndices;
};

static void CollectOperandCaptures(const dxp::sm5::RecipeOperandPattern &operand,
                                   CaptureNameTables &captures) {
  if (!operand.Capture.empty()) {
    captures.Operands.insert(operand.Capture);
  }

  for (const dxp::sm5::RecipeOperandIndexPattern &indexPattern :
       operand.IndexPatterns) {
    if (!indexPattern.Capture.empty()) {
      captures.OperandIndices.insert(indexPattern.Capture);
    }
    if (indexPattern.RelativeOperand) {
      CollectOperandCaptures(*indexPattern.RelativeOperand, captures);
    }
  }
}

static void CollectInstructionCaptures(
    const dxp::sm5::RecipeInstructionPattern &instruction,
    CaptureNameTables &captures) {
  if (!instruction.Capture.empty()) {
    captures.Instructions.insert(instruction.Capture);
  }

  for (const dxp::sm5::RecipeOperandPattern &operand : instruction.Operands) {
    CollectOperandCaptures(operand, captures);
  }
}

static void CollectMatchCaptures(const dxp::sm5::RecipeMatchPattern &match,
                                 CaptureNameTables &captures) {
  if (!match.Capture.empty()) {
    captures.Instructions.insert(match.Capture);
  }

  for (const dxp::sm5::RecipeOperandPattern &operand : match.Operands) {
    CollectOperandCaptures(operand, captures);
  }

  for (const dxp::sm5::RecipeInstructionPattern &instruction : match.Sequence) {
    CollectInstructionCaptures(instruction, captures);
  }
}

static bool DescribeCaptureKind(const CaptureNameTables &captures,
                                const std::string &capture,
                                std::string &kindDescription) {
  if (captures.Operands.find(capture) != captures.Operands.end()) {
    kindDescription = "operand";
    return true;
  }
  if (captures.OperandIndices.find(capture) != captures.OperandIndices.end()) {
    kindDescription = "index";
    return true;
  }
  if (captures.Instructions.find(capture) != captures.Instructions.end()) {
    kindDescription = "instruction";
    return true;
  }
  kindDescription.clear();
  return false;
}

static bool ValidateCaptureReference(const CaptureNameTables &captures,
                                     const std::string &capture,
                                     const char *expectedKind,
                                     const std::unordered_set<std::string> &expectedSet,
                                     const char *referenceSite,
                                     std::string &error) {
  if (capture.empty()) {
    return true;
  }

  if (expectedSet.find(capture) != expectedSet.end()) {
    return true;
  }

  std::string actualKind;
  if (DescribeCaptureKind(captures, capture, actualKind)) {
    error = std::string("SM5 ") + referenceSite + " '" + capture +
            "' expects " + expectedKind + " capture but found " +
            actualKind + " capture";
    return false;
  }

  error = std::string("SM5 ") + referenceSite + " '" + capture +
          "' references an unknown capture";
  return false;
}

static bool ValidateOperandCaptureReferences(
    const dxp::sm5::RecipeOperandPattern &operand,
    const CaptureNameTables &captures, bool emitOperand, std::string &error) {
  if (!operand.MatchCapture.empty()) {
    if (!ValidateCaptureReference(captures, operand.MatchCapture, "operand",
                                  captures.Operands, "operand match_capture",
                                  error)) {
      return false;
    }
  }

  if (emitOperand && !operand.Capture.empty()) {
    if (!ValidateCaptureReference(captures, operand.Capture, "operand",
                                  captures.Operands, "emit operand capture",
                                  error)) {
      return false;
    }
  }

  for (const dxp::sm5::RecipeOperandIndexPattern &indexPattern :
       operand.IndexPatterns) {
    if (!indexPattern.MatchCapture.empty()) {
      const char *referenceSite =
          emitOperand ? "emit index match_capture" : "index match_capture";
      if (!ValidateCaptureReference(captures, indexPattern.MatchCapture,
                                    "index", captures.OperandIndices,
                                    referenceSite, error)) {
        return false;
      }
    }

    if (indexPattern.RelativeOperand) {
      if (!ValidateOperandCaptureReferences(*indexPattern.RelativeOperand,
                                            captures, emitOperand, error)) {
        return false;
      }
    }
  }

  return true;
}

static bool ValidateRuleCaptureReferences(const dxp::sm5::RecipeRule &rule,
                                          std::string &error) {
  CaptureNameTables captures;
  CollectMatchCaptures(rule.Match, captures);

  for (const dxp::sm5::RecipeOperandPattern &operand : rule.Match.Operands) {
    if (!ValidateOperandCaptureReferences(operand, captures, false, error)) {
      return false;
    }
  }

  for (const dxp::sm5::RecipeInstructionPattern &instruction :
       rule.Match.Sequence) {
    for (const dxp::sm5::RecipeOperandPattern &operand : instruction.Operands) {
      if (!ValidateOperandCaptureReferences(operand, captures, false, error)) {
        return false;
      }
    }
  }

  for (const dxp::sm5::RecipeInstructionTemplate &instruction : rule.Emit) {
    for (const dxp::sm5::RecipeOperandPattern &operand : instruction.Operands) {
      if (!ValidateOperandCaptureReferences(operand, captures, true, error)) {
        return false;
      }
    }
  }

  return true;
}

static bool TryParseComponentChar(char ch, D3D10_SB_4_COMPONENT_NAME &component,
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
                                      uint32_t &numComponents,
                                      uint32_t &componentMode,
                                      std::string &error) {
  const bool hasSelectorObject = !operandModel.components.kind.empty() ||
                                 !operandModel.components.value.empty();
  std::string selectToken;
  std::string maskToken;
  std::string swizzleToken;
  if (hasSelectorObject) {
    const std::string kind = operandModel.components.kind;
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
      error = "unsupported operand components.kind: " +
              operandModel.components.kind;
      return false;
    }
  }

  if (!selectToken.empty()) {
    D3D10_SB_4_COMPONENT_NAME component = D3D10_SB_4_COMPONENT_X;
    if (selectToken.size() != 1 ||
        !TryParseComponentChar(selectToken.front(), component, error)) {
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
      if (!TryParseComponentChar(swizzleToken[index], components[index],
                                 error)) {
        return false;
      }
    }
    numComponents = D3D10_SB_OPERAND_4_COMPONENT;
    componentMode =
        ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
            D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) |
        ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(
            components[0], components[1], components[2], components[3]);
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
    const size_t immediateWordCount = operandModel.indices.size() +
                                      operandModel.immediates_u32.size() +
                                      (operandModel.immediates_u64.size() * 2) +
                                      operandModel.immediates_i32.size() +
                                      (operandModel.immediates_i64.size() * 2) +
                                      operandModel.immediates_f32.size() +
                                      (operandModel.immediates_f64.size() * 2);
    numComponents = immediateWordCount > 1
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

static bool ParseEmitOperand(const YamlOperand &operandModel, Operand &operand,
                             std::string &error) {
  operand = Operand{};

  const std::string &captureRef = operandModel.capture;
  const bool hasCaptureReference = !captureRef.empty();

  const bool hasCaptureFields = operandModel.capture_fields.type ||
                                operandModel.capture_fields.components ||
                                operandModel.capture_fields.modifier ||
                                operandModel.capture_fields.indices ||
                                operandModel.capture_fields.immediates;
  if (operandModel.capture.empty() && hasCaptureFields) {
    error = "SM5 capture_fields requires emit operand capture";
    return false;
  }

  if (hasCaptureReference) {
    operand.CaptureName = captureRef;
    operand.CaptureType = operandModel.capture_fields.type;
    operand.CaptureComponents = operandModel.capture_fields.components;
    operand.CaptureModifier = operandModel.capture_fields.modifier;
    operand.CaptureIndices = operandModel.capture_fields.indices;
    operand.CaptureImmediates = operandModel.capture_fields.immediates;
  }

  if (!hasCaptureReference && operandModel.type.empty()) {
    error = "literal SM5 emit operands require type or capture";
    return false;
  }

  if (!operandModel.from_handle.empty() && !operandModel.type.empty()) {
    OperandType parsedType = D3D10_SB_OPERAND_TYPE_TEMP;
    if (!ParseOperandType(operandModel.type, parsedType, error)) {
      return false;
    }
  }

  if (!operandModel.type.empty()) {
    if (!ParseOperandType(operandModel.type, operand.Type, error)) {
      return false;
    }
  }

  const bool hasLiteralComponentSpec =
      !operandModel.components.kind.empty() ||
      !operandModel.components.value.empty() || operandModel.num_components >= 0;
  if (!hasCaptureReference || !operandModel.type.empty() ||
      hasLiteralComponentSpec) {
    if (!ParseOperandComponentMode(operandModel, operand.Type,
                                   operand.NumComponents,
                                   operand.ComponentMode, error)) {
      return false;
    }
  }

  std::vector<RecipeOperandIndexPattern> indexPatterns;
  if (!DecodeOperandIndexPatternsWithShorthands(
          operandModel, indexPatterns, false, true, error)) {
    return false;
  }

  operand.IndexEntries.clear();
  operand.IndexEntries.reserve(indexPatterns.size());
  for (const RecipeOperandIndexPattern &indexPattern : indexPatterns) {
    Operand::Index indexEntry;
    switch (indexPattern.Representation) {
    case RecipeOperandIndexRepresentation::Immediate32:
      indexEntry.Representation = Operand::IndexRepresentation::Immediate32;
      break;
    case RecipeOperandIndexRepresentation::Immediate64:
      indexEntry.Representation = Operand::IndexRepresentation::Immediate64;
      break;
    case RecipeOperandIndexRepresentation::Relative:
      indexEntry.Representation = Operand::IndexRepresentation::Relative;
      break;
    case RecipeOperandIndexRepresentation::Immediate32PlusRelative:
      indexEntry.Representation =
          Operand::IndexRepresentation::Immediate32PlusRelative;
      break;
    case RecipeOperandIndexRepresentation::Immediate64PlusRelative:
      indexEntry.Representation =
          Operand::IndexRepresentation::Immediate64PlusRelative;
      break;
    }
    indexEntry.HasImmediateLo = indexPattern.HasImmediateLo;
    indexEntry.ImmediateLo = indexPattern.ImmediateLo;
    indexEntry.HasImmediateHi = indexPattern.HasImmediateHi;
    indexEntry.ImmediateHi = indexPattern.ImmediateHi;
    indexEntry.MatchCaptureName = indexPattern.MatchCapture;
    operand.IndexEntries.push_back(std::move(indexEntry));
  }

  for (const Operand::Index &indexEntry : operand.IndexEntries) {
    if (indexEntry.HasImmediateLo) {
      operand.Indices.push_back(indexEntry.ImmediateLo);
    }
    if (indexEntry.HasImmediateHi) {
      operand.Indices.push_back(indexEntry.ImmediateHi);
    }
  }

  if (operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
      operand.Type == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) {
    operand.ImmediateValues = operand.Indices;
    operand.Indices.clear();
  }

  operand.FromHandle = operandModel.from_handle;

  if (!operandModel.modifier.empty() &&
      !ParseOperandModifier(operandModel.modifier, operand.Modifier, error)) {
    return false;
  }

  return true;
}

static bool ParseMatchOperand(const YamlOperand &operandModel,
                              OperandMatch &operandMatch, std::string &error) {
  operandMatch = OperandMatch{};
  operandMatch.Any = operandModel.any;

  if (operandMatch.Any && !operandModel.match_capture.empty()) {
    error = "SM5 any operand cannot use match_capture";
    return false;
  }

  const bool hasMatchCaptureFields =
      operandModel.match_capture_fields.type ||
      operandModel.match_capture_fields.components ||
      operandModel.match_capture_fields.modifier ||
      operandModel.match_capture_fields.indices ||
      operandModel.match_capture_fields.immediates;
  if (operandModel.match_capture.empty() && hasMatchCaptureFields) {
    error = "SM5 match_capture_fields requires operand match_capture";
    return false;
  }

  if (!operandModel.type.empty()) {
    if (!ParseOperandType(operandModel.type, operandMatch.MatchType, error)) {
      return false;
    }
    operandMatch.HasTypeMatch = true;
  }

  std::vector<RecipeOperandIndexPattern> indexPatterns;
  if (!DecodeOperandIndexPatternsWithShorthands(
          operandModel, indexPatterns, true, false, error)) {
    return false;
  }

  if (!indexPatterns.empty()) {
    operandMatch.MatchIndexPatterns.clear();
    for (const RecipeOperandIndexPattern &indexPattern : indexPatterns) {
      OperandIndexMatchPattern matchIndexPattern;
      matchIndexPattern.Any = indexPattern.Any;
      matchIndexPattern.HasRepresentation = true;
      switch (indexPattern.Representation) {
      case RecipeOperandIndexRepresentation::Immediate32:
        matchIndexPattern.Representation =
            Operand::IndexRepresentation::Immediate32;
        break;
      case RecipeOperandIndexRepresentation::Immediate64:
        matchIndexPattern.Representation =
            Operand::IndexRepresentation::Immediate64;
        break;
      case RecipeOperandIndexRepresentation::Relative:
        matchIndexPattern.Representation = Operand::IndexRepresentation::Relative;
        break;
      case RecipeOperandIndexRepresentation::Immediate32PlusRelative:
        matchIndexPattern.Representation =
            Operand::IndexRepresentation::Immediate32PlusRelative;
        break;
      case RecipeOperandIndexRepresentation::Immediate64PlusRelative:
        matchIndexPattern.Representation =
            Operand::IndexRepresentation::Immediate64PlusRelative;
        break;
      }
      matchIndexPattern.HasImmediateLo = indexPattern.HasImmediateLo;
      matchIndexPattern.ImmediateLo = indexPattern.ImmediateLo;
      matchIndexPattern.HasImmediateHi = indexPattern.HasImmediateHi;
      matchIndexPattern.ImmediateHi = indexPattern.ImmediateHi;
      matchIndexPattern.CaptureName = indexPattern.Capture;
      matchIndexPattern.MatchCapture = indexPattern.MatchCapture;
      operandMatch.MatchIndexPatterns.push_back(std::move(matchIndexPattern));
    }
  }

  OperandType componentType = operandMatch.HasTypeMatch
                                  ? operandMatch.MatchType
                                  : D3D10_SB_OPERAND_TYPE_TEMP;
  uint32_t numComponents = 0;
  uint32_t componentMode = 0;
    if (!operandModel.components.kind.empty() ||
      !operandModel.components.value.empty() ||
      operandModel.num_components >= 0) {
    if (!ParseOperandComponentMode(operandModel, componentType, numComponents,
                                   componentMode, error)) {
      return false;
    }
    operandMatch.MatchNumComponents = numComponents;
    operandMatch.HasNumComponentsMatch = true;
    operandMatch.MatchComponentMode = componentMode;
    operandMatch.HasComponentMatch = true;
  }

  if (!operandModel.modifier.empty()) {
    if (!ParseOperandModifier(operandModel.modifier, operandMatch.MatchModifier,
                              error)) {
      return false;
    }
    operandMatch.HasModifierMatch = true;
  }

  if (operandMatch.HasTypeMatch &&
      (operandMatch.MatchType == D3D10_SB_OPERAND_TYPE_IMMEDIATE32 ||
       operandMatch.MatchType == D3D10_SB_OPERAND_TYPE_IMMEDIATE64) &&
      !operandMatch.MatchIndexPatterns.empty()) {
    for (const OperandIndexMatchPattern &indexPattern :
         operandMatch.MatchIndexPatterns) {
      if (indexPattern.HasImmediateLo) {
        operandMatch.MatchImmediates.push_back(indexPattern.ImmediateLo);
      }
      if (indexPattern.HasImmediateHi) {
        operandMatch.MatchImmediates.push_back(indexPattern.ImmediateHi);
      }
    }
    operandMatch.HasImmediateMatch = true;
  }

  operandMatch.CaptureName = operandModel.capture;
  operandMatch.MatchAgainstCapture = operandModel.match_capture;
  operandMatch.MatchCaptureType = operandModel.match_capture_fields.type;
  operandMatch.MatchCaptureComponents =
      operandModel.match_capture_fields.components;
  operandMatch.MatchCaptureModifier = operandModel.match_capture_fields.modifier;
  operandMatch.MatchCaptureIndices = operandModel.match_capture_fields.indices;
  operandMatch.MatchCaptureImmediates =
      operandModel.match_capture_fields.immediates;
  return true;
}

static RecipeOperandCaptureFields BuildCaptureFields(
    const YamlOperandCaptureFields &yamlFields) {
  RecipeOperandCaptureFields fields;
  fields.Type = yamlFields.type;
  fields.Components = yamlFields.components;
  fields.Modifier = yamlFields.modifier;
  fields.Indices = yamlFields.indices;
  fields.Immediates = yamlFields.immediates;
  return fields;
}

static bool FillRecipeOperandPattern(const YamlOperand &operandModel,
                                     RecipeOperandPattern &operandPattern,
                                     bool allowEmitImmediateShorthands,
                                     std::string &error) {
  std::vector<RecipeOperandIndexPattern> indexPatterns;
  if (!DecodeOperandIndexPatternsWithShorthands(
          operandModel, indexPatterns, true,
          allowEmitImmediateShorthands, error)) {
    return false;
  }

  operandPattern = RecipeOperandPattern{}
                       .WithAny(operandModel.any)
                       .WithType(operandModel.type)
                       .WithIndexPatterns(std::move(indexPatterns))
                       .WithFromHandle(operandModel.from_handle)
                       .WithNumComponents(operandModel.num_components)
                       .WithModifier(operandModel.modifier)
                         .WithCaptureFields(
                           BuildCaptureFields(operandModel.capture_fields))
                       .WithMatchCapture(operandModel.match_capture)
                         .WithMatchCaptureFields(BuildCaptureFields(
                           operandModel.match_capture_fields))
                       .CaptureAs(operandModel.capture);

  if (!operandModel.components.kind.empty() ||
      !operandModel.components.value.empty()) {
    const std::string kind = operandModel.components.kind;
    if (kind == "mask") {
      operandPattern.WithMask(operandModel.components.value);
    } else if (kind == "swizzle") {
      operandPattern.WithSwizzle(operandModel.components.value);
    } else if (kind == "select") {
      operandPattern.WithSelect(operandModel.components.value);
    } else {
      error =
          "unsupported SM5 component selector: " + operandModel.components.kind;
      return false;
    }
  }

  return true;
}

static bool ResolveOpcodeAndTestBoolean(const std::string &opcodeName,
                                        int32_t requestedTestBoolean,
                                        Opcode &opcode,
                                        std::string &canonicalName,
                                        int32_t &resolvedTestBoolean,
                                        std::string &error,
                                        const char *context) {
  int32_t implicitTestBoolean = -1;
  if (!ParseOpcodeWithImplicitTestBoolean(opcodeName, opcode,
                                          implicitTestBoolean)) {
    error = std::string("Unknown SM5 opcode in ") + context + ": " + opcodeName;
    return false;
  }

  canonicalName = GetOpcodeName(opcode);
  resolvedTestBoolean = requestedTestBoolean;
  if (implicitTestBoolean >= 0) {
    if (resolvedTestBoolean >= 0 && resolvedTestBoolean != implicitTestBoolean) {
      error = std::string("SM5 opcode alias '") + opcodeName +
              "' conflicts with explicit test_boolean";
      return false;
    }
    resolvedTestBoolean = implicitTestBoolean;
  }

  if (resolvedTestBoolean >= 0 && !OpcodeUsesTestBoolean(opcode)) {
    error = std::string("SM5 test_boolean is not valid for opcode '") +
            opcodeName + "'";
    return false;
  }

  return true;
}

static bool BuildRecipeInstructionPattern(const YamlInstructionMatch &matchModel,
                                         RecipeInstructionPattern &pattern,
                                         std::string &error) {
  Opcode parsedOpcode;
  std::string canonicalOpcodeName;
  int32_t resolvedTestBoolean = matchModel.test_boolean;
  if (!ResolveOpcodeAndTestBoolean(matchModel.opcode, matchModel.test_boolean,
                                   parsedOpcode, canonicalOpcodeName,
                                   resolvedTestBoolean, error, "match")) {
    return false;
  }

  pattern = RecipeInstructionPattern{}
                .WithOpcode(canonicalOpcodeName)
                .CaptureAs(matchModel.capture)
                .WithSaturate(matchModel.saturate ? "true" : "false")
                .WithInterpolationMode(interpolationModeToString(matchModel.interpolation_mode))
                .WithTestBoolean(resolvedTestBoolean);

  for (const YamlOperand &operandModel : matchModel.operands) {
    RecipeOperandPattern operand;
    if (!FillRecipeOperandPattern(operandModel, operand, false, error)) {
      return false;
    }
    pattern.AddOperand(std::move(operand));
  }

  return true;
}

static bool ParseInstructionMatch(const YamlInstructionMatch &matchModel,
                                  InstructionMatch &instructionMatch,
                                  std::string &error) {
  instructionMatch = InstructionMatch{};

  std::string canonicalOpcodeName;
  int32_t resolvedTestBoolean = matchModel.test_boolean;
  if (!ResolveOpcodeAndTestBoolean(matchModel.opcode, matchModel.test_boolean,
                                   instructionMatch.Opcode,
                                   canonicalOpcodeName, resolvedTestBoolean,
                                   error, "match")) {
    return false;
  }
  instructionMatch.HasOpcode = true;
  instructionMatch.CaptureName = matchModel.capture;
  instructionMatch.HasSaturateMatch = true;
  instructionMatch.SaturateValue = matchModel.saturate;
  if (resolvedTestBoolean >= 0) {
    instructionMatch.HasTestBooleanMatch = true;
    instructionMatch.MatchTestBoolean =
      static_cast<uint32_t>(resolvedTestBoolean);
  }
  if (matchModel.interpolation_mode != dxp::sm5::InterpolationMode::Undefined) {
    instructionMatch.HasInputInterpolationModeMatch = true;
    instructionMatch.MatchInputInterpolationMode =
        static_cast<uint32_t>(matchModel.interpolation_mode);
    if (instructionMatch.Opcode != Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS} &&
        instructionMatch.Opcode != Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS_SIV}) {
      error = "SM5 interpolation_mode is only valid for dcl_input_ps and "
              "dcl_input_ps_siv";
      return false;
    }
  }
  for (const YamlOperand &operandModel : matchModel.operands) {
    OperandMatch operandMatch;
    if (!ParseMatchOperand(operandModel, operandMatch, error)) {
      return false;
    }
    instructionMatch.OperandPatterns.push_back(std::move(operandMatch));
  }

  return true;
}

static bool ParseRule(const YamlRule &ruleModel,
                      RecipeRuleApplicationMode inheritedMode, RecipeRule &rule,
                      std::string &error) {
  if (!ruleModel.replace.empty()) {
    error = "SM5 rule.replace was removed; use match.rewrite_mode and emit";
    return false;
  }

  rule = RecipeRule{}.Named(ruleModel.name).ApplyMode(inheritedMode);
  rule.RequireMatch(ruleModel.required_match);

  if (ruleModel.mode != RecipeRuleApplicationMode::First) {
    rule.ApplyMode(ruleModel.mode);
  }

  rule.RewriteAs(ruleModel.match.rewrite_mode);
  const RecipeRuleRewriteMode rewriteMode = ruleModel.match.rewrite_mode;
  rule.RangeOffsets(ruleModel.match.range_start_offset,
                    ruleModel.match.range_end_offset);
  rule.InsertAfterRelativeIndex(ruleModel.match.insert_relative_index);

  const bool hasCustomRangeOffsets =
      ruleModel.match.range_start_offset != 0 ||
      ruleModel.match.range_end_offset != -1;
  const bool hasInsertRelativeIndex =
      ruleModel.match.insert_relative_index >= 0;
  if (ruleModel.match.range_start_offset < 0) {
    error = "SM5 match.range_start_offset must be >= 0";
    return false;
  }
  if (ruleModel.match.range_end_offset < -1) {
    error = "SM5 match.range_end_offset must be -1 or >= 0";
    return false;
  }
  if (rewriteMode != RecipeRuleRewriteMode::ReplaceRange &&
      hasCustomRangeOffsets) {
    error =
        "SM5 range offsets require match.rewrite_mode: replace_range";
    return false;
  }
  if (ruleModel.match.insert_relative_index < -1) {
    error = "SM5 match.insert_relative_index must be -1 or >= 0";
    return false;
  }
  if (rewriteMode != RecipeRuleRewriteMode::Before &&
      rewriteMode != RecipeRuleRewriteMode::After &&
      hasInsertRelativeIndex) {
    error = "SM5 match.insert_relative_index requires match.rewrite_mode: "
            "before or after";
    return false;
  }
  if ((rewriteMode == RecipeRuleRewriteMode::Before ||
       rewriteMode == RecipeRuleRewriteMode::After) &&
      ruleModel.match.insert_relative_index < 0) {
    error = "SM5 before/after rewrites require match.insert_relative_index";
    return false;
  }

  if (!ruleModel.match.sequence.empty()) {
    if (!ruleModel.match.opcode.empty() || !ruleModel.match.capture.empty() ||
        ruleModel.match.test_boolean >= 0 ||
        ruleModel.match.test_boolean >= 0 ||
        !ruleModel.match.operands.empty()) {
      error = "SM5 match.sequence cannot be combined with single-instruction "
              "match fields";
      return false;
    }

    RecipeMatchPattern match;
    for (const YamlInstructionMatch &matchModel : ruleModel.match.sequence) {
      InstructionMatch instructionMatch;
      if (!ParseInstructionMatch(matchModel, instructionMatch, error)) {
        return false;
      }

      RecipeInstructionPattern pattern;
      if (!BuildRecipeInstructionPattern(matchModel, pattern, error)) {
        return false;
      }
      match.AddInstruction(std::move(pattern));
    }
    rule.WithMatch(std::move(match));
  } else if (!ruleModel.match.opcode.empty()) {
    Opcode parsedOpcode;
    std::string canonicalOpcodeName;
    int32_t resolvedTestBoolean = ruleModel.match.test_boolean;
    if (!ResolveOpcodeAndTestBoolean(ruleModel.match.opcode,
                                     ruleModel.match.test_boolean,
                                     parsedOpcode, canonicalOpcodeName,
                                     resolvedTestBoolean, error, "match")) {
      return false;
    }

    RecipeMatchPattern match = RecipeMatchPattern{}
                                   .WithOpcode(canonicalOpcodeName)
                                   .CaptureAs(ruleModel.match.capture)
                                   .WithSaturate(ruleModel.match.saturate ? "true" : "false")
                                   .WithInterpolationMode(
                                       interpolationModeToString(ruleModel.match.interpolation_mode))
                                   .WithTestBoolean(resolvedTestBoolean);
    for (const YamlOperand &operandModel : ruleModel.match.operands) {
      OperandMatch operandMatch;
      if (!ParseMatchOperand(operandModel, operandMatch, error)) {
        return false;
      }
      RecipeOperandPattern operand;
      if (!FillRecipeOperandPattern(operandModel, operand, false, error)) {
        return false;
      }
      match.AddOperand(std::move(operand));
    }
    rule.WithMatch(std::move(match));
  } else {
    error = "SM5 rules require match.opcode or match.sequence";
    return false;
  }

  if (rule.RewriteMode == RecipeRuleRewriteMode::None) {
    if (!ruleModel.emit.empty() || hasCustomRangeOffsets ||
        hasInsertRelativeIndex) {
      error = "SM5 rewrite mode None cannot be combined with emit, "
              "range offsets, or insert_relative_index";
      return false;
    }
  } else {
    if (ruleModel.emit.empty()) {
      error = "SM5 rules without emit must use match.rewrite_mode: None";
      return false;
    }
  }

  for (const YamlEmitInstruction &emitModel : ruleModel.emit) {
    if (emitModel.opcode.empty()) {
      error = "SM5 emit entries require opcode";
      return false;
    }

    Instruction instruction;
    Opcode parsedOpcode;
    std::string canonicalOpcodeName;
    int32_t resolvedTestBoolean = emitModel.test_boolean;
    if (!ResolveOpcodeAndTestBoolean(emitModel.opcode, emitModel.test_boolean,
                                     parsedOpcode, canonicalOpcodeName,
                                     resolvedTestBoolean, error, "emit")) {
      return false;
    }
    instruction.Opcode = parsedOpcode;
    instruction.Controls.Saturate = emitModel.saturate;
    if (resolvedTestBoolean >= 0) {
      instruction.Controls.HasTestBoolean = true;
      instruction.Controls.TestBoolean =
          static_cast<uint32_t>(resolvedTestBoolean);
    }
    if (emitModel.interpolation_mode != dxp::sm5::InterpolationMode::Undefined) {
      instruction.Controls.HasInputInterpolationMode = true;
      instruction.Controls.InputInterpolationMode =
          static_cast<uint32_t>(emitModel.interpolation_mode);
      const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
      if (opcode != D3D10_SB_OPCODE_DCL_INPUT_PS &&
          opcode != D3D10_SB_OPCODE_DCL_INPUT_PS_SIV) {
        error = "SM5 interpolation_mode is only valid for dcl_input_ps and "
                "dcl_input_ps_siv";
        return false;
      }
    }
    for (const YamlOperand &operandModel : emitModel.operands) {
      Operand operand;
      if (!ParseEmitOperand(operandModel, operand, error)) {
        return false;
      }
      instruction.Operands.push_back(std::move(operand));
    }
    RecipeInstructionTemplate emitInstruction =
        RecipeInstructionTemplate{}
        .WithOpcode(canonicalOpcodeName)
        .WithSaturate(emitModel.saturate ? "true" : "false")
        .WithInterpolationMode(interpolationModeToString(emitModel.interpolation_mode))
        .WithTestBoolean(resolvedTestBoolean);
    for (const YamlOperand &operandModel : emitModel.operands) {
      RecipeOperandPattern operandPattern;
      if (!FillRecipeOperandPattern(operandModel, operandPattern, true,
                                    error)) {
        return false;
      }
      emitInstruction.AddOperand(std::move(operandPattern));
    }
    rule.AddEmit(std::move(emitInstruction));
  }

  if (!ValidateRuleCaptureReferences(rule, error)) {
    return false;
  }

  return true;
}

static bool CollectDeclarationHandlesFromSteps(
    const std::vector<YamlStep> &steps,
  std::unordered_map<std::string, uint32_t> &tempHandles,
    std::unordered_map<std::string, uint32_t> &inputHandles,
    std::unordered_map<std::string, uint32_t> &outputHandles,
    std::unordered_map<std::string, uint32_t> &resourceHandles,
    std::unordered_map<std::string, uint32_t> &cbufferHandles,
    std::unordered_map<std::string, uint32_t> &samplerHandles,
    std::unordered_map<std::string, uint32_t> &uavHandles, std::string &error) {
  uint32_t ordinal = 0;
  auto insertHandle = [&](std::unordered_map<std::string, uint32_t> &table,
                          const std::string &handle, const char *kind) -> bool {
    if (handle.empty()) {
      return true;
    }
    if (!table.emplace(handle, ordinal++).second) {
      error = std::string("duplicate SM5 ") + kind + " declaration handle: '" +
              handle + "'";
      return false;
    }
    return true;
  };

  for (const YamlStep &step : steps) {
    const std::string stepKind =
      step.kind.empty() ? "apply_rules" : step.kind;
    if (stepKind == "add_temp") {
      if (!step.handle.empty()) {
        error = "add_temp steps no longer support handle; use handles";
        return false;
      }

      if (step.handles.empty()) {
        error = "add_temp steps require handles";
        return false;
      }

      for (const std::string &tempHandle : step.handles) {
        if (tempHandle.empty()) {
          error = "add_temp handles entries must be non-empty";
          return false;
        }
        if (!insertHandle(tempHandles, tempHandle, "temp")) {
          return false;
        }
      }
    } else if (stepKind == "add_input") {
      if (!insertHandle(inputHandles, step.handle, "input")) {
        return false;
      }
    } else if (stepKind == "add_output") {
      if (!insertHandle(outputHandles, step.handle, "output")) {
        return false;
      }
    } else if (stepKind == "add_texture" || stepKind == "add_raw_resource" ||
               stepKind == "add_structured_resource") {
      if (!insertHandle(resourceHandles, step.handle, "resource")) {
        return false;
      }
    } else if (stepKind == "add_cbuffer") {
      if (!insertHandle(cbufferHandles, step.handle, "cbuffer")) {
        return false;
      }
    } else if (stepKind == "add_sampler") {
      if (!insertHandle(samplerHandles, step.handle, "sampler")) {
        return false;
      }
    } else if (stepKind == "add_uav") {
      if (!insertHandle(uavHandles, step.handle, "uav")) {
        return false;
      }
    }
  }

  return true;
}

static bool ValidateUniqueDeclarationHandles(const YamlRecipeDocument &document,
                                             std::string &error) {
  std::unordered_map<std::string, uint32_t> tempHandles;
  std::unordered_map<std::string, uint32_t> inputHandles;
  std::unordered_map<std::string, uint32_t> outputHandles;
  std::unordered_map<std::string, uint32_t> resourceHandles;
  std::unordered_map<std::string, uint32_t> cbufferHandles;
  std::unordered_map<std::string, uint32_t> samplerHandles;
  std::unordered_map<std::string, uint32_t> uavHandles;
  return CollectDeclarationHandlesFromSteps(
      document.steps, tempHandles, inputHandles, outputHandles, resourceHandles,
      cbufferHandles, samplerHandles, uavHandles, error);
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
  if (operand.from_handle.empty()) {
    return true;
  }

  if (operand.type.empty()) {
    error = "SM5 from_handle emit operands require explicit operand type";
    return false;
  }

  OperandType type = D3D10_SB_OPERAND_TYPE_TEMP;
  if (!ParseOperandType(operand.type, type, error)) {
    return false;
  }

  switch (type) {
  case D3D10_SB_OPERAND_TYPE_TEMP:
    if (tempHandles.find(operand.from_handle) == tempHandles.end()) {
      error = "SM5 from_handle references unknown temp declaration handle '" +
              operand.from_handle + "'";
      return false;
    }
    return true;
  case D3D10_SB_OPERAND_TYPE_INPUT:
    if (inputHandles.find(operand.from_handle) == inputHandles.end()) {
      error = "SM5 from_handle references unknown input declaration handle '" +
              operand.from_handle + "'";
      return false;
    }
    return true;
  case D3D10_SB_OPERAND_TYPE_OUTPUT:
    if (outputHandles.find(operand.from_handle) == outputHandles.end()) {
      error = "SM5 from_handle references unknown output declaration handle '" +
              operand.from_handle + "'";
      return false;
    }
    return true;
  case D3D10_SB_OPERAND_TYPE_RESOURCE:
    if (resourceHandles.find(operand.from_handle) == resourceHandles.end()) {
      error =
          "SM5 from_handle references unknown resource declaration handle '" +
          operand.from_handle + "'";
      return false;
    }
    return true;
  case D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW:
    if (uavHandles.find(operand.from_handle) == uavHandles.end()) {
      error = "SM5 from_handle references unknown uav declaration handle '" +
              operand.from_handle + "'";
      return false;
    }
    return true;
  case D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER:
    if (cbufferHandles.find(operand.from_handle) == cbufferHandles.end()) {
      error =
          "SM5 from_handle references unknown cbuffer declaration handle '" +
          operand.from_handle + "'";
      return false;
    }
    return true;
  case D3D10_SB_OPERAND_TYPE_SAMPLER:
    if (samplerHandles.find(operand.from_handle) == samplerHandles.end()) {
      error =
          "SM5 from_handle references unknown sampler declaration handle '" +
          operand.from_handle + "'";
      return false;
    }
    return true;
  default:
    error = "SM5 from_handle operand type is unsupported for resource binding";
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

  if (!CollectDeclarationHandlesFromSteps(
      document.steps, tempHandles, inputHandles, outputHandles,
      resourceHandles,
          cbufferHandles, samplerHandles, uavHandles, error)) {
    return false;
  }

  for (const YamlStep &step : document.steps) {
    for (const YamlRule &rule : step.rules) {
      for (const YamlEmitInstruction &emit : rule.emit) {
        for (const YamlOperand &operand : emit.operands) {
          if (!ValidateEmitOperandHandleReference(
                  operand, tempHandles, inputHandles, outputHandles,
                  resourceHandles, cbufferHandles, samplerHandles, uavHandles,
                  error)) {
            return false;
          }
        }
      }
    }
  }

  return true;
}

static bool BuildStepCondition(const YamlStepCondition &conditionModel,
                               RecipeStepCondition &condition,
                               std::string &error) {
  condition = RecipeStepCondition{};
  condition.Negate = conditionModel.not_condition;

  auto isComparisonSet = [](const YamlStepCondition::Comparison &comparison) {
    return !comparison.state.empty() || !comparison.input.empty() ||
           !comparison.value.empty();
  };

  auto buildComparison = [&](const YamlStepCondition::Comparison &comparison,
                             RecipeConditionCompareOp op) -> bool {
    const bool hasState = !comparison.state.empty();
    const bool hasInput = !comparison.input.empty();
    if (hasState == hasInput) {
      error =
          "SM5 step if comparison requires exactly one of state or input";
      return false;
    }
    if (comparison.value.empty()) {
      error = "SM5 step if comparison requires non-empty value";
      return false;
    }

    RecipeStepComparison compiledComparison;
    if (hasState) {
      compiledComparison.FromState(comparison.state);
    } else {
      compiledComparison.FromInput(comparison.input);
    }
    compiledComparison.WithValue(comparison.value);
    condition.CompareOp = op;
    condition.Compare = std::move(compiledComparison);
    return true;
  };

  size_t populatedFields = 0;
  if (!conditionModel.state.empty()) {
    ++populatedFields;
  }
  if (!conditionModel.input.empty()) {
    ++populatedFields;
  }
  if (!conditionModel.and_conditions.empty()) {
    ++populatedFields;
  }
  if (!conditionModel.or_conditions.empty()) {
    ++populatedFields;
  }
  if (isComparisonSet(conditionModel.eq)) {
    ++populatedFields;
  }
  if (isComparisonSet(conditionModel.ne)) {
    ++populatedFields;
  }
  if (isComparisonSet(conditionModel.gt)) {
    ++populatedFields;
  }
  if (isComparisonSet(conditionModel.gte)) {
    ++populatedFields;
  }
  if (isComparisonSet(conditionModel.lt)) {
    ++populatedFields;
  }
  if (isComparisonSet(conditionModel.lte)) {
    ++populatedFields;
  }

  if (populatedFields == 0) {
    return true;
  }

  if (populatedFields != 1) {
    error = "SM5 step if must specify exactly one of state, input, and, or, eq, ne, gt, gte, lt, or lte";
    return false;
  }

  if (!conditionModel.state.empty()) {
    condition.State = conditionModel.state;
    if (condition.State.empty()) {
      error = "SM5 step if.state must not be empty";
      return false;
    }
    return true;
  }

  if (!conditionModel.input.empty()) {
    condition.Input = conditionModel.input;
    if (condition.Input.empty()) {
      error = "SM5 step if.input must not be empty";
      return false;
    }
    return true;
  }

  if (!conditionModel.and_conditions.empty() || !conditionModel.or_conditions.empty()) {
    std::vector<RecipeStepCondition> &destination =
        !conditionModel.and_conditions.empty() ? condition.All : condition.Any;
    const std::vector<YamlStepCondition> &source =
        !conditionModel.and_conditions.empty() ? conditionModel.and_conditions
                                               : conditionModel.or_conditions;
    destination.reserve(source.size());
    for (const YamlStepCondition &childModel : source) {
      RecipeStepCondition childCondition;
      if (!BuildStepCondition(childModel, childCondition, error)) {
        return false;
      }
      if (!childCondition.IsSet()) {
        error = "SM5 nested step if condition must not be empty";
        return false;
      }
      destination.push_back(std::move(childCondition));
    }
    return true;
  }

  if (isComparisonSet(conditionModel.eq)) {
    return buildComparison(conditionModel.eq, RecipeConditionCompareOp::Eq);
  }
  if (isComparisonSet(conditionModel.ne)) {
    return buildComparison(conditionModel.ne, RecipeConditionCompareOp::Ne);
  }
  if (isComparisonSet(conditionModel.gt)) {
    return buildComparison(conditionModel.gt, RecipeConditionCompareOp::Gt);
  }
  if (isComparisonSet(conditionModel.gte)) {
    return buildComparison(conditionModel.gte, RecipeConditionCompareOp::Gte);
  }
  if (isComparisonSet(conditionModel.lt)) {
    return buildComparison(conditionModel.lt, RecipeConditionCompareOp::Lt);
  }
  if (isComparisonSet(conditionModel.lte)) {
    return buildComparison(conditionModel.lte, RecipeConditionCompareOp::Lte);
  }

  return true;
}

} // namespace

bool ParseRecipeText(const std::string &recipeText,
                     RecipeParseResult &result,
                     const std::string &sourceName) {
  result = RecipeParseResult{};

  YamlRecipeDocument document;
  llvm::yaml::Input input(recipeText);
  input >> document;
  if (input.error()) {
    result.Error = sourceName + ": " + input.error().message();
    return false;
  }

  if (document.version != 1) {
    result.Error = sourceName + ": unsupported SM5 recipe schema version";
    return false;
  }

  std::string parseError;

  if (!document.rewrite_rules.empty()) {
    result.Error = sourceName + ": schema version 1 requires steps and "
                                      "does not allow top-level rewrite_rules";
    return false;
  }
  if (document.steps.empty()) {
    result.Error =
        sourceName + ": schema version 1 requires at least one step";
    return false;
  }
  if (!document.input_decls.empty() || !document.output_decls.empty() ||
      !document.texture_decls.empty() || !document.raw_resource_decls.empty() ||
      !document.structured_resource_decls.empty() ||
      !document.cbuffer_decls.empty() || !document.sampler_decls.empty() ||
      !document.uav_decls.empty()) {
    result.Error = sourceName +
                   ": schema version 1 does not allow top-level "
                   "*_decls; use add_* declaration steps";
    return false;
  }

  if (!ValidateUniqueDeclarationHandles(document, parseError)) {
    result.Error = sourceName + ": " + parseError;
    return false;
  }

  if (!ValidateEmitHandleReferences(document, parseError)) {
    result.Error = sourceName + ": " + parseError;
    return false;
  }

  auto appendRule = [&](const YamlRule &ruleModel,
                        RecipeRuleApplicationMode inheritedMode,
                        std::vector<RecipeRule> &rules) -> bool {
    RecipeRule rule;
    if (!ParseRule(ruleModel, inheritedMode, rule, parseError)) {
      result.Error = sourceName + ": " + parseError;
      return false;
    }
    rules.push_back(std::move(rule));
    return true;
  };

  std::unordered_map<std::string, std::string> seenNames;

  auto reserveName = [&](const std::string &name,
                         const std::string &kind) -> bool {
    const auto [it, inserted] = seenNames.emplace(name, kind);
    if (inserted) {
      return true;
    }

    result.Error = sourceName + ": duplicate SM5 name '" + name +
                   "' reused by " + kind + " (already used by " +
                   it->second + ")";
    return false;
  };

  for (const YamlStep &stepModel : document.steps) {
    const std::string stepKind =
      stepModel.kind.empty() ? "apply_rules" : stepModel.kind;
    if (stepModel.name.empty()) {
      result.Error = sourceName +
                     ": SM5 step names are required and must be unique";
      return false;
    }
    const std::string stepName = stepModel.name;

    if (!reserveName(stepName, "step")) {
      return false;
    }

    RecipeStepCondition stepCondition;
    if (!BuildStepCondition(stepModel.if_condition, stepCondition, parseError)) {
      result.Error = sourceName + ": " + parseError;
      return false;
    }

    if (stepKind == "apply_rules") {
      RecipeRuleApplicationMode applicationMode = stepModel.mode;

      std::vector<RecipeRule> rules;
      rules.reserve(stepModel.rules.size());

      for (const YamlRule &ruleModel : stepModel.rules) {
        if (ruleModel.name.empty()) {
          result.Error = sourceName +
                         ": SM5 rule names are required and must be unique";
          return false;
        }
        if (!reserveName(ruleModel.name, "rule")) {
          return false;
        }
        if (!appendRule(ruleModel, applicationMode, rules)) {
          return false;
        }
      }

      result.Recipe.AddStep(MakeRewriteRulesStep(stepName, std::move(rules),
                                                applicationMode,
                                                stepModel.abort_on_failure)
                                .When(stepCondition));
      continue;
    }

    if (stepKind == "check_shader_version") {
      if (!stepModel.rules.empty()) {
        result.Error = sourceName +
                       ": SM5 check_shader_version steps cannot define rules";
        return false;
      }
      if (stepModel.mode != RecipeRuleApplicationMode::First) {
        result.Error = sourceName +
                       ": SM5 step mode is only valid for apply_rules steps";
        return false;
      }
      if (stepModel.major < 0 || stepModel.minor < 0) {
        result.Error = sourceName +
                       ": SM5 check_shader_version steps require major and minor";
        return false;
      }

      result.Recipe.AddStep(
          MakeCheckShaderVersionStep(stepName,
                                     static_cast<uint32_t>(stepModel.major),
                                     static_cast<uint32_t>(stepModel.minor),
                                     stepModel.abort_on_failure)
              .When(stepCondition));
      continue;
    }

    if (stepKind == "check_opcode_count") {
      if (!stepModel.rules.empty()) {
        result.Error = sourceName +
                       ": SM5 check_opcode_count steps cannot define rules";
        return false;
      }
      if (stepModel.mode != RecipeRuleApplicationMode::First) {
        result.Error = sourceName +
                       ": SM5 step mode is only valid for apply_rules steps";
        return false;
      }
      if (stepModel.opcode.empty()) {
        result.Error = sourceName +
                       ": SM5 check_opcode_count steps require opcode";
        return false;
      }
      if (stepModel.expected_count == INT_MIN) {
        result.Error = sourceName +
                       ": SM5 check_opcode_count steps require expected_count";
        return false;
      }

      result.Recipe.AddStep(
          MakeCheckOpcodeCountStep(stepName, stepModel.opcode,
                                   stepModel.expected_count,
                                   stepModel.abort_on_failure)
              .When(stepCondition));
      continue;
    }

    if (stepKind == "check_resource_count") {
      if (!stepModel.rules.empty()) {
        result.Error = sourceName +
                       ": SM5 check_resource_count steps cannot define rules";
        return false;
      }
      if (stepModel.mode != RecipeRuleApplicationMode::First) {
        result.Error = sourceName +
                       ": SM5 step mode is only valid for apply_rules steps";
        return false;
      }
      if (stepModel.expected_resources == INT_MIN) {
        result.Error = sourceName +
                       ": SM5 check_resource_count steps require expected_resources";
        return false;
      }

      result.Recipe.AddStep(
          MakeCheckResourceCountStep(stepName, stepModel.expected_resources,
                                     stepModel.abort_on_failure)
              .When(stepCondition));
      continue;
    }

    if (stepModel.mode != RecipeRuleApplicationMode::First) {
      result.Error = sourceName +
                     ": SM5 step mode is only valid for apply_rules steps";
      return false;
    }

    if (!stepModel.rules.empty()) {
      result.Error =
          sourceName + ": SM5 non-apply_rules steps cannot define rules";
      return false;
    }

    if (stepKind == "add_input") {
      RecipeInputDecl decl = RecipeInputDecl{}
                                .WithBindPoint(stepModel.bind_point >= 0
                                                   ? static_cast<uint32_t>(
                                                         stepModel.bind_point)
                                                   : 0u)
                                .WithHandle(stepModel.handle)
                                .AutoBindToNext(stepModel.auto_bind);
      uint32_t interpolatedMode = static_cast<uint32_t>(stepModel.interpolation_mode);
      decl.WithInterpolationMode(interpolatedMode);
      result.Recipe.AddStep(MakeAddInputStep(stepName, std::move(decl))
                                .AbortOnFailureFlag(stepModel.abort_on_failure)
                                .When(stepCondition));
    } else if (stepKind == "add_temp") {
      if (!stepModel.handle.empty()) {
        result.Error = sourceName +
                       ": add_temp steps no longer support handle; use "
                       "handles";
        return false;
      }

      if (stepModel.handles.empty()) {
        result.Error = sourceName +
                       ": add_temp steps require handles";
        return false;
      }
      if (stepModel.bind_point >= 0) {
        result.Error = sourceName +
                       ": add_temp steps do not allow bind_point";
        return false;
      }
      if (stepModel.auto_bind) {
        result.Error = sourceName +
                       ": add_temp steps do not allow auto_bind";
        return false;
      }

      std::vector<std::string> tempHandles;
      for (const std::string &tempHandle : stepModel.handles) {
        if (tempHandle.empty()) {
          result.Error = sourceName +
                         ": add_temp handles entries must be non-empty";
          return false;
        }
        tempHandles.push_back(tempHandle);
      }

      for (size_t tempIndex = 0; tempIndex < tempHandles.size(); ++tempIndex) {
        RecipeTempDecl decl = RecipeTempDecl{}.WithHandle(tempHandles[tempIndex]);
        const std::string tempStepName =
            tempHandles.size() == 1
                ? stepName
                : stepName + "[" + std::to_string(tempIndex) + "]";

        if (tempHandles.size() > 1 && !reserveName(tempStepName, "step")) {
          return false;
        }

        result.Recipe.AddStep(MakeAddTempStep(tempStepName, std::move(decl))
                                  .AbortOnFailureFlag(stepModel.abort_on_failure)
                                  .When(stepCondition));
      }
    } else if (stepKind == "add_output") {
      RecipeOutputDecl decl = RecipeOutputDecl{}
                                 .WithBindPoint(stepModel.bind_point >= 0
                                                    ? static_cast<uint32_t>(
                                                          stepModel.bind_point)
                                                    : 0u)
                                 .WithHandle(stepModel.handle)
                                 .AutoBindToNext(stepModel.auto_bind);
      result.Recipe.AddStep(MakeAddOutputStep(stepName, std::move(decl))
                                .AbortOnFailureFlag(stepModel.abort_on_failure)
                                .When(stepCondition));
    } else if (stepKind == "add_texture") {
      RecipeTextureDecl decl = RecipeTextureDecl{}
                                  .WithBindPoint(stepModel.bind_point >= 0
                                                     ? static_cast<uint32_t>(
                                                           stepModel.bind_point)
                                                     : 0u)
                                  .WithHandle(stepModel.handle)
                                  .AutoBindToNext(stepModel.auto_bind);
      decl.WithDimension(static_cast<uint32_t>(stepModel.dimension));
      result.Recipe.AddStep(MakeAddTextureStep(stepName, std::move(decl))
                                .AbortOnFailureFlag(stepModel.abort_on_failure)
                                .When(stepCondition));
    } else if (stepKind == "add_raw_resource") {
      RecipeRawResourceDecl decl = RecipeRawResourceDecl{}
                                      .WithBindPoint(stepModel.bind_point >= 0
                                                         ? static_cast<uint32_t>(
                                                               stepModel.bind_point)
                                                         : 0u)
                                      .WithHandle(stepModel.handle)
                                      .AutoBindToNext(stepModel.auto_bind);
      result.Recipe.AddStep(MakeAddRawResourceStep(stepName, std::move(decl))
                                .AbortOnFailureFlag(stepModel.abort_on_failure)
                                .When(stepCondition));
    } else if (stepKind == "add_structured_resource") {
      RecipeStructuredResourceDecl decl =
          RecipeStructuredResourceDecl{}
              .WithBindPoint(stepModel.bind_point >= 0
                                 ? static_cast<uint32_t>(stepModel.bind_point)
                                 : 0u)
              .WithStructureStride(stepModel.stride)
              .WithHandle(stepModel.handle)
              .AutoBindToNext(stepModel.auto_bind);
      result.Recipe.AddStep(
          MakeAddStructuredResourceStep(stepName, std::move(decl))
            .AbortOnFailureFlag(stepModel.abort_on_failure)
              .When(stepCondition));
    } else if (stepKind == "add_cbuffer") {
      RecipeCBufferDecl decl = RecipeCBufferDecl{}
                                  .WithBindPoint(stepModel.bind_point >= 0
                                                     ? static_cast<uint32_t>(
                                                           stepModel.bind_point)
                                                     : 0u)
                                  .WithElements(stepModel.elements)
                                  .WithHandle(stepModel.handle)
                                  .AutoBindToNext(stepModel.auto_bind);
      decl.WithAccessPattern(static_cast<uint32_t>(stepModel.access_pattern));
      result.Recipe.AddStep(MakeAddCBufferStep(stepName, std::move(decl))
                                .AbortOnFailureFlag(stepModel.abort_on_failure)
                                .When(stepCondition));
    } else if (stepKind == "add_sampler") {
      RecipeSamplerDecl decl = RecipeSamplerDecl{}
                                  .WithBindPoint(stepModel.bind_point >= 0
                                                     ? static_cast<uint32_t>(
                                                           stepModel.bind_point)
                                                     : 0u)
                                  .WithHandle(stepModel.handle)
                                  .AutoBindToNext(stepModel.auto_bind);
      decl.WithMode(static_cast<uint32_t>(stepModel.sampler_mode));
      result.Recipe.AddStep(MakeAddSamplerStep(stepName, std::move(decl))
                                .AbortOnFailureFlag(stepModel.abort_on_failure)
                                .When(stepCondition));
    } else if (stepKind == "add_uav") {
      RecipeUavDecl decl = RecipeUavDecl{}
                              .WithBindPoint(stepModel.bind_point >= 0
                                                 ? static_cast<uint32_t>(
                                                       stepModel.bind_point)
                                                 : 0u)
                              .WithHandle(stepModel.handle)
                              .AutoBindToNext(stepModel.auto_bind)
                              .WithStructureStride(stepModel.stride)
                              .WithGloballyCoherent(stepModel.globally_coherent)
                              .WithOrderPreservingCounter(stepModel.has_counter)
                              .WithKind(stepModel.uav_kind)
                              .WithDimension(static_cast<uint32_t>(stepModel.dimension));
      result.Recipe.AddStep(MakeAddUavStep(stepName, std::move(decl))
                                .AbortOnFailureFlag(stepModel.abort_on_failure)
                                .When(stepCondition));
    } else {
      result.Error = sourceName + ": unsupported SM5 step kind '" +
                     stepModel.kind + "'";
      return false;
    }
  }

  return true;
}

bool ParseRecipeFile(const std::string &recipePath, RecipeParseResult &result) {
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

