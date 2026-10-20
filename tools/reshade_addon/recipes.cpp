#include "recipes.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace dxp::addon {

namespace {

std::atomic<uint64_t> g_next_generation{1};

std::optional<Stage> StageFromDirectory(std::string name) {
  std::ranges::transform(name, name.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  for (size_t i = 0; i < kStageCount; ++i) {
    if (name == StageName(static_cast<Stage>(i))) return static_cast<Stage>(i);
  }
  return std::nullopt;
}

bool IsRecipeFile(const std::filesystem::path& path) {
  std::string ext = path.extension().string();
  std::ranges::transform(ext, ext.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  return ext == ".yml" || ext == ".yaml";
}

bool IsResourceBinding(dxp::BindingClass binding_class) {
  switch (binding_class) {
    case dxp::BindingClass::Texture:
    case dxp::BindingClass::TextureUav:
    case dxp::BindingClass::RawResource:
    case dxp::BindingClass::StructuredResource:
    case dxp::BindingClass::CBuffer:
    case dxp::BindingClass::Sampler:
    case dxp::BindingClass::Uav:
      return true;
    default:
      return false;
  }
}

template <typename RecipeT>
void FillEntry(RecipeEntry& entry, std::expected<RecipeT, std::string> parsed, std::shared_ptr<const RecipeT>& slot) {
  if (!parsed) {
    entry.load_error = parsed.error();
    return;
  }
  if (auto valid = ValidateRecipe(*parsed); !valid) {
    entry.load_error = valid.error();
    return;
  }
  entry.version = parsed->GetVersion();
  entry.step_count = parsed->GetStepCount();
  entry.has_add_resource = parsed->HasAddResourceSteps();
  for (const auto& [key, value] : parsed->GetEnv()) entry.env_defaults[key] = value;
  entry.resources = DescribeDeclarations(parsed->GetDeclaredResources());
  for (const DeclaredResource& resource : entry.resources) {
    for (const std::string& warning : resource.hints.warnings) {
      Log(reshade::log::level::warning, "Recipe {}, resource '{}': {}", entry.name, resource.declaration.handle, warning);
    }
  }
  slot = std::make_shared<const RecipeT>(std::move(*parsed));
}

void LoadDirectory(const std::filesystem::path& recipe_dir, const std::filesystem::path& dir, ShaderModel model,
                   std::optional<Stage> stage, std::vector<RecipeEntry>& out) {
  std::error_code ec;
  for (const auto& item : std::filesystem::directory_iterator(dir, ec)) {
    if (item.is_directory()) {
      if (!stage) {
        if (const auto sub_stage = StageFromDirectory(item.path().filename().string())) {
          LoadDirectory(recipe_dir, item.path(), model, sub_stage, out);
        }
      }
      continue;
    }
    if (!item.is_regular_file() || !IsRecipeFile(item.path())) continue;

    RecipeEntry entry;
    entry.path = item.path();
    entry.name = std::filesystem::relative(item.path(), recipe_dir, ec).generic_string();
    entry.model = model;
    entry.stage = stage;
    const std::string path_string = item.path().string();
    if (model == ShaderModel::kSm5) {
      FillEntry(entry, dxp::sm5::Recipe::ParseFromFile(path_string), entry.sm5);
    } else {
#if DXP_ADDON_HAS_SM6
      FillEntry(entry, dxp::sm6::Recipe::ParseFromFile(path_string), entry.sm6);
#else
      entry.load_error = "SM6 recipes require the 64-bit add-on";
#endif
    }
    if (!entry.Loaded()) {
      Log(reshade::log::level::error, "Recipe {} failed to load: {}", entry.name, entry.load_error);
    }
    out.push_back(std::move(entry));
  }
}

void DumpShader(const Settings& settings, uint64_t hash, Stage stage, std::string_view suffix, std::span<const uint8_t> code) {
  std::error_code ec;
  std::filesystem::create_directories(settings.DumpDir(), ec);
  const auto path = settings.DumpDir() / std::format("{:016x}.{}.{}.cso", hash, StageName(stage), suffix);
  std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(code.data()), static_cast<std::streamsize>(code.size()));
}

}  // namespace

std::optional<ShaderModel> DetectShaderModel(std::span<const uint8_t> code) {
  if (code.size() < 32 || std::memcmp(code.data(), "DXBC", 4) != 0) return std::nullopt;
  uint32_t chunk_count = 0;
  std::memcpy(&chunk_count, code.data() + 28, 4);
  if (code.size() < 32 + static_cast<size_t>(chunk_count) * 4) return std::nullopt;
  std::optional<ShaderModel> model;
  for (uint32_t i = 0; i < chunk_count; ++i) {
    uint32_t offset = 0;
    std::memcpy(&offset, code.data() + 32 + i * 4, 4);
    if (offset + 4 > code.size()) return std::nullopt;
    const auto* four_cc = code.data() + offset;
    if (std::memcmp(four_cc, "DXIL", 4) == 0) return ShaderModel::kSm6;
    if (std::memcmp(four_cc, "SHEX", 4) == 0 || std::memcmp(four_cc, "SHDR", 4) == 0) model = ShaderModel::kSm5;
  }
  return model;
}

uint64_t HashShader(std::span<const uint8_t> code) {
  // DXBC/DXIL containers carry a 128-bit digest at offset 4. Unsigned DXIL leaves it zeroed.
  if (code.size() >= 20 && std::memcmp(code.data(), "DXBC", 4) == 0) {
    uint64_t lo = 0;
    uint64_t hi = 0;
    std::memcpy(&lo, code.data() + 4, 8);
    std::memcpy(&hi, code.data() + 12, 8);
    if ((lo | hi) != 0) return lo ^ (hi * 0x9E3779B97F4A7C15ull) ^ code.size();
  }
  uint64_t hash = 14695981039346656037ull;
  for (const uint8_t byte : code) hash = (hash ^ byte) * 1099511628211ull;
  return hash;
}

std::shared_ptr<RecipeSet> RecipeSet::Load(const std::filesystem::path& recipe_dir,
                                           const std::unordered_map<std::string, EnvMap>& overrides) {
  auto set = std::shared_ptr<RecipeSet>(new RecipeSet());
  std::error_code ec;
  for (const auto& item : std::filesystem::directory_iterator(recipe_dir, ec)) {
    if (!item.is_directory()) continue;
    std::string name = item.path().filename().string();
    std::ranges::transform(name, name.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    if (name == "sm5") LoadDirectory(recipe_dir, item.path(), ShaderModel::kSm5, std::nullopt, set->entries_);
    if (name == "sm6") LoadDirectory(recipe_dir, item.path(), ShaderModel::kSm6, std::nullopt, set->entries_);
  }
  std::ranges::sort(set->entries_, {}, &RecipeEntry::name);
  set->overrides_ = overrides;
  set->Finalize();
  Log(reshade::log::level::info, "Loaded {} recipe(s) from {}", set->entries_.size(), recipe_dir.string());
  return set;
}

std::shared_ptr<RecipeSet> RecipeSet::WithOverrides(const std::unordered_map<std::string, EnvMap>& overrides) const {
  auto set = std::shared_ptr<RecipeSet>(new RecipeSet());
  set->entries_ = entries_;  // parsed recipes are shared, not re-parsed
  set->overrides_ = overrides;
  set->Finalize();
  return set;
}

void RecipeSet::Finalize() {
  generation_ = g_next_generation.fetch_add(1);
  stats_ = std::make_unique<RecipeStats[]>(entries_.size());
  any_add_resource_ = std::ranges::any_of(entries_, [](const RecipeEntry& e) { return e.Loaded() && e.has_add_resource; });

  // Drop overrides for variables the recipes no longer declare.
  for (auto it = overrides_.begin(); it != overrides_.end();) {
    const auto entry = std::ranges::find(entries_, it->first, &RecipeEntry::name);
    if (entry == entries_.end()) {
      it = overrides_.erase(it);
      continue;
    }
    std::erase_if(it->second, [&](const auto& kv) { return !entry->env_defaults.contains(kv.first); });
    it = it->second.empty() ? overrides_.erase(it) : std::next(it);
  }
}

bool RecipeSet::AppliesTo(ShaderModel model, Stage stage) const {
  return std::ranges::any_of(entries_, [&](const RecipeEntry& e) {
    return e.Loaded() && e.model == model && (!e.stage || *e.stage == stage);
  });
}

std::shared_ptr<const PatchedShader> RecipeSet::Patch(ShaderModel model, Stage stage, uint64_t hash,
                                                      std::span<const uint8_t> code, const Settings& settings) const {
  const uint64_t key = hash * 31 + static_cast<uint64_t>(stage);
  {
    std::lock_guard lock(cache_mutex_);
    if (const auto it = cache_.find(key); it != cache_.end()) return it->second;
  }

  std::vector<uint8_t> current(code.begin(), code.end());
  auto patched = std::make_shared<PatchedShader>();

  for (size_t i = 0; i < entries_.size(); ++i) {
    const RecipeEntry& entry = entries_[i];
    if (!entry.Loaded() || entry.model != model || (entry.stage && *entry.stage != stage)) continue;

    dxp::PatchOptions options;
    options.log_level = settings.log_level;
    // Errors are reported once below from the returned expected.
    options.logger = [&](dxp::LogLevel level, const std::string& message) {
      if (level != dxp::LogLevel::Error) {
        Log(ToReShadeLevel(level), "{} [{:016x}.{}]: {}", entry.name, hash, StageName(stage), message);
      }
    };
    if (const auto it = overrides_.find(entry.name); it != overrides_.end()) {
      for (const auto& [variable, value] : it->second) options.SetEnv(variable, value);
    }

    std::expected<dxp::RecipeReport, std::string> result = std::unexpected("recipe not loaded");
    if (entry.sm5) result = entry.sm5->Execute(current, options);
#if DXP_ADDON_HAS_SM6
    if (entry.sm6) result = entry.sm6->Execute(current, options);
#endif

    RecipeStats& stats = stats_[i];
    if (!result) {
      // Only the first few failures per recipe reach the log; the overlay keeps the latest.
      if (stats.failed.fetch_add(1) < 3) {
        Log(reshade::log::level::warning, "{} failed on {:016x}.{}: {}", entry.name, hash, StageName(stage), result.error());
      }
      std::lock_guard lock(stats.mutex);
      stats.last_error = std::format("{:016x}.{}: {}", hash, StageName(stage), result.error());
      continue;
    }
    if (!result->modified) continue;

    stats.patched.fetch_add(1);
    current = std::move(result->output_bytes);
    patched->recipes.push_back(entry.name);
    for (const auto& [handle, binding] : result->new_bindings) {
      if (!IsResourceBinding(binding.binding_class)) continue;
      std::erase_if(patched->bindings, [&](const dxp::ResourceBinding& b) { return b.handle == binding.handle; });
      patched->bindings.push_back(binding);
    }
  }

  std::shared_ptr<const PatchedShader> output;
  if (!patched->recipes.empty()) {
    patched->code = std::move(current);
    if (settings.dump_shaders) {
      DumpShader(settings, hash, stage, "original", code);
      DumpShader(settings, hash, stage, "patched", patched->code);
    }
    output = std::move(patched);
  }

  std::lock_guard lock(cache_mutex_);
  return cache_.try_emplace(key, std::move(output)).first->second;
}

}  // namespace dxp::addon
