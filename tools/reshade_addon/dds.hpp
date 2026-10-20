#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

#include "common.hpp"

namespace dxp::addon {

/// A DDS file decoded into a ReShade resource description plus initial data.
/// `subresources` point into `pixels` and are ordered the D3D way
/// (mip + array_layer * mip_count), which is what device::create_resource expects.
struct DdsImage {
  api::resource_desc desc;
  api::resource_view_type view_type = api::resource_view_type::texture_2d;
  std::vector<uint8_t> pixels;
  std::vector<api::subresource_data> subresources;
};

/// Supports DX10-extended headers and the common legacy FourCC/bitmask formats
/// (BC1-BC5, RGBA8/BGRA8, 16/32-bit float). 1D/2D/3D, cube maps, arrays and mips.
std::expected<DdsImage, std::string> LoadDds(const std::filesystem::path& path);

}  // namespace dxp::addon
