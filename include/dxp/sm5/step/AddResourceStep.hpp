#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dxp/Condition.hpp"
#include "dxp/sm5/Model.hpp"
#include "dxp/StepResults.hpp"

namespace dxp::sm5::step {

/// @brief Runtime step type for grouped resource declarations.
/// All declaration types used by this step are nested here so the step namespace
/// contains only the step structs themselves.
struct AddResourceStep {
  static constexpr std::string_view kind = "add_resource";
  using Results = dxp::AddResourceResults;
  using SamplerMode = dxp::sm5::model::SamplerMode;
  using CbufferAccessPattern = dxp::sm5::model::CbufferAccessPattern;

  /// @brief Identifies the UAV kind requested by a recipe declaration.
  enum class UavKind : std::uint8_t {
    Typed,
    Raw,
    Structured,
  };

  /// @brief Declares a texture binding to add or reference in a recipe.
  struct TextureDecl {
    std::optional<uint32_t> register_index;  ///< Binding register index. Empty for next available, or a number for explicit.
    uint32_t dimension = 3U;
    std::string handle;
  };

  /// @brief Declares one or more temporary register handles consumed by add_temp steps.
  struct TempDecl {
    std::vector<std::string> handles;
  };

  /// @brief Input signature binding declaration.
  struct InputDecl {
    std::optional<uint32_t> register_index;  ///< Binding register index. Empty for next available, or a number for explicit.
    uint32_t interpolation_mode = 2U;
    std::string handle;
  };

  /// @brief Output signature binding declaration.
  struct OutputDecl {
    std::optional<uint32_t> register_index;  ///< Binding register index. Empty for next available, or a number for explicit.
    std::string handle;
  };

  /// @brief Constant buffer binding declaration.
  struct CBufferDecl {
    std::optional<uint32_t> register_index;  ///< Binding register index. Empty for next available, or a number for explicit.
    uint32_t elements = 1;
    CbufferAccessPattern access_pattern = CbufferAccessPattern::ImmediateIndexed;
    std::string handle;
  };

  /// @brief Sampler binding declaration.
  struct SamplerDecl {
    std::optional<uint32_t> register_index;  ///< Binding register index. Empty for next available, or a number for explicit.
    SamplerMode mode = SamplerMode::Default;
    std::string handle;
  };

  /// @brief Raw resource binding declaration.
  struct RawResourceDecl {
    std::optional<uint32_t> register_index;  ///< Binding register index. Empty for next available, or a number for explicit.
    std::string handle;
  };

  /// @brief Structured resource binding declaration.
  struct StructuredResourceDecl {
    std::optional<uint32_t> register_index;  ///< Binding register index. Empty for next available, or a number for explicit.
    uint32_t structure_stride;               ///< Required: byte stride of structured buffer elements
    std::string handle;
  };

  /// @brief UAV binding declaration.
  struct UavDecl {
    std::optional<uint32_t> register_index;  ///< Binding register index. Empty for next available, or a number for explicit.
    UavKind kind = UavKind::Typed;
    uint32_t dimension = 3U;
    uint32_t structure_stride = 0;  ///< 0 = skip stride token, DXBC loader uses its default
    bool globally_coherent = false;
    bool has_order_preserving_counter = false;
    std::string handle;
  };

  std::string name;
  bool required = true;
  std::optional<ConditionNode> condition;

  std::vector<TextureDecl> textures;
  std::vector<RawResourceDecl> raw_resources;
  std::vector<StructuredResourceDecl> structured_resources;
  std::vector<CBufferDecl> cbuffers;
  std::vector<SamplerDecl> samplers;
  std::vector<UavDecl> uavs;
  std::vector<InputDecl> inputs;
  std::vector<OutputDecl> outputs;
  std::vector<std::string> temps;
};

}  // namespace dxp::sm5::step
