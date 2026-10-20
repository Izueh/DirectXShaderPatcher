#include "resources.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <mutex>
#include <shared_mutex>

#include "dds.hpp"

namespace dxp::addon {

namespace {

std::shared_mutex g_provider_mutex;
std::unordered_map<std::string, std::shared_ptr<const BindingProvider>> g_providers;

std::string Lower(std::string text) {
  std::ranges::transform(text, text.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  return text;
}

std::string Trim(std::string_view text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) return {};
  const auto end = text.find_last_not_of(" \t\r\n");
  return std::string(text.substr(begin, end - begin + 1));
}

api::descriptor_type DescriptorTypeFor(dxp::BindingClass binding_class) {
  switch (binding_class) {
    case dxp::BindingClass::Sampler:    return api::descriptor_type::sampler;
    case dxp::BindingClass::Uav:
    case dxp::BindingClass::TextureUav: return api::descriptor_type::unordered_access_view;
    case dxp::BindingClass::CBuffer:    return api::descriptor_type::constant_buffer;
    default:                            return api::descriptor_type::shader_resource_view;
  }
}

std::optional<api::filter_mode> ParseFilter(const std::string& value) {
  if (value == "point") return api::filter_mode::min_mag_mip_point;
  if (value == "linear" || value == "trilinear") return api::filter_mode::min_mag_mip_linear;
  if (value == "bilinear") return api::filter_mode::min_mag_linear_mip_point;
  if (value == "aniso" || value == "anisotropic") return api::filter_mode::anisotropic;
  return std::nullopt;
}

std::optional<api::texture_address_mode> ParseAddress(const std::string& value) {
  if (value == "wrap" || value == "repeat") return api::texture_address_mode::wrap;
  if (value == "clamp") return api::texture_address_mode::clamp;
  if (value == "mirror") return api::texture_address_mode::mirror;
  if (value == "border") return api::texture_address_mode::border;
  if (value == "mirror_once") return api::texture_address_mode::mirror_once;
  return std::nullopt;
}

std::optional<api::compare_op> ParseCompare(const std::string& value) {
  constexpr std::pair<const char*, api::compare_op> kOps[] = {
      {"never", api::compare_op::never}, {"less", api::compare_op::less}, {"equal", api::compare_op::equal},
      {"less_equal", api::compare_op::less_equal}, {"greater", api::compare_op::greater},
      {"not_equal", api::compare_op::not_equal}, {"greater_equal", api::compare_op::greater_equal},
      {"always", api::compare_op::always}};
  for (const auto& [name, op] : kOps) {
    if (value == name) return op;
  }
  return std::nullopt;
}

float ParseFloat(const std::string& text, float fallback) {
  float value = fallback;
  std::from_chars(text.data(), text.data() + text.size(), value);
  return value;
}

api::sampler_desc MakeSampler(api::filter_mode filter, api::texture_address_mode address) {
  api::sampler_desc desc;
  desc.filter = filter;
  desc.address_u = desc.address_v = desc.address_w = address;
  desc.max_anisotropy = filter == api::filter_mode::anisotropic ? 16.0f : 1.0f;
  return desc;
}

}  // namespace

void RegisterBindingProvider(const std::string& handle, BindingProvider provider) {
  std::unique_lock lock(g_provider_mutex);
  g_providers[handle] = std::make_shared<const BindingProvider>(std::move(provider));
}

void UnregisterBindingProvider(const std::string& handle) {
  std::unique_lock lock(g_provider_mutex);
  g_providers.erase(handle);
}

bool HasBindingProvider(const std::string& handle) {
  std::shared_lock lock(g_provider_mutex);
  return g_providers.contains(handle);
}

std::shared_ptr<ResourceSet> ResourceSet::Load(api::device* device, const std::filesystem::path& directory) {
  auto set = std::shared_ptr<ResourceSet>(new ResourceSet(device));
  set->LoadTextures(directory);
  set->LoadSamplers(directory);
  Log(reshade::log::level::info, "Loaded {} texture(s) and {} sampler(s) from {}", set->textures_.size(),
      set->samplers_.size(), directory.string());
  return set;
}

ResourceSet::~ResourceSet() {
  for (const TextureInfo& texture : textures_) {
    if (texture.srv != 0) device_->destroy_resource_view(texture.srv);
    if (texture.resource != 0) device_->destroy_resource(texture.resource);
  }
  for (const SamplerInfo& sampler : samplers_) {
    if (sampler.sampler != 0) device_->destroy_sampler(sampler.sampler);
  }
}

void ResourceSet::LoadTextures(const std::filesystem::path& directory) {
  std::error_code ec;
  for (const auto& item : std::filesystem::recursive_directory_iterator(directory, ec)) {
    if (!item.is_regular_file() || Lower(item.path().extension().string()) != ".dds") continue;

    TextureInfo info;
    info.name = std::filesystem::relative(item.path(), directory, ec).generic_string();
    auto image = LoadDds(item.path());
    if (!image) {
      info.error = image.error();
    } else {
      info.desc = image->desc;
      info.view_type = image->view_type;
      if (!device_->create_resource(image->desc, image->subresources.data(), api::resource_usage::shader_resource, &info.resource)) {
        info.error = "create_resource failed (format unsupported by the device?)";
      } else {
        const auto view_desc = api::resource_view_desc(image->view_type, api::format_to_default_typed(image->desc.texture.format), 0,
                                                       image->desc.texture.levels, 0, image->desc.texture.depth_or_layers);
        if (!device_->create_resource_view(info.resource, api::resource_usage::shader_resource, view_desc, &info.srv)) {
          info.error = "create_resource_view failed";
        }
      }
    }
    if (!info.error.empty()) Log(reshade::log::level::warning, "Texture {}: {}", info.name, info.error);

    const size_t index = textures_.size();
    const std::filesystem::path relative(info.name);
    for (const std::string& key : {Lower(info.name), Lower(relative.filename().string()), Lower(relative.stem().string()),
                                   Lower((relative.parent_path() / relative.stem()).generic_string())}) {
      texture_lookup_.try_emplace(key, index);
    }
    textures_.push_back(std::move(info));
  }
}

void ResourceSet::LoadSamplers(const std::filesystem::path& directory) {
  // Built-in "<filter>_<address>" samplers used for name-based lookup.
  for (const char* filter : {"point", "linear", "aniso"}) {
    for (const char* address : {"wrap", "clamp", "mirror", "border"}) {
      samplers_.push_back({std::format("{}_{}", filter, address), MakeSampler(*ParseFilter(filter), *ParseAddress(address))});
    }
  }

  std::ifstream file(directory / "samplers.ini");
  SamplerInfo* current = nullptr;
  std::string line;
  while (file && std::getline(file, line)) {
    line = Trim(line.substr(0, line.find_first_of(";#")));
    if (line.empty()) continue;
    if (line.front() == '[' && line.back() == ']') {
      samplers_.push_back({Trim(line.substr(1, line.size() - 2)), MakeSampler(api::filter_mode::min_mag_mip_linear, api::texture_address_mode::clamp),
                           {0}, true});
      current = &samplers_.back();
      continue;
    }
    const auto equals = line.find('=');
    if (current == nullptr || equals == std::string::npos) continue;
    const std::string key = Lower(Trim(line.substr(0, equals)));
    const std::string value = Lower(Trim(line.substr(equals + 1)));
    api::sampler_desc& desc = current->desc;
    if (key == "filter") {
      if (const auto filter = ParseFilter(value)) desc.filter = *filter;
    } else if (key == "address" || key == "address_u" || key == "address_v" || key == "address_w") {
      if (const auto address = ParseAddress(value)) {
        if (key == "address" || key == "address_u") desc.address_u = *address;
        if (key == "address" || key == "address_v") desc.address_v = *address;
        if (key == "address" || key == "address_w") desc.address_w = *address;
      }
    } else if (key == "compare") {
      if (const auto op = ParseCompare(value)) desc.compare_op = *op;
    } else if (key == "max_anisotropy") {
      desc.max_anisotropy = ParseFloat(value, desc.max_anisotropy);
    } else if (key == "mip_lod_bias") {
      desc.mip_lod_bias = ParseFloat(value, desc.mip_lod_bias);
    } else if (key == "min_lod") {
      desc.min_lod = ParseFloat(value, desc.min_lod);
    } else if (key == "max_lod") {
      desc.max_lod = ParseFloat(value, desc.max_lod);
    } else if (key == "border_color") {
      size_t start = 0;
      for (float& channel : desc.border_color) {
        const size_t comma = value.find(',', start);
        channel = ParseFloat(Trim(value.substr(start, comma - start)), channel);
        if (comma == std::string::npos) break;
        start = comma + 1;
      }
    }
  }

  for (size_t i = 0; i < samplers_.size(); ++i) {
    SamplerInfo& info = samplers_[i];
    if (info.desc.compare_op != api::compare_op::never) {
      // Comparison filters share the encoding of the plain ones with bit 0x80 set.
      info.desc.filter = static_cast<api::filter_mode>(static_cast<uint32_t>(info.desc.filter) | 0x80);
    }
    if (!device_->create_sampler(info.desc, &info.sampler)) {
      Log(reshade::log::level::warning, "Sampler {}: create_sampler failed", info.name);
    }
    // Later samplers.ini sections override built-ins of the same name.
    sampler_lookup_[Lower(info.name)] = i;
  }
}

const ResourceSet::TextureInfo* ResourceSet::FindTexture(const std::string& handle) const {
  const auto it = texture_lookup_.find(Lower(handle));
  return it != texture_lookup_.end() ? &textures_[it->second] : nullptr;
}

const ResourceSet::SamplerInfo* ResourceSet::FindSampler(const std::string& handle) const {
  const std::string lower = Lower(handle);
  if (const auto it = sampler_lookup_.find(lower); it != sampler_lookup_.end()) return &samplers_[it->second];

  // Derive from tokens in the handle, e.g. "s_noise_point_wrap" -> point_wrap.
  std::string filter = "linear";
  std::string address = "clamp";
  size_t start = 0;
  while (start <= lower.size()) {
    size_t end = start;
    while (end < lower.size() && std::isalnum(static_cast<unsigned char>(lower[end]))) ++end;
    const std::string token = lower.substr(start, end - start);
    if (token == "point" || token == "linear" || token == "aniso" || token == "anisotropic") filter = token == "anisotropic" ? "aniso" : token;
    if (token == "repeat") address = "wrap";
    if (token == "wrap" || token == "clamp" || token == "mirror" || token == "border") address = token;
    start = end + 1;
  }
  const auto it = sampler_lookup_.find(filter + "_" + address);
  return it != sampler_lookup_.end() ? &samplers_[it->second] : nullptr;
}

bool ResourceSet::Resolve(const dxp::ResourceBinding& binding, ResolvedBinding& out, std::string& error) const {
  out = {};
  out.binding = binding;
  out.type = DescriptorTypeFor(binding.binding_class);

  {
    std::shared_lock lock(g_provider_mutex);
    if (const auto it = g_providers.find(binding.handle); it != g_providers.end()) {
      out.provider = it->second;
      return true;
    }
  }

  switch (binding.binding_class) {
    case dxp::BindingClass::Texture: {
      const TextureInfo* found = FindTexture(binding.handle);
      if (found == nullptr) {
        error = std::format("texture '{}' not found in resources/ (expected a .dds file)", binding.handle);
        return false;
      }
      const TextureInfo& texture = *found;
      if (texture.srv == 0) {
        error = std::format("texture '{}' failed to load: {}", binding.handle, texture.error);
        return false;
      }
      out.descriptor = texture.srv.handle;
      return true;
    }
    case dxp::BindingClass::Sampler: {
      const SamplerInfo* sampler = FindSampler(binding.handle);
      if (sampler == nullptr || sampler->sampler == 0) {
        error = std::format("sampler '{}' could not be created", binding.handle);
        return false;
      }
      out.descriptor = sampler->sampler.handle;
      return true;
    }
    case dxp::BindingClass::CBuffer:
      error = std::format("constant buffer '{}' is not supported by the add-on", binding.handle);
      return false;
    default:
      error = std::format("'{}' needs a binding provider (only textures and samplers load from resources/)", binding.handle);
      return false;
  }
}

}  // namespace dxp::addon
