// Tests the ReShade add-on's recipe, patching, DDS and draw-state modules without ReShade.
//
// reshade.hpp locates ReShade by searching the process for a module exporting
// "ReShadeRegisterAddon", so this executable exports the few entry points those
// modules use (logging and config) and stands in for ReShade.

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "config.hpp"
#include "dds.hpp"
#include "draw_state.hpp"
#include "recipes.hpp"
#include "requirements.hpp"

namespace {
std::map<std::string, std::string> g_config;
}

extern "C" __declspec(dllexport) bool ReShadeRegisterAddon(void*, uint32_t) { return true; }

extern "C" __declspec(dllexport) void ReShadeLogMessage(void*, int level, const char* message) {
  std::cout << "  [log " << level << "] " << message << '\n';
}

extern "C" __declspec(dllexport) bool ReShadeGetConfigValue(void*, reshade::api::effect_runtime*, const char* section, const char* key,
                                                            char* value, size_t* value_size) {
  const auto it = g_config.find(std::string(section) + "/" + key);
  if (it == g_config.end()) return false;
  if (value != nullptr && *value_size > it->second.size()) std::memcpy(value, it->second.c_str(), it->second.size() + 1);
  *value_size = it->second.size() + 1;
  return true;
}

extern "C" __declspec(dllexport) void ReShadeSetConfigValue(void*, reshade::api::effect_runtime*, const char* section, const char* key,
                                                           const char* value) {
  g_config[std::string(section) + "/" + key] = value;
}

namespace {

using namespace dxp::addon;
namespace fs = std::filesystem;

int g_failures = 0;

void Check(bool condition, const std::string& what) {
  std::cout << (condition ? "ok   " : "FAIL ") << what << '\n';
  if (!condition) ++g_failures;
}

std::vector<uint8_t> ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void WriteText(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream(path) << text;
}

template <typename T>
void Append(std::vector<uint8_t>& out, const T& value) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(T));
}

/// Builds a DDS file: 124-byte header with the given pixel format, optional DX10 header, payload.
std::vector<uint8_t> MakeDds(uint32_t width, uint32_t height, uint32_t depth, uint32_t mips, uint32_t pf_flags, uint32_t four_cc,
                             uint32_t bit_count, const std::array<uint32_t, 4>& masks, uint32_t caps2, const std::vector<uint32_t>& dx10,
                             size_t payload) {
  std::vector<uint8_t> out = {'D', 'D', 'S', ' '};
  std::array<uint32_t, 31> header{};
  header[0] = 124;
  header[1] = 0x1 | 0x2 | 0x4 | 0x1000 | (mips > 1 ? 0x20000 : 0) | (depth > 1 ? 0x800000 : 0);
  header[2] = height;
  header[3] = width;
  header[5] = depth;
  header[6] = mips;
  header[18] = 32;  // pixel format
  header[19] = pf_flags;
  header[20] = four_cc;
  header[21] = bit_count;
  for (size_t i = 0; i < 4; ++i) header[22 + i] = masks[i];
  header[27] = caps2;
  for (const uint32_t value : header) Append(out, value);
  for (const uint32_t value : dx10) Append(out, value);
  out.resize(out.size() + payload, 0x7f);
  return out;
}

