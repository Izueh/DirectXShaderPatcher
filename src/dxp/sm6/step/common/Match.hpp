#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "dxp/sm6/step/common/Rule.hpp"
#include "dxp/VariableStore.hpp"

namespace llvm {
class Value;
class Instruction;
class CallInst;
class Function;
}  // namespace llvm
namespace hlsl {
class DxilModule;
}

namespace dxp::sm6::step::common {

struct MatchResult {
  llvm::CallInst* rootCall = nullptr;
  std::unordered_map<std::string, llvm::Value*> captures;
  std::vector<llvm::Instruction*> instructions;
};

/// Read-only matching inputs and an error output for constant resolution.
struct MatchContext {
  const dxp::VariableStore* variables = nullptr;
  hlsl::DxilModule* dxil_module = nullptr;
  const std::unordered_map<std::string, llvm::Value*>* global_captures = nullptr;
  std::string error;
};

void CollectAllMatches(llvm::Function& function, const InstructionPattern& pattern,
                       std::vector<MatchResult>& matches, MatchContext& context);
unsigned CollectSequenceMatches(llvm::Function& function, const std::vector<InstructionPattern>& patterns,
                                std::vector<MatchResult>& matches, MatchContext& context);

}  // namespace dxp::sm6::step::common
