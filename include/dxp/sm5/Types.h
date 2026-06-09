#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dxp::sm5 {

/// @brief Operand role — source or destination.
enum class PublicOperandRole : uint32_t {
  Source = 0,
  Destination = 1,
};

/// @brief Lightweight opcode representation for public API use.
enum class PublicOpcode : uint32_t {
  CustomData = 54,
  Unknown = 0xFFFFFFFFu,
};

/// @brief Per-slot index data captured from a matched operand.
struct CapturedOperandIndex {
  uint32_t Representation = 0;
  bool HasImmediateLo = false;
  uint32_t ImmediateLo = 0;
  bool HasImmediateHi = false;
  uint32_t ImmediateHi = 0;
};

/// @brief Lightweight captured operand — exposes only the fields needed for
/// declarative emit resolution.
///
/// Stored in `CaptureStore` and `RecipeRuleMatch`. Callbacks may set these
/// via `SetCapturedOperand` so that subsequent declarative emit templates can
/// reference them by capture name.
struct CapturedOperand {
  uint32_t Type = 0;
  uint32_t NumComponents = 0;
  uint32_t ComponentMode = 0;
  uint32_t Modifier = 0;
  std::vector<uint32_t> Indices;
  std::vector<uint32_t> ImmediateValues;
  std::vector<CapturedOperandIndex> IndexEntries;
  std::vector<uint32_t> RawTokens;
  std::shared_ptr<CapturedOperand> RelativeOperand;
  std::string FromHandle;
  PublicOperandRole Role = PublicOperandRole::Source;
};

/// @brief Lightweight captured instruction — exposes only the fields needed
/// for declarative emit resolution.
///
/// Stored in `CaptureStore` and `RecipeRuleMatch`. Callbacks may set these
/// via `SetCapturedInstruction` so that subsequent declarative emit templates
/// can reference them by capture name.
struct CapturedInstruction {
  PublicOpcode OpCode = PublicOpcode::Unknown;
  bool Saturate = false;
  bool HasTestBoolean = false;
  uint32_t TestBoolean = 0;
  std::vector<CapturedOperand> Operands;
  std::vector<uint32_t> CustomData;
};

} // namespace dxp::sm5
