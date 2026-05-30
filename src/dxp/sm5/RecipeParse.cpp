#include "dxp/sm5/RecipeParse.h"

#include "d3d11TokenizedProgramFormat.hpp"

#include "dxp/sm5/Model.h"
#include "dxp/sm5/Transforms.h"

#include <cctype>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_set>

#include "llvm/Support/YAMLTraits.h"

namespace {

static std::string Lowercase(const std::string &value) {
  std::string lowered = value;
  for (char &ch : lowered) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return lowered;
}

static std::string Trim(const std::string &value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return value.substr(start, end - start);
}

static std::string StripOptionalQuotes(const std::string &value) {
  if (value.size() >= 2 &&
      ((value.front() == '"' && value.back() == '"') ||
       (value.front() == '\'' && value.back() == '\''))) {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

static bool TryNormalizeReplayFromLine(const std::string &line,
                                       const std::string &sourceField,
                                       const std::string &targetField,
                                       std::string &normalizedLine,
                                       std::string &captureName) {
  const std::string fieldPattern =
      sourceField == "immediate_hi"
      ? R"(^([\t ]*(?:- [\t ]*)?)immediate_hi:[\t ]*\{[\t ]*from:[\t ]*([^}]+)[\t ]*\}[\t ]*$)"
      : sourceField == "immediate_lo"
    ? R"(^([\t ]*(?:- [\t ]*)?)immediate_lo:[\t ]*\{[\t ]*from:[\t ]*([^}]+)[\t ]*\}[\t ]*$)"
            : sourceField == "capture"
      ? R"(^([\t ]*(?:- [\t ]*)?)capture:[\t ]*\{[\t ]*from:[\t ]*([^}]+)[\t ]*\}[\t ]*$)"
                  : sourceField == "match_capture"
        ? R"(^([\t ]*(?:- [\t ]*)?)match_capture:[\t ]*\{[\t ]*from:[\t ]*([^}]+)[\t ]*\}[\t ]*$)"
        : R"(^([\t ]*(?:- [\t ]*)?)replace:[\t ]*\{[\t ]*from:[\t ]*([^}]+)[\t ]*\}[\t ]*$)";

  const std::regex pattern(fieldPattern);
  std::smatch match;
  if (!std::regex_match(line, match, pattern)) {
    normalizedLine.clear();
    captureName.clear();
    return false;
  }

  captureName = StripOptionalQuotes(Trim(match[2].str()));
  normalizedLine = match[1].str() + targetField + ": " + captureName;
  return true;
}

static bool NormalizeReplayObjectSyntax(llvm::StringRef recipeText,
                                        std::string &normalizedText,
                                        std::string &error) {
  normalizedText.clear();
  error.clear();

  std::istringstream stream(recipeText.str());
  std::string line;
  uint32_t lineNumber = 0;
  while (std::getline(stream, line)) {
    ++lineNumber;
    std::string normalizedLine;
    std::string captureName;
    if (TryNormalizeReplayFromLine(line, "immediate_lo", "match_capture",
                                   normalizedLine, captureName)) {
      if (captureName.empty()) {
        error = "line " + std::to_string(lineNumber) +
                ": index immediate_lo replay object requires non-empty from";
        return false;
      }
      line = std::move(normalizedLine);
    } else if (TryNormalizeReplayFromLine(line, "immediate_hi",
                                          "immediate_hi", normalizedLine,
                                          captureName)) {
      error = "line " + std::to_string(lineNumber) +
              ": SM5 index immediate_hi replay object is unsupported; use "
              "literal immediate_hi";
      return false;
    } else if (TryNormalizeReplayFromLine(line, "capture", "capture",
                                          normalizedLine, captureName)) {
      if (captureName.empty()) {
        error = "line " + std::to_string(lineNumber) +
                ": capture replay object requires non-empty from";
        return false;
      }
      line = std::move(normalizedLine);
    } else if (TryNormalizeReplayFromLine(line, "match_capture",
                                          "match_capture", normalizedLine,
                                          captureName)) {
      if (captureName.empty()) {
        error = "line " + std::to_string(lineNumber) +
                ": match_capture replay object requires non-empty from";
        return false;
      }
      line = std::move(normalizedLine);
    } else if (TryNormalizeReplayFromLine(line, "replace", "replace",
                                          normalizedLine, captureName)) {
      if (captureName.empty()) {
        error = "line " + std::to_string(lineNumber) +
                ": replace replay object requires non-empty from";
        return false;
      }
      line = std::move(normalizedLine);
    }

    normalizedText += line;
    normalizedText.push_back('\n');
  }

  return true;
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

static bool ParsePrefilterMode(const std::string &value,
                               dxp::sm5::RecipePrefilterMode &mode,
                               std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered.empty() || lowered == "all") {
    mode = dxp::sm5::RecipePrefilterMode::All;
    return true;
  }
  if (lowered == "any") {
    mode = dxp::sm5::RecipePrefilterMode::Any;
    return true;
  }

  error = "unsupported SM5 prefilter mode '" + value + "'";
  return false;
}

static bool ParseRuleRewriteMode(const std::string &value,
                                 dxp::sm5::RecipeRuleRewriteMode &mode,
                                 std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered.empty()) {
    mode = dxp::sm5::RecipeRuleRewriteMode::Replace;
    return true;
  }
  if (lowered == "auto") {
    error = "SM5 rewrite mode Auto was removed; use Replace or ReplaceRange";
    return false;
  }
  if (lowered == "none") {
    mode = dxp::sm5::RecipeRuleRewriteMode::None;
    return true;
  }
  if (lowered == "replace") {
    mode = dxp::sm5::RecipeRuleRewriteMode::Replace;
    return true;
  }
  if (lowered == "before") {
    mode = dxp::sm5::RecipeRuleRewriteMode::Before;
    return true;
  }
  if (lowered == "after") {
    mode = dxp::sm5::RecipeRuleRewriteMode::After;
    return true;
  }
  if (lowered == "replacerange" || lowered == "replace_range") {
    mode = dxp::sm5::RecipeRuleRewriteMode::ReplaceRange;
    return true;
  }

  error = "unsupported SM5 rewrite mode '" + value + "'";
  return false;
}

struct YamlMatch {
  std::string opcode;
  std::string capture;
  std::string rewrite_mode;
  int32_t range_start_offset = 0;
  int32_t range_end_offset = -1;
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

struct YamlOperandIndex {
  bool any = false;
  std::string representation;
  std::string immediate_lo;
  std::string immediate_hi;
  std::string capture;
  std::string match_capture;
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
  std::vector<uint32_t> immediates_u32;
  std::vector<uint64_t> immediates_u64;
  std::vector<int32_t> immediates_i32;
  std::vector<int64_t> immediates_i64;
  std::vector<float> immediates_f32;
  std::vector<double> immediates_f64;
  std::string bind_handle;
  std::string state_temp;
  YamlComponentSelector components;
  std::string mask;
  std::string swizzle;
  std::string select;
  int32_t num_components = -1;
  std::string modifier;
  std::string capture;
  std::string match_capture;
  YamlOperandCaptureFields capture_fields;
  YamlOperandCaptureFields match_capture_fields;
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
  std::string mode;
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

struct YamlStepCondition {
  std::string state;
  std::vector<YamlStepCondition> all;
  std::vector<YamlStepCondition> any;
  bool not_condition = false;
};

struct YamlStep {
  std::string kind;
  std::string name;
  bool required = true;
  std::string mode;
  std::string set;
  YamlStepCondition if_condition;
  std::vector<YamlRule> rules;
  std::vector<YamlPrefilter> checks;

  int32_t bind_point = -1;
  std::string handle;
  std::vector<std::string> handles;
  bool auto_bind = false;
  std::string dimension = "Texture2D";
  std::string interpolation_mode = "linear";
  uint32_t elements = 1;
  std::string access_pattern = "immediateIndexed";
  std::string sampler_mode = "default";
  std::string uav_kind = "typed";
  uint32_t stride = 16;
  bool globally_coherent = false;
  bool has_counter = false;
};

struct YamlTextureDecl {
  int32_t bind_point = -1;
  std::string dimension = "Texture2D";
  std::string handle;
  bool auto_bind = false;
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
  std::vector<YamlPrefilter> prefilters;
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

LLVM_YAML_IS_SEQUENCE_VECTOR(uint32_t)
LLVM_YAML_IS_SEQUENCE_VECTOR(uint64_t)
LLVM_YAML_IS_SEQUENCE_VECTOR(int32_t)
LLVM_YAML_IS_SEQUENCE_VECTOR(int64_t)
LLVM_YAML_IS_SEQUENCE_VECTOR(float)
LLVM_YAML_IS_SEQUENCE_VECTOR(double)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlOperandIndex)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlOperand)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlInstructionMatch)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlEmitInstruction)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRule)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlStep)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlStepCondition)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlPrefilter)
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
    io.mapOptional("bind_handle", operand.bind_handle);
    io.mapOptional("state_temp", operand.state_temp);
    io.mapOptional("components", operand.components);
    io.mapOptional("mask", operand.mask);
    io.mapOptional("swizzle", operand.swizzle);
    io.mapOptional("select", operand.select);
    io.mapOptional("num_components", operand.num_components, -1);
    io.mapOptional("modifier", operand.modifier);
    io.mapOptional("capture", operand.capture);
    io.mapOptional("match_capture", operand.match_capture);
    io.mapOptional("capture_fields", operand.capture_fields);
    io.mapOptional("match_capture_fields", operand.match_capture_fields);
    io.mapOptional("scratch", operand.scratch);
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
    io.mapOptional("mode", rule.mode);
  }
};

