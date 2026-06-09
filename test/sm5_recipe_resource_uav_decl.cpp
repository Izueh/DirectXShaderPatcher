#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <filesystem>
#include <iostream>
#include <unordered_set>
#include <vector>

namespace {

static int CountOpcode(const dxp::sm5::ProgramInspection &program,
                       uint32_t opcode) {
  int count = 0;
  for (const auto &instruction : program.Instructions) {
    if (instruction.Opcode == opcode) {
      ++count;
    }
  }
  return count;
}

static bool HasBindPoint(const std::unordered_set<uint32_t> &values,
                         uint32_t bindPoint) {
  return values.find(bindPoint) != values.end();
}

static std::unordered_set<uint32_t>
CollectResourceBindPoints(const dxp::sm5::ProgramInspection &program) {
  std::unordered_set<uint32_t> bindPoints;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = instruction.Opcode;
    if (opcode != D3D10_SB_OPCODE_DCL_RESOURCE &&
        opcode != D3D11_SB_OPCODE_DCL_RESOURCE_RAW &&
        opcode != D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED) {
      continue;
    }

    if (!instruction.Operands.empty() &&
        !instruction.Operands.front().Indices.empty()) {
      bindPoints.insert(instruction.Operands.front().Indices.front());
    }
  }
  return bindPoints;
}

static std::unordered_set<uint32_t>
CollectUavBindPoints(const dxp::sm5::ProgramInspection &program) {
  std::unordered_set<uint32_t> bindPoints;
  for (const auto &instruction : program.Instructions) {
    const auto opcode = instruction.Opcode;
    if (opcode != D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_TYPED &&
        opcode != D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW &&
        opcode != D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_STRUCTURED) {
      continue;
    }

    if (!instruction.Operands.empty() &&
        !instruction.Operands.front().Indices.empty()) {
      bindPoints.insert(instruction.Operands.front().Indices.front());
    }
  }
  return bindPoints;
}

static bool HasMovUsingResource(const dxp::sm5::ProgramInspection &program,
                                uint32_t bindPoint) {
  for (const auto &instruction : program.Instructions) {
    if (instruction.Opcode != D3D10_SB_OPCODE_MOV ||
        instruction.Operands.size() < 2) {
      continue;
    }

    const auto &src = instruction.Operands[1];
    if (src.Type == D3D10_SB_OPERAND_TYPE_RESOURCE && !src.Indices.empty() &&
        src.Indices.front() == bindPoint) {
      return true;
    }
  }
  return false;
}

