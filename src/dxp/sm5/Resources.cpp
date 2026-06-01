#include "dxp/sm5/Recipe.h"

#include "d3d11TokenizedProgramFormat.hpp"

#include "dxp/sm5/Serialize.h"
#include "dxp/sm5/Transforms.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <limits>
#include <set>
#include <unordered_set>
#include <vector>

namespace dxp::sm5 {

namespace {

static uint32_t MakeSelectComponentMode(D3D10_SB_4_COMPONENT_NAME component) {
  return ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
             D3D10_SB_OPERAND_4_COMPONENT_SELECT_1_MODE) |
         ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECT_1(component);
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

static Operand MakeConstantBufferDeclarationOperand(uint32_t bindPoint,
                                                    uint32_t elementCount) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_CONSTANT_BUFFER;
  operand.NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
  operand.ComponentMode =
      ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
          D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE_MODE) |
      ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SWIZZLE(
          D3D10_SB_4_COMPONENT_X, D3D10_SB_4_COMPONENT_Y,
          D3D10_SB_4_COMPONENT_Z, D3D10_SB_4_COMPONENT_W);
  operand.Indices = {bindPoint, elementCount};
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

static Operand MakeInputOperand(uint32_t bindPoint) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_INPUT;
  operand.NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
  operand.ComponentMode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                              D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
                          ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(
                              D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL);
  operand.Indices = {bindPoint};
  return operand;
}

static Operand MakeOutputOperand(uint32_t bindPoint) {
  Operand operand;
  operand.Type = D3D10_SB_OPERAND_TYPE_OUTPUT;
  operand.NumComponents = D3D10_SB_OPERAND_4_COMPONENT;
  operand.ComponentMode = ENCODE_D3D10_SB_OPERAND_4_COMPONENT_SELECTION_MODE(
                              D3D10_SB_OPERAND_4_COMPONENT_MASK_MODE) |
                          ENCODE_D3D10_SB_OPERAND_4_COMPONENT_MASK(
                              D3D10_SB_OPERAND_4_COMPONENT_MASK_ALL);
  operand.Indices = {bindPoint};
  return operand;
}

static Operand MakeUavOperand(uint32_t bindPoint) {
  Operand operand;
  operand.Type = D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW;
  operand.NumComponents = D3D10_SB_OPERAND_0_COMPONENT;
  operand.ComponentMode = 0;
  operand.Indices = {bindPoint};
  return operand;
}

static Instruction
BuildConstantBufferDeclaration(const RecipeCBufferDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER};
  const auto declarationOperand =
      MakeConstantBufferDeclarationOperand(decl.BindPoint, decl.Elements);
  const auto operand = EncodeOperand(declarationOperand);

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER) |
      ENCODE_D3D10_SB_D3D10_SB_CONSTANT_BUFFER_ACCESS_PATTERN(
          decl.AccessPattern) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          1u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(),
                               operand.end());
  instruction.LengthInDwords =
      static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(std::move(declarationOperand));
  return instruction;
}

static Instruction BuildTextureDeclaration(const RecipeTextureDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D10_SB_OPCODE_DCL_RESOURCE};
  const auto operand = EncodeOperand(MakeResourceOperand(decl.BindPoint));
  const uint32_t returnTypeToken =
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 0) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 1) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 2) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 3);

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_RESOURCE) |
      ENCODE_D3D10_SB_RESOURCE_DIMENSION(
          static_cast<D3D10_SB_RESOURCE_DIMENSION>(decl.Dimension)) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          2u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(),
                               operand.end());
  instruction.RawTokens.push_back(returnTypeToken);
  instruction.LengthInDwords =
      static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeResourceOperand(decl.BindPoint));
  return instruction;
}

static Instruction BuildInputDeclaration(const RecipeInputDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D10_SB_OPCODE_DCL_INPUT_PS};
  instruction.Controls.HasInputInterpolationMode = true;
  instruction.Controls.InputInterpolationMode = decl.InterpolationMode;
  const auto operand = EncodeOperand(MakeInputOperand(decl.BindPoint));

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_INPUT_PS) |
      ENCODE_D3D10_SB_INPUT_INTERPOLATION_MODE(
          static_cast<D3D10_SB_INTERPOLATION_MODE>(decl.InterpolationMode)) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          1u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(),
                               operand.end());
  instruction.LengthInDwords =
      static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeInputOperand(decl.BindPoint));
  return instruction;
}

