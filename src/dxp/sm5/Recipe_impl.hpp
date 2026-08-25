#pragma once

#include <dxp/sm5/Recipe.hpp>
#include <glaze/glaze.hpp>
#include "dxp/sm5/Model_impl.hpp"
#include "dxp/sm5/step/AddResourceStep_impl.hpp"
#include "dxp/sm5/step/ApplyRuleStep_impl.hpp"
#include "dxp/sm5/step/CheckOpcodeCountStep_impl.hpp"
#include "dxp/sm5/step/CheckResourceCountStep_impl.hpp"
#include "dxp/sm5/step/CheckShaderVersionStep_impl.hpp"

namespace dxp::sm5 {
using namespace dxp::sm5::model;

/// @brief Variant type for all SM5 recipe step data types.
using StepDataVariant =
    std::variant<step::AddResourceData, step::ApplyRuleData, step::CheckShaderVersionData,
                 step::CheckOpcodeCountData, step::CheckResourceCountData>;

/// @brief Top-level SM5 recipe data structure.
struct RecipeData {
  uint32_t version = 1;
  std::unordered_map<std::string, PrimitiveValue> env;
  std::vector<StepDataVariant> steps;
};

}  // namespace dxp::sm5

namespace glz {

template <>
struct meta<dxp::sm5::StepDataVariant> {
  using T = dxp::sm5::StepDataVariant;
  static constexpr std::array<std::string_view, 5> ids = {"add_resource", "apply_rule",
                                                          "check_shader_version",
                                                          "check_opcode_count",
                                                          "check_resource_count"};
  static constexpr std::string_view tag = "kind";
  static_assert(std::size(ids) == std::variant_size_v<T>,
                "variant_ids count must match std::variant template parameter count");
};

template <>
struct meta<dxp::sm5::RecipeData> {
  using T = dxp::sm5::RecipeData;
  static constexpr auto value = object("version", &T::version, "env", &T::env, "steps", &T::steps);
  static constexpr auto validate = [](const dxp::sm5::RecipeData& self, std::string& error) {
    if (self.version != 1) {
      error = "unsupported recipe schema version";
      return;
    }
    if (self.steps.empty()) {
      error = "recipe requires at least one step";
      return;
    }
  };
};

}  // namespace glz
