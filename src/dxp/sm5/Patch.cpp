#include "dxp/sm5/Patch.h"

#include "dxp/sm5/Container.h"
#include "dxp/sm5/Model.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/Serialize.h"

#include <cstring>
#include <cctype>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace dxp {
namespace sm5 {

namespace {

static uint32_t FloatAsUint(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

static PatchResult MakeError(const std::string &message,
                             const RecipeContext *context = nullptr) {
  PatchResult result;
  result.Success = false;
  result.Error = message;
  if (context != nullptr) {
    result.RecipeContext = *context;
  }
  return result;
}

static bool IsOpcode(const Instruction &instruction, OpcodeType opcode) {
  return static_cast<OpcodeType>(instruction.Opcode) == opcode;
}

static std::optional<D3D10_SB_4_COMPONENT_NAME> TryGetSingleReferencedComponent(
    const Operand &operand) {
  const auto selectionMode = static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(operand.ComponentMode));
  if (selectionMode == D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) {
    return static_cast<D3D10_SB_4_COMPONENT_NAME>(
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(operand.ComponentMode));
  }

  if (selectionMode != D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) {
    return std::nullopt;
  }

  const uint32_t mask = DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(operand.ComponentMode);
  if (mask == D3D10_SB_OPERAND_4_COMPONENT_MASK_X) {
    return D3D10_SB_4_COMPONENT_X;
  }
  if (mask == D3D10_SB_OPERAND_4_COMPONENT_MASK_Y) {
    return D3D10_SB_4_COMPONENT_Y;
  }
  if (mask == D3D10_SB_OPERAND_4_COMPONENT_MASK_Z) {
    return D3D10_SB_4_COMPONENT_Z;
  }
  if (mask == D3D10_SB_OPERAND_4_COMPONENT_MASK_W) {
    return D3D10_SB_4_COMPONENT_W;
  }

  return std::nullopt;
}

static bool SameSingleComponentTempRegister(const Operand &lhs,
                                            const Operand &rhs) {
  if (lhs.Type != D3D10_SB_OPERAND_TYPE_TEMP || rhs.Type != D3D10_SB_OPERAND_TYPE_TEMP ||
      lhs.Indices.empty() || rhs.Indices.empty() || lhs.Indices.front() != rhs.Indices.front()) {
    return false;
  }

  const auto lhsComponent = TryGetSingleReferencedComponent(lhs);
  const auto rhsComponent = TryGetSingleReferencedComponent(rhs);
  return lhsComponent.has_value() && rhsComponent.has_value() &&
         lhsComponent.value() == rhsComponent.value();
}

static uint32_t MakeMaskComponentMode(uint32_t mask) {
  return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
             D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
         ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(mask);
}

static uint32_t MakeSelectComponentMode(D3D10_SB_4_COMPONENT_NAME component) {
  return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
             D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) |
         ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(component);
}

static uint32_t MakeSwizzleComponentMode(D3D10_SB_4_COMPONENT_NAME x,
                                         D3D10_SB_4_COMPONENT_NAME y,
                                         D3D10_SB_4_COMPONENT_NAME z,
                                         D3D10_SB_4_COMPONENT_NAME w) {
  return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
             D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) |
         ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(x, y, z, w);
}

static Operand MakeTempOperand(uint32_t regIndex,
                               uint32_t componentMode) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_TEMP;
  operand.NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
  operand.ComponentMode = componentMode;
  operand.Indices = {regIndex};
  return operand;
}

static Operand MakeConstantBufferOperand(uint32_t bindPoint,
                                         uint32_t elementIndex,
                                         D3D10_SB_4_COMPONENT_NAME component) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER;
  operand.NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
  operand.ComponentMode = MakeSelectComponentMode(component);
  operand.Indices = {bindPoint, elementIndex};
  return operand;
}

static Operand MakeSamplerOperand(uint32_t bindPoint) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_SAMPLER;
  operand.NumComponents = D3D10_SB_OPERAND_0_COMPONENT;
  operand.ComponentMode = 0;
  operand.Indices = {bindPoint};
  return operand;
}