static Instruction BuildOutputDeclaration(const RecipeOutputDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D10_SB_OPCODE_DCL_OUTPUT};
  const auto operand = EncodeOperand(MakeOutputOperand(decl.BindPoint));

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_OUTPUT) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          1u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(),
                               operand.end());
  instruction.LengthInDwords =
      static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeOutputOperand(decl.BindPoint));
  return instruction;
}

static Instruction BuildSamplerDeclaration(const RecipeSamplerDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D10_SB_OPCODE_DCL_SAMPLER};
  const auto operand = EncodeOperand(MakeSamplerOperand(decl.BindPoint));

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D10_SB_OPCODE_DCL_SAMPLER) |
      ENCODE_D3D10_SB_SAMPLER_MODE(
          static_cast<D3D10_SB_SAMPLER_MODE>(decl.Mode)) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          1u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(),
                               operand.end());
  instruction.LengthInDwords =
      static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeSamplerOperand(decl.BindPoint));
  return instruction;
}

static Instruction
BuildRawResourceDeclaration(const RecipeRawResourceDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D11_SB_OPCODE_DCL_RESOURCE_RAW};
  const auto operand = EncodeOperand(MakeResourceOperand(decl.BindPoint));

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D11_SB_OPCODE_DCL_RESOURCE_RAW) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          1u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(),
                               operand.end());
  instruction.LengthInDwords =
      static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeResourceOperand(decl.BindPoint));
  return instruction;
}

static Instruction
BuildStructuredResourceDeclaration(const RecipeStructuredResourceDecl &decl) {
  Instruction instruction;
  instruction.Opcode = Opcode{D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED};
  const auto operand = EncodeOperand(MakeResourceOperand(decl.BindPoint));

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          2u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(),
                               operand.end());
  instruction.RawTokens.push_back(decl.StructureStride);
  instruction.LengthInDwords =
      static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeResourceOperand(decl.BindPoint));
  return instruction;
}

static Instruction BuildUavDeclaration(const RecipeUavDecl &decl) {
  Instruction instruction;

  if (decl.Kind == RecipeUavKind::Raw) {
    instruction.Opcode = Opcode{D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW};
    const auto operand = EncodeOperand(MakeUavOperand(decl.BindPoint));
    instruction.RawTokens.push_back(
        ENCODE_D3D10_SB_OPCODE_TYPE(
            D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW) |
        ENCODE_D3D11_SB_ACCESS_COHERENCY_FLAGS(
            decl.GloballyCoherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0) |
        ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
            1u + static_cast<uint32_t>(operand.size())));
    instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(),
                                 operand.end());
    instruction.LengthInDwords =
        static_cast<uint32_t>(instruction.RawTokens.size());
    instruction.Operands.push_back(MakeUavOperand(decl.BindPoint));
    return instruction;
  }

  if (decl.Kind == RecipeUavKind::Structured) {
    instruction.Opcode =
        Opcode{D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED};
    const auto operand = EncodeOperand(MakeUavOperand(decl.BindPoint));
    uint32_t flags =
        decl.GloballyCoherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0;
    if (decl.HasOrderPreservingCounter) {
      flags |= D3D11_SB_UAV_HAS_ORDER_PRESERVING_COUNTER;
    }
    instruction.RawTokens.push_back(
        ENCODE_D3D10_SB_OPCODE_TYPE(
            D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED) |
        ENCODE_D3D11_SB_ACCESS_COHERENCY_FLAGS(flags) |
        ENCODE_D3D11_SB_UAV_FLAGS(flags) |
        ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
            2u + static_cast<uint32_t>(operand.size())));
    instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(),
                                 operand.end());
    instruction.RawTokens.push_back(decl.StructureStride);
    instruction.LengthInDwords =
        static_cast<uint32_t>(instruction.RawTokens.size());
    instruction.Operands.push_back(MakeUavOperand(decl.BindPoint));
    return instruction;
  }

  instruction.Opcode = Opcode{D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED};
  const auto operand = EncodeOperand(MakeUavOperand(decl.BindPoint));
  const uint32_t returnTypeToken =
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 0) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 1) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 2) |
      ENCODE_D3D10_SB_RESOURCE_RETURN_TYPE(D3D10_SB_RETURN_TYPE_FLOAT, 3);

  instruction.RawTokens.push_back(
      ENCODE_D3D10_SB_OPCODE_TYPE(
          D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED) |
      ENCODE_D3D10_SB_RESOURCE_DIMENSION(
          static_cast<D3D10_SB_RESOURCE_DIMENSION>(decl.Dimension)) |
      ENCODE_D3D11_SB_ACCESS_COHERENCY_FLAGS(
          decl.GloballyCoherent ? D3D11_SB_GLOBALLY_COHERENT_ACCESS : 0) |
      ENCODE_D3D10_SB_TOKENIZED_INSTRUCTION_LENGTH(
          2u + static_cast<uint32_t>(operand.size())));
  instruction.RawTokens.insert(instruction.RawTokens.end(), operand.begin(),
                               operand.end());
  instruction.RawTokens.push_back(returnTypeToken);
  instruction.LengthInDwords =
      static_cast<uint32_t>(instruction.RawTokens.size());
  instruction.Operands.push_back(MakeUavOperand(decl.BindPoint));
  return instruction;
}

