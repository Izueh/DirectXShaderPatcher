#include "Transforms.h"

#include "d3d11TokenizedProgramFormat.hpp"

#include <algorithm>

namespace dxp::sm5 {

namespace {

} // anonymous namespace

bool Operand::Equals(const CapturedOperand &other) const {
  if (Type != other.Type ||
      NumComponents != other.NumComponents ||
      ComponentMode != other.ComponentMode ||
      Modifier != other.Modifier) {
    return false;
  }

  if (Indices != other.Indices ||
      ImmediateValues != other.ImmediateValues) {
    return false;
  }

  if (IndexEntries.size() != other.IndexEntries.size()) {
    return false;
  }

  for (size_t i = 0; i < IndexEntries.size(); ++i) {
    const auto &lhsIdx = IndexEntries[i];
    const auto &rhsIdx = other.IndexEntries[i];
    if (static_cast<uint32_t>(lhsIdx.Representation) != rhsIdx.Representation ||
        lhsIdx.HasImmediateLo != rhsIdx.HasImmediateLo ||
        lhsIdx.HasImmediateHi != rhsIdx.HasImmediateHi ||
        lhsIdx.ImmediateLo != rhsIdx.ImmediateLo ||
        lhsIdx.ImmediateHi != rhsIdx.ImmediateHi) {
      return false;
    }
  }

  if (static_cast<bool>(RelativeOperand) != static_cast<bool>(other.RelativeOperand)) {
    return false;
  }
  if (RelativeOperand && other.RelativeOperand) {
    if (!RelativeOperand->Equals(*other.RelativeOperand)) {
      return false;
    }
  }

  return true;
}

bool Operand::Equals(const Operand &other) const {
  if (Type != other.Type ||
      NumComponents != other.NumComponents ||
      ComponentMode != other.ComponentMode ||
      Modifier != other.Modifier ||
      Indices != other.Indices ||
      ImmediateValues != other.ImmediateValues ||
      RelativeOperand != other.RelativeOperand) {
    return false;
  }

  if (IndexEntries.size() != other.IndexEntries.size()) {
    return false;
  }

  for (size_t i = 0; i < IndexEntries.size(); ++i) {
    const auto &lhs = IndexEntries[i];
    const auto &rhs = other.IndexEntries[i];
    if (lhs.Representation != rhs.Representation ||
        lhs.HasImmediateLo != rhs.HasImmediateLo ||
        lhs.HasImmediateHi != rhs.HasImmediateHi ||
        lhs.ImmediateLo != rhs.ImmediateLo ||
        lhs.ImmediateHi != rhs.ImmediateHi) {
      return false;
    }
    if (static_cast<bool>(lhs.RelativeOperand) !=
        static_cast<bool>(rhs.RelativeOperand)) {
      return false;
    }
    if (lhs.RelativeOperand && rhs.RelativeOperand) {
      if (!lhs.RelativeOperand->Equals(*rhs.RelativeOperand)) {
        return false;
      }
    }
  }

  return true;
}

OperandMatch::OperandMatch()
  : Any(false), MatchType(D3D10_SB_OPERAND_TYPE_TEMP), HasTypeMatch(false),
      MatchComponentMode(D3D10_SB_OPERAND_4_COMPONENT_NOSWIZZLE),
      HasComponentMatch(false), MatchNumComponents(0),
      HasNumComponentsMatch(false),
      MatchModifier(D3D10_SB_OPERAND_MODIFIER_NONE), HasModifierMatch(false),
      HasImmediateMatch(false), MatchRelativeAddressing(false),
      HasRelativeMatch(false) {}

InstructionMatch::InstructionMatch()
    : Opcode(Opcode::Unknown()), HasOpcode(false), MatchSaturate(false),
      HasSaturateMatch(false), SaturateValue(false), MatchTestBoolean(0),
      HasTestBooleanMatch(false),
      MatchInputInterpolationMode(D3D10_SB_INTERPOLATION_UNDEFINED),
      HasInputInterpolationModeMatch(false) {}

namespace {

static std::vector<Operand::Index>
BuildComparableIndexEntries(const Operand &operand) {
  if (!operand.IndexEntries.empty()) {
    return operand.IndexEntries;
  }

  std::vector<Operand::Index> indexEntries;
  indexEntries.reserve(operand.Indices.size());
  for (uint32_t value : operand.Indices) {
    Operand::Index index;
    index.Representation = Operand::IndexRepresentation::Immediate32;
    index.HasImmediateLo = true;
    index.ImmediateLo = value;
    indexEntries.push_back(std::move(index));
  }
  return indexEntries;
}

static const uint32_t *IndexValueForCapture(const Operand::Index &index) {
  if (index.HasImmediateLo) {
    return &index.ImmediateLo;
  }
  if (index.HasImmediateHi) {
    return &index.ImmediateHi;
  }
  return nullptr;
}

static bool OperandsEqual(const Operand &lhs, const Operand &rhs) {
  if (lhs.Type != rhs.Type || lhs.NumComponents != rhs.NumComponents ||
      lhs.ComponentMode != rhs.ComponentMode || lhs.Modifier != rhs.Modifier ||
      lhs.Indices != rhs.Indices || lhs.IndexEntries.size() != rhs.IndexEntries.size() ||
      lhs.ImmediateValues != rhs.ImmediateValues) {
    return false;
  }

  for (size_t index = 0; index < lhs.IndexEntries.size(); ++index) {
    const Operand::Index &lhsIndex = lhs.IndexEntries[index];
    const Operand::Index &rhsIndex = rhs.IndexEntries[index];
    if (lhsIndex.Representation != rhsIndex.Representation ||
        lhsIndex.HasImmediateLo != rhsIndex.HasImmediateLo ||
        lhsIndex.HasImmediateHi != rhsIndex.HasImmediateHi ||
        lhsIndex.ImmediateLo != rhsIndex.ImmediateLo ||
        lhsIndex.ImmediateHi != rhsIndex.ImmediateHi) {
      return false;
    }

    if (static_cast<bool>(lhsIndex.RelativeOperand) !=
        static_cast<bool>(rhsIndex.RelativeOperand)) {
      return false;
    }

    if (lhsIndex.RelativeOperand && rhsIndex.RelativeOperand) {
      if (!OperandsEqual(*lhsIndex.RelativeOperand, *rhsIndex.RelativeOperand)) {
        return false;
      }
    }
  }

  if (static_cast<bool>(lhs.RelativeOperand) !=
      static_cast<bool>(rhs.RelativeOperand)) {
    return false;
  }

  if (lhs.RelativeOperand && rhs.RelativeOperand) {
    return OperandsEqual(*lhs.RelativeOperand, *rhs.RelativeOperand);
  }

  return true;
}

static bool OperandIndexEntriesEqual(const Operand &lhs, const Operand &rhs) {
  if (lhs.Indices != rhs.Indices ||
      lhs.IndexEntries.size() != rhs.IndexEntries.size()) {
    return false;
  }

  for (size_t index = 0; index < lhs.IndexEntries.size(); ++index) {
    const Operand::Index &lhsIndex = lhs.IndexEntries[index];
    const Operand::Index &rhsIndex = rhs.IndexEntries[index];
    if (lhsIndex.Representation != rhsIndex.Representation ||
        lhsIndex.HasImmediateLo != rhsIndex.HasImmediateLo ||
        lhsIndex.HasImmediateHi != rhsIndex.HasImmediateHi ||
        lhsIndex.ImmediateLo != rhsIndex.ImmediateLo ||
        lhsIndex.ImmediateHi != rhsIndex.ImmediateHi) {
      return false;
    }

    if (static_cast<bool>(lhsIndex.RelativeOperand) !=
        static_cast<bool>(rhsIndex.RelativeOperand)) {
      return false;
    }

    if (lhsIndex.RelativeOperand && rhsIndex.RelativeOperand) {
      if (!OperandsEqual(*lhsIndex.RelativeOperand, *rhsIndex.RelativeOperand)) {
        return false;
      }
    }
  }

  if (static_cast<bool>(lhs.RelativeOperand) !=
      static_cast<bool>(rhs.RelativeOperand)) {
    return false;
  }

  if (lhs.RelativeOperand && rhs.RelativeOperand) {
    return OperandsEqual(*lhs.RelativeOperand, *rhs.RelativeOperand);
  }

  return true;
}

static bool OperandsEqualProjected(const Operand &lhs, const Operand &rhs,
                                   const OperandMatch &pattern) {
  if (!pattern.HasMatchCaptureProjection()) {
    return lhs.Equals(rhs);
  }

  if (pattern.MatchCaptureType && lhs.Type != rhs.Type) {
    return false;
  }

  if (pattern.MatchCaptureComponents &&
      (lhs.NumComponents != rhs.NumComponents ||
       lhs.ComponentMode != rhs.ComponentMode)) {
    return false;
  }

  if (pattern.MatchCaptureModifier && lhs.Modifier != rhs.Modifier) {
    return false;
  }

  if (pattern.MatchCaptureIndices && !OperandIndexEntriesEqual(lhs, rhs)) {
    return false;
  }

  if (pattern.MatchCaptureImmediates && lhs.ImmediateValues != rhs.ImmediateValues) {
    return false;
  }

  return true;
}

bool MatchesOperand(const Operand &operand, const OperandMatch &pattern) {
  if (pattern.Any)
    return true;

  if (pattern.HasTypeMatch && operand.Type != pattern.MatchType)
    return false;

  if (pattern.HasComponentMatch &&
      operand.ComponentMode != pattern.MatchComponentMode)
    return false;

  if (pattern.HasNumComponentsMatch &&
      operand.NumComponents != pattern.MatchNumComponents)
    return false;

  if (pattern.HasModifierMatch && operand.Modifier != pattern.MatchModifier)
    return false;

  if (pattern.HasImmediateMatch) {
    if (operand.ImmediateValues.size() != pattern.MatchImmediates.size())
      return false;
    for (size_t i = 0; i < pattern.MatchImmediates.size(); ++i) {
      if (operand.ImmediateValues[i] != pattern.MatchImmediates[i])
        return false;
    }
  }

  if (pattern.HasRelativeMatch) {
    bool hasRelative = (operand.RelativeOperand != nullptr);
    if (hasRelative != pattern.MatchRelativeAddressing)
      return false;
  }

  if (!pattern.MatchIndexPatterns.empty()) {
    const std::vector<Operand::Index> indexEntries =
        BuildComparableIndexEntries(operand);
    if (indexEntries.size() != pattern.MatchIndexPatterns.size()) {
      return false;
    }

    for (size_t index = 0; index < pattern.MatchIndexPatterns.size(); ++index) {
      const OperandIndexMatchPattern &matchIndex =
          pattern.MatchIndexPatterns[index];
      const Operand::Index &operandIndex = indexEntries[index];

      if (matchIndex.Any) {
        continue;
      }

      if (matchIndex.HasRepresentation &&
          operandIndex.Representation != matchIndex.Representation) {
        return false;
      }

      if (matchIndex.HasImmediateLo &&
          (!operandIndex.HasImmediateLo ||
           operandIndex.ImmediateLo != matchIndex.ImmediateLo)) {
        return false;
      }

      if (matchIndex.HasImmediateHi &&
          (!operandIndex.HasImmediateHi ||
           operandIndex.ImmediateHi != matchIndex.ImmediateHi)) {
        return false;
      }
    }
  }

  return true;
}

} // namespace

static bool MatchOperand(
    const Operand &operand, const OperandMatch &pattern,
    std::unordered_map<std::string, CapturedOperand> &localOperands,
    std::unordered_map<std::string, uint32_t> &localIndexValues,
    const std::unordered_map<std::string, uint32_t> &existingCapturedIndexValues) {
  if (!MatchesOperand(operand, pattern)) {
    return false;
  }

  if (pattern.MatchIndexPatterns.empty()) {
    return true;
  }

  const std::vector<Operand::Index> indexEntries =
      BuildComparableIndexEntries(operand);
  for (size_t index = 0; index < pattern.MatchIndexPatterns.size(); ++index) {
    const OperandIndexMatchPattern &matchIndex =
        pattern.MatchIndexPatterns[index];
    const Operand::Index &operandIndex = indexEntries[index];

    if (!matchIndex.MatchCapture.empty()) {
      const uint32_t *capturedIndexValue = nullptr;
      const auto it = localIndexValues.find(matchIndex.MatchCapture);
      if (it != localIndexValues.end()) {
        capturedIndexValue = &it->second;
      } else {
        const auto priorIt = existingCapturedIndexValues.find(matchIndex.MatchCapture);
        if (priorIt != existingCapturedIndexValues.end()) {
          capturedIndexValue = &priorIt->second;
        }
      }
      const uint32_t *currentValue = IndexValueForCapture(operandIndex);
      if (capturedIndexValue == nullptr || currentValue == nullptr ||
          *capturedIndexValue != *currentValue) {
        return false;
      }
    }

    if (!matchIndex.CaptureName.empty()) {
      const uint32_t *currentValue = IndexValueForCapture(operandIndex);
      if (currentValue == nullptr) {
        return false;
      }
      localIndexValues[matchIndex.CaptureName] = *currentValue;
    }
  }

  return true;
}

static bool MatchInstruction(
    const Instruction &instruction, const InstructionMatch &pattern,
    std::unordered_map<std::string, CapturedOperand> &localOperands,
    std::unordered_map<std::string, Instruction> &localInstructions,
    std::unordered_map<std::string, uint32_t> &localIndexValues,
    const CaptureStore &globalCaptures,
    const std::unordered_map<std::string, uint32_t> &existingCapturedIndexValues) {
  if (pattern.HasOpcode && instruction.Opcode != pattern.Opcode)
    return false;

  if (pattern.HasSaturateMatch &&
      instruction.Controls.Saturate != pattern.SaturateValue)
    return false;

  if (pattern.HasTestBooleanMatch &&
      (!instruction.Controls.HasTestBoolean ||
       instruction.Controls.TestBoolean != pattern.MatchTestBoolean))
    return false;

  if (pattern.HasInputInterpolationModeMatch) {
    if (!instruction.Controls.HasInputInterpolationMode ||
        instruction.Controls.InputInterpolationMode !=
            pattern.MatchInputInterpolationMode) {
      return false;
    }
  }

  if (pattern.OperandPatterns.size() > instruction.Operands.size())
    return false;

  for (size_t index = 0; index < pattern.OperandPatterns.size(); ++index) {
    const auto &operandPattern = pattern.OperandPatterns[index];
    const auto &operand = instruction.Operands[index];

    if (!MatchOperand(operand, operandPattern,
                      localOperands, localIndexValues,
                      existingCapturedIndexValues))
      return false;

    if (!operandPattern.MatchAgainstCapture.empty()) {
      // Check local captures first (from this instruction's earlier operands)
      const auto localIt = localOperands.find(operandPattern.MatchAgainstCapture);
      if (localIt != localOperands.end()) {
        if (!operand.Equals(localIt->second)) {
          return false;
        }
      // Fall back to global captures for cross-step persistence
      } else {
        const auto globalIt = globalCaptures.operands.find(operandPattern.MatchAgainstCapture);
        if (globalIt != globalCaptures.operands.end()) {
          if (!operand.Equals(globalIt->second)) {
            return false;
          }
        }
      }
    }

    if (!operandPattern.CaptureName.empty()) {
      CapturedOperand capOp = operand.ToCaptured();
      capOp.Role = static_cast<PublicOperandRole>(
          GetOperandRole(static_cast<OpcodeType>(instruction.Opcode), index));
      localOperands[operandPattern.CaptureName] = std::move(capOp);
    }
  }

  return true;
}

std::vector<MatchResult> CollectMatches(const Program &program,
                                        const InstructionMatch &pattern,
                                        CaptureStore &captures) {
  std::vector<MatchResult> matches;
  matches.reserve(program.Instructions.size());

  for (uint32_t index = 0; index < program.Instructions.size(); ++index) {
    std::unordered_map<std::string, CapturedOperand> localOperands;
    std::unordered_map<std::string, Instruction> localInstructions;
    std::unordered_map<std::string, uint32_t> localIndexValues;

    if (!MatchInstruction(program.Instructions[index], pattern,
                          localOperands, localInstructions, localIndexValues,
                          captures, {}))
      continue;

    MatchResult result;
    result.InstructionIndex = index;
    result.Instruction = &program.Instructions[index];
    result.RangeStartIndex = index;
    result.RangeEndIndex = index;
    result.operands = std::move(localOperands);
    result.instructions = std::move(localInstructions);
    result.indexValues = std::move(localIndexValues);

    if (!pattern.CaptureName.empty()) {
      result.instructions[pattern.CaptureName] = program.Instructions[index];
      result.indexValues[pattern.CaptureName + "_index"] = index;
    }

    matches.push_back(std::move(result));
  }
  return matches;
}

std::vector<MatchResult>
CollectSequenceMatches(const Program &program,
                       const std::vector<InstructionMatch> &patterns,
                       CaptureStore &captures) {
  std::vector<MatchResult> matches;
  if (patterns.empty() || patterns.size() > program.Instructions.size()) {
    return matches;
  }

  const uint32_t limit =
      static_cast<uint32_t>(program.Instructions.size() - patterns.size() + 1);
  for (uint32_t startIndex = 0; startIndex < limit; ++startIndex) {
    std::unordered_map<std::string, CapturedOperand> localOperands;
    std::unordered_map<std::string, Instruction> localInstructions;
    std::unordered_map<std::string, uint32_t> localIndexValues;

    bool matched = true;

    for (uint32_t patternIndex = 0; patternIndex < patterns.size();
         ++patternIndex) {
      const uint32_t instructionIndex = startIndex + patternIndex;
      const Instruction &instruction = program.Instructions[instructionIndex];
      const InstructionMatch &pattern = patterns[patternIndex];

      if (!MatchInstruction(instruction, pattern,
                            localOperands, localInstructions, localIndexValues,
                            captures, {})) {
        matched = false;
        break;
      }

      if (!pattern.CaptureName.empty()) {
        localInstructions[pattern.CaptureName] = instruction;
        localIndexValues[pattern.CaptureName + "_index"] = instructionIndex;
      }
    }

    if (!matched) {
      continue;
    }

    MatchResult result;
    result.InstructionIndex = startIndex;
    result.Instruction = &program.Instructions[startIndex];
    result.RangeStartIndex = startIndex;
    result.RangeEndIndex =
        startIndex + static_cast<uint32_t>(patterns.size() - 1);
    result.operands = std::move(localOperands);
    result.instructions = std::move(localInstructions);
    result.indexValues = std::move(localIndexValues);
    matches.push_back(std::move(result));
  }

  return matches;
}

RewriteAction::RewriteAction()
    : Type(RewriteActionType::ReplaceOne), ReplaceIndex(0), RangeStart(0),
  RangeEnd(0), InsertPosition(0), RemoveStart(0), RemoveEnd(0),
  RequiredTempCount(0) {}

static bool TryGetDeclaredTempCount(const Operand &operand,
                                    uint32_t &tempCount) {
  if (!operand.Indices.empty()) {
    tempCount = operand.Indices.front();
    return true;
  }

  if (!operand.ImmediateValues.empty()) {
    tempCount = operand.ImmediateValues.front();
    return true;
  }

  if (operand.RawTokens.size() >= 2) {
    tempCount = operand.RawTokens.back();
    return true;
  }

  return false;
}

void RefreshDeclarations(Program &program) {
  program.Resources.clear();
  program.CBuffers.clear();
  program.Samplers.clear();
  program.ThreadGroups.clear();
  program.GlobalFlags = GlobalFlags{};
  program.TempCount = 0;
  program.TempSize = 0;
  program.IndexableTemps.clear();

  for (const auto &instruction : program.Instructions) {
    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    if (opcode == D3D10_SB_OPCODE_DCL_RESOURCE ||
        opcode == D3D11_SB_OPCODE_DCL_RESOURCE_RAW ||
        opcode == D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED) {
      ResourceDecl decl;
      if (!instruction.Operands.empty() &&
          !instruction.Operands.front().Indices.empty()) {
        decl.RegisterBindPoint = instruction.Operands.front().Indices.front();
      }
      program.Resources.push_back(decl);
      continue;
    }

    if (opcode == D3D10_SB_OPCODE_DCL_SAMPLER) {
      SamplerDecl decl;
      if (!instruction.Operands.empty() &&
          !instruction.Operands.front().Indices.empty()) {
        decl.RegisterBindPoint = instruction.Operands.front().Indices.front();
      }
      program.Samplers.push_back(decl);
      continue;
    }

    if (opcode == D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) {
      CBufferDecl decl;
      if (!instruction.Operands.empty() &&
          !instruction.Operands.front().Indices.empty()) {
        decl.RegisterBindPoint = instruction.Operands.front().Indices.front();
      }
      program.CBuffers.push_back(decl);
      continue;
    }

    if (opcode == D3D10_SB_OPCODE_DCL_TEMPS) {
      uint32_t tempCount = 0;
      if (!instruction.Operands.empty() &&
          TryGetDeclaredTempCount(instruction.Operands.front(), tempCount)) {
        program.TempCount = tempCount;
      }
      program.TempSize = program.TempCount * 4;
      continue;
    }

    if (opcode == D3D11_SB_OPCODE_DCL_THREAD_GROUP) {
      ThreadGroupDecl decl;
      if (instruction.Operands.size() >= 3) {
        if (!instruction.Operands[0].ImmediateValues.empty()) {
          decl.GroupSizeX = instruction.Operands[0].ImmediateValues.front();
        }
        if (!instruction.Operands[1].ImmediateValues.empty()) {
          decl.GroupSizeY = instruction.Operands[1].ImmediateValues.front();
        }
        if (!instruction.Operands[2].ImmediateValues.empty()) {
          decl.GroupSizeZ = instruction.Operands[2].ImmediateValues.front();
        }
      }
      program.ThreadGroups.push_back(decl);
      continue;
    }
  }
}

namespace {

struct RewriteEntry {
  uint32_t pos;
  uint8_t type : 4;
  uint8_t priority : 4;
  uint32_t removeEnd;
  std::vector<Instruction> newInstructions;
};

static RewriteEntry NormalizeEntry(const RewriteAction &action) {
  RewriteEntry e{};
  switch (action.Type) {
  case RewriteActionType::ReplaceOne:
    e.pos = action.ReplaceIndex;
    e.type = static_cast<uint8_t>(RewriteActionType::ReplaceRange);
    e.priority = 1;
    e.removeEnd = action.ReplaceIndex;
    e.newInstructions = std::move(action.NewInstructions);
    break;
  case RewriteActionType::ReplaceRange:
    e.pos = action.RangeStart;
    e.type = static_cast<uint8_t>(RewriteActionType::ReplaceRange);
    e.priority = 1;
    e.removeEnd = action.RangeEnd;
    e.newInstructions = std::move(action.NewInstructions);
    break;
  case RewriteActionType::RemoveRange:
    e.pos = action.RemoveStart;
    e.type = static_cast<uint8_t>(RewriteActionType::RemoveRange);
    e.priority = 1;
    e.removeEnd = action.RemoveEnd;
    break;
  case RewriteActionType::InsertBefore:
    e.pos = action.InsertPosition;
    e.type = static_cast<uint8_t>(RewriteActionType::InsertBefore);
    e.priority = 0;
    e.newInstructions = std::move(action.NewInstructions);
    break;
  case RewriteActionType::InsertAfter:
    // InsertAfter at X = InsertBefore at X+1
    e.pos = action.InsertPosition + 1;
    e.type = static_cast<uint8_t>(RewriteActionType::InsertBefore);
    e.priority = 0;
    e.newInstructions = std::move(action.NewInstructions);
    break;
  }
  return e;
}

} // namespace

bool ApplyRewriteActions(Program &program,
                         const std::vector<RewriteAction> &actions) {
  if (actions.empty())
    return true;

  std::vector<RewriteEntry> entries;
  entries.reserve(actions.size());
  for (const auto &action : actions) {
    entries.push_back(NormalizeEntry(action));
  }

  std::sort(entries.begin(), entries.end(),
            [](const RewriteEntry &a, const RewriteEntry &b) {
              if (a.pos != b.pos)
                return a.pos < b.pos;
              return a.priority < b.priority;
            });

  size_t outSize = program.Instructions.size();
  for (const auto &e : entries) {
    outSize += e.newInstructions.size();
    if (e.type == static_cast<uint8_t>(RewriteActionType::ReplaceRange) ||
        e.type == static_cast<uint8_t>(RewriteActionType::RemoveRange)) {
      outSize -= (e.removeEnd - e.pos + 1);
    }
  }

  std::vector<Instruction> output;
  output.reserve(outSize);

  uint32_t instrIdx = 0;
  size_t eIdx = 0;

  while (instrIdx < program.Instructions.size()) {
    // Process all entries targeting this position.
    while (eIdx < entries.size() && entries[eIdx].pos == instrIdx) {
      const auto &e = entries[eIdx];

      if (e.type == static_cast<uint8_t>(RewriteActionType::InsertBefore)) {
        output.insert(output.end(), e.newInstructions.begin(),
                      e.newInstructions.end());
      } else {
        output.insert(output.end(), e.newInstructions.begin(),
                      e.newInstructions.end());
        instrIdx = e.removeEnd + 1;  // advance past consumed range
        ++eIdx;                       // skip this entry on next iteration
        break;                        // exit inner loop, outer loop continues
      }
      ++eIdx;
    }

    // Emit original instruction only if not consumed by Replace/Remove.
    if (instrIdx < program.Instructions.size() &&
        (eIdx >= entries.size() || entries[eIdx].pos != instrIdx)) {
      output.push_back(std::move(program.Instructions[instrIdx]));
      ++instrIdx;
    }
  }

  while (eIdx < entries.size()) {
    output.insert(output.end(), entries[eIdx].newInstructions.begin(),
                  entries[eIdx].newInstructions.end());
    ++eIdx;
  }

  program.Instructions = std::move(output);
  return true;
}



} // namespace dxp::sm5
