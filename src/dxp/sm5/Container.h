#pragma once

#include <cstdint>
#include <vector>

namespace dxp::sm5 {

struct DxbcContainerHeader {
  uint32_t Signature;
  uint32_t Hash[4];
  uint32_t One;
  uint32_t TotalSizeInBytes;
  uint32_t ChunkCount;
};

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

struct DxbcChunk {
  uint32_t FourCC;
  ChunkKind Kind;
  std::vector<uint8_t> Data;
  uint32_t OffsetInContainer;
};

struct Container {
  DxbcContainerHeader Header;
  std::vector<DxbcChunk> Chunks;
  std::vector<uint8_t> RawBytes;

  const DxbcChunk *FindChunk(ChunkKind kind) const;
  DxbcChunk *FindChunk(ChunkKind kind);

  const DxbcChunk *FindChunkByFourCC(uint32_t fourCC) const;
  DxbcChunk *FindChunkByFourCC(uint32_t fourCC);

  const DxbcChunk *GetShaderChunk() const;
  DxbcChunk *GetShaderChunk();
};

bool ParseDxbcContainer(const std::vector<uint8_t> &bytes,
                        Container &container);

bool SerializeDxbcContainer(const Container &container,
                            std::vector<uint8_t> &outBytes);

bool RecomputeDxbcHash(std::vector<uint8_t> &containerBytes);

} // namespace dxp::sm5
