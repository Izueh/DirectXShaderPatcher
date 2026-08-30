#pragma once

#include <dxp/sm6/ResourceTypes.hpp>
#include <dxp/sm6/step/AddResourceStep.hpp>
#include <glaze/glaze.hpp>
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
  struct Field {
    std::string name;
    dxp::ComponentType type = dxp::ComponentType::F32;
    unsigned width = 0;
    unsigned offset = 0;
  };
  std::vector<Field> fields;
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
  static constexpr auto keys = std::array{"SRV", "UAV", "CBuffer", "Sampler", "Invalid"};
  static constexpr auto value = std::array{T::SRV, T::UAV, T::CBuffer, T::Sampler, T::Invalid};
};

template <>
struct meta<dxp::sm6::ResourceKind> {
  using T = dxp::sm6::ResourceKind;
  static constexpr auto keys = std::array{
      "Invalid", "Texture1D", "Texture2D", "Texture2DMS", "Texture3D",
      "TextureCube", "Texture1DArray", "Texture2DArray", "Texture2DMSArray",
      "TextureCubeArray", "TypedBuffer", "RawBuffer", "StructuredBuffer",
      "CBuffer", "Sampler", "TBuffer", "RTAccelerationStructure",
      "FeedbackTexture2D", "FeedbackTexture2DArray", "NumEntries"};
  static constexpr auto value = std::array{
      T::Invalid, T::Texture1D, T::Texture2D, T::Texture2DMS, T::Texture3D,
      T::TextureCube, T::Texture1DArray, T::Texture2DArray, T::Texture2DMSArray,
      T::TextureCubeArray, T::TypedBuffer, T::RawBuffer, T::StructuredBuffer,
      T::CBuffer, T::Sampler, T::TBuffer, T::RTAccelerationStructure,
      T::FeedbackTexture2D, T::FeedbackTexture2DArray, T::NumEntries};
};

template <>
struct meta<dxp::ComponentType> {
  using T = dxp::ComponentType;
  static constexpr auto keys = std::array{
      "Invalid", "I1", "I16", "U16", "I32", "U32", "I64", "U64",
      "F16", "F32", "F64", "SNormF16", "UNormF16", "SNormF32", "UNormF32",
      "SNormF64", "UNormF64", "PackedS8x32", "PackedU8x32", "U8", "I8",
      "F8_E4M3FN", "F8_E5M2"};
  static constexpr auto value = std::array{
      T::Invalid, T::I1, T::I16, T::U16, T::I32, T::U32, T::I64, T::U64,
      T::F16, T::F32, T::F64, T::SNormF16, T::UNormF16, T::SNormF32, T::UNormF32,
      T::SNormF64, T::UNormF64, T::PackedS8x32, T::PackedU8x32, T::U8, T::I8,
      T::F8_E4M3FN, T::F8_E5M2};
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
