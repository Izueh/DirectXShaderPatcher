#include "../../../include/dxp/sm6/RecipeParse.h"

#include "../../../include/dxp/sm6/Resources.h"
#include "../../../include/dxp/sm6/Transforms.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/YAMLTraits.h"

#include "dxc/DXIL/DxilCompType.h"
#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilOperations.h"

namespace {

template <typename TValue> struct RecipeParseEntry {
  const char *name = nullptr;
  TValue value{};
};

template <typename TValue>
static bool
ParseRecipeValueByTable(const std::string &text, TValue &value,
                        llvm::ArrayRef<RecipeParseEntry<TValue>> entries) {
  for (const RecipeParseEntry<TValue> &entry : entries) {
    if (text == entry.name) {
      value = entry.value;
      return true;
    }
  }

  return false;
}

template <typename TValue, size_t N>
static bool
ParseRecipeValueByTable(const std::string &text, TValue &value,
                        const RecipeParseEntry<TValue> (&entries)[N]) {
  return ParseRecipeValueByTable(
      text, value, llvm::ArrayRef<RecipeParseEntry<TValue>>(entries));
}

struct ParsedRecipeResourceRef {
  bool found = false;
  std::string resourceName;
  ResourceBindingDesc binding{};
};

static bool ParseRecipeUnsignedValue(const std::string &text, unsigned &value,
                                     std::string &error) {
  if (llvm::StringRef(text).getAsInteger(0, value)) {
    error = "invalid unsigned integer '" + text + "'";
    return false;
  }

  return true;
}

static bool ParseRecipeBoolValue(const std::string &text, bool &value,
                                 std::string &error) {
  if (text == "true" || text == "1") {
    value = true;
    return true;
  }

  if (text == "false" || text == "0") {
    value = false;
    return true;
  }

  error = "invalid boolean '" + text + "'";
  return false;
}

static std::string LowercaseRecipeToken(std::string text) {
  return llvm::StringRef(text).lower();
}

static bool ParseRecipeOpCode(const std::string &text, hlsl::OP::OpCode &opCode,
                              std::string &error) {
  const std::string lowered = LowercaseRecipeToken(text);
  for (unsigned index = 0;
       index < static_cast<unsigned>(hlsl::OP::OpCode::NumOpCodes); ++index) {
    const hlsl::OP::OpCode candidate = static_cast<hlsl::OP::OpCode>(index);
    const char *candidateName = hlsl::OP::GetOpCodeName(candidate);
    if (candidateName == nullptr)
      continue;
    if (LowercaseRecipeToken(candidateName) == lowered) {
      opCode = candidate;
      return true;
    }
  }

  error = "unsupported dx.op opcode '" + text + "'";
  return false;
}

static bool ParseRecipeInstructionOpcode(const std::string &text,
                                         unsigned &opcode, std::string &error) {
  const std::string lowered = LowercaseRecipeToken(text);
  for (unsigned index = 1; index < llvm::Instruction::OtherOpsEnd; ++index) {
    const char *candidateName = llvm::Instruction::getOpcodeName(index);
    if (candidateName == nullptr)
      continue;
    if (LowercaseRecipeToken(candidateName) == lowered) {
      opcode = index;
      return true;
    }
  }

  error = "unsupported LLVM instruction opcode '" + text + "'";
  return false;
}

static bool TryResolveParsedRecipeResourceRef(
    const std::string &id,
    const std::unordered_map<std::string, TextureResourceDesc> &textures,
    const std::unordered_map<std::string, TextureResourceDesc> &uavs,
    const std::unordered_map<std::string, CBufferDesc> &cbuffers,
    const std::unordered_map<std::string, SamplerDesc> &samplers,
    ParsedRecipeResourceRef &resourceRef) {
  resourceRef = ParsedRecipeResourceRef{};

  auto textureIt = textures.find(id);
  if (textureIt != textures.end()) {
    resourceRef.found = true;
    resourceRef.resourceName = textureIt->second.name;
    resourceRef.binding = textureIt->second.binding;
    resourceRef.binding.SetResourceClass(hlsl::DXIL::ResourceClass::SRV);
    return true;
  }

  auto uavIt = uavs.find(id);
  if (uavIt != uavs.end()) {
    resourceRef.found = true;
    resourceRef.resourceName = uavIt->second.name;
    resourceRef.binding = uavIt->second.binding;
    resourceRef.binding.SetResourceClass(hlsl::DXIL::ResourceClass::UAV);
    return true;
  }

  auto cbufferIt = cbuffers.find(id);
  if (cbufferIt != cbuffers.end()) {
    resourceRef.found = true;
    resourceRef.resourceName = cbufferIt->second.name;
    resourceRef.binding = cbufferIt->second.binding;
    resourceRef.binding.SetResourceClass(hlsl::DXIL::ResourceClass::CBuffer);
    return true;
  }

  auto samplerIt = samplers.find(id);
  if (samplerIt != samplers.end()) {
    resourceRef.found = true;
    resourceRef.resourceName = samplerIt->second.name;
    resourceRef.binding = samplerIt->second.binding;
    resourceRef.binding.SetResourceClass(hlsl::DXIL::ResourceClass::Sampler);
    return true;
  }

  return true;
}

static bool ParseRecipeRewriteMode(const std::string &text,
                                   DxilRewriteMode &mode, std::string &error) {
  static const RecipeParseEntry<DxilRewriteMode> kRewriteModeEntries[] = {
      {"None", DxilRewriteMode::None},
      {"Replace", DxilRewriteMode::Replace},
      {"ReplaceRange", DxilRewriteMode::ReplaceRange},
  };
  if (ParseRecipeValueByTable(text, mode, kRewriteModeEntries))
    return true;

  error = "unsupported rewrite mode '" + text + "'";
  return false;
}

static bool ParseRecipeRuleApplicationMode(const std::string &text,
                                           DxilRecipeRuleApplicationMode &mode,
                                           std::string &error) {
  static const RecipeParseEntry<DxilRecipeRuleApplicationMode>
      kRuleApplicationModeEntries[] = {
          {"First", DxilRecipeRuleApplicationMode::First},
          {"Last", DxilRecipeRuleApplicationMode::Last},
          {"MatchAll", DxilRecipeRuleApplicationMode::MatchAll},
      };
  if (ParseRecipeValueByTable(text, mode, kRuleApplicationModeEntries))
    return true;

  error = "unsupported rule application mode '" + text + "'";
  return false;
}

static bool ParseRecipeResourceKind(const std::string &text,
                                    hlsl::DXIL::ResourceKind &kind,
                                    std::string &error) {
  static const RecipeParseEntry<hlsl::DXIL::ResourceKind>
      kResourceKindEntries[] = {
          {"Texture1D", hlsl::DXIL::ResourceKind::Texture1D},
          {"Texture2D", hlsl::DXIL::ResourceKind::Texture2D},
          {"Texture2DMS", hlsl::DXIL::ResourceKind::Texture2DMS},
          {"Texture3D", hlsl::DXIL::ResourceKind::Texture3D},
          {"TextureCube", hlsl::DXIL::ResourceKind::TextureCube},
          {"Texture1DArray", hlsl::DXIL::ResourceKind::Texture1DArray},
          {"Texture2DArray", hlsl::DXIL::ResourceKind::Texture2DArray},
          {"Texture2DMSArray", hlsl::DXIL::ResourceKind::Texture2DMSArray},
          {"TextureCubeArray", hlsl::DXIL::ResourceKind::TextureCubeArray},
          {"TypedBuffer", hlsl::DXIL::ResourceKind::TypedBuffer},
          {"RawBuffer", hlsl::DXIL::ResourceKind::RawBuffer},
          {"StructuredBuffer", hlsl::DXIL::ResourceKind::StructuredBuffer},
          {"CBuffer", hlsl::DXIL::ResourceKind::CBuffer},
          {"Sampler", hlsl::DXIL::ResourceKind::Sampler},
          {"TBuffer", hlsl::DXIL::ResourceKind::TBuffer},
          {"RTAccelerationStructure",
           hlsl::DXIL::ResourceKind::RTAccelerationStructure},
          {"FeedbackTexture2D", hlsl::DXIL::ResourceKind::FeedbackTexture2D},
          {"FeedbackTexture2DArray",
           hlsl::DXIL::ResourceKind::FeedbackTexture2DArray},
      };
  if (ParseRecipeValueByTable(text, kind, kResourceKindEntries))
    return true;

  error = "unsupported resource kind '" + text + "'";
  return false;
}

static bool ParseRecipeResourceClass(const std::string &text,
                                     hlsl::DXIL::ResourceClass &resourceClass,
                                     std::string &error) {
  static const RecipeParseEntry<hlsl::DXIL::ResourceClass>
      kResourceClassEntries[] = {
          {"SRV", hlsl::DXIL::ResourceClass::SRV},
          {"UAV", hlsl::DXIL::ResourceClass::UAV},
          {"CBuffer", hlsl::DXIL::ResourceClass::CBuffer},
          {"Sampler", hlsl::DXIL::ResourceClass::Sampler},
      };
  if (ParseRecipeValueByTable(text, resourceClass, kResourceClassEntries))
    return true;

  error = "unsupported resource class '" + text + "'";
  return false;
}

static bool ParseRecipeComponentType(const std::string &text,
                                     hlsl::DXIL::ComponentType &componentType,
                                     std::string &error) {
  static const RecipeParseEntry<hlsl::DXIL::ComponentType>
      kComponentTypeEntries[] = {
          {"F32", hlsl::DXIL::ComponentType::F32},
          {"U32", hlsl::DXIL::ComponentType::U32},
          {"I32", hlsl::DXIL::ComponentType::I32},
      };
  if (ParseRecipeValueByTable(text, componentType, kComponentTypeEntries))
    return true;

  error = "unsupported component type '" + text + "'";
  return false;
}

static bool ParseRecipeCompTypeKind(const std::string &text,
                                    hlsl::CompType::Kind &compType,
                                    std::string &error) {
  static const RecipeParseEntry<hlsl::CompType::Kind> kCompTypeKindEntries[] = {
      {"F32", hlsl::CompType::getF32().GetKind()},
      {"U32", hlsl::CompType::getU32().GetKind()},
      {"I32", hlsl::CompType::getI32().GetKind()},
  };
  if (ParseRecipeValueByTable(text, compType, kCompTypeKindEntries))
    return true;

  error = "unsupported cbuffer field type '" + text + "'";
  return false;
}

static bool ParseRecipeBinaryInstructionOpcode(const std::string &text,
                                               unsigned &instructionOpcode,
                                               std::string &error) {
  static const RecipeParseEntry<unsigned> kBinaryInstructionOpcodeEntries[] = {
      {"Add", llvm::Instruction::Add},
      {"Mul", llvm::Instruction::Mul},
      {"And", llvm::Instruction::And},
      {"URem", llvm::Instruction::URem},
  };
  if (ParseRecipeValueByTable(text, instructionOpcode,
                              kBinaryInstructionOpcodeEntries)) {
    return true;
  }

  error = "unsupported binary instruction opcode '" + text + "'";
  return false;
}

static void
SortOperandPatternTree(std::vector<DxilOperandPattern> &operandPatterns) {
  std::sort(operandPatterns.begin(), operandPatterns.end(),
            [](const DxilOperandPattern &lhs, const DxilOperandPattern &rhs) {
              return lhs.operandIndex < rhs.operandIndex;
            });

  for (DxilOperandPattern &operandPattern : operandPatterns)
    SortOperandPatternTree(operandPattern.operandPatterns);
}

static bool ParseRecipeCastInstructionOpcode(const std::string &text,
                                             unsigned &castOpcode,
                                             std::string &error) {
  static const RecipeParseEntry<unsigned> kCastInstructionOpcodeEntries[] = {
      {"UIToFP", llvm::Instruction::UIToFP},
      {"FPToUI", llvm::Instruction::FPToUI},
      {"SIToFP", llvm::Instruction::SIToFP},
      {"FPToSI", llvm::Instruction::FPToSI},
      {"BitCast", llvm::Instruction::BitCast},
  };
  if (ParseRecipeValueByTable(text, castOpcode, kCastInstructionOpcodeEntries))
    return true;

  error = "unsupported cast instruction opcode '" + text + "'";
  return false;
}

struct YamlRecipeBindingModel {
  std::string bind = "auto";
  unsigned space = 0;
};

struct YamlRecipeTextureModel {
  std::string id;
  std::string name;
  std::string kind;
  std::string element;
  unsigned width = 0;
  YamlRecipeBindingModel binding;
};

struct YamlRecipeFieldModel {
  std::string name;
  std::string type;
  unsigned width = 0;
  unsigned offset = 0;
};

struct YamlRecipeCBufferModel {
  std::string id;
  std::string name;
  std::string type;
  unsigned size = 0;
  YamlRecipeBindingModel binding;
  std::vector<YamlRecipeFieldModel> fields;
};

struct YamlRecipeSamplerModel {
  std::string id;
  std::string name;
  YamlRecipeBindingModel binding;
};

struct YamlRecipeResourcesModel {
  std::vector<YamlRecipeTextureModel> textures;
  std::vector<YamlRecipeTextureModel> texture_uavs;
  std::vector<YamlRecipeCBufferModel> cbuffers;
  std::vector<YamlRecipeSamplerModel> samplers;
};

struct YamlRecipeOperandModel {
  unsigned index = 0;
  std::string kind;
  std::string capture;
  unsigned value = 0;
  std::string opcode;
  std::string resource_class;
  std::string resource_kind;
  std::string resource_name;
  std::string resource_name_like;
  int bind = -1;
  int space = -1;
  std::vector<YamlRecipeOperandModel> operands;
};

struct YamlRecipeBindingPatternModel {
  std::string kind;
  std::string capture;
  std::string opcode;
  std::vector<YamlRecipeOperandModel> operands;
};

struct YamlRecipeEmitOperandModel {
  unsigned index = 0;
  std::string kind;
  std::string capture;
  std::string id;
  unsigned value = 0;
};

struct YamlRecipeEmitModel {
  std::string kind;
  std::string id;
  std::string resource;
  std::string handle;
  std::string opcode;
  std::string type;
  std::string aggregate;
  unsigned index = 0;
  std::vector<YamlRecipeEmitOperandModel> operands;
};

struct YamlRecipeMatchModel {
  std::string opcode;
  std::string capture;
  std::string replace;
  std::string mode = "Replace";
  bool prune_dead = true;
  std::vector<std::string> prune_captures;
  std::vector<YamlRecipeOperandModel> operands;
};

struct YamlRecipePrefilterModel {
  std::string id;
  std::string name;
  std::string opcode;
  std::string capture;
  std::vector<YamlRecipeOperandModel> operands;
};

struct YamlRecipeRuleModel {
  std::string id;
  std::string name;
  YamlRecipeMatchModel match;
  std::vector<YamlRecipeBindingPatternModel> bindings;
  std::vector<YamlRecipeEmitModel> emit;
  std::string replace_with;
  std::string replace_with_capture;
};

struct YamlRecipeStepModel {
  std::string kind;
  std::string id;
  std::string pattern;
  std::vector<std::string> patterns;
  std::string rule;
  std::vector<std::string> rules;
  std::string name;
  std::string mode;
  bool required = true;
};

struct YamlRecipeOptionsModel {
  bool restore_reflection = true;
};

struct YamlRecipeDocumentModel {
  unsigned version = 1;
  YamlRecipeOptionsModel options;
  YamlRecipeResourcesModel resources;
  std::vector<YamlRecipePrefilterModel> prefilters;
  std::vector<YamlRecipeRuleModel> rewrite_rules;
  std::vector<YamlRecipeStepModel> steps;
};

} // namespace

LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeFieldModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeTextureModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeCBufferModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeSamplerModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeOperandModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeBindingPatternModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeEmitOperandModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeEmitModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipePrefilterModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeRuleModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(YamlRecipeStepModel)
LLVM_YAML_IS_SEQUENCE_VECTOR(std::string)

namespace llvm {
namespace yaml {

template <> struct MappingTraits<YamlRecipeBindingModel> {
  static void mapping(IO &io, YamlRecipeBindingModel &binding) {
    io.mapOptional("bind", binding.bind, std::string("auto"));
    io.mapOptional("space", binding.space, 0u);
  }
};

template <> struct MappingTraits<YamlRecipeTextureModel> {
  static void mapping(IO &io, YamlRecipeTextureModel &texture) {
    io.mapRequired("id", texture.id);
    io.mapRequired("name", texture.name);
    io.mapRequired("kind", texture.kind);
    io.mapRequired("element", texture.element);
    io.mapRequired("width", texture.width);
    io.mapOptional("binding", texture.binding);
  }
};

template <> struct MappingTraits<YamlRecipeFieldModel> {
  static void mapping(IO &io, YamlRecipeFieldModel &field) {
    io.mapRequired("name", field.name);
    io.mapRequired("type", field.type);
    io.mapRequired("width", field.width);
    io.mapRequired("offset", field.offset);
  }
};

template <> struct MappingTraits<YamlRecipeCBufferModel> {
  static void mapping(IO &io, YamlRecipeCBufferModel &cbuffer) {
    io.mapRequired("id", cbuffer.id);
    io.mapRequired("name", cbuffer.name);
    io.mapRequired("type", cbuffer.type);
    io.mapRequired("size", cbuffer.size);
    io.mapOptional("binding", cbuffer.binding);
    io.mapRequired("fields", cbuffer.fields);
  }
};

template <> struct MappingTraits<YamlRecipeSamplerModel> {
  static void mapping(IO &io, YamlRecipeSamplerModel &sampler) {
    io.mapRequired("id", sampler.id);
    io.mapRequired("name", sampler.name);
    io.mapOptional("binding", sampler.binding);
  }
};

template <> struct MappingTraits<YamlRecipeResourcesModel> {
  static void mapping(IO &io, YamlRecipeResourcesModel &resources) {
    io.mapOptional("textures", resources.textures);
    io.mapOptional("texture_uavs", resources.texture_uavs);
    io.mapOptional("cbuffers", resources.cbuffers);
    io.mapOptional("samplers", resources.samplers);
  }
};

template <> struct MappingTraits<YamlRecipeOperandModel> {
  static void mapping(IO &io, YamlRecipeOperandModel &operand) {
    io.mapRequired("index", operand.index);
    io.mapRequired("kind", operand.kind);
    io.mapOptional("capture", operand.capture);
    io.mapOptional("value", operand.value, 0u);
    io.mapOptional("opcode", operand.opcode);
    io.mapOptional("resource_class", operand.resource_class);
    io.mapOptional("resource_kind", operand.resource_kind);
    io.mapOptional("resource_name", operand.resource_name);
    io.mapOptional("resource_name_like", operand.resource_name_like);
    io.mapOptional("bind", operand.bind, -1);
    io.mapOptional("space", operand.space, -1);
    io.mapOptional("operands", operand.operands);
  }
};

template <> struct MappingTraits<YamlRecipeBindingPatternModel> {
  static void mapping(IO &io, YamlRecipeBindingPatternModel &binding) {
    io.mapRequired("kind", binding.kind);
    io.mapRequired("capture", binding.capture);
    io.mapRequired("opcode", binding.opcode);
    io.mapOptional("operands", binding.operands);
  }
};

template <> struct MappingTraits<YamlRecipeEmitOperandModel> {
  static void mapping(IO &io, YamlRecipeEmitOperandModel &operand) {
    io.mapRequired("index", operand.index);
    io.mapRequired("kind", operand.kind);
    io.mapOptional("capture", operand.capture);
    io.mapOptional("id", operand.id);
    io.mapOptional("value", operand.value, 0u);
  }
};

template <> struct MappingTraits<YamlRecipeEmitModel> {
  static void mapping(IO &io, YamlRecipeEmitModel &emit) {
    io.mapRequired("kind", emit.kind);
    io.mapOptional("id", emit.id);
    io.mapOptional("resource", emit.resource);
    io.mapOptional("handle", emit.handle);
    io.mapOptional("opcode", emit.opcode);
    io.mapOptional("type", emit.type);
    io.mapOptional("aggregate", emit.aggregate);
    io.mapOptional("index", emit.index, 0u);
    io.mapOptional("operands", emit.operands);
  }
};

template <> struct MappingTraits<YamlRecipeMatchModel> {
  static void mapping(IO &io, YamlRecipeMatchModel &match) {
    io.mapRequired("opcode", match.opcode);
    io.mapOptional("replace", match.replace);
    io.mapOptional("capture", match.capture);
    io.mapOptional("mode", match.mode, std::string("Replace"));
    io.mapOptional("prune_dead", match.prune_dead, true);
    io.mapOptional("prune_captures", match.prune_captures);
    io.mapOptional("operands", match.operands);
  }
};

template <> struct MappingTraits<YamlRecipePrefilterModel> {
  static void mapping(IO &io, YamlRecipePrefilterModel &prefilter) {
    io.mapRequired("id", prefilter.id);
    io.mapOptional("name", prefilter.name);
    io.mapRequired("opcode", prefilter.opcode);
    io.mapOptional("capture", prefilter.capture);
    io.mapOptional("operands", prefilter.operands);
  }
};

template <> struct MappingTraits<YamlRecipeRuleModel> {
  static void mapping(IO &io, YamlRecipeRuleModel &rule) {
    io.mapRequired("id", rule.id);
    io.mapOptional("name", rule.name);
    io.mapRequired("match", rule.match);
    io.mapOptional("bindings", rule.bindings);
    io.mapOptional("emit", rule.emit);
    io.mapOptional("replace_with", rule.replace_with);
    io.mapOptional("replace_with_capture", rule.replace_with_capture);
  }
};

template <> struct MappingTraits<YamlRecipeStepModel> {
  static void mapping(IO &io, YamlRecipeStepModel &step) {
    io.mapRequired("kind", step.kind);
    io.mapOptional("id", step.id);
    io.mapOptional("pattern", step.pattern);
    io.mapOptional("patterns", step.patterns);
    io.mapOptional("rule", step.rule);
    io.mapOptional("rules", step.rules);
    io.mapOptional("name", step.name);
    io.mapOptional("mode", step.mode, std::string());
    io.mapOptional("required", step.required, true);
  }
};

template <> struct MappingTraits<YamlRecipeOptionsModel> {
  static void mapping(IO &io, YamlRecipeOptionsModel &options) {
    io.mapOptional("restore_reflection", options.restore_reflection, true);
  }
};

template <> struct MappingTraits<YamlRecipeDocumentModel> {
  static void mapping(IO &io, YamlRecipeDocumentModel &document) {
    io.mapOptional("version", document.version, 1u);
    io.mapOptional("options", document.options);
    io.mapOptional("resources", document.resources);
    io.mapOptional("prefilters", document.prefilters);
    io.mapOptional("rewrite_rules", document.rewrite_rules);
    io.mapOptional("steps", document.steps);
  }
};

} // namespace yaml
} // namespace llvm

