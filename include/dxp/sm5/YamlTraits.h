#pragma once

#include "dxp/sm5/Model.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/YAMLTraits.h"

namespace dxp {
namespace sm5 {

// ---------------------------------------------------------------------------
// Free functions: string -> enum lookup tables
// ---------------------------------------------------------------------------

inline bool ParseInterpolationMode(const llvm::StringRef &scalar,
                                   InterpolationMode &value) {
  if (scalar == "undefined") {
    value = InterpolationMode::Undefined;
    return true;
  }
  if (scalar == "constant") {
    value = InterpolationMode::Constant;
    return true;
  }
  if (scalar == "linear") {
    value = InterpolationMode::Linear;
    return true;
  }
  if (scalar == "linear_centroid") {
    value = InterpolationMode::LinearCentroid;
    return true;
  }
  if (scalar == "linear_noperspective") {
    value = InterpolationMode::LinearNoperspective;
    return true;
  }
  if (scalar == "linear_noperspective_centroid") {
    value = InterpolationMode::LinearNoperspectiveCentroid;
    return true;
  }
  if (scalar == "linear_sample") {
    value = InterpolationMode::LinearSample;
    return true;
  }
  if (scalar == "linear_noperspective_sample") {
    value = InterpolationMode::LinearNoperspectiveSample;
    return true;
  }
  return false;
}

inline bool ParseResourceDimension(const llvm::StringRef &scalar,
                                   ResourceDimension &value) {
  if (scalar == "texture_1d") {
    value = ResourceDimension::Texture1D;
    return true;
  }
  if (scalar == "texture_2d") {
    value = ResourceDimension::Texture2D;
    return true;
  }
  if (scalar == "texture_2dms") {
    value = ResourceDimension::Texture2DMS;
    return true;
  }
  if (scalar == "texture_cube") {
    value = ResourceDimension::TextureCube;
    return true;
  }
  if (scalar == "texture_3d") {
    value = ResourceDimension::Texture3D;
    return true;
  }
  if (scalar == "texture_2d_array") {
    value = ResourceDimension::Texture2DArray;
    return true;
  }
  if (scalar == "texture_2dms_array") {
    value = ResourceDimension::Texture2DMSArray;
    return true;
  }
  if (scalar == "texture_cube_array") {
    value = ResourceDimension::TextureCubeArray;
    return true;
  }
  return false;
}

inline bool ParseCbufferAccessPattern(const llvm::StringRef &scalar,
                                      CbufferAccessPattern &value) {
  if (scalar == "immediate_indexed") {
    value = CbufferAccessPattern::ImmediateIndexed;
    return true;
  }
  if (scalar == "dynamic_indexed") {
    value = CbufferAccessPattern::DynamicIndexed;
    return true;
  }
  return false;
}

inline bool ParseSamplerMode(const llvm::StringRef &scalar, SamplerMode &value) {
  if (scalar == "default") {
    value = SamplerMode::Default;
    return true;
  }
  if (scalar == "comparison") {
    value = SamplerMode::Comparison;
    return true;
  }
  if (scalar == "mono") {
    value = SamplerMode::Mono;
    return true;
  }
  return false;
}

} // namespace sm5
} // namespace dxp

// ---------------------------------------------------------------------------
// LLVM YAML ScalarTraits for SM5 recipe enums
// Must be outside dxp namespace so LLVM's yamlize can find them via ADL.
// ---------------------------------------------------------------------------

