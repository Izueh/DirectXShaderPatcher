#pragma once

#include <dxp/sm6/Recipe.hpp>
#include <glaze/glaze.hpp>

#include "dxp/sm6/step/AddResourceStep_impl.hpp"
#include "dxp/sm6/step/ApplyRuleStep_impl.hpp"
#include "dxp/sm6/step/CheckOpcodeCountStep_impl.hpp"
#include "dxp/sm6/step/CheckResourceCountStep_impl.hpp"
#include "dxp/sm6/step/CheckShaderVersionStep_impl.hpp"

namespace dxp::sm6 {

/// @brief Variant type for all SM6 recipe step data types.
using StepDataVariant = std::variant<
    step::AddResourceData, step::ApplyRuleData,
    step::CheckOpcodeCountData, step::CheckResourceCountData, step::CheckShaderVersionData>;

/// @brief Top-level SM6 recipe data structure.
struct RecipeData {
  uint32_t version = 1;
  std::vector<StepDataVariant> steps;
};

std::pair<Recipe, std::string> ConvertRecipe(const RecipeData& data);

}  // namespace dxp::sm6

namespace glz {

template <>
struct meta<dxp::sm6::StepDataVariant> {
  using T = dxp::sm6::StepDataVariant;
  static constexpr auto ids = std::array<std::string_view, 5>{
      "add_resource", "apply_rule", "check_opcode_count", "check_resource_count", "check_shader_version"};
  static constexpr auto tag = std::string_view{"kind"};
  static_assert(std::size(ids) == std::variant_size_v<T>,
                "variant_ids count must match std::variant template parameter count");
};

template <>
struct meta<dxp::sm6::RecipeData> {
  using T = dxp::sm6::RecipeData;
  static constexpr auto value =
      object("version", &T::version, "steps", &T::steps);
  static constexpr auto validate = [](const dxp::sm6::RecipeData& self, std::string& error) {
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
