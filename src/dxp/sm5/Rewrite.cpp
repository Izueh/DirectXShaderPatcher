#include "dxp/sm5/Rewrite.h"

namespace dxp {
namespace sm5 {

RewriteAction::RewriteAction()
  : Type(RewriteActionType::ReplaceOne), ReplaceIndex(0), RangeStart(0),
      RangeEnd(0), InsertPosition(0), RemoveStart(0), RemoveEnd(0) {}

namespace {

static bool TryGetDeclaredTempCount(const Operand &operand, uint32_t &tempCount) {
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

static void RebuildOverlays(Program &program) {
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
      if (!instruction.Operands.empty() && !instruction.Operands.front().Indices.empty()) {
        decl.RegisterBindPoint = instruction.Operands.front().Indices.front();
      }
      program.Resources.push_back(decl);
      continue;
    }

    if (opcode == D3D10_SB_OPCODE_DCL_SAMPLER) {
      SamplerDecl decl;
      if (!instruction.Operands.empty() && !instruction.Operands.front().Indices.empty()) {
        decl.RegisterBindPoint = instruction.Operands.front().Indices.front();
      }
      program.Samplers.push_back(decl);
      continue;
    }

    if (opcode == D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) {
      CBufferDecl decl;
      if (!instruction.Operands.empty() && !instruction.Operands.front().Indices.empty()) {
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

static bool ReplaceRangeAt(Program &program, uint32_t start, uint32_t end,
                           const std::vector<Instruction> &replacement) {
  if (start > end || end >= program.Instructions.size())
    return false;
  program.Instructions.erase(program.Instructions.begin() + static_cast<ptrdiff_t>(start),
                             program.Instructions.begin() + static_cast<ptrdiff_t>(end + 1));
  program.Instructions.insert(program.Instructions.begin() + static_cast<ptrdiff_t>(start),
                              replacement.begin(), replacement.end());
  RebuildOverlays(program);
  return true;
}

} // namespace

bool ReplaceInstruction(Program &program, uint32_t index,
                        const Instruction &newInstruction) {
  if (index >= program.Instructions.size())
    return false;
  program.Instructions[index] = newInstruction;
  RebuildOverlays(program);
  return true;
}

bool ReplaceRange(Program &program, uint32_t start, uint32_t end,
                  const std::vector<Instruction> &newInstructions) {
  return ReplaceRangeAt(program, start, end, newInstructions);
}

bool InsertBefore(Program &program, uint32_t index,
                  const std::vector<Instruction> &newInstructions) {
  if (index > program.Instructions.size())
    return false;
  program.Instructions.insert(program.Instructions.begin() + static_cast<ptrdiff_t>(index),
                              newInstructions.begin(), newInstructions.end());
  RebuildOverlays(program);
  return true;
}

bool InsertAfter(Program &program, uint32_t index,
                 const std::vector<Instruction> &newInstructions) {
  if (index >= program.Instructions.size())
    return false;
  program.Instructions.insert(program.Instructions.begin() + static_cast<ptrdiff_t>(index + 1),
                              newInstructions.begin(), newInstructions.end());
  RebuildOverlays(program);
  return true;
}

bool RemoveRange(Program &program, uint32_t start, uint32_t end) {
  return ReplaceRangeAt(program, start, end, {});
}

bool ApplyRewriteActions(Program &program,
                         const std::vector<RewriteAction> &actions) {
  for (const auto &action : actions) {
    bool ok = false;
    switch (action.Type) {
      case RewriteActionType::ReplaceOne:
        ok = !action.NewInstructions.empty() && ReplaceInstruction(program, action.ReplaceIndex, action.NewInstructions.front());
        break;
      case RewriteActionType::ReplaceRange:
        ok = ReplaceRange(program, action.RangeStart, action.RangeEnd, action.NewInstructions);
        break;
      case RewriteActionType::InsertBefore:
        ok = InsertBefore(program, action.InsertPosition, action.NewInstructions);
        break;
      case RewriteActionType::InsertAfter:
        ok = InsertAfter(program, action.InsertPosition, action.NewInstructions);
        break;
      case RewriteActionType::RemoveRange:
        ok = RemoveRange(program, action.RemoveStart, action.RemoveEnd);
        break;
    }
    if (!ok)
      return false;
  }
  return true;
}

} // namespace sm5
} // namespace dxp
