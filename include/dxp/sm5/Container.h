#pragma once

#include <cstdint>
#include <vector>

namespace dxp::sm5 {

/// @brief Describes the fixed header at the start of a DXBC container.
struct DxbcContainerHeader {
  uint32_t Signature;
  uint32_t Hash[4];
  uint32_t One;
  uint32_t TotalSizeInBytes;
  uint32_t ChunkCount;
};

/// @brief Identifies known DXBC chunk categories.
enum class ChunkKind {
  None,
  Shader,
  RDEF,
  PSIO,
  VSIO,
  GSIO,
  DSIO,
  HSIO,
  CSIO,
  SBIO,
  STAT,
  INFO,
  Flags,
  Type,
  Unknown,
};

/// @brief Stores one DXBC chunk and its payload bytes.
struct DxbcChunk {
  uint32_t FourCC;
  ChunkKind Kind;
  std::vector<uint8_t> Data;
  uint32_t OffsetInContainer;
};

/// @brief Represents a parsed DXBC container.
struct Container {
  DxbcContainerHeader Header;
  std::vector<DxbcChunk> Chunks;
  std::vector<uint8_t> RawBytes;

  /// @brief Finds the first chunk with the requested kind.
  /// @param kind Chunk kind to search for.
  /// @return The matching chunk, or `nullptr` when no chunk matches.
  const DxbcChunk *FindChunk(ChunkKind kind) const;
  DxbcChunk *FindChunk(ChunkKind kind);

  /// @brief Finds the first chunk with the requested FourCC.
  /// @param fourCC FourCC identifier to search for.
  /// @return The matching chunk, or `nullptr` when no chunk matches.
  const DxbcChunk *FindChunkByFourCC(uint32_t fourCC) const;
  DxbcChunk *FindChunkByFourCC(uint32_t fourCC);

  /// @brief Returns the shader chunk payload.
  /// @return The SHDR or SHEX chunk, or `nullptr` when not present.
  const DxbcChunk *GetShaderChunk() const;
  DxbcChunk *GetShaderChunk();
};

/// @brief Parses a DXBC container from raw bytes.
/// @param bytes Input container bytes.
/// @param container Receives the parsed container state.
/// @return `true` on success, or `false` when the input is not a valid DXBC
/// container.
bool ParseDxbcContainer(const std::vector<uint8_t> &bytes,
                        Container &container);

/// @brief Serializes a DXBC container back to bytes.
/// @param container Parsed container to serialize.
/// @param outBytes Receives the rebuilt container bytes.
/// @return `true` on success, or `false` when serialization fails.
bool SerializeDxbcContainer(const Container &container,
                            std::vector<uint8_t> &outBytes);

/// @brief Recomputes the container hash for serialized DXBC bytes.
/// @param containerBytes Container bytes to update in place.
/// @return `true` on success, or `false` when the input cannot be hashed.
bool RecomputeDxbcHash(std::vector<uint8_t> &containerBytes);

} // namespace dxp::sm5
