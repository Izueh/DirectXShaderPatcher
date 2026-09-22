// Direct unit tests for the extractvalue helpers (synthetic LLVM IR).
//
// Real SM6 fixtures only contain single-index extractvalue against flat
// ResRet/CBufRet aggregates (the DXC compiler flattens nested cbuffers into
// cbufferLoadLegacy loads), so genuine positive multi-index coverage lives
// here against a synthetic function with a nested-struct argument.

#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "dxp/ExportTypes.hpp"
#include "dxp/sm6/step/ApplyRuleStep.hpp"
#include "dxp/sm6/step/ApplyRuleStep_impl.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

namespace {

using namespace dxp::sm6::step;

int failures = 0;

void Check(bool condition, const std::string& label) {
  if (condition) {
    std::cout << "ok: " << label << "\n";
  } else {
    std::cerr << "FAILED: " << label << "\n";
    ++failures;
  }
}

using dxp::sm6::step::detail::MatchesTypePattern;
using dxp::sm6::step::detail::ResolveExtractType;
using dxp::sm6::step::detail::MatchesExtractValue;
using dxp::sm6::step::detail::ConstantIntToExtractIndex;

dxp::sm6::step::ExtractValuePattern MakePattern(
    std::vector<uint32_t> indices,
    std::optional<dxp::sm6::step::ValueTypePattern> aggregate_type = std::nullopt,
    std::optional<dxp::sm6::step::ValueTypePattern> result_type = std::nullopt) {
  dxp::sm6::step::ExtractValuePattern pattern;
  pattern.indices = std::move(indices);
  pattern.aggregate_type = std::move(aggregate_type);
  pattern.result_type = std::move(result_type);
  return pattern;
}

}  // namespace

