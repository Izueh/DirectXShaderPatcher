#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "Recipe.h"

struct DxilLoadedShaderState {
  std::vector<uint8_t> inputBytes;
  llvm::LLVMContext context;
  std::unique_ptr<llvm::LLVMContext> reflectionContext;
  std::unique_ptr<llvm::Module> module;
  hlsl::DxilModule *dxilModule = nullptr;

  ~DxilLoadedShaderState();
};

struct DxilContainerPatchOptions {
  bool restoreReflection = true;
  DxilRecipeExecutionOptions recipeExecutionOptions;
};

struct DxilRecipeParseResult {
  DxilRecipe recipe;
  DxilContainerPatchOptions patchOptions;
  std::string error;
};

bool LoadDxilContainerForMutation(const void *containerData,
                                  size_t containerSize,
                                  DxilLoadedShaderState &shader,
                                  bool restoreReflection = true);
bool LoadDxilContainerForMutation(const std::vector<uint8_t> &containerBytes,
                                  DxilLoadedShaderState &shader,
                                  bool restoreReflection = true);
bool ReloadDxilContainerFromMemory(const std::vector<uint8_t> &containerBytes,
                                   llvm::LLVMContext &context,
                                   std::unique_ptr<llvm::Module> &module,
                                   hlsl::DxilModule *&dxilModule);
bool PatchDxilContainerInMemory(const DxilRecipe &recipe, const void *inputData,
                                size_t inputSize,
                                std::vector<uint8_t> &outputContainer,
                                const DxilContainerPatchOptions &options = {},
                                DxilRecipeContext *outContext = nullptr);
bool PatchDxilContainerInMemory(const DxilRecipe &recipe,
                                const std::vector<uint8_t> &inputContainer,
                                std::vector<uint8_t> &outputContainer,
                                const DxilContainerPatchOptions &options = {},
                                DxilRecipeContext *outContext = nullptr);
bool ParseDxilRecipeText(llvm::StringRef recipeText,
                         DxilRecipeParseResult &result,
                         llvm::StringRef sourceName = "recipe");
bool ParseDxilRecipeFile(const std::string &recipePath,
                         DxilRecipeParseResult &result);
void RefreshDxilAfterResourceMutation(hlsl::DxilModule &dxilModule,
                                      bool traceEnabled = false);
std::vector<uint8_t> SerializeModuleToBitcode(llvm::Module &module);
bool SerializePatchedContainer(hlsl::DxilModule &dxilModule,
                               const std::vector<uint8_t> &moduleBitcode,
                               std::vector<uint8_t> &outputContainer);
void RestoreOriginalResourceReflection(const std::vector<uint8_t> &inputBytes,
                                       hlsl::DxilModule &targetDxilModule,
                                       llvm::LLVMContext &reflectionContext);