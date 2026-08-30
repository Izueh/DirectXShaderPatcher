#pragma once

#include <any>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <dxp/ExportTypes.hpp>
#include <dxp/sm6/ResourceTypes.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/Logging.hpp"
#include "dxp/sm6/ShaderProgram.hpp"
#include "dxp/VariableStore.hpp"
#include "llvm/IR/Value.h"

namespace dxp::sm6 {

/// @brief Global capture store — stores captured values as pointers
/// so they survive rewrites and can be referenced in subsequent steps.
struct CaptureStore {
  /// Captured values, keyed by their `capture` name.
  std::unordered_map<std::string, llvm::Value*> values;
};

/// @brief Unified execution context for SM6 (DXIL) recipe step execution. Holds the ShaderProgram plus all transient execution state.
struct ExecutionContext : VariableStore {
  /// @brief Touches the per-thread DXC runtime (file system + thread malloc)
  /// before the program is loaded. Declared before `program`.
  DxcRuntime dxc_runtime;

  /// The shader program being modified.
  ShaderProgram program;

  uint32_t major_version = 0;
  uint32_t minor_version = 0;

  bool program_modified = false;

  std::vector<std::string> diagnostics;

  std::unordered_map<std::string, TextureResourceDesc> textures;
  std::unordered_map<std::string, TextureResourceDesc> uavs;
  std::unordered_map<std::string, CBufferDesc> cbuffers;
  std::unordered_map<std::string, SamplerDesc> samplers;
  std::unordered_map<std::string, uint32_t> input_bindings;
  std::unordered_map<std::string, uint32_t> output_bindings;

  /// Results storage for dot-notation resolution.
  std::unordered_map<std::string, std::any> results;
  /// Global capture store — persists captures across steps.
  CaptureStore captures;
  std::unordered_map<std::string, const hlsl::DxilResourceBase*> resource_handles;  ///< Resource handles from add_resource step.
  std::unordered_map<std::string, llvm::Value*> resource_handle_values;             ///< LLVM IR values for resource handles from add_resource step.
  std::unordered_map<std::string, dxp::ResourceUsage> resource_exports;             ///< Resource usage and immediate values from pattern matching.
  std::unordered_map<std::string, dxp::ImmediateValue> immediate_exports;
  std::unordered_map<std::string, dxp::ResourceBinding> resource_bindings;  ///< Resource bindings from AddResourceStep.

  /// @brief Per-execution logging state (sink + level filter, populated from PatchOptions).
  dxp::LogContext logger;

  /**
   * @brief Resolve a field offset from a cbuffer schema.
   * @param cbuffer_name The cbuffer handle name (without field).
   * @param field_name The field name (after the dot).
   * @return The byte offset of the field, or 0 if not found.
   */
  [[nodiscard]] uint32_t ResolveFieldOffset(const std::string& cbuffer_name, const std::string& field_name) const {
    auto cbuf_it = cbuffers.find(cbuffer_name);
    if (cbuf_it == cbuffers.end() || cbuf_it->second.schema == nullptr) {
      return 0;
    }
    for (const auto& field : cbuf_it->second.schema->fields) {
      if (field.name == field_name) {
        return field.offset;
      }
    }
    return 0;
  }
};

}  // namespace dxp::sm6