static uint32_t FindInsertAfterLastDeclaration(const Program &program,
                                               OpcodeType opcode) {
  uint32_t insertIndex = 0;
  for (uint32_t i = 0; i < program.Instructions.size(); ++i) {
    if (static_cast<OpcodeType>(program.Instructions[i].Opcode) == opcode) {
      insertIndex = i + 1;
    }
  }
  return insertIndex;
}

static bool AllocateBindPoint(std::unordered_set<uint32_t> &occupied,
                              bool autoBind, uint32_t requestedBindPoint,
                              uint32_t &resolvedBindPoint, std::string &error) {
  if (!autoBind) {
    if (occupied.find(requestedBindPoint) != occupied.end()) {
      error = "SM5 declaration bind point already occupied: " +
              std::to_string(requestedBindPoint);
      return false;
    }

    resolvedBindPoint = requestedBindPoint;
    occupied.insert(resolvedBindPoint);
    return true;
  }

  uint32_t candidate = requestedBindPoint;
  while (occupied.find(candidate) != occupied.end()) {
    if (candidate == std::numeric_limits<uint32_t>::max()) {
      error = "SM5 declaration auto_bind exhausted available bind points";
      return false;
    }
    ++candidate;
  }

  resolvedBindPoint = candidate;
  occupied.insert(resolvedBindPoint);
  return true;
}

static bool
RecordNamedBinding(std::unordered_map<std::string, uint32_t> &bindings,
                   const std::string &handle, uint32_t bindPoint,
                   const char *resourceKind, std::string &error) {
  if (handle.empty()) {
    return true;
  }

  if (bindings.find(handle) != bindings.end()) {
    error = std::string("duplicate SM5 ") + resourceKind +
            " declaration handle: " + handle;
    return false;
  }

  bindings.emplace(handle, bindPoint);
  return true;
}

} // namespace

bool AddInputDeclaration(Program &program, const RecipeInputDecl &decl,
                         RecipeContext &context, std::string &error) {
  std::unordered_set<uint32_t> occupied;
  uint32_t insertIndex = 0;
  for (uint32_t i = 0; i < program.Instructions.size(); ++i) {
    const auto opcode = static_cast<OpcodeType>(program.Instructions[i].Opcode);
    if ((opcode == D3D10_SB_OPCODE_DCL_INPUT ||
         opcode == D3D10_SB_OPCODE_DCL_INPUT_PS ||
         opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SIV ||
         opcode == D3D10_SB_OPCODE_DCL_INPUT_PS_SGV) &&
        !program.Instructions[i].Operands.empty() &&
        !program.Instructions[i].Operands.front().Indices.empty()) {
      occupied.insert(program.Instructions[i].Operands.front().Indices.front());
      insertIndex = i + 1;
    }
  }

  uint32_t resolvedBindPoint = decl.BindPoint;
  if (!AllocateBindPoint(occupied, decl.AutoBind, decl.BindPoint,
                         resolvedBindPoint, error)) {
    return false;
  }

  RecipeInputDecl resolvedDecl = decl;
  resolvedDecl.BindPoint = resolvedBindPoint;
  program.Instructions.insert(program.Instructions.begin() +
                                  static_cast<ptrdiff_t>(insertIndex),
                              BuildInputDeclaration(resolvedDecl));
  if (!RecordNamedBinding(context.InputBindings, decl.Handle, resolvedBindPoint,
                          "input", error)) {
    return false;
  }

  return true;
}

