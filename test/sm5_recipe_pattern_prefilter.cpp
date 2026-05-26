#include "TestSupport.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Recipe.h"
#include "dxp/sm5/RecipeParse.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

static bool Contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

static bool ContainsDiagnostic(const dxp::sm5::RecipeContext &context,
                               const std::string &needle) {
  for (const auto &message : context.Diagnostics) {
    if (Contains(message, needle)) {
      return true;
    }
  }
  return false;
}

static dxp::sm5::RecipeMatchPattern MakeSingleMulPattern() {
  return dxp::sm5::RecipeMatchPattern{}.WithOpcode("mul");
}

static dxp::sm5::RecipeMatchPattern MakeFrcMulSequencePattern() {
  return dxp::sm5::RecipeMatchPattern{}
      .AddInstruction(dxp::sm5::RecipeInstructionPattern{}.WithOpcode("frc"))
      .AddInstruction(dxp::sm5::RecipeInstructionPattern{}.WithOpcode("mul"));
}

static dxp::sm5::RecipeMatchPattern MakeImpossibleMulPattern() {
  return dxp::sm5::RecipeMatchPattern{}
      .WithOpcode("mul")
      .AddOperand(dxp::sm5::RecipeOperandPattern{}.WithType("sampler"));
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_pattern_prefilter <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> inputBytes;
  if (!ReadFile(argv[1], inputBytes)) {
    std::cerr << "Failed to read input file: " << argv[1] << "\n";
    return 1;
  }

  dxp::sm5::Recipe optionalFailRecipe;
  optionalFailRecipe.AddStep(dxp::sm5::MakePrefilterStep(
      "required_single_mul",
      {dxp::sm5::MakePatternPrefilter(MakeSingleMulPattern())}));
  optionalFailRecipe.AddStep(dxp::sm5::MakePrefilterStep(
      "required_frc_mul_sequence",
      {dxp::sm5::MakePatternPrefilter(MakeFrcMulSequencePattern())}));
  optionalFailRecipe.AddStep(dxp::sm5::MakePrefilterStep(
      "optional_impossible",
      {dxp::sm5::MakePatternPrefilter(MakeImpossibleMulPattern())}));
  optionalFailRecipe.AddStep(
      dxp::sm5::MakeCustomRecipeStep(
          "execute_after_required", [](dxp::sm5::RecipeContext &context) {
            context.SetState<bool>("executed_after_required", true);
            return dxp::sm5::MakeRecipeStepSuccess();
          })
          .When(dxp::sm5::RecipeStepCondition::AllOf(
              {dxp::sm5::RecipeStepCondition::FromState("required_single_mul"),
               dxp::sm5::RecipeStepCondition::FromState(
                   "required_frc_mul_sequence")})));
  optionalFailRecipe.AddStep(
      dxp::sm5::MakeCustomRecipeStep(
          "execute_after_any", [](dxp::sm5::RecipeContext &context) {
            context.SetState<bool>("executed_after_any", true);
            return dxp::sm5::MakeRecipeStepSuccess();
          })
          .When(dxp::sm5::RecipeStepCondition::AnyOf(
              {dxp::sm5::RecipeStepCondition::FromState("optional_impossible"),
               dxp::sm5::RecipeStepCondition::FromState(
                   "required_single_mul")})));
  optionalFailRecipe.AddStep(
      dxp::sm5::MakeCustomRecipeStep(
          "execute_after_negated_optional",
          [](dxp::sm5::RecipeContext &context) {
            context.SetState<bool>("executed_after_negated_optional", true);
            return dxp::sm5::MakeRecipeStepSuccess();
          })
          .When(dxp::sm5::RecipeStepCondition::FromState(
              "optional_impossible", true)));
  optionalFailRecipe.AddStep(
      dxp::sm5::MakeCustomRecipeStep(
          "execute_after_callback", [](dxp::sm5::RecipeContext &context) {
            context.SetState<bool>("executed_after_callback", true);
            return dxp::sm5::MakeRecipeStepSuccess();
          })
          .When([](dxp::sm5::RecipeContext &context) {
            const bool *requiredSingleMul =
                context.FindState<bool>("required_single_mul");
            const bool *optionalImpossible =
                context.FindState<bool>("optional_impossible");
            return requiredSingleMul != nullptr && *requiredSingleMul &&
                   optionalImpossible != nullptr && !*optionalImpossible;
          }));
    optionalFailRecipe.AddStep(
      dxp::sm5::MakeCustomRecipeStep(
        "skip_after_optional_miss", [](dxp::sm5::RecipeContext &context) {
        return dxp::sm5::MakeRecipeStepFailure(
          context, "gated step executed unexpectedly");
        })
        .When(dxp::sm5::RecipeStepCondition::AllOf(
          {dxp::sm5::RecipeStepCondition::FromState("required_single_mul"),
           dxp::sm5::RecipeStepCondition::FromState(
             "optional_impossible")})));

  const auto optionalFailResult =
      dxp::sm5::PatchContainer(inputBytes, optionalFailRecipe);
  if (!optionalFailResult.Success) {
    std::cerr
        << "Expected optional-failure recipe to succeed, but patch failed: "
        << optionalFailResult.Error << "\n";
    return 1;
  }

  const bool *requiredSingleMul =
      optionalFailResult.RecipeContext.FindState<bool>("required_single_mul");
  const bool *requiredSequence = optionalFailResult.RecipeContext.FindState<bool>(
      "required_frc_mul_sequence");
  const bool *optionalImpossible =
      optionalFailResult.RecipeContext.FindState<bool>("optional_impossible");
  const bool *executedAfterRequired = optionalFailResult.RecipeContext.FindState<bool>(
      "executed_after_required");
  const bool *executedAfterAny = optionalFailResult.RecipeContext.FindState<bool>(
      "executed_after_any");
  const bool *executedAfterNegatedOptional =
      optionalFailResult.RecipeContext.FindState<bool>(
          "executed_after_negated_optional");
  const bool *executedAfterCallback =
      optionalFailResult.RecipeContext.FindState<bool>(
          "executed_after_callback");
  if (requiredSingleMul == nullptr || !*requiredSingleMul ||
      requiredSequence == nullptr || !*requiredSequence ||
      optionalImpossible == nullptr || *optionalImpossible ||
      executedAfterRequired == nullptr || !*executedAfterRequired ||
      executedAfterAny == nullptr || !*executedAfterAny ||
      executedAfterNegatedOptional == nullptr ||
      !*executedAfterNegatedOptional || executedAfterCallback == nullptr ||
      !*executedAfterCallback) {
    std::cerr << "Expected SM5 prefilters to publish boolean state for later "
                 "step guards.\n";
    return 1;
  }

  if (optionalFailResult.Report.Steps.size() != 8) {
    std::cerr << "Expected three probe steps plus five gated steps, but saw "
                 "step reports, but saw "
              << optionalFailResult.Report.Steps.size() << ".\n";
    return 1;
  }

  const dxp::PatchStepReport &optionalMissReport = optionalFailResult.Report.Steps[2];
  if (optionalMissReport.Name != "optional_impossible" ||
      !optionalMissReport.Executed || optionalMissReport.Skipped ||
      !optionalMissReport.Success || optionalMissReport.MatchCount != 0 ||
      optionalMissReport.Changed || !optionalMissReport.Error.empty()) {
    std::cerr << "Expected optional prefilter miss to be reported as a "
                 "successful executed probe step with zero matches.\n";
    return 1;
  }

    const dxp::PatchStepReport &skippedGateReport =
        optionalFailResult.Report.Steps[7];
  if (skippedGateReport.Name != "skip_after_optional_miss" ||
      skippedGateReport.Executed || !skippedGateReport.Skipped ||
      !skippedGateReport.Success || skippedGateReport.Changed ||
      skippedGateReport.MatchCount != 0 || !skippedGateReport.Error.empty()) {
    std::cerr << "Expected gated step after optional probe miss to be skipped.\n";
    return 1;
  }

  dxp::sm5::Recipe requiredFailRecipe;
  requiredFailRecipe.AddStep(dxp::sm5::MakePrefilterStep(
      "required_impossible",
      {dxp::sm5::MakePatternPrefilter(MakeImpossibleMulPattern())}));
    requiredFailRecipe.AddStep(
      dxp::sm5::MakeCustomRecipeStep(
        "should_not_execute_after_failed_probe",
        [](dxp::sm5::RecipeContext &context) {
        return dxp::sm5::MakeRecipeStepFailure(
          context, "guarded failure step executed unexpectedly");
        })
        .When(dxp::sm5::RecipeStepCondition::FromState(
          "required_impossible")));

  const auto requiredFailResult =
      dxp::sm5::PatchContainer(inputBytes, requiredFailRecipe);
  if (!requiredFailResult.Success) {
    std::cerr << "Expected guarded probe-miss recipe to succeed, but patch failed.\n";
    return 1;
  }

  if (requiredFailResult.Report.Steps.size() != 2) {
    std::cerr << "Expected probe plus skipped guarded step, but saw "
              << requiredFailResult.Report.Steps.size() << ".\n";
    return 1;
  }

  const dxp::PatchStepReport &requiredMissReport =
      requiredFailResult.Report.Steps.front();
  if (requiredMissReport.Name != "required_impossible" ||
      !requiredMissReport.Executed || requiredMissReport.Skipped ||
      !requiredMissReport.Success || requiredMissReport.MatchCount != 0 ||
      !requiredMissReport.Error.empty()) {
    std::cerr << "Expected guarded prefilter miss to remain a successful probe "
                 "step with zero matches.\n";
    return 1;
  }

  const dxp::PatchStepReport &guardedSkipReport =
      requiredFailResult.Report.Steps[1];
  if (guardedSkipReport.Name != "should_not_execute_after_failed_probe" ||
      guardedSkipReport.Executed || !guardedSkipReport.Skipped ||
      !guardedSkipReport.Success) {
    std::cerr << "Expected probe-gated failure step to be skipped.\n";
    return 1;
  }

  const char *yamlPrefilterRecipeText = R"YAML(version: 1
steps:
  - kind: prefilter
    name: yaml_sequence_probe
    checks:
      - kind: check_pattern_match
        match:
          sequence:
            - opcode: frc
            - opcode: mul
  - kind: apply_rules
    name: yaml_match_only_probe
    if:
      state: yaml_sequence_probe
    rules:
      - match:
          opcode: frc
          rewrite_mode: None
)YAML";

  dxp::sm5::RecipeParseResult yamlParseResult;
  if (!dxp::sm5::ParseRecipeText(yamlPrefilterRecipeText, yamlParseResult,
                                 "inline-sm5-yaml-prefilter-test")) {
    std::cerr << "Expected inline YAML prefilter recipe to parse, but saw: "
              << yamlParseResult.Error << "\n";
    return 1;
  }

  const auto yamlPrefilterResult =
      dxp::sm5::PatchContainer(inputBytes, yamlParseResult.Recipe);
  if (!yamlPrefilterResult.Success) {
    std::cerr << "Expected inline YAML prefilter recipe to patch, but saw: "
              << yamlPrefilterResult.Error << "\n";
    return 1;
  }

  const bool *yamlSequenceProbe =
      yamlPrefilterResult.RecipeContext.FindState<bool>("yaml_sequence_probe");
  if (yamlSequenceProbe == nullptr || !*yamlSequenceProbe) {
    std::cerr << "Expected YAML check_pattern_match prefilter to publish true "
                 "state for a present FRC/MUL sequence.\n";
    return 1;
  }

  if (yamlPrefilterResult.Report.Steps.size() != 2) {
    std::cerr << "Expected YAML prefilter recipe to report two steps, but saw "
              << yamlPrefilterResult.Report.Steps.size() << ".\n";
    return 1;
  }

  const dxp::PatchStepReport &yamlProbeReport =
      yamlPrefilterResult.Report.Steps.front();
  if (yamlProbeReport.Name != "yaml_sequence_probe" ||
      !yamlProbeReport.Executed || yamlProbeReport.Skipped ||
      !yamlProbeReport.Success || yamlProbeReport.MatchCount != 1) {
    std::cerr << "Expected YAML sequence prefilter to execute with one match.\n";
    return 1;
  }

  const dxp::PatchStepReport &yamlRuleReport = yamlPrefilterResult.Report.Steps[1];
  if (yamlRuleReport.Name != "yaml_match_only_probe" ||
      !yamlRuleReport.Executed || yamlRuleReport.Skipped ||
      !yamlRuleReport.Success || yamlRuleReport.MatchCount == 0 ||
      yamlPrefilterResult.RecipeContext.TotalRuleMatches == 0 ||
      yamlPrefilterResult.RecipeContext.ProgramModified) {
    std::cerr << "Expected YAML prefilter state to enable a match-only apply_rules "
                 "step without mutating the program.\n";
    return 1;
  }

  std::cout << "SM5 prefilter probes now publish context state and later steps "
               "run through generic if guards.\n";
  return 0;
}
