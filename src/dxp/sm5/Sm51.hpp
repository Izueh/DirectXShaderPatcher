#pragma once

#include <cstdint>
#include <expected>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace dxp::sm5::sm51 {

/// @brief Register families that SM5.1 numbers independently (range ids, spaces).
enum class RegClass : std::uint8_t {
  CBuffer = 0,
  Sampler = 1,
  Resource = 2,  ///< SRVs: textures, raw and structured buffers (t#).
  Uav = 3,
};

/// @brief Where a resource really lives in the SM5.1 output.
struct Placement {
  uint32_t space = 0;
  uint32_t reg = 0;
};

/// @brief Maps the register a 5.0-shaped program uses (the "program register")
/// to its real place in the SM5.1 output.
///
/// The program model is always 5.0-shaped: one register per resource, no
/// spaces. A resource that lives outside space 0, or at another register than
/// the one the program uses, has an entry here. Resources without an entry
/// are in space 0 at their program register.
using PlacementMap = std::map<std::pair<RegClass, uint32_t>, Placement>;

/// @brief True when the shader version token is SM5.1 (major 5, minor 1).
[[nodiscard]] bool IsSm51VersionToken(uint32_t version_token) noexcept;

/// @brief Converts a 5.0-shaped token stream (the shader chunk's dwords) to SM5.1.
///
/// Declarations take the 5.1 form (range id, lower and upper bound, space),
/// every resource, sampler, UAV and constant buffer operand gets its range id
/// index, and the resource-dimension and return-type extended opcode tokens
/// are dropped the way fxc does for 5.1. Range ids are dense per register
/// family, in declaration order. @p placements gives the real space and
/// register of the resources that have one.
[[nodiscard]] std::expected<std::vector<uint32_t>, std::string> UpgradeTokens(std::span<const uint32_t> words,
                                                                              const PlacementMap& placements);

/// @brief Converts an SM5.1 token stream to the 5.0-shaped program model.
///
/// Range ids are stripped. Resources outside space 0 (or whose register is
/// taken in space 0) get a free program register and an entry in @p placements,
/// so UpgradeTokens restores them. Supports one register per range: arrays
/// (lower bound != upper bound) and dynamically indexed resources are
/// reported as errors. The version token is left at 5.1.
[[nodiscard]] std::expected<std::vector<uint32_t>, std::string> DowngradeTokens(std::span<const uint32_t> words,
                                                                                PlacementMap& placements);

}  // namespace dxp::sm5::sm51
