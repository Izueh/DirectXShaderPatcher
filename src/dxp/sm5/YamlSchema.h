#pragma once

#include "dxp/sm5/Recipe.h"
#include "Model.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "glaze/yaml.hpp"

// ---------------------------------------------------------------------------
// YAML schema structs — mirror the declarative recipe format
// ---------------------------------------------------------------------------

namespace dxp::sm5 {

struct YamlComponentSelector {
  std::string kind;
  std::string value;
};

struct YamlOperandIndex {
  bool any = false;
  RecipeOperandIndexRepresentation representation = RecipeOperandIndexRepresentation::Immediate32;
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

struct YamlInstructionMatch {
  std::string opcode;
  std::string capture;
  bool saturate = false;
  InterpolationMode interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<YamlOperand> operands;
};

struct YamlMatch {
  std::string opcode;
  std::string capture;
  RecipeRuleRewriteMode rewrite_mode = RecipeRuleRewriteMode::Replace;
  int32_t range_start_offset = 0;
  int32_t range_end_offset = -1;
  int32_t insert_relative_index = -1;
  bool saturate = false;
  InterpolationMode interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<YamlOperand> operands;
  std::vector<YamlInstructionMatch> sequence;
};

struct YamlEmitInstruction {
  std::string opcode;
  bool saturate = false;
  InterpolationMode interpolation_mode;
  int32_t test_boolean = -1;
  std::vector<YamlOperand> operands;
};

struct YamlRule {
  std::string name;
  YamlMatch match;
  std::string replace;
  std::vector<YamlEmitInstruction> emit;
  RecipeRuleApplicationMode mode = RecipeRuleApplicationMode::First;
  bool required_match = false;
  /// @brief When true, declarations are refreshed after this rule applies.
  bool refresh_declarations = false;
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
  RecipeRuleApplicationMode mode = RecipeRuleApplicationMode::First;
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
  ResourceDimension dimension = ResourceDimension::Texture2D;
  InterpolationMode interpolation_mode;
  uint32_t elements = 1;
  CbufferAccessPattern access_pattern = CbufferAccessPattern::ImmediateIndexed;
  SamplerMode sampler_mode = SamplerMode::Default;
  RecipeUavKind uav_kind = RecipeUavKind::Typed;
  uint32_t stride = 16;
  bool globally_coherent = false;
  bool has_counter = false;
};

struct YamlTextureDecl {
  int32_t bind_point = -1;
  ResourceDimension dimension = ResourceDimension::Texture2D;
  std::string handle;
  bool auto_bind = false;
};

struct YamlInputDecl {
  int32_t bind_point = -1;
  InterpolationMode interpolation_mode;
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
  CbufferAccessPattern access_pattern = CbufferAccessPattern::ImmediateIndexed;
  std::string handle;
  bool auto_bind = false;
};

struct YamlSamplerDecl {
  int32_t bind_point = -1;
  SamplerMode mode = SamplerMode::Default;
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
  RecipeUavKind kind = RecipeUavKind::Typed;
  ResourceDimension dimension = ResourceDimension::Texture2D;
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

} // namespace dxp::sm5

// ---------------------------------------------------------------------------
// glaze custom deserialization for YamlImmediateScalar
// Reads a plain YAML scalar (string or number) and stores it in .value
// Uses glz::custom wrapper for proper sequence element handling
// ---------------------------------------------------------------------------

namespace glz {

template <>
struct meta<dxp::sm5::YamlImmediateScalar> {
  using T = dxp::sm5::YamlImmediateScalar;
  static constexpr auto value = glz::custom<
    [](T& val, std::string sv) { val.value = std::move(sv); },
    [](const T& val) -> const std::string& { return val.value; }
  >;
};

} // namespace glz

// ---------------------------------------------------------------------------
// glaze::meta for SM5 enums — snake_case YAML keys → enum values
// ---------------------------------------------------------------------------

namespace glz {

template <>
struct meta<dxp::sm5::RecipeRuleApplicationMode> {
  using T = dxp::sm5::RecipeRuleApplicationMode;
  static constexpr std::array keys{"first", "last", "match_all"};
  static constexpr std::array value{
    dxp::sm5::RecipeRuleApplicationMode::First,
    dxp::sm5::RecipeRuleApplicationMode::Last,
    dxp::sm5::RecipeRuleApplicationMode::MatchAll
  };
};

template <>
struct meta<dxp::sm5::RecipeRuleRewriteMode> {
  using T = dxp::sm5::RecipeRuleRewriteMode;
  static constexpr std::array keys{"none", "replace", "before", "after", "replace_range"};
  static constexpr std::array value{
    dxp::sm5::RecipeRuleRewriteMode::None,
    dxp::sm5::RecipeRuleRewriteMode::Replace,
    dxp::sm5::RecipeRuleRewriteMode::Before,
    dxp::sm5::RecipeRuleRewriteMode::After,
    dxp::sm5::RecipeRuleRewriteMode::ReplaceRange
  };
};

template <>
struct meta<dxp::sm5::RecipeUavKind> {
  using T = dxp::sm5::RecipeUavKind;
  static constexpr std::array keys{"typed", "raw", "structured"};
  static constexpr std::array value{
    dxp::sm5::RecipeUavKind::Typed,
    dxp::sm5::RecipeUavKind::Raw,
    dxp::sm5::RecipeUavKind::Structured
  };
};

template <>
struct meta<dxp::sm5::RecipeOperandIndexRepresentation> {
  using T = dxp::sm5::RecipeOperandIndexRepresentation;
  static constexpr std::array keys{"immediate32", "immediate64", "relative", "immediate32_plus_relative", "immediate64_plus_relative"};
  static constexpr std::array value{
    dxp::sm5::RecipeOperandIndexRepresentation::Immediate32,
    dxp::sm5::RecipeOperandIndexRepresentation::Immediate64,
    dxp::sm5::RecipeOperandIndexRepresentation::Relative,
    dxp::sm5::RecipeOperandIndexRepresentation::Immediate32PlusRelative,
    dxp::sm5::RecipeOperandIndexRepresentation::Immediate64PlusRelative
  };
};

template <>
struct meta<dxp::sm5::InterpolationMode> {
  using T = dxp::sm5::InterpolationMode;
  static constexpr std::array keys{
    "undefined", "constant", "linear", "linear_centroid",
    "linear_noperspective", "linear_noperspective_centroid",
    "linear_sample", "linear_noperspective_sample"
  };
  static constexpr std::array value{
    dxp::sm5::InterpolationMode::Undefined,
    dxp::sm5::InterpolationMode::Constant,
    dxp::sm5::InterpolationMode::Linear,
    dxp::sm5::InterpolationMode::LinearCentroid,
    dxp::sm5::InterpolationMode::LinearNoperspective,
    dxp::sm5::InterpolationMode::LinearNoperspectiveCentroid,
    dxp::sm5::InterpolationMode::LinearSample,
    dxp::sm5::InterpolationMode::LinearNoperspectiveSample
  };
};

template <>
struct meta<dxp::sm5::ResourceDimension> {
  using T = dxp::sm5::ResourceDimension;
  static constexpr std::array keys{
    "texture_1d", "texture_2d", "texture_2dms", "texture_cube",
    "texture_3d", "texture_2d_array", "texture_2dms_array", "texture_cube_array"
  };
  static constexpr std::array value{
    dxp::sm5::ResourceDimension::Texture1D,
    dxp::sm5::ResourceDimension::Texture2D,
    dxp::sm5::ResourceDimension::Texture2DMS,
    dxp::sm5::ResourceDimension::TextureCube,
    dxp::sm5::ResourceDimension::Texture3D,
    dxp::sm5::ResourceDimension::Texture2DArray,
    dxp::sm5::ResourceDimension::Texture2DMSArray,
    dxp::sm5::ResourceDimension::TextureCubeArray
  };
};

template <>
struct meta<dxp::sm5::CbufferAccessPattern> {
  using T = dxp::sm5::CbufferAccessPattern;
  static constexpr std::array keys{"immediate_indexed", "dynamic_indexed"};
  static constexpr std::array value{
    dxp::sm5::CbufferAccessPattern::ImmediateIndexed,
    dxp::sm5::CbufferAccessPattern::DynamicIndexed
  };
};

template <>
struct meta<dxp::sm5::SamplerMode> {
  using T = dxp::sm5::SamplerMode;
  static constexpr std::array keys{"default", "comparison", "mono"};
  static constexpr std::array value{
    dxp::sm5::SamplerMode::Default,
    dxp::sm5::SamplerMode::Comparison,
    dxp::sm5::SamplerMode::Mono
  };
};

// ---------------------------------------------------------------------------
// glz::meta for structs with non-matching YAML keys
// ---------------------------------------------------------------------------

template <>
struct meta<dxp::sm5::YamlStepCondition> {
  using T = dxp::sm5::YamlStepCondition;
  static constexpr auto value = object(
    "state", &T::state,
    "input", &T::input,
    "and", &T::and_conditions,
    "or", &T::or_conditions,
    "eq", &T::eq,
    "ne", &T::ne,
    "gt", &T::gt,
    "gte", &T::gte,
    "lt", &T::lt,
    "lte", &T::lte,
    "not", &T::not_condition
  );
};

template <>
struct meta<dxp::sm5::YamlStep> {
  using T = dxp::sm5::YamlStep;
  static constexpr auto value = object(
    "kind", &T::kind,
    "name", &T::name,
    "abort_on_failure", &T::abort_on_failure,
    "mode", &T::mode,
    "if", &T::if_condition,
    "rules", &T::rules,
    "major", &T::major,
    "minor", &T::minor,
    "opcode", &T::opcode,
    "expected_count", &T::expected_count,
    "expected_resources", &T::expected_resources,
    "bind_point", &T::bind_point,
    "handle", &T::handle,
    "handles", &T::handles,
    "auto_bind", &T::auto_bind,
    "dimension", &T::dimension,
    "interpolation_mode", &T::interpolation_mode,
    "elements", &T::elements,
    "access_pattern", &T::access_pattern,
    "sampler_mode", &T::sampler_mode,
    "uav_kind", &T::uav_kind,
    "stride", &T::stride,
    "globally_coherent", &T::globally_coherent,
    "has_counter", &T::has_counter
  );
};

} // namespace glz