static Operand MakeResourceOperand(uint32_t bindPoint) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_RESOURCE;
  operand.NumComponents = D3D10_SB_OPERAND_0_COMPONENT;
  operand.ComponentMode = 0;
  operand.Indices = {bindPoint};
  return operand;
}

static Operand MakeImmediateFloatOperand(float value) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_IMMEDIATE32;
  operand.NumComponents = D3D10_SB_OPERAND_1_COMPONENT;
  operand.ComponentMode = 0;
  operand.ImmediateValues = {FloatAsUint(value)};
  return operand;
}

static Instruction FinalizeInstruction(Instruction instruction) {
  instruction.RawTokens = EncodeInstruction(instruction);
  instruction.LengthInDwords = static_cast<uint32_t>(instruction.RawTokens.size());
  return instruction;
}

struct SignatureParameter {
  std::string SemanticName;
  uint32_t SemanticIndex = 0;
  uint32_t SystemValueType = 0;
  uint32_t ComponentType = 0;
  uint32_t Register = 0;
  uint32_t MaskAndRw = 0;
};

static std::string ReadNullTerminatedString(const std::vector<uint8_t> &bytes,
                                            uint32_t offset) {
  if (offset >= bytes.size()) {
    return {};
  }

  const char *begin = reinterpret_cast<const char *>(bytes.data() + offset);
  size_t length = 0;
  while ((offset + length) < bytes.size() && begin[length] != '\0') {
    ++length;
  }

  return std::string(begin, length);
}

static void WriteU32(std::vector<uint8_t> &bytes,
                     size_t offset,
                     uint32_t value) {
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

static uint32_t ReadU32(const std::vector<uint8_t> &bytes,
                        size_t offset) {
  uint32_t value = 0;
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

static bool ParseSignatureChunk(const DxbcChunk &chunk,
                                std::vector<SignatureParameter> &parameters,
                                uint32_t &headerFlags,
                                std::string &error) {
  parameters.clear();
  headerFlags = 0;

  if (chunk.Data.size() < 8) {
    error = "signature chunk too small";
    return false;
  }

  const uint32_t parameterCount = ReadU32(chunk.Data, 0);
  headerFlags = ReadU32(chunk.Data, 4);

  const size_t tableBytes = static_cast<size_t>(parameterCount) * 24;
  if (8 + tableBytes > chunk.Data.size()) {
    error = "signature chunk table exceeds chunk size";
    return false;
  }

  parameters.reserve(parameterCount);
  for (uint32_t i = 0; i < parameterCount; ++i) {
    const size_t base = 8 + static_cast<size_t>(i) * 24;

    SignatureParameter parameter;
    const uint32_t nameOffset = ReadU32(chunk.Data, base + 0);
    parameter.SemanticName = ReadNullTerminatedString(chunk.Data, nameOffset);
    parameter.SemanticIndex = ReadU32(chunk.Data, base + 4);
    parameter.SystemValueType = ReadU32(chunk.Data, base + 8);
    parameter.ComponentType = ReadU32(chunk.Data, base + 12);
    parameter.Register = ReadU32(chunk.Data, base + 16);
    parameter.MaskAndRw = ReadU32(chunk.Data, base + 20);

    if (parameter.SemanticName.empty()) {
      error = "signature parameter has invalid semantic name offset";
      return false;
    }

    parameters.push_back(std::move(parameter));
  }

  return true;
}

static std::vector<uint8_t> BuildSignatureChunk(
    const std::vector<SignatureParameter> &parameters,
    uint32_t headerFlags) {
  std::vector<uint8_t> bytes;
  bytes.resize(8 + static_cast<size_t>(parameters.size()) * 24);

  WriteU32(bytes, 0, static_cast<uint32_t>(parameters.size()));
  WriteU32(bytes, 4, headerFlags);

  std::unordered_map<std::string, uint32_t> semanticOffsets;
  semanticOffsets.reserve(parameters.size());

  auto getOrCreateSemanticOffset = [&](const std::string &semanticName) {
    const auto it = semanticOffsets.find(semanticName);
    if (it != semanticOffsets.end()) {
      return it->second;
    }

    const uint32_t offset = static_cast<uint32_t>(bytes.size());
    bytes.insert(bytes.end(), semanticName.begin(), semanticName.end());
    bytes.push_back('\0');
    semanticOffsets.emplace(semanticName, offset);
    return offset;
  };

  for (size_t i = 0; i < parameters.size(); ++i) {
    const auto &parameter = parameters[i];
    const size_t base = 8 + i * 24;

    WriteU32(bytes, base + 0, getOrCreateSemanticOffset(parameter.SemanticName));
    WriteU32(bytes, base + 4, parameter.SemanticIndex);
    WriteU32(bytes, base + 8, parameter.SystemValueType);
    WriteU32(bytes, base + 12, parameter.ComponentType);
    WriteU32(bytes, base + 16, parameter.Register);
    WriteU32(bytes, base + 20, parameter.MaskAndRw & 0x0000FFFFu);
  }

  while ((bytes.size() & 3u) != 0u) {
    bytes.push_back(0);
  }

  return bytes;
}

static void InsertSignatureParameterByRegister(
    std::vector<SignatureParameter> &parameters,
    SignatureParameter parameter) {
  const auto insertIt = std::lower_bound(
      parameters.begin(), parameters.end(), parameter.Register,
      [](const SignatureParameter &lhs, uint32_t registerIndex) {
        return lhs.Register < registerIndex;
      });
  parameters.insert(insertIt, std::move(parameter));
}

static uint32_t WithMaskAndRw(uint32_t value,
                              uint8_t mask,
                              uint8_t rwMask) {
  const uint32_t preservedUpper = value & 0xFFFF0000u;
  return preservedUpper | static_cast<uint32_t>(mask) |
         (static_cast<uint32_t>(rwMask) << 8);
}

static uint8_t ComponentBit(D3D10_SB_4_COMPONENT_NAME component) {
  switch (component) {
    case D3D10_SB_4_COMPONENT_X:
      return 0x1;
    case D3D10_SB_4_COMPONENT_Y:
      return 0x2;
    case D3D10_SB_4_COMPONENT_Z:
      return 0x4;
    case D3D10_SB_4_COMPONENT_W:
      return 0x8;
    default:
      return 0;
  }
}

static uint8_t ReadComponentMask(const Operand &operand) {
  if (operand.NumComponents == D3D10_SB_OPERAND_0_COMPONENT) {
    return 0;
  }

  if (operand.NumComponents == D3D10_SB_OPERAND_1_COMPONENT) {
    return 0x1;
  }

  const auto selectionMode = static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(operand.ComponentMode));
  if (selectionMode == D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) {
    return static_cast<uint8_t>(DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(operand.ComponentMode) &
                                0x0F);
  }

  if (selectionMode == D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) {
    const auto comp = static_cast<D3D10_SB_4_COMPONENT_NAME>(
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(operand.ComponentMode));
    return ComponentBit(comp);
  }

  if (selectionMode == D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) {
    uint8_t mask = 0;
    for (uint32_t dst = 0; dst < 4; ++dst) {
      const auto src = DECODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_SOURCE(
          operand.ComponentMode, static_cast<D3D10_SB_4_COMPONENT_NAME>(dst));
      mask |= ComponentBit(src);
    }
    return mask;
  }

  return 0x0F;
}

static uint8_t WriteComponentMask(const Operand &operand) {
  if (operand.NumComponents == D3D10_SB_OPERAND_0_COMPONENT) {
    return 0;
  }

  if (operand.NumComponents == D3D10_SB_OPERAND_1_COMPONENT) {
    return 0x1;
  }

  const auto selectionMode = static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(
      DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(operand.ComponentMode));
  if (selectionMode == D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) {
    return static_cast<uint8_t>(DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(operand.ComponentMode) &
                                0x0F);
  }

  if (selectionMode == D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) {
    const auto comp = static_cast<D3D10_SB_4_COMPONENT_NAME>(
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(operand.ComponentMode));
    return ComponentBit(comp);
  }

  return 0x0F;
}

static uint8_t CollectInputAlwaysReadMask(const Program &program,
                                          uint32_t registerIndex) {
  uint8_t mask = 0;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    if (opcode == D3D10_SB_OPCODE_DCL_INPUT ||
        opcode == D3D10_SB_OPCODE_DCL_INPUT_PS ||
        opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV ||
        opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SGV) {
      continue;
    }

    for (const auto &operand : instruction.Operands) {
      if (operand.Type != D3D10_SB_OPERAND_TYPE_INPUT || operand.Indices.empty() ||
          operand.Indices.front() != registerIndex) {
        continue;
      }
      mask |= ReadComponentMask(operand);
    }
  }
  return mask;
}

