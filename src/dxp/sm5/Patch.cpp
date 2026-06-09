#include "dxp/sm5/Patch.h"

#include "d3d11TokenizedProgramFormat.hpp"

#include "Container.h"
#include "Model.h"
#include "Parse.h"
#include "Serialize.h"
#include "Transforms.h"

#include <cctype>
#include <cstring>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace dxp {
namespace sm5 {

namespace {

}

bool ExecuteRecipe(Program &program, const Recipe &recipe,
           RecipeContext &context, dxp::PatchReport *report = nullptr,
           const std::function<void(const std::string &, RecipeContext &)>
             *beforeStep = nullptr,
           const std::function<void(const std::string &,
                      const RecipeStepResult &,
                      RecipeContext &)> *afterStep =
             nullptr);

namespace {

constexpr uint32_t DXBC_CHUNK_ISGN = 0x4E475349;
constexpr uint32_t DXBC_CHUNK_ISG1 = 0x31475349;
constexpr uint32_t DXBC_CHUNK_OSGN = 0x4E47534F;
constexpr uint32_t DXBC_CHUNK_OSG1 = 0x3147534F;

static std::string FourCCToString(uint32_t fourCC) {
  std::string text(4, '\0');
  text[0] = static_cast<char>(fourCC & 0xffu);
  text[1] = static_cast<char>((fourCC >> 8u) & 0xffu);
  text[2] = static_cast<char>((fourCC >> 16u) & 0xffu);
  text[3] = static_cast<char>((fourCC >> 24u) & 0xffu);

  for (char &ch : text) {
    if (!std::isprint(static_cast<unsigned char>(ch)))
      ch = '?';
  }

  return text;
}

static std::string DxbcHashToHex(const DxbcContainerHeader &header) {
  static constexpr char kHexDigits[] = "0123456789abcdef";

  std::string hex;
  hex.reserve(32);
  for (uint32_t word : header.Hash) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
      const uint8_t byte = static_cast<uint8_t>((word >> shift) & 0xffu);
      hex.push_back(kHexDigits[(byte >> 4u) & 0xfu]);
      hex.push_back(kHexDigits[byte & 0xfu]);
    }
  }
  return hex;
}

