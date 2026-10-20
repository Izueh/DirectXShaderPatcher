#pragma once

// Draw-time replacement bookkeeping. Kept free of device calls so it can be unit tested.

#include <algorithm>
#include <cstdint>
#include <vector>

#include "common.hpp"

namespace dxp::addon {

/// What the draw path does with the pipeline before the game's draw runs.
enum class SwapAction : uint8_t { kNone, kBindReplacement, kBindOriginal };

/// `ready`: the replacement can be used for this draw (DXP enabled, every binding resolved).
/// `swapped`: the replacement is currently bound in place of the game's pipeline; updated.
constexpr SwapAction NextSwapAction(bool ready, bool& swapped) {
  if (ready == swapped) return SwapAction::kNone;
  swapped = ready;
  return ready ? SwapAction::kBindReplacement : SwapAction::kBindOriginal;
}

/// Register class of a descriptor type, as D3D11 slots see it.
enum class SlotClass : uint8_t { kOther, kSrv, kUav, kSampler };

constexpr SlotClass SlotClassOf(api::descriptor_type type) {
  switch (type) {
    case api::descriptor_type::sampler:                      return SlotClass::kSampler;
    case api::descriptor_type::shader_resource_view:
    case api::descriptor_type::buffer_shader_resource_view:  return SlotClass::kSrv;
    case api::descriptor_type::unordered_access_view:
    case api::descriptor_type::buffer_unordered_access_view: return SlotClass::kUav;
    default:                                                 return SlotClass::kOther;
  }
}

struct SlotKey {
  api::shader_stage stage = api::shader_stage::all;  // a single stage
  SlotClass slot_class = SlotClass::kOther;
  uint32_t slot = 0;
  bool operator==(const SlotKey&) const = default;
};

/// D3D11 slots DXP pushed a resource into, each holding the game's binding saved before the
/// first push. The draw stays the game's (it is never re-issued), so the game's binding is put
/// back lazily: at the first draw that doesn't use the slot, at close/present, or never when the
/// game writes the slot itself. `Object` is the saved binding (a COM pointer holding a reference).
template <typename Object>
class SavedSlots {
 public:
  struct Entry {
    SlotKey key;
    api::descriptor_type type = api::descriptor_type::shader_resource_view;
    Object object{};
  };

  [[nodiscard]] bool Empty() const { return entries_.empty(); }
  [[nodiscard]] size_t Size() const { return entries_.size(); }
  [[nodiscard]] bool Contains(const SlotKey& key) const {
    return std::ranges::any_of(entries_, [&](const Entry& entry) { return entry.key == key; });
  }

  /// Saves the game's binding before DXP's first push into the slot. Once saved, the slot
  /// holds DXP's binding, so later pushes keep the first save.
  template <typename Save>
  void SaveOnce(const SlotKey& key, api::descriptor_type type, Save&& save) {
    if (!Contains(key)) entries_.push_back({key, type, save()});
  }

  /// The game bound [first, first + count) of `cls` in `stages` itself: its binding wins and
  /// the saved one is released without being restored.
  template <typename Release>
  void DropWritten(api::shader_stage stages, SlotClass cls, uint32_t first, uint32_t count, Release&& release) {
    const uint64_t end = uint64_t{first} + count;
    std::erase_if(entries_, [&](Entry& entry) {
      if ((entry.key.stage & stages) == 0 || entry.key.slot_class != cls || entry.key.slot < first || entry.key.slot >= end) return false;
      release(entry);
      return true;
    });
  }

  /// Restores (and forgets) every saved slot for which `used(key)` is false. `restore` binds
  /// the saved object back and releases it.
  template <typename Used, typename Restore>
  void RestoreUnused(Used&& used, Restore&& restore) {
    std::erase_if(entries_, [&](Entry& entry) {
      if (used(entry.key)) return false;
      restore(entry);
      return true;
    });
  }

  template <typename Restore>
  void RestoreAll(Restore&& restore) {
    RestoreUnused([](const SlotKey&) { return false; }, restore);
  }

  /// Releases every saved binding without restoring it (the context state was cleared).
  template <typename Release>
  void Clear(Release&& release) {
    for (Entry& entry : entries_) release(entry);
    entries_.clear();
  }

 private:
  std::vector<Entry> entries_;
};

}  // namespace dxp::addon