bool AddOutputDeclaration(Program &program, const RecipeOutputDecl &decl,
                          RecipeContext &context, std::string &error) {
  std::unordered_set<uint32_t> occupied;
  uint32_t insertIndex = 0;
  for (uint32_t i = 0; i < program.Instructions.size(); ++i) {
    const auto opcode = static_cast<OpcodeType>(program.Instructions[i].Opcode);
    if ((opcode == D3D10_SB_OPCODE_DCL_OUTPUT ||
         opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SIV ||
         opcode == D3D10_SB_OPCODE_DCL_OUTPUT_SGV) &&
        !program.Instructions[i].Operands.empty() &&
        !program.Instructions[i].Operands.front().Indices.empty()) {
      occupied.insert(program.Instructions[i].Operands.front().Indices.front());
      insertIndex = i + 1;
    }
  }

  uint32_t resolvedBindPoint = decl.BindPoint;
  if (!AllocateBindPoint(occupied, decl.AutoBind, decl.BindPoint,
                         resolvedBindPoint, error)) {
    return false;
  }

  RecipeOutputDecl resolvedDecl = decl;
  resolvedDecl.BindPoint = resolvedBindPoint;
  program.Instructions.insert(program.Instructions.begin() +
                                  static_cast<ptrdiff_t>(insertIndex),
                              BuildOutputDeclaration(resolvedDecl));
  if (!RecordNamedBinding(context.OutputBindings, decl.Handle,
                          resolvedBindPoint, "output", error)) {
    return false;
  }

  return true;
}

bool AddCBufferDeclaration(Program &program, const RecipeCBufferDecl &decl,
                           RecipeContext &context, std::string &error) {
  std::unordered_set<uint32_t> occupied;
  for (const auto &cbuffer : program.CBuffers) {
    occupied.insert(cbuffer.RegisterBindPoint);
  }

  uint32_t resolvedBindPoint = decl.BindPoint;
  if (!AllocateBindPoint(occupied, decl.AutoBind, decl.BindPoint,
                         resolvedBindPoint, error)) {
    return false;
  }

  RecipeCBufferDecl resolvedDecl = decl;
  resolvedDecl.BindPoint = resolvedBindPoint;
  const uint32_t insertIndex = FindInsertAfterLastDeclaration(
      program, D3D10_SB_OPCODE_DCL_CONSTANT_BUFFER);
  program.Instructions.insert(program.Instructions.begin() +
                                  static_cast<ptrdiff_t>(insertIndex),
                              BuildConstantBufferDeclaration(resolvedDecl));
  if (!RecordNamedBinding(context.CBufferBindings, decl.Handle,
                          resolvedBindPoint, "cbuffer", error)) {
    return false;
  }

  return true;
}

bool AddTextureDeclaration(Program &program, const RecipeTextureDecl &decl,
                           RecipeContext &context, std::string &error) {
  std::unordered_set<uint32_t> occupied;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    if ((opcode == D3D10_SB_OPCODE_DCL_RESOURCE ||
         opcode == D3D11_SB_OPCODE_DCL_RESOURCE_RAW ||
         opcode == D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED) &&
        !instruction.Operands.empty() &&
        !instruction.Operands.front().Indices.empty()) {
      occupied.insert(instruction.Operands.front().Indices.front());
    }
  }

  uint32_t resolvedBindPoint = decl.BindPoint;
  if (!AllocateBindPoint(occupied, decl.AutoBind, decl.BindPoint,
                         resolvedBindPoint, error)) {
    return false;
  }

  RecipeTextureDecl resolvedDecl = decl;
  resolvedDecl.BindPoint = resolvedBindPoint;
  const uint32_t insertIndex =
      FindInsertAfterLastDeclaration(program, D3D10_SB_OPCODE_DCL_RESOURCE);
  program.Instructions.insert(program.Instructions.begin() +
                                  static_cast<ptrdiff_t>(insertIndex),
                              BuildTextureDeclaration(resolvedDecl));
  if (!RecordNamedBinding(context.TextureBindings, decl.Handle,
                          resolvedBindPoint, "texture", error)) {
    return false;
  }

  return true;
}

bool AddRawResourceDeclaration(Program &program,
                               const RecipeRawResourceDecl &decl,
                               RecipeContext &context, std::string &error) {
  std::unordered_set<uint32_t> occupied;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    if ((opcode == D3D10_SB_OPCODE_DCL_RESOURCE ||
         opcode == D3D11_SB_OPCODE_DCL_RESOURCE_RAW ||
         opcode == D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED) &&
        !instruction.Operands.empty() &&
        !instruction.Operands.front().Indices.empty()) {
      occupied.insert(instruction.Operands.front().Indices.front());
    }
  }

  uint32_t resolvedBindPoint = decl.BindPoint;
  if (!AllocateBindPoint(occupied, decl.AutoBind, decl.BindPoint,
                         resolvedBindPoint, error)) {
    return false;
  }

  RecipeRawResourceDecl resolvedDecl = decl;
  resolvedDecl.BindPoint = resolvedBindPoint;
  const uint32_t insertIndex =
      FindInsertAfterLastDeclaration(program, D3D11_SB_OPCODE_DCL_RESOURCE_RAW);
  program.Instructions.insert(program.Instructions.begin() +
                                  static_cast<ptrdiff_t>(insertIndex),
                              BuildRawResourceDeclaration(resolvedDecl));
  if (!RecordNamedBinding(context.RawResourceBindings, decl.Handle,
                          resolvedBindPoint, "raw resource", error)) {
    return false;
  }

  return true;
}