namespace {

static bool
ParseYamlRecipeBindingModel(const YamlRecipeBindingModel &bindingModel,
                            ResourceBindingDesc &binding, std::string &error) {
  if (bindingModel.bind == "auto") {
    binding.Auto(bindingModel.space);
    return true;
  }

  unsigned bindPoint = 0;
  if (!ParseRecipeUnsignedValue(bindingModel.bind, bindPoint, error))
    return false;

  binding.Register(bindPoint, bindingModel.space);
  return true;
}

static bool
ParseYamlRecipeOperandModel(const YamlRecipeOperandModel &operandModel,
                            DxilOperandPattern &operandPattern,
                            std::string &error) {
  operandPattern = DxilOperandPattern();
  operandPattern.operandIndex = operandModel.index;
  operandPattern.captureName = operandModel.capture;

  const std::string loweredKind = LowercaseRecipeToken(operandModel.kind);
  if (loweredKind == "any") {
    operandPattern.kind = DxilOperandPatternKind::Any;
  } else if (loweredKind == "constant_int") {
    operandPattern.kind = DxilOperandPatternKind::ConstantInt;
    operandPattern.constantIntValue = operandModel.value;
  } else if (loweredKind == "resource_handle") {
    operandPattern.kind = DxilOperandPatternKind::ResourceHandle;
  } else if (loweredKind == "instruction") {
    operandPattern.kind = DxilOperandPatternKind::Instruction;
    if (operandModel.opcode.empty() ||
        !ParseRecipeInstructionOpcode(
            operandModel.opcode, operandPattern.instructionOpcode, error)) {
      if (error.empty())
        error = "instruction operands require opcode";
      return false;
    }
  } else if (loweredKind == "dxop") {
    operandPattern.kind = DxilOperandPatternKind::DxOpCall;
    operandPattern.matchDxilOpCode = true;
    if (operandModel.opcode.empty() ||
        !ParseRecipeOpCode(operandModel.opcode, operandPattern.dxilOpCode,
                           error)) {
      if (error.empty())
        error = "dxop operands require opcode";
      return false;
    }
  } else {
    error = "unsupported operand kind '" + operandModel.kind + "'";
    return false;
  }

  if (!operandModel.resource_class.empty()) {
    if (!ParseRecipeResourceClass(operandModel.resource_class,
                                  operandPattern.resourceClass, error)) {
      return false;
    }
    operandPattern.matchResourceClass = true;
  }

  if (!operandModel.resource_kind.empty()) {
    if (!ParseRecipeResourceKind(operandModel.resource_kind,
                                 operandPattern.resourceKind, error)) {
      return false;
    }
    operandPattern.matchResourceKind = true;
  }

  operandPattern.resourceName = operandModel.resource_name;
  operandPattern.resourceNameLikePattern = operandModel.resource_name_like;
  if (!operandPattern.resourceNameLikePattern.empty()) {
    std::string regexError;
    llvm::Regex resourceNameRegex(operandPattern.resourceNameLikePattern);
    if (!resourceNameRegex.isValid(regexError)) {
      error = "invalid resource_name_like regex '" +
              operandPattern.resourceNameLikePattern + "': " + regexError;
      return false;
    }
  }

  if (operandModel.bind >= 0)
    operandPattern.resourceBindPoint = operandModel.bind;
  if (operandModel.space >= 0)
    operandPattern.resourceSpace = operandModel.space;

  operandPattern.operandPatterns.clear();
  for (const YamlRecipeOperandModel &childModel : operandModel.operands) {
    DxilOperandPattern childPattern;
    if (!ParseYamlRecipeOperandModel(childModel, childPattern, error))
      return false;
    operandPattern.operandPatterns.push_back(std::move(childPattern));
  }

  SortOperandPatternTree(operandPattern.operandPatterns);
  return true;
}

static bool ParseYamlRecipeEmitOperandModel(
    const YamlRecipeEmitOperandModel &operandModel,
    const std::unordered_map<std::string, TextureResourceDesc> &parsedTextures,
    const std::unordered_map<std::string, TextureResourceDesc> &parsedUavs,
    const std::unordered_map<std::string, CBufferDesc> &parsedCBuffers,
    const std::unordered_map<std::string, SamplerDesc> &parsedSamplers,
    DxilRewriteEmitOperand &emitOperand, std::string &error) {
  emitOperand = DxilRewriteEmitOperand();
  emitOperand.operandIndex = operandModel.index;

  const std::string loweredKind = LowercaseRecipeToken(operandModel.kind);
  if (loweredKind == "capture") {
    if (operandModel.capture.empty()) {
      error = "capture emit operands require capture";
      return false;
    }
    emitOperand.kind = DxilRewriteEmitOperandKind::Capture;
    emitOperand.captureName = operandModel.capture;
  } else if (loweredKind == "temporary") {
    if (operandModel.id.empty()) {
      error = "temporary emit operands require id";
      return false;
    }
    emitOperand.kind = DxilRewriteEmitOperandKind::Temporary;
    emitOperand.temporaryName = operandModel.id;
  } else if (loweredKind == "constant_int") {
    emitOperand.kind = DxilRewriteEmitOperandKind::ConstantInt;
    emitOperand.constantIntValue = operandModel.value;
  } else if (loweredKind == "resource") {
    ParsedRecipeResourceRef resourceRef;
    TryResolveParsedRecipeResourceRef(operandModel.id, parsedTextures,
                                      parsedUavs, parsedCBuffers,
                                      parsedSamplers, resourceRef);
    if (!resourceRef.found) {
      error = "unknown resource id '" + operandModel.id + "'";
      return false;
    }
    emitOperand.kind = DxilRewriteEmitOperandKind::ResourceHandle;
    emitOperand.resourceName = resourceRef.resourceName;
    emitOperand.resourceBinding = resourceRef.binding;
  } else if (loweredKind == "undef") {
    emitOperand.kind = DxilRewriteEmitOperandKind::Undef;
  } else {
    error = "unsupported emit operand kind '" + operandModel.kind + "'";
    return false;
  }

  return true;
}

static bool ParseDxilRecipeTextAsYaml(llvm::StringRef recipeText,
                                      DxilRecipeParseResult &result,
                                      llvm::StringRef sourceName) {
  result = DxilRecipeParseResult();
  YamlRecipeDocumentModel document;
  llvm::yaml::Input input(recipeText);
  input >> document;
  if (input.error()) {
    result.error = sourceName.str() + ": " + input.error().message();
    return false;
  }

  if (document.version != 1) {
    result.error = sourceName.str() + ": unsupported recipe schema version";
    return false;
  }

  result.patchOptions.restoreReflection = document.options.restore_reflection;

  std::unordered_map<std::string, DxilCallPattern> parsedPrefilters;
  std::unordered_map<std::string, DxilRewriteRule> parsedRewriteRules;
  std::unordered_map<std::string, TextureResourceDesc> parsedTextures;
  std::unordered_map<std::string, TextureResourceDesc> parsedUavs;
  std::unordered_map<std::string, CBufferDesc> parsedCBuffers;
  std::unordered_map<std::string, SamplerDesc> parsedSamplers;
  std::string parseError;

  auto buildRecipeCallPattern =
      [&](llvm::StringRef owningKind, llvm::StringRef owningId,
          const std::string &opcodeText, const std::string &captureName,
          const std::vector<YamlRecipeOperandModel> &operandModels,
          DxilCallPattern &pattern) -> bool {
    hlsl::OP::OpCode rootOpcode = static_cast<hlsl::OP::OpCode>(0);
    if (!ParseRecipeOpCode(opcodeText, rootOpcode, parseError)) {
      result.error = sourceName.str() + ": invalid " + owningKind.str() +
                     " opcode for '" + owningId.str() + "': " + parseError;
      return false;
    }

    std::vector<DxilOperandPattern> rootOperands;
    rootOperands.push_back(
        ConstantIntOperand(0, static_cast<uint64_t>(rootOpcode)).Build());
    for (const YamlRecipeOperandModel &operandModel : operandModels) {
      DxilOperandPattern operandPattern;
      if (!ParseYamlRecipeOperandModel(operandModel, operandPattern,
                                       parseError)) {
        result.error = sourceName.str() + ": invalid operand in " +
                       owningKind.str() + " '" + owningId.str() +
                       "': " + parseError;
        return false;
      }
      rootOperands.push_back(std::move(operandPattern));
    }

    SortOperandPatternTree(rootOperands);
    pattern = DxOpCall(rootOpcode)
                  .Capture(captureName)
                  .Args(std::move(rootOperands))
                  .Build();
    return true;
  };

  auto parseTextureModel =
      [&](const YamlRecipeTextureModel &textureModel, bool isUav,
          std::unordered_map<std::string, TextureResourceDesc> &outMap)
      -> bool {
    TextureResourceDesc desc;
    desc.name = textureModel.name;
    if (!ParseRecipeResourceKind(textureModel.kind, desc.kind, parseError) ||
        !ParseRecipeComponentType(textureModel.element, desc.elementKind,
                                  parseError)) {
      return false;
    }
    desc.vectorWidth = textureModel.width;
    if (isUav) {
      desc.binding.AsUAV();
      desc.isReadWrite = true;
    } else {
      desc.binding.AsSRV();
      desc.isReadWrite = false;
    }
    if (!ParseYamlRecipeBindingModel(textureModel.binding, desc.binding,
                                     parseError))
      return false;
    return outMap.emplace(textureModel.id, std::move(desc)).second;
  };

  for (const YamlRecipeTextureModel &textureModel :
       document.resources.textures) {
    if (!parseTextureModel(textureModel, false, parsedTextures)) {
      result.error = sourceName.str() + ": invalid texture resource '" +
                     textureModel.id + "': " + parseError;
      return false;
    }
  }
  for (const YamlRecipeTextureModel &textureModel :
       document.resources.texture_uavs) {
    if (!parseTextureModel(textureModel, true, parsedUavs)) {
      result.error = sourceName.str() + ": invalid texture_uav resource '" +
                     textureModel.id + "': " + parseError;
      return false;
    }
  }

  for (const YamlRecipeCBufferModel &cbufferModel :
       document.resources.cbuffers) {
    CBufferDesc desc;
    CBufferSchema *schema = new CBufferSchema();
    schema->typeName = cbufferModel.type;
    schema->sizeInBytes = cbufferModel.size;
    for (const YamlRecipeFieldModel &fieldModel : cbufferModel.fields) {
      CBufferFieldDesc field;
      field.name = fieldModel.name;
      if (!ParseRecipeCompTypeKind(fieldModel.type, field.compType,
                                   parseError)) {
        delete schema;
        result.error = sourceName.str() + ": invalid cbuffer field '" +
                       fieldModel.name + "': " + parseError;
        return false;
      }
      field.vectorSize = fieldModel.width;
      field.offset = fieldModel.offset;
      schema->fields.push_back(std::move(field));
    }
    desc.name = cbufferModel.name;
    desc.binding.AsCBuffer();
    if (!ParseYamlRecipeBindingModel(cbufferModel.binding, desc.binding,
                                     parseError)) {
      delete schema;
      result.error = sourceName.str() + ": invalid cbuffer binding for '" +
                     cbufferModel.id + "': " + parseError;
      return false;
    }
    desc.sizeInBytes = schema->sizeInBytes;
    desc.schema = schema;
    if (!parsedCBuffers.emplace(cbufferModel.id, std::move(desc)).second) {
      delete schema;
      result.error =
          sourceName.str() + ": duplicate cbuffer id '" + cbufferModel.id + "'";
      return false;
    }
  }

  for (const YamlRecipeSamplerModel &samplerModel :
       document.resources.samplers) {
    SamplerDesc desc;
    desc.name = samplerModel.name;
    if (!ParseYamlRecipeBindingModel(samplerModel.binding, desc.binding,
                                     parseError)) {
      result.error = sourceName.str() + ": invalid sampler binding for '" +
                     samplerModel.id + "': " + parseError;
      return false;
    }
    if (!parsedSamplers.emplace(samplerModel.id, std::move(desc)).second) {
      result.error =
          sourceName.str() + ": duplicate sampler id '" + samplerModel.id + "'";
      return false;
    }
  }

  for (const YamlRecipePrefilterModel &prefilterModel : document.prefilters) {
    DxilCallPattern pattern;
    const std::string captureName = prefilterModel.capture.empty()
                                        ? prefilterModel.id
                                        : prefilterModel.capture;
    if (!buildRecipeCallPattern("prefilter", prefilterModel.id,
                                prefilterModel.opcode, captureName,
                                prefilterModel.operands, pattern)) {
      return false;
    }

    if (!parsedPrefilters.emplace(prefilterModel.id, std::move(pattern))
             .second) {
      result.error = sourceName.str() + ": duplicate prefilter id '" +
                     prefilterModel.id + "'";
      return false;
    }
  }

  for (const YamlRecipeRuleModel &ruleModel : document.rewrite_rules) {
    DxilRewriteRule rule;
    rule.name = ruleModel.name.empty() ? ruleModel.id : ruleModel.name;
    rule.replaceCaptureName = ruleModel.match.replace;
    rule.replacementCaptureName = ruleModel.replace_with_capture;
    rule.pruneDeadInstructions = ruleModel.match.prune_dead;
    rule.pruneCaptureNames = ruleModel.match.prune_captures;
    if (!ParseRecipeRewriteMode(ruleModel.match.mode, rule.mode, parseError)) {
      result.error = sourceName.str() + ": invalid rewrite rule mode for '" +
                     ruleModel.id + "': " + parseError;
      return false;
    }

    const std::string rootCaptureName = ruleModel.match.capture.empty()
                                            ? rule.replaceCaptureName
                                            : ruleModel.match.capture;
    if (!buildRecipeCallPattern("rewrite rule", ruleModel.id,
                                ruleModel.match.opcode, rootCaptureName,
                                ruleModel.match.operands, rule.pattern)) {
      return false;
    }

    const hlsl::OP::OpCode rootOpcode = rule.pattern.dxilOpCode;

    for (const YamlRecipeBindingPatternModel &bindingModel :
         ruleModel.bindings) {
      if (LowercaseRecipeToken(bindingModel.kind) != "dxop") {
        result.error = sourceName.str() + ": unsupported binding kind '" +
                       bindingModel.kind + "'";
        return false;
      }

      hlsl::OP::OpCode bindingOpcode = static_cast<hlsl::OP::OpCode>(0);
      if (!ParseRecipeOpCode(bindingModel.opcode, bindingOpcode, parseError)) {
        result.error = sourceName.str() +
                       ": invalid binding opcode for rule '" + ruleModel.id +
                       "': " + parseError;
        return false;
      }

      DxilCallPattern bindingPattern =
          DxOpCall(bindingOpcode).Capture(bindingModel.capture).Build();
      bindingPattern.operandPatterns.push_back(
          ConstantIntOperand(0, static_cast<uint64_t>(bindingOpcode)).Build());
      for (const YamlRecipeOperandModel &operandModel : bindingModel.operands) {
        DxilOperandPattern operandPattern;
        if (!ParseYamlRecipeOperandModel(operandModel, operandPattern,
                                         parseError)) {
          result.error = sourceName.str() +
                         ": invalid binding operand in rule '" + ruleModel.id +
                         "': " + parseError;
          return false;
        }
        bindingPattern.operandPatterns.push_back(std::move(operandPattern));
      }
      SortOperandPatternTree(bindingPattern.operandPatterns);
      rule.bindingPatterns.push_back(std::move(bindingPattern));
    }

    for (const YamlRecipeEmitModel &emitModel : ruleModel.emit) {
      const std::string loweredKind = LowercaseRecipeToken(emitModel.kind);
      if (loweredKind == "create_handle") {
        ParsedRecipeResourceRef resourceRef;
        TryResolveParsedRecipeResourceRef(emitModel.resource, parsedTextures,
                                          parsedUavs, parsedCBuffers,
                                          parsedSamplers, resourceRef);
        if (!resourceRef.found) {
          result.error = sourceName.str() + ": unknown resource id '" +
                         emitModel.resource + "'";
          return false;
        }
        rule.emittedSequence.values.push_back(EmitCreateHandleValue(
            emitModel.id, resourceRef.resourceName, resourceRef.binding));
      } else if (loweredKind == "annotate_handle") {
        ParsedRecipeResourceRef resourceRef;
        TryResolveParsedRecipeResourceRef(emitModel.resource, parsedTextures,
                                          parsedUavs, parsedCBuffers,
                                          parsedSamplers, resourceRef);
        if (!resourceRef.found) {
          result.error = sourceName.str() + ": unknown resource id '" +
                         emitModel.resource + "'";
          return false;
        }
        rule.emittedSequence.values.push_back(EmitAnnotateHandleValue(
            emitModel.id, emitModel.handle, resourceRef.resourceName,
            resourceRef.binding));
      } else if (loweredKind == "call") {
        DxilRewriteEmitValue emittedValue;
        emittedValue.name = emitModel.id;
        emittedValue.kind = DxilRewriteEmitValueKind::DxOpCall;
        if (!ParseRecipeOpCode(emitModel.opcode, emittedValue.dxilOpCode,
                               parseError)) {
          result.error = sourceName.str() +
                         ": invalid emit call opcode in rule '" + ruleModel.id +
                         "': " + parseError;
          return false;
        }
        if (!emitModel.type.empty()) {
          if (!ParseRecipeComponentType(emitModel.type,
                                        emittedValue.resultComponentType,
                                        parseError)) {
            result.error = sourceName.str() +
                           ": invalid emit call type in rule '" + ruleModel.id +
                           "': " + parseError;
            return false;
          }
          emittedValue.hasExplicitResultComponentType = true;
        }
        for (const YamlRecipeEmitOperandModel &operandModel :
             emitModel.operands) {
          DxilRewriteEmitOperand emitOperand;
          if (!ParseYamlRecipeEmitOperandModel(
                  operandModel, parsedTextures, parsedUavs, parsedCBuffers,
                  parsedSamplers, emitOperand, parseError)) {
            result.error = sourceName.str() +
                           ": invalid emit operand in rule '" + ruleModel.id +
                           "': " + parseError;
            return false;
          }
          emittedValue.operands.push_back(std::move(emitOperand));
        }
        rule.emittedSequence.values.push_back(std::move(emittedValue));
      } else if (loweredKind == "extract") {
        rule.emittedSequence.values.push_back(EmitExtractValue(
            emitModel.id, emitModel.aggregate, emitModel.index));
      } else if (loweredKind == "binop") {
        unsigned instructionOpcode = 0;
        hlsl::DXIL::ComponentType componentType =
            hlsl::DXIL::ComponentType::Invalid;
        if (!ParseRecipeBinaryInstructionOpcode(
                emitModel.opcode, instructionOpcode, parseError) ||
            !ParseRecipeComponentType(emitModel.type, componentType,
                                      parseError)) {
          result.error = sourceName.str() + ": invalid binop emit in rule '" +
                         ruleModel.id + "': " + parseError;
          return false;
        }
        DxilRewriteEmitValue emittedValue = EmitBinaryInstructionValue(
            emitModel.id, instructionOpcode, componentType, {});
        for (const YamlRecipeEmitOperandModel &operandModel :
             emitModel.operands) {
          DxilRewriteEmitOperand emitOperand;
          if (!ParseYamlRecipeEmitOperandModel(
                  operandModel, parsedTextures, parsedUavs, parsedCBuffers,
                  parsedSamplers, emitOperand, parseError)) {
            result.error = sourceName.str() +
                           ": invalid binop operand in rule '" + ruleModel.id +
                           "': " + parseError;
            return false;
          }
          emittedValue.operands.push_back(std::move(emitOperand));
        }
        rule.emittedSequence.values.push_back(std::move(emittedValue));
      } else if (loweredKind == "cast") {
        unsigned castOpcode = 0;
        hlsl::DXIL::ComponentType componentType =
            hlsl::DXIL::ComponentType::Invalid;
        if (!ParseRecipeCastInstructionOpcode(emitModel.opcode, castOpcode,
                                              parseError) ||
            !ParseRecipeComponentType(emitModel.type, componentType,
                                      parseError)) {
          result.error = sourceName.str() + ": invalid cast emit in rule '" +
                         ruleModel.id + "': " + parseError;
          return false;
        }
        DxilRewriteEmitValue emittedValue = EmitCastInstructionValue(
            emitModel.id, castOpcode, componentType, {});
        for (const YamlRecipeEmitOperandModel &operandModel :
             emitModel.operands) {
          DxilRewriteEmitOperand emitOperand;
          if (!ParseYamlRecipeEmitOperandModel(
                  operandModel, parsedTextures, parsedUavs, parsedCBuffers,
                  parsedSamplers, emitOperand, parseError)) {
            result.error = sourceName.str() +
                           ": invalid cast operand in rule '" + ruleModel.id +
                           "': " + parseError;
            return false;
          }
          emittedValue.operands.push_back(std::move(emitOperand));
        }
        rule.emittedSequence.values.push_back(std::move(emittedValue));
      } else {
        result.error = sourceName.str() + ": unsupported emit kind '" +
                       emitModel.kind + "'";
        return false;
      }
    }

    rule.emittedSequence.replacementValueName = ruleModel.replace_with;
    const bool hasReplacementCapture = !rule.replacementCaptureName.empty();
    const bool hasReplacementValue =
        !rule.emittedSequence.replacementValueName.empty();
    const bool isMatchOnlyMode = rule.mode == DxilRewriteMode::None;
    if (hasReplacementCapture && hasReplacementValue) {
      result.error =
          sourceName.str() + ": rewrite rule '" + ruleModel.id +
          "' must provide exactly one of replace_with or replace_with_capture";
      return false;
    }

    if (isMatchOnlyMode) {
      if (!rule.replaceCaptureName.empty() || !ruleModel.emit.empty() ||
          hasReplacementCapture || hasReplacementValue ||
          !rule.pruneCaptureNames.empty()) {
        result.error =
            sourceName.str() + ": rewrite rule '" + ruleModel.id +
            "' with mode None must not define replace, emit, replace_with, "
            "replace_with_capture, or prune_captures";
        return false;
      }
    } else if (!hasReplacementCapture && !hasReplacementValue &&
               ruleModel.emit.empty()) {
      result.error = sourceName.str() + ": rewrite rule '" + ruleModel.id +
                     "' without rewrite payload must use mode None";
      return false;
    } else if (!hasReplacementCapture && !hasReplacementValue &&
               !ruleModel.emit.empty()) {
      result.error = sourceName.str() + ": rewrite rule '" + ruleModel.id +
                     "' with emit values must provide replace_with or "
                     "replace_with_capture";
      return false;
    }

    if (!parsedRewriteRules.emplace(ruleModel.id, std::move(rule)).second) {
      result.error = sourceName.str() + ": duplicate rewrite rule id '" +
                     ruleModel.id + "'";
      return false;
    }
  }

  for (const YamlRecipeStepModel &stepModel : document.steps) {
    const std::string loweredKind = LowercaseRecipeToken(stepModel.kind);
    if (loweredKind == "add_texture") {
      auto it = parsedTextures.find(stepModel.id);
      if (it == parsedTextures.end()) {
        result.error =
            sourceName.str() + ": unknown texture id '" + stepModel.id + "'";
        return false;
      }
      result.recipe.AddStep(MakeAddTextureStep(stepModel.id, it->second));
    } else if (loweredKind == "add_texture_uav") {
      auto it = parsedUavs.find(stepModel.id);
      if (it == parsedUavs.end()) {
        result.error = sourceName.str() + ": unknown texture_uav id '" +
                       stepModel.id + "'";
        return false;
      }
      result.recipe.AddStep(MakeAddTextureUAVStep(stepModel.id, it->second));
    } else if (loweredKind == "add_cbuffer") {
      auto it = parsedCBuffers.find(stepModel.id);
      if (it == parsedCBuffers.end()) {
        result.error =
            sourceName.str() + ": unknown cbuffer id '" + stepModel.id + "'";
        return false;
      }
      result.recipe.AddStep(MakeAddCBufferStep(stepModel.id, it->second));
    } else if (loweredKind == "add_sampler") {
      auto it = parsedSamplers.find(stepModel.id);
      if (it == parsedSamplers.end()) {
        result.error =
            sourceName.str() + ": unknown sampler id '" + stepModel.id + "'";
        return false;
      }
      result.recipe.AddStep(MakeAddSamplerStep(stepModel.id, it->second));
    } else if (loweredKind == "apply_rule") {
      auto it = parsedRewriteRules.find(stepModel.rule);
      if (it == parsedRewriteRules.end()) {
        result.error = sourceName.str() + ": unknown rewrite rule '" +
                       stepModel.rule + "'";
        return false;
      }

      DxilRecipeRuleApplicationMode applicationMode =
          DxilRecipeRuleApplicationMode::First;
      const std::string modeText =
          stepModel.mode.empty() ? "First" : stepModel.mode;
      if (!ParseRecipeRuleApplicationMode(modeText, applicationMode,
                                          parseError)) {
        result.error = sourceName.str() + ": invalid apply_rule mode for '" +
                       stepModel.rule + "': " + parseError;
        return false;
      }

      const std::string stepName = stepModel.name.empty()
                                       ? ("apply_rule:" + stepModel.rule)
                                       : stepModel.name;
      result.recipe.AddStep(MakeApplyRewriteRulesStep(
          stepName, {it->second}, applicationMode, stepModel.required));
    } else if (loweredKind == "apply_rules") {
      if (stepModel.rules.empty()) {
        result.error =
            sourceName.str() + ": apply_rules requires a non-empty rules list";
        return false;
      }

      std::vector<DxilRewriteRule> rules;
      rules.reserve(stepModel.rules.size());
      for (const std::string &ruleId : stepModel.rules) {
        auto it = parsedRewriteRules.find(ruleId);
        if (it == parsedRewriteRules.end()) {
          result.error =
              sourceName.str() + ": unknown rewrite rule '" + ruleId + "'";
          return false;
        }
        rules.push_back(it->second);
      }

      DxilRecipeRuleApplicationMode applicationMode =
          DxilRecipeRuleApplicationMode::MatchAll;
      const std::string modeText =
          stepModel.mode.empty() ? "MatchAll" : stepModel.mode;
      if (!ParseRecipeRuleApplicationMode(modeText, applicationMode,
                                          parseError)) {
        result.error =
            sourceName.str() + ": invalid apply_rules mode: " + parseError;
        return false;
      }

      if (applicationMode != DxilRecipeRuleApplicationMode::MatchAll) {
        result.error =
            sourceName.str() + ": apply_rules only supports MatchAll mode";
        return false;
      }

      const std::string stepName =
          stepModel.name.empty() ? "apply_rules" : stepModel.name;
      result.recipe.AddStep(MakeApplyRewriteRulesStep(
          stepName, std::move(rules), applicationMode, stepModel.required));
    } else if (loweredKind == "prefilter") {
      const bool hasPattern = !stepModel.pattern.empty();
      const bool hasPatterns = !stepModel.patterns.empty();
      if (hasPattern == hasPatterns) {
        result.error =
            sourceName.str() +
            ": prefilter requires exactly one of pattern or patterns";
        return false;
      }

      std::vector<DxilCallPattern> patterns;
      if (hasPattern) {
        auto it = parsedPrefilters.find(stepModel.pattern);
        if (it == parsedPrefilters.end()) {
          result.error = sourceName.str() + ": unknown prefilter pattern '" +
                         stepModel.pattern + "'";
          return false;
        }
        patterns.push_back(it->second);
      } else {
        patterns.reserve(stepModel.patterns.size());
        for (const std::string &patternId : stepModel.patterns) {
          auto it = parsedPrefilters.find(patternId);
          if (it == parsedPrefilters.end()) {
            result.error = sourceName.str() + ": unknown prefilter pattern '" +
                           patternId + "'";
            return false;
          }
          patterns.push_back(it->second);
        }
      }

      const std::string stepName =
          stepModel.name.empty()
              ? (hasPattern ? ("prefilter:" + stepModel.pattern) : "prefilter")
              : stepModel.name;
      result.recipe.AddStep(MakePrefilterStep(stepName, std::move(patterns)));
    } else if (loweredKind == "refresh_resources") {
      result.recipe.AddStep(MakeRefreshResourcesStep());
    } else if (loweredKind == "prune_dead_code") {
      result.recipe.AddStep(MakePruneDeadCodeStep());
    } else {
      result.error =
          sourceName.str() + ": unsupported step kind '" + stepModel.kind + "'";
      return false;
    }
  }

  return true;
}

} // namespace

bool ParseDxilRecipeText(llvm::StringRef recipeText,
                         DxilRecipeParseResult &result,
                         llvm::StringRef sourceName) {
  return ParseDxilRecipeTextAsYaml(recipeText, result, sourceName);
}

bool ParseDxilRecipeFile(const std::string &recipePath,
                         DxilRecipeParseResult &result) {
  std::ifstream file(recipePath);
  if (!file) {
    result = DxilRecipeParseResult();
    result.error = "failed to open recipe file '" + recipePath + "'";
    return false;
  }

  std::string recipeText((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
  return ParseDxilRecipeText(recipeText, result, recipePath);
}