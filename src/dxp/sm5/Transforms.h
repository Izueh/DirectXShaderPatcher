#pragma once

#include "dxp/sm5/Model.h"
#include "dxp/sm5/Recipe.h"
#include "dxp/sm5/Types.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "d3d11TokenizedProgramFormat.hpp"

namespace dxp::sm5 {

static inline uint32_t ExtractComponentMask(uint32_t fromComponentMode,
                                            uint32_t fromSelectionMode) {
  switch (static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(
      fromSelectionMode)) {
  case D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE: {
    const uint32_t mask =
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(fromComponentMode);
    return mask >> 4;
  }
  case D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE: {
    const uint32_t selected =
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(fromComponentMode);
    return 1u << selected;
  }
  case D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE: {
    uint32_t unique = 0;
    for (int c = 0; c < 4; ++c) {
      const uint32_t src =
          DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_SOURCE(fromComponentMode, c);
      unique |= (1u << src);
    }
    return unique;
  }
  default:
    return 0xF;
  }
}

struct OperandIndexMatchPattern {
  bool Any = false;
  bool HasRepresentation = false;
  Operand::IndexRepresentation Representation =
      Operand::IndexRepresentation::Immediate32;
  bool HasImmediateLo = false;
  uint32_t ImmediateLo = 0;
  bool HasImmediateHi = false;
  uint32_t ImmediateHi = 0;
  std::string CaptureName;
  std::string MatchCapture;
};

struct OperandMatch {
  bool Any;

  OperandType MatchType;
  bool HasTypeMatch;

  std::vector<OperandIndexMatchPattern> MatchIndexPatterns;

  uint32_t MatchComponentMode;
  bool HasComponentMatch;

  uint32_t MatchNumComponents;
  bool HasNumComponentsMatch;

  OperandModifier MatchModifier;
  bool HasModifierMatch;

  std::vector<uint32_t> MatchImmediates;
  bool HasImmediateMatch;

  bool MatchRelativeAddressing;
  bool HasRelativeMatch;

  std::string CaptureName;
  std::string MatchAgainstCapture;

  bool MatchCaptureType = false;
  bool MatchCaptureComponents = false;
  bool MatchCaptureModifier = false;
  bool MatchCaptureIndices = false;
  bool MatchCaptureImmediates = false;

  bool HasMatchCaptureProjection() const {
    return MatchCaptureType || MatchCaptureComponents || MatchCaptureModifier ||
           MatchCaptureIndices || MatchCaptureImmediates;
  }

  OperandMatch();
};

struct InstructionMatch {
  dxp::sm5::Opcode Opcode;
  bool HasOpcode;

  bool MatchSaturate;
  bool HasSaturateMatch;
  bool SaturateValue;

  uint32_t MatchTestBoolean;
  bool HasTestBooleanMatch;

  uint32_t MatchInputInterpolationMode;
  bool HasInputInterpolationModeMatch;

  std::vector<OperandMatch> OperandPatterns;
  std::string CaptureName;
  std::string MatchAgainstCapture;

  InstructionMatch();
};

struct MatchResult {
  uint32_t InstructionIndex;
  const dxp::sm5::Instruction *Instruction;
  uint32_t RangeStartIndex;
  uint32_t RangeEndIndex;
  std::unordered_map<std::string, CapturedOperand> operands;
  std::unordered_map<std::string, dxp::sm5::Instruction> instructions;
  std::unordered_map<std::string, uint32_t> indexValues;
};

std::vector<MatchResult> CollectMatches(const Program &program,
                                        const InstructionMatch &pattern,
                                        CaptureStore &captures);

std::vector<MatchResult>
CollectSequenceMatches(const Program &program,
                       const std::vector<InstructionMatch> &patterns,
                       CaptureStore &captures);

enum class RewriteActionType {
  ReplaceOne,
  ReplaceRange,
  InsertBefore,
  InsertAfter,
  RemoveRange,
};

struct RewriteAction {
  RewriteActionType Type;

  uint32_t ReplaceIndex;

  uint32_t RangeStart;
  uint32_t RangeEnd;

  uint32_t InsertPosition;

  uint32_t RemoveStart;
  uint32_t RemoveEnd;

  uint32_t RequiredTempCount;

  std::vector<Instruction> NewInstructions;

  RewriteAction();
};

bool ApplyRewriteActions(Program &program,
                         const std::vector<RewriteAction> &actions);

void RefreshDeclarations(Program &program);

} // namespace dxp::sm5
