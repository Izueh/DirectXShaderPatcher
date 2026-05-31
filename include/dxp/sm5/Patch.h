#pragma once

#include "dxp/PatchReport.h"
#include "Recipe.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace dxp::sm5 {

/// @brief Reports the result of patching an SM5 container.
struct PatchResult {
  bool Success = false;
  std::vector<uint8_t> OutputBytes;
  std::string Error;
  RecipeContext RecipeContext;
  /// Caller-facing patch report. Use Report.NewBindings for final runtime
  /// binding requirements and Report.OutputContainer for the emitted DXBC
  /// envelope. Report.Steps provides optional execution detail.
  dxp::PatchReport Report;
};

/// @brief Optional callbacks for mutating/observing context during execution.
struct RecipeExecutionOptions {
  std::function<void(const std::string &stepName, RecipeContext &context)>
      BeforeStep;
  std::function<void(const std::string &stepName,
                     const RecipeStepResult &stepResult,
                     RecipeContext &context)>
      AfterStep;
};

/// @brief Describes one operand in the inspection-friendly program view.
struct ProgramOperand {
  uint32_t Type = 0;
  uint32_t NumComponents = 0;
  uint32_t ComponentMode = 0;
  uint32_t Modifier = 0;
  std::vector<uint32_t> Indices;
  std::vector<uint32_t> ImmediateValues;
  std::vector<ProgramOperand> RelativeOperands;
};

/// @brief Describes one instruction in the inspection-friendly program view.
struct ProgramInstruction {
  uint32_t Opcode = 0;
  uint32_t LengthInDwords = 0;
  std::vector<uint32_t> RawTokens;
  std::vector<ProgramOperand> Operands;
  bool HasInputInterpolationMode = false;
  uint32_t InputInterpolationMode = 0;
};

/// @brief Summarizes decoded program state for lightweight inspection.
struct ProgramInspection {
  uint32_t TempCount = 0;
  std::vector<ProgramInstruction> Instructions;
  std::vector<uint32_t> ResourceBindPoints;
  std::vector<uint32_t> CBufferBindPoints;
  std::vector<uint32_t> SamplerBindPoints;
};

/// @brief Applies an SM5 recipe to a container byte vector.
/// @param inputContainer Input DXBC container bytes.
/// @param recipe Recipe to execute.
/// @param context Initial recipe execution context.
/// @return The patch result, including output bytes, final binding requirements,
/// and execution diagnostics.
PatchResult PatchContainer(const std::vector<uint8_t> &inputContainer,
                           const Recipe &recipe,
                           const RecipeContext &context = {});

/// @brief Applies an SM5 recipe with a mutable execution context and hooks.
PatchResult PatchContainer(const std::vector<uint8_t> &inputContainer,
                           const Recipe &recipe, RecipeContext &context,
                           const RecipeExecutionOptions &execution = {});

/// @brief Applies an SM5 recipe to an in-memory container buffer.
/// @param recipe Recipe to execute.
/// @param inputData Pointer to the input container bytes.
/// @param inputSize Size of the input container in bytes.
/// @param context Initial recipe execution context.
/// @return The patch result, including output bytes, final binding requirements,
/// and execution diagnostics.
PatchResult PatchContainer(const Recipe &recipe, const uint8_t *inputData,
                           size_t inputSize, const RecipeContext &context = {});

/// @brief Applies an SM5 recipe from memory with mutable context and hooks.
PatchResult PatchContainer(const Recipe &recipe, const uint8_t *inputData,
                           size_t inputSize, RecipeContext &context,
                           const RecipeExecutionOptions &execution = {});

/// @brief Extracts opcode values from a container byte vector.
/// @param inputContainer Input DXBC container bytes.
/// @param opcodes Receives the opcode sequence.
/// @param error Receives an error message when extraction fails.
/// @return `true` on success.
bool ExtractProgramOpcodes(const std::vector<uint8_t> &inputContainer,
                           std::vector<uint32_t> &opcodes,
                           std::string *error = nullptr);

/// @brief Extracts opcode values from an in-memory container buffer.
/// @param inputData Pointer to the input container bytes.
/// @param inputSize Size of the input container in bytes.
/// @param opcodes Receives the opcode sequence.
/// @param error Receives an error message when extraction fails.
/// @return `true` on success.
bool ExtractProgramOpcodes(const uint8_t *inputData, size_t inputSize,
                           std::vector<uint32_t> &opcodes,
                           std::string *error = nullptr);

/// @brief Decodes inspection data from a container byte vector.
/// @param inputContainer Input DXBC container bytes.
/// @param inspection Receives the extracted inspection summary.
/// @param error Receives an error message when inspection fails.
/// @return `true` on success.
bool InspectProgram(const std::vector<uint8_t> &inputContainer,
                    ProgramInspection &inspection,
                    std::string *error = nullptr);

/// @brief Decodes inspection data from an in-memory container buffer.
/// @param inputData Pointer to the input container bytes.
/// @param inputSize Size of the input container in bytes.
/// @param inspection Receives the extracted inspection summary.
/// @param error Receives an error message when inspection fails.
/// @return `true` on success.
bool InspectProgram(const uint8_t *inputData, size_t inputSize,
                    ProgramInspection &inspection,
                    std::string *error = nullptr);

} // namespace dxp::sm5