namespace llvm {
namespace yaml {

template <> struct ScalarTraits<dxp::sm5::RecipeRuleApplicationMode> {
  static void output(const dxp::sm5::RecipeRuleApplicationMode &value, void *,
                     raw_ostream &out) {
    switch (value) {
    case dxp::sm5::RecipeRuleApplicationMode::First:
      out << "first";
      break;
    case dxp::sm5::RecipeRuleApplicationMode::Last:
      out << "last";
      break;
    case dxp::sm5::RecipeRuleApplicationMode::MatchAll:
      out << "match_all";
      break;
    }
  }

  static StringRef input(StringRef scalar, void *,
                         dxp::sm5::RecipeRuleApplicationMode &value) {
    if (scalar.empty() || scalar == "first") {
      value = dxp::sm5::RecipeRuleApplicationMode::First;
      return {};
    }
    if (scalar == "last") {
      value = dxp::sm5::RecipeRuleApplicationMode::Last;
      return {};
    }
    if (scalar == "match_all") {
      value = dxp::sm5::RecipeRuleApplicationMode::MatchAll;
      return {};
    }
    return StringRef((std::string{"unsupported SM5 rule application mode '" +
                                  scalar.str() + "'"}));
  }

  static bool mustQuote(StringRef) { return false; }
};

template <> struct ScalarTraits<dxp::sm5::RecipeRuleRewriteMode> {
  static void output(const dxp::sm5::RecipeRuleRewriteMode &value, void *,
                     raw_ostream &out) {
    switch (value) {
    case dxp::sm5::RecipeRuleRewriteMode::None:
      out << "none";
      break;
    case dxp::sm5::RecipeRuleRewriteMode::Replace:
      out << "replace";
      break;
    case dxp::sm5::RecipeRuleRewriteMode::Before:
      out << "before";
      break;
    case dxp::sm5::RecipeRuleRewriteMode::After:
      out << "after";
      break;
    case dxp::sm5::RecipeRuleRewriteMode::ReplaceRange:
      out << "replace_range";
      break;
    }
  }

  static StringRef input(StringRef scalar, void *,
                         dxp::sm5::RecipeRuleRewriteMode &value) {
    if (scalar.empty() || scalar == "replace") {
      value = dxp::sm5::RecipeRuleRewriteMode::Replace;
      return {};
    }
    if (scalar == "none") {
      value = dxp::sm5::RecipeRuleRewriteMode::None;
      return {};
    }
    if (scalar == "before") {
      value = dxp::sm5::RecipeRuleRewriteMode::Before;
      return {};
    }
    if (scalar == "after") {
      value = dxp::sm5::RecipeRuleRewriteMode::After;
      return {};
    }
    if (scalar == "replace_range") {
      value = dxp::sm5::RecipeRuleRewriteMode::ReplaceRange;
      return {};
    }
    if (scalar == "auto") {
      return "SM5 rewrite mode auto was removed; use replace or replace_range";
    }
    return StringRef((std::string{"unsupported SM5 rewrite mode '" +
                                  scalar.str() + "'"}));
  }

  static bool mustQuote(StringRef) { return false; }
};

template <> struct ScalarTraits<dxp::sm5::RecipeUavKind> {
  static void output(const dxp::sm5::RecipeUavKind &value, void *,
                     raw_ostream &out) {
    switch (value) {
    case dxp::sm5::RecipeUavKind::Typed:
      out << "typed";
      break;
    case dxp::sm5::RecipeUavKind::Raw:
      out << "raw";
      break;
    case dxp::sm5::RecipeUavKind::Structured:
      out << "structured";
      break;
    }
  }

  static StringRef input(StringRef scalar, void *,
                         dxp::sm5::RecipeUavKind &value) {
    if (scalar == "typed") {
      value = dxp::sm5::RecipeUavKind::Typed;
      return {};
    }
    if (scalar == "raw") {
      value = dxp::sm5::RecipeUavKind::Raw;
      return {};
    }
    if (scalar == "structured") {
      value = dxp::sm5::RecipeUavKind::Structured;
      return {};
    }
    return StringRef((std::string{"unsupported SM5 uav kind '" +
                                  scalar.str() + "'"}));
  }

  static bool mustQuote(StringRef) { return false; }
};

template <> struct ScalarTraits<dxp::sm5::RecipeOperandIndexRepresentation> {
  static void output(
      const dxp::sm5::RecipeOperandIndexRepresentation &value, void *,
      raw_ostream &out) {
    switch (value) {
    case dxp::sm5::RecipeOperandIndexRepresentation::Immediate32:
      out << "immediate32";
      break;
    case dxp::sm5::RecipeOperandIndexRepresentation::Immediate64:
      out << "immediate64";
      break;
    case dxp::sm5::RecipeOperandIndexRepresentation::Relative:
      out << "relative";
      break;
    case dxp::sm5::RecipeOperandIndexRepresentation::Immediate32PlusRelative:
      out << "immediate32_plus_relative";
      break;
    case dxp::sm5::RecipeOperandIndexRepresentation::Immediate64PlusRelative:
      out << "immediate64_plus_relative";
      break;
    }
  }

  static StringRef input(StringRef scalar, void *,
                         dxp::sm5::RecipeOperandIndexRepresentation &value) {
    if (scalar.empty() || scalar == "immediate32") {
      value = dxp::sm5::RecipeOperandIndexRepresentation::Immediate32;
      return {};
    }
    if (scalar == "immediate64") {
      value = dxp::sm5::RecipeOperandIndexRepresentation::Immediate64;
      return {};
    }
    if (scalar == "relative") {
      value = dxp::sm5::RecipeOperandIndexRepresentation::Relative;
      return {};
    }
    if (scalar == "immediate32_plus_relative") {
      value = dxp::sm5::RecipeOperandIndexRepresentation::Immediate32PlusRelative;
      return {};
    }
    if (scalar == "immediate64_plus_relative") {
      value = dxp::sm5::RecipeOperandIndexRepresentation::Immediate64PlusRelative;
      return {};
    }
    return StringRef((std::string{"unsupported SM5 operand index representation '" +
                                  scalar.str() + "'"}));
  }

  static bool mustQuote(StringRef) { return false; }
};

template <> struct ScalarTraits<dxp::sm5::InterpolationMode> {
  static void output(const dxp::sm5::InterpolationMode &value, void *,
                     raw_ostream &out) {
    switch (value) {
    case dxp::sm5::InterpolationMode::Undefined:
      out << "undefined";
      break;
    case dxp::sm5::InterpolationMode::Constant:
      out << "constant";
      break;
    case dxp::sm5::InterpolationMode::Linear:
      out << "linear";
      break;
    case dxp::sm5::InterpolationMode::LinearCentroid:
      out << "linear_centroid";
      break;
    case dxp::sm5::InterpolationMode::LinearNoperspective:
      out << "linear_noperspective";
      break;
    case dxp::sm5::InterpolationMode::LinearNoperspectiveCentroid:
      out << "linear_noperspective_centroid";
      break;
    case dxp::sm5::InterpolationMode::LinearSample:
      out << "linear_sample";
      break;
    case dxp::sm5::InterpolationMode::LinearNoperspectiveSample:
      out << "linear_noperspective_sample";
      break;
    }
  }

  static StringRef input(StringRef scalar, void *,
                         dxp::sm5::InterpolationMode &value) {
    if (dxp::sm5::ParseInterpolationMode(scalar, value))
      return {};
    return StringRef((std::string{"unsupported SM5 interpolation mode '" +
                                  scalar.str() + "'"}));
  }

  static bool mustQuote(StringRef) { return false; }
};

template <> struct ScalarTraits<dxp::sm5::ResourceDimension> {
  static void output(const dxp::sm5::ResourceDimension &value, void *,
                     raw_ostream &out) {
    switch (value) {
    case dxp::sm5::ResourceDimension::Texture1D:
      out << "texture_1d";
      break;
    case dxp::sm5::ResourceDimension::Texture2D:
      out << "texture_2d";
      break;
    case dxp::sm5::ResourceDimension::Texture2DMS:
      out << "texture_2dms";
      break;
    case dxp::sm5::ResourceDimension::TextureCube:
      out << "texture_cube";
      break;
    case dxp::sm5::ResourceDimension::Texture3D:
      out << "texture_3d";
      break;
    case dxp::sm5::ResourceDimension::Texture2DArray:
      out << "texture_2d_array";
      break;
    case dxp::sm5::ResourceDimension::Texture2DMSArray:
      out << "texture_2dms_array";
      break;
    case dxp::sm5::ResourceDimension::TextureCubeArray:
      out << "texture_cube_array";
      break;
    }
  }

  static StringRef input(StringRef scalar, void *,
                         dxp::sm5::ResourceDimension &value) {
    if (dxp::sm5::ParseResourceDimension(scalar, value))
      return {};
    return StringRef((std::string{"unsupported SM5 resource dimension '" +
                                  scalar.str() + "'"}));
  }

  static bool mustQuote(StringRef) { return false; }
};

template <> struct ScalarTraits<dxp::sm5::CbufferAccessPattern> {
  static void output(const dxp::sm5::CbufferAccessPattern &value, void *,
                     raw_ostream &out) {
    switch (value) {
    case dxp::sm5::CbufferAccessPattern::ImmediateIndexed:
      out << "immediate_indexed";
      break;
    case dxp::sm5::CbufferAccessPattern::DynamicIndexed:
      out << "dynamic_indexed";
      break;
    }
  }

  static StringRef input(StringRef scalar, void *,
                         dxp::sm5::CbufferAccessPattern &value) {
    if (dxp::sm5::ParseCbufferAccessPattern(scalar, value))
      return {};
    return StringRef((std::string{"unsupported SM5 cbuffer access pattern '" +
                                  scalar.str() + "'"}));
  }

  static bool mustQuote(StringRef) { return false; }
};

template <> struct ScalarTraits<dxp::sm5::SamplerMode> {
  static void output(const dxp::sm5::SamplerMode &value, void *,
                     raw_ostream &out) {
    switch (value) {
    case dxp::sm5::SamplerMode::Default:
      out << "default";
      break;
    case dxp::sm5::SamplerMode::Comparison:
      out << "comparison";
      break;
    case dxp::sm5::SamplerMode::Mono:
      out << "mono";
      break;
    }
  }

  static StringRef input(StringRef scalar, void *,
                         dxp::sm5::SamplerMode &value) {
    if (dxp::sm5::ParseSamplerMode(scalar, value))
      return {};
    return StringRef((std::string{"unsupported SM5 sampler mode '" +
                                  scalar.str() + "'"}));
  }

  static bool mustQuote(StringRef) { return false; }
};

} // namespace yaml
} // namespace llvm
