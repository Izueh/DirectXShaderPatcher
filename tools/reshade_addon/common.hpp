#pragma once

// Shared include block for the DXP ReShade add-on. ImGui must be included before
// reshade.hpp so ReShade routes ImGui calls through its function table, and
// ImTextureID must match the size of reshade::api::resource_view.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#define ImTextureID ImU64
#include <imgui.h>
#include <reshade.hpp>

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

#include "dxp/Logging.hpp"

namespace dxp::addon {

namespace api = reshade::api;

/// Shader stages the add-on can patch. The order is used as an index into
/// per-stage arrays (recipe directories, D3D11 command list slots).
enum class Stage : uint8_t {
  kVertex,
  kHull,
  kDomain,
  kGeometry,
  kPixel,
  kCompute,
  kAmplification,
  kMesh,
  kCount,
};

inline constexpr size_t kStageCount = static_cast<size_t>(Stage::kCount);

enum class ShaderModel : uint8_t {
  kSm5,  // DXBC (SM4/SM5)
  kSm6,  // DXIL
};

inline const char* StageName(Stage stage) {
  constexpr const char* kNames[] = {"vs", "hs", "ds", "gs", "ps", "cs", "as", "ms"};
  return stage < Stage::kCount ? kNames[static_cast<size_t>(stage)] : "??";
}

inline api::shader_stage ToShaderStage(Stage stage) {
  constexpr api::shader_stage kStages[] = {
      api::shader_stage::vertex, api::shader_stage::hull, api::shader_stage::domain,
      api::shader_stage::geometry, api::shader_stage::pixel, api::shader_stage::compute,
      api::shader_stage::amplification, api::shader_stage::mesh};
  return kStages[static_cast<size_t>(stage)];
}

template <typename... Args>
void Log(reshade::log::level level, std::format_string<Args...> fmt, Args&&... args) {
  const std::string message = "[DXP] " + std::format(fmt, std::forward<Args>(args)...);
  reshade::log::message(level, message.c_str());
}

inline reshade::log::level ToReShadeLevel(dxp::LogLevel level) {
  switch (level) {
    case dxp::LogLevel::Error:   return reshade::log::level::error;
    case dxp::LogLevel::Warning: return reshade::log::level::warning;
    case dxp::LogLevel::Info:    return reshade::log::level::info;
    default:                     return reshade::log::level::debug;
  }
}

}  // namespace dxp::addon
