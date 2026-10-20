#include "dds.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace dxp::addon {

namespace {

constexpr uint32_t MakeFourCC(char a, char b, char c, char d) {
  return static_cast<uint32_t>(static_cast<uint8_t>(a)) | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8) |
         (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16) | (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
}

#pragma pack(push, 1)
struct DdsPixelFormat {
  uint32_t size;
  uint32_t flags;
  uint32_t four_cc;
  uint32_t rgb_bit_count;
  uint32_t r_mask;
  uint32_t g_mask;
  uint32_t b_mask;
  uint32_t a_mask;
};

struct DdsHeader {
  uint32_t size;
  uint32_t flags;
  uint32_t height;
  uint32_t width;
  uint32_t pitch_or_linear_size;
  uint32_t depth;
  uint32_t mip_map_count;
  uint32_t reserved1[11];
  DdsPixelFormat format;
  uint32_t caps;
  uint32_t caps2;
  uint32_t caps3;
  uint32_t caps4;
  uint32_t reserved2;
};

struct DdsHeaderDx10 {
  uint32_t dxgi_format;
  uint32_t resource_dimension;
  uint32_t misc_flag;
  uint32_t array_size;
  uint32_t misc_flags2;
};
#pragma pack(pop)

static_assert(sizeof(DdsHeader) == 124);
static_assert(sizeof(DdsHeaderDx10) == 20);

constexpr uint32_t kDdsMagic = MakeFourCC('D', 'D', 'S', ' ');
constexpr uint32_t kDdsdMipMapCount = 0x20000;
constexpr uint32_t kDdsdDepth = 0x800000;
constexpr uint32_t kDdpfAlphaPixels = 0x1;
constexpr uint32_t kDdpfAlpha = 0x2;
constexpr uint32_t kDdpfFourCC = 0x4;
constexpr uint32_t kDdpfRgb = 0x40;
constexpr uint32_t kDdpfLuminance = 0x20000;
constexpr uint32_t kDdsCaps2Cubemap = 0x200;
constexpr uint32_t kDdsCaps2Volume = 0x200000;
constexpr uint32_t kDx10MiscTextureCube = 0x4;

// Values are DXGI_FORMAT; reshade::api::format uses the same numbering.
uint32_t FormatFromLegacy(const DdsPixelFormat& pf) {
  if ((pf.flags & kDdpfFourCC) != 0) {
    switch (pf.four_cc) {
      case MakeFourCC('D', 'X', 'T', '1'): return 71;  // BC1_UNORM
      case MakeFourCC('D', 'X', 'T', '2'):
      case MakeFourCC('D', 'X', 'T', '3'): return 74;  // BC2_UNORM
      case MakeFourCC('D', 'X', 'T', '4'):
      case MakeFourCC('D', 'X', 'T', '5'): return 77;  // BC3_UNORM
      case MakeFourCC('A', 'T', 'I', '1'):
      case MakeFourCC('B', 'C', '4', 'U'): return 80;  // BC4_UNORM
      case MakeFourCC('B', 'C', '4', 'S'): return 81;  // BC4_SNORM
      case MakeFourCC('A', 'T', 'I', '2'):
      case MakeFourCC('B', 'C', '5', 'U'): return 83;  // BC5_UNORM
      case MakeFourCC('B', 'C', '5', 'S'): return 84;  // BC5_SNORM
      // D3DFORMAT numeric codes stored in the FourCC field.
      case 36:  return 11;  // R16G16B16A16_UNORM
      case 110: return 13;  // R16G16B16A16_SNORM
      case 111: return 54;  // R16_FLOAT
      case 112: return 34;  // R16G16_FLOAT
      case 113: return 10;  // R16G16B16A16_FLOAT
      case 114: return 41;  // R32_FLOAT
      case 115: return 16;  // R32G32_FLOAT
      case 116: return 2;   // R32G32B32A32_FLOAT
      default:  return 0;
    }
  }

  const auto masks = [&](uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    return pf.r_mask == r && pf.g_mask == g && pf.b_mask == b && pf.a_mask == a;
  };
  if ((pf.flags & kDdpfRgb) != 0) {
    const bool has_alpha = (pf.flags & kDdpfAlphaPixels) != 0;
    switch (pf.rgb_bit_count) {
      case 32:
        if (masks(0xff, 0xff00, 0xff0000, has_alpha ? 0xff000000 : 0)) return 28;  // R8G8B8A8_UNORM (no X8 variant)
        if (masks(0xff0000, 0xff00, 0xff, 0xff000000)) return 87;                                  // B8G8R8A8_UNORM
        if (masks(0xff0000, 0xff00, 0xff, 0)) return 88;                                           // B8G8R8X8_UNORM
        if (masks(0x3ff, 0xffc00, 0x3ff00000, has_alpha ? 0xc0000000 : 0)) return 24;              // R10G10B10A2_UNORM
        if (masks(0xffff, 0xffff0000, 0, 0)) return 35;                                            // R16G16_UNORM
        if (masks(0xffffffff, 0, 0, 0)) return 41;                                                 // R32_FLOAT
        break;
      case 16:
        if (masks(0xf800, 0x07e0, 0x001f, 0)) return 85;       // B5G6R5_UNORM
        if (masks(0x7c00, 0x03e0, 0x001f, 0x8000)) return 86;  // B5G5R5A1_UNORM
        if (masks(0x0f00, 0x00f0, 0x000f, 0xf000)) return 115; // B4G4R4A4_UNORM
        break;
      default:
        break;
    }
  }
  if ((pf.flags & kDdpfLuminance) != 0) {
    if (pf.rgb_bit_count == 8) return 61;                                    // R8_UNORM
    if (pf.rgb_bit_count == 16 && (pf.flags & kDdpfAlphaPixels) != 0) return 49;  // R8G8_UNORM
    if (pf.rgb_bit_count == 16) return 56;                                   // R16_UNORM
  }
  if ((pf.flags & kDdpfAlpha) != 0 && pf.rgb_bit_count == 8) return 65;  // A8_UNORM
  return 0;
}

/// Bytes per 4x4 block for block-compressed formats, 0 otherwise.
uint32_t BlockBytes(uint32_t format) {
  if ((format >= 70 && format <= 72) || (format >= 79 && format <= 81)) return 8;   // BC1, BC4
  if ((format >= 73 && format <= 78) || (format >= 82 && format <= 84) || (format >= 94 && format <= 99)) return 16;  // BC2/3/5/6H/7
  return 0;
}

/// Bits per pixel for uncompressed formats, 0 if unsupported.
uint32_t BitsPerPixel(uint32_t format) {
  if (format >= 1 && format <= 4) return 128;
  if (format >= 5 && format <= 8) return 96;
  if (format >= 9 && format <= 22) return 64;
  if (format >= 23 && format <= 47) return 32;
  if (format >= 48 && format <= 59) return 16;
  if (format >= 60 && format <= 65) return 8;
  if (format == 67) return 32;               // R9G9B9E5_SHAREDEXP
  if (format == 85 || format == 86 || format == 115) return 16;
  if (format >= 87 && format <= 93) return 32;
  return 0;
}

struct SurfaceSize {
  uint32_t row_pitch = 0;
  uint32_t rows = 0;
};

SurfaceSize ComputeSurface(uint32_t format, uint32_t width, uint32_t height) {
  if (const uint32_t block = BlockBytes(format); block != 0) {
    return {std::max(1u, (width + 3) / 4) * block, std::max(1u, (height + 3) / 4)};
  }
  return {(width * BitsPerPixel(format) + 7) / 8, height};
}

}  // namespace

