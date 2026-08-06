#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dxp/Condition.hpp"
#include "dxp/sm6/ResourceTypes.hpp"
#include "dxp/StepResults.hpp"

namespace dxp::sm6::step {

/// @brief Runtime step type for grouped resource declarations.
/// Signature declaration types are nested here so the step namespace contains
/// only the step structs themselves.
struct AddResourceStep {
  static constexpr std::string_view kind = "add_resource";
  using Results = dxp::AddResourceResults;

  /// @brief One input signature declaration.
  struct InputSignatureDecl {
    std::string handle;
    std::string semantic_name;
    dxp::ComponentType comp_type = dxp::ComponentType::F32;
    uint32_t vector_size = 4;
    std::optional<uint32_t> register_index;
    InterpolationMode interp_mode = InterpolationMode::Linear;
  };

  /// @brief One output signature declaration.
  struct OutputSignatureDecl {
    std::string handle;
    std::string semantic_name;
    dxp::ComponentType comp_type = dxp::ComponentType::F32;
    uint32_t vector_size = 4;
    std::optional<uint32_t> register_index;
  };

  std::string name;
  bool required = true;
  std::optional<ConditionNode> condition;

  std::vector<TextureResourceDesc> textures;
  std::vector<TextureResourceDesc> uavs;
  std::vector<CBufferDesc> cbuffers;
  std::vector<SamplerDesc> samplers;
  std::vector<InputSignatureDecl> inputs;
  std::vector<OutputSignatureDecl> outputs;

  AddResourceStep() = default;
};

}  // namespace dxp::sm6::step
