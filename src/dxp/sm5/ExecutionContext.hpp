#pragma once

#include <any>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <dxp/ExportTypes.hpp>
#include "dxp/Condition_impl.hpp"
#include "dxp/Logging.hpp"
#include "dxp/sm5/DeclarationIndex.hpp"
#include "dxp/sm5/Model_impl.hpp"
#include "dxp/sm5/ShaderProgram.hpp"
#include "dxp/sm5/step/ApplyRuleStep.hpp"
#include "dxp/VariableStore.hpp"

namespace dxp::sm5 {
using namespace dxp::sm5::model;

/// @brief Binding namespace for handle→register maps (one per register family).
/// Keeps handle names separate per class (a texture and a raw buffer may share
/// a handle name). Uses the shared @c dxp::BindingClass vocabulary.
using BindingClass = dxp::BindingClass;

/// @brief Global capture store — stores captured operands, instructions, index
/// immediates, and instruction blobs as copies so they survive rewrites.
struct CaptureStore {
  std::unordered_map<std::string, CapturedOperand> operands;          ///< Captured operands, keyed by their `capture` name.
  std::unordered_map<std::string, CapturedInstruction> instructions;  ///< Captured instructions (sequence matches), keyed by name.
  std::unordered_map<std::string, Operand::Index> index_values;       ///< Captured index values (full Operand::Index), keyed by name.
  std::unordered_map<std::string, CapturedBlob> blobs;                ///< Captured instruction blobs (match_blob windows / scoped edits), keyed by name.

  void Clear() {
    operands.clear();
    instructions.clear();
    index_values.clear();
    blobs.clear();
  }
};

/// @brief Unified execution context for SM5 (DXBC) recipe step execution. Holds the shader program plus all transient execution state.
struct ExecutionContext : VariableStore {
  /// The shader program being modified.
  ShaderProgram program;

  uint32_t major_version = 0;
  uint32_t minor_version = 0;

  bool program_modified = false;

  std::string lastError;
  std::vector<std::string> diagnostics;

  /// @brief Named binding maps (handle → register index), one namespace per kind.
  std::unordered_map<BindingClass, std::unordered_map<std::string, uint32_t>> bindings;

  /// @brief Accessor for a binding namespace (creates it on first use).
  std::unordered_map<std::string, uint32_t>& Bindings(BindingClass kind) {
    return bindings[kind];
  }

  /// @brief Const accessor for a binding namespace. Returns an empty map when the
  /// namespace was never populated.
  const std::unordered_map<std::string, uint32_t>& Bindings(BindingClass kind) const {
    static const std::unordered_map<std::string, uint32_t> kEmpty;
    auto it = bindings.find(kind);
    return it != bindings.end() ? it->second : kEmpty;
  }

  uint32_t reserved_temp_base = 0;
  uint32_t reserved_temp_count = 0;

  /// @brief Template pool (reuse model): fixed contiguous r# region
  /// [template_pool_base, template_pool_base + template_pool_size), reused by
  /// every instantiation. Safe because template temp names are unbound after
  /// each expansion, so no instruction outside an expansion can reference
  /// those registers by name. Width = max temp count across all templates.
  uint32_t template_pool_base = 0;
  uint32_t template_pool_size = 0;

  /// @brief Registered template — temps + emit patterns + required captures
  /// colocated (one struct per template, no parallel maps).
  struct RegisteredTemplate {
    std::vector<std::string> temps;
    std::vector<step::EmitPattern> emits;
    /// @brief Capture names (instruction + operand + index level) referenced by
    /// the template's emits; resolved against the global capture store at
    /// expansion time (cross-step captures persist; named runtime failure if
    /// not produced). Registered at declare time.
    std::unordered_set<std::string> required_captures;
  };

  /// @brief Registered templates — name → RegisteredTemplate (flattened from Template wrappers).
  std::unordered_map<std::string, RegisteredTemplate> templates;

  CaptureStore captures;

  std::unordered_map<std::string, dxp::ResourceUsage> resource_exports;     ///< Resource usage discovered during pattern matching.
  std::unordered_map<std::string, dxp::ImmediateValue> immediate_exports;   ///< Immediate values discovered during pattern matching.
  std::unordered_map<std::string, dxp::ResourceBinding> resource_bindings;  ///< Resource bindings from AddResourceStep.
  std::unordered_map<std::string, std::any> results;                        ///< Results for dot-notation resolution.

  /// @brief Cross-reference index from operand registers to their declarations.
  /// Invalidated by MarkProgramMutated, rebuilt lazily by Declarations().
  mutable DeclarationIndex declaration_index;
  mutable bool declaration_index_dirty = true;

  /// @brief Marks the program mutated (invalidates the declaration index).
  void MarkProgramMutated() {
    program_modified = true;
    declaration_index_dirty = true;
  }

  /// @brief Access to the declaration index, rebuilding it if stale.
  [[nodiscard]] const DeclarationIndex& Declarations() const {
    if (declaration_index_dirty) {
      declaration_index = BuildDeclarationIndex(program);
      declaration_index_dirty = false;
    }
    return declaration_index;
  }

  /// @brief Per-execution logging state (sink + level filter, populated from PatchOptions).
  dxp::LogContext logger;
};

}  // namespace dxp::sm5
