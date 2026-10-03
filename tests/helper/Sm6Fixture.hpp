#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "dxp/sm6/ExecutionContext.hpp"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"

// Find preassembled fixture instructions. Test setup never inserts IR.
inline llvm::BinaryOperator* FixtureBinary(dxp::sm6::ExecutionContext& ctx,
                                           llvm::Value* lhs, llvm::Value* rhs,
                                           const char* prefix) {
  for (auto& block : *ctx.program.GetEntryFunction()) {
    for (auto& instruction : block) {
      auto* binary = llvm::dyn_cast<llvm::BinaryOperator>(&instruction);
      if (binary != nullptr && binary->getName().startswith(prefix) && binary->getOperand(0) == lhs && binary->getOperand(1) == rhs) return binary;
    }
  }
  throw std::runtime_error(std::string("Missing binary fixture: ") + prefix);
}

inline llvm::ExtractValueInst* FixtureExtract(dxp::sm6::ExecutionContext& ctx,
                                              llvm::Type* aggregate_type,
                                              const std::vector<unsigned>& indices,
                                              const char* prefix) {
  for (auto& block : *ctx.program.GetEntryFunction()) {
    for (auto& instruction : block) {
      auto* extract = llvm::dyn_cast<llvm::ExtractValueInst>(&instruction);
      if (extract != nullptr && extract->getName().startswith(prefix) && extract->getAggregateOperand()->getType() == aggregate_type && std::equal(indices.begin(), indices.end(), extract->idx_begin(), extract->idx_end())) return extract;
    }
  }
  throw std::runtime_error(std::string("Missing extract fixture: ") + prefix);
}

inline llvm::Instruction* FixtureConsumer(llvm::Instruction& original) {
  for (auto* value : original.users()) {
    if (auto* instruction = llvm::dyn_cast<llvm::Instruction>(value)) return instruction;
  }
  throw std::runtime_error("Missing fixture consumer");
}
