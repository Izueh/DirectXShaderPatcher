#include "dxp/sm5/Container.h"

#include "llvm/Support/MD5.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

constexpr uint32_t DXBC_CONTAINER_SIGNATURE = 0x43425844;
constexpr uint32_t DXBC_CHUNK_SHDR = 0x52444853;
constexpr uint32_t DXBC_CHUNK_SHEX = 0x58454853;
constexpr uint32_t DXBC_CHUNK_RDEF = 0x46454452;
constexpr uint32_t DXBC_CHUNK_PSIO = 0x4F495350;
constexpr uint32_t DXBC_CHUNK_VSIO = 0x4F495356;
constexpr uint32_t DXBC_CHUNK_GSIO = 0x4F495347;
constexpr uint32_t DXBC_CHUNK_DSIO = 0x4F495344;
constexpr uint32_t DXBC_CHUNK_HSIO = 0x4F495348;
constexpr uint32_t DXBC_CHUNK_CSIO = 0x4F495343;
constexpr uint32_t DXBC_CHUNK_SBIO = 0x4F494253;
constexpr uint32_t DXBC_CHUNK_STAT = 0x54415453;
constexpr uint32_t DXBC_CHUNK_INFO = 0x4F464E49;
constexpr uint32_t DXBC_CHUNK_FLAGS = 0x47414C46;
constexpr uint32_t DXBC_CHUNK_TYPE = 0x45505954;

struct DxbcChunkHeader {
  uint32_t FourCC;
  uint32_t ChunkSize;
};

static dxp::sm5::ChunkKind FourCCToChunkKind(uint32_t fourCC) {
  using dxp::sm5::ChunkKind;
  switch (fourCC) {
  case DXBC_CHUNK_SHDR:
  case DXBC_CHUNK_SHEX:
    return ChunkKind::Shader;
  case DXBC_CHUNK_RDEF:
    return ChunkKind::RDEF;
  case DXBC_CHUNK_PSIO:
    return ChunkKind::PSIO;
  case DXBC_CHUNK_VSIO:
    return ChunkKind::VSIO;
  case DXBC_CHUNK_GSIO:
    return ChunkKind::GSIO;
  case DXBC_CHUNK_DSIO:
    return ChunkKind::DSIO;
  case DXBC_CHUNK_HSIO:
    return ChunkKind::HSIO;
  case DXBC_CHUNK_CSIO:
    return ChunkKind::CSIO;
  case DXBC_CHUNK_SBIO:
    return ChunkKind::SBIO;
  case DXBC_CHUNK_STAT:
    return ChunkKind::STAT;
  case DXBC_CHUNK_INFO:
    return ChunkKind::INFO;
  case DXBC_CHUNK_FLAGS:
    return ChunkKind::Flags;
  case DXBC_CHUNK_TYPE:
    return ChunkKind::Type;
  default:
    return ChunkKind::Unknown;
  }
}

void ComputeDXBCHash(const uint8_t *data, uint32_t byteCount,
                     uint8_t *outHash) {
  using llvm::MD5;
  constexpr size_t BlockSize = 64u;

  const uint8_t *bytes = data;
  size_t size = static_cast<size_t>(byteCount);

  const uint32_t aNum = static_cast<uint32_t>(size) * 8u;
  const uint32_t bNum = (aNum >> 2u) | 1u;

  std::array<uint8_t, sizeof(uint32_t)> a = {};
  std::array<uint8_t, sizeof(uint32_t)> b = {};
  for (uint32_t i = 0; i < sizeof(uint32_t); ++i) {
    a[i] = static_cast<uint8_t>((aNum >> (8u * i)) & 0xffu);
    b[i] = static_cast<uint8_t>((bNum >> (8u * i)) & 0xffu);
  }

  size_t remainder = size % BlockSize;
  size_t paddingSize = BlockSize - remainder;

  MD5 md5;
  if (size > remainder)
    md5.update(llvm::ArrayRef<uint8_t>(bytes, size - remainder));

  static const std::array<uint8_t, BlockSize> s_padding = {0x80};

  if (remainder >= 56u) {
    if (remainder)
      md5.update(llvm::ArrayRef<uint8_t>(bytes + size - remainder, remainder));

    md5.update(llvm::ArrayRef<uint8_t>(s_padding.data(), paddingSize));

    md5.update(llvm::ArrayRef<uint8_t>(a.data(), a.size()));
    md5.update(llvm::ArrayRef<uint8_t>(s_padding.data() + a.size(),
                                       s_padding.size() - a.size() - b.size()));
    md5.update(llvm::ArrayRef<uint8_t>(b.data(), b.size()));
  } else {
    md5.update(llvm::ArrayRef<uint8_t>(a.data(), a.size()));

    if (remainder)
      md5.update(llvm::ArrayRef<uint8_t>(bytes + size - remainder, remainder));

    md5.update(llvm::ArrayRef<uint8_t>(s_padding.data(),
                                       paddingSize - a.size() - b.size()));

    md5.update(llvm::ArrayRef<uint8_t>(b.data(), b.size()));
  }

  MD5::MD5Result result;
  md5.final(result);
  std::memcpy(outHash, result, 16);
}

} // namespace

namespace dxp::sm5 {

const DxbcChunk *Container::FindChunk(ChunkKind kind) const {
  for (const auto &chunk : Chunks) {
    if (chunk.Kind == kind) {
      return &chunk;
    }
  }
  return nullptr;
}

DxbcChunk *Container::FindChunk(ChunkKind kind) {
  for (auto &chunk : Chunks) {
    if (chunk.Kind == kind) {
      return &chunk;
    }
  }
  return nullptr;
}

const DxbcChunk *Container::FindChunkByFourCC(uint32_t fourCC) const {
  for (const auto &chunk : Chunks) {
    if (chunk.FourCC == fourCC) {
      return &chunk;
    }
  }
  return nullptr;
}

DxbcChunk *Container::FindChunkByFourCC(uint32_t fourCC) {
  for (auto &chunk : Chunks) {
    if (chunk.FourCC == fourCC) {
      return &chunk;
    }
  }
  return nullptr;
}

const DxbcChunk *Container::GetShaderChunk() const {
  for (const auto &chunk : Chunks) {
    if (chunk.Kind == ChunkKind::Shader) {
      return &chunk;
    }
  }
  return nullptr;
}

DxbcChunk *Container::GetShaderChunk() {
  for (auto &chunk : Chunks) {
    if (chunk.Kind == ChunkKind::Shader) {
      return &chunk;
    }
  }
  return nullptr;
}

static bool ReadBytes(const std::vector<uint8_t> &bytes, size_t offset,
                      uint32_t &value) {
  if (offset + 4 > bytes.size()) {
    return false;
  }
  std::memcpy(&value, bytes.data() + offset, 4);
  return true;
}

static bool ReadBytes(const std::vector<uint8_t> &bytes, size_t offset,
                      uint64_t &value) {
  if (offset + 8 > bytes.size()) {
    return false;
  }
  std::memcpy(&value, bytes.data() + offset, 8);
  return true;
}

bool ParseDxbcContainer(const std::vector<uint8_t> &bytes,
                        Container &container) {

  container = Container{};
  container.RawBytes = bytes;

  if (bytes.size() < 32) {
    std::cerr << "[sm5] DXBC container too small: " << bytes.size()
              << " bytes\n";
    return false;
  }

  size_t offset = 0;
  if (!ReadBytes(bytes, offset, container.Header.Signature))
    return false;
  offset += 4;
  for (int i = 0; i < 4; ++i) {
    if (!ReadBytes(bytes, offset, container.Header.Hash[i]))
      return false;
    offset += 4;
  }
  if (!ReadBytes(bytes, offset, container.Header.One))
    return false;
  offset += 4;
  if (!ReadBytes(bytes, offset, container.Header.TotalSizeInBytes))
    return false;
  offset += 4;
  if (!ReadBytes(bytes, offset, container.Header.ChunkCount))
    return false;
  offset += 4;

  if (container.Header.Signature != DXBC_CONTAINER_SIGNATURE) {
    std::cerr << "[sm5] Invalid DXBC signature: 0x" << std::hex
              << container.Header.Signature << "\n";
    return false;
  }

  if (container.Header.TotalSizeInBytes != bytes.size()) {
    std::cerr << "[sm5] DXBC byte count mismatch: header says "
              << container.Header.TotalSizeInBytes << ", actual "
              << bytes.size() << "\n";
  }

  uint32_t chunkCount = container.Header.ChunkCount;
  if (offset + (chunkCount * 4) > bytes.size()) {
    std::cerr << "[sm5] Chunk table extends beyond container\n";
    return false;
  }

  std::vector<uint32_t> chunkOffsets;
  chunkOffsets.reserve(chunkCount);
  for (uint32_t i = 0; i < chunkCount; ++i) {
    uint32_t offsetValue;
    if (!ReadBytes(bytes, offset + (i * 4), offsetValue)) {
      std::cerr << "[sm5] Failed to read chunk table entry " << i << "\n";
      return false;
    }
    chunkOffsets.push_back(offsetValue);
  }

  std::vector<uint32_t> sortedOffsets = chunkOffsets;
  std::sort(sortedOffsets.begin(), sortedOffsets.end());

  container.Chunks.reserve(sortedOffsets.size());
  for (size_t i = 0; i < sortedOffsets.size(); ++i) {
    uint32_t chunkOffset = sortedOffsets[i];

    if (chunkOffset + 8 > bytes.size()) {
      std::cerr << "[sm5] Chunk header extends beyond container at offset "
                << chunkOffset << "\n";
      continue;
    }

    uint32_t fourCC;
    uint32_t chunkSize;
    std::memcpy(&fourCC, bytes.data() + chunkOffset, 4);
    std::memcpy(&chunkSize, bytes.data() + chunkOffset + 4, 4);

    if (static_cast<size_t>(chunkOffset) + 8u + chunkSize > bytes.size()) {
      std::cerr << "[sm5] Invalid chunk size " << chunkSize << " at offset "
                << chunkOffset << "\n";
      continue;
    }

    DxbcChunk chunk;
    chunk.FourCC = fourCC;
    chunk.Kind = FourCCToChunkKind(fourCC);
    chunk.OffsetInContainer = chunkOffset;

    if (chunkSize > 0) {
      chunk.Data.resize(chunkSize);
      std::memcpy(chunk.Data.data(), bytes.data() + chunkOffset + 8, chunkSize);
    }

    container.Chunks.push_back(std::move(chunk));
  }

  return !container.Chunks.empty();
}

static uint32_t AlignToDword(uint32_t byteCount) {
  return (byteCount + 3) & ~3u;
}

bool SerializeDxbcContainer(const Container &container,
                            std::vector<uint8_t> &outBytes) {
  if (container.Chunks.empty()) {
    std::cerr << "[sm5] Cannot serialize empty container\n";
    return false;
  }

  uint32_t chunkCount = static_cast<uint32_t>(container.Chunks.size());
  uint32_t headerSize = 32 + (chunkCount * 4);

  uint32_t totalChunkDataSize = 0;
  for (const auto &chunk : container.Chunks) {
    totalChunkDataSize +=
        AlignToDword(8 + static_cast<uint32_t>(chunk.Data.size()));
  }

  uint32_t totalSize = headerSize + totalChunkDataSize;

  totalSize = AlignToDword(totalSize);

  outBytes.resize(totalSize, 0);

  size_t offset = 0;
  std::memcpy(outBytes.data() + offset, &container.Header.Signature, 4);
  offset += 4;

  std::uint32_t zero = 0;
  for (int i = 0; i < 4; ++i) {
    std::memcpy(outBytes.data() + offset, &zero, 4);
    offset += 4;
  }
  std::uint32_t one = container.Header.One == 0 ? 1u : container.Header.One;
  std::memcpy(outBytes.data() + offset, &one, 4);
  offset += 4;
  std::uint32_t totalSizeU32 = static_cast<uint32_t>(totalSize);
  std::memcpy(outBytes.data() + offset, &totalSizeU32, 4);
  offset += 4;
  std::memcpy(outBytes.data() + offset, &chunkCount, 4);
  offset += 4;

  std::vector<uint32_t> chunkOffsets;
  chunkOffsets.reserve(container.Chunks.size());

  uint32_t currentOffset = static_cast<uint32_t>(headerSize);
  for (const auto &chunk : container.Chunks) {
    chunkOffsets.push_back(currentOffset);
    currentOffset += AlignToDword(8 + static_cast<uint32_t>(chunk.Data.size()));
  }

  for (uint32_t i = 0; i < chunkCount; ++i) {
    std::memcpy(outBytes.data() + offset, &chunkOffsets[i], 4);
    offset += 4;
  }

  for (size_t i = 0; i < container.Chunks.size(); ++i) {
    const auto &chunk = container.Chunks[i];
    uint32_t chunkDataSize = static_cast<uint32_t>(chunk.Data.size());

    std::memcpy(outBytes.data() + chunkOffsets[i], &chunk.FourCC, 4);
    std::memcpy(outBytes.data() + chunkOffsets[i] + 4, &chunkDataSize, 4);

    if (chunkDataSize > 0) {
      std::memcpy(outBytes.data() + chunkOffsets[i] + 8, chunk.Data.data(),
                  chunkDataSize);
    }
  }

  return true;
}

bool RecomputeDxbcHash(std::vector<uint8_t> &containerBytes) {
  if (containerBytes.size() < 32) {
    std::cerr << "[sm5] Container too small for hash recomputation\n";
    return false;
  }

  constexpr uint32_t kHashStartOffset = offsetof(DxbcContainerHeader, One);
  ComputeDXBCHash(
      containerBytes.data() + kHashStartOffset,
      static_cast<uint32_t>(containerBytes.size() - kHashStartOffset),
      containerBytes.data() + offsetof(DxbcContainerHeader, Hash));

  return true;
}

} // namespace dxp::sm5
