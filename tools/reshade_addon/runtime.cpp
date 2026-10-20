#include "runtime.hpp"

#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <thread>
#include <vector>

#include "draw_state.hpp"

// Pipeline replacement follows the ReShade/RenoDX create-and-bind model:
//
//  1. init_pipeline        Capture: the pipeline description is cloned (shader bytecode is
//                          deduplicated by hash) and a patch job is queued.
//  2. worker threads       Run the recipes, resolve the bindings they added, and create the
//                          replacement pipeline with device->create_pipeline.
//  3. bind_pipeline        If a replacement is ready, record it as pending for the command
//                          list. The game's pipeline stays bound.
//  4. draw / dispatch      Resolve every binding of the pending replacement. If all resolve,
//                          bind the replacement (once per game bind) and push the descriptors
//                          of recipe-added resources; if one doesn't, put the game's pipeline
//                          back. The draw itself is always the game's: draw events return
//                          false, so add-ons whose draw callbacks run later still apply.
//
// Resources need a binding slot the game does not use:
//  - D3D12: every game root signature is extended at create_pipeline_layout with one
//    push-descriptor table per descriptor type (SRV/UAV/sampler) in a dedicated register
//    space (Settings::injection). Recipes declare their resources inside those ranges.
//  - D3D11: resources bind straight to the register DXP assigned. The game's binding in
//    that slot is saved before the first push and restored at the next draw that doesn't
//    use the slot (or at close_command_list / present). If the game writes the slot itself
//    first, its binding wins and the saved one is dropped.
//
// Patching is asynchronous: until a pipeline's job finishes the game's original pipeline
// is used, and replacements are swapped in on the next bind.

namespace dxp::addon {

namespace {

// ── Shared state ──────────────────────────────────────────────────────────────

struct ShaderBlob {
  uint64_t hash = 0;
  Stage stage = Stage::kVertex;
  std::optional<ShaderModel> model;
  std::vector<uint8_t> code;
};

struct DrawBinding {
  uint32_t layout_param = 0;  // D3D12 root parameter of the injected table
  uint32_t slot = 0;          // D3D12: offset inside the table. D3D11: shader register
  api::shader_stage stage = api::shader_stage::all;
  ResolvedBinding resolved;
};

/// A published replacement pipeline. Destroyed (with its pipeline) once the last
/// reference drops, which the retire queue delays until the GPU is done with it.
struct Replacement {
  api::device* device = nullptr;
  api::pipeline pipeline = {0};
  api::pipeline original = {0};
  api::pipeline_layout layout = {0};
  bool compute = false;
  std::vector<DrawBinding> bindings;  // sorted by (layout_param, slot)
  std::shared_ptr<const ResourceSet> resources;
  std::vector<std::string> recipes;

  ~Replacement() {
    if (pipeline != 0) device->destroy_pipeline(pipeline);
  }
};

enum class RecordStatus : uint8_t { kPending, kUnmodified, kPatched, kFailed, kWaitingForResources, kDestroyed, kCount };

/// Owned copy of a pipeline description. ReShade only guarantees the subobjects for the
/// duration of init_pipeline, so everything (including strings) is copied.
class OwnedPipelineDesc {
 public:
  struct ShaderRef {
    size_t index = 0;
    Stage stage = Stage::kVertex;
    std::shared_ptr<const ShaderBlob> blob;
  };

  template <typename Intern>
  bool Clone(uint32_t count, const api::pipeline_subobject* source, Intern&& intern) {
    subobjects.assign(source, source + count);
    for (uint32_t i = 0; i < count; ++i) {
      api::pipeline_subobject& subobject = subobjects[i];
      if (subobject.count == 0 || subobject.data == nullptr) continue;

      if (const auto stage = StageFromSubobject(subobject.type)) {
        auto* desc = static_cast<api::shader_desc*>(Copy(subobject.data, sizeof(api::shader_desc) * subobject.count));
        subobject.data = desc;
        desc->entry_point = CopyString(desc->entry_point);
        desc->spec_constants = 0;  // SPIR-V only
        desc->spec_constant_ids = nullptr;
        desc->spec_constant_values = nullptr;
        if (desc->code_size == 0) continue;
        auto blob = intern(std::span(static_cast<const uint8_t*>(desc->code), desc->code_size), *stage);
        desc->code = blob->code.data();
        shaders.push_back({i, *stage, std::move(blob)});
        continue;
      }

      const size_t element_size = ElementSize(subobject.type);
      if (element_size == 0) return false;  // ray tracing, libraries, unknown: not patchable
      subobject.data = Copy(subobject.data, element_size * subobject.count);
      if (subobject.type == api::pipeline_subobject_type::input_layout) {
        auto* elements = static_cast<api::input_element*>(subobject.data);
        for (uint32_t k = 0; k < subobject.count; ++k) elements[k].semantic = CopyString(elements[k].semantic);
      }
    }
    return true;
  }

  std::vector<api::pipeline_subobject> subobjects;
  std::vector<ShaderRef> shaders;

 private:
  static std::optional<Stage> StageFromSubobject(api::pipeline_subobject_type type) {
    switch (type) {
      case api::pipeline_subobject_type::vertex_shader:        return Stage::kVertex;
      case api::pipeline_subobject_type::hull_shader:          return Stage::kHull;
      case api::pipeline_subobject_type::domain_shader:        return Stage::kDomain;
      case api::pipeline_subobject_type::geometry_shader:      return Stage::kGeometry;
      case api::pipeline_subobject_type::pixel_shader:         return Stage::kPixel;
      case api::pipeline_subobject_type::compute_shader:       return Stage::kCompute;
      case api::pipeline_subobject_type::amplification_shader: return Stage::kAmplification;
      case api::pipeline_subobject_type::mesh_shader:          return Stage::kMesh;
      default:                                                 return std::nullopt;
    }
  }

  static size_t ElementSize(api::pipeline_subobject_type type) {
    using T = api::pipeline_subobject_type;
    switch (type) {
      case T::input_layout:            return sizeof(api::input_element);
      case T::stream_output_state:     return sizeof(api::stream_output_desc);
      case T::blend_state:             return sizeof(api::blend_desc);
      case T::rasterizer_state:        return sizeof(api::rasterizer_desc);
      case T::depth_stencil_state:     return sizeof(api::depth_stencil_desc);
      case T::primitive_topology:      return sizeof(api::primitive_topology);
      case T::depth_stencil_format:
      case T::render_target_formats:   return sizeof(api::format);
      case T::sample_mask:
      case T::sample_count:
      case T::viewport_count:
      case T::max_vertex_count:
      case T::max_payload_size:
      case T::max_attribute_size:
      case T::max_recursion_depth:     return sizeof(uint32_t);
      case T::dynamic_pipeline_states: return sizeof(api::dynamic_state);
      case T::flags:                   return sizeof(api::pipeline_flags);
      default:                         return 0;
    }
  }

  void* Copy(const void* data, size_t bytes) {
    auto& storage = storage_.emplace_back(std::make_unique<std::byte[]>(bytes));
    std::memcpy(storage.get(), data, bytes);
    return storage.get();
  }

  const char* CopyString(const char* text) {
    if (text == nullptr || text[0] == '\0') return nullptr;  // ReShade treats "" differently from nullptr
    return static_cast<const char*>(Copy(text, std::strlen(text) + 1));
  }

  std::vector<std::unique_ptr<std::byte[]>> storage_;
};

struct PipelineRecord {
  api::pipeline pipeline = {0};
  api::pipeline_layout layout = {0};
  bool compute = false;
  OwnedPipelineDesc desc;

  std::atomic<std::shared_ptr<const Replacement>> replacement;
  std::atomic<uint64_t> requested_generation{0};
  std::atomic<RecordStatus> status{RecordStatus::kPending};
};

/// Root parameter indices of the injected tables in one D3D12 pipeline layout.
struct LayoutInjection {
  int32_t srv = -1;
  int32_t uav = -1;
  int32_t sampler = -1;
};

struct __declspec(uuid("6f0c2f3e-8a4b-4c1d-9e57-2b1d7a5c0e41")) DeviceState {
  api::device* device = nullptr;
  api::device_api api = api::device_api::d3d11;
  InjectionLayout injection;

