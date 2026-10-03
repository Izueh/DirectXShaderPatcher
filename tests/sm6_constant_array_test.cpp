#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <expected>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/Casting.h>

#include "dxp/ExportTypes.hpp"
#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/sm6/step/ApplyRuleStep.hpp"
#include "dxp/sm6/step/ApplyRuleStep_impl.hpp"
#include "tests/helper/Sm6Fixture.hpp"
#include "tests/helper/TestHelper.hpp"
#include "value_types/indirect.h"

namespace {
using dxp::sm6::step::ApplyRuleStep;
using dxp::sm6::step::EmitOperand;
using dxp::sm6::step::EmitPattern;
using dxp::sm6::step::Execute;
using dxp::sm6::step::InstructionPattern;
using dxp::sm6::step::MatchKind;
using dxp::sm6::step::OperandKind;
using dxp::sm6::step::OperandPattern;
using dxp::sm6::step::RewriteKind;
using dxp::sm6::step::Rule;

// Numeric literals below exercise shader bit patterns and conversion boundaries.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
class ConstantArrayTests {
 public:
  int Run(dxp::sm6::ExecutionContext& ctx) {
    TestCastSources(ctx);
    TestEmissionWidths(ctx);
    TestResolution(ctx);
    TestImmediateExports(ctx);
    TestIntegerLiteralMatching(ctx);
    TestMatching(ctx);
    Check(ctx.program.Verify().has_value(), "Execute output is valid LLVM IR");
    return failures_ == 0 ? 0 : 1;
  }

 private:
  int failures_ = 0;
  void Check(bool condition, const char* label) {
    if (!condition) {
      std::cerr << "FAILED: " << label << '\n';
      ++failures_;
    }
  }

  void TestCastSources(dxp::sm6::ExecutionContext& ctx) {
    for (const bool cast_field : {false, true}) {
      InstructionPattern pattern;
      pattern.opcode = "Frc";
      EmitPattern emit;
      if (cast_field) {
        emit.cast_opcode = "fptosi";
      } else {
        emit.opcode = "fptosi";
      }
      emit.result_component_type = dxp::ComponentType::I32;
      EmitOperand source;
      Rule rule;
      rule.match_patterns = {pattern};
      const auto run = [&]() {
        emit.operands = {source};
        rule.emit_patterns = {emit};
        const ApplyRuleStep step("cast_source", false, RewriteKind::Before, std::nullopt, rule);
        return Execute(step, ctx);
      };
      for (auto kind : {OperandKind::Undefined, OperandKind::Call}) {
        source.kind = kind;
        source.component_type = dxp::ComponentType::F32;
        auto result = run();
        Check(result && result->applied_count == 1, "typed undefined cast source");
        source.component_type.reset();
        result = run();
        Check(!result && result.error().find("cast undefined source requires component_type") != std::string::npos,
              "untyped undefined cast source rejected");
        source.component_type = dxp::ComponentType::PackedU8x32;
        result = run();
        Check(!result && result.error().find("unsupported cast source component_type") != std::string::npos,
              "unsupported undefined cast source type rejected");
        source.component_type = dxp::ComponentType::I32;
        result = run();
        Check(!result && result.error().find("invalid cast source/result types") != std::string::npos,
              "invalid undefined cast source type rejected");
      }
      source.kind = OperandKind::Constant;
      source.component_type = dxp::ComponentType::PackedU8x32;
      source.constant_float_values = {0.375};
      auto result = run();
      Check(!result && result.error().find("unsupported cast source component_type") != std::string::npos,
            "unsupported constant cast source type rejected");
    }
  }