void TestDds(const fs::path& dir) {
  const auto write = [&](const char* name, const std::vector<uint8_t>& data) {
    const fs::path path = dir / name;
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return path;
  };
  constexpr uint32_t kFourCC = 0x4;
  constexpr uint32_t kRgb = 0x40 | 0x1;

  // DX10 RGBA8 2D, 4x4 with 3 mips: 64 + 16 + 4 bytes.
  {
    const auto path = write("rgba.dds", MakeDds(4, 4, 1, 3, kFourCC, '01XD', 0, {}, 0, {28, 3, 0, 1, 0}, 84));
    auto image = LoadDds(path);
    Check(image.has_value(), "DX10 RGBA8 loads");
    if (image) {
      Check(image->desc.texture.levels == 3 && image->subresources.size() == 3, "DX10 RGBA8 has 3 mips");
      Check(image->subresources[0].row_pitch == 16 && image->subresources[2].row_pitch == 4, "DX10 RGBA8 row pitches");
      Check(image->view_type == reshade::api::resource_view_type::texture_2d, "DX10 RGBA8 view type");
    }
  }
  // Legacy DXT1 8x8, 1 mip: 2x2 blocks * 8 bytes.
  {
    const auto path = write("bc1.dds", MakeDds(8, 8, 1, 1, kFourCC, '1TXD', 0, {}, 0, {}, 32));
    auto image = LoadDds(path);
    Check(image.has_value() && image->desc.texture.format == reshade::api::format::bc1_unorm, "legacy DXT1 maps to BC1");
    Check(image.has_value() && image->subresources[0].row_pitch == 16, "BC1 row pitch is per block row");
  }
  // Legacy BGRA8 volume 4x4x4: 256 bytes.
  {
    const auto path = write("volume.dds", MakeDds(4, 4, 4, 1, kRgb, 0, 32, {0xff0000, 0xff00, 0xff, 0xff000000}, 0x200000, {}, 256));
    auto image = LoadDds(path);
    Check(image.has_value() && image->desc.type == reshade::api::resource_type::texture_3d, "legacy BGRA8 volume is 3D");
    Check(image.has_value() && image->desc.texture.format == reshade::api::format::b8g8r8a8_unorm, "legacy BGRA8 format");
    Check(image.has_value() && image->subresources[0].slice_pitch == 64, "volume slice pitch");
  }
  // Truncated payload must be rejected, not read out of bounds.
  {
    const auto path = write("short.dds", MakeDds(4, 4, 1, 3, kFourCC, '01XD', 0, {}, 0, {28, 3, 0, 1, 0}, 40));
    Check(!LoadDds(path).has_value(), "truncated DDS is rejected");
  }
}