  std::atomic<std::shared_ptr<const ResourceSet>> resources;

  std::shared_mutex mutex;
  std::unordered_map<uint64_t, std::shared_ptr<PipelineRecord>> pipelines;
  // D3D11 only: a game can read our shader back (PSGetShader) and bind it again later.
  std::unordered_map<uint64_t, std::shared_ptr<PipelineRecord>> replacement_owners;
  std::unordered_map<uint64_t, LayoutInjection> layouts;

  std::mutex blob_mutex;
  std::unordered_map<uint64_t, std::weak_ptr<const ShaderBlob>> blobs;

  std::atomic<bool> alive{true};
  std::atomic<uint32_t> in_flight{0};
  std::array<std::atomic<int64_t>, static_cast<size_t>(RecordStatus::kCount)> status_counts{};
  std::atomic<uint32_t> layouts_extended{0};
  std::atomic<uint32_t> layouts_skipped{0};

  std::mutex error_mutex;
  std::deque<std::string> recent_errors;

  std::shared_ptr<const ShaderBlob> InternBlob(std::span<const uint8_t> code, Stage stage) {
    const uint64_t hash = HashShader(code);
    std::lock_guard lock(blob_mutex);
    auto& slot = blobs[hash * 31 + static_cast<uint64_t>(stage)];
    if (auto existing = slot.lock()) return existing;
    auto blob = std::make_shared<ShaderBlob>();
    blob->hash = hash;
    blob->stage = stage;
    blob->model = DetectShaderModel(code);
    blob->code.assign(code.begin(), code.end());
    slot = blob;
    return blob;
  }

  void AddError(std::string error) {
    std::lock_guard lock(error_mutex);
    recent_errors.push_back(std::move(error));
    if (recent_errors.size() > 32) recent_errors.pop_front();
  }
};

constexpr size_t kD3D12Slot = 0;

/// A replacement for the pipeline the game bound last. It is bound at the next draw that
/// can resolve all its bindings.
struct PendingReplacement {
  std::shared_ptr<const Replacement> replacement;
  bool swapped = false;  // the replacement is bound in place of the game's pipeline
};

struct __declspec(uuid("b3e4a1d2-7f60-4c8b-a5d9-0c2e6f1a9b77")) CommandListState {
  // D3D12 has a single pipeline slot (index 0); D3D11 one per shader stage.
  std::array<PendingReplacement, kStageCount> pending;

  // D3D11: game bindings replaced by DXP pushes, each holding one reference.
  SavedSlots<IUnknown*> saved;

  // D3D12: push_descriptors switches to ReShade's descriptor heaps. The game's heaps are
  // restored by re-binding the last game descriptor table through ReShade's API.
  struct TableBind {
    api::pipeline_layout layout = {0};
    uint32_t param = 0;
    api::descriptor_table table = {0};
  };
  bool heaps_dirty = false;
  std::array<TableBind, 2> last_table;               // [graphics, compute]
  std::array<api::pipeline_layout, 2> current_layout{};  // [graphics, compute]

  CommandListState() = default;
  CommandListState(const CommandListState&) = delete;
  CommandListState& operator=(const CommandListState&) = delete;
  ~CommandListState() { ReleaseSaved(); }

  void ReleaseSaved() {
    saved.Clear([](auto& entry) {
      if (entry.object != nullptr) entry.object->Release();
    });
  }