  void TestEmissionWidths(dxp::sm6::ExecutionContext& ctx) {
    auto& llvm_ctx = ctx.program.GetModule()->getContext();
    const std::vector<std::pair<dxp::ComponentType, llvm::Type*>> types = {
        {dxp::ComponentType::I1, llvm::Type::getInt1Ty(llvm_ctx)},
        {dxp::ComponentType::I8, llvm::Type::getInt8Ty(llvm_ctx)},
        {dxp::ComponentType::U8, llvm::Type::getInt8Ty(llvm_ctx)},
        {dxp::ComponentType::I16, llvm::Type::getInt16Ty(llvm_ctx)},
        {dxp::ComponentType::U16, llvm::Type::getInt16Ty(llvm_ctx)},
        {dxp::ComponentType::I32, llvm::Type::getInt32Ty(llvm_ctx)},
        {dxp::ComponentType::U32, llvm::Type::getInt32Ty(llvm_ctx)},
        {dxp::ComponentType::I64, llvm::Type::getInt64Ty(llvm_ctx)},
        {dxp::ComponentType::U64, llvm::Type::getInt64Ty(llvm_ctx)},
        {dxp::ComponentType::F16, llvm::Type::getHalfTy(llvm_ctx)},
        {dxp::ComponentType::F32, llvm::Type::getFloatTy(llvm_ctx)},
        {dxp::ComponentType::F64, llvm::Type::getDoubleTy(llvm_ctx)}};
    for (const auto& [component, type] : types) {
      const bool floating = type->isFloatingPointTy();
      auto* zero = llvm::Constant::getNullValue(type);
      auto* original = FixtureBinary(ctx, zero, zero, "width_original");
      auto* user = FixtureConsumer(*original);
      ctx.captures.values["width_anchor"] = original;
      InstructionPattern pattern;
      pattern.opcode = floating ? "fadd" : "add";
      pattern.capture_name = "width_original";
      pattern.match_capture = "width_anchor";
      EmitOperand lhs;
      lhs.kind = OperandKind::Constant;
      lhs.component_type = component;
      EmitOperand rhs = lhs;
      rhs.operand_index = 1;
      if (floating) {
        lhs.constant_float_values = {0.375};
        rhs.constant_float_values = {std::string("width_setting")};
        ctx.SetVariable("width_setting", 0.5);
      } else {
        lhs.constant_int_values = {int64_t{0}};
        rhs.constant_int_values = {std::string("width_setting")};
        ctx.SetVariable("width_setting", true);
      }
      EmitPattern emit;
      emit.opcode = pattern.opcode;
      emit.result_component_type = component;
      emit.operands = {lhs, rhs};
      emit.replace_captured = "width_original";
      Rule rule;
      rule.match_patterns = {pattern};
      rule.emit_patterns = {emit};
      const ApplyRuleStep step("emission_width", false, RewriteKind::Replace, std::nullopt, rule);
      auto result = Execute(step, ctx);
      Check(result && result->applied_count == 1, "numeric emission width executes");
      auto* value = user->getOperand(0);
      Check(value->getType() == type, "emitted constant preserves result width");
      if (floating) {
        auto* constant = llvm::dyn_cast<llvm::ConstantFP>(value);
        Check(constant != nullptr && constant->isExactlyValue(0.875), "floating emission bits");
      } else {
        auto* constant = llvm::dyn_cast<llvm::ConstantInt>(value);
        Check(constant != nullptr && constant->getZExtValue() == 1, "integer emission bits");
      }
      ctx.captures.values.erase("width_anchor");
      ctx.captures.values.erase("width_original");
      user->eraseFromParent();
      if (!result) original->eraseFromParent();
    }
    InstructionPattern pattern;
    pattern.opcode = "Frc";
    EmitPattern emit;
    emit.opcode = "add";
    EmitOperand lhs;
    lhs.kind = OperandKind::Constant;
    lhs.constant_int_values = {int64_t{0}};
    EmitOperand rhs = lhs;
    rhs.operand_index = 1;
    emit.operands = {lhs, rhs};
    for (auto type : {dxp::ComponentType::Invalid, dxp::ComponentType::PackedU8x32,
                      dxp::ComponentType::UNormF32, dxp::ComponentType::F8_E4M3FN}) {
      emit.result_component_type = type;
      Rule rule;
      rule.match_patterns = {pattern};
      rule.emit_patterns = {emit};
      const ApplyRuleStep step("unsupported_width", false, RewriteKind::Before, std::nullopt, rule);
      auto result = Execute(step, ctx);
      Check(!result && result.error().find("unsupported result_component_type") != std::string::npos,
            "unsupported result type rejected explicitly");
    }
  }

