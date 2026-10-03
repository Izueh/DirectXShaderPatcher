#include <iostream>
#include "dxp/sm5/step/common/Match.hpp"
#include "dxp/sm6/step/common/Match.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

namespace {
int failures = 0;
void Check(bool passed, const char* message) {
  if (!passed) {
    std::cerr << message << '\n';
    ++failures;
  }
}

void TestSM5() {
  using namespace dxp::sm5::model;
  using namespace dxp::sm5::step;
  using common::CollectMatches;
  using common::CollectSequenceMatches;
  using common::CollectWindowMatches;
  using common::MatchContext;
  dxp::sm5::CaptureStore captures;
  dxp::sm5::DeclarationIndex declarations;
  MatchContext context{captures, declarations};
  std::vector<Instruction> instructions(3);
  for (auto& instruction : instructions) instruction.opcode = Opcode::Mov;
  InstructionPattern pattern;
  pattern.opcode = Opcode::Mov;
  pattern.capture = "instruction";
  auto matches = CollectMatches(instructions, pattern, context);
  Check(matches.size() == 3 && matches[1].instruction == &instructions[1], "SM5 must return instruction locations");
  Check(captures.instructions.empty() && captures.index_values.empty(), "SM5 matching must not publish captures");
  Check(matches.size() == 3 && matches[1].index_values.at("instruction_index").immediate_lo == 1, "SM5 instruction captures must retain indices");
  matches = CollectSequenceMatches(instructions, {pattern, pattern}, context);
  Check(matches.size() == 2 && matches[1].range_start_index == 1 && matches[1].range_end_index == 2, "SM5 must preserve overlapping sequences");
  matches = CollectWindowMatches(instructions, pattern, pattern, context);
  Check(matches.size() == 3 && matches[0].range_end_index == 0, "SM5 windows must choose the first matching endpoint");
}

void TestSM6() {
  using namespace dxp::sm6::step;
  using common::CollectAllMatches;
  using common::CollectSequenceMatches;
  using common::MatchContext;
  using common::MatchResult;
  llvm::LLVMContext llvm_context;
  llvm::Module module("matching", llvm_context);
  auto* type = llvm::Type::getInt32Ty(llvm_context);
  auto* function = llvm::Function::Create(llvm::FunctionType::get(type, {type}, false), llvm::GlobalValue::InternalLinkage, "test", &module);
  auto* block = llvm::BasicBlock::Create(llvm_context, "entry", function);
  llvm::IRBuilder<> builder(block);
  auto* argument = &*function->arg_begin();
  auto* first = builder.CreateAdd(argument, builder.getInt32(2));
  auto* second = builder.CreateAdd(first, builder.getInt32(2));
  builder.CreateRet(second);
  dxp::VariableStore variables;
  variables.SetVariable("amount", int32_t{2});
  std::unordered_map<std::string, llvm::Value*> globals;
  MatchContext context{&variables, nullptr, &globals, {}};
  OperandPattern constant;
  constant.operand_index = 1;
  constant.kind = OperandKind::Constant;
  constant.constant_int_values = {std::string("amount")};
  InstructionPattern pattern;
  pattern.opcode = "add";
  pattern.capture_name = "sum";
  pattern.operand_patterns = {constant};
  std::vector<MatchResult> matches;
  CollectAllMatches(*function, pattern, matches, context);
  Check(context.error.empty() && matches.size() == 2, "SM6 must resolve constants from environment values");
  Check(globals.empty(), "SM6 matching must not publish captures");
  Check(matches.size() == 2 && matches[0].captures.at("sum") == first, "SM6 must return local SSA captures");
  InstructionPattern consumer = pattern;
  consumer.operand_patterns.insert(consumer.operand_patterns.begin(), OperandPattern{.operand_index = 0, .match_capture = "sum"});
  consumer.capture_name = "consumer";
  CollectSequenceMatches(*function, {pattern, consumer}, matches, context);
  Check(matches.size() == 1 && matches[0].instructions.size() == 2, "SM6 sequences must share local captures");
  globals["sum"] = argument;
  CollectSequenceMatches(*function, {pattern, consumer}, matches, context);
  Check(matches.empty(), "SM6 global captures must preserve precedence over local captures");
  variables.UnsetVariable("amount");
  CollectAllMatches(*function, pattern, matches, context);
  Check(!context.error.empty(), "SM6 missing environment constants must report an error");
}
}  // namespace

int main() {
  TestSM5();
  TestSM6();
  return failures == 0 ? 0 : 1;
}