std::expected<DdsImage, std::string> LoadDds(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) return std::unexpected("cannot open file");
  const auto file_size = static_cast<size_t>(file.tellg());
  file.seekg(0);
  std::vector<uint8_t> data(file_size);
  if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(file_size))) {
    return std::unexpected("read failed");
  }

  if (data.size() < 4 + sizeof(DdsHeader)) return std::unexpected("file too small");
  uint32_t magic = 0;
  std::memcpy(&magic, data.data(), 4);
  if (magic != kDdsMagic) return std::unexpected("not a DDS file");

  DdsHeader header{};
  std::memcpy(&header, data.data() + 4, sizeof(header));
  size_t offset = 4 + sizeof(DdsHeader);

  uint32_t format = 0;
  uint32_t array_size = 1;
  bool cube = false;
  api::resource_type type = api::resource_type::texture_2d;
  const uint32_t width = std::max(1u, header.width);
  const uint32_t height = std::max(1u, header.height);
  uint32_t depth = 1;

  if ((header.format.flags & kDdpfFourCC) != 0 && header.format.four_cc == MakeFourCC('D', 'X', '1', '0')) {
    if (data.size() < offset + sizeof(DdsHeaderDx10)) return std::unexpected("truncated DX10 header");
    DdsHeaderDx10 dx10{};
    std::memcpy(&dx10, data.data() + offset, sizeof(dx10));
    offset += sizeof(dx10);
    format = dx10.dxgi_format;
    array_size = std::max(1u, dx10.array_size);
    switch (dx10.resource_dimension) {
      case 2: type = api::resource_type::texture_1d; break;
      case 3: type = api::resource_type::texture_2d; break;
      case 4:
        type = api::resource_type::texture_3d;
        depth = std::max(1u, header.depth);
        break;
      default: return std::unexpected("unsupported DX10 resource dimension");
    }
    cube = (dx10.misc_flag & kDx10MiscTextureCube) != 0;
  } else {
    format = FormatFromLegacy(header.format);
    if ((header.caps2 & kDdsCaps2Volume) != 0 && (header.flags & kDdsdDepth) != 0) {
      type = api::resource_type::texture_3d;
      depth = std::max(1u, header.depth);
    } else if ((header.caps2 & kDdsCaps2Cubemap) != 0) {
      cube = true;
    }
  }

  if (format == 0 || (BlockBytes(format) == 0 && BitsPerPixel(format) == 0)) {
    return std::unexpected(std::format("unsupported pixel format (DXGI {})", format));
  }

  const uint32_t mip_count = (header.flags & kDdsdMipMapCount) != 0 ? std::max(1u, header.mip_map_count) : 1u;
  const uint32_t layers = cube ? array_size * 6 : array_size;
  if (type == api::resource_type::texture_3d && layers != 1) return std::unexpected("3D texture arrays are not supported");

  DdsImage image;
  image.desc = api::resource_desc(type, width, height, static_cast<uint16_t>(type == api::resource_type::texture_3d ? depth : layers),
                                  static_cast<uint16_t>(mip_count), static_cast<api::format>(format), 1,
                                  api::memory_heap::gpu_only, api::resource_usage::shader_resource | api::resource_usage::copy_dest);
  if (cube) image.desc.flags |= api::resource_flags::cube_compatible;

  if (type == api::resource_type::texture_3d) {
    image.view_type = api::resource_view_type::texture_3d;
  } else if (cube) {
    image.view_type = array_size > 1 ? api::resource_view_type::texture_cube_array : api::resource_view_type::texture_cube;
  } else if (type == api::resource_type::texture_1d) {
    image.view_type = layers > 1 ? api::resource_view_type::texture_1d_array : api::resource_view_type::texture_1d;
  } else {
    image.view_type = layers > 1 ? api::resource_view_type::texture_2d_array : api::resource_view_type::texture_2d;
  }

  // Validate the payload size before handing out pointers into it.
  size_t required = 0;
  for (uint32_t layer = 0; layer < layers; ++layer) {
    for (uint32_t mip = 0; mip < mip_count; ++mip) {
      const SurfaceSize surface = ComputeSurface(format, std::max(1u, width >> mip), std::max(1u, height >> mip));
      required += static_cast<size_t>(surface.row_pitch) * surface.rows * std::max(1u, depth >> mip);
    }
  }
  if (data.size() - offset < required) {
    return std::unexpected(std::format("truncated pixel data ({} of {} bytes)", data.size() - offset, required));
  }

  image.pixels.assign(data.begin() + static_cast<std::ptrdiff_t>(offset), data.begin() + static_cast<std::ptrdiff_t>(offset + required));
  image.subresources.reserve(static_cast<size_t>(layers) * mip_count);
  size_t cursor = 0;
  for (uint32_t layer = 0; layer < layers; ++layer) {
    for (uint32_t mip = 0; mip < mip_count; ++mip) {
      const SurfaceSize surface = ComputeSurface(format, std::max(1u, width >> mip), std::max(1u, height >> mip));
      const uint32_t slice_pitch = surface.row_pitch * surface.rows;
      image.subresources.push_back({image.pixels.data() + cursor, surface.row_pitch, slice_pitch});
      cursor += static_cast<size_t>(slice_pitch) * std::max(1u, depth >> mip);
    }
  }
  return image;
}

}  // namespace dxp::addon