int main() {
  using ValueTypeKind = dxp::sm6::step::ValueTypeKind;
  using ValueTypePattern = dxp::sm6::step::ValueTypePattern;

  llvm::LLVMContext ctx;
  llvm::Module module("detail_test", ctx);
  auto* i32 = llvm::Type::getInt32Ty(ctx);
  auto* i64 = llvm::Type::getInt64Ty(ctx);
  auto* f32 = llvm::Type::getFloatTy(ctx);
  auto* v4f = llvm::VectorType::get(f32, 4);
  auto* arr3f = llvm::ArrayType::get(f32, 3);
  auto* inner = llvm::StructType::create(ctx, {f32, i32}, "Inner", false);
  auto* outer = llvm::StructType::create(ctx, {inner, i32}, "Outer", false);
  auto* arr_outer = llvm::StructType::create(ctx, {arr3f}, "ArrOuter", false);
  auto* opaque = llvm::StructType::create(ctx, "opaque_t");

  // fn(arg: Outer) {
  //   %e0  = extractvalue %arg, 0      // Inner
  //   %e01 = extractvalue %arg, 0, 1   // i32
  //   %e00 = extractvalue %arg, 0, 0   // f32 (Inner's first element)
  // }
  std::vector<llvm::Type*> params = {outer};
  auto* fn = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), params, false), llvm::Function::InternalLinkage, "f", &module);
  auto* arg = &*fn->getArgumentList().begin();
  auto* entry = llvm::BasicBlock::Create(ctx, "entry", fn);
  llvm::IRBuilder<> builder(entry);
  auto* ev0 = builder.CreateExtractValue(arg, {0});
  auto* ev01 = builder.CreateExtractValue(arg, {0, 1});
  auto* ev00 = builder.CreateExtractValue(arg, {0, 0});

  // fn2(arg: literal { float, i32 }) { %e = extractvalue %arg, 1 }
  auto* literal = llvm::StructType::create({f32, i32});
  std::vector<llvm::Type*> params2 = {literal};
  auto* fn2 = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), params2, false), llvm::Function::InternalLinkage, "f2", &module);
  auto* arg2 = &*fn2->getArgumentList().begin();
  auto* entry2 = llvm::BasicBlock::Create(ctx, "entry2", fn2);
  llvm::IRBuilder<> builder2(entry2);
  auto* ev_lit = builder2.CreateExtractValue(arg2, {1});

  // ─── Full-path index matching ──────────────────────────────────────────────
  Check(MatchesExtractValue(ev01, MakePattern({0, 1})), "positive multi-index [0,1]");
  Check(!MatchesExtractValue(ev01, MakePattern({0, 0})), "different last index [0,0] does not match");
  Check(!MatchesExtractValue(ev00, MakePattern({0, 1})), "different last index [0,1] does not match %e00");
  Check(!MatchesExtractValue(ev01, MakePattern({0})), "shorter path [0] does not match");
  Check(!MatchesExtractValue(ev01, MakePattern({0, 1, 0})), "longer path [0,1,0] does not match");
  Check(!MatchesExtractValue(llvm::ConstantInt::get(i32, 7), MakePattern({0, 1})), "non-ExtractValueInst value does not match");

  // ─── Guards ─────────────────────────────────────────────────────────────────
  Check(MatchesExtractValue(ev01, MakePattern({0, 1}, ValueTypePattern{.kind = ValueTypeKind::Struct})), "aggregate kind struct guard");
  Check(MatchesExtractValue(ev01, MakePattern({0, 1}, ValueTypePattern{.struct_name = "Outer"})), "aggregate struct_name Outer");
  Check(!MatchesExtractValue(ev01, MakePattern({0, 1}, ValueTypePattern{.struct_name = "Other"})), "aggregate struct_name Other does not match");
  Check(!MatchesExtractValue(ev01, MakePattern({0, 1}, ValueTypePattern{.kind = ValueTypeKind::Array})), "aggregate kind array does not match struct");
  Check(MatchesExtractValue(ev01, MakePattern({0, 1}, std::nullopt, ValueTypePattern{.component_type = dxp::ComponentType::I32})), "result component_type I32");
  Check(!MatchesExtractValue(ev00, MakePattern({0, 0}, std::nullopt, ValueTypePattern{.component_type = dxp::ComponentType::I32})), "result component_type I32 does not match f32 element");
  Check(MatchesExtractValue(ev0, MakePattern({0}, std::nullopt, ValueTypePattern{.kind = ValueTypeKind::Struct, .struct_name = "Inner"})), "result struct_name Inner");

  // ─── Array aggregate + array bounds ───────────────────────────────────────
  std::vector<llvm::Type*> params3 = {arr_outer};
  auto* fn3 = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), params3, false), llvm::Function::InternalLinkage, "f3", &module);
  auto* arg3 = &*fn3->getArgumentList().begin();
  auto* entry3 = llvm::BasicBlock::Create(ctx, "entry3", fn3);
  llvm::IRBuilder<> builder3(entry3);
  auto* ev_arr = builder3.CreateExtractValue(arg3, {0, 1});
  Check(MatchesExtractValue(ev_arr, MakePattern({0, 1})), "array aggregate [0,1]");
  Check(MatchesExtractValue(ev_arr, MakePattern({0, 1}, std::nullopt, ValueTypePattern{.component_type = dxp::ComponentType::F32})), "array element result F32");
  Check(!MatchesExtractValue(ev_arr, MakePattern({0, 1}, std::nullopt, ValueTypePattern{.kind = ValueTypeKind::Array})), "array element is not kind array");

  // ─── ResolveExtractType: positive + error paths ────────────────────────────
  {
    std::vector<uint32_t> path = {0, 1};
    auto resolved = ResolveExtractType(outer, path);
    Check(resolved.has_value() && resolved.value() == i32, "resolve Outer[0,1] == i32");
    std::vector<uint32_t> path0 = {0, 0};
    auto resolved0 = ResolveExtractType(outer, path0);
    Check(resolved0.has_value() && resolved0.value() == f32, "resolve Outer[0,0] == f32 (Inner's element)");
    std::vector<uint32_t> path_i = {0};
    auto resolved_i = ResolveExtractType(outer, path_i);
    Check(resolved_i.has_value() && resolved_i.value() == inner, "resolve Outer[0] == Inner");
    std::vector<uint32_t> path_arr = {0, 2};
    auto resolved_arr = ResolveExtractType(arr_outer, path_arr);
    Check(resolved_arr.has_value() && resolved_arr.value() == f32, "resolve ArrOuter[0,2] == float");
  }
  {
    std::vector<uint32_t> path = {0, 1, 0};
    auto resolved = ResolveExtractType(outer, path);
    Check(!resolved && resolved.error().find("non-aggregate type i32 at depth 2") != std::string::npos,
          "resolve Outer[0,1,0] reports non-aggregate at depth 2");
    std::vector<uint32_t> path_rr = {0, 2};
    auto resolved_rr = ResolveExtractType(outer, path_rr);
    Check(!resolved_rr && resolved_rr.error().find("out of range") != std::string::npos,
          "resolve Outer[0,2] reports out of range");
    std::vector<uint32_t> path_ar = {0, 3};
    auto resolved_ar = ResolveExtractType(arr_outer, path_ar);
    Check(!resolved_ar && resolved_ar.error().find("out of range") != std::string::npos,
          "resolve ArrOuter[0,3] reports array out of range");
    std::vector<uint32_t> path_op = {0};
    auto resolved_op = ResolveExtractType(opaque, path_op);
    Check(!resolved_op && resolved_op.error().find("opaque struct") != std::string::npos,
          "resolve opaque struct reports opaque struct");
    std::vector<uint32_t> empty;
    auto resolved_empty = ResolveExtractType(outer, empty);
    Check(!resolved_empty && resolved_empty.error().find("at least one index") != std::string::npos,
          "resolve empty path reports at least one index");
  }

  // ─── Literal struct aggregate ──────────────────────────────────────────────
  Check(MatchesExtractValue(ev_lit, MakePattern({1}, ValueTypePattern{.kind = ValueTypeKind::Struct})),
        "literal struct aggregate with kind struct");
  Check(!MatchesExtractValue(ev_lit, MakePattern({1}, ValueTypePattern{.struct_name = "Whatever"})),
        "literal struct does not match struct_name");

  // ─── ConstantIntToExtractIndex: unsigned interpretation ───────────────────
  Check(ConstantIntToExtractIndex("idx", 0, llvm::ConstantInt::get(i32, 7)).has_value()
        && ConstantIntToExtractIndex("idx", 0, llvm::ConstantInt::get(i32, 7)).value() == 7,
        "ConstantInt i32 7 → 7");
  Check(ConstantIntToExtractIndex("idx", 0, llvm::ConstantInt::get(i32, 0xFFFFFFFFu)).has_value()
        && ConstantIntToExtractIndex("idx", 0, llvm::ConstantInt::get(i32, 0xFFFFFFFFu)).value() == 4294967295u,
        "ConstantInt i32 all-ones → 4294967295 (unsigned bit pattern)");
  {
    auto sized = ConstantIntToExtractIndex("idx", 0, llvm::ConstantInt::get(i64, 0xFFFFFFFFFFFFFFFFull));
    Check(!sized && sized.error().find("is outside the supported unsigned index range") != std::string::npos,
          "ConstantInt i64 all-ones → oversized range error");
  }
  Check(ConstantIntToExtractIndex("idx", 0, llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx), 1u)).has_value()
        && ConstantIntToExtractIndex("idx", 0, llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx), 1u)).value() == 1,
        "ConstantInt i1 1 → 1 (signless bit pattern)");
  {
    auto null_ci = ConstantIntToExtractIndex("idx", 0, nullptr);
    Check(!null_ci && null_ci.error().find("must resolve to an integer ConstantInt") != std::string::npos,
          "null ConstantInt → ConstantInt error");
  }

  // ─── Null-input defensive behavior ────────────────────────────────────────
  {
    std::vector<uint32_t> path = {0};
    auto null_type = ResolveExtractType(nullptr, path);
    Check(!null_type && null_type.error().find("extractvalue aggregate type is null") != std::string::npos,
          "ResolveExtractType(null) reports null aggregate type");
  }
  Check(!MatchesExtractValue(nullptr, MakePattern({0, 1})), "MatchesExtractValue(null) returns false");

  // ─── MatchesTypePattern v1 semantics ───────────────────────────────────────
  Check(MatchesTypePattern(f32, ValueTypePattern{.component_type = dxp::ComponentType::F32}), "component F32 matches float");
  Check(!MatchesTypePattern(i32, ValueTypePattern{.component_type = dxp::ComponentType::F32}), "component F32 does not match i32");
  Check(MatchesTypePattern(v4f, ValueTypePattern{.component_type = dxp::ComponentType::F32}), "component F32 matches vector element");
  Check(!MatchesTypePattern(arr3f, ValueTypePattern{.component_type = dxp::ComponentType::F32}), "component F32 does not match array (structural only)");
  Check(!MatchesTypePattern(inner, ValueTypePattern{.component_type = dxp::ComponentType::F32}), "component F32 does not match struct (structural only)");
  Check(MatchesTypePattern(arr3f, ValueTypePattern{.kind = ValueTypeKind::Array}), "kind array matches array");
  Check(!MatchesTypePattern(outer, ValueTypePattern{.kind = ValueTypeKind::Array}), "kind array does not match struct");
  Check(MatchesTypePattern(v4f, ValueTypePattern{.kind = ValueTypeKind::Vector}), "kind vector matches vector");
  Check(!MatchesTypePattern(f32, ValueTypePattern{.kind = ValueTypeKind::Vector}), "kind vector does not match scalar");
  Check(MatchesTypePattern(i32, ValueTypePattern{.kind = ValueTypeKind::Scalar, .component_type = dxp::ComponentType::I32}), "kind scalar + I32 matches i32");
  Check(!MatchesTypePattern(i64, ValueTypePattern{.kind = ValueTypeKind::Scalar, .component_type = dxp::ComponentType::I32}), "kind scalar + I32 does not match i64");
  Check(MatchesTypePattern(i64, ValueTypePattern{}), "empty pattern matches anything");

  if (failures != 0) {
    std::cerr << failures << " detail test(s) failed\n";
    return 1;
  }
  std::cout << "sm6_extract_value_detail_test passed\n";
  std::cout.flush();
  std::cerr.flush();
  return 0;
}