  void Reset() {
    ReleaseSaved();
    pending = {};
    heaps_dirty = false;
    last_table = {};
    current_layout = {};
  }
};

HMODULE g_module = nullptr;
std::atomic<std::shared_ptr<const Settings>> g_settings;
std::atomic<bool> g_enabled{true};
std::atomic<std::shared_ptr<const RecipeSet>> g_recipes;
std::atomic<bool> g_any_bindings{false};

std::mutex g_devices_mutex;
std::vector<DeviceState*> g_devices;

// ── Retire queue ──────────────────────────────────────────────────────────────

constexpr uint64_t kRetireFrames = 8;

struct Retired {
  uint64_t frame = 0;
  api::device* device = nullptr;
  std::shared_ptr<const void> object;
};

std::mutex g_retire_mutex;
std::deque<Retired> g_retired;
std::atomic<uint64_t> g_frame{0};

void Retire(api::device* device, std::shared_ptr<const void> object) {
  if (!object) return;
  std::lock_guard lock(g_retire_mutex);
  g_retired.push_back({g_frame.load(), device, std::move(object)});
}

void ReleaseRetired(api::device* device_filter, bool all) {
  std::vector<std::shared_ptr<const void>> released;
  {
    std::lock_guard lock(g_retire_mutex);
    const uint64_t frame = g_frame.load();
    for (auto it = g_retired.begin(); it != g_retired.end();) {
      const bool expired = all ? (device_filter == nullptr || it->device == device_filter) : it->frame + kRetireFrames <= frame;
      if (expired) {
        released.push_back(std::move(it->object));
        it = g_retired.erase(it);
      } else {
        ++it;
      }
    }
  }
  // Destructors run outside the lock.
}

// ── Status bookkeeping ────────────────────────────────────────────────────────

void SetStatus(DeviceState& state, PipelineRecord& record, RecordStatus status) {
  RecordStatus old = record.status.load();
  do {
    if (old == RecordStatus::kDestroyed) return;
  } while (!record.status.compare_exchange_weak(old, status));
  state.status_counts[static_cast<size_t>(old)].fetch_sub(1);
  state.status_counts[static_cast<size_t>(status)].fetch_add(1);
}

void Publish(DeviceState& state, PipelineRecord& record, uint64_t generation, std::shared_ptr<const Replacement> replacement,
             RecordStatus status, const std::string& error) {
  if (record.requested_generation.load() != generation || record.status.load() == RecordStatus::kDestroyed) {
    Retire(state.device, std::move(replacement));  // superseded or destroyed meanwhile
    return;
  }

  if (replacement && !replacement->bindings.empty()) g_any_bindings.store(true);
  auto old = record.replacement.exchange(replacement);

  if (state.api == api::device_api::d3d11) {
    std::unique_lock lock(state.mutex);
    if (old) state.replacement_owners.erase(old->pipeline.handle);
    if (replacement) {
      if (const auto it = state.pipelines.find(record.pipeline.handle); it != state.pipelines.end()) {
        state.replacement_owners[replacement->pipeline.handle] = it->second;
      }
    }
  }
  Retire(state.device, std::move(old));
  SetStatus(state, record, status);

  if (!error.empty()) {
    const std::string message = std::format("pipeline {:#x}: {}", record.pipeline.handle, error);
    if (state.status_counts[static_cast<size_t>(RecordStatus::kFailed)].load() <= 5) {
      Log(reshade::log::level::warning, "{}", message);
    }
    state.AddError(message);
  }
}

// ── Binding resolution ────────────────────────────────────────────────────────

bool BuildBindings(DeviceState& state, const PipelineRecord& record, Stage d3d11_stage,
                   const std::vector<dxp::ResourceBinding>& bindings, Replacement& replacement, std::string& error,
                   bool& waiting_for_resources) {
  if (bindings.empty()) return true;
  replacement.resources = state.resources.load();
  if (!replacement.resources) {
    waiting_for_resources = true;
    error = "resources are not loaded yet";
    return false;
  }

  LayoutInjection injected;
  if (state.api == api::device_api::d3d12) {
    std::shared_lock lock(state.mutex);
    const auto it = state.layouts.find(record.layout.handle);
    if (it == state.layouts.end()) {
      error = "the pipeline layout was not extended with the DXP descriptor tables "
              "(InjectLayouts setting, register overlap or root signature size; see the ReShade log)";
      return false;
    }
    injected = it->second;
  }

  for (const dxp::ResourceBinding& binding : bindings) {
    DrawBinding draw;
    if (!replacement.resources->Resolve(binding, draw.resolved, error)) return false;
    const auto type = draw.resolved.type;
    const uint32_t reg = binding.register_index;

    if (state.api == api::device_api::d3d12) {
      const InjectionLayout& layout = state.injection;
      int32_t param = -1;
      uint32_t base = 0;
      uint32_t count = 0;
      const char* register_class = "t";
      if (type == api::descriptor_type::shader_resource_view) {
        param = injected.srv;
        base = layout.srv_base;
        count = layout.srv_count;
      } else if (type == api::descriptor_type::unordered_access_view) {
        param = injected.uav;
        base = layout.uav_base;
        count = layout.uav_count;
        register_class = "u";
      } else if (type == api::descriptor_type::sampler) {
        param = injected.sampler;
        base = layout.sampler_base;
        count = layout.sampler_count;
        register_class = "s";
      } else {
        error = std::format("'{}': constant buffers are not supported", binding.handle);
        return false;
      }
      if (param < 0 || binding.space != layout.space || reg < base || reg >= base + count) {
        error = std::format("'{}' is bound at {}{} space{} but the injected range is {}{}-{}{} space{}; set register_index/space "
                            "in the recipe or adjust the [DXP] Inject* settings",
                            binding.handle, register_class, reg, binding.space, register_class, base, register_class,
                            base + count - 1, layout.space);
        return false;
      }
      draw.layout_param = static_cast<uint32_t>(param);
      draw.slot = reg - base;
      draw.stage = replacement.compute ? api::shader_stage::all_compute : api::shader_stage::all_graphics;
    } else {
      const bool limit_ok = type == api::descriptor_type::sampler                 ? reg < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT
                            : type == api::descriptor_type::shader_resource_view  ? reg < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT
                            : type == api::descriptor_type::unordered_access_view ? d3d11_stage == Stage::kCompute && reg < D3D11_1_UAV_SLOT_COUNT
                                                                                  : false;
      if (!limit_ok) {
        error = std::format("'{}' (register {}) cannot be bound on D3D11 (constant buffers and graphics UAVs are unsupported)",
                            binding.handle, reg);
        return false;
      }
      draw.slot = reg;
      draw.stage = ToShaderStage(d3d11_stage);
    }
    replacement.bindings.push_back(std::move(draw));
  }

  std::ranges::sort(replacement.bindings, {}, [](const DrawBinding& b) { return std::pair(b.layout_param, b.slot); });
  for (size_t i = 1; i < replacement.bindings.size(); ++i) {
    const auto& a = replacement.bindings[i - 1];
    const auto& b = replacement.bindings[i];
    if (a.layout_param == b.layout_param && a.slot == b.slot && a.resolved.type == b.resolved.type && a.stage == b.stage &&
        a.resolved.binding.handle != b.resolved.binding.handle) {
      error = std::format("'{}' and '{}' were assigned the same register", a.resolved.binding.handle, b.resolved.binding.handle);
      return false;
    }
  }
  return true;
}

// ── Worker pool ───────────────────────────────────────────────────────────────

struct Job {
  std::shared_ptr<PipelineRecord> record;
  DeviceState* state = nullptr;
  std::shared_ptr<const RecipeSet> recipes;
};

std::mutex g_queue_mutex;
std::condition_variable g_queue_cv;
std::deque<Job> g_queue;
std::vector<std::thread> g_workers;
bool g_stop_workers = false;

void ProcessJob(const Job& job) {
  DeviceState& state = *job.state;
  PipelineRecord& record = *job.record;
  const uint64_t generation = job.recipes->Generation();
  if (!state.alive.load() || record.requested_generation.load() != generation) return;

  const auto settings = GetSettings();
  std::vector<std::pair<size_t, std::shared_ptr<const PatchedShader>>> patched;
  std::vector<dxp::ResourceBinding> bindings;
  auto replacement = std::make_shared<Replacement>();
  replacement->device = state.device;
  replacement->original = record.pipeline;
  replacement->layout = record.layout;
  replacement->compute = record.compute;

  Stage d3d11_stage = Stage::kVertex;
  for (const auto& shader : record.desc.shaders) {
    const ShaderBlob& blob = *shader.blob;
    if (!blob.model || !job.recipes->AppliesTo(*blob.model, shader.stage)) continue;
    auto result = job.recipes->Patch(*blob.model, shader.stage, blob.hash, blob.code, *settings);
    if (!result) continue;
    d3d11_stage = shader.stage;
    for (const auto& binding : result->bindings) {
      if (std::ranges::none_of(bindings, [&](const auto& b) { return b.handle == binding.handle && b.binding_class == binding.binding_class; })) {
        bindings.push_back(binding);
      }
    }
    replacement->recipes.insert(replacement->recipes.end(), result->recipes.begin(), result->recipes.end());
    patched.emplace_back(shader.index, std::move(result));
  }

  if (patched.empty()) {
    Publish(state, record, generation, nullptr, RecordStatus::kUnmodified, {});
    return;
  }

  std::string error;
  bool waiting = false;
  if (!BuildBindings(state, record, d3d11_stage, bindings, *replacement, error, waiting)) {
    Publish(state, record, generation, nullptr, waiting ? RecordStatus::kWaitingForResources : RecordStatus::kFailed,
            waiting ? std::string() : error);
    return;
  }

  std::vector<api::pipeline_subobject> subobjects = record.desc.subobjects;
  std::vector<api::shader_desc> shader_descs(patched.size());
  for (size_t i = 0; i < patched.size(); ++i) {
    const auto& [index, shader] = patched[i];
    shader_descs[i] = *static_cast<const api::shader_desc*>(subobjects[index].data);
    shader_descs[i].code = shader->code.data();
    shader_descs[i].code_size = shader->code.size();
    subobjects[index].data = &shader_descs[i];
  }

  if (!state.alive.load()) return;
  if (!state.device->create_pipeline(record.layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &replacement->pipeline)) {
    replacement->pipeline = {0};
    Publish(state, record, generation, nullptr, RecordStatus::kFailed,
            std::format("create_pipeline rejected the patched shader(s) from {}", replacement->recipes.front()));
    return;
  }
  Publish(state, record, generation, std::move(replacement), RecordStatus::kPatched, {});
}

void WorkerMain() {
  SetThreadDescription(GetCurrentThread(), L"DXP patch worker");
  while (true) {
    Job job;
    {
      std::unique_lock lock(g_queue_mutex);
      g_queue_cv.wait(lock, [] { return g_stop_workers || !g_queue.empty(); });
      if (g_stop_workers) return;
      job = std::move(g_queue.front());
      g_queue.pop_front();
    }
    try {
      ProcessJob(job);
    } catch (const std::exception& e) {
      Log(reshade::log::level::error, "Patch job threw: {}", e.what());
    }
    job.state->in_flight.fetch_sub(1);
  }
}

void Enqueue(DeviceState& state, const std::shared_ptr<PipelineRecord>& record, const std::shared_ptr<const RecipeSet>& recipes) {
  const uint64_t generation = recipes->Generation();
  record->requested_generation.store(generation);

  const bool applicable = std::ranges::any_of(record->desc.shaders, [&](const auto& shader) {
    return shader.blob->model && recipes->AppliesTo(*shader.blob->model, shader.stage);
  });
  if (!applicable) {
    Publish(state, *record, generation, nullptr, RecordStatus::kUnmodified, {});
    return;
  }

  SetStatus(state, *record, RecordStatus::kPending);
  state.in_flight.fetch_add(1);
  std::lock_guard lock(g_queue_mutex);
  if (g_workers.empty() && !g_stop_workers) {
    for (uint32_t i = 0; i < GetSettings()->worker_threads; ++i) g_workers.emplace_back(WorkerMain);
  }
  g_queue.push_back({record, &state, recipes});
  g_queue_cv.notify_one();
}

void RepatchDevice(DeviceState& state, const std::shared_ptr<const RecipeSet>& recipes,
                   std::optional<RecordStatus> only_status = std::nullopt) {
  std::vector<std::shared_ptr<PipelineRecord>> records;
  {
    std::shared_lock lock(state.mutex);
    records.reserve(state.pipelines.size());
    for (const auto& [handle, record] : state.pipelines) {
      if (!only_status || record->status.load() == *only_status) records.push_back(record);
    }
  }
  for (const auto& record : records) Enqueue(state, record, recipes);
}

void RepatchAll(const std::shared_ptr<const RecipeSet>& recipes) {
  std::lock_guard lock(g_devices_mutex);
  for (DeviceState* state : g_devices) RepatchDevice(*state, recipes);
}

DeviceState* GetDeviceState(api::device* device) {
  return device->get_private_data<DeviceState>();
}

void LoadResources(DeviceState& state) {
  auto resources = ResourceSet::Load(state.device, GetSettings()->ResourceDir());
  const bool first_load = !state.resources.load();
  Retire(state.device, state.resources.exchange(std::move(resources)));
  if (first_load) RepatchDevice(state, GetRecipes(), RecordStatus::kWaitingForResources);
}

// ── Layout injection (D3D12) ──────────────────────────────────────────────────

bool ShouldInject() {
  switch (GetSettings()->inject_mode) {
    case InjectMode::kAlways: return true;
    case InjectMode::kNever:  return false;
    default:                  return GetRecipes()->AnyAddResource();
  }
}

enum class RegisterClass : uint8_t { kNone, kT, kU, kS, kB };

RegisterClass ClassOf(api::descriptor_type type) {
  switch (type) {
    case api::descriptor_type::sampler:                      return RegisterClass::kS;
    case api::descriptor_type::constant_buffer:              return RegisterClass::kB;
    case api::descriptor_type::shader_resource_view:
    case api::descriptor_type::buffer_shader_resource_view:
    case api::descriptor_type::acceleration_structure:       return RegisterClass::kT;
    case api::descriptor_type::unordered_access_view:
    case api::descriptor_type::buffer_unordered_access_view: return RegisterClass::kU;
    default:                                                 return RegisterClass::kNone;
  }
}

std::vector<api::descriptor_range> InjectedRanges(const InjectionLayout& layout) {
  std::vector<api::descriptor_range> ranges;
  const auto add = [&](uint32_t base, uint32_t count, api::descriptor_type type) {
    if (count != 0) ranges.push_back({0, base, layout.space, count, api::shader_stage::all, 1, type});
  };
  add(layout.srv_base, layout.srv_count, api::descriptor_type::shader_resource_view);
  add(layout.uav_base, layout.uav_count, api::descriptor_type::unordered_access_view);
  add(layout.sampler_base, layout.sampler_count, api::descriptor_type::sampler);
  return ranges;
}

bool Overlaps(const api::descriptor_range& a, const api::descriptor_range& b) {
  if (ClassOf(a.type) != ClassOf(b.type) || a.dx_register_space != b.dx_register_space || a.count == 0 || b.count == 0) return false;
  const uint64_t a_end = a.count == UINT32_MAX ? UINT64_MAX : uint64_t{a.dx_register_index} + a.count;
  const uint64_t b_end = b.count == UINT32_MAX ? UINT64_MAX : uint64_t{b.dx_register_index} + b.count;
  return a.dx_register_index < b_end && b.dx_register_index < a_end;
}

bool OnCreatePipelineLayout(api::device* device, uint32_t& param_count, api::pipeline_layout_param*& params) {
  DeviceState* state = GetDeviceState(device);
  if (state == nullptr || state->api != api::device_api::d3d12 || param_count == 0 || !ShouldInject()) return false;

  const auto injected = InjectedRanges(state->injection);
  if (injected.empty()) return false;

  // Mirrors D3D12 root signature costs: tables 1 DWORD, root descriptors 2, constants 1 each.
  uint32_t dwords = 0;
  uint32_t insert_at = param_count;
  bool overlap = false;
  const auto check = [&](const api::descriptor_range& range) {
    for (const auto& ours : injected) overlap |= Overlaps(range, ours);
  };
  for (uint32_t i = 0; i < param_count; ++i) {
    const auto& param = params[i];
    switch (param.type) {
      case api::pipeline_layout_param_type::push_constants: {
        dwords += param.push_constants.count;
        const auto& c = param.push_constants;
        check({0, c.dx_register_index, c.dx_register_space, 1, c.visibility, 1, api::descriptor_type::constant_buffer});
        break;
      }
      case api::pipeline_layout_param_type::push_descriptors: {
        const auto& range = param.push_descriptors;
        const bool root_descriptor = range.count == 1 && range.binding == 0 &&
                                     (range.type == api::descriptor_type::constant_buffer || range.type == api::descriptor_type::buffer_shader_resource_view ||
                                      range.type == api::descriptor_type::buffer_unordered_access_view || range.type == api::descriptor_type::acceleration_structure);
        if (range.count != 0) dwords += root_descriptor ? 2 : 1;
        check(range);
        break;
      }
      case api::pipeline_layout_param_type::descriptor_table:
      case api::pipeline_layout_param_type::push_descriptors_with_ranges:
        if (param.descriptor_table.count != 0) dwords += 1;
        for (uint32_t k = 0; k < param.descriptor_table.count; ++k) check(param.descriptor_table.ranges[k]);
        break;
      case api::pipeline_layout_param_type::descriptor_table_with_static_samplers:
      case api::pipeline_layout_param_type::push_descriptors_with_static_samplers: {
        const auto& table = param.descriptor_table_with_static_samplers;
        bool static_only = table.count != 0;
        for (uint32_t k = 0; k < table.count; ++k) {
          check(table.ranges[k]);
          static_only &= table.ranges[k].type == api::descriptor_type::sampler && table.ranges[k].static_samplers != nullptr;
        }
        // ReShade turns static-sampler params into static samplers, not root parameters,
        // so injected params go in front of them to keep root parameter indices stable.
        if (static_only && param.type == api::pipeline_layout_param_type::push_descriptors_with_static_samplers) {
          insert_at = std::min(insert_at, i);
        } else if (table.count != 0) {
          dwords += 1;
        }
        break;
      }
      default:
        break;
    }
  }

  if (overlap || dwords + injected.size() > 64) {
    state->layouts_skipped.fetch_add(1);
    Log(reshade::log::level::warning, "Not extending a root signature: {}",
        overlap ? std::format("it already binds registers in space {}", state->injection.space)
                : std::format("{} + {} DWORDs exceeds the 64 DWORD limit", dwords, injected.size()));
    return false;
  }

  // ReShade consumes the modified array before the event returns to the next creation
  // on this thread, so thread-local storage is sufficient.
  static thread_local std::vector<api::pipeline_layout_param> modified;
  modified.assign(params, params + insert_at);
  for (const auto& range : injected) modified.emplace_back(range);
  modified.insert(modified.end(), params + insert_at, params + param_count);
  params = modified.data();
  param_count = static_cast<uint32_t>(modified.size());
  return true;
}

void OnInitPipelineLayout(api::device* device, uint32_t param_count, const api::pipeline_layout_param* params, api::pipeline_layout layout) {
  DeviceState* state = GetDeviceState(device);
  if (state == nullptr || state->api != api::device_api::d3d12) return;

  LayoutInjection injection;
  const auto injected = InjectedRanges(state->injection);
  for (uint32_t i = 0; i < param_count; ++i) {
    if (params[i].type != api::pipeline_layout_param_type::push_descriptors) continue;
    const auto& range = params[i].push_descriptors;
    for (const auto& ours : injected) {
      if (range.dx_register_index != ours.dx_register_index || range.dx_register_space != ours.dx_register_space ||
          range.count != ours.count || range.type != ours.type || range.visibility != ours.visibility) continue;
      if (ours.type == api::descriptor_type::shader_resource_view) injection.srv = static_cast<int32_t>(i);
      if (ours.type == api::descriptor_type::unordered_access_view) injection.uav = static_cast<int32_t>(i);
      if (ours.type == api::descriptor_type::sampler) injection.sampler = static_cast<int32_t>(i);
    }
  }
  if (injection.srv < 0 && injection.uav < 0 && injection.sampler < 0) return;
  state->layouts_extended.fetch_add(1);
  std::unique_lock lock(state->mutex);
  state->layouts[layout.handle] = injection;
}

void OnDestroyPipelineLayout(api::device* device, api::pipeline_layout layout) {
  if (DeviceState* state = GetDeviceState(device); state != nullptr) {
    std::unique_lock lock(state->mutex);
    state->layouts.erase(layout.handle);
  }
}

// ── Device / queue lifetime ───────────────────────────────────────────────────

void OnInitDevice(api::device* device) {
  const auto device_api = device->get_api();
  if (device_api != api::device_api::d3d11 && device_api != api::device_api::d3d12) {
    Log(reshade::log::level::info, "Device API {} is not supported; DXP stays inactive for it", static_cast<int>(device_api));
    return;
  }
  DeviceState& state = *device->create_private_data<DeviceState>();
  state.device = device;
  state.api = device_api;
  state.injection = GetSettings()->injection;
  std::lock_guard lock(g_devices_mutex);
  g_devices.push_back(&state);
}

void OnInitCommandQueue(api::command_queue* queue) {
  // D3D12 initial data is uploaded through a graphics queue's immediate command list,
  // which does not exist yet during init_device.
  if ((queue->get_type() & api::command_queue_type::graphics) == 0) return;
  DeviceState* state = GetDeviceState(queue->get_device());
  if (state != nullptr && !state->resources.load()) LoadResources(*state);
}

void OnDestroyDevice(api::device* device) {
  DeviceState* state = GetDeviceState(device);
  if (state == nullptr) return;
  {
    std::lock_guard lock(g_devices_mutex);
    std::erase(g_devices, state);
  }
  state->alive.store(false);
  {
    std::lock_guard lock(g_queue_mutex);
    const auto removed = std::erase_if(g_queue, [&](const Job& job) { return job.state == state; });
    state->in_flight.fetch_sub(static_cast<uint32_t>(removed));
  }
  while (state->in_flight.load() != 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));

  {
    std::unique_lock lock(state->mutex);
    for (auto& [handle, record] : state->pipelines) record->replacement.store(nullptr);
    state->pipelines.clear();
    state->replacement_owners.clear();
  }
  ReleaseRetired(device, true);
  state->resources.store(nullptr);
  device->destroy_private_data<DeviceState>();
}

// ── Pipeline capture ──────────────────────────────────────────────────────────

void OnInitPipeline(api::device* device, api::pipeline_layout layout, uint32_t subobject_count,
                    const api::pipeline_subobject* subobjects, api::pipeline pipeline) {
  DeviceState* state = GetDeviceState(device);
  if (state == nullptr || pipeline == 0) return;

  if (state->api == api::device_api::d3d11) {
    // D3D11 input layouts arrive as {input_layout, vertex_shader}: nothing to replace.
    for (uint32_t i = 0; i < subobject_count; ++i) {
      if (subobjects[i].type == api::pipeline_subobject_type::input_layout) return;
    }
  }

  auto record = std::make_shared<PipelineRecord>();
  record->pipeline = pipeline;
  record->layout = layout;
  if (!record->desc.Clone(subobject_count, subobjects, [&](std::span<const uint8_t> code, Stage stage) { return state->InternBlob(code, stage); }) ||
      record->desc.shaders.empty()) {
    return;
  }
  record->compute = std::ranges::any_of(record->desc.shaders, [](const auto& s) { return s.stage == Stage::kCompute; });

  {
    std::unique_lock lock(state->mutex);
    state->pipelines[pipeline.handle] = record;
  }
  state->status_counts[static_cast<size_t>(RecordStatus::kPending)].fetch_add(1);
  Enqueue(*state, record, GetRecipes());
}

void OnDestroyPipeline(api::device* device, api::pipeline pipeline) {
  DeviceState* state = GetDeviceState(device);
  if (state == nullptr) return;
  std::shared_ptr<PipelineRecord> record;
  {
    std::unique_lock lock(state->mutex);
    const auto it = state->pipelines.find(pipeline.handle);
    if (it == state->pipelines.end()) return;
    record = std::move(it->second);
    state->pipelines.erase(it);
  }
  const RecordStatus old = record->status.exchange(RecordStatus::kDestroyed);
  if (old != RecordStatus::kDestroyed) state->status_counts[static_cast<size_t>(old)].fetch_sub(1);
  if (auto replacement = record->replacement.exchange(nullptr)) {
    if (state->api == api::device_api::d3d11) {
      std::unique_lock lock(state->mutex);
      state->replacement_owners.erase(replacement->pipeline.handle);
    }
    Retire(device, std::move(replacement));
  }
}

// ── Bind / draw ───────────────────────────────────────────────────────────────

api::pipeline_stage ToPipelineStage(Stage stage) {
  constexpr api::pipeline_stage kStages[] = {
      api::pipeline_stage::vertex_shader, api::pipeline_stage::hull_shader, api::pipeline_stage::domain_shader,
      api::pipeline_stage::geometry_shader, api::pipeline_stage::pixel_shader, api::pipeline_stage::compute_shader,
      api::pipeline_stage::amplification_shader, api::pipeline_stage::mesh_shader};
  return kStages[static_cast<size_t>(stage)];
}

std::optional<Stage> StageFromPipelineStage(api::pipeline_stage stages) {
  switch (stages) {
    case api::pipeline_stage::vertex_shader:   return Stage::kVertex;
    case api::pipeline_stage::hull_shader:     return Stage::kHull;
    case api::pipeline_stage::domain_shader:   return Stage::kDomain;
    case api::pipeline_stage::geometry_shader: return Stage::kGeometry;
    case api::pipeline_stage::pixel_shader:    return Stage::kPixel;
    case api::pipeline_stage::compute_shader:  return Stage::kCompute;
    default:                                   return std::nullopt;
  }
}

constexpr Stage kD3D11GraphicsStages[] = {Stage::kVertex, Stage::kHull, Stage::kDomain, Stage::kGeometry, Stage::kPixel};
constexpr Stage kD3D11ComputeStages[] = {Stage::kCompute};

void OnBindPipeline(api::command_list* cmd_list, api::pipeline_stage stages, api::pipeline pipeline) {
  DeviceState* state = GetDeviceState(cmd_list->get_device());
  if (state == nullptr) return;
  auto* cmd_state = cmd_list->get_private_data<CommandListState>();

  size_t slot = kD3D12Slot;
  if (state->api == api::device_api::d3d11) {
    const auto stage = StageFromPipelineStage(stages);
    if (!stage) {
      // ClearState and ExecuteCommandList without state restore unbind several stages at once.
      if (cmd_state != nullptr) {
        for (const auto group : {std::span<const Stage>(kD3D11GraphicsStages), std::span<const Stage>(kD3D11ComputeStages)}) {
          for (const Stage s : group) {
            if ((stages & ToPipelineStage(s)) != 0) cmd_state->pending[static_cast<size_t>(s)] = {};
          }
        }
      }
      return;
    }
    slot = static_cast<size_t>(*stage);
  }

  std::shared_ptr<const Replacement> replacement;
  if (g_enabled.load(std::memory_order_relaxed) && pipeline != 0) {
    std::shared_lock lock(state->mutex);
    const PipelineRecord* record = nullptr;
    if (const auto it = state->pipelines.find(pipeline.handle); it != state->pipelines.end()) {
      record = it->second.get();
    } else if (const auto owner = state->replacement_owners.find(pipeline.handle); owner != state->replacement_owners.end()) {
      record = owner->second.get();
    }
    if (record != nullptr) replacement = record->replacement.load();
  }

  // Only flag the replacement here; the draw binds it once its resources are available.
  if (replacement) {
    if (cmd_state == nullptr) cmd_state = cmd_list->create_private_data<CommandListState>();
    const bool swapped = replacement->pipeline == pipeline;  // D3D11: the game re-bound our object
    cmd_state->pending[slot] = {std::move(replacement), swapped};
  } else if (cmd_state != nullptr) {
    cmd_state->pending[slot] = {};
  }
}

// D3D12: re-bind the most recent game table so ReShade switches back to the game's heaps.
void RestoreHeaps(api::command_list* cmd_list, CommandListState& cmd_state, size_t preferred) {
  for (const size_t t : {preferred, 1 - preferred}) {
    const auto& bind = cmd_state.last_table[t];
    if (bind.table == 0 || bind.layout != cmd_state.current_layout[t]) continue;
    cmd_list->bind_descriptor_tables(t == 1 ? api::shader_stage::all_compute : api::shader_stage::all_graphics, bind.layout,
                                     bind.param, 1, &bind.table);
    cmd_state.heaps_dirty = false;
    return;
  }
}

void OnBindDescriptorTables(api::command_list* cmd_list, api::shader_stage stages, api::pipeline_layout layout, uint32_t first,
                            uint32_t count, const api::descriptor_table* tables) {
  if (!g_any_bindings.load(std::memory_order_relaxed)) return;
  if (cmd_list->get_device()->get_api() != api::device_api::d3d12) return;

  auto* cmd_state = cmd_list->get_private_data<CommandListState>();
  if (cmd_state == nullptr) cmd_state = cmd_list->create_private_data<CommandListState>();
  const size_t t = (stages & api::shader_stage::all_graphics) == 0 ? 1 : 0;
  if (count == 0) {
    // Root signature change: previously tracked tables are no longer bound.
    cmd_state->current_layout[t] = layout;
    cmd_state->last_table[t] = {};
    return;
  }
  cmd_state->current_layout[t] = layout;
  cmd_state->last_table[t] = {layout, first + count - 1, tables[count - 1]};
  if (cmd_state->heaps_dirty) RestoreHeaps(cmd_list, *cmd_state, t);
}

// D3D11 slot save/restore through the native context (ReShade's API cannot read bindings).
using SavedSlot = SavedSlots<IUnknown*>::Entry;

IUnknown* GetD3D11Binding(ID3D11DeviceContext* ctx, api::shader_stage stage, api::descriptor_type type, uint32_t slot) {
  if (type == api::descriptor_type::sampler) {
    ID3D11SamplerState* sampler = nullptr;
    switch (stage) {
      case api::shader_stage::vertex:   ctx->VSGetSamplers(slot, 1, &sampler); break;
      case api::shader_stage::hull:     ctx->HSGetSamplers(slot, 1, &sampler); break;
      case api::shader_stage::domain:   ctx->DSGetSamplers(slot, 1, &sampler); break;
      case api::shader_stage::geometry: ctx->GSGetSamplers(slot, 1, &sampler); break;
      case api::shader_stage::pixel:    ctx->PSGetSamplers(slot, 1, &sampler); break;
      case api::shader_stage::compute:  ctx->CSGetSamplers(slot, 1, &sampler); break;
      default: break;
    }
    return sampler;
  }
  if (type == api::descriptor_type::unordered_access_view) {
    ID3D11UnorderedAccessView* uav = nullptr;
    ctx->CSGetUnorderedAccessViews(slot, 1, &uav);
    return uav;
  }
  ID3D11ShaderResourceView* srv = nullptr;
  switch (stage) {
    case api::shader_stage::vertex:   ctx->VSGetShaderResources(slot, 1, &srv); break;
    case api::shader_stage::hull:     ctx->HSGetShaderResources(slot, 1, &srv); break;
    case api::shader_stage::domain:   ctx->DSGetShaderResources(slot, 1, &srv); break;
    case api::shader_stage::geometry: ctx->GSGetShaderResources(slot, 1, &srv); break;
    case api::shader_stage::pixel:    ctx->PSGetShaderResources(slot, 1, &srv); break;
    case api::shader_stage::compute:  ctx->CSGetShaderResources(slot, 1, &srv); break;
    default: break;
  }
  return srv;
}

void SetD3D11Binding(ID3D11DeviceContext* ctx, const SavedSlot& saved) {
  const api::shader_stage stage = saved.key.stage;
  const uint32_t slot = saved.key.slot;
  if (saved.type == api::descriptor_type::sampler) {
    auto* sampler = static_cast<ID3D11SamplerState*>(saved.object);
    switch (stage) {
      case api::shader_stage::vertex:   ctx->VSSetSamplers(slot, 1, &sampler); break;
      case api::shader_stage::hull:     ctx->HSSetSamplers(slot, 1, &sampler); break;
      case api::shader_stage::domain:   ctx->DSSetSamplers(slot, 1, &sampler); break;
      case api::shader_stage::geometry: ctx->GSSetSamplers(slot, 1, &sampler); break;
      case api::shader_stage::pixel:    ctx->PSSetSamplers(slot, 1, &sampler); break;
      case api::shader_stage::compute:  ctx->CSSetSamplers(slot, 1, &sampler); break;
      default: break;
    }
    return;
  }
  if (saved.type == api::descriptor_type::unordered_access_view) {
    auto* uav = static_cast<ID3D11UnorderedAccessView*>(saved.object);
    const UINT keep_counter = UINT_MAX;
    ctx->CSSetUnorderedAccessViews(slot, 1, &uav, &keep_counter);
    return;
  }
  auto* srv = static_cast<ID3D11ShaderResourceView*>(saved.object);
  switch (stage) {
    case api::shader_stage::vertex:   ctx->VSSetShaderResources(slot, 1, &srv); break;
    case api::shader_stage::hull:     ctx->HSSetShaderResources(slot, 1, &srv); break;
    case api::shader_stage::domain:   ctx->DSSetShaderResources(slot, 1, &srv); break;
    case api::shader_stage::geometry: ctx->GSSetShaderResources(slot, 1, &srv); break;
    case api::shader_stage::pixel:    ctx->PSSetShaderResources(slot, 1, &srv); break;
    case api::shader_stage::compute:  ctx->CSSetShaderResources(slot, 1, &srv); break;
    default: break;
  }
}

ID3D11DeviceContext* NativeContext(api::command_list* cmd_list) {
  return reinterpret_cast<ID3D11DeviceContext*>(static_cast<uintptr_t>(cmd_list->get_native()));
}

void RestoreAndRelease(ID3D11DeviceContext* ctx, SavedSlot& saved) {
  SetD3D11Binding(ctx, saved);
  if (saved.object != nullptr) saved.object->Release();
}

/// D3D11: puts every saved game binding back.
void RestoreAllSlots(api::command_list* cmd_list, CommandListState& cmd_state) {
  if (cmd_state.saved.Empty()) return;
  ID3D11DeviceContext* ctx = NativeContext(cmd_list);
  cmd_state.saved.RestoreAll([&](SavedSlot& saved) { RestoreAndRelease(ctx, saved); });
}

void OnPushDescriptors(api::command_list* cmd_list, api::shader_stage stages, api::pipeline_layout, uint32_t,
                       const api::descriptor_table_update& update) {
  // D3D11 fires this after the game's XXSetShaderResources/XXSetSamplers/CSSetUnorderedAccessViews
  // (and with every slot after ClearState). DXP's own pushes go through ReShade's API and don't.
  if (!g_any_bindings.load(std::memory_order_relaxed)) return;
  auto* cmd_state = cmd_list->get_private_data<CommandListState>();
  if (cmd_state == nullptr || cmd_state->saved.Empty()) return;
  // The game's binding replaces ours, so there's nothing left to restore for these slots.
  cmd_state->saved.DropWritten(stages, SlotClassOf(update.type), update.binding, update.count, [](SavedSlot& saved) {
    if (saved.object != nullptr) saved.object->Release();
  });
}

void OnResetCommandList(api::command_list* cmd_list) {
  // D3D11 resets a deferred context after FinishCommandList(FALSE) cleared its state, so the
  // saved bindings are released, not restored (close_command_list already restored them).
  if (auto* cmd_state = cmd_list->get_private_data<CommandListState>()) cmd_state->Reset();
}

void OnCloseCommandList(api::command_list* cmd_list) {
  // D3D11 deferred contexts: record the restore into the command list, so neither it nor a
  // context kept by FinishCommandList(TRUE) ends with DXP's bindings.
  if (cmd_list->get_device()->get_api() != api::device_api::d3d11) return;
  if (auto* cmd_state = cmd_list->get_private_data<CommandListState>()) RestoreAllSlots(cmd_list, *cmd_state);
}

void OnDestroyCommandList(api::command_list* cmd_list) {
  cmd_list->destroy_private_data<CommandListState>();
}

/// Resolves every binding to the descriptor to push. False when a provider has nothing to bind.
bool ResolveBindings(const DrawContext& context, const Replacement& replacement, std::vector<uint64_t>& values) {
  values.clear();
  for (const DrawBinding& binding : replacement.bindings) {
    const uint64_t value = binding.resolved.provider ? (*binding.resolved.provider)(context, binding.resolved.binding) : binding.resolved.descriptor;
    if (value == 0) return false;
    values.push_back(value);
  }
  return true;
}

/// Binds the replacement once per game bind, or the game's pipeline back when the
/// replacement can no longer be used.
void ApplySwap(api::command_list* cmd_list, api::pipeline_stage stages, PendingReplacement& pending, bool ready) {
  switch (NextSwapAction(ready, pending.swapped)) {
    case SwapAction::kBindReplacement: cmd_list->bind_pipeline(stages, pending.replacement->pipeline); break;
    case SwapAction::kBindOriginal:    cmd_list->bind_pipeline(stages, pending.replacement->original); break;
    case SwapAction::kNone:            break;
  }
}

/// Fills one descriptor table per injected root parameter. Gaps (registers the shader
/// does not use) repeat a valid descriptor so the copied table never contains garbage.
void PushD3D12(api::command_list* cmd_list, const Replacement& replacement, std::span<const uint64_t> values) {
  static thread_local std::vector<uint64_t> descriptors;
  const api::shader_stage stage = replacement.compute ? api::shader_stage::all_compute : api::shader_stage::all_graphics;

  for (size_t begin = 0; begin < replacement.bindings.size();) {
    size_t end = begin;
    const uint32_t param = replacement.bindings[begin].layout_param;
    while (end < replacement.bindings.size() && replacement.bindings[end].layout_param == param) ++end;

    descriptors.assign(replacement.bindings[end - 1].slot + 1, 0);
    for (size_t i = begin; i < end; ++i) descriptors[replacement.bindings[i].slot] = values[i];
    const uint64_t filler = descriptors[replacement.bindings[begin].slot];
    std::ranges::replace(descriptors, uint64_t{0}, filler);

    const api::descriptor_table_update update{
        .table = {0},
        .binding = 0,
        .array_offset = 0,
        .count = static_cast<uint32_t>(descriptors.size()),
        .type = replacement.bindings[begin].resolved.type,
        .descriptors = descriptors.data(),
    };
    cmd_list->push_descriptors(stage, replacement.layout, param, update);
    begin = end;
  }
}

/// Runs before every draw/dispatch. Never issues the draw: returns false so the game's
/// draw (and later add-ons' draw callbacks) still run.
bool HandleDraw(api::command_list* cmd_list, std::optional<bool> compute) {
  auto* cmd_state = cmd_list->get_private_data<CommandListState>();
  if (cmd_state == nullptr) return false;

  static thread_local std::vector<uint64_t> values;
  const bool enabled = g_enabled.load(std::memory_order_relaxed);

  if (cmd_list->get_device()->get_api() == api::device_api::d3d12) {
    bool pushed = false;
    PendingReplacement& pending = cmd_state->pending[kD3D12Slot];
    if (pending.replacement && (!compute || *compute == pending.replacement->compute)) {
      const Replacement& replacement = *pending.replacement;
      const DrawContext context{cmd_list, replacement.compute ? api::shader_stage::all_compute : api::shader_stage::all_graphics};
      const bool ready = enabled && ResolveBindings(context, replacement, values);
      ApplySwap(cmd_list, api::pipeline_stage::all, pending, ready);
      if (ready && !replacement.bindings.empty()) {
        PushD3D12(cmd_list, replacement, values);
        cmd_state->heaps_dirty = true;
        pushed = true;
      }
    }
    if (!pushed && cmd_state->heaps_dirty) RestoreHeaps(cmd_list, *cmd_state, compute.value_or(false) ? 1 : 0);
    return false;
  }

  // D3D11
  const std::span<const Stage> stages =
      compute.value_or(false) ? std::span<const Stage>(kD3D11ComputeStages) : std::span<const Stage>(kD3D11GraphicsStages);
  static thread_local std::vector<SlotKey> used;
  used.clear();
  ID3D11DeviceContext* ctx = NativeContext(cmd_list);
  for (const Stage stage : stages) {
    PendingReplacement& pending = cmd_state->pending[static_cast<size_t>(stage)];
    if (!pending.replacement) continue;
    const Replacement& replacement = *pending.replacement;
    const bool ready = enabled && ResolveBindings({cmd_list, ToShaderStage(stage)}, replacement, values);
    ApplySwap(cmd_list, ToPipelineStage(stage), pending, ready);
    if (!ready) continue;
    for (size_t i = 0; i < replacement.bindings.size(); ++i) {
      const DrawBinding& binding = replacement.bindings[i];
      const SlotKey key{binding.stage, SlotClassOf(binding.resolved.type), binding.slot};
      cmd_state->saved.SaveOnce(key, binding.resolved.type,
                                [&] { return GetD3D11Binding(ctx, binding.stage, binding.resolved.type, binding.slot); });
      cmd_list->push_descriptors(binding.stage, {0}, 0, {.binding = binding.slot, .count = 1, .type = binding.resolved.type, .descriptors = &values[i]});
      used.push_back(key);
    }
  }
  if (!cmd_state->saved.Empty()) {
    cmd_state->saved.RestoreUnused([](const SlotKey& key) { return std::ranges::find(used, key) != used.end(); },
                                   [&](SavedSlot& saved) { RestoreAndRelease(ctx, saved); });
  }
  return false;
}

bool OnDraw(api::command_list* cmd_list, uint32_t, uint32_t, uint32_t, uint32_t) {
  return HandleDraw(cmd_list, false);
}

bool OnDrawIndexed(api::command_list* cmd_list, uint32_t, uint32_t, uint32_t, int32_t, uint32_t) {
  return HandleDraw(cmd_list, false);
}

bool OnDispatch(api::command_list* cmd_list, uint32_t, uint32_t, uint32_t) {
  return HandleDraw(cmd_list, true);
}

bool OnDispatchMesh(api::command_list* cmd_list, uint32_t, uint32_t, uint32_t) {
  return HandleDraw(cmd_list, false);
}

bool OnDrawOrDispatchIndirect(api::command_list* cmd_list, api::indirect_command type, api::resource, uint64_t, uint32_t, uint32_t) {
  // D3D12 ExecuteIndirect may use a custom command signature (type unknown): the pending
  // pipeline is used whatever kind it is.
  const std::optional<bool> compute = type == api::indirect_command::unknown ? std::nullopt
                                                                             : std::optional<bool>(type == api::indirect_command::dispatch);
  return HandleDraw(cmd_list, compute);
}

void OnPresent(api::command_queue* queue, api::swapchain*, const api::rect*, const api::rect*, uint32_t, const api::rect*) {
  DeviceState* state = GetDeviceState(queue->get_device());
  // Fallback for runtimes that never reported a graphics queue.
  if (state != nullptr && !state->resources.load()) LoadResources(*state);
  // D3D11: the frame ends with the game's bindings on the immediate context.
  if (state != nullptr && state->api == api::device_api::d3d11) {
    if (api::command_list* cmd_list = queue->get_immediate_command_list()) {
      if (auto* cmd_state = cmd_list->get_private_data<CommandListState>()) RestoreAllSlots(cmd_list, *cmd_state);
    }
  }
  g_frame.fetch_add(1);
  ReleaseRetired(nullptr, false);
}

}  // namespace