static bool BuildDxbcContainerReport(const std::vector<uint8_t> &containerBytes,
                                     dxp::PatchContainerReport &report) {
  Container container;
  if (!ParseDxbcContainer(containerBytes, container))
    return false;

  report = dxp::PatchContainerReport{};
  report.Format = "DXBC";
  report.TotalSizeInBytes = container.Header.TotalSizeInBytes;
  report.HashHex = DxbcHashToHex(container.Header);
  report.Chunks.reserve(container.Chunks.size());

  for (const DxbcChunk &chunk : container.Chunks) {
    dxp::PatchChunkReport chunkReport;
    chunkReport.Id = FourCCToString(chunk.FourCC);
    chunkReport.FourCC = chunk.FourCC;
    chunkReport.OffsetInContainer = chunk.OffsetInContainer;
    chunkReport.SizeInBytes = static_cast<uint32_t>(chunk.Data.size());
    report.Chunks.push_back(std::move(chunkReport));
  }

  return true;
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

static void WriteU32(std::vector<uint8_t> &bytes, size_t offset,
                     uint32_t value) {
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

static uint32_t ReadU32(const std::vector<uint8_t> &bytes, size_t offset) {
  uint32_t value = 0;
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

static bool ParseSignatureChunk(const DxbcChunk &chunk,
                                std::vector<SignatureParameter> &parameters,
                                uint32_t &headerFlags, std::string &error) {
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

static std::vector<uint8_t>
BuildSignatureChunk(const std::vector<SignatureParameter> &parameters,
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

    WriteU32(bytes, base + 0,
             getOrCreateSemanticOffset(parameter.SemanticName));
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

static void
InsertSignatureParameterByRegister(std::vector<SignatureParameter> &parameters,
                                   SignatureParameter parameter) {
  const auto insertIt = std::lower_bound(
      parameters.begin(), parameters.end(), parameter.Register,
      [](const SignatureParameter &lhs, uint32_t registerIndex) {
        return lhs.Register < registerIndex;
      });
  parameters.insert(insertIt, std::move(parameter));
}

static uint32_t WithMaskAndRw(uint32_t value, uint8_t mask, uint8_t rwMask) {
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

  const auto selectionMode =
      static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(
          DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
              operand.ComponentMode));
  if (selectionMode == D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) {
    return static_cast<uint8_t>(
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(operand.ComponentMode) & 0x0F);
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

  const auto selectionMode =
      static_cast<D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE>(
          DECODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
              operand.ComponentMode));
  if (selectionMode == D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) {
    return static_cast<uint8_t>(
        DECODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(operand.ComponentMode) & 0x0F);
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
      if (operand.Type != D3D10_SB_OPERAND_TYPE_INPUT ||
          operand.Indices.empty() || operand.Indices.front() != registerIndex) {
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
    const bool isInputOpcode = opcode == D3D10_SB_OPCODE_DCL_INPUT ||
                               opcode == D3D10_SB_OPCODE_DCL_INPUT_PS ||
                               opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV ||
                               opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SGV;
    const bool isOutputOpcode = opcode == D3D10_SB_OPCODE_DCL_OUTPUT ||
                                opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SIV ||
                                opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SGV;

    if ((inputs && !isInputOpcode) || (!inputs && !isOutputOpcode)) {
      continue;
    }

    if (instruction.Operands.empty() ||
        instruction.Operands.front().Indices.empty()) {
      continue;
    }

    registers.insert(instruction.Operands.front().Indices.front());
  }

  return std::vector<uint32_t>(registers.begin(), registers.end());
}

static std::vector<uint32_t>
ComputeAddedRegisters(const std::vector<uint32_t> &before,
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

static bool
ExtendInputSignature(DxbcChunk &chunk, const Program &program,
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
    if (HasSemanticName(parameter, "TEXCOORD") &&
        parameter.SystemValueType == 0) {
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
    const uint8_t alwaysRead =
        CollectInputAlwaysReadMask(program, registerIndex);
    const uint8_t resolvedAlwaysRead =
        alwaysRead == 0
            ? static_cast<uint8_t>((templateParameter.MaskAndRw >> 8) & 0x0F)
            : alwaysRead;
    injected.MaskAndRw =
        WithMaskAndRw(injected.MaskAndRw, 0x0F, resolvedAlwaysRead);
    InsertSignatureParameterByRegister(parameters, std::move(injected));
  }

  chunk.Data = BuildSignatureChunk(parameters, headerFlags);
  return true;
}

static bool
ExtendOutputSignature(DxbcChunk &chunk, const Program &program,
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
    const uint8_t resolvedWriteMask =
        writeMask == 0
            ? static_cast<uint8_t>(templateParameter.MaskAndRw & 0x0F)
            : writeMask;
    const uint8_t neverWriteMask =
        static_cast<uint8_t>(0x0F & ~resolvedWriteMask);
    injected.MaskAndRw =
        WithMaskAndRw(injected.MaskAndRw, 0x0F, neverWriteMask);
    InsertSignatureParameterByRegister(parameters, std::move(injected));
  }

  chunk.Data = BuildSignatureChunk(parameters, headerFlags);
  return true;
}

static bool UpdateIoSignatures(Container &container, const Program &before,
                               const Program &after, std::string &error) {
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

bool ProgramOperand::operator==(const ProgramOperand &rhs) const {
  if (Type != rhs.Type ||
      NumComponents != rhs.NumComponents ||
      ComponentMode != rhs.ComponentMode ||
      Modifier != rhs.Modifier ||
      Indices != rhs.Indices ||
      ImmediateValues != rhs.ImmediateValues) {
    return false;
  }

  if (RelativeOperands.size() != rhs.RelativeOperands.size()) {
    return false;
  }

  for (size_t i = 0; i < RelativeOperands.size(); ++i) {
    if (RelativeOperands[i] != rhs.RelativeOperands[i]) {
      return false;
    }
  }

  return true;
}

static ProgramOperand ConvertOperand(const Operand &operand) {
  ProgramOperand converted;
  converted.Type = static_cast<uint32_t>(operand.Type);
  converted.NumComponents = static_cast<uint32_t>(operand.NumComponents);
  converted.ComponentMode = operand.ComponentMode;
  converted.Modifier = static_cast<uint32_t>(operand.Modifier);
  converted.Indices = operand.Indices;
  converted.ImmediateValues = operand.ImmediateValues;
  if (operand.RelativeOperand) {
    converted.RelativeOperands.push_back(
        ConvertOperand(*operand.RelativeOperand));
  }
  return converted;
}

static ProgramInstruction ConvertInstruction(const Instruction &instruction) {
  ProgramInstruction converted;
  converted.Opcode = static_cast<uint32_t>(instruction.Opcode);
  converted.LengthInDwords = instruction.LengthInDwords;
  converted.RawTokens = instruction.RawTokens;
  converted.HasInputInterpolationMode =
      instruction.Controls.HasInputInterpolationMode;
  converted.InputInterpolationMode =
      instruction.Controls.InputInterpolationMode;
  converted.Operands.reserve(instruction.Operands.size());
  for (const auto &operand : instruction.Operands) {
    converted.Operands.push_back(ConvertOperand(operand));
  }
  return converted;
}

static void FillInspectionFromProgram(const Program &program,
                                      ProgramInspection &inspection) {
  inspection = {};
  inspection.TempCount = program.TempCount;

  inspection.Instructions.reserve(program.Instructions.size());
  for (const auto &instruction : program.Instructions) {
    inspection.Instructions.push_back(ConvertInstruction(instruction));
  }

  inspection.ResourceBindPoints.reserve(program.Resources.size());
  for (const auto &resource : program.Resources) {
    inspection.ResourceBindPoints.push_back(resource.RegisterBindPoint);
  }

  inspection.CBufferBindPoints.reserve(program.CBuffers.size());
  for (const auto &cbuffer : program.CBuffers) {
    inspection.CBufferBindPoints.push_back(cbuffer.RegisterBindPoint);
  }

  inspection.SamplerBindPoints.reserve(program.Samplers.size());
  for (const auto &sampler : program.Samplers) {
    inspection.SamplerBindPoints.push_back(sampler.RegisterBindPoint);
  }
}

static bool
ParseProgramForInspection(const std::vector<uint8_t> &inputContainer,
                          Program &program, std::string *error) {
  Container container;
  if (!ParseDxbcContainer(inputContainer, container)) {
    if (error != nullptr) {
      *error = "failed to parse DXBC container";
    }
    return false;
  }

  if (!ParseShaderChunk(container, program)) {
    if (error != nullptr) {
      *error = "failed to parse shader chunk";
    }
    return false;
  }

  return true;
}

}

PatchResult PatchContainer(const std::vector<uint8_t> &inputContainer,
                           const Recipe &recipe, const RecipeContext &context) {
  RecipeContext mutableContext = context;
  return PatchContainer(inputContainer, recipe, mutableContext);
}

PatchResult PatchContainer(const std::vector<uint8_t> &inputContainer,
                           const Recipe &recipe, RecipeContext &context,
                           const RecipeExecutionOptions &execution) {
  PatchResult result;
  result.RecipeContext = context;

  Container container;
  if (!ParseDxbcContainer(inputContainer, container))
    return MakeError("failed to parse DXBC container");

  Program program;
  if (!ParseShaderChunk(container, program))
    return MakeError("failed to parse shader chunk");

  const Program originalProgram = program;

  if (!ExecuteRecipe(program, recipe, result.RecipeContext, &result.Report,
                     &execution.BeforeStep, &execution.AfterStep)) {
    const std::string error = result.RecipeContext.LastError.empty()
                                  ? "failed to execute SM5 recipe"
                                  : result.RecipeContext.LastError;
    result.Success = false;
    result.Error = error;
    return result;
  }


  for (const auto &exp : recipe.GetExports()) {
    if (exp.keys.empty()) {

      switch (exp.kind) {
        case RecipeExport::Kind::CapturedOperands:
          for (const auto &entry : result.RecipeContext.captures.operands) {
            result.Report.Exports.captured_operands[entry.first] = entry.second;
          }
          break;
        case RecipeExport::Kind::CapturedInstructions:
          for (const auto &entry : result.RecipeContext.captures.instructions) {
            result.Report.Exports.captured_instructions[entry.first] = entry.second;
          }
          break;
        case RecipeExport::Kind::CapturedIndexValues:
          result.Report.Exports.captured_index_values =
              result.RecipeContext.captures.indexValues;
          break;
        case RecipeExport::Kind::Variables:
          result.Report.Exports.variables = result.RecipeContext.Variables;
          break;
        case RecipeExport::Kind::State:
          result.Report.Exports.state = result.RecipeContext.State;
          break;
      }
    } else {

      for (const auto &k : exp.keys) {
        switch (exp.kind) {
          case RecipeExport::Kind::CapturedOperands:
            if (auto it = result.RecipeContext.captures.operands.find(k);
                it != result.RecipeContext.captures.operands.end())
              result.Report.Exports.captured_operands[k] = it->second;
            break;
          case RecipeExport::Kind::CapturedInstructions:
            if (auto it =
                    result.RecipeContext.captures.instructions.find(k);
                it != result.RecipeContext.captures.instructions.end())
              result.Report.Exports.captured_instructions[k] = it->second;
            break;
          case RecipeExport::Kind::CapturedIndexValues:
            if (auto it =
                    result.RecipeContext.captures.indexValues.find(k);
                it != result.RecipeContext.captures.indexValues.end())
              result.Report.Exports.captured_index_values[k] = it->second;
            break;
          case RecipeExport::Kind::Variables:
            if (auto it = result.RecipeContext.Variables.find(k);
                it != result.RecipeContext.Variables.end())
              result.Report.Exports.variables[k] = it->second;
            break;
          case RecipeExport::Kind::State:
            if (auto it = result.RecipeContext.State.find(k);
                it != result.RecipeContext.State.end())
              result.Report.Exports.state[k] = it->second;
            break;
        }
      }
    }
  }

  if (!result.RecipeContext.ModuleVerified) {
    std::vector<uint8_t> verifiedShaderBytes;
    if (!RebuildShaderChunk(program, verifiedShaderBytes)) {
      return MakeError("failed to verify SM5 shader chunk",
                       &result.RecipeContext);
    }
    result.RecipeContext.ModuleVerified = true;
  }

  std::vector<uint8_t> shaderBytes;
  if (!RebuildShaderChunk(program, shaderBytes))
    return MakeError("failed to serialize shader chunk", &result.RecipeContext);

  DxbcChunk *shaderChunk = container.GetShaderChunk();
  if (shaderChunk == nullptr)
    return MakeError("shader chunk missing from container",
                     &result.RecipeContext);
  shaderChunk->Data = std::move(shaderBytes);

  std::string signatureError;
  if (!UpdateIoSignatures(container, originalProgram, program,
                          signatureError)) {
    return MakeError("failed to update signature chunks: " + signatureError,
                     &result.RecipeContext);
  }

  if (!SerializeDxbcContainer(container, result.OutputBytes))
    return MakeError("failed to serialize DXBC container",
                     &result.RecipeContext);
  if (!RecomputeDxbcHash(result.OutputBytes))
    return MakeError("failed to recompute DXBC hash", &result.RecipeContext);
  if (!BuildDxbcContainerReport(result.OutputBytes,
                                result.Report.OutputContainer)) {
    return MakeError("failed to inspect serialized DXBC container",
                     &result.RecipeContext);
  }

  result.Success = true;
  context = result.RecipeContext;
  return result;
}

PatchResult PatchContainer(const Recipe &recipe, const uint8_t *inputData,
                           size_t inputSize, const RecipeContext &context) {
  RecipeContext mutableContext = context;
  return PatchContainer(recipe, inputData, inputSize, mutableContext);
}

PatchResult PatchContainer(const Recipe &recipe, const uint8_t *inputData,
                           size_t inputSize, RecipeContext &context,
                           const RecipeExecutionOptions &execution) {
  if (inputData == nullptr || inputSize == 0)
    return MakeError("invalid input data");
  PatchResult result = PatchContainer(
      std::vector<uint8_t>(inputData, inputData + inputSize), recipe, context,
      execution);
  return result;
}

bool ExtractProgramOpcodes(const std::vector<uint8_t> &inputContainer,
                           std::vector<uint32_t> &opcodes, std::string *error) {
  opcodes.clear();

  Program program;
  if (!ParseProgramForInspection(inputContainer, program, error)) {
    return false;
  }

  opcodes.reserve(program.Instructions.size());
  for (const auto &instruction : program.Instructions) {
    opcodes.push_back(static_cast<uint32_t>(instruction.Opcode));
  }

  return true;
}

bool ExtractProgramOpcodes(const uint8_t *inputData, size_t inputSize,
                           std::vector<uint32_t> &opcodes, std::string *error) {
  if (inputData == nullptr || inputSize == 0) {
    if (error != nullptr) {
      *error = "invalid input data";
    }
    opcodes.clear();
    return false;
  }

  return ExtractProgramOpcodes(
      std::vector<uint8_t>(inputData, inputData + inputSize), opcodes, error);
}

bool InspectProgram(const std::vector<uint8_t> &inputContainer,
                    ProgramInspection &inspection, std::string *error) {
  inspection = {};

  Program program;
  if (!ParseProgramForInspection(inputContainer, program, error)) {
    return false;
  }

  FillInspectionFromProgram(program, inspection);
  return true;
}

bool InspectProgram(const uint8_t *inputData, size_t inputSize,
                    ProgramInspection &inspection, std::string *error) {
  if (inputData == nullptr || inputSize == 0) {
    if (error != nullptr) {
      *error = "invalid input data";
    }
    inspection = {};
    return false;
  }

  return InspectProgram(std::vector<uint8_t>(inputData, inputData + inputSize),
                        inspection, error);
}

}
}
