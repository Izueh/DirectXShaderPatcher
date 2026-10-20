#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "dxp/ExportTypes.hpp"

namespace dxp::addon {

/// Draw-time context passed to binding providers.
struct DrawContext {
  api::command_list* cmd_list = nullptr;
  api::shader_stage stage = api::shader_stage::all;
};

/// Extension point for mods that bind something other than a static DDS file
/// (tracked game render targets, buffers written by another pass, ...).
///
/// A provider is called on the recording thread right before every draw that uses
/// a patched shader referencing `handle`, and returns the descriptor to bind:
/// a resource_view handle for SRV/UAV bindings or a sampler handle for samplers.
/// Returning 0 makes the add-on fall back to the original (unpatched) shader for
/// that draw, so a provider can safely report "not available yet".
using BindingProvider = std::function<uint64_t(const DrawContext& context, const dxp::ResourceBinding& binding)>;

/// Registers a provider for a recipe resource handle. Providers take precedence over
/// files in resources/. Register before the shaders that use the handle are patched
/// (e.g. in AddonInit) or trigger a re-patch afterwards.
void RegisterBindingProvider(const std::string& handle, BindingProvider provider);
void UnregisterBindingProvider(const std::string& handle);
bool HasBindingProvider(const std::string& handle);

/// A recipe binding resolved to something the draw path can push.
struct ResolvedBinding {
  dxp::ResourceBinding binding;
  api::descriptor_type type = api::descriptor_type::shader_resource_view;
  uint64_t descriptor = 0;                          // static resource_view / sampler handle
  std::shared_ptr<const BindingProvider> provider;  // set for dynamic bindings
};

/// Textures and samplers for one device, loaded from <root>/resources:
///   *.dds          textures, addressed by file name ("noise.dds"), stem ("noise")
///                  or path relative to resources/ ("blue/noise.dds")
///   samplers.ini   named sampler states, one [handle] section each
/// Sampler handles not listed in samplers.ini are parsed from their name:
/// "linear_wrap", "point_clamp", "aniso_mirror", ... (default linear + clamp).
///
/// Destroyed GPU objects must not be in flight, so retired sets are released
/// through the runtime's deferred retire queue.
class ResourceSet {
 public:
  static std::shared_ptr<ResourceSet> Load(api::device* device, const std::filesystem::path& directory);
  ~ResourceSet();
  ResourceSet(const ResourceSet&) = delete;
  ResourceSet& operator=(const ResourceSet&) = delete;

  /// Maps a recipe binding to a descriptor. Thread-safe (read-only after Load).
  bool Resolve(const dxp::ResourceBinding& binding, ResolvedBinding& out, std::string& error) const;

  struct TextureInfo {
    std::string name;
    std::string error;
    api::resource_desc desc;
    api::resource_view_type view_type = api::resource_view_type::unknown;
    api::resource resource = {0};
    api::resource_view srv = {0};
  };
  struct SamplerInfo {
    std::string name;
    api::sampler_desc desc;
    api::sampler sampler = {0};
    bool from_ini = false;  // a samplers.ini section, not a built-in
  };
  [[nodiscard]] const std::vector<TextureInfo>& Textures() const { return textures_; }
  [[nodiscard]] const std::vector<SamplerInfo>& Samplers() const { return samplers_; }
  [[nodiscard]] api::device* Device() const { return device_; }

  /// The DDS file and sampler state Resolve would use for a handle; nullptr when none.
  [[nodiscard]] const TextureInfo* FindTexture(const std::string& handle) const;
  [[nodiscard]] const SamplerInfo* FindSampler(const std::string& handle) const;

 private:
  explicit ResourceSet(api::device* device) : device_(device) {}
  void LoadTextures(const std::filesystem::path& directory);
  void LoadSamplers(const std::filesystem::path& directory);

  api::device* device_ = nullptr;
  std::vector<TextureInfo> textures_;
  std::vector<SamplerInfo> samplers_;
  std::unordered_map<std::string, size_t> texture_lookup_;  // lowercase key -> textures_ index
  std::unordered_map<std::string, size_t> sampler_lookup_;
};

}  // namespace dxp::addon
