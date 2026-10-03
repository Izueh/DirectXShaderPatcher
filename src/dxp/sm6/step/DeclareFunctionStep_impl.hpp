#pragma once

#include "dxp/Condition_impl.hpp"
#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/sm6/step/common/Rule_impl.hpp"
#include "dxp/sm6/step/DeclareFunctionStep.hpp"
#include "dxp/StepConcept.hpp"
#include "dxp/ValidationContext.hpp"

namespace dxp::sm6::step {

std::expected<DeclareFunctionStep::Results, std::string> Execute(const DeclareFunctionStep& step, ExecutionContext& context);
std::expected<void, std::string> Validate(const DeclareFunctionStep& step, dxp::ValidationContext& context);
std::string DescribeOutcome(const DeclareFunctionStep&, const DeclareFunctionStep::Results& results, const ExecutionContext&);

struct FunctionParameterData {
  std::string name;
  dxp::ComponentType type = dxp::ComponentType::Invalid;
};

struct DeclareFunctionData {
  std::string name;
  bool required = true;
  dxp::ConditionData condition;
  std::vector<FunctionParameterData> params;
  std::string return_type = "void";
  std::vector<EmitPatternData> emit;
  std::optional<EmitOperandPatternData> return_value;

  std::expected<DeclareFunctionStep, std::string> Compile() const;
};

}  // namespace dxp::sm6::step

namespace glz {

template <>
struct meta<dxp::sm6::step::DeclareFunctionData> {
  using T = dxp::sm6::step::DeclareFunctionData;
  static constexpr auto value = object("name", &T::name, "required", &T::required, "condition", &T::condition,
                                       "params", &T::params, "return_type", &T::return_type,
                                       "emit", &T::emit, "return", &T::return_value);
};

}  // namespace glz