  void TestResolution(dxp::sm6::ExecutionContext& ctx) {
    auto& llvm_ctx = ctx.program.GetModule()->getContext();
    auto* i8 = llvm::Type::getInt8Ty(llvm_ctx);
    auto* i32 = llvm::Type::getInt32Ty(llvm_ctx);
    auto* i64 = llvm::Type::getInt64Ty(llvm_ctx);
    auto* f32 = llvm::Type::getFloatTy(llvm_ctx);
    EmitOperand operand;
    operand.kind = OperandKind::Constant;
    operand.constant_int_values = {std::string("setting")};
    const auto emit_constant = [&](llvm::Type* requested_type) -> std::expected<llvm::Constant*, std::string> {
      auto* type = requested_type != nullptr ? requested_type : f32;
      const bool floating = type->isFloatingPointTy();
      auto* zero = llvm::Constant::getNullValue(type);
      auto* original = FixtureBinary(ctx, zero, zero, "resolution_original");
      auto* user = FixtureConsumer(*original);
      ctx.captures.values["resolution_anchor"] = original;
      InstructionPattern pattern;
      pattern.opcode = floating ? "fadd" : "add";
      pattern.match_capture = "resolution_anchor";
      pattern.capture_name = "resolution_original";
      EmitPattern emit;
      emit.replace_captured = "resolution_original";
      emit.operands = {operand};
      if (requested_type != nullptr) {
        emit.opcode = pattern.opcode;
        if (type == i8) {
          emit.result_component_type = dxp::ComponentType::I8;
        } else if (type == i32) {
          emit.result_component_type = dxp::ComponentType::I32;
        } else if (type == i64) {
          emit.result_component_type = dxp::ComponentType::I64;
        } else if (type->isHalfTy()) {
          emit.result_component_type = dxp::ComponentType::F16;
        } else if (type->isFloatTy()) {
          emit.result_component_type = dxp::ComponentType::F32;
        } else {
          emit.result_component_type = dxp::ComponentType::F64;
        }
        EmitOperand rhs;
        rhs.operand_index = 1;
        rhs.kind = OperandKind::Constant;
        if (floating) {
          rhs.constant_float_values = {0.0};
        } else {
          rhs.constant_int_values = {int64_t{0}};
        }
        emit.operands.push_back(rhs);
      }
      Rule rule;
      rule.match_patterns = {pattern};
      rule.emit_patterns = {emit};
      const ApplyRuleStep step("constant_resolution", false, RewriteKind::Replace, std::nullopt, rule);
      auto result = Execute(step, ctx);
      auto* constant = llvm::dyn_cast<llvm::Constant>(user->getOperand(0));
      ctx.captures.values.erase("resolution_anchor");
      ctx.captures.values.erase("resolution_original");
      user->eraseFromParent();
      if (!result) {
        original->eraseFromParent();
        return std::unexpected(result.error());
      }
      if (result->applied_count != 1 || constant == nullptr) return std::unexpected("expected one constant replacement");
      return constant;
    };
    auto integer = [&](llvm::Type* type, dxp::PrimitiveValue value, uint64_t expected) {
      ctx.SetVariable("setting", value);
      auto result = emit_constant(type);
      Check(result && llvm::cast<llvm::ConstantInt>(*result)->getZExtValue() == expected, "integer conversion/boundary");
    };
    integer(i8, int32_t{-128}, 128);
    integer(i8, uint32_t{255}, 255);
    integer(i32, uint64_t{0xffffffff}, 0xffffffff);
    integer(i32, int64_t{-2147483648LL}, 0x80000000);
    integer(i64, std::numeric_limits<int64_t>::min(), uint64_t{1} << 63);
    integer(i64, std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::max());
    integer(i64, -std::ldexp(1.0, 63), uint64_t{1} << 63);
    integer(i64, std::nextafter(std::ldexp(1.0, 64), 0.0), 0xfffffffffffff800ULL);
    integer(i32, true, 1);
    for (const dxp::PrimitiveValue value : {dxp::PrimitiveValue{int32_t{-129}}, dxp::PrimitiveValue{uint32_t{256}},
                                            dxp::PrimitiveValue{0.5}, dxp::PrimitiveValue{std::numeric_limits<double>::quiet_NaN()},
                                            dxp::PrimitiveValue{std::numeric_limits<double>::infinity()}}) {
      ctx.SetVariable("setting", value);
      Check(!emit_constant(i8), "invalid integer env conversion rejected");
    }
    ctx.SetVariable("setting", std::ldexp(1.0, 64));
    Check(!emit_constant(i64), "float at unsigned 64-bit upper bound rejected");
    ctx.SetVariable("setting", std::nextafter(-std::ldexp(1.0, 63), -INFINITY));
    Check(!emit_constant(i64), "float below signed 64-bit lower bound rejected");
    ctx.UnsetVariable("setting");
    auto missing = emit_constant(i32);
    Check(!missing && missing.error().find("setting") != std::string::npos, "missing variable diagnostic");
    ctx.SetVariable("setting", uint32_t{7});
    Check(!emit_constant(f32), "integer array requires integer operand");
    const auto match_array = [&](llvm::Constant* candidate) {
      auto* zero = llvm::Constant::getNullValue(candidate->getType());
      const bool floating = candidate->getType()->getScalarType()->isFloatingPointTy();
      auto* instruction = FixtureBinary(ctx, candidate, zero, "resolution_match");
      ctx.captures.values["resolution_match_anchor"] = instruction;
      OperandPattern array;
      array.kind = OperandKind::Constant;
      array.constant_int_values = operand.constant_int_values;
      array.constant_float_values = operand.constant_float_values;
      InstructionPattern pattern;
      pattern.opcode = floating ? "fadd" : "add";
      pattern.match_capture = "resolution_match_anchor";
      pattern.operand_patterns = {array};
      Rule rule;
      rule.match_patterns = {pattern};
      const ApplyRuleStep step("constant_array_match", false, RewriteKind::None, std::nullopt, rule);
      auto result = Execute(step, ctx);
      ctx.captures.values.erase("resolution_match_anchor");
      instruction->eraseFromParent();
      return result;
    };
    operand.constant_int_values = {int64_t{1}, std::string("setting"), int64_t{3}, int64_t{4}};
    auto* vector = llvm::ConstantVector::get({llvm::ConstantInt::get(i32, 1), llvm::ConstantInt::get(i32, 7), llvm::ConstantInt::get(i32, 3), llvm::ConstantInt::get(i32, 4)});
    auto matched = match_array(vector);
    Check(matched && matched->match_count == 1,
          "mixed vector literal/variable array");
    Check(!emit_constant(i32), "multiple entries rejected for scalar operand");
    operand.constant_int_values.resize(2);
    matched = match_array(vector);
    Check(matched && matched->match_count == 0, "vector length mismatch is a nonmatch");
    operand.constant_int_values = {std::string("setting")};
    vector = llvm::ConstantVector::get({llvm::ConstantInt::get(i32, 7), llvm::ConstantInt::get(i32, 7), llvm::ConstantInt::get(i32, 7), llvm::ConstantInt::get(i32, 7)});
    matched = match_array(vector);
    Check(matched && matched->match_count == 1,
          "single value broadcasts to exactly vector width");
    operand.constant_float_values = {std::string("absent")};
    Check(emit_constant(i32).has_value(), "integer array retains precedence over float array");
    operand.constant_int_values.clear();
    operand.constant_float_values = {std::string("setting")};
    auto scalar = emit_constant(f32);
    Check(scalar && llvm::cast<llvm::ConstantFP>(*scalar)->isExactlyValue(7.0), "integer env to float array");
    Check(!emit_constant(i32), "float array requires floating-point operand");
    for (auto* type : {llvm::Type::getHalfTy(llvm_ctx), f32, llvm::Type::getDoubleTy(llvm_ctx)}) {
      ctx.SetVariable("setting", 0.375);
      auto result = emit_constant(type);
      Check(result && (*result)->getType() == type && llvm::cast<llvm::ConstantFP>(*result)->isExactlyValue(0.375),
            "float width follows operand type");
    }
    operand.constant_float_values = {0.25, std::string("setting")};
    auto* float_vector_type = llvm::VectorType::get(f32, 2);
    vector = llvm::ConstantVector::get({llvm::ConstantFP::get(f32, 0.25), llvm::ConstantFP::get(f32, 0.375)});
    matched = match_array(vector);
    Check(matched && matched->match_count == 1,
          "mixed float vector array");
    operand.constant_float_values = {0.0};
    matched = match_array(llvm::Constant::getNullValue(float_vector_type));
    Check(matched && matched->match_count == 1, "zero broadcast matches aggregate zero");
    operand.constant_float_values = {0.25};
    auto missing_type = emit_constant(nullptr);
    Check(!missing_type && missing_type.error().find("constant requires an operand type") != std::string::npos, "missing operand type rejected");
    operand.constant_float_values.clear();
    operand.capture = "setting";
    Check(!emit_constant(f32), "constant capture no longer reads env");
  }