template <> struct MappingTraits<YamlStepCondition> {
  static void mapping(IO &io, YamlStepCondition &condition) {
    io.mapOptional("state", condition.state);
    io.mapOptional("all", condition.all);
    io.mapOptional("any", condition.any);
    io.mapOptional("not", condition.not_condition, false);
  }
};

template <> struct MappingTraits<YamlStep> {
  static void mapping(IO &io, YamlStep &step) {
    io.mapOptional("kind", step.kind);
    io.mapOptional("name", step.name);
    io.mapOptional("required", step.required, true);
    io.mapOptional("mode", step.mode);
    io.mapOptional("set", step.set);
    io.mapOptional("if", step.if_condition);
    io.mapOptional("rules", step.rules);
    io.mapOptional("checks", step.checks);
    io.mapOptional("bind_point", step.bind_point, -1);
    io.mapOptional("handle", step.handle);
    io.mapOptional("handles", step.handles);
    io.mapOptional("auto_bind", step.auto_bind, false);
    io.mapOptional("dimension", step.dimension, std::string("Texture2D"));
    io.mapOptional("interpolation_mode", step.interpolation_mode,
                   std::string("linear"));
    io.mapOptional("elements", step.elements, 1u);
    io.mapOptional("access_pattern", step.access_pattern,
                   std::string("immediateIndexed"));
    io.mapOptional("sampler_mode", step.sampler_mode, std::string("default"));
    io.mapOptional("uav_kind", step.uav_kind, std::string("typed"));
    io.mapOptional("stride", step.stride, 16u);
    io.mapOptional("globally_coherent", step.globally_coherent, false);
    io.mapOptional("has_counter", step.has_counter, false);
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
    io.mapOptional("prefilters", document.prefilters);
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

static bool ParseSamplerModeToken(const std::string &value, uint32_t &mode,
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

static bool ParseUavKindToken(const std::string &value, RecipeUavKind &kind,
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
                                    PrefilterKind &kind, std::string &error) {
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

static bool ParseBoolToken(const std::string &value, bool &parsedValue,
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
                                        uint32_t &mode, std::string &error) {
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

static bool ParseOperandType(const std::string &value, OperandType &type,
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

static bool ParseOperandIndexRepresentationToken(
    const std::string &value, RecipeOperandIndexRepresentation &representation,
    std::string &error) {
  const std::string lowered = Lowercase(value);
  if (lowered.empty() || lowered == "immediate32") {
    representation = RecipeOperandIndexRepresentation::Immediate32;
    return true;
  }
  if (lowered == "immediate64") {
    representation = RecipeOperandIndexRepresentation::Immediate64;
    return true;
  }
  if (lowered == "relative") {
    representation = RecipeOperandIndexRepresentation::Relative;
    return true;
  }
  if (lowered == "immediate32_plus_relative") {
    representation = RecipeOperandIndexRepresentation::Immediate32PlusRelative;
    return true;
  }
  if (lowered == "immediate64_plus_relative") {
    representation = RecipeOperandIndexRepresentation::Immediate64PlusRelative;
    return true;
  }

  error = "unsupported SM5 operand index representation: " + value;
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
    std::vector<RecipeOperandIndexPattern> &indexPatterns) {
  indexPatterns.clear();
  indexPatterns.reserve(operandModel.immediates_u32.size() +
                        operandModel.immediates_u64.size() +
                        operandModel.immediates_i32.size() +
                        operandModel.immediates_i64.size() +
                        operandModel.immediates_f32.size() +
                        operandModel.immediates_f64.size());

  auto appendImmediate32 = [&](uint32_t immediate) {
    RecipeOperandIndexPattern indexPattern;
    indexPattern.Representation = RecipeOperandIndexRepresentation::Immediate32;
    indexPattern.HasImmediateLo = true;
    indexPattern.ImmediateLo = immediate;
    indexPatterns.push_back(std::move(indexPattern));
  };

  auto appendImmediate64 = [&](uint64_t immediate) {
    RecipeOperandIndexPattern indexPattern;
    indexPattern.Representation = RecipeOperandIndexRepresentation::Immediate64;
    indexPattern.HasImmediateLo = true;
    indexPattern.ImmediateLo = static_cast<uint32_t>(immediate & 0xFFFFFFFFull);
    indexPattern.HasImmediateHi = true;
    indexPattern.ImmediateHi = static_cast<uint32_t>(immediate >> 32);
    indexPatterns.push_back(std::move(indexPattern));
  };

  for (uint32_t value : operandModel.immediates_u32) {
    appendImmediate32(value);
  }

  for (uint64_t immediate : operandModel.immediates_u64) {
    appendImmediate64(immediate);
  }

  for (int32_t value : operandModel.immediates_i32) {
    appendImmediate32(BitCastImmediate<uint32_t>(value));
  }

  for (int64_t value : operandModel.immediates_i64) {
    appendImmediate64(BitCastImmediate<uint64_t>(value));
  }

  for (float value : operandModel.immediates_f32) {
    appendImmediate32(BitCastImmediate<uint32_t>(value));
  }

  for (double value : operandModel.immediates_f64) {
    appendImmediate64(BitCastImmediate<uint64_t>(value));
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
    return DecodeEmitImmediateShorthands(operandModel, indexPatterns);
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

    if (!ParseOperandIndexRepresentationToken(yamlIndex.representation,
                                              indexPattern.Representation,
                                              error)) {
      return false;
    }

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

  if (!rule.Replace.empty()) {
    if (!ValidateCaptureReference(captures, rule.Replace, "instruction",
                                  captures.Instructions, "replace capture",
                                  error)) {
      return false;
    }
  }

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
  const bool hasLegacySelectors = !operandModel.select.empty() ||
                                  !operandModel.mask.empty() ||
                                  !operandModel.swizzle.empty();

  if (hasLegacySelectors) {
    error = "SM5 operands require components.kind/components.value instead of "
            "mask/swizzle/select";
    return false;
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

  if (!operandModel.scratch.empty()) {
      error = "SM5 scratch is unsupported; use add_temp and bind_handle on "
        "type: temp operands";
    return false;
  }

  if (!operandModel.state_temp.empty()) {
    error = "SM5 state_temp is unsupported; use callback-driven emit logic";
    return false;
  }

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

  if (!operandModel.bind_handle.empty() && !operandModel.type.empty()) {
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

  operand.BindHandle = operandModel.bind_handle;

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
  if (!operandModel.mask.empty() || !operandModel.swizzle.empty() ||
      !operandModel.components.kind.empty() ||
      !operandModel.components.value.empty() || !operandModel.select.empty() ||
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
  if (!operandModel.scratch.empty()) {
      error = "SM5 scratch is unsupported; use add_temp and bind_handle on "
        "type: temp operands";
    return false;
  }

  if (!operandModel.state_temp.empty()) {
    error = "SM5 state_temp is unsupported; use callback-driven emit logic";
    return false;
  }

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
                       .WithBindHandle(operandModel.bind_handle)
                       .WithNumComponents(operandModel.num_components)
                       .WithModifier(operandModel.modifier)
                         .WithCaptureFields(
                           BuildCaptureFields(operandModel.capture_fields))
                       .WithMatchCapture(operandModel.match_capture)
                         .WithMatchCaptureFields(BuildCaptureFields(
                           operandModel.match_capture_fields))
                       .CaptureAs(operandModel.capture);

  if (!operandModel.mask.empty() || !operandModel.swizzle.empty() ||
      !operandModel.select.empty()) {
    error = "SM5 operands require components.kind/components.value instead of "
            "mask/swizzle/select";
    return false;
  }

  if (!operandModel.components.kind.empty() ||
      !operandModel.components.value.empty()) {
    const std::string kind = Lowercase(operandModel.components.kind);
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
                .WithSaturate(matchModel.saturate)
                .WithInterpolationMode(matchModel.interpolation_mode)
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
  if (!matchModel.saturate.empty()) {
    bool saturate = false;
    if (!ParseBoolToken(matchModel.saturate, saturate, error)) {
      error = "invalid SM5 saturate value: " + error;
      return false;
    }
    instructionMatch.HasSaturateMatch = true;
    instructionMatch.SaturateValue = saturate;
  }
    if (resolvedTestBoolean >= 0) {
    instructionMatch.HasTestBooleanMatch = true;
    instructionMatch.MatchTestBoolean =
      static_cast<uint32_t>(resolvedTestBoolean);
  }
  if (!matchModel.interpolation_mode.empty()) {
    uint32_t interpolationMode = 0;
    if (!ParseInterpolationModeToken(matchModel.interpolation_mode,
                                     interpolationMode, error)) {
      return false;
    }
    if (instructionMatch.Opcode != Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS} &&
        instructionMatch.Opcode != Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS_SIV}) {
      error = "SM5 interpolation_mode is only valid for dcl_input_ps and "
              "dcl_input_ps_siv";
      return false;
    }
    instructionMatch.HasInputInterpolationModeMatch = true;
    instructionMatch.MatchInputInterpolationMode = interpolationMode;
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
  rule = RecipeRule{}.ApplyMode(inheritedMode);

  if (!ruleModel.mode.empty()) {
    rule.ApplyMode(ParseRuleApplicationMode(ruleModel.mode));
  }

  RecipeRuleRewriteMode rewriteMode = RecipeRuleRewriteMode::Replace;
  if (!ParseRuleRewriteMode(ruleModel.match.rewrite_mode, rewriteMode, error)) {
    return false;
  }
  rule.RewriteAs(rewriteMode);
  rule.RangeOffsets(ruleModel.match.range_start_offset,
                    ruleModel.match.range_end_offset);

  const bool hasCustomRangeOffsets =
      ruleModel.match.range_start_offset != 0 ||
      ruleModel.match.range_end_offset != -1;
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
        "SM5 range offsets require match.rewrite_mode: ReplaceRange";
    return false;
  }

  if (!ruleModel.match.sequence.empty()) {
    if (!ruleModel.match.opcode.empty() || !ruleModel.match.capture.empty() ||
        !ruleModel.match.saturate.empty() ||
        !ruleModel.match.interpolation_mode.empty() ||
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
                                   .WithSaturate(ruleModel.match.saturate)
                                   .WithInterpolationMode(
                                       ruleModel.match.interpolation_mode)
                                   .WithTestBoolean(resolvedTestBoolean);
    if (!ruleModel.match.saturate.empty()) {
      bool saturate = false;
      if (!ParseBoolToken(ruleModel.match.saturate, saturate, error)) {
        error = "invalid SM5 saturate value: " + error;
        return false;
      }
    }
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

  rule.ReplaceCapture(ruleModel.replace);

  if ((rule.RewriteMode == RecipeRuleRewriteMode::Replace ||
       rule.RewriteMode == RecipeRuleRewriteMode::ReplaceRange) &&
      !rule.Replace.empty()) {
    error = "SM5 replace capture is only valid with Before or After rewrite "
            "modes";
    return false;
  }

  if (rule.RewriteMode == RecipeRuleRewriteMode::None) {
    if (!rule.Replace.empty() || !ruleModel.emit.empty() ||
        hasCustomRangeOffsets) {
      error = "SM5 rewrite mode None cannot be combined with replace, emit, "
              "or range offsets";
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
    if (!emitModel.saturate.empty()) {
      bool saturate = false;
      if (!ParseBoolToken(emitModel.saturate, saturate, error)) {
        error = "invalid SM5 emit saturate value: " + error;
        return false;
      }
      instruction.Controls.Saturate = saturate;
    }
        if (resolvedTestBoolean >= 0) {
      instruction.Controls.HasTestBoolean = true;
      instruction.Controls.TestBoolean =
          static_cast<uint32_t>(resolvedTestBoolean);
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
        error = "SM5 interpolation_mode is only valid for dcl_input_ps and "
                "dcl_input_ps_siv";
        return false;
      }

      instruction.Controls.HasInputInterpolationMode = true;
      instruction.Controls.InputInterpolationMode = interpolationMode;
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
            .WithSaturate(emitModel.saturate)
            .WithInterpolationMode(emitModel.interpolation_mode)
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

static bool IsEmptyYamlMatch(const YamlMatch &match) {
  return match.opcode.empty() && match.capture.empty() &&
         match.saturate.empty() && match.interpolation_mode.empty() &&
         match.test_boolean < 0 && match.operands.empty() &&
         match.sequence.empty();
}

static bool ValidatePrefilterModel(const YamlPrefilter &prefilter,
                                   PrefilterKind kind, std::string &error) {
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
      error = "SM5 check_pattern_match prefilter requires match.opcode or "
              "match.sequence";
      return false;
    }
    if (!prefilter.match.sequence.empty() &&
        (!prefilter.match.opcode.empty() || !prefilter.match.capture.empty() ||
         !prefilter.match.saturate.empty() ||
         !prefilter.match.interpolation_mode.empty() ||
         prefilter.match.test_boolean >= 0 ||
         !prefilter.match.operands.empty())) {
      error = "SM5 prefilter match.sequence cannot be combined with "
              "single-instruction match fields";
      return false;
    }
    if (prefilter.match.sequence.empty() && prefilter.match.opcode.empty()) {
      error = "SM5 check_pattern_match prefilter requires match.opcode when "
              "match.sequence is omitted";
      return false;
    }
    return true;
  }

  return false;
}

static bool BuildRecipePrefilter(const YamlPrefilter &prefilterModel,
                                 RecipePrefilter &prefilter,
                                 std::string &parseError) {
  PrefilterKind kind = PrefilterKind::CheckShaderVersion;
  if (!ParsePrefilterKindToken(prefilterModel.kind, kind, parseError)) {
    return false;
  }
  if (!ValidatePrefilterModel(prefilterModel, kind, parseError)) {
    return false;
  }

  prefilter = RecipePrefilter{}
                  .Named(prefilterModel.name)
                  .Require(prefilterModel.required);
  prefilter.Kind = kind;

  std::string canonicalOpcodeName = prefilterModel.match.opcode;
  int32_t resolvedTestBoolean = prefilterModel.match.test_boolean;
  if (!prefilterModel.match.opcode.empty()) {
    Opcode parsedOpcode;
    if (!ResolveOpcodeAndTestBoolean(prefilterModel.match.opcode,
                                     prefilterModel.match.test_boolean,
                                     parsedOpcode, canonicalOpcodeName,
                                     resolvedTestBoolean, parseError,
                                     "prefilter")) {
      return false;
    }
  }

  RecipeMatchPattern match = RecipeMatchPattern{}
                                 .WithOpcode(canonicalOpcodeName)
                                 .CaptureAs(prefilterModel.match.capture)
                                 .WithSaturate(prefilterModel.match.saturate)
                                 .WithInterpolationMode(
                                     prefilterModel.match.interpolation_mode)
                                 .WithTestBoolean(resolvedTestBoolean);

  for (const YamlOperand &operandModel : prefilterModel.match.operands) {
    RecipeOperandPattern operand;
    if (!FillRecipeOperandPattern(operandModel, operand, false, parseError)) {
      return false;
    }
    match.AddOperand(std::move(operand));
  }

  for (const YamlInstructionMatch &matchModel : prefilterModel.match.sequence) {
    RecipeInstructionPattern pattern;
    if (!BuildRecipeInstructionPattern(matchModel, pattern, parseError)) {
      return false;
    }
    match.AddInstruction(std::move(pattern));
  }

  switch (kind) {
  case PrefilterKind::CheckShaderVersion:
    prefilter.CheckShaderVersion(prefilterModel.major, prefilterModel.minor);
    break;
  case PrefilterKind::CheckOpcodeCount: {
    Opcode parsedOpcode;
    if (!ParseOpcode(prefilterModel.opcode, parsedOpcode)) {
      parseError = "Unknown SM5 opcode in prefilter: " + prefilterModel.opcode;
      return false;
    }
    prefilter.CheckOpcodeCount(prefilterModel.opcode,
                               prefilterModel.expected_count);
    break;
  }
  case PrefilterKind::CheckResourceCount:
    prefilter.CheckResourceCount(prefilterModel.expected_resources);
    break;
  case PrefilterKind::CheckPatternMatch:
    prefilter.CheckPatternMatch(std::move(match));
    break;
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
        step.kind.empty() ? "apply_rules" : Lowercase(step.kind);
    if (stepKind == "add_temp") {
      if (!step.handle.empty() && !step.handles.empty()) {
        error = "add_temp steps cannot combine handle and handles";
        return false;
      }

      if (step.handle.empty() && step.handles.empty()) {
        error = "add_temp steps require handle or handles";
        return false;
      }

      if (!step.handle.empty()) {
        if (!insertHandle(tempHandles, step.handle, "temp")) {
          return false;
        }
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
      error =
          "SM5 bind_handle references unknown resource declaration handle '" +
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
      error =
          "SM5 bind_handle references unknown cbuffer declaration handle '" +
          operand.bind_handle + "'";
      return false;
    }
    return true;
  case D3D10_SB_OPERAND_TYPE_SAMPLER:
    if (samplerHandles.find(operand.bind_handle) == samplerHandles.end()) {
      error =
          "SM5 bind_handle references unknown sampler declaration handle '" +
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

  size_t populatedFields = 0;
  if (!conditionModel.state.empty()) {
    ++populatedFields;
  }
  if (!conditionModel.all.empty()) {
    ++populatedFields;
  }
  if (!conditionModel.any.empty()) {
    ++populatedFields;
  }

  if (populatedFields == 0) {
    return true;
  }

  if (populatedFields != 1) {
    error = "SM5 step if must specify exactly one of state, all, or any";
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

  std::vector<RecipeStepCondition> &destination =
      !conditionModel.all.empty() ? condition.All : condition.Any;
  const std::vector<YamlStepCondition> &source =
      !conditionModel.all.empty() ? conditionModel.all : conditionModel.any;
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

static bool ValidatePortableDeclarationModel(const YamlRecipeDocument &document,
                                             std::string &error) {
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

  for (const YamlStructuredResourceDecl &decl :
       document.structured_resource_decls) {
    if (decl.handle.empty()) {
      error = "portable schema structured_resource_decls require handle";
      return false;
    }
    if (!decl.auto_bind) {
      error =
          "portable schema structured_resource_decls require auto_bind: true";
      return false;
    }
    if (decl.bind_point >= 0) {
      error =
          "portable schema structured_resource_decls do not allow bind_point";
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

bool ParseRecipeText(llvm::StringRef recipeText, RecipeParseResult &result,
                     llvm::StringRef sourceName) {
  result = RecipeParseResult{};

  std::string normalizedRecipeText;
  std::string normalizationError;
  if (!NormalizeReplayObjectSyntax(recipeText, normalizedRecipeText,
                                   normalizationError)) {
    result.Error = sourceName.str() + ": " + normalizationError;
    return false;
  }

  YamlRecipeDocument document;
  llvm::yaml::Input input(normalizedRecipeText);
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

  if (!document.rewrite_rules.empty()) {
    result.Error = sourceName.str() + ": schema version 1 requires steps and "
                                      "does not allow top-level rewrite_rules";
    return false;
  }
  if (!document.prefilters.empty()) {
    result.Error = sourceName.str() +
                   ": top-level prefilters are deprecated and unsupported in "
                   "schema version 1; use prefilter steps";
    return false;
  }
  if (document.steps.empty()) {
    result.Error =
        sourceName.str() + ": schema version 1 requires at least one step";
    return false;
  }
  if (!document.input_decls.empty() || !document.output_decls.empty() ||
      !document.texture_decls.empty() || !document.raw_resource_decls.empty() ||
      !document.structured_resource_decls.empty() ||
      !document.cbuffer_decls.empty() || !document.sampler_decls.empty() ||
      !document.uav_decls.empty()) {
    result.Error = sourceName.str() +
                   ": schema version 1 does not allow top-level "
                   "*_decls; use add_* declaration steps";
    return false;
  }

  if (!ValidateUniqueDeclarationHandles(document, parseError)) {
    result.Error = sourceName.str() + ": " + parseError;
    return false;
  }

  if (!ValidateEmitHandleReferences(document, parseError)) {
    result.Error = sourceName.str() + ": " + parseError;
    return false;
  }

  auto appendRule = [&](const YamlRule &ruleModel,
                        RecipeRuleApplicationMode inheritedMode,
                        std::vector<RecipeRule> &rules) -> bool {
    RecipeRule rule;
    if (!ParseRule(ruleModel, inheritedMode, rule, parseError)) {
      result.Error = sourceName.str() + ": " + parseError;
      return false;
    }
    rules.push_back(std::move(rule));
    return true;
  };

  for (const YamlStep &stepModel : document.steps) {
    const std::string stepKind =
        stepModel.kind.empty() ? "apply_rules" : Lowercase(stepModel.kind);
    const std::string stepName =
        stepModel.name.empty() ? stepKind : stepModel.name;
    RecipeStepCondition stepCondition;
    if (!BuildStepCondition(stepModel.if_condition, stepCondition, parseError)) {
      result.Error = sourceName.str() + ": " + parseError;
      return false;
    }

    if (stepKind == "apply_rules") {
      RecipeRuleApplicationMode applicationMode = RecipeRuleApplicationMode::First;
      if (!stepModel.mode.empty()) {
        applicationMode = ParseRuleApplicationMode(stepModel.mode);
      }

      std::vector<RecipeRule> rules;
      rules.reserve(stepModel.rules.size());

      for (const YamlRule &ruleModel : stepModel.rules) {
        if (!appendRule(ruleModel, applicationMode, rules)) {
          return false;
        }
      }

      result.Recipe.AddStep(MakeRewriteRulesStep(stepName, std::move(rules),
                                                applicationMode,
                                                stepModel.required)
                                .When(stepCondition));
      continue;
    }

    if (stepKind == "prefilter") {
      if (!stepModel.rules.empty()) {
        result.Error = sourceName.str() +
                       ": SM5 prefilter steps cannot define rules";
        return false;
      }
      if (stepModel.checks.empty()) {
        result.Error = sourceName.str() +
                       ": SM5 prefilter steps require at least one check";
        return false;
      }

      RecipePrefilterMode prefilterMode = RecipePrefilterMode::All;
      if (!ParsePrefilterMode(stepModel.mode, prefilterMode, parseError)) {
        result.Error = sourceName.str() + ": " + parseError;
        return false;
      }

      std::vector<RecipePrefilter> checks;
      checks.reserve(stepModel.checks.size());
      for (const YamlPrefilter &checkModel : stepModel.checks) {
        RecipePrefilter prefilter;
        if (!BuildRecipePrefilter(checkModel, prefilter, parseError)) {
          result.Error = sourceName.str() + ": " + parseError;
          return false;
        }
        checks.push_back(std::move(prefilter));
      }

        result.Recipe.AddStep(
            MakePrefilterStep(stepName, std::move(checks), stepModel.set,
                  prefilterMode)
                .Require(stepModel.required)
                .When(stepCondition));
      continue;
    }

    if (!stepModel.mode.empty()) {
      result.Error = sourceName.str() +
                     ": SM5 step mode is only valid for apply_rules or "
                     "prefilter steps";
      return false;
    }

    if (!stepModel.checks.empty()) {
      result.Error = sourceName.str() +
                     ": SM5 step checks are only valid for prefilter steps";
      return false;
    }

    if (!stepModel.set.empty()) {
      result.Error = sourceName.str() +
                     ": SM5 step set is only valid for prefilter steps";
      return false;
    }

    if (!stepModel.rules.empty()) {
      result.Error =
          sourceName.str() + ": SM5 non-apply_rules steps cannot define rules";
      return false;
    }

    if (stepKind == "refresh_resources") {
      result.Recipe.AddStep(MakeRefreshResourcesStep(stepName)
                                .Require(stepModel.required)
                                .When(stepCondition));
    } else if (stepKind == "verify_program") {
      result.Recipe.AddStep(MakeVerifyProgramStep(stepName)
                                .Require(stepModel.required)
                                .When(stepCondition));
    } else if (stepKind == "add_input") {
      RecipeInputDecl decl = RecipeInputDecl{}
                                .WithBindPoint(stepModel.bind_point >= 0
                                                   ? static_cast<uint32_t>(
                                                         stepModel.bind_point)
                                                   : 0u)
                                .WithHandle(stepModel.handle)
                                .AutoBindToNext(stepModel.auto_bind);
      if (!ParseInterpolationModeToken(stepModel.interpolation_mode,
                                       decl.InterpolationMode, parseError)) {
        result.Error = sourceName.str() + ": " + parseError;
        return false;
      }
      decl.WithInterpolationMode(decl.InterpolationMode);
      result.Recipe.AddStep(MakeAddInputStep(stepName, std::move(decl))
                                .Require(stepModel.required)
                                .When(stepCondition));
    } else if (stepKind == "add_temp") {
      if (!stepModel.handle.empty() && !stepModel.handles.empty()) {
        result.Error = sourceName.str() +
                       ": add_temp steps cannot combine handle and handles";
        return false;
      }

      if (stepModel.handle.empty() && stepModel.handles.empty()) {
        result.Error = sourceName.str() +
                       ": add_temp steps require handle or handles";
        return false;
      }
      if (stepModel.bind_point >= 0) {
        result.Error = sourceName.str() +
                       ": add_temp steps do not allow bind_point";
        return false;
      }
      if (stepModel.auto_bind) {
        result.Error = sourceName.str() +
                       ": add_temp steps do not allow auto_bind";
        return false;
      }

      std::vector<std::string> tempHandles;
      if (!stepModel.handle.empty()) {
        tempHandles.push_back(stepModel.handle);
      }
      for (const std::string &tempHandle : stepModel.handles) {
        if (tempHandle.empty()) {
          result.Error = sourceName.str() +
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
        result.Recipe.AddStep(MakeAddTempStep(tempStepName, std::move(decl))
                                  .Require(stepModel.required)
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
                                .Require(stepModel.required)
                                .When(stepCondition));
    } else if (stepKind == "add_texture") {
      RecipeTextureDecl decl = RecipeTextureDecl{}
                                  .WithBindPoint(stepModel.bind_point >= 0
                                                     ? static_cast<uint32_t>(
                                                           stepModel.bind_point)
                                                     : 0u)
                                  .WithHandle(stepModel.handle)
                                  .AutoBindToNext(stepModel.auto_bind);
      if (!ParseTextureDimensionToken(stepModel.dimension, decl.Dimension,
                                      parseError)) {
        result.Error = sourceName.str() + ": " + parseError;
        return false;
      }
      decl.WithDimension(decl.Dimension);
      result.Recipe.AddStep(MakeAddTextureStep(stepName, std::move(decl))
                                .Require(stepModel.required)
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
                                .Require(stepModel.required)
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
              .Require(stepModel.required)
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
      if (!ParseCBufferAccessPatternToken(stepModel.access_pattern,
                                          decl.AccessPattern, parseError)) {
        result.Error = sourceName.str() + ": " + parseError;
        return false;
      }
      decl.WithAccessPattern(decl.AccessPattern);
      result.Recipe.AddStep(MakeAddCBufferStep(stepName, std::move(decl))
                                .Require(stepModel.required)
                                .When(stepCondition));
    } else if (stepKind == "add_sampler") {
      RecipeSamplerDecl decl = RecipeSamplerDecl{}
                                  .WithBindPoint(stepModel.bind_point >= 0
                                                     ? static_cast<uint32_t>(
                                                           stepModel.bind_point)
                                                     : 0u)
                                  .WithHandle(stepModel.handle)
                                  .AutoBindToNext(stepModel.auto_bind);
      if (!ParseSamplerModeToken(stepModel.sampler_mode, decl.Mode,
                                 parseError)) {
        result.Error = sourceName.str() + ": " + parseError;
        return false;
      }
      decl.WithMode(decl.Mode);
      result.Recipe.AddStep(MakeAddSamplerStep(stepName, std::move(decl))
                                .Require(stepModel.required)
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
                              .WithOrderPreservingCounter(stepModel.has_counter);
      if (!ParseUavKindToken(stepModel.uav_kind, decl.Kind, parseError)) {
        result.Error = sourceName.str() + ": " + parseError;
        return false;
      }
      decl.WithKind(decl.Kind);
      if (!ParseTextureDimensionToken(stepModel.dimension, decl.Dimension,
                                      parseError)) {
        result.Error = sourceName.str() + ": " + parseError;
        return false;
      }
      decl.WithDimension(decl.Dimension);
      result.Recipe.AddStep(MakeAddUavStep(stepName, std::move(decl))
                                .Require(stepModel.required)
                                .When(stepCondition));
    } else {
      result.Error = sourceName.str() + ": unsupported SM5 step kind '" +
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