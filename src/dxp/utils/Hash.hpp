#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace dxp::utils::hash {

/*
 * Copyright (C) 1986 Gary S. Brown.
 * You may use this program, or code or tables extracted from it, as desired without restriction.
 */

/** CRC32 of the given bytes (IEEE 802.3, polynomial 0xEDB88320). */
inline uint32_t ComputeCrc32(std::span<const uint8_t> data) {
  static constexpr uint32_t crc32_table[256] = {// CRC polynomial 0xEDB88320
                                                0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
                                                0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
                                                0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
                                                0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
                                                0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
                                                0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
                                                0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
                                                0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
                                                0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
                                                0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
                                                0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
                                                0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
                                                0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
                                                0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
                                                0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
                                                0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
                                                0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
                                                0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
                                                0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
                                                0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
                                                0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
                                                0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
                                                0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
                                                0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
                                                0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
                                                0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
                                                0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
                                                0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
                                                0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
                                                0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
                                                0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
                                                0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};

  uint32_t crc = 0xFFFFFFFF;
  for (const uint8_t byte : data) {
    crc = (crc >> 8) ^ crc32_table[(crc ^ byte) & 0xFF];
  }
  return ~crc;
}

// MD5 implementation adapted from dxbc-spirv
// (github.com/doitsujin/dxbc-spirv, Copyright (c) 2025 Philip Rebohle), MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/** 128-bit MD5 digest */
struct MD5Digest {
  std::array<uint8_t, 16U> data = {};

  bool operator==(const MD5Digest& other) const {
    return std::memcmp(data.data(), other.data.data(), sizeof(data)) == 0;
  }
  bool operator!=(const MD5Digest& other) const {
    return std::memcmp(data.data(), other.data.data(), sizeof(data)) != 0;
  }
};

/** MD5 hash state */
class MD5Hasher {
  static constexpr size_t BlockSize = 64U;

 public:
  MD5Hasher() = default;

  /** Processes data to hash */
  void Update(const void* data, size_t size) {
    const std::span<const unsigned char> input(static_cast<const unsigned char*>(data), size);

    size_t consumed = 0;
    const size_t blockOffset = m_size % BlockSize;
    m_size += size;

    if (blockOffset != 0u) {
      const size_t n = size < (BlockSize - blockOffset) ? size : (BlockSize - blockOffset);
      std::memcpy(&m_block.at(blockOffset), input.data(), n);
      consumed += n;

      if (blockOffset + n >= BlockSize) {
        ProcessBlock(std::span<const unsigned char>(m_block.data(), m_block.size()));
      }
    }

    while (size - consumed >= BlockSize) {
      ProcessBlock(input.subspan(consumed, BlockSize));
      consumed += BlockSize;
    }

    if ((size - consumed) != 0u) {
      std::memcpy(m_block.data(), input.subspan(consumed).data(), size - consumed);
    }
  }

  /** Finalizes hash and computes digest. The hasher will be returned to
   *  its default state after. */
  MD5Digest Finalize() {
    PadBlock();

    auto digest = GetDigest();

    Reset();
    return digest;
  }

  /** Retrieves current digest without finalizing the stream properly. */
  [[nodiscard]] MD5Digest GetDigest() const {
    MD5Digest digest = {};

    for (uint32_t i = 0U; i < m_state.size(); i++) {
      auto dw = m_state.at(i);

      for (uint32_t j = 0U; j < sizeof(dw); j++) {
        digest.data.at((sizeof(dw) * i) + j) = static_cast<uint8_t>((dw >> (8U * j)) & 0xffu);
      }
    }

    return digest;
  }

  /** Resets hasher to initial state */
  void Reset() {
    *this = MD5Hasher();
  }

 private:
  std::array<uint8_t, BlockSize> m_block = {};
  uint64_t m_size = 0U;

  std::array<uint32_t, 4U> m_state = {
      0x67452301U,
      0xefcdab89U,
      0x98badcfeu,
      0x10325476U,
  };

  void ProcessBlock(std::span<const unsigned char> data) {
    static const std::array<uint8_t, 64U> kShifts = {
        7U,
        12U,
        17U,
        22U,
        7U,
        12U,
        17U,
        22U,
        7U,
        12U,
        17U,
        22U,
        7U,
        12U,
        17U,
        22U,
        5U,
        9U,
        14U,
        20U,
        5U,
        9U,
        14U,
        20U,
        5U,
        9U,
        14U,
        20U,
        5U,
        9U,
        14U,
        20U,
        4U,
        11U,
        16U,
        23U,
        4U,
        11U,
        16U,
        23U,
        4U,
        11U,
        16U,
        23U,
        4U,
        11U,
        16U,
        23U,
        6U,
        10U,
        15U,
        21U,
        6U,
        10U,
        15U,
        21U,
        6U,
        10U,
        15U,
        21U,
        6U,
        10U,
        15U,
        21U,
    };

    static const std::array<uint32_t, 64U> kConstants = {
        0xd76aa478U,
        0xe8c7b756U,
        0x242070dbu,
        0xc1bdceeeu,
        0xf57c0fafU,
        0x4787c62au,
        0xa8304613U,
        0xfd469501U,
        0x698098d8U,
        0x8b44f7afu,
        0xffff5bb1U,
        0x895cd7beu,
        0x6b901122U,
        0xfd987193U,
        0xa679438eu,
        0x49b40821U,
        0xf61e2562U,
        0xc040b340U,
        0x265e5a51U,
        0xe9b6c7aau,
        0xd62f105du,
        0x02441453U,
        0xd8a1e681U,
        0xe7d3fbc8U,
        0x21e1cde6U,
        0xc33707d6U,
        0xf4d50d87U,
        0x455a14edu,
        0xa9e3e905U,
        0xfcefa3f8U,
        0x676f02d9U,
        0x8d2a4c8au,
        0xfffa3942U,
        0x8771f681U,
        0x6d9d6122U,
        0xfde5380cu,
        0xa4beea44U,
        0x4bdecfa9U,
        0xf6bb4b60U,
        0xbebfbc70U,
        0x289b7ec6U,
        0xeaa127fau,
        0xd4ef3085U,
        0x04881d05U,
        0xd9d4d039U,
        0xe6db99e5U,
        0x1fa27cf8U,
        0xc4ac5665U,
        0xf4292244U,
        0x432aff97U,
        0xab9423a7U,
        0xfc93a039U,
        0x655b59c3U,
        0x8f0ccc92U,
        0xffeff47du,
        0x85845dd1U,
        0x6fa87e4fu,
        0xfe2ce6e0U,
        0xa3014314U,
        0x4e0811a1U,
        0xf7537e82U,
        0xbd3af235U,
        0x2ad7d2bbu,
        0xeb86d391U,
    };

    std::array<uint32_t, 4U> state = m_state;

    auto finalizeIteration = [&state, data,
                              s = kShifts,
                              k = kConstants](uint32_t i, uint32_t f, uint32_t g) {
      f = f + state[0U] + k.at(i) + ReadDword(data.subspan(static_cast<size_t>(4U) * g).data());
      state[0U] = state[3U];
      state[3U] = state[2U];
      state[2U] = state[1U];
      state[1U] += (f << s.at(i)) + (f >> (32U - s.at(i)));
    };

    for (uint32_t i = 0U; i < 16U; i++) {
      const uint32_t f = (state[1U] & state[2U]) | (~state[1U] & state[3U]);
      const uint32_t g = i;
      finalizeIteration(i, f, g);
    }

    for (uint32_t i = 16U; i < 32U; i++) {
      const uint32_t f = (state[3U] & state[1U]) | (~state[3U] & state[2U]);
      const uint32_t g = (5U * i + 1U) % 16U;
      finalizeIteration(i, f, g);
    }

    for (uint32_t i = 32U; i < 48U; i++) {
      const uint32_t f = state[1U] ^ state[2U] ^ state[3U];
      const uint32_t g = (3U * i + 5U) % 16U;
      finalizeIteration(i, f, g);
    }

    for (uint32_t i = 48U; i < 64U; i++) {
      const uint32_t f = state[2U] ^ (state[1U] | ~state[3U]);
      const uint32_t g = (7U * i) % 16U;
      finalizeIteration(i, f, g);
    }

    for (uint32_t i = 0U; i < state.size(); i++) {
      m_state.at(i) += state.at(i);
    }
  }

  void PadBlock() {
    static const std::array<uint8_t, BlockSize> kPadding = {0x80U};

    const uint64_t bitCount = 8U * m_size;

    const size_t currentLength = m_size % BlockSize;
    const size_t desiredLength = currentLength >= 56U ? 120U : 56U;

    Update(kPadding.data(), desiredLength - currentLength);

    std::array<uint8_t, sizeof(bitCount)> finalPadding = {};

    for (uint32_t i = 0U; i < finalPadding.size(); i++) {
      finalPadding.at(i) = static_cast<uint8_t>((bitCount >> (8U * i)) & 0xffu);
    }

    Update(finalPadding.data(), finalPadding.size());
  }

  static uint32_t ReadDword(const unsigned char* src) {
    // Little-endian word load (x86 target); memcpy avoids pointer indexing.
    uint32_t value = 0;
    std::memcpy(&value, src, sizeof(value));
    return value;
  }
};

}  // namespace dxp::utils::hash
