#pragma once

#include <functional>
#include <span>
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxp/sm6/step/common/Function.hpp"
#include "dxp/sm6/step/common/Rule_impl.hpp"
#include "dxp/ValidationContext.hpp"
#include "dxp/VariableStore.hpp"
#include "llvm/IR/IRBuilder.h"

namespace dxp::sm6::step::common {

struct EmitContext {
  llvm::IRBuilder<>& builder;
  llvm::Module& module;
  hlsl::DxilModule& dxil_module;
  std::unordered_map<std::string, llvm::Value*>& values;
  const dxp::VariableStore* variables = nullptr;
  const std::unordered_map<std::string, llvm::Value*>* global_values = nullptr;
  std::function<std::expected<llvm::Value*, std::string>(const std::string&)> resolve_resource;
  std::function<llvm::Function*(const std::string&)> resolve_function;
};

struct EmitResult {
  llvm::Value* value = nullptr;
  std::vector<llvm::Instruction*> instructions;
};

std::string PrintType(llvm::Type* type);
llvm::Type* LlvmTypeFor(dxp::ComponentType type, llvm::LLVMContext& context);
llvm::Type* ResolveFunctionType(const FunctionType& type, llvm::LLVMContext& context);
std::pair<std::optional<hlsl::OP::OpCode>, std::optional<unsigned>> ResolveOpCode(const std::string& opcode);
std::expected<ValueTypePattern, std::string> CompileValueTypePattern(const ValueTypePatternData& data);
bool MatchesTypePattern(llvm::Type* type, const ValueTypePattern& pattern);
std::expected<llvm::Type*, std::string> ResolveExtractType(llvm::Type* aggregate_type, std::span<const uint32_t> indices);
std::expected<uint32_t, std::string> ConstantIntToExtractIndex(const std::string& capture_name, size_t depth, llvm::ConstantInt* value);
std::expected<llvm::Constant*, std::string> ResolveConstantValues(const EmitOperand& operand, llvm::Type* type, const dxp::VariableStore* variables);
std::expected<llvm::Constant*, std::string> ResolveConstantValues(const OperandPattern& operand, llvm::Type* type, const dxp::VariableStore* variables);
std::expected<llvm::Value*, std::string> ResolveEmitOperand(const EmitOperand& operand, llvm::Type* type, EmitContext& context,
                                                            const std::string& emit_name,
                                                            std::vector<std::pair<std::string, llvm::Value*>>* consumed = nullptr);
std::expected<EmitResult, std::string> EmitInstruction(const EmitPattern& pattern, EmitContext& context,
                                                       std::vector<std::pair<std::string, llvm::Value*>>* consumed = nullptr);
std::expected<void, std::string> ValidateEmitPattern(const EmitPattern& pattern, const dxp::ValidationContext& context);
bool EmitWritesOutput(const EmitPattern& pattern, const dxp::ValidationContext& context);

}  // namespace dxp::sm6::step::common