static uint8_t CollectOutputWriteMask(const Program &program,
                                      uint32_t registerIndex) {
  uint8_t mask = 0;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    if (opcode == D3D10_SB_OPCODE_DCL_OUTPUT ||
        opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SIV ||
        opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SGV ||
        instruction.Operands.empty()) {
      continue;
    }

    const auto &dst = instruction.Operands.front();
    if (dst.Type != D3D10_SB_OPERAND_TYPE_OUTPUT || dst.Indices.empty() ||
        dst.Indices.front() != registerIndex) {
      continue;
    }

    mask |= WriteComponentMask(dst);
  }

  return mask;
}

static std::vector<uint32_t> CollectDeclaredRegisters(const Program &program,
                                                      bool inputs) {
  std::set<uint32_t> registers;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    const bool isInputOpcode =
        opcode == D3D10_SB_OPCODE_DCL_INPUT ||
        opcode == D3D10_SB_OPCODE_DCL_INPUT_PS ||
        opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV ||
        opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SGV;
    const bool isOutputOpcode =
        opcode == D3D10_SB_OPCODE_DCL_OUTPUT ||
        opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SIV ||
        opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SGV;

    if ((inputs && !isInputOpcode) || (!inputs && !isOutputOpcode)) {
      continue;
    }

    if (instruction.Operands.empty() || instruction.Operands.front().Indices.empty()) {
      continue;
    }

    registers.insert(instruction.Operands.front().Indices.front());
  }

  return std::vector<uint32_t>(registers.begin(), registers.end());
}

static std::vector<uint32_t> ComputeAddedRegisters(
    const std::vector<uint32_t> &before,
    const std::vector<uint32_t> &after) {
  std::unordered_set<uint32_t> beforeSet(before.begin(), before.end());
  std::vector<uint32_t> added;
  for (uint32_t value : after) {
    if (beforeSet.find(value) == beforeSet.end()) {
      added.push_back(value);
    }
  }
  return added;
}

static bool HasSemanticName(const SignatureParameter &parameter,
                            const std::string &expected) {
  std::string lowered = parameter.SemanticName;
  for (char &ch : lowered) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  std::string loweredExpected = expected;
  for (char &ch : loweredExpected) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }

  return lowered == loweredExpected;
}

