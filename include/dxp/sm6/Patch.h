#pragma once

#include "../PatchReport.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "Recipe.h"
#include "RecipeParse.h"

/// @brief Loads a DXIL container from an in-memory buffer.
/// @param containerData Pointer to the container bytes.
/// @param containerSize Size of the container in bytes.
/// @param shader Receives the loaded shader state.
/// @param restoreReflection Whether original reflection metadata should be
/// restored.
/// @return `true` on success.
bool LoadDxilContainer(const void *containerData, size_t containerSize,
                       DxilLoadedShaderState &shader,
                       bool restoreReflection = true);

/// @brief Loads a DXIL container from a byte vector.
/// @param containerBytes Container bytes to load.
/// @param shader Receives the loaded shader state.
/// @param restoreReflection Whether original reflection metadata should be
/// restored.
/// @return `true` on success.
bool LoadDxilContainer(const std::vector<uint8_t> &containerBytes,
                       DxilLoadedShaderState &shader,
                       bool restoreReflection = true);

/// @brief Rebuilds LLVM module state from container bytes.
bool ReloadDxilContainerFromMemory(const std::vector<uint8_t> &containerBytes,
                                   llvm::LLVMContext &context,
                                   std::unique_ptr<llvm::Module> &module,
                                   hlsl::DxilModule *&dxilModule);

/// @brief Applies a DXIL recipe to an in-memory container buffer.
/// @param outReport Optional caller-facing patch report. Read
/// `outReport->NewBindings` for final runtime binding requirements and
/// `outReport->OutputContainer` for the emitted DXIL envelope.
bool PatchDxilContainer(const DxilRecipe &recipe, const void *inputData,
                        size_t inputSize, std::vector<uint8_t> &outputContainer,
                        const DxilContainerPatchOptions &options = {},
                        DxilRecipeContext *outContext = nullptr,
                        dxp::PatchReport *outReport = nullptr);

/// @brief Applies a DXIL recipe to a container byte vector.
/// @param outReport Optional caller-facing patch report. Read
/// `outReport->NewBindings` for final runtime binding requirements and
/// `outReport->OutputContainer` for the emitted DXIL envelope.
bool PatchDxilContainer(const DxilRecipe &recipe,
                        const std::vector<uint8_t> &inputContainer,
                        std::vector<uint8_t> &outputContainer,
                        const DxilContainerPatchOptions &options = {},
                        DxilRecipeContext *outContext = nullptr,
                        dxp::PatchReport *outReport = nullptr);

/// @brief Refreshes derived DXIL module state after IR mutation.
void RefreshDxilModule(hlsl::DxilModule &dxilModule, bool traceEnabled = false);

/// @brief Serializes an LLVM module to bitcode.
std::vector<uint8_t> SerializeModuleToBitcode(llvm::Module &module);

/// @brief Rebuilds a patched container from DXIL module state and bitcode.
bool SerializePatchedContainer(hlsl::DxilModule &dxilModule,
                               const std::vector<uint8_t> &moduleBitcode,
                               std::vector<uint8_t> &outputContainer);

/// @brief Restores resource-reflection metadata from the original container.
void RestoreOriginalResourceReflection(const std::vector<uint8_t> &inputBytes,
                                       hlsl::DxilModule &targetDxilModule,
                                       llvm::LLVMContext &reflectionContext);