#include "../../../include/dxp/sm6/RecipeParse.h"

#include "../../../include/dxp/sm6/Resources.h"
#include "../../../include/dxp/sm6/Transforms.h"

#include "YamlSchema.h"

#include <glaze/yaml.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/Regex.h"

#include "dxc/DXIL/DxilCompType.h"
#include "dxc/DXIL/DxilConstants.h"
#include "dxc/DXIL/DxilOperations.h"

namespace {

static llvm::StringRef MakeYamlError(llvm::Twine msg) {
  thread_local std::string buffer;
  buffer = msg.str();
  return llvm::StringRef(buffer);
}

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
      {"first", DxilRecipeRuleApplicationMode::First},
      {"last", DxilRecipeRuleApplicationMode::Last},
      {"match_all", DxilRecipeRuleApplicationMode::MatchAll},
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
          {"texture_2d_array", hlsl::DXIL::ResourceKind::Texture2DArray},
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

// Strip UTF-8 BOM (EF BB BF) if present
static std::string StripBom(std::string text) {
  if (text.size() >= 3 &&
      static_cast<uint8_t>(text[0]) == 0xEF &&
      static_cast<uint8_t>(text[1]) == 0xBB &&
      static_cast<uint8_t>(text[2]) == 0xBF) {
    text.erase(0, 3);
  }
  return text;
}

} // namespace

using namespace dxp::sm6;

namespace {

static bool BuildStepCondition(const YamlRecipeStepConditionModel &conditionModel,
                               DxilRecipeStepCondition &condition,
                               std::string &error) {
  condition = DxilRecipeStepCondition{};
  condition.negate = conditionModel.not_condition;

  size_t populatedFields = 0;
  if (!conditionModel.state.empty()) {
    ++populatedFields;
  }
  if (!conditionModel.all.empty()) {
    ++populatedFields;
  }
  if (!conditionModel.any.empty()) {
    ++populatedFields;
  }

  if (populatedFields == 0) {
    return true;
  }
  if (populatedFields != 1) {
    error = "step if must specify exactly one of state, all, or any";
    return false;
  }

  if (!conditionModel.state.empty()) {
    condition.state = conditionModel.state;
    return true;
  }

  std::vector<DxilRecipeStepCondition> &destination =
      !conditionModel.all.empty() ? condition.all : condition.any;
  const std::vector<YamlRecipeStepConditionModel> &source =
      !conditionModel.all.empty() ? conditionModel.all : conditionModel.any;
  destination.reserve(source.size());
  for (const YamlRecipeStepConditionModel &childModel : source) {
    DxilRecipeStepCondition childCondition;
    if (!BuildStepCondition(childModel, childCondition, error)) {
      return false;
    }
    if (!childCondition.IsSet()) {
      error = "nested step if condition must not be empty";
      return false;
    }
    destination.push_back(std::move(childCondition));
  }

  return true;
}

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
  {
    auto loweredKind = llvm::StringRef(operandModel.kind).lower();
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
  {
    auto loweredKind = llvm::StringRef(operandModel.kind).lower();
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
      emitOperand.kind = DxilRewriteEmitOperandKind::ResourceHandle;
      ParsedRecipeResourceRef resourceRef;
      TryResolveParsedRecipeResourceRef(operandModel.id, parsedTextures,
                                        parsedUavs, parsedCBuffers,
                                        parsedSamplers, resourceRef);
      if (!resourceRef.found) {
        error = "unknown resource id '" + operandModel.id + "'";
        return false;
      }
      emitOperand.resourceName = resourceRef.resourceName;
      emitOperand.resourceBinding = resourceRef.binding;
    } else if (loweredKind == "undef") {
      emitOperand.kind = DxilRewriteEmitOperandKind::Undef;
    } else {
      error = "unsupported emit operand kind '" + operandModel.kind + "'";
      return false;
    }
  }

  return true;
}

