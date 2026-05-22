#pragma once

#include <cstdint>
#include <vector>

namespace dxp::sm5 {

// DXBC container header constants
static constexpr uint32_t DXBC_CONTAINER_SIGNATURE = 0x43425844; // DXBC

struct DxbcContainerHeader {
  uint32_t Signature;                    // 0x43424458 ("DXBC")
  uint32_t Hash[4];                      // Container hash/checksum (16 bytes)
  uint32_t One;                          // Usually 1
  uint32_t TotalSizeInBytes;             // Total bytes in container
  uint32_t ChunkCount;                   // Number of chunk offsets that follow
};

// DXBC chunk fourcc constants
static constexpr uint32_t DXBC_CHUNK_SHDR = 0x52444853; // "SHDR"
static constexpr uint32_t DXBC_CHUNK_SHEX = 0x58454853; // "SHEX"
static constexpr uint32_t DXBC_CHUNK_ISGN = 0x4E475349; // "ISGN"
static constexpr uint32_t DXBC_CHUNK_OSGN = 0x4E47534F; // "OSGN"
static constexpr uint32_t DXBC_CHUNK_ISG1 = 0x31475349; // "ISG1"
static constexpr uint32_t DXBC_CHUNK_OSG1 = 0x3147534F; // "OSG1"
static constexpr uint32_t DXBC_CHUNK_RDEF = 0x46454452; // "RDEF"
static constexpr uint32_t DXBC_CHUNK_PSIO = 0x4F495350; // "PSIO"
static constexpr uint32_t DXBC_CHUNK_VSIO = 0x4F495356; // "VSIO"
static constexpr uint32_t DXBC_CHUNK_GSIO = 0x4F495347; // "GSIO"
static constexpr uint32_t DXBC_CHUNK_DSIO = 0x4F495344; // "DSIO"
static constexpr uint32_t DXBC_CHUNK_HSIO = 0x4F495348; // "HSIO"
static constexpr uint32_t DXBC_CHUNK_CSIO = 0x4F495343; // "CSIO"
static constexpr uint32_t DXBC_CHUNK_SBIO = 0x4F494253; // "SBIO"
static constexpr uint32_t DXBC_CHUNK_STAT = 0x54415453; // "STAT"
static constexpr uint32_t DXBC_CHUNK_INFO = 0x4F464E49; // "INFO"
static constexpr uint32_t DXBC_CHUNK_FLAGS = 0x47414C46; // "FLAG"
static constexpr uint32_t DXBC_CHUNK_TYPE  = 0x45505954; // "TYPE"

struct DxbcChunkHeader {
  uint32_t FourCC;       // Chunk type identifier
  uint32_t ChunkSize;    // Size of chunk in bytes (including this header)
};

enum class ChunkKind {
  None,
  Shader,    // SHDR or SHEX
  RDEF,      // Resource definition
  PSIO,      // Pixel shader I/O
  VSIO,      // Vertex shader I/O
  GSIO,      // Geometry shader I/O
  DSIO,      // Domain shader I/O
  HSIO,      // Hull shader I/O
  CSIO,      // Compute shader I/O
  SBIO,      // Shader blob
  STAT,      // Statistics
  INFO,      // Info
  Flags,     // Flags
  Type,      // Type
  Unknown,
};

inline ChunkKind FourCCToChunkKind(uint32_t fourCC) {
  switch (fourCC) {
    case DXBC_CHUNK_SHDR: return ChunkKind::Shader;
    case DXBC_CHUNK_SHEX: return ChunkKind::Shader;
    case DXBC_CHUNK_RDEF: return ChunkKind::RDEF;
    case DXBC_CHUNK_PSIO: return ChunkKind::PSIO;
    case DXBC_CHUNK_VSIO: return ChunkKind::VSIO;
    case DXBC_CHUNK_GSIO: return ChunkKind::GSIO;
    case DXBC_CHUNK_DSIO: return ChunkKind::DSIO;
    case DXBC_CHUNK_HSIO: return ChunkKind::HSIO;
    case DXBC_CHUNK_CSIO: return ChunkKind::CSIO;
    case DXBC_CHUNK_SBIO: return ChunkKind::SBIO;
    case DXBC_CHUNK_STAT: return ChunkKind::STAT;
    case DXBC_CHUNK_INFO: return ChunkKind::INFO;
    case DXBC_CHUNK_FLAGS: return ChunkKind::Flags;
    case DXBC_CHUNK_TYPE: return ChunkKind::Type;
    default: return ChunkKind::Unknown;
  }
}

inline const char* ChunkKindToFourCC(ChunkKind kind) {
  switch (kind) {
    case ChunkKind::Shader: return "SHDR";
    case ChunkKind::RDEF: return "RDEF";
    case ChunkKind::PSIO: return "PSIO";
    case ChunkKind::VSIO: return "VSIO";
    case ChunkKind::GSIO: return "GSIO";
    case ChunkKind::DSIO: return "DSIO";
    case ChunkKind::HSIO: return "HSIO";
    case ChunkKind::CSIO: return "CSIO";
    case ChunkKind::SBIO: return "SBIO";
    case ChunkKind::STAT: return "STAT";
    case ChunkKind::INFO: return "INFO";
    case ChunkKind::Flags: return "FLAG";
    case ChunkKind::Type: return "TYPE";
    default: return "????";
  }
}

struct DxbcChunk {
  uint32_t FourCC;
  ChunkKind Kind;
  std::vector<uint8_t> Data;   // Raw chunk bytes (excluding header)
  uint32_t OffsetInContainer;  // Byte offset in container
};

struct Container {
  DxbcContainerHeader Header;
  std::vector<DxbcChunk> Chunks;
  std::vector<uint8_t> RawBytes; // Original bytes for unknown chunks

  /// Find a chunk by kind. Returns nullptr if not found.
  const DxbcChunk* FindChunk(ChunkKind kind) const;
  DxbcChunk* FindChunk(ChunkKind kind);

  /// Find a chunk by fourcc string. Returns nullptr if not found.
  const DxbcChunk* FindChunkByFourCC(uint32_t fourCC) const;
  DxbcChunk* FindChunkByFourCC(uint32_t fourCC);

  /// Get the shader chunk (SHDR or SHEX). Returns nullptr if not found.
  const DxbcChunk* GetShaderChunk() const;
  DxbcChunk* GetShaderChunk();
};

/// Parse a DXBC container from raw bytes.
/// Returns false if the data is not a valid DXBC container.
bool ParseDxbcContainer(const std::vector<uint8_t> &bytes, Container &container);

/// Serialize a DXBC container back to raw bytes.
/// Rebuilds the container with updated chunk data.
bool SerializeDxbcContainer(const Container &container, std::vector<uint8_t> &outBytes);

/// Recompute the DXBC container hash after rebuilding.
/// Call this after SerializeDxbcContainer to finalize the hash.
bool RecomputeDxbcHash(std::vector<uint8_t> &containerBytes);

} // namespace dxp::sm5
