#pragma once

#include <dxp/sm6/ResourceTypes.hpp>
#include <dxp/sm6/step/AddResourceStep.hpp>
#include <glaze/glaze.hpp>
#include "dxp/CBufferLayout_impl.hpp"
#include "dxp/Condition_impl.hpp"
#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/StepResults.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm6::step {

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
std::expected<void, std::string> Validate(const AddResourceStep& step, ValidationContext& ctx);
/// @brief Formats the step's result as a Trace log message.
std::string DescribeOutcome(const AddResourceStep& step, const dxp::AddResourceResults& results, const ExecutionContext& ctx);

/// @brief Shader-facing type of a declaration, as reported in bindings and declarations.
dxp::ResourceTypeInfo TypeInfoOf(const TextureResourceDesc& desc);
dxp::ResourceTypeInfo TypeInfoOf(const CBufferDesc& desc);
/// @brief The step's resource declarations (inputs/outputs excluded).
std::vector<dxp::ResourceDeclaration> Declarations(const AddResourceStep& step);
/// @brief Registers this step's cbuffer layouts for field references in later steps.
std::expected<void, std::string> ResolveCBufferReferences(AddResourceStep& step, dxp::detail::CBufferLayouts& layouts);

/// @brief One resource declaration within a typed array (YAML data form).
struct AddResourceDeclData {
  std::string handle;
  std::optional<unsigned> register_index;
  std::optional<unsigned> space;
  ResourceKind kind = ResourceKind::Texture2D;
  dxp::ComponentType element_type = dxp::ComponentType::F32;
  unsigned vector_width = 4;
  unsigned size = 0;
  std::string type;
  bool is_read_write = false;
  std::vector<dxp::detail::CBufferFieldData> fields;  ///< cbuffers: field layout (auto-packed when offsets are omitted).
  glz::generic meta;  ///< Opaque consumer metadata, reported verbatim (null = absent; see CBufferFieldData).
};

/// @brief Top-level YAML block for grouped resource declarations.
struct AddResourceData {
  std::string name;
  ::dxp::ConditionData condition;
  bool required = true;

  std::vector<AddResourceDeclData> textures;
  std::vector<AddResourceDeclData> uavs;
  std::vector<AddResourceDeclData> cbuffers;
  std::vector<AddResourceDeclData> samplers;
  std::vector<AddResourceStep::InputSignatureDecl> inputs;
  std::vector<AddResourceStep::OutputSignatureDecl> outputs;

  /// @brief Compile this YAML data into an AddResourceStep.
  auto Compile() const -> std::expected<AddResourceStep, std::string>;
};

}  // namespace dxp::sm6::step

namespace glz {

template <>
struct meta<dxp::sm6::ResourceClass> {
  using T = dxp::sm6::ResourceClass;
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "SRV", T::SRV,
      "UAV", T::UAV,
      "CBuffer", T::CBuffer,
      "Sampler", T::Sampler,
      "Invalid", T::Invalid);
};

template <>
struct meta<dxp::sm6::ResourceKind> {
  using T = dxp::sm6::ResourceKind;
  // enumerate (not keys/value arrays) so the JSON schema lists the names.
  static constexpr auto value = enumerate(
      "Invalid", T::Invalid,
      "Texture1D", T::Texture1D,
      "Texture2D", T::Texture2D,
      "Texture2DMS", T::Texture2DMS,
      "Texture3D", T::Texture3D,
      "TextureCube", T::TextureCube,
      "Texture1DArray", T::Texture1DArray,
      "Texture2DArray", T::Texture2DArray,
      "Texture2DMSArray", T::Texture2DMSArray,
      "TextureCubeArray", T::TextureCubeArray,
      "TypedBuffer", T::TypedBuffer,
      "RawBuffer", T::RawBuffer,
      "StructuredBuffer", T::StructuredBuffer,
      "CBuffer", T::CBuffer,
      "Sampler", T::Sampler,
      "TBuffer", T::TBuffer,
      "RTAccelerationStructure", T::RTAccelerationStructure,
      "FeedbackTexture2D", T::FeedbackTexture2D,
      "FeedbackTexture2DArray", T::FeedbackTexture2DArray,
      "NumEntries", T::NumEntries);
};

template <>
struct meta<dxp::sm6::ResourceBindingDesc> {
  using T = dxp::sm6::ResourceBindingDesc;
  static constexpr auto value = object(
      "resource_class", &T::resource_class,
      "register_index", &T::register_index,
      "space", &T::space);
};

template <>
struct meta<dxp::sm6::step::AddResourceData> {
  static constexpr auto validate = [](const auto& self, std::string& error) {
    if (self.name.empty()) {
      error = "add_resource step requires a name";
    }
  };
};

}  // namespace glz