  void TestImmediateExports(dxp::sm6::ExecutionContext& ctx) {
    auto& llvm_ctx = ctx.program.GetModule()->getContext();
    const std::vector<std::pair<dxp::ComponentType, llvm::Type*>> types = {
        {dxp::ComponentType::I1, llvm::Type::getInt1Ty(llvm_ctx)},
        {dxp::ComponentType::U8, llvm::Type::getInt8Ty(llvm_ctx)},
        {dxp::ComponentType::I16, llvm::Type::getInt16Ty(llvm_ctx)},
        {dxp::ComponentType::U32, llvm::Type::getInt32Ty(llvm_ctx)},
        {dxp::ComponentType::U64, llvm::Type::getInt64Ty(llvm_ctx)},
        {dxp::ComponentType::F16, llvm::Type::getHalfTy(llvm_ctx)},
        {dxp::ComponentType::F32, llvm::Type::getFloatTy(llvm_ctx)},
        {dxp::ComponentType::F64, llvm::Type::getDoubleTy(llvm_ctx)}};
    for (const auto& [component, type] : types) {
      const bool floating = type->isFloatingPointTy();
      const unsigned bits = type->getScalarSizeInBits();
      const uint64_t sign_bit = uint64_t{1} << (bits - 1);
      uint64_t first_bits = 1;
      if (floating) {
        switch (bits) {
          case 16: first_bits = 0x3600ULL; break;
          case 32: first_bits = 0x3ec00000ULL; break;
          default: first_bits = 0x3fd8000000000000ULL; break;
        }
      }
      llvm::Constant* first = floating ? llvm::ConstantFP::get(type, 0.375) : llvm::ConstantInt::get(type, 1);
      llvm::Constant* second = floating ? llvm::ConstantFP::get(type, -0.0) : llvm::ConstantInt::get(type, sign_bit);
      auto* zero = llvm::Constant::getNullValue(type);
      auto* vector_type = llvm::VectorType::get(type, 3);
      const std::vector<std::pair<llvm::Constant*, std::vector<uint64_t>>> fixtures = {
          {first, {first_bits}},
          {llvm::ConstantVector::get({first, first, first}), {first_bits, first_bits, first_bits}},
          {llvm::ConstantVector::get({first, second, zero}), {first_bits, sign_bit, 0}},
          {llvm::Constant::getNullValue(vector_type), {0, 0, 0}}};
      for (const auto& [value, expected] : fixtures) {
        auto* instruction = FixtureBinary(ctx, value, llvm::Constant::getNullValue(value->getType()), "export_fixture");
        ctx.captures.values["export_anchor"] = instruction;
        for (unsigned mode = 0; mode < 3; ++mode) {
          OperandPattern operand;
          operand.capture_name = "export_value";
          operand.export_as = "numeric_export";
          if (mode != 0) {
            operand.kind = OperandKind::Constant;
            operand.component_type = component;
          }
          if (mode == 2) {
            for (size_t i = 0; i < expected.size(); ++i) {
              const auto name = "export_setting_" + std::to_string(i);
              if (floating) {
                double setting = 0.0;
                if (expected[i] == first_bits) {
                  setting = 0.375;
                } else if (expected[i] == sign_bit) {
                  setting = -0.0;
                }
                ctx.SetVariable(name, setting);
                operand.constant_float_values.emplace_back(name);
              } else {
                ctx.SetVariable(name, expected[i]);
                operand.constant_int_values.emplace_back(name);
              }
            }
            if (expected.front() == expected.back()) {
              if (floating) {
                operand.constant_float_values.resize(1);
              } else {
                operand.constant_int_values.resize(1);
              }
            }
          }
          InstructionPattern pattern;
          pattern.opcode = floating ? "fadd" : "add";
          pattern.match_capture = "export_anchor";
          pattern.operand_patterns = {operand};
          Rule rule;
          rule.match_patterns = {pattern};
          ctx.immediate_exports.erase("numeric_export");
          const ApplyRuleStep step("numeric_export", false, RewriteKind::None, std::nullopt, rule);
          auto result = Execute(step, ctx);
          Check(result && result->match_count == 1, "numeric export fixture matches");
          auto exported = ctx.immediate_exports.find("numeric_export");
          Check(exported != ctx.immediate_exports.end() && exported->second.raw_values == expected,
                "scalar/vector exports preserve all shader bits");
          if (mode != 0) {
            Check(exported != ctx.immediate_exports.end() && exported->second.type == component,
                  "constant export preserves explicit component type");
          }
        }
        ctx.captures.values.erase("export_anchor");
        ctx.captures.values.erase("export_value");
        instruction->eraseFromParent();
      }
    }
  }

