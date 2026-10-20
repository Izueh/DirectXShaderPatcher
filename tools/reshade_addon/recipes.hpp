#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "common.hpp"
#include "config.hpp"
#include "requirements.hpp"
#include "dxp/sm5/Recipe.hpp"
#if DXP_ADDON_HAS_SM6
#include "dxp/sm6/Recipe.hpp"
#endif

namespace dxp::addon {

using EnvMap = std::map<std::string, dxp::PrimitiveValue>;

/// One recipe file. Recipes in <recipes>/<model>/ apply to every stage; recipes in
/// <recipes>/<model>/<vs|ps|cs|...>/ only to that stage (ShaderInjector layout).
struct RecipeEntry {
  std::string name;  // path relative to the recipe dir, e.g. "sm6/ps/blue_noise.recipe.yml"
  std::filesystem::path path;
  ShaderModel model = ShaderModel::kSm5;
  std::optional<Stage> stage;

  std::shared_ptr<const dxp::sm5::Recipe> sm5;
#if DXP_ADDON_HAS_SM6
  std::shared_ptr<const dxp::sm6::Recipe> sm6;
#endif
  std::string load_error;  // parse/validation failure; the recipe is skipped
  uint32_t version = 0;
  size_t step_count = 0;
  bool has_add_resource = false;
  EnvMap env_defaults;
  std::vector<DeclaredResource> resources;  // add_resource declarations, in step order

  [[nodiscard]] bool Loaded() const { return load_error.empty(); }
};

/// Per-recipe execution counters for the current generation, shown in the overlay.
struct RecipeStats {
  std::atomic<uint32_t> patched{0};
  std::atomic<uint32_t> failed{0};
  std::mutex mutex;
  std::string last_error;
};

/// Output of running every applicable recipe on one shader.
struct PatchedShader {
  std::vector<uint8_t> code;
  std::vector<dxp::ResourceBinding> bindings;  // resource bindings the recipes added
  std::vector<std::string> recipes;            // recipes that modified the shader
};

/// Immutable snapshot of loaded recipes plus env overrides. Each reload or env
/// change publishes a new snapshot with a new generation; pipelines are re-patched
/// against it. Patch results are cached per snapshot by (shader hash, stage).
class RecipeSet {
 public:
  static std::shared_ptr<RecipeSet> Load(const std::filesystem::path& recipe_dir,
                                         const std::unordered_map<std::string, EnvMap>& overrides);
  /// Same recipes, new env overrides (no re-parse).
  [[nodiscard]] std::shared_ptr<RecipeSet> WithOverrides(const std::unordered_map<std::string, EnvMap>& overrides) const;

  [[nodiscard]] uint64_t Generation() const { return generation_; }
  [[nodiscard]] const std::vector<RecipeEntry>& Entries() const { return entries_; }
  [[nodiscard]] RecipeStats& Stats(size_t index) const { return stats_[index]; }
  [[nodiscard]] const std::unordered_map<std::string, EnvMap>& Overrides() const { return overrides_; }
  [[nodiscard]] bool AnyAddResource() const { return any_add_resource_; }
  [[nodiscard]] bool AppliesTo(ShaderModel model, Stage stage) const;

  /// Runs all recipes for (model, stage) in name order, chaining outputs.
  /// Returns nullptr when no recipe modified the shader. Thread-safe.
  std::shared_ptr<const PatchedShader> Patch(ShaderModel model, Stage stage, uint64_t hash,
                                             std::span<const uint8_t> code, const Settings& settings) const;

 private:
  RecipeSet() = default;
  void Finalize();

  uint64_t generation_ = 0;
  std::vector<RecipeEntry> entries_;
  std::unordered_map<std::string, EnvMap> overrides_;
  std::unique_ptr<RecipeStats[]> stats_;
  bool any_add_resource_ = false;

  mutable std::mutex cache_mutex_;
  mutable std::unordered_map<uint64_t, std::shared_ptr<const PatchedShader>> cache_;
};

/// Identifies DXBC vs DXIL containers. nullopt for anything else (SPIR-V, DXBC SM1-3 ...).
std::optional<ShaderModel> DetectShaderModel(std::span<const uint8_t> code);

/// Stable 64-bit identity of a shader container (container digest when present, else FNV-1a).
uint64_t HashShader(std::span<const uint8_t> code);

}  // namespace dxp::addon