void TestRecipes(const fs::path& source_root, const fs::path& dir) {
  const fs::path examples = source_root / "tools/reshade_addon/example/dxp/recipes";
  const fs::path recipes = dir / "recipes";
  fs::create_directories(recipes / "sm6/ps");
  fs::create_directories(recipes / "sm5/ps");
  fs::copy_file(examples / "sm6/ps/replace_noise_srv.recipe.yml.example", recipes / "sm6/ps/replace_noise_srv.recipe.yml",
                fs::copy_options::overwrite_existing);
  fs::copy_file(examples / "sm5/ps/ign_to_blue_noise.recipe.yml.example", recipes / "sm5/ps/ign_to_blue_noise.recipe.yml",
                fs::copy_options::overwrite_existing);
  WriteText(recipes / "sm6/broken.recipe.yml", "steps: [ {kind: not_a_step} ]\n");
  WriteText(recipes / "sm6/ignored/skipped.recipe.yml", "steps: []\n");  // unknown stage folder

  Settings settings;
  settings.root = dir;
  auto set = RecipeSet::Load(recipes, {});
  Check(set->Entries().size() == 3, "three recipes found (unknown stage folder ignored)");

  const RecipeEntry* sm6 = nullptr;
  const RecipeEntry* broken = nullptr;
  for (const auto& entry : set->Entries()) {
    if (entry.name == "sm6/ps/replace_noise_srv.recipe.yml") sm6 = &entry;
    if (entry.name == "sm6/broken.recipe.yml") broken = &entry;
  }
  Check(broken != nullptr && !broken->Loaded(), "broken recipe reports a load error");
  Check(sm6 != nullptr && sm6->Loaded() && sm6->stage == Stage::kPixel, "SM6 recipe loads with the pixel stage");
  Check(sm6 != nullptr && sm6->env_defaults.contains("noise_mip"), "env defaults come from Recipe::GetEnv");
  Check(set->AnyAddResource(), "add_resource is detected");
  Check(set->AppliesTo(ShaderModel::kSm6, Stage::kPixel) && !set->AppliesTo(ShaderModel::kSm6, Stage::kCompute),
        "stage folders filter recipes");

  const auto dxil = ReadFile(source_root / "playground/dither_ps_6_6.cso");
  Check(DetectShaderModel(dxil) == ShaderModel::kSm6, "DXIL container detected");
  const uint64_t hash = HashShader(dxil);
  auto patched = set->Patch(ShaderModel::kSm6, Stage::kPixel, hash, dxil, settings);
  Check(patched != nullptr && patched->code != dxil, "SM6 pixel shader is patched");
  if (patched) {
    Check(patched->bindings.size() == 1 && patched->bindings[0].handle == "noise" && patched->bindings[0].space == 100 &&
              patched->bindings[0].register_index == 0 && patched->bindings[0].binding_class == dxp::BindingClass::Texture,
          "SM6 binding is the noise texture at t0 space100");
  }
  Check(set->Patch(ShaderModel::kSm6, Stage::kPixel, hash, dxil, settings) == patched, "patch results are cached per generation");

  // Env override: a different mip constant must change the output, and the set must be a new generation.
  auto overridden = set->WithOverrides({{"sm6/ps/replace_noise_srv.recipe.yml", {{"noise_mip", dxp::PrimitiveValue(int64_t{3})}}}});
  Check(overridden->Generation() != set->Generation(), "env overrides publish a new generation");
  auto patched_mip3 = overridden->Patch(ShaderModel::kSm6, Stage::kPixel, hash, dxil, settings);
  Check(patched && patched_mip3 && patched_mip3->code != patched->code, "env override changes the patched shader");
  Check(overridden->WithOverrides({{"sm6/ps/replace_noise_srv.recipe.yml", {{"unknown", dxp::PrimitiveValue(true)}}}})->Overrides().empty(),
        "overrides for undeclared variables are dropped");

  // Config round trip of an override value, parsed with the default's type.
  SaveEnvOverride("r.yml", "noise_mip", dxp::PrimitiveValue(int64_t{7}));
  const auto loaded = LoadEnvOverride("r.yml", "noise_mip", dxp::PrimitiveValue(int64_t{0}));
  Check(loaded && std::get<int64_t>(*loaded) == 7, "env override round-trips through the config");

  // SM5: at least one test shader contains interleaved gradient noise.
  bool sm5_patched = false;
  for (const auto& item : fs::directory_iterator(source_root / "tests/shaders")) {
    if (item.path().string().find("ps_5_0.cso") == std::string::npos) continue;
    const auto dxbc = ReadFile(item.path());
    if (DetectShaderModel(dxbc) != ShaderModel::kSm5) continue;
    auto result = set->Patch(ShaderModel::kSm5, Stage::kPixel, HashShader(dxbc), dxbc, settings);
    if (!result) continue;
    sm5_patched = true;
    bool texture = false;
    bool sampler = false;
    for (const auto& binding : result->bindings) {
      texture |= binding.handle == "blue_noise" && binding.binding_class == dxp::BindingClass::Texture;
      sampler |= binding.handle == "point_wrap" && binding.binding_class == dxp::BindingClass::Sampler;
    }
    Check(texture && sampler, "SM5 bindings report the texture and sampler (" + item.path().filename().string() + ")");
  }
  Check(sm5_patched, "an SM5 pixel shader is patched");

  // Declarations are collected at load time, for both shader models.
  Check(sm6 != nullptr && sm6->resources.size() == 1 && sm6->resources[0].declaration.handle == "noise" &&
            sm6->resources[0].declaration.binding_class == dxp::BindingClass::Texture &&
            sm6->resources[0].declaration.type.dimension == dxp::ResourceDimension::Texture2D &&
            !sm6->resources[0].declaration.conditional && DescribeSlot(sm6->resources[0].declaration) == "t0, space100",
        "SM6 entry stores its declared noise texture");
  const auto sm5 = std::ranges::find(set->Entries(), std::string("sm5/ps/ign_to_blue_noise.recipe.yml"), &RecipeEntry::name);
  Check(sm5 != set->Entries().end() && sm5->resources.size() == 2 && sm5->resources[0].declaration.handle == "blue_noise" &&
            sm5->resources[1].declaration.binding_class == dxp::BindingClass::Sampler && DescribeSlot(sm5->resources[1].declaration) == "auto",
        "SM5 entry stores its declared texture and sampler");
  Check(broken != nullptr && broken->resources.empty(), "a broken recipe has no declarations");
}