bool AddStructuredResourceDeclaration(Program &program,
                                      const RecipeStructuredResourceDecl &decl,
                                      RecipeContext &context,
                                      std::string &error) {
  std::unordered_set<uint32_t> occupied;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    if ((opcode == D3D10_SB_OPCODE_DCL_RESOURCE ||
         opcode == D3D11_SB_OPCODE_DCL_RESOURCE_RAW ||
         opcode == D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED) &&
        !instruction.Operands.empty() &&
        !instruction.Operands.front().Indices.empty()) {
      occupied.insert(instruction.Operands.front().Indices.front());
    }
  }

  uint32_t resolvedBindPoint = decl.BindPoint;
  if (!AllocateBindPoint(occupied, decl.AutoBind, decl.BindPoint,
                         resolvedBindPoint, error)) {
    return false;
  }

  RecipeStructuredResourceDecl resolvedDecl = decl;
  resolvedDecl.BindPoint = resolvedBindPoint;
  const uint32_t insertIndex = FindInsertAfterLastDeclaration(
      program, D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED);
  program.Instructions.insert(program.Instructions.begin() +
                                  static_cast<ptrdiff_t>(insertIndex),
                              BuildStructuredResourceDeclaration(resolvedDecl));
  if (!RecordNamedBinding(context.StructuredResourceBindings, decl.Handle,
                          resolvedBindPoint, "structured resource", error)) {
    return false;
  }

  return true;
}

bool AddSamplerDeclaration(Program &program, const RecipeSamplerDecl &decl,
                           RecipeContext &context, std::string &error) {
  std::unordered_set<uint32_t> occupied;
  for (const auto &sampler : program.Samplers) {
    occupied.insert(sampler.RegisterBindPoint);
  }

  uint32_t resolvedBindPoint = decl.BindPoint;
  if (!AllocateBindPoint(occupied, decl.AutoBind, decl.BindPoint,
                         resolvedBindPoint, error)) {
    return false;
  }

  RecipeSamplerDecl resolvedDecl = decl;
  resolvedDecl.BindPoint = resolvedBindPoint;
  const uint32_t insertIndex =
      FindInsertAfterLastDeclaration(program, D3D10_SB_OPCODE_DCL_SAMPLER);
  program.Instructions.insert(program.Instructions.begin() +
                                  static_cast<ptrdiff_t>(insertIndex),
                              BuildSamplerDeclaration(resolvedDecl));
  if (!RecordNamedBinding(context.SamplerBindings, decl.Handle,
                          resolvedBindPoint, "sampler", error)) {
    return false;
  }

  return true;
}

bool AddUavDeclaration(Program &program, const RecipeUavDecl &decl,
                       RecipeContext &context, std::string &error) {
  std::unordered_set<uint32_t> occupied;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = static_cast<OpcodeType>(instruction.Opcode);
    if ((opcode == D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED ||
         opcode == D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW ||
         opcode == D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED) &&
        !instruction.Operands.empty() &&
        !instruction.Operands.front().Indices.empty()) {
      occupied.insert(instruction.Operands.front().Indices.front());
    }
  }

  uint32_t resolvedBindPoint = decl.BindPoint;
  if (!AllocateBindPoint(occupied, decl.AutoBind, decl.BindPoint,
                         resolvedBindPoint, error)) {
    return false;
  }

  RecipeUavDecl resolvedDecl = decl;
  resolvedDecl.BindPoint = resolvedBindPoint;
  const uint32_t insertIndex = FindInsertAfterLastDeclaration(
      program, D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED);
  program.Instructions.insert(program.Instructions.begin() +
                                  static_cast<ptrdiff_t>(insertIndex),
                              BuildUavDeclaration(resolvedDecl));
  if (!RecordNamedBinding(context.UavBindings, decl.Handle, resolvedBindPoint,
                          "uav", error)) {
    return false;
  }

  return true;
}

} // namespace dxp::sm5
