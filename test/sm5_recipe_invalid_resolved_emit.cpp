#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

static bool Contains(const std::string &text, const std::string &needle) {
  return text.find(needle) != std::string::npos;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr
        << "Usage: sm5_recipe_invalid_resolved_emit <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> inputBytes;
  if (!ReadFile(argv[1], inputBytes)) {
    std::cerr << "Failed to read input file: " << argv[1] << "\n";
    return 1;
  }

  const char *recipeText = R"YAML(version: 1
steps:
  - name: invalid_resolved_emit
    rules:
      - match:
          opcode: frc
          operands:
            - capture: dst
            - capture: src
        emit:
          - opcode: mov
            operands:
              - capture: dst
              - type: temp
                indices:
                  - representation: immediate32_plus_relative
                    immediate_lo: 0
)YAML";

  dxp::sm5::RecipeParseResult parseResult;
  if (!dxp::sm5::ParseRecipeText(recipeText, parseResult,
                                 "inline-sm5-invalid-resolved-emit-test")) {
    std::cerr << "Expected parse to succeed for runtime validation test, got: "
              << parseResult.Error << "\n";
    return 1;
  }

  const auto patchResult = dxp::sm5::PatchContainer(inputBytes, parseResult.Recipe);
  if (patchResult.Success) {
    std::cerr << "Expected patch to fail due to invalid resolved emit structure.\n";
    return 1;
  }

    if (!Contains(patchResult.Error, "immediate32_plus_relative") ||
        !Contains(patchResult.Error, "step[") ||
        !Contains(patchResult.Error, ".rule[") ||
        !Contains(patchResult.Error, ".match[") ||
        !Contains(patchResult.Error, ".emit[0]")) {
    std::cerr
          << "Expected structural validation error with step/rule/match/emit "
             "path context and "
             "immediate32_plus_relative, got: "
        << patchResult.Error << "\n";
    return 1;
  }

  std::cout << "SM5 runtime validator rejected invalid resolved emit as expected.\n";
  return 0;
}