void TestRequirements(const fs::path& dir) {
  const fs::path recipes = dir / "requirements";
  WriteText(recipes / "sm6/cs/outputs.recipe.yml", R"YAML(
steps:
  - kind: add_resource
    name: add_inputs
    textures:
      - handle: hdr_scene
        kind: Texture2D
        element_type: F32
        vector_width: 4
        space: 100
        register_index: 0
        meta:
          description: Scene color before tonemapping
          format: r16g16b16a16_float
          size: full
          size_of: render_target
          clear: [0, 0, 0, 1]
          reshade: { group: Lighting }
      - handle: volume
        kind: Texture3D
  - kind: add_resource
    name: add_optional_output
    required: false
    uavs:
      - handle: exposure
        kind: Texture2D
        element_type: F32
        vector_width: 1
        meta: { size: huge, width: 1, height: 1, colour: red }
)YAML");
  WriteText(recipes / "sm5/ps/params.recipe.yml", R"YAML(
steps:
  - kind: add_resource
    name: add_params
    cbuffers:
      - handle: params
        meta: { description: Grading parameters }
        fields:
          - { name: strength, type: F32, default: 0.25 }
)YAML");

  auto set = RecipeSet::Load(recipes, {});
  Check(set->Entries().size() == 2 && std::ranges::all_of(set->Entries(), &RecipeEntry::Loaded), "requirement recipes load");
  const auto find = [&](const std::string& handle) -> const DeclaredResource* {
    for (const auto& entry : set->Entries()) {
      for (const auto& resource : entry.resources) {
        if (resource.declaration.handle == handle) return &resource;
      }
    }
    return nullptr;
  };

  const DeclaredResource* hdr = find("hdr_scene");
  Check(hdr != nullptr && DescribeType(hdr->declaration.type) == "Texture2D, F32x4", "declared type is described");
  if (hdr != nullptr) {
    const ResourceHints& hints = hdr->hints;
    Check(hints.description == "Scene color before tonemapping", "description hint");
    Check(hints.format == reshade::api::format::r16g16b16a16_float && hints.format_name == "r16g16b16a16_float", "format hint");
    Check(hints.size == SizeMode::kFull && hints.size_of == SizeReference::kRenderTarget, "size hints");
    Check(hints.clear == std::array<float, 4>{0, 0, 0, 1}, "clear hint");
    Check(hints.warnings.empty(), "valid hints and namespaced keys produce no warnings");
  }

  const DeclaredResource* exposure = find("exposure");
  Check(exposure != nullptr && exposure->declaration.conditional && exposure->declaration.binding_class == dxp::BindingClass::TextureUav &&
            DescribeSlot(exposure->declaration) == "auto",
        "required: false marks the UAV conditional");
  if (exposure != nullptr) {
    const auto& warnings = exposure->hints.warnings;
    const auto mentions = [&](std::string_view text) {
      return std::ranges::any_of(warnings, [&](const std::string& w) { return w.find(text) != std::string::npos; });
    };
    Check(!exposure->hints.size && mentions("'huge'"), "invalid size is a warning, not a value");
    Check(exposure->hints.width == 1u && mentions("only apply with size: fixed"), "width without size: fixed is a warning");
    Check(mentions("meta.colour: unknown key"), "unknown keys are warnings");
  }

  const DeclaredResource* params = find("params");
  Check(params != nullptr && params->declaration.binding_class == dxp::BindingClass::CBuffer &&
            params->hints.description == "Grading parameters" && DescribeType(params->declaration.type) == "16 bytes, 1 field(s)",
        "SM5 cbuffer declaration and description");

  // Hints parsed directly from MetaValue.
  using Meta = dxp::MetaValue;
  const auto object = [](Meta::Object members) { return std::optional<Meta>(Meta{std::move(members)}); };
  Check(ParseResourceHints(std::nullopt).warnings.empty(), "no meta, no hints");
  Check(ParseResourceHints(Meta{std::string("text")}).warnings.size() == 1, "non-map meta is a warning");
  {
    const auto hints = ParseResourceHints(object({{"size", Meta{std::string("Fixed")}}, {"width", Meta{64.0}}, {"height", Meta{32.0}},
                                                  {"format", Meta{std::string("DXGI_FORMAT_R8G8B8A8_UNORM")}}, {"clear", Meta{0.5}}}));
    Check(hints.size == SizeMode::kFixed && hints.width == 64u && hints.height == 32u && hints.warnings.empty(), "fixed size hints");
    Check(hints.format == reshade::api::format::r8g8b8a8_unorm, "DXGI_FORMAT_ prefixed names parse");
    Check(hints.clear == std::array<float, 4>{0.5f, 0.5f, 0.5f, 0.5f}, "scalar clear is broadcast");
  }
  {
    const auto hints = ParseResourceHints(object({{"size", Meta{std::string("fixed")}}, {"width", Meta{-4.0}},
                                                  {"format", Meta{std::string("r99_float")}}, {"clear", Meta{Meta::Array(5, Meta{1.0})}},
                                                  {"size_of", Meta{std::string("screen")}}}));
    Check(!hints.width && !hints.format && !hints.clear && !hints.size_of && hints.format_name == "r99_float", "invalid values are dropped");
    Check(hints.warnings.size() == 5, "each invalid value warns, plus fixed without width/height");
  }

  // DDS dimension against declared dimension.
  const auto volume = LoadDds(dir / "volume.dds");
  Check(volume && !DimensionMatches(dxp::ResourceDimension::Texture2D, volume->view_type) &&
            DimensionMatches(dxp::ResourceDimension::Texture3D, volume->view_type),
        "a 3D DDS matches Texture3D only");
  const auto flat = LoadDds(dir / "rgba.dds");
  Check(flat && !DimensionMatches(dxp::ResourceDimension::Texture3D, flat->view_type) && DimensionMatches(std::nullopt, flat->view_type),
        "a 2D DDS mismatches Texture3D; undeclared dimensions match");
}