// ── Public API ────────────────────────────────────────────────────────────────

std::shared_ptr<const Settings> GetSettings() { return g_settings.load(); }

void UpdateSettings(const Settings& settings) {
  g_enabled.store(settings.enabled);
  g_settings.store(std::make_shared<const Settings>(settings));
}

std::shared_ptr<const RecipeSet> GetRecipes() { return g_recipes.load(); }

void InitializeRuntime(HMODULE addon_module) {
  g_module = addon_module;
  const Settings settings = LoadSettings(addon_module);
  UpdateSettings(settings);

  // Env overrides saved in ReShade.ini are applied on top of each recipe's defaults.
  auto defaults = RecipeSet::Load(settings.RecipeDir(), {});
  std::unordered_map<std::string, EnvMap> overrides;
  for (const RecipeEntry& entry : defaults->Entries()) {
    for (const auto& [variable, value] : entry.env_defaults) {
      if (auto stored = LoadEnvOverride(entry.name, variable, value)) overrides[entry.name][variable] = *stored;
    }
  }
  g_recipes.store(overrides.empty() ? std::shared_ptr<const RecipeSet>(defaults) : defaults->WithOverrides(overrides));
}

void ShutdownRuntime() {
  {
    std::lock_guard lock(g_queue_mutex);
    g_stop_workers = true;
  }
  g_queue_cv.notify_all();
  for (auto& worker : g_workers) worker.join();
  g_workers.clear();
}

void ReloadAll() {
  const auto settings = GetSettings();
  const auto recipes = std::shared_ptr<const RecipeSet>(RecipeSet::Load(settings->RecipeDir(), GetRecipes()->Overrides()));
  g_recipes.store(recipes);

  std::vector<DeviceState*> devices;
  {
    std::lock_guard lock(g_devices_mutex);
    devices = g_devices;
  }
  for (DeviceState* state : devices) {
    if (state->resources.load()) {
      Retire(state->device, state->resources.exchange(ResourceSet::Load(state->device, settings->ResourceDir())));
    }
  }
  RepatchAll(recipes);
}

void ApplyEnvOverrides(const std::unordered_map<std::string, EnvMap>& overrides) {
  const auto recipes = std::shared_ptr<const RecipeSet>(GetRecipes()->WithOverrides(overrides));
  g_recipes.store(recipes);
  RepatchAll(recipes);
}

std::vector<DeviceInfo> GetDeviceInfo() {
  std::vector<DeviceInfo> infos;
  std::lock_guard lock(g_devices_mutex);
  for (DeviceState* state : g_devices) {
    DeviceInfo& info = infos.emplace_back();
    const auto count = [&](RecordStatus status) { return static_cast<size_t>(std::max<int64_t>(0, state->status_counts[static_cast<size_t>(status)].load())); };
    info.api = state->api;
    info.injection = state->injection;
    info.pending = count(RecordStatus::kPending) + count(RecordStatus::kWaitingForResources);
    info.patched = count(RecordStatus::kPatched);
    info.failed = count(RecordStatus::kFailed);
    info.unmodified = count(RecordStatus::kUnmodified);
    info.pipelines = info.pending + info.patched + info.failed + info.unmodified;
    info.layouts_extended = state->layouts_extended.load();
    info.layouts_skipped = state->layouts_skipped.load();
    info.layouts_injected = info.layouts_extended != 0;
    info.resources = state->resources.load();
    std::lock_guard error_lock(state->error_mutex);
    info.recent_errors.assign(state->recent_errors.rbegin(), state->recent_errors.rend());
  }
  return infos;
}