static void SetYamlParseError(::dxp::ParseError &error,
                              const glz::error_ctx &ec,
                              const std::string &text,
                              const std::string &sourceName) {
  // glaze v7.7.1 error_ctx does not expose line/column/path directly;
  // format_error embeds them in the message string.
  error.message = sourceName + ": " + glz::format_error(ec, text);
}

static void SetParseError(::dxp::ParseError &error,
                          const std::string &sourceName,
                          const std::string &message) {
  error.message = sourceName + ": " + message;
}

static bool ParseDxilRecipeTextAsYaml(llvm::StringRef recipeText,
                                      DxilRecipeParseResult &result,
                                      llvm::StringRef sourceName) {
  result = DxilRecipeParseResult();
  YamlRecipeDocumentModel document;
  std::string text = StripBom(std::string(recipeText));
  auto ec = glz::read_yaml(document, text);
  if (ec) {
    SetYamlParseError(result.yaml_diagnostic, ec, text, sourceName.str());
    return false;
  }

  if (document.version != 1) {
    result.yaml_diagnostic.message = "unsupported recipe schema version";
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
      result.yaml_diagnostic.message = "invalid " + owningKind.str() +
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
        result.yaml_diagnostic.message = "invalid operand in " +
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
    if (!ParseRecipeResourceKind(textureModel.kind, desc.kind, parseError)) {
      return false;
    }
    if (!ParseRecipeComponentType(textureModel.element, desc.elementKind,
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
      result.yaml_diagnostic.message = "invalid texture resource '" +
                     textureModel.id + "': " + parseError;
      return false;
    }
  }
  for (const YamlRecipeTextureModel &textureModel :
       document.resources.texture_uavs) {
    if (!parseTextureModel(textureModel, true, parsedUavs)) {
      result.yaml_diagnostic.message = "invalid texture_uav resource '" +
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
        result.yaml_diagnostic.message = "invalid cbuffer field '" +
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
      result.yaml_diagnostic.message = "invalid cbuffer binding for '" +
                     cbufferModel.id + "': " + parseError;
      return false;
    }
    desc.sizeInBytes = schema->sizeInBytes;
    desc.schema = schema;
    if (!parsedCBuffers.emplace(cbufferModel.id, std::move(desc)).second) {
      delete schema;
      result.yaml_diagnostic.message =
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
      result.yaml_diagnostic.message = "invalid sampler binding for '" +
                     samplerModel.id + "': " + parseError;
      return false;
    }
    if (!parsedSamplers.emplace(samplerModel.id, std::move(desc)).second) {
      result.yaml_diagnostic.message =
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
      result.yaml_diagnostic.message = "duplicate prefilter id '" +
                     prefilterModel.id + "'";
      return false;
    }
  }

  for (const YamlRecipeRuleModel &ruleModel : document.rewrite_rules) {
    DxilRewriteRule rule;
    rule.name = ruleModel.name.empty() ? ruleModel.id : ruleModel.name;
    rule.replaceCaptureName = ruleModel.match.replace;
    rule.rangeStartOffset = ruleModel.match.range_start_offset;
    rule.rangeEndOffset = ruleModel.match.range_end_offset;
    rule.replacementCaptureName = ruleModel.replace_with_capture;
    rule.pruneDeadInstructions = ruleModel.match.prune_dead;
    rule.pruneCaptureNames = ruleModel.match.prune_captures;
    if (!ParseRecipeRewriteMode(ruleModel.match.mode, rule.mode, parseError)) {
      result.yaml_diagnostic.message = "invalid rewrite rule mode for '" +
                     ruleModel.id + "': " + parseError;
      return false;
    }

    const std::string rootCaptureName = ruleModel.match.capture;
    if (!buildRecipeCallPattern("rewrite rule", ruleModel.id,
                                ruleModel.match.opcode, rootCaptureName,
                                ruleModel.match.operands, rule.pattern)) {
      return false;
    }

    const hlsl::OP::OpCode rootOpcode = rule.pattern.dxilOpCode;

    for (const YamlRecipeBindingPatternModel &bindingModel :
         ruleModel.bindings) {
      if (LowercaseRecipeToken(bindingModel.kind) != "dxop") {
        result.yaml_diagnostic.message = "unsupported binding kind '" +
                       bindingModel.kind + "'";
        return false;
      }

      hlsl::OP::OpCode bindingOpcode = static_cast<hlsl::OP::OpCode>(0);
      if (!ParseRecipeOpCode(bindingModel.opcode, bindingOpcode, parseError)) {
        result.yaml_diagnostic.message = 
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
          result.yaml_diagnostic.message = 
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
          result.yaml_diagnostic.message = "unknown resource id '" +
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
          result.yaml_diagnostic.message = "unknown resource id '" +
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
          result.yaml_diagnostic.message = 
                         ": invalid emit call opcode in rule '" + ruleModel.id +
                         "': " + parseError;
          return false;
        }
        if (!emitModel.type.empty()) {
          if (!ParseRecipeComponentType(emitModel.type,
                                        emittedValue.resultComponentType,
                                        parseError)) {
            result.yaml_diagnostic.message = 
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
            result.yaml_diagnostic.message = 
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
          result.yaml_diagnostic.message = "invalid binop emit in rule '" +
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
            result.yaml_diagnostic.message = 
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
          result.yaml_diagnostic.message = "invalid cast emit in rule '" +
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
            result.yaml_diagnostic.message = 
                           ": invalid cast operand in rule '" + ruleModel.id +
                           "': " + parseError;
            return false;
          }
          emittedValue.operands.push_back(std::move(emitOperand));
        }
        rule.emittedSequence.values.push_back(std::move(emittedValue));
      } else {
        result.yaml_diagnostic.message = "unsupported emit kind '" +
                       emitModel.kind + "'";
        return false;
      }
    }

    rule.emittedSequence.replacementValueName = ruleModel.replace_with;
    const bool hasReplacementCapture = !rule.replacementCaptureName.empty();
    const bool hasReplacementValue =
        !rule.emittedSequence.replacementValueName.empty();
    const bool isMatchOnlyMode = rule.mode == DxilRewriteMode::None;
    const bool hasCustomRangeOffsets =
        rule.rangeStartOffset != 0 || rule.rangeEndOffset != -1;
    if (hasReplacementCapture && hasReplacementValue) {
      result.yaml_diagnostic.message =
          sourceName.str() + ": rewrite rule '" + ruleModel.id +
          "' must provide exactly one of replace_with or replace_with_capture";
      return false;
    }

    if (rule.rangeStartOffset < 0) {
      result.yaml_diagnostic.message = "rewrite rule '" + ruleModel.id +
                     "' range_start_offset must be >= 0";
      return false;
    }
    if (rule.rangeEndOffset < -1) {
      result.yaml_diagnostic.message = "rewrite rule '" + ruleModel.id +
                     "' range_end_offset must be -1 or >= 0";
      return false;
    }
    if (rule.mode != DxilRewriteMode::ReplaceRange && hasCustomRangeOffsets) {
      result.yaml_diagnostic.message = "rewrite rule '" + ruleModel.id +
                     "' range offsets require mode ReplaceRange";
      return false;
    }
    if (!rule.replaceCaptureName.empty()) {
      result.yaml_diagnostic.message = "rewrite rule '" + ruleModel.id +
                     "' must not define replace; Replace rewrites the full "
                     "matched instruction and ReplaceRange uses "
                     "range_start_offset/range_end_offset within that match";
      return false;
    }

    if (isMatchOnlyMode) {
      if (!rule.replaceCaptureName.empty() || !ruleModel.emit.empty() ||
          hasReplacementCapture || hasReplacementValue ||
          !rule.pruneCaptureNames.empty() || hasCustomRangeOffsets) {
        result.yaml_diagnostic.message =
            sourceName.str() + ": rewrite rule '" + ruleModel.id +
            "' with mode None must not define replace, emit, replace_with, "
            "replace_with_capture, prune_captures, or range offsets";
        return false;
      }
    } else if (!hasReplacementCapture && !hasReplacementValue &&
               ruleModel.emit.empty()) {
      result.yaml_diagnostic.message = "rewrite rule '" + ruleModel.id +
                     "' without rewrite payload must use mode None";
      return false;
    } else if (!hasReplacementCapture && !hasReplacementValue &&
               !ruleModel.emit.empty()) {
      result.yaml_diagnostic.message = "rewrite rule '" + ruleModel.id +
                     "' with emit values must provide replace_with or "
                     "replace_with_capture";
      return false;
    }

    if (!parsedRewriteRules.emplace(ruleModel.id, std::move(rule)).second) {
      result.yaml_diagnostic.message = "duplicate rewrite rule id '" +
                     ruleModel.id + "'";
      return false;
    }
  }

  for (const YamlRecipeStepModel &stepModel : document.steps) {
    const std::string loweredKind = LowercaseRecipeToken(stepModel.kind);
    DxilRecipeStepCondition stepCondition;
    if (!BuildStepCondition(stepModel.if_condition, stepCondition,
                            parseError)) {
      result.yaml_diagnostic.message = "invalid step if condition: " +
                     parseError;
      return false;
    }

    if (loweredKind == "add_texture") {
      auto it = parsedTextures.find(stepModel.id);
      if (it == parsedTextures.end()) {
        result.yaml_diagnostic.message =
            sourceName.str() + ": unknown texture id '" + stepModel.id + "'";
        return false;
      }
      result.recipe.AddStep(MakeAddTextureStep(stepModel.id, it->second)
                .Require(stepModel.required)
                .When(stepCondition));
    } else if (loweredKind == "add_texture_uav") {
      auto it = parsedUavs.find(stepModel.id);
      if (it == parsedUavs.end()) {
        result.yaml_diagnostic.message = "unknown texture_uav id '" +
                       stepModel.id + "'";
        return false;
      }
      result.recipe.AddStep(MakeAddTextureUAVStep(stepModel.id, it->second)
                .Require(stepModel.required)
                .When(stepCondition));
    } else if (loweredKind == "add_cbuffer") {
      auto it = parsedCBuffers.find(stepModel.id);
      if (it == parsedCBuffers.end()) {
        result.yaml_diagnostic.message =
            sourceName.str() + ": unknown cbuffer id '" + stepModel.id + "'";
        return false;
      }
      result.recipe.AddStep(MakeAddCBufferStep(stepModel.id, it->second)
                .Require(stepModel.required)
                .When(stepCondition));
    } else if (loweredKind == "add_sampler") {
      auto it = parsedSamplers.find(stepModel.id);
      if (it == parsedSamplers.end()) {
        result.yaml_diagnostic.message =
            sourceName.str() + ": unknown sampler id '" + stepModel.id + "'";
        return false;
      }
      result.recipe.AddStep(MakeAddSamplerStep(stepModel.id, it->second)
                .Require(stepModel.required)
                .When(stepCondition));
    } else if (loweredKind == "apply_rule") {
      auto it = parsedRewriteRules.find(stepModel.rule);
      if (it == parsedRewriteRules.end()) {
        result.yaml_diagnostic.message = "unknown rewrite rule '" +
                       stepModel.rule + "'";
        return false;
      }

      DxilRecipeRuleApplicationMode applicationMode;
      if (!ParseRecipeRuleApplicationMode(stepModel.mode, applicationMode,
                                          parseError)) {
        result.yaml_diagnostic.message = "invalid apply_rule mode for '" +
                       stepModel.rule + "': " + parseError;
        return false;
      }

      const std::string stepName = stepModel.name.empty()
                                       ? ("apply_rule:" + stepModel.rule)
                                       : stepModel.name;
      result.recipe.AddStep(MakeApplyRewriteRulesStep(
                                stepName, {it->second}, applicationMode,
                                stepModel.required)
                                .When(stepCondition));
    } else if (loweredKind == "apply_rules") {
      if (stepModel.rules.empty()) {
        result.yaml_diagnostic.message =
            sourceName.str() + ": apply_rules requires a non-empty rules list";
        return false;
      }

      std::vector<DxilRewriteRule> rules;
      rules.reserve(stepModel.rules.size());
      for (const std::string &ruleId : stepModel.rules) {
        auto it = parsedRewriteRules.find(ruleId);
        if (it == parsedRewriteRules.end()) {
          result.yaml_diagnostic.message =
              sourceName.str() + ": unknown rewrite rule '" + ruleId + "'";
          return false;
        }
        rules.push_back(it->second);
      }

      DxilRecipeRuleApplicationMode applicationMode =
          DxilRecipeRuleApplicationMode::MatchAll;
      if (!ParseRecipeRuleApplicationMode(stepModel.mode, applicationMode,
                                          parseError)) {
        result.yaml_diagnostic.message =
            sourceName.str() + ": invalid apply_rules mode: " + parseError;
        return false;
      }

      if (applicationMode != DxilRecipeRuleApplicationMode::MatchAll) {
        result.yaml_diagnostic.message =
            sourceName.str() + ": apply_rules only supports match_all mode";
        return false;
      }

      const std::string stepName =
          stepModel.name.empty() ? "apply_rules" : stepModel.name;
      result.recipe.AddStep(MakeApplyRewriteRulesStep(
                                stepName, std::move(rules), applicationMode,
                                stepModel.required)
                                .When(stepCondition));
    } else if (loweredKind == "prefilter") {
      const bool hasPattern = !stepModel.pattern.empty();
      const bool hasPatterns = !stepModel.patterns.empty();
      if (hasPattern == hasPatterns) {
        result.yaml_diagnostic.message =
            sourceName.str() +
            ": prefilter requires exactly one of pattern or patterns";
        return false;
      }

      std::vector<DxilCallPattern> patterns;
      if (hasPattern) {
        auto it = parsedPrefilters.find(stepModel.pattern);
        if (it == parsedPrefilters.end()) {
          result.yaml_diagnostic.message = "unknown prefilter pattern '" +
                         stepModel.pattern + "'";
          return false;
        }
        patterns.push_back(it->second);
      } else {
        patterns.reserve(stepModel.patterns.size());
        for (const std::string &patternId : stepModel.patterns) {
          auto it = parsedPrefilters.find(patternId);
          if (it == parsedPrefilters.end()) {
            result.yaml_diagnostic.message = "unknown prefilter pattern '" +
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
        result.recipe.AddStep(MakePrefilterStep(stepName, std::move(patterns),
                            stepModel.set)
                    .Require(stepModel.required)
                    .When(stepCondition));
    } else if (loweredKind == "refresh_resources") {
      if (!stepModel.set.empty()) {
        result.yaml_diagnostic.message =
            sourceName.str() + ": step set is only valid for prefilter";
        return false;
      }
      result.recipe.AddStep(MakeRefreshResourcesStep()
                .Require(stepModel.required)
                .When(stepCondition));
    } else if (loweredKind == "prune_dead_code") {
      if (!stepModel.set.empty()) {
        result.yaml_diagnostic.message =
            sourceName.str() + ": step set is only valid for prefilter";
        return false;
      }
      result.recipe.AddStep(MakePruneDeadCodeStep()
                .Require(stepModel.required)
                .When(stepCondition));
    } else {
      result.yaml_diagnostic.message =
          sourceName.str() + ": unsupported step kind '" + stepModel.kind + "'";
      return false;
    }
  }

  return true;
}

} // namespace

bool ParseDxilRecipeText(const std::string &recipeText,
                         DxilRecipeParseResult &result,
                         const std::string &sourceName) {
  return ParseDxilRecipeTextAsYaml(
      llvm::StringRef(recipeText), result, llvm::StringRef(sourceName));
}

bool ParseDxilRecipeFile(const std::string &recipePath,
                         DxilRecipeParseResult &result) {
  std::ifstream file(recipePath);
  if (!file) {
    result = DxilRecipeParseResult();
    result.yaml_diagnostic.message = "failed to open recipe file '" + recipePath + "'";
    return false;
  }

  std::string recipeText((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
  return ParseDxilRecipeText(recipeText, result, recipePath);
}
