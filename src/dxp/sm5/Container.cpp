#include "dxp/sm5/Container.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

constexpr uint8_t kHashPadding[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

inline void FF(uint32_t &a, uint32_t b, uint32_t c, uint32_t d, uint32_t x,
               uint8_t s, uint32_t ac) {
  a += ((b & c) | (~b & d)) + x + ac;
  a = ((a << s) | (a >> (32 - s))) + b;
}

inline void GG(uint32_t &a, uint32_t b, uint32_t c, uint32_t d, uint32_t x,
               uint8_t s, uint32_t ac) {
  a += ((b & d) | (c & ~d)) + x + ac;
  a = ((a << s) | (a >> (32 - s))) + b;
}

inline void HH(uint32_t &a, uint32_t b, uint32_t c, uint32_t d, uint32_t x,
               uint8_t s, uint32_t ac) {
  a += (b ^ c ^ d) + x + ac;
  a = ((a << s) | (a >> (32 - s))) + b;
}

inline void II(uint32_t &a, uint32_t b, uint32_t c, uint32_t d, uint32_t x,
               uint8_t s, uint32_t ac) {
  a += (c ^ (b | ~d)) + x + ac;
  a = ((a << s) | (a >> (32 - s))) + b;
}

void ComputeHashRetail(const uint8_t *data, uint32_t byteCount, uint8_t *outHash) {
  constexpr uint8_t S11 = 7;
  constexpr uint8_t S12 = 12;
  constexpr uint8_t S13 = 17;
  constexpr uint8_t S14 = 22;
  constexpr uint8_t S21 = 5;
  constexpr uint8_t S22 = 9;
  constexpr uint8_t S23 = 14;
  constexpr uint8_t S24 = 20;
  constexpr uint8_t S31 = 4;
  constexpr uint8_t S32 = 11;
  constexpr uint8_t S33 = 16;
  constexpr uint8_t S34 = 23;
  constexpr uint8_t S41 = 6;
  constexpr uint8_t S42 = 10;
  constexpr uint8_t S43 = 15;
  constexpr uint8_t S44 = 21;

  uint32_t leftOver = byteCount & 0x3f;
  uint32_t padAmount = leftOver < 56 ? 56 - leftOver : 120 - leftOver;
  bool twoRowsPadding = leftOver >= 56;
  uint32_t state[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
  uint32_t blockCount = (byteCount + padAmount + 8) >> 6;
  uint32_t offset = 0;
  uint32_t nextEndState = twoRowsPadding ? blockCount - 2 : blockCount - 1;
  const uint8_t *current = data;

  for (uint32_t block = 0; block < blockCount; ++block, offset += 64, current += 64) {
    uint32_t x[16] = {};
    const uint32_t *words = nullptr;

    if (block == nextEndState) {
      if (!twoRowsPadding && block == blockCount - 1) {
        uint32_t remainder = byteCount - offset;
        x[0] = byteCount << 3;
        if (current + remainder != data + byteCount) {
          const ptrdiff_t available = (data + byteCount) - current;
          remainder = available > 0 ? static_cast<uint32_t>(available) : 0u;
        }
        std::memcpy(reinterpret_cast<uint8_t *>(x) + 4, current, remainder);
        std::memcpy(reinterpret_cast<uint8_t *>(x) + 4 + remainder, kHashPadding, padAmount);
        x[15] = 1 | (byteCount << 1);
      } else if (twoRowsPadding) {
        if (block == blockCount - 2) {
          uint32_t remainder = byteCount - offset;
          if (current + remainder != data + byteCount) {
            const ptrdiff_t available = (data + byteCount) - current;
            remainder = available > 0 ? static_cast<uint32_t>(available) : 0u;
          }
          std::memcpy(x, current, remainder);
          std::memcpy(reinterpret_cast<uint8_t *>(x) + remainder, kHashPadding, padAmount - 56);
          nextEndState = blockCount - 1;
        } else if (block == blockCount - 1) {
          x[0] = byteCount << 3;
          std::memcpy(reinterpret_cast<uint8_t *>(x) + 4, kHashPadding + padAmount - 56, 56);
          x[15] = 1 | (byteCount << 1);
        }
      }
      words = x;
    } else {
      if (current + 64 > data + byteCount) {
        const ptrdiff_t available = (data + byteCount) - current;
        const size_t copyBytes = available > 0 ? static_cast<size_t>(available) : 0u;
        std::memset(x, 0, sizeof(x));
        if (copyBytes > 0) {
          std::memcpy(x, current, copyBytes);
        }
        words = x;
      } else {
        words = reinterpret_cast<const uint32_t *>(current);
      }
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];

    FF(a, b, c, d, words[0], S11, 0xd76aa478);
    FF(d, a, b, c, words[1], S12, 0xe8c7b756);
    FF(c, d, a, b, words[2], S13, 0x242070db);
    FF(b, c, d, a, words[3], S14, 0xc1bdceee);
    FF(a, b, c, d, words[4], S11, 0xf57c0faf);
    FF(d, a, b, c, words[5], S12, 0x4787c62a);
    FF(c, d, a, b, words[6], S13, 0xa8304613);
    FF(b, c, d, a, words[7], S14, 0xfd469501);
    FF(a, b, c, d, words[8], S11, 0x698098d8);
    FF(d, a, b, c, words[9], S12, 0x8b44f7af);
    FF(c, d, a, b, words[10], S13, 0xffff5bb1);
    FF(b, c, d, a, words[11], S14, 0x895cd7be);
    FF(a, b, c, d, words[12], S11, 0x6b901122);
    FF(d, a, b, c, words[13], S12, 0xfd987193);
    FF(c, d, a, b, words[14], S13, 0xa679438e);
    FF(b, c, d, a, words[15], S14, 0x49b40821);

    GG(a, b, c, d, words[1], S21, 0xf61e2562);
    GG(d, a, b, c, words[6], S22, 0xc040b340);
    GG(c, d, a, b, words[11], S23, 0x265e5a51);
    GG(b, c, d, a, words[0], S24, 0xe9b6c7aa);
    GG(a, b, c, d, words[5], S21, 0xd62f105d);
    GG(d, a, b, c, words[10], S22, 0x02441453);
    GG(c, d, a, b, words[15], S23, 0xd8a1e681);
    GG(b, c, d, a, words[4], S24, 0xe7d3fbc8);
    GG(a, b, c, d, words[9], S21, 0x21e1cde6);
    GG(d, a, b, c, words[14], S22, 0xc33707d6);
    GG(c, d, a, b, words[3], S23, 0xf4d50d87);
    GG(b, c, d, a, words[8], S24, 0x455a14ed);
    GG(a, b, c, d, words[13], S21, 0xa9e3e905);
    GG(d, a, b, c, words[2], S22, 0xfcefa3f8);
    GG(c, d, a, b, words[7], S23, 0x676f02d9);
    GG(b, c, d, a, words[12], S24, 0x8d2a4c8a);

    HH(a, b, c, d, words[5], S31, 0xfffa3942);
    HH(d, a, b, c, words[8], S32, 0x8771f681);
    HH(c, d, a, b, words[11], S33, 0x6d9d6122);
    HH(b, c, d, a, words[14], S34, 0xfde5380c);
    HH(a, b, c, d, words[1], S31, 0xa4beea44);
    HH(d, a, b, c, words[4], S32, 0x4bdecfa9);
    HH(c, d, a, b, words[7], S33, 0xf6bb4b60);
    HH(b, c, d, a, words[10], S34, 0xbebfbc70);
    HH(a, b, c, d, words[13], S31, 0x289b7ec6);
    HH(d, a, b, c, words[0], S32, 0xeaa127fa);
    HH(c, d, a, b, words[3], S33, 0xd4ef3085);
    HH(b, c, d, a, words[6], S34, 0x04881d05);
    HH(a, b, c, d, words[9], S31, 0xd9d4d039);
    HH(d, a, b, c, words[12], S32, 0xe6db99e5);
    HH(c, d, a, b, words[15], S33, 0x1fa27cf8);
    HH(b, c, d, a, words[2], S34, 0xc4ac5665);

    II(a, b, c, d, words[0], S41, 0xf4292244);
    II(d, a, b, c, words[7], S42, 0x432aff97);
    II(c, d, a, b, words[14], S43, 0xab9423a7);
    II(b, c, d, a, words[5], S44, 0xfc93a039);
    II(a, b, c, d, words[12], S41, 0x655b59c3);
    II(d, a, b, c, words[3], S42, 0x8f0ccc92);
    II(c, d, a, b, words[10], S43, 0xffeff47d);
    II(b, c, d, a, words[1], S44, 0x85845dd1);
    II(a, b, c, d, words[8], S41, 0x6fa87e4f);
    II(d, a, b, c, words[15], S42, 0xfe2ce6e0);
    II(c, d, a, b, words[6], S43, 0xa3014314);
    II(b, c, d, a, words[13], S44, 0x4e0811a1);
    II(a, b, c, d, words[4], S41, 0xf7537e82);
    II(d, a, b, c, words[11], S42, 0xbd3af235);
    II(c, d, a, b, words[2], S43, 0x2ad7d2bb);
    II(b, c, d, a, words[9], S44, 0xeb86d391);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
  }

  std::memcpy(outHash, state, 16);
}

} // namespace

namespace dxp::sm5 {

// --- DxbcChunk helpers ---

const DxbcChunk* Container::FindChunk(ChunkKind kind) const {
  for (const auto &chunk : Chunks) {
    if (chunk.Kind == kind) {
      return &chunk;
    }
  }
  return nullptr;
}

DxbcChunk* Container::FindChunk(ChunkKind kind) {
  for (auto &chunk : Chunks) {
    if (chunk.Kind == kind) {
      return &chunk;
    }
  }
  return nullptr;
}

const DxbcChunk* Container::FindChunkByFourCC(uint32_t fourCC) const {
  for (const auto &chunk : Chunks) {
    if (chunk.FourCC == fourCC) {
      return &chunk;
    }
  }
  return nullptr;
}

DxbcChunk* Container::FindChunkByFourCC(uint32_t fourCC) {
  for (auto &chunk : Chunks) {
    if (chunk.FourCC == fourCC) {
      return &chunk;
    }
  }
  return nullptr;
}

const DxbcChunk* Container::GetShaderChunk() const {
  for (const auto &chunk : Chunks) {
    if (chunk.Kind == ChunkKind::Shader) {
      return &chunk;
    }
  }
  return nullptr;
}

DxbcChunk* Container::GetShaderChunk() {
  for (auto &chunk : Chunks) {
    if (chunk.Kind == ChunkKind::Shader) {
      return &chunk;
    }
  }
  return nullptr;
}

// --- Container parsing ---

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

bool ParseDxbcContainer(const std::vector<uint8_t> &bytes, Container &container) {
  // Clear previous state
  container = Container{};
  container.RawBytes = bytes;

  // Check minimum size: signature + hash + one + size + chunkCount
  if (bytes.size() < 32) {
    std::cerr << "[sm5] DXBC container too small: " << bytes.size() << " bytes\n";
    return false;
  }

  // Read header
  size_t offset = 0;
  if (!ReadBytes(bytes, offset, container.Header.Signature)) return false; offset += 4;
  for (int i = 0; i < 4; ++i) {
    if (!ReadBytes(bytes, offset, container.Header.Hash[i])) return false;
    offset += 4;
  }
  if (!ReadBytes(bytes, offset, container.Header.One)) return false; offset += 4;
  if (!ReadBytes(bytes, offset, container.Header.TotalSizeInBytes)) return false; offset += 4;
  if (!ReadBytes(bytes, offset, container.Header.ChunkCount)) return false; offset += 4;

  // Validate signature
  if (container.Header.Signature != DXBC_CONTAINER_SIGNATURE) {
    std::cerr << "[sm5] Invalid DXBC signature: 0x" << std::hex << container.Header.Signature << "\n";
    return false;
  }

  // Validate byte count
  if (container.Header.TotalSizeInBytes != bytes.size()) {
    std::cerr << "[sm5] DXBC byte count mismatch: header says " 
              << container.Header.TotalSizeInBytes << ", actual " << bytes.size() << "\n";
    // Don't fail on this - some containers may have padding
  }

  // Read chunk table
  uint32_t chunkCount = container.Header.ChunkCount;
  if (offset + (chunkCount * 4) > bytes.size()) {
    std::cerr << "[sm5] Chunk table extends beyond container\n";
    return false;
  }

  // Parse chunks from the table
  // The chunk table contains offsets (in DWORDs) from the start of the container
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

  // Sort offsets to process chunks in order
  std::vector<uint32_t> sortedOffsets = chunkOffsets;
  std::sort(sortedOffsets.begin(), sortedOffsets.end());

  // Create chunks from sorted offsets
  container.Chunks.reserve(sortedOffsets.size());
  for (size_t i = 0; i < sortedOffsets.size(); ++i) {
    uint32_t chunkOffset = sortedOffsets[i];
    
    // Read chunk header
    if (chunkOffset + 8 > bytes.size()) {
      std::cerr << "[sm5] Chunk header extends beyond container at offset " 
                << chunkOffset << "\n";
      continue;
    }

    uint32_t fourCC;
    uint32_t chunkSize;
    std::memcpy(&fourCC, bytes.data() + chunkOffset, 4);
    std::memcpy(&chunkSize, bytes.data() + chunkOffset + 4, 4);

    // DXBC chunk size is payload size and does not include the 8-byte chunk header.
    if (static_cast<size_t>(chunkOffset) + 8u + chunkSize > bytes.size()) {
      std::cerr << "[sm5] Invalid chunk size " << chunkSize << " at offset " 
                << chunkOffset << "\n";
      continue;
    }

    DxbcChunk chunk;
    chunk.FourCC = fourCC;
    chunk.Kind = FourCCToChunkKind(fourCC);
    chunk.OffsetInContainer = chunkOffset;
    
    // Copy chunk payload bytes (excluding the 8-byte chunk header).
    if (chunkSize > 0) {
      chunk.Data.resize(chunkSize);
      std::memcpy(chunk.Data.data(), bytes.data() + chunkOffset + 8, chunkSize);
    }

    container.Chunks.push_back(std::move(chunk));
  }

  return !container.Chunks.empty();
}

// --- Container serialization ---

static uint32_t AlignToDword(uint32_t byteCount) {
  return (byteCount + 3) & ~3u;
}

bool SerializeDxbcContainer(const Container &container, std::vector<uint8_t> &outBytes) {
  if (container.Chunks.empty()) {
    std::cerr << "[sm5] Cannot serialize empty container\n";
    return false;
  }

  // Calculate total size
  // Header (16 bytes) + chunk table (wordCount * 4 bytes) + chunk data
  uint32_t chunkCount = static_cast<uint32_t>(container.Chunks.size());
  uint32_t headerSize = 32 + (chunkCount * 4);
  
  // Calculate chunk data sizes (aligned to dword)
  uint32_t totalChunkDataSize = 0;
  for (const auto &chunk : container.Chunks) {
    totalChunkDataSize += AlignToDword(8 + static_cast<uint32_t>(chunk.Data.size()));
  }
  
  uint32_t totalSize = headerSize + totalChunkDataSize;
  
  // Round up to dword boundary
  totalSize = AlignToDword(totalSize);
  
  outBytes.resize(totalSize, 0);
  
  // Write header
  size_t offset = 0;
  std::memcpy(outBytes.data() + offset, &container.Header.Signature, 4); offset += 4;
  // Hash is zeroed now and filled by RecomputeDxbcHash
  std::uint32_t zero = 0;
  for (int i = 0; i < 4; ++i) {
    std::memcpy(outBytes.data() + offset, &zero, 4);
    offset += 4;
  }
  std::uint32_t one = container.Header.One == 0 ? 1u : container.Header.One;
  std::memcpy(outBytes.data() + offset, &one, 4); offset += 4;
  std::uint32_t totalSizeU32 = static_cast<uint32_t>(totalSize);
  std::memcpy(outBytes.data() + offset, &totalSizeU32, 4); offset += 4;
  std::memcpy(outBytes.data() + offset, &chunkCount, 4); offset += 4;
  
  // Write chunk table (offsets in DWORDs from start of container)
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
  
  // Write chunk data
  for (size_t i = 0; i < container.Chunks.size(); ++i) {
    const auto &chunk = container.Chunks[i];
    uint32_t chunkDataSize = static_cast<uint32_t>(chunk.Data.size());
    
    // Write chunk header
    std::memcpy(outBytes.data() + chunkOffsets[i], &chunk.FourCC, 4);
    std::memcpy(outBytes.data() + chunkOffsets[i] + 4, &chunkDataSize, 4);
    
    // Write chunk data
    if (chunkDataSize > 0) {
      std::memcpy(outBytes.data() + chunkOffsets[i] + 8,
                  chunk.Data.data(), chunkDataSize);
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
  ComputeHashRetail(containerBytes.data() + kHashStartOffset,
                    static_cast<uint32_t>(containerBytes.size() - kHashStartOffset),
                    containerBytes.data() + offsetof(DxbcContainerHeader, Hash));
  
  return true;
}

} // namespace dxp::sm5