void TestSwap() {
  bool swapped = false;
  Check(NextSwapAction(false, swapped) == SwapAction::kNone && !swapped, "unresolved bindings leave the game's pipeline alone");
  Check(NextSwapAction(true, swapped) == SwapAction::kBindReplacement && swapped, "first ready draw binds the replacement");
  Check(NextSwapAction(true, swapped) == SwapAction::kNone && swapped, "later ready draws don't re-bind");
  Check(NextSwapAction(false, swapped) == SwapAction::kBindOriginal && !swapped, "a missing provider binds the original back");
  Check(NextSwapAction(false, swapped) == SwapAction::kNone && !swapped, "the original is bound back only once");
  Check(NextSwapAction(true, swapped) == SwapAction::kBindReplacement && swapped, "the replacement returns when the provider does");
  bool rebound = true;  // the game bound our object itself (D3D11 PSGetShader round trip)
  Check(NextSwapAction(true, rebound) == SwapAction::kNone, "a game-bound replacement isn't bound again");

  using T = reshade::api::descriptor_type;
  Check(SlotClassOf(T::shader_resource_view) == SlotClass::kSrv && SlotClassOf(T::buffer_shader_resource_view) == SlotClass::kSrv &&
            SlotClassOf(T::unordered_access_view) == SlotClass::kUav && SlotClassOf(T::sampler) == SlotClass::kSampler &&
            SlotClassOf(T::constant_buffer) == SlotClass::kOther,
        "descriptor types map to D3D11 slot classes");
}

