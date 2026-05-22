#pragma once

#include "Model.h"

#include <cstdint>
#include <vector>

namespace dxp::sm5 {

/// Rewrite action types for modifying the instruction stream
enum class RewriteActionType {
  ReplaceOne,
  ReplaceRange,
  InsertBefore,
  InsertAfter,
  RemoveRange,
};

/// A single rewrite action
struct RewriteAction {
  RewriteActionType Type;

  // For ReplaceOne: index of instruction to replace
  uint32_t ReplaceIndex;

  // For ReplaceRange: range of instructions to replace
  uint32_t RangeStart;
  uint32_t RangeEnd; // inclusive

  // For InsertBefore/InsertAfter: position
  uint32_t InsertPosition;

  // For RemoveRange: range to remove
  uint32_t RemoveStart;
  uint32_t RemoveEnd; // inclusive

  // Instructions to insert (for ReplaceOne, ReplaceRange, InsertBefore, InsertAfter)
  std::vector<Instruction> NewInstructions;

  RewriteAction();
};

/// Apply rewrite actions to a program.
/// Returns false if actions are invalid.
bool ApplyRewriteActions(Program &program,
                         const std::vector<RewriteAction> &actions);

/// Replace a single instruction at the given index.
bool ReplaceInstruction(Program &program, uint32_t index,
                        const Instruction &newInstruction);

/// Replace a range of instructions.
bool ReplaceRange(Program &program, uint32_t start, uint32_t end,
                  const std::vector<Instruction> &newInstructions);

/// Insert instructions before the given index.
bool InsertBefore(Program &program, uint32_t index,
                  const std::vector<Instruction> &newInstructions);

/// Insert instructions after the given index.
bool InsertAfter(Program &program, uint32_t index,
                 const std::vector<Instruction> &newInstructions);

/// Remove a range of instructions.
bool RemoveRange(Program &program, uint32_t start, uint32_t end);

} // namespace dxp::sm5