  void TestIntegerLiteralMatching(dxp::sm6::ExecutionContext& ctx) {
    auto& llvm_ctx = ctx.program.GetModule()->getContext();
    struct IntegerType {
      dxp::ComponentType unsigned_component;
      dxp::ComponentType signed_component;
      llvm::Type* type;
    };
    const std::vector<IntegerType> types = {
        {.unsigned_component = dxp::ComponentType::I1, .signed_component = dxp::ComponentType::I1, .type = llvm::Type::getInt1Ty(llvm_ctx)},
        {.unsigned_component = dxp::ComponentType::U8, .signed_component = dxp::ComponentType::I8, .type = llvm::Type::getInt8Ty(llvm_ctx)},
        {.unsigned_component = dxp::ComponentType::U16, .signed_component = dxp::ComponentType::I16, .type = llvm::Type::getInt16Ty(llvm_ctx)},
        {.unsigned_component = dxp::ComponentType::U32, .signed_component = dxp::ComponentType::I32, .type = llvm::Type::getInt32Ty(llvm_ctx)},
        {.unsigned_component = dxp::ComponentType::U64, .signed_component = dxp::ComponentType::I64, .type = llvm::Type::getInt64Ty(llvm_ctx)}};
    for (const auto& [unsigned_component, signed_component, type] : types) {
      const unsigned bits = type->getIntegerBitWidth();
      const uint64_t maximum = bits == 64 ? std::numeric_limits<uint64_t>::max() : (uint64_t{1} << bits) - 1;
      const int64_t literal = bits == 64 ? std::numeric_limits<int64_t>::max() : static_cast<int64_t>(maximum);
      auto* scalar = llvm::ConstantInt::get(type, maximum);
      auto* zero = llvm::ConstantInt::get(type, 0);
      std::vector<llvm::Constant*> values = {scalar, llvm::ConstantVector::get({scalar, zero})};
      if (bits == 64) values.push_back(llvm::ConstantInt::get(type, static_cast<uint64_t>(literal)));
      for (auto* value : values) {
        auto* first = value->getType()->isVectorTy() ? value->getAggregateElement(0U) : value;
        const uint64_t raw = llvm::cast<llvm::ConstantInt>(first)->getZExtValue();
        auto* instruction = FixtureBinary(ctx, value, llvm::Constant::getNullValue(value->getType()), "literal_fixture");
        ctx.captures.values["literal_anchor"] = instruction;
        OperandPattern operand;
        operand.kind = OperandKind::Constant;
        InstructionPattern pattern;
        pattern.opcode = "xor";
        pattern.match_capture = "literal_anchor";
        const auto run = [&](std::optional<dxp::ComponentType> component,
                             const std::variant<std::string, int64_t>& entry, bool matches) {
          operand.component_type = component;
          operand.constant_int_values = {entry};
          if (value->getType()->isVectorTy()) operand.constant_int_values.emplace_back(int64_t{0});
          pattern.operand_patterns = {operand};
          Rule rule;
          rule.match_patterns = {pattern};
          const ApplyRuleStep step("integer_literal", false, RewriteKind::None, std::nullopt, rule);
          auto result = Execute(step, ctx);
          Check(result && std::cmp_equal(result->match_count, matches ? 1U : 0U), "integer literal signedness and boundary matching");
        };
        run(unsigned_component, literal, std::cmp_equal(raw, literal));
        run(unsigned_component, int64_t{-1}, false);
        run(signed_component, int64_t{-1}, raw == maximum && bits != 1);
        run(std::nullopt, int64_t{-1}, raw == maximum && bits != 1);
        run(signed_component, literal, bits == 1 || (bits == 64 && std::cmp_equal(raw, literal)));
        ctx.SetVariable("literal_setting", raw);
        run(unsigned_component, std::string("literal_setting"), true);
        if (bits < 64) run(unsigned_component, literal + 1, false);
        if (bits == 1) {
          ctx.SetVariable("literal_setting", true);
          run(unsigned_component, std::string("literal_setting"), true);
        }
        ctx.captures.values.erase("literal_anchor");
        instruction->eraseFromParent();
      }
    }
    auto* i1 = llvm::Type::getInt1Ty(llvm_ctx);
    auto* zero = llvm::ConstantInt::get(i1, 0);
    auto* instruction = FixtureBinary(ctx, zero, zero, "false_fixture");
    ctx.captures.values["false_anchor"] = instruction;
    OperandPattern operand;
    operand.kind = OperandKind::Constant;
    operand.constant_int_values = {int64_t{0}};
    InstructionPattern pattern;
    pattern.opcode = "xor";
    pattern.match_capture = "false_anchor";
    for (const bool variable : {false, true}) {
      if (variable) {
        ctx.SetVariable("false_setting", false);
        operand.constant_int_values = {std::string("false_setting")};
      }
      pattern.operand_patterns = {operand};
      Rule rule;
      rule.match_patterns = {pattern};
      const ApplyRuleStep step("false_literal", false, RewriteKind::None, std::nullopt, rule);
      auto result = Execute(step, ctx);
      Check(result && result->match_count == 1, "i1 false literal and env parity");
    }
    ctx.captures.values.erase("false_anchor");
    instruction->eraseFromParent();
  }