void TestSavedSlots() {
  using S = reshade::api::shader_stage;
  using T = reshade::api::descriptor_type;
  // Objects are ints standing in for COM pointers; released/restored ones are recorded.
  std::vector<int> released;
  std::vector<std::pair<int, uint32_t>> restored;
  const auto release = [&](SavedSlots<int>::Entry& entry) { released.push_back(entry.object); };
  const auto restore = [&](SavedSlots<int>::Entry& entry) { restored.emplace_back(entry.object, entry.key.slot); };
  const SlotKey ps_t3{S::pixel, SlotClass::kSrv, 3};
  const SlotKey ps_s3{S::pixel, SlotClass::kSampler, 3};
  const SlotKey vs_t3{S::vertex, SlotClass::kSrv, 3};
  const SlotKey cs_u1{S::compute, SlotClass::kUav, 1};

  SavedSlots<int> saved;
  int saves = 0;
  saved.SaveOnce(ps_t3, T::shader_resource_view, [&] { return ++saves, 10; });
  saved.SaveOnce(ps_t3, T::shader_resource_view, [&] { return ++saves, 99; });  // slot now holds DXP's view
  Check(saves == 1 && saved.Size() == 1, "the game's binding is saved once, before the first push");
  saved.SaveOnce(ps_s3, T::sampler, [] { return 11; });
  saved.SaveOnce(vs_t3, T::shader_resource_view, [] { return 12; });
  Check(saved.Size() == 3, "slots are keyed by stage, register class and index");

  // A draw that uses ps t3 and ps s3 restores only vs t3.
  const std::vector<SlotKey> used = {ps_t3, ps_s3};
  saved.RestoreUnused([&](const SlotKey& key) { return std::ranges::find(used, key) != used.end(); }, restore);
  Check(restored == std::vector<std::pair<int, uint32_t>>{{12, 3}} && saved.Size() == 2 && saved.Contains(ps_t3),
        "slots the draw doesn't use are restored, used ones stay");

  // The game writes PS SRVs 0-3 itself: its binding wins, no restore. Samplers and other stages stay.
  saved.DropWritten(S::pixel, SlotClass::kSrv, 0, 4, release);
  Check(released == std::vector<int>{10} && !saved.Contains(ps_t3) && saved.Contains(ps_s3), "a game write drops the pending restore");
  saved.DropWritten(S::pixel, SlotClass::kSampler, 4, 2, release);
  saved.DropWritten(S::vertex, SlotClass::kSampler, 0, 16, release);
  Check(released.size() == 1 && saved.Contains(ps_s3), "writes to other slots, stages or classes keep the restore");

  // ClearState reports every slot of every stage.
  saved.SaveOnce(cs_u1, T::unordered_access_view, [] { return 13; });
  saved.DropWritten(S::all, SlotClass::kUav, 0, 8, release);
  saved.DropWritten(S::all, SlotClass::kSampler, 0, 16, release);
  Check(saved.Empty() && released == std::vector<int>{10, 13, 11}, "ClearState drops every pending restore");

  // close_command_list / present restore everything; reset releases without restoring.
  restored.clear();
  saved.SaveOnce(ps_t3, T::shader_resource_view, [] { return 20; });
  saved.SaveOnce(cs_u1, T::unordered_access_view, [] { return 21; });
  saved.RestoreAll(restore);
  Check(saved.Empty() && restored.size() == 2, "RestoreAll restores every slot");
  restored.clear();
  released.clear();
  saved.SaveOnce(ps_t3, T::shader_resource_view, [] { return 30; });
  saved.Clear(release);
  Check(saved.Empty() && restored.empty() && released == std::vector<int>{30}, "Clear releases without restoring");
}

}  // namespace

int main() {
  const fs::path source_root = DXP_SOURCE_DIR;
  const fs::path dir = fs::path(DXP_TEST_OUTPUT_DIR) / "reshade_addon_test";
  fs::remove_all(dir);
  fs::create_directories(dir);

  TestDds(dir);
  TestRecipes(source_root, dir);
  TestRequirements(dir);
  TestSwap();
  TestSavedSlots();

  std::cout << (g_failures == 0 ? "All checks passed\n" : std::to_string(g_failures) + " check(s) failed\n");
  return g_failures == 0 ? 0 : 1;
}