static bool HasMovUsingUav(const dxp::sm5::ProgramInspection &program,
                           uint32_t bindPoint) {
  for (const auto &instruction : program.Instructions) {
    if (instruction.Opcode != D3D10_SB_OPCODE_MOV ||
        instruction.Operands.size() < 2) {
      continue;
    }

    const auto &src = instruction.Operands[1];
    if (src.Type == D3D11_SB_OPERAND_TYPE_UNORDERED_ACCESS_VIEW &&
        !src.Indices.empty() && src.Indices.front() == bindPoint) {
      return true;
    }
  }
  return false;
}

}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_resource_uav_decl <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> inputBytes;
  if (!ReadFile(argv[1], inputBytes)) {
    std::cerr << "Failed to read input file: " << argv[1] << "\n";
    return 1;
  }

  dxp::sm5::ProgramInspection inputProgram;
  std::string inspectError;
  if (!dxp::sm5::InspectProgram(inputBytes, inputProgram, &inspectError)) {
    std::cerr << "Failed to inspect input SM5 program: " << inspectError
              << "\n";
    return 1;
  }

  const auto initialResourceBindPoints =
      CollectResourceBindPoints(inputProgram);
  const auto initialUavBindPoints = CollectUavBindPoints(inputProgram);
  const int initialRawSrvCount =
      CountOpcode(inputProgram, D3D11_SB_OPCODE_DCL_RESOURCE_RAW);
  const int initialStructuredSrvCount =
      CountOpcode(inputProgram, D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED);
  const int initialRawUavCount =
      CountOpcode(inputProgram, D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW);

  dxp::sm5::RecipeParseResult parseResult;
  const std::filesystem::path recipePath =
      RepoRootPath() / "recipes" / "sm5_resource_uav_decl.recipe.yml";
  if (!dxp::sm5::ParseRecipeFile(recipePath.string(), parseResult)) {
    std::cerr << "Failed to parse SM5 recipe file: " << parseResult.Error
              << "\n";
    return 1;
  }

  const auto patchResult =
      dxp::sm5::PatchContainer(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader: " << patchResult.Error << "\n";
    return 1;
  }

  const auto rawSrvIt =
      patchResult.RecipeContext.RawResourceBindings.find("injected_raw_srv");
  const auto structuredSrvIt =
      patchResult.RecipeContext.StructuredResourceBindings.find(
          "injected_structured_srv");
  const auto rawUavIt =
      patchResult.RecipeContext.UavBindings.find("injected_raw_uav");
  if (rawSrvIt == patchResult.RecipeContext.RawResourceBindings.end() ||
      structuredSrvIt ==
          patchResult.RecipeContext.StructuredResourceBindings.end() ||
      rawUavIt == patchResult.RecipeContext.UavBindings.end()) {
    std::cerr << "Expected injected declaration handles to be resolved in "
                 "recipe context.\n";
    return 1;
  }

  if (HasBindPoint(initialResourceBindPoints, rawSrvIt->second) ||
      HasBindPoint(initialResourceBindPoints, structuredSrvIt->second)) {
    std::cerr << "Expected auto-bind to choose free SRV slots for "
                 "raw/structured resources.\n";
    return 1;
  }

  if (HasBindPoint(initialUavBindPoints, rawUavIt->second)) {
    std::cerr << "Expected auto-bind to choose a free UAV slot.\n";
    return 1;
  }

    const auto rawSrvBindingIt =
      patchResult.Report.NewBindings.find("injected_raw_srv");
    const auto structuredSrvBindingIt =
      patchResult.Report.NewBindings.find("injected_structured_srv");
    const auto rawUavBindingIt =
      patchResult.Report.NewBindings.find("injected_raw_uav");
    if (rawSrvBindingIt == patchResult.Report.NewBindings.end() ||
      structuredSrvBindingIt == patchResult.Report.NewBindings.end() ||
      rawUavBindingIt == patchResult.Report.NewBindings.end()) {
    std::cerr << "Expected injected declaration handles to be exported in the "
                 "patch report.\n";
    return 1;
  }

    if (rawSrvBindingIt->second.ResourceKind !=
        dxp::PatchResourceKind::RawResource ||
      rawSrvBindingIt->second.Handle != "injected_raw_srv" ||
      rawSrvBindingIt->second.BindPoint != rawSrvIt->second ||
      rawSrvBindingIt->second.Space != 0u) {
    std::cerr << "Expected raw SRV export to expose the resolved binding.\n";
    return 1;
  }

    if (structuredSrvBindingIt->second.ResourceKind !=
        dxp::PatchResourceKind::StructuredResource ||
      structuredSrvBindingIt->second.Handle !=
          "injected_structured_srv" ||
      structuredSrvBindingIt->second.BindPoint != structuredSrvIt->second ||
      structuredSrvBindingIt->second.Space != 0u) {
    std::cerr << "Expected structured SRV export to expose the resolved "
                 "binding.\n";
    return 1;
  }

    if (rawUavBindingIt->second.ResourceKind !=
        dxp::PatchResourceKind::Uav ||
      rawUavBindingIt->second.Handle != "injected_raw_uav" ||
      rawUavBindingIt->second.BindPoint != rawUavIt->second ||
      rawUavBindingIt->second.Space != 0u) {
    std::cerr << "Expected raw UAV export to expose the resolved binding.\n";
    return 1;
  }

  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Failed to inspect patched SM5 program: " << inspectError
              << "\n";
    return 1;
  }

  if (CountOpcode(patchedProgram, D3D11_SB_OPCODE_DCL_RESOURCE_RAW) <
      initialRawSrvCount + 1) {
    std::cerr << "Expected one additional raw SRV declaration.\n";
    return 1;
  }

  if (CountOpcode(patchedProgram, D3D11_SB_OPCODE_DCL_RESOURCE_STRUCTURED) <
      initialStructuredSrvCount + 1) {
    std::cerr << "Expected one additional structured SRV declaration.\n";
    return 1;
  }

  if (CountOpcode(patchedProgram,
                  D3D11_SB_OPCODE_DCL_UNORDERED_ACCESS_VIEW_RAW) <
      initialRawUavCount + 1) {
    std::cerr << "Expected one additional raw UAV declaration.\n";
    return 1;
  }

  if (!HasMovUsingResource(patchedProgram, rawSrvIt->second)) {
    std::cerr << "Expected rewrite emit to resolve raw SRV from_handle in MOV "
                 "source.\n";
    return 1;
  }

  if (!HasMovUsingUav(patchedProgram, rawUavIt->second)) {
    std::cerr
        << "Expected rewrite emit to resolve UAV from_handle in MOV source.\n";
    return 1;
  }

  std::cout << "SM5 recipe injected raw/structured resources and raw UAV with "
               "auto-bind and from_handle resolution.\n";
  return 0;
}