void RegisterEvents() {
  reshade::register_event<reshade::addon_event::init_device>(OnInitDevice);
  reshade::register_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
  reshade::register_event<reshade::addon_event::init_command_queue>(OnInitCommandQueue);
  reshade::register_event<reshade::addon_event::destroy_command_list>(OnDestroyCommandList);
  reshade::register_event<reshade::addon_event::reset_command_list>(OnResetCommandList);
  reshade::register_event<reshade::addon_event::close_command_list>(OnCloseCommandList);
  reshade::register_event<reshade::addon_event::create_pipeline_layout>(OnCreatePipelineLayout);
  reshade::register_event<reshade::addon_event::init_pipeline_layout>(OnInitPipelineLayout);
  reshade::register_event<reshade::addon_event::destroy_pipeline_layout>(OnDestroyPipelineLayout);
  reshade::register_event<reshade::addon_event::init_pipeline>(OnInitPipeline);
  reshade::register_event<reshade::addon_event::destroy_pipeline>(OnDestroyPipeline);
  reshade::register_event<reshade::addon_event::bind_pipeline>(OnBindPipeline);
  reshade::register_event<reshade::addon_event::bind_descriptor_tables>(OnBindDescriptorTables);
  reshade::register_event<reshade::addon_event::push_descriptors>(OnPushDescriptors);
  reshade::register_event<reshade::addon_event::draw>(OnDraw);
  reshade::register_event<reshade::addon_event::draw_indexed>(OnDrawIndexed);
  reshade::register_event<reshade::addon_event::dispatch>(OnDispatch);
  reshade::register_event<reshade::addon_event::dispatch_mesh>(OnDispatchMesh);
  reshade::register_event<reshade::addon_event::draw_or_dispatch_indirect>(OnDrawOrDispatchIndirect);
  reshade::register_event<reshade::addon_event::present>(OnPresent);
}

