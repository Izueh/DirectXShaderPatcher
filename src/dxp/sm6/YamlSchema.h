#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glaze/yaml.hpp>

namespace dxp::sm6 {

struct YamlRecipeBindingModel {
  std::string bind = "auto";
  unsigned space = 0;
};

struct YamlRecipeTextureModel {
  std::string id;
  std::string name;
  std::string kind;
  std::string element;
  unsigned width = 0;
  YamlRecipeBindingModel binding;
};

struct YamlRecipeFieldModel {
  std::string name;
  std::string type;
  unsigned width = 0;
  unsigned offset = 0;
};

struct YamlRecipeCBufferModel {
  std::string id;
  std::string name;
  std::string type;
  unsigned size = 0;
  YamlRecipeBindingModel binding;
  std::vector<YamlRecipeFieldModel> fields;
};

struct YamlRecipeSamplerModel {
  std::string id;
  std::string name;
  YamlRecipeBindingModel binding;
};

struct YamlRecipeResourcesModel {
  std::vector<YamlRecipeTextureModel> textures;
  std::vector<YamlRecipeTextureModel> texture_uavs;
  std::vector<YamlRecipeCBufferModel> cbuffers;
  std::vector<YamlRecipeSamplerModel> samplers;
};

struct YamlRecipeOperandModel {
  unsigned index = 0;
  std::string kind;
  std::string capture;
  unsigned value = 0;
  std::string opcode;
  std::string resource_class;
  std::string resource_kind;
  std::string resource_name;
  std::string resource_name_like;
  int bind = -1;
  int space = -1;
  std::vector<YamlRecipeOperandModel> operands;
};

struct YamlRecipeBindingPatternModel {
  std::string kind;
  std::string capture;
  std::string opcode;
  std::vector<YamlRecipeOperandModel> operands;
};

struct YamlRecipeEmitOperandModel {
  unsigned index = 0;
  std::string kind;
  std::string capture;
  std::string id;
  unsigned value = 0;
};

struct YamlRecipeEmitModel {
  std::string kind;
  std::string id;
  std::string resource;
  std::string handle;
  std::string opcode;
  std::string type;
  std::string aggregate;
  unsigned index = 0;
  std::vector<YamlRecipeEmitOperandModel> operands;
};

struct YamlRecipeMatchModel {
  std::string opcode;
  std::string capture;
  std::string replace;
  std::string mode = "Replace";
  int32_t range_start_offset = 0;
  int32_t range_end_offset = -1;
  bool prune_dead = true;
  std::vector<std::string> prune_captures;
  std::vector<YamlRecipeOperandModel> operands;
};

struct YamlRecipePrefilterModel {
  std::string id;
  std::string name;
  std::string opcode;
  std::string capture;
  std::vector<YamlRecipeOperandModel> operands;
};

struct YamlRecipeRuleModel {
  std::string id;
  std::string name;
  YamlRecipeMatchModel match;
  std::vector<YamlRecipeBindingPatternModel> bindings;
  std::vector<YamlRecipeEmitModel> emit;
  std::string replace_with;
  std::string replace_with_capture;
};

struct YamlRecipeStepConditionModel {
  std::string state;
  std::vector<YamlRecipeStepConditionModel> all;
  std::vector<YamlRecipeStepConditionModel> any;
  bool not_condition = false;
};

struct YamlRecipeStepModel {
  std::string kind;
  std::string id;
  std::string pattern;
  std::vector<std::string> patterns;
  std::string rule;
  std::vector<std::string> rules;
  std::string name;
  std::string set;
  std::string mode;
  bool required = true;
  YamlRecipeStepConditionModel if_condition;
};

struct YamlRecipeOptionsModel {
  bool restore_reflection = true;
};

struct YamlRecipeDocumentModel {
  unsigned version = 1;
  YamlRecipeOptionsModel options;
  YamlRecipeResourcesModel resources;
  std::vector<YamlRecipePrefilterModel> prefilters;
  std::vector<YamlRecipeRuleModel> rewrite_rules;
  std::vector<YamlRecipeStepModel> steps;
};

} // namespace dxp::sm6

template <>
struct glz::meta<dxp::sm6::YamlRecipeStepModel> {
  using T = dxp::sm6::YamlRecipeStepModel;
  static constexpr auto value = glz::object(
    "kind", &T::kind,
    "id", &T::id,
    "pattern", &T::pattern,
    "patterns", &T::patterns,
    "rule", &T::rule,
    "rules", &T::rules,
    "name", &T::name,
    "set", &T::set,
    "if", &T::if_condition,
    "mode", &T::mode,
    "required", &T::required
  );
};