  void TestMatching(dxp::sm6::ExecutionContext& ctx) {
    ctx.program.GetDxilModule()->SetEntryFunction(ctx.program.GetModule()->getFunction("array_matching"));
    OperandPattern array;
    array.kind = OperandKind::Constant;
    array.component_type = dxp::ComponentType::I8;
    array.capture_name = "matched_array";
    array.export_as = "array_export";
    array.constant_int_values = {int64_t{1}, std::string("second"), int64_t{3}, int64_t{4}};
    OperandPattern zeros;
    zeros.operand_index = 1;
    zeros.kind = OperandKind::Constant;
    zeros.component_type = dxp::ComponentType::I8;
    zeros.constant_int_values = {std::string("zero")};
    InstructionPattern pattern;
    pattern.opcode = "add";
    pattern.operand_patterns = {array, zeros};
    Rule rule;
    rule.match_patterns = {pattern};
    auto run = [&](const Rule& value) {
      const ApplyRuleStep step("array_match", false, RewriteKind::None, std::nullopt, value, MatchKind::MatchAll);
      return Execute(step, ctx);
    };
    ctx.SetVariable("second", uint32_t{2});
    ctx.SetVariable("zero", false);
    auto result = run(rule);
    Check(result && result->match_count == 1, "integer data vector and aggregate zero matching");
    Check(ctx.immediate_exports.contains("array_export") && ctx.immediate_exports.at("array_export").raw_values == std::vector<uint64_t>({1, 2, 3, 4}), "resolved vector export");
    ctx.SetVariable("second", uint32_t{5});
    result = run(rule);
    Check(result && result->match_count == 0, "updated env changes match without mutating rule");
    ctx.SetVariable("second", uint32_t{2});
    InstructionPattern nested;
    nested.opcode = "mul";
    OperandPattern nested_operand;
    nested_operand.kind = OperandKind::Call;
    nested_operand.instruction = xyz::indirect<InstructionPattern>(pattern);
    nested.operand_patterns = {nested_operand, zeros};
    Rule nested_rule;
    nested_rule.match_patterns = {nested};
    result = run(nested_rule);
    Check(result && result->match_count == 1, "nested constant env match");
    Rule sequence;
    sequence.match_patterns = {pattern, nested};
    result = run(sequence);
    Check(result && result->match_count == 1, "consecutive sequence env match");
    ctx.UnsetVariable("second");
    for (const auto* value : {&rule, &nested_rule, &sequence}) {
      result = run(*value);
      Check(!result && result.error().find("second") != std::string::npos, "match resolution error propagated");
    }
    ctx.SetVariable("second", 1.5);
    result = run(rule);
    Check(!result && result.error().find("representable by i8") != std::string::npos, "match invalid conversion is execution error");
    pattern.opcode = "fadd";
    array.component_type = dxp::ComponentType::F64;
    array.export_as.reset();
    array.constant_int_values.clear();
    array.constant_float_values = {std::string("scale")};
    pattern.operand_patterns = {array};
    // A single entry broadcasts; the nonuniform vector candidate simply differs.
    rule.match_patterns = {pattern};
    ctx.SetVariable("scale", 0.375);
    result = run(rule);
    Check(result && result->match_count == 1, "float env scalar matching");
    ctx.SetVariable("scale", 0.5);
    result = run(rule);
    Check(result && result->match_count == 0, "float env unequal is ordinary nonmatch");
    array.constant_float_values = {0.25, std::string("scale")};
    pattern.operand_patterns = {array};
    rule.match_patterns = {pattern};
    ctx.SetVariable("scale", 0.375);
    result = run(rule);
    Check(result && result->match_count == 1, "mixed float data-vector matching skips incompatible shapes");
    ctx.UnsetVariable("scale");
    result = run(rule);
    Check(!result && result.error().find("scale") != std::string::npos, "compatible vector still reports missing env value");
    ctx.SetVariable("scale", 0.375);

    // Numeric literals retain matching at operand precision and signed integer
    // comparison; env integers additionally accept the unsigned positive range.
    array.component_type = dxp::ComponentType::F32;
    array.constant_float_values = {0.1};
    pattern.opcode = "frem";
    pattern.operand_patterns = {array};
    rule.match_patterns = {pattern};
    result = run(rule);
    Check(result && result->match_count == 1, "float literals retain operand-precision matching");
    array.constant_float_values = {std::string("rounded")};
    ctx.SetVariable("rounded", 0.1);
    pattern.operand_patterns = {array};
    rule.match_patterns = {pattern};
    result = run(rule);
    Check(result && result->match_count == 1, "float env value converts to matching f32");
    array.component_type = dxp::ComponentType::I8;
    array.constant_float_values.clear();
    array.constant_int_values = {int64_t{255}};
    pattern.opcode = "xor";
    pattern.operand_patterns = {array};
    rule.match_patterns = {pattern};
    result = run(rule);
    Check(result && result->match_count == 0, "integer literals retain signed exact matching");
    array.constant_int_values = {std::string("unsigned")};
    ctx.SetVariable("unsigned", uint32_t{255});
    pattern.operand_patterns = {array};
    rule.match_patterns = {pattern};
    result = run(rule);
    Check(result && result->match_count == 1, "integer env accepts unsigned positive bit range");
  }
};
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 2) return 1;
    const ScopedCoInitialize coinit;
    std::vector<uint8_t> input;
    const std::span arguments(argv, static_cast<size_t>(argc));
    if (!ReadFile(arguments[1], input)) return 1;
    dxp::sm6::ExecutionContext ctx;
    auto loaded = dxp::sm6::ShaderProgram::FromBytes(input, ctx.program);
    if (!loaded) {
      std::cerr << loaded.error();
      return 1;
    }
    ConstantArrayTests tests;
    return tests.Run(ctx);
  } catch (const std::exception& error) {
    std::fputs(error.what(), stderr);
    std::fputc('\n', stderr);
    return 1;
  } catch (...) {
    std::fputs("Unexpected exception\n", stderr);
    return 1;
  }
}