void UnregisterEvents() {
  reshade::unregister_event<reshade::addon_event::init_device>(OnInitDevice);
  reshade::unregister_event<reshade::addon_event::destroy_device>(OnDestroyDevice);
  reshade::unregister_event<reshade::addon_event::init_command_queue>(OnInitCommandQueue);
  reshade::unregister_event<reshade::addon_event::destroy_command_list>(OnDestroyCommandList);
  reshade::unregister_event<reshade::addon_event::reset_command_list>(OnResetCommandList);
  reshade::unregister_event<reshade::addon_event::close_command_list>(OnCloseCommandList);
  reshade::unregister_event<reshade::addon_event::create_pipeline_layout>(OnCreatePipelineLayout);
  reshade::unregister_event<reshade::addon_event::init_pipeline_layout>(OnInitPipelineLayout);
  reshade::unregister_event<reshade::addon_event::destroy_pipeline_layout>(OnDestroyPipelineLayout);
  reshade::unregister_event<reshade::addon_event::init_pipeline>(OnInitPipeline);
  reshade::unregister_event<reshade::addon_event::destroy_pipeline>(OnDestroyPipeline);
  reshade::unregister_event<reshade::addon_event::bind_pipeline>(OnBindPipeline);
  reshade::unregister_event<reshade::addon_event::bind_descriptor_tables>(OnBindDescriptorTables);
  reshade::unregister_event<reshade::addon_event::push_descriptors>(OnPushDescriptors);
  reshade::unregister_event<reshade::addon_event::draw>(OnDraw);
  reshade::unregister_event<reshade::addon_event::draw_indexed>(OnDrawIndexed);
  reshade::unregister_event<reshade::addon_event::dispatch>(OnDispatch);
  reshade::unregister_event<reshade::addon_event::dispatch_mesh>(OnDispatchMesh);
  reshade::unregister_event<reshade::addon_event::draw_or_dispatch_indirect>(OnDrawOrDispatchIndirect);
  reshade::unregister_event<reshade::addon_event::present>(OnPresent);
}

}  // namespace dxp::addon