static bool ExtendInputSignature(DxbcChunk &chunk,
                                 const Program &program,
                                 const std::vector<uint32_t> &addedInputRegisters,
                                 std::string &error) {
  if (addedInputRegisters.empty()) {
    return true;
  }

  std::vector<SignatureParameter> parameters;
  uint32_t headerFlags = 0;
  if (!ParseSignatureChunk(chunk, parameters, headerFlags, error)) {
    return false;
  }

  if (parameters.empty()) {
    error = "input signature chunk has no template parameter";
    return false;
  }

  const SignatureParameter *templateParameterRef = nullptr;
  for (const auto &parameter : parameters) {
    if (HasSemanticName(parameter, "TEXCOORD") && parameter.SystemValueType == 0) {
      templateParameterRef = &parameter;
      break;
    }
  }
  if (templateParameterRef == nullptr) {
    for (const auto &parameter : parameters) {
      if (parameter.SystemValueType == 0) {
        templateParameterRef = &parameter;
        break;
      }
    }
  }
  if (templateParameterRef == nullptr) {
    templateParameterRef = &parameters.back();
  }
  const SignatureParameter templateParameter = *templateParameterRef;

  uint32_t maxSemanticIndex = templateParameter.SemanticIndex;
  for (const auto &parameter : parameters) {
    if (HasSemanticName(parameter, templateParameter.SemanticName)) {
      maxSemanticIndex = std::max(maxSemanticIndex, parameter.SemanticIndex);
    }
  }

  for (uint32_t registerIndex : addedInputRegisters) {
    SignatureParameter injected = templateParameter;
    injected.SemanticIndex = ++maxSemanticIndex;
    injected.Register = registerIndex;
    const uint8_t alwaysRead = CollectInputAlwaysReadMask(program, registerIndex);
    const uint8_t resolvedAlwaysRead = alwaysRead == 0
                                           ? static_cast<uint8_t>((templateParameter.MaskAndRw >> 8) & 0x0F)
                                           : alwaysRead;
    injected.MaskAndRw = WithMaskAndRw(injected.MaskAndRw, 0x0F, resolvedAlwaysRead);
    InsertSignatureParameterByRegister(parameters, std::move(injected));
  }

  chunk.Data = BuildSignatureChunk(parameters, headerFlags);
  return true;
}

static bool ExtendOutputSignature(DxbcChunk &chunk,
                                  const Program &program,
                                  const std::vector<uint32_t> &addedOutputRegisters,
                                  std::string &error) {
  if (addedOutputRegisters.empty()) {
    return true;
  }

  std::vector<SignatureParameter> parameters;
  uint32_t headerFlags = 0;
  if (!ParseSignatureChunk(chunk, parameters, headerFlags, error)) {
    return false;
  }

  if (parameters.empty()) {
    error = "output signature chunk has no template parameter";
    return false;
  }

  const SignatureParameter *templateParameterRef = nullptr;
  for (const auto &parameter : parameters) {
    if (HasSemanticName(parameter, "SV_Target")) {
      templateParameterRef = &parameter;
      break;
    }
  }
  if (templateParameterRef == nullptr) {
    templateParameterRef = &parameters.back();
  }
  const SignatureParameter templateParameter = *templateParameterRef;

  uint32_t maxSemanticIndex = templateParameter.SemanticIndex;
  for (const auto &parameter : parameters) {
    if (HasSemanticName(parameter, templateParameter.SemanticName)) {
      maxSemanticIndex = std::max(maxSemanticIndex, parameter.SemanticIndex);
    }
  }

  for (uint32_t registerIndex : addedOutputRegisters) {
    SignatureParameter injected = templateParameter;
    injected.SemanticIndex = ++maxSemanticIndex;
    injected.Register = registerIndex;
    const uint8_t writeMask = CollectOutputWriteMask(program, registerIndex);
    const uint8_t resolvedWriteMask = writeMask == 0
                                          ? static_cast<uint8_t>(templateParameter.MaskAndRw & 0x0F)
                                          : writeMask;
    const uint8_t neverWriteMask = static_cast<uint8_t>(0x0F & ~resolvedWriteMask);
    injected.MaskAndRw = WithMaskAndRw(injected.MaskAndRw, 0x0F, neverWriteMask);
    InsertSignatureParameterByRegister(parameters, std::move(injected));
  }

  chunk.Data = BuildSignatureChunk(parameters, headerFlags);
  return true;
}

