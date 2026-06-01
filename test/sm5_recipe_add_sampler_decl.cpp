#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <vector>

namespace {

static bool HasSamplerDecl(const dxp::sm5::ProgramInspection &program,
                           uint32_t bindPoint, uint32_t mode) {
  for (const auto &instruction : program.Instructions) {
    if (instruction.Opcode != D3D10_SB_OPCODE_DCL_SAMPLER) {
      continue;
    }

    if (instruction.Operands.empty() ||
        instruction.Operands.front().Indices.empty()) {
      continue;
    }

    if (instruction.Operands.front().Indices.front() == bindPoint &&
        !instruction.RawTokens.empty() &&
        DECODE_D3D10_SB_SAMPLER_MODE(instruction.RawTokens.front()) ==
            static_cast<D3D10_SB_SAMPLER_MODE>(mode)) {
      return true;
    }
  }

  return false;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_add_sampler_decl <input.ps_5_0.cso>\n";
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

  const size_t initialSamplerCount = inputProgram.SamplerBindPoints.size();

  const char *recipeText = R"YAML(version: 1
steps:
  - kind: add_sampler
    name: add_s11
    bind_point: 11
    sampler_mode: comparison

  - name: noop
    abort_on_failure: false
    rules: []
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-sampler-decl-test")) {
    std::cerr << "Failed to parse inline SM5 sampler recipe: "
              << parseResult.Error << "\n";
    return 1;
  }

  const auto patchResult =
      dxp::sm5::PatchContainer(inputBytes, parseResult.Recipe);
  if (!patchResult.Success) {
    std::cerr << "Failed to patch SM5 shader with sampler declaration recipe: "
              << patchResult.Error << "\n";
    return 1;
  }

  if (patchResult.Report.Steps.empty() ||
      patchResult.Report.Steps.front().SideEffects.size() != 1) {
    std::cerr << "Expected sampler-add step to report one side effect.\n";
    return 1;
  }

  const auto &stepReport = patchResult.Report.Steps.front();
  const auto &sideEffect = stepReport.SideEffects.front();
  if (sideEffect.Kind != dxp::PatchSideEffectKind::ResourceAdded ||
      sideEffect.ResourceKind != dxp::PatchResourceKind::Sampler ||
      sideEffect.StepName != stepReport.Name ||
      sideEffect.BindPoint != 11u || sideEffect.Space != 0u ||
      !sideEffect.Changed) {
    std::cerr << "Expected sampler-add side effect to describe added s11.\n";
    return 1;
  }

  const auto bindingIt = patchResult.Report.NewBindings.find("add_s11");
  if (bindingIt == patchResult.Report.NewBindings.end() ||
      bindingIt->second.Handle != "add_s11" ||
      bindingIt->second.ResourceKind != dxp::PatchResourceKind::Sampler ||
      bindingIt->second.BindPoint != 11u ||
      bindingIt->second.Space != 0u) {
    std::cerr << "Expected sampler-add report to expose binding s11 in "
                 "NewBindings.\n";
    return 1;
  }

  dxp::sm5::ProgramInspection patchedProgram;
  if (!dxp::sm5::InspectProgram(patchResult.OutputBytes, patchedProgram,
                                &inspectError)) {
    std::cerr << "Failed to inspect patched SM5 program: " << inspectError
              << "\n";
    return 1;
  }

  if (patchedProgram.SamplerBindPoints.size() != initialSamplerCount + 1) {
    std::cerr << "Expected one additional sampler declaration.\n";
    return 1;
  }

  if (!HasSamplerDecl(patchedProgram, 11u, D3D10_SB_SAMPLER_MODE_COMPARISON)) {
    std::cerr << "Expected patched shader to declare comparison sampler s11.\n";
    return 1;
  }

  std::cout << "SM5 recipe added comparison sampler declaration s11.\n";
  return 0;
}