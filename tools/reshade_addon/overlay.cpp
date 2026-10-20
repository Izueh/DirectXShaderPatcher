#include "overlay.hpp"

#include <dxp/Version.hpp>

#include "runtime.hpp"

namespace dxp::addon {

namespace {

// UI-thread working copy of the env overrides. Synced from the published recipe set
// whenever nothing is pending, so a reload shows what is actually applied.
std::unordered_map<std::string, EnvMap> g_pending;
const RecipeSet* g_synced_from = nullptr;
bool g_dirty = false;

const char* DeviceApiName(api::device_api device_api) {
  return device_api == api::device_api::d3d12 ? "D3D12" : "D3D11";
}

void HelpMarker(const char* text) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  ImGui::SetItemTooltip("%s", text);
}

/// Draws one env variable editor. `changed` reports any edit (stored right away so the
/// typed value survives until focus is lost); the return value reports a commit: text edits
/// commit when the field loses focus, +/- buttons and checkboxes immediately. Every
/// commit re-patches all pipelines, so per-keystroke commits are avoided.
bool EnvWidget(const char* label, dxp::PrimitiveValue& value, bool& changed) {
  return std::visit(
      [&](auto& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
          changed = ImGui::Checkbox(label, &v);
          return changed;
        } else if constexpr (std::is_same_v<T, double>) {
          changed = ImGui::InputDouble(label, &v, 0.0, 0.0, "%.6g");
        } else {
          constexpr ImGuiDataType kType = std::is_same_v<T, int32_t>    ? ImGuiDataType_S32
                                          : std::is_same_v<T, uint32_t> ? ImGuiDataType_U32
                                          : std::is_same_v<T, int64_t>  ? ImGuiDataType_S64
                                                                        : ImGuiDataType_U64;
          const T step = 1;
          changed = ImGui::InputScalar(label, kType, &v, &step);
        }
        return (changed && !ImGui::IsItemActive()) || ImGui::IsItemDeactivatedAfterEdit();
      },
      value);
}

constexpr ImU32 kColorOk = IM_COL32(128, 224, 128, 255);
constexpr ImU32 kColorWarning = IM_COL32(255, 200, 64, 255);
constexpr ImU32 kColorError = IM_COL32(255, 96, 96, 255);

struct RequirementStatus {
  std::string text;
  ImU32 color = kColorOk;
};

/// How the add-on would satisfy a declaration on the current device, mirroring ResourceSet::Resolve.
RequirementStatus GetRequirementStatus(const dxp::ResourceDeclaration& declaration, const ResourceSet* resources) {
  if (HasBindingProvider(declaration.handle)) return {"binding provider registered"};
  switch (declaration.binding_class) {
    case dxp::BindingClass::Texture: {
      if (resources == nullptr) return {"no device yet", kColorWarning};
      const ResourceSet::TextureInfo* texture = resources->FindTexture(declaration.handle);
      if (texture == nullptr) return {"DDS not found in resources/", kColorError};
      if (!texture->error.empty()) return {std::format("{}: {}", texture->name, texture->error), kColorError};
      if (!DimensionMatches(declaration.type.dimension, texture->view_type)) {
        return {std::format("{} is {}, recipe declares {}", texture->name, ViewTypeName(texture->view_type),
                            DimensionName(*declaration.type.dimension)),
                kColorWarning};
      }
      return {std::format("found {}", texture->name)};
    }
    case dxp::BindingClass::Sampler: {
      if (resources == nullptr) return {"no device yet", kColorWarning};
      const ResourceSet::SamplerInfo* sampler = resources->FindSampler(declaration.handle);
      if (sampler == nullptr || sampler->sampler == 0) return {"sampler could not be created", kColorError};
      if (sampler->from_ini) return {std::format("samplers.ini [{}]", sampler->name)};
      return {std::format("built-in {}", sampler->name)};
    }
    case dxp::BindingClass::CBuffer:
      return {"not supported yet", kColorWarning};
    default:
      return {"needs a binding provider", kColorWarning};
  }
}

void HintsTooltip(const ResourceHints& hints) {
  if (!hints.description.empty()) ImGui::TextUnformatted(hints.description.c_str());
  if (!hints.format_name.empty()) ImGui::Text("format: %s", hints.format_name.c_str());
  if (hints.size) {
    std::string size = SizeModeName(*hints.size);
    if (*hints.size == SizeMode::kFixed) {
      size += std::format(" {}x{}", hints.width.value_or(0), hints.height.value_or(0));
    } else if (hints.size_of) {
      size += *hints.size_of == SizeReference::kBackbuffer ? " of backbuffer" : " of render target";
    }
    ImGui::Text("size: %s", size.c_str());
  }
  if (hints.clear) ImGui::Text("clear: %g, %g, %g, %g", (*hints.clear)[0], (*hints.clear)[1], (*hints.clear)[2], (*hints.clear)[3]);
  for (const std::string& warning : hints.warnings) ImGui::TextColored(ImColor(kColorWarning), "%s", warning.c_str());
}

void DrawRequirements(const RecipeEntry& entry, const ResourceSet* resources) {
  if (entry.resources.empty()) return;
  constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
  if (!ImGui::BeginTable("requirements", 5, kFlags)) return;
  ImGui::TableSetupColumn("Handle");
  ImGui::TableSetupColumn("Class");
  ImGui::TableSetupColumn("Type");
  ImGui::TableSetupColumn("Register");
  ImGui::TableSetupColumn("Status");
  ImGui::TableHeadersRow();
  for (const DeclaredResource& resource : entry.resources) {
    const dxp::ResourceDeclaration& declaration = resource.declaration;
    const ResourceHints& hints = resource.hints;
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::TextUnformatted(declaration.handle.c_str());
    const bool has_tooltip = !hints.description.empty() || !hints.format_name.empty() || hints.size || hints.clear || !hints.warnings.empty();
    if (has_tooltip && ImGui::BeginItemTooltip()) {
      HintsTooltip(hints);
      ImGui::EndTooltip();
    }
    if (declaration.conditional) {
      ImGui::SameLine();
      ImGui::TextDisabled("(conditional)");
      ImGui::SetItemTooltip("The add_resource step has a condition or required: false, so it may not be added.");
    }

    ImGui::TableNextColumn();
    ImGui::TextUnformatted(BindingClassName(declaration.binding_class));
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(DescribeType(declaration.type).c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(DescribeSlot(declaration).c_str());

    ImGui::TableNextColumn();
    const RequirementStatus status = GetRequirementStatus(declaration, resources);
    ImGui::PushStyleColor(ImGuiCol_Text, status.color);
    ImGui::TextWrapped("%s", status.text.c_str());
    ImGui::PopStyleColor();
    if (!hints.warnings.empty()) {
      ImGui::TextColored(ImColor(kColorWarning), "%zu meta warning(s)", hints.warnings.size());
      if (ImGui::BeginItemTooltip()) {
        HintsTooltip(hints);
        ImGui::EndTooltip();
      }
    }
  }
  ImGui::EndTable();
}

void DrawRecipes(const RecipeSet& recipes, const ResourceSet* resources, bool auto_apply) {
  const auto& entries = recipes.Entries();
  if (!ImGui::CollapsingHeader(std::format("Recipes ({})###recipes", entries.size()).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) return;

  if (entries.empty()) {
    ImGui::TextWrapped("No recipes found. Place *.recipe.yml files in recipes/sm5 or recipes/sm6 (optionally in vs/ps/cs/... subfolders) and press Reload.");
  }

  bool apply = false;
  for (size_t i = 0; i < entries.size(); ++i) {
    const RecipeEntry& entry = entries[i];
    RecipeStats& stats = recipes.Stats(i);
    ImGui::PushID(static_cast<int>(i));

    const bool failed = !entry.Loaded();
    if (failed) ImGui::PushStyleColor(ImGuiCol_Text, kColorError);
    const bool open = ImGui::TreeNodeEx(entry.name.c_str(), entry.env_defaults.empty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen);
    if (failed) ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("%u patched, %u failed", stats.patched.load(), stats.failed.load());

    if (open) {
      if (failed) {
        ImGui::TextWrapped("Load error: %s", entry.load_error.c_str());
      } else {
        ImGui::TextDisabled("%s, stage %s, version %u, %zu step(s)%s", entry.model == ShaderModel::kSm6 ? "SM6" : "SM5",
                            entry.stage ? StageName(*entry.stage) : "all", entry.version, entry.step_count,
                            entry.has_add_resource ? ", adds resources" : "");
      }
      {
        std::lock_guard lock(stats.mutex);
        if (!stats.last_error.empty()) ImGui::TextWrapped("Last error: %s", stats.last_error.c_str());
      }
      DrawRequirements(entry, resources);

      for (const auto& [variable, default_value] : entry.env_defaults) {
        auto& overrides = g_pending[entry.name];
        const auto it = overrides.find(variable);
        dxp::PrimitiveValue value = it != overrides.end() ? it->second : default_value;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
        bool changed = false;
        const bool committed = EnvWidget(variable.c_str(), value, changed);
        if (changed) {
          overrides[variable] = value;
          g_dirty = true;
        }
        apply |= committed && auto_apply;
        if (it != overrides.end()) {
          ImGui::SameLine();
          if (ImGui::SmallButton("Reset")) {
            overrides.erase(variable);
            g_dirty = true;
            apply |= auto_apply;
          }
          ImGui::SameLine();
          ImGui::TextDisabled("default %s", FormatValue(default_value).c_str());
        }
      }
      ImGui::TreePop();
    }
    ImGui::PopID();
  }

  if (g_dirty && !auto_apply) {
    apply |= ImGui::Button("Apply env changes");
    ImGui::SameLine();
  }
  if (ImGui::Button("Save env values")) {
    for (const RecipeEntry& entry : entries) {
      const auto overrides = g_pending.find(entry.name);
      for (const auto& [variable, default_value] : entry.env_defaults) {
        std::optional<dxp::PrimitiveValue> value;
        if (overrides != g_pending.end()) {
          if (const auto it = overrides->second.find(variable); it != overrides->second.end()) value = it->second;
        }
        SaveEnvOverride(entry.name, variable, value);
      }
    }
  }
  HelpMarker("Writes the current values to ReShade.ini [DXP.Env] so they are applied on the next launch.");

  if (apply) {
    ApplyEnvOverrides(g_pending);
    g_dirty = false;
  }
}

void DrawResources(const ResourceSet& resources) {
  if (!ImGui::CollapsingHeader(std::format("Resources ({} textures, {} samplers)###resources", resources.Textures().size(),
                                           resources.Samplers().size()).c_str())) {
    return;
  }
  if (ImGui::BeginTable("textures", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Handle");
    ImGui::TableSetupColumn("Size");
    ImGui::TableSetupColumn("Format");
    ImGui::TableSetupColumn("Status");
    ImGui::TableHeadersRow();
    for (const auto& texture : resources.Textures()) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(texture.name.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%ux%ux%u, %u mip(s)", texture.desc.texture.width, texture.desc.texture.height,
                  static_cast<uint32_t>(texture.desc.texture.depth_or_layers), static_cast<uint32_t>(texture.desc.texture.levels));
      ImGui::TableNextColumn();
      ImGui::Text("%u", static_cast<uint32_t>(texture.desc.texture.format));
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(texture.error.empty() ? "ok" : texture.error.c_str());
    }
    ImGui::EndTable();
  }
  std::string names;
  for (const auto& sampler : resources.Samplers()) names += (names.empty() ? "" : ", ") + sampler.name;
  ImGui::TextWrapped("Samplers: %s", names.c_str());
}

}  // namespace

void DrawOverlay(api::effect_runtime*) {
  Settings settings = *GetSettings();
  const Settings before = settings;
  const auto recipes = GetRecipes();
  if (!g_dirty && g_synced_from != recipes.get()) {
    g_pending = recipes->Overrides();
    g_synced_from = recipes.get();
  }

  ImGui::Text("DirectXShaderPatcher %s", DXP_VERSION_STRING);
  ImGui::Checkbox("Enabled", &settings.enabled);
  HelpMarker("Bind patched pipelines. Takes effect on the next time the game binds each pipeline.");

  if (ImGui::Button("Reload recipes & resources")) {
    ReloadAll();
    g_dirty = false;
  }
  ImGui::SameLine();
  if (ImGui::Button("Re-patch")) {
    ApplyEnvOverrides(g_pending);
    g_dirty = false;
  }

  ImGui::Checkbox("Apply env changes immediately", &settings.auto_apply_env);
  ImGui::Checkbox("Dump patched shaders", &settings.dump_shaders);
  HelpMarker("Writes original and patched bytecode of every shader a recipe modifies to dxp/dump. Takes effect on the next re-patch.");

  int level = static_cast<int>(settings.log_level);
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
  if (ImGui::Combo("DXP log level", &level, "error\0warning\0info\0debug\0trace\0")) settings.log_level = static_cast<dxp::LogLevel>(level);
  ImGui::TextDisabled("Folder: %s", settings.root.string().c_str());

  const auto devices = GetDeviceInfo();
  if (devices.empty()) ImGui::TextDisabled("No D3D11/D3D12 device yet.");
  for (const DeviceInfo& device : devices) {
    ImGui::SeparatorText(DeviceApiName(device.api));
    ImGui::Text("%zu pipelines: %zu patched, %zu pending, %zu failed, %zu unmodified", device.pipelines, device.patched, device.pending,
                device.failed, device.unmodified);
    if (device.api == api::device_api::d3d12) {
      const InjectionLayout& inj = device.injection;
      ImGui::Text("Root signatures extended: %u (skipped %u)", device.layouts_extended, device.layouts_skipped);
      HelpMarker("Recipe resources on D3D12 must be declared in these register ranges (set register_index and space in add_resource).");
      ImGui::TextDisabled("space%u: t%u-t%u, u%u-u%u, s%u-s%u", inj.space, inj.srv_base, inj.srv_base + inj.srv_count - 1, inj.uav_base,
                          inj.uav_base + inj.uav_count - 1, inj.sampler_base, inj.sampler_base + inj.sampler_count - 1);
    }
    if (!device.recent_errors.empty() && ImGui::TreeNode("Recent pipeline errors")) {
      for (const auto& error : device.recent_errors) ImGui::TextWrapped("%s", error.c_str());
      ImGui::TreePop();
    }
  }

  ImGui::Separator();
  const ResourceSet* resources = devices.empty() ? nullptr : devices.front().resources.get();
  DrawRecipes(*recipes, resources, settings.auto_apply_env);
  if (resources != nullptr) DrawResources(*resources);

  if (settings.enabled != before.enabled || settings.auto_apply_env != before.auto_apply_env ||
      settings.dump_shaders != before.dump_shaders || settings.log_level != before.log_level) {
    UpdateSettings(settings);
    SaveSettings(settings);
  }
}

}  // namespace dxp::addon