static bool UpdateIoSignatures(Container &container,
                               const Program &before,
                               const Program &after,
                               std::string &error) {
  const auto oldInputs = CollectDeclaredRegisters(before, true);
  const auto newInputs = CollectDeclaredRegisters(after, true);
  const auto addedInputs = ComputeAddedRegisters(oldInputs, newInputs);

  const auto oldOutputs = CollectDeclaredRegisters(before, false);
  const auto newOutputs = CollectDeclaredRegisters(after, false);
  const auto addedOutputs = ComputeAddedRegisters(oldOutputs, newOutputs);

  if (addedInputs.empty() && addedOutputs.empty()) {
    return true;
  }

  if (!addedInputs.empty()) {
    DxbcChunk *inputSigChunk = container.FindChunkByFourCC(DXBC_CHUNK_ISGN);
    if (inputSigChunk == nullptr) {
      if (container.FindChunkByFourCC(DXBC_CHUNK_ISG1) != nullptr) {
        error = "ISG1 signature updates are not implemented yet";
      } else {
        error = "input signature chunk ISGN is missing";
      }
      return false;
    }

    if (!ExtendInputSignature(*inputSigChunk, after, addedInputs, error)) {
      return false;
    }
  }

  if (!addedOutputs.empty()) {
    DxbcChunk *outputSigChunk = container.FindChunkByFourCC(DXBC_CHUNK_OSGN);
    if (outputSigChunk == nullptr) {
      if (container.FindChunkByFourCC(DXBC_CHUNK_OSG1) != nullptr) {
        error = "OSG1 signature updates are not implemented yet";
      } else {
        error = "output signature chunk OSGN is missing";
      }
      return false;
    }

    if (!ExtendOutputSignature(*outputSigChunk, after, addedOutputs, error)) {
      return false;
    }
  }

  return true;
}

} // namespace

PatchResult PatchContainerInMemory(const std::vector<uint8_t> &inputContainer,
                                   const Recipe &recipe,
                                   const RecipeContext &context) {
  PatchResult result;
  result.RecipeContext = context;

  Container container;
  if (!ParseDxbcContainer(inputContainer, container))
    return MakeError("failed to parse DXBC container");

  Program program;
  if (!ParseShaderChunk(container, program))
    return MakeError("failed to parse shader chunk");

  const Program originalProgram = program;

  if (!ExecuteRecipe(program, recipe, result.RecipeContext)) {
    const std::string error = result.RecipeContext.LastError.empty()
                                  ? "failed to execute SM5 recipe"
                                  : result.RecipeContext.LastError;
    return MakeError(error, &result.RecipeContext);
  }

  std::vector<uint8_t> shaderBytes;
  if (!RebuildShaderChunk(program, shaderBytes))
    return MakeError("failed to serialize shader chunk", &result.RecipeContext);

  DxbcChunk *shaderChunk = container.GetShaderChunk();
  if (shaderChunk == nullptr)
    return MakeError("shader chunk missing from container", &result.RecipeContext);
  shaderChunk->Data = std::move(shaderBytes);

  std::string signatureError;
  if (!UpdateIoSignatures(container, originalProgram, program, signatureError)) {
    return MakeError("failed to update signature chunks: " + signatureError,
                     &result.RecipeContext);
  }

  if (!SerializeDxbcContainer(container, result.OutputBytes))
    return MakeError("failed to serialize DXBC container", &result.RecipeContext);
  if (!RecomputeDxbcHash(result.OutputBytes))
    return MakeError("failed to recompute DXBC hash", &result.RecipeContext);

  result.Success = true;
  return result;
}

PatchResult PatchContainerInMemory(const Recipe &recipe,
                                   const uint8_t *inputData,
                                   size_t inputSize,
                                   const RecipeContext &context) {
  if (inputData == nullptr || inputSize == 0)
    return MakeError("invalid input data");
  return PatchContainerInMemory(std::vector<uint8_t>(inputData, inputData + inputSize),
                                recipe, context);
}

} // namespace sm5
} // namespace dxp
