#pragma once

#include <dxp/sm5/step/AddResourceStep.hpp>
#include <glaze/glaze.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/sm5/ExecutionContext.hpp"
#include "dxp/sm5/Model_impl.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm5::step {
using namespace dxp::sm5::model;

/// @brief Execute the AddResourceStep against the shader program.
/// @param step The step to execute.
/// @param ctx Execution context containing the shader program.
/// @return Results with counts of added resources, or error message.
std::expected<dxp::AddResourceResults, std::string> Execute(const AddResourceStep& step, ExecutionContext& ctx);

/// @brief Validate the AddResourceStep.
/// @param step The step to validate.
/// @param error Output error message on failure.
/// @param ctx Validation context.
/// @return void on success, error message on failure.
std::expected<void, std::string> Validate(const AddResourceStep& step, dxp::ValidationContext& ctx);
/// @brief Formats the step's result as a Trace log message.
std::string DescribeOutcome(const AddResourceStep& step, const dxp::AddResourceResults& results, const ExecutionContext& ctx);

/// @brief One resource declaration within a typed array.
struct AddResourceDeclData {
  std::string handle;
  std::optional<uint32_t> register_index;
  std::optional<bool> reverse_bind;            ///< Auto-bind reverse order: take the HIGHEST free slot instead of the lowest.

  std::optional<uint32_t> elements;
  std::optional<InterpolationMode> interpolation;
  std::optional<SamplerMode> mode;
  std::optional<AddResourceStep::UavKind> kind;
  uint32_t stride{};  // Required: non-zero byte stride for structured buffer elements
  std::optional<ResourceDimension> dimension;
  std::optional<bool> globally_coherent;
  std::optional<bool> has_counter;
};

/// @brief Top-level YAML block for grouped resource declarations.
struct AddResourceData {
  std::string name;
  ::dxp::ConditionData condition;
  bool required = true;

  std::vector<AddResourceDeclData> textures;
  std::vector<AddResourceDeclData> raw_resources;
  std::vector<AddResourceDeclData> structured_resources;
  std::vector<AddResourceDeclData> cbuffers;
  std::vector<AddResourceDeclData> samplers;
  std::vector<AddResourceDeclData> uavs;
  std::vector<AddResourceDeclData> inputs;
  std::vector<AddResourceDeclData> outputs;
  std::vector<std::string> temps;

  /**
   * @brief Compile this YAML data into an AddResourceStep.
   * @return Compiled step or error message.
   */
  auto Compile() const -> std::expected<AddResourceStep, std::string>;
};

}  // namespace dxp::sm5::step

namespace glz {

template <>
struct meta<dxp::sm5::step::AddResourceStep::UavKind> {
  using T = dxp::sm5::step::AddResourceStep::UavKind;
  static constexpr auto keys = std::array{"typed", "raw", "structured"};
  static constexpr auto value = std::array{T::Typed, T::Raw, T::Structured};
};

}  // namespace glz
