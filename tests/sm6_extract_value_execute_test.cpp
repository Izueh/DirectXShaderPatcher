#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "dxp/ExportTypes.hpp"
#include "dxp/sm6/ExecutionContext.hpp"
#include "dxp/sm6/ShaderProgram.hpp"
#include "dxp/sm6/step/ApplyRuleStep.hpp"
#include "dxp/sm6/step/ApplyRuleStep_impl.hpp"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "tests/helper/Sm6Fixture.hpp"
#include "tests/helper/TestHelper.hpp"

namespace {
using dxp::sm6::step::ApplyRuleStep;
using dxp::sm6::step::CaptureExtractIndex;
using dxp::sm6::step::EmitExtractIndex;
using dxp::sm6::step::EmitExtractValue;
using dxp::sm6::step::EmitPattern;
using dxp::sm6::step::Execute;
using dxp::sm6::step::ExtractValuePattern;
using dxp::sm6::step::InstructionPattern;
using dxp::sm6::step::RewriteKind;
using dxp::sm6::step::Rule;
using dxp::sm6::step::ValueTypeKind;
using dxp::sm6::step::ValueTypePattern;

// Numeric literals describe aggregate shapes, index paths and boundary inputs.
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
class ExtractValueTests {
 public:
  int Run(dxp::sm6::ExecutionContext& ctx) {
    ctx.program.GetDxilModule()->SetEntryFunction(ctx.program.GetModule()->getFunction("extract_matching"));
    auto& llvm_ctx = ctx.program.GetModule()->getContext();
    auto* i32 = llvm::Type::getInt32Ty(llvm_ctx);
    auto* i64 = llvm::Type::getInt64Ty(llvm_ctx);
    auto* f32 = llvm::Type::getFloatTy(llvm_ctx);
    auto* v4f = llvm::VectorType::get(f32, 4);
    auto* arr3f = llvm::ArrayType::get(f32, 3);
    auto* inner = ctx.program.GetModule()->getTypeByName("Inner");
    auto* outer = ctx.program.GetModule()->getTypeByName("Outer");
    auto* arr_outer = ctx.program.GetModule()->getTypeByName("ArrOuter");
    auto* literal = llvm::StructType::get(llvm_ctx, {f32, i32});
    auto* opaque = ctx.program.GetModule()->getTypeByName("opaque_t");
    const auto extract = [&](llvm::Type* aggregate_type, const std::vector<unsigned>& indices) {
      return FixtureExtract(ctx, aggregate_type, indices, "extract_fixture");
    };
    auto* ev0 = extract(outer, {0});
    auto* ev01 = extract(outer, {0, 1});
    auto* ev00 = extract(outer, {0, 0});
    auto* ev_arr = extract(arr_outer, {0, 1});
    auto* ev_lit = extract(literal, {1});

    Match(ctx, *ev01, MakePattern({0, 1}), true, "positive multi-index path");
    Match(ctx, *ev01, MakePattern({0, 0}), false, "different last index");
    Match(ctx, *ev00, MakePattern({0, 1}), false, "different candidate path");
    Match(ctx, *ev01, MakePattern({0}), false, "shorter path");
    Match(ctx, *ev01, MakePattern({0, 1, 0}), false, "longer path");
    auto* add = FixtureBinary(ctx, llvm::ConstantInt::get(i32, 7), llvm::ConstantInt::get(i32, 0), "non_extract");
    Match(ctx, *add, MakePattern({0, 1}), false, "non-extract instruction");
    add->eraseFromParent();
    Match(ctx, *ev01, MakePattern({0, 1}, KindGuard(ValueTypeKind::Struct)), true, "aggregate struct guard");
    Match(ctx, *ev01, MakePattern({0, 1}, NameGuard("Outer")), true, "aggregate struct name");
    Match(ctx, *ev01, MakePattern({0, 1}, NameGuard("Other")), false, "wrong aggregate struct name");
    Match(ctx, *ev01, MakePattern({0, 1}, KindGuard(ValueTypeKind::Array)), false, "wrong aggregate kind");
    Match(ctx, *ev01, MakePattern({0, 1}, std::nullopt, ComponentGuard(dxp::ComponentType::I32)), true, "integer result component guard");
    Match(ctx, *ev00, MakePattern({0, 0}, std::nullopt, ComponentGuard(dxp::ComponentType::I32)), false, "wrong result component guard");
    Match(ctx, *ev0, MakePattern({0}, std::nullopt, StructGuard("Inner")), true, "nested struct result guard");
    Match(ctx, *ev_arr, MakePattern({0, 1}), true, "array element path");
    Match(ctx, *ev_arr, MakePattern({0, 1}, std::nullopt, ComponentGuard(dxp::ComponentType::F32)), true, "array element component guard");
    Match(ctx, *ev_arr, MakePattern({0, 1}, std::nullopt, KindGuard(ValueTypeKind::Array)), false, "scalar result rejects array guard");
    Match(ctx, *ev_lit, MakePattern({1}, KindGuard(ValueTypeKind::Struct)), true, "literal struct aggregate");
    Match(ctx, *ev_lit, MakePattern({1}, NameGuard("Whatever")), false, "literal struct has no identified name");
    const auto result_guard = [&](llvm::Type* result_type, const ValueTypePattern& guard, bool matches, const char* label) {
      auto* candidate = extract(llvm::StructType::get(llvm_ctx, llvm::ArrayRef<llvm::Type*>{result_type}), {0});
      Match(ctx, *candidate, MakePattern({0}, std::nullopt, guard), matches, label);
      candidate->eraseFromParent();
    };
    result_guard(f32, ComponentGuard(dxp::ComponentType::F32), true, "float component guard");
    result_guard(i32, ComponentGuard(dxp::ComponentType::F32), false, "float guard rejects integer");
    result_guard(v4f, ComponentGuard(dxp::ComponentType::F32), true, "vector component guard uses element type");
    result_guard(arr3f, ComponentGuard(dxp::ComponentType::F32), false, "component guard rejects array");
    result_guard(inner, ComponentGuard(dxp::ComponentType::F32), false, "component guard rejects struct");
    result_guard(arr3f, KindGuard(ValueTypeKind::Array), true, "array result kind");
    result_guard(outer, KindGuard(ValueTypeKind::Array), false, "array guard rejects struct");
    result_guard(v4f, KindGuard(ValueTypeKind::Vector), true, "vector result kind");
    result_guard(f32, KindGuard(ValueTypeKind::Vector), false, "vector guard rejects scalar");
    result_guard(i32, ScalarGuard(dxp::ComponentType::I32), true, "scalar integer result guard");
    result_guard(i64, ScalarGuard(dxp::ComponentType::I32), false, "scalar integer width guard");
    result_guard(i64, {}, true, "empty result guard");

    Emit(ctx, outer, {uint32_t{0}, uint32_t{1}}, i32, "emit nested integer extraction");
    Emit(ctx, outer, {uint32_t{0}, uint32_t{0}}, f32, "emit nested float extraction");
    Emit(ctx, outer, {uint32_t{0}}, inner, "emit nested struct extraction");
    Emit(ctx, arr_outer, {uint32_t{0}, uint32_t{2}}, f32, "emit nested array extraction");
    Emit(ctx, outer, {uint32_t{0}, uint32_t{1}, uint32_t{0}}, i32, "non-aggregate path depth", "non-aggregate type i32 at depth 2");
    Emit(ctx, outer, {uint32_t{0}, uint32_t{2}}, i32, "struct bounds error", "out of range at depth 1");
    Emit(ctx, arr_outer, {uint32_t{0}, uint32_t{3}}, i32, "array bounds error", "out of range at depth 1");
    Emit(ctx, opaque, {uint32_t{0}}, i32, "opaque aggregate error", "opaque struct at index depth 0");
    Emit(ctx, outer, {}, i32, "empty emit path error", "at least one index");
    Emit(ctx, nullptr, {uint32_t{0}}, i32, "missing aggregate error", "aggregate 'extract_aggregate' was not produced");
    Emit(ctx, outer, {uint32_t{0}, uint32_t{1}}, i32, "emit aggregate guard", {}, StructGuard("Outer"));
    Emit(ctx, outer, {uint32_t{0}, uint32_t{1}}, i32, "emit wrong aggregate guard", "does not satisfy the requested aggregate type", KindGuard(ValueTypeKind::Array));
    Emit(ctx, outer, {uint32_t{0}, uint32_t{1}}, i32, "emit result guard", {}, std::nullopt, ComponentGuard(dxp::ComponentType::I32));
    Emit(ctx, outer, {uint32_t{0}, uint32_t{1}}, i32, "emit wrong result guard", "does not satisfy result_type", std::nullopt, ComponentGuard(dxp::ComponentType::F32));
    auto* array = llvm::ArrayType::get(i32, 10);
    const std::vector<EmitExtractIndex> captured = {CaptureExtractIndex{.capture = "extract_index"}};
    ctx.captures.values["extract_index"] = llvm::ConstantInt::get(i32, 7);
    Emit(ctx, array, captured, i32, "captured i32 index");
    ctx.captures.values["extract_index"] = llvm::ConstantInt::get(i32, 0xffffffffU);
    Emit(ctx, array, captured, i32, "captured i32 index is unsigned", "extract index 4294967295 out of range");
    ctx.captures.values["extract_index"] = llvm::ConstantInt::get(i64, 0xffffffffffffffffULL);
    Emit(ctx, array, captured, i32, "oversized captured i64 index", "outside the supported unsigned index range");
    ctx.captures.values["extract_index"] = llvm::ConstantInt::getTrue(llvm_ctx);
    Emit(ctx, array, captured, i32, "captured i1 index");
    ctx.captures.values["extract_index"] = llvm::ConstantFP::get(f32, 1.0);
    Emit(ctx, array, captured, i32, "noninteger captured index", "must resolve to an integer ConstantInt");
    ctx.captures.values.erase("extract_index");
    Emit(ctx, array, captured, i32, "missing captured index", "capture 'extract_index' was not produced");
    for (auto* instruction : {ev0, ev01, ev00, ev_arr, ev_lit}) instruction->eraseFromParent();
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
  static ValueTypePattern KindGuard(ValueTypeKind kind) {
    ValueTypePattern guard;
    guard.kind = kind;
    return guard;
  }
  static ValueTypePattern NameGuard(std::string name) {
    ValueTypePattern guard;
    guard.struct_name = std::move(name);
    return guard;
  }
  static ValueTypePattern StructGuard(std::string name) {
    auto guard = NameGuard(std::move(name));
    guard.kind = ValueTypeKind::Struct;
    return guard;
  }
  static ValueTypePattern ComponentGuard(dxp::ComponentType component) {
    ValueTypePattern guard;
    guard.component_type = component;
    return guard;
  }
  static ValueTypePattern ScalarGuard(dxp::ComponentType component) {
    auto guard = ComponentGuard(component);
    guard.kind = ValueTypeKind::Scalar;
    return guard;
  }
  static ExtractValuePattern MakePattern(std::vector<uint32_t> indices,
                                         std::optional<ValueTypePattern> aggregate_type = std::nullopt,
                                         std::optional<ValueTypePattern> result_type = std::nullopt) {
    return {.indices = std::move(indices), .aggregate_type = std::move(aggregate_type), .result_type = std::move(result_type)};
  }
  void Match(dxp::sm6::ExecutionContext& ctx, llvm::Instruction& candidate,
             const ExtractValuePattern& extract, bool matches, const char* label) {
    ctx.captures.values["extract_match_anchor"] = &candidate;
    InstructionPattern pattern;
    pattern.opcode = candidate.getOpcodeName();
    pattern.match_capture = "extract_match_anchor";
    pattern.extract = extract;
    Rule rule;
    rule.match_patterns = {pattern};
    const ApplyRuleStep step("extract_match", false, RewriteKind::None, std::nullopt, rule);
    auto result = Execute(step, ctx);
    if (!result) std::cerr << result.error() << '\n';
    Check(result && std::cmp_equal(result->match_count, matches ? 1U : 0U), label);
    ctx.captures.values.erase("extract_match_anchor");
  }
  void Emit(dxp::sm6::ExecutionContext& ctx, llvm::Type* aggregate_type,
            const std::vector<EmitExtractIndex>& indices, llvm::Type* expected_type, const char* label,
            std::string_view expected_error = {}, std::optional<ValueTypePattern> aggregate_guard = std::nullopt,
            std::optional<ValueTypePattern> result_guard = std::nullopt) {
    auto* envelope = llvm::StructType::get(expected_type->getContext(), llvm::ArrayRef<llvm::Type*>{expected_type});
    auto* original = FixtureExtract(ctx, envelope, {0}, "extract_original");
    auto* user = FixtureConsumer(*original);
    ctx.captures.values["extract_emit_anchor"] = original;
    if (aggregate_type != nullptr) {
      ctx.captures.values["extract_aggregate"] = llvm::UndefValue::get(aggregate_type);
      for (auto& argument : ctx.program.GetEntryFunction()->args()) {
        if (argument.getType() == aggregate_type) ctx.captures.values["extract_aggregate"] = &argument;
      }
    } else {
      ctx.captures.values.erase("extract_aggregate");
    }
    InstructionPattern pattern;
    pattern.opcode = "extractvalue";
    pattern.match_capture = "extract_emit_anchor";
    pattern.capture_name = "extract_original";
    EmitPattern emit;
    emit.replace_captured = "extract_original";
    emit.extract = EmitExtractValue{.aggregate = "extract_aggregate", .indices = indices, .aggregate_type = std::move(aggregate_guard), .result_type = std::move(result_guard)};
    Rule rule;
    rule.match_patterns = {pattern};
    rule.emit_patterns = {emit};
    const ApplyRuleStep step("extract_emit", false, RewriteKind::Replace, std::nullopt, rule);
    auto result = Execute(step, ctx);
    if (expected_error.empty()) {
      if (!result) std::cerr << result.error() << '\n';
      Check(result && result->applied_count == 1 && user->getOperand(1) != original && user->getOperand(1)->getType() == expected_type, label);
      if (result) {
        auto* output = llvm::dyn_cast<llvm::ExtractValueInst>(user->getOperand(1));
        std::vector<unsigned> expected_indices;
        for (const auto& index : indices) {
          if (const auto* literal = std::get_if<uint32_t>(&index)) {
            expected_indices.push_back(*literal);
          } else {
            const auto& capture = std::get<CaptureExtractIndex>(index).capture;
            expected_indices.push_back(static_cast<unsigned>(llvm::cast<llvm::ConstantInt>(ctx.captures.values.at(capture))->getZExtValue()));
          }
        }
        Check(output != nullptr && output->getAggregateOperand() == ctx.captures.values.at("extract_aggregate") && std::equal(expected_indices.begin(), expected_indices.end(), output->idx_begin(), output->idx_end()),
              "Execute output preserves aggregate and complete extraction path");
        Check(ctx.program.Verify().has_value(), "Execute output is valid LLVM IR");
      }
    } else {
      Check(!result && result.error().find(expected_error) != std::string::npos, label);
      if (!result && result.error().find(expected_error) == std::string::npos) std::cerr << result.error() << '\n';
    }
    ctx.captures.values.erase("extract_emit_anchor");
    ctx.captures.values.erase("extract_original");
    ctx.captures.values.erase("extract_aggregate");
    user->eraseFromParent();
    if (!result) original->eraseFromParent();
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
    ExtractValueTests tests;
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
