#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <atlbase.h>

#include "d3d11TokenizedProgramFormat.hpp"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilResource.h"
#include "dxp/sm6/Patch.h"
#include "dxp/sm6/Recipe.h"
#include "dxp/sm6/Resources.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

class ScopedCoInitialize {
public:
  ScopedCoInitialize();
  ~ScopedCoInitialize();

  bool IsInitialized() const { return initialized_; }

private:
  bool initialized_ = false;
};

struct DxilProgramBitcode {
  const uint8_t *ptr = nullptr;
  uint32_t size = 0;
};

struct LoadedDxilShader {
  std::vector<uint8_t> inputBytes;
  llvm::LLVMContext context;
  std::unique_ptr<llvm::LLVMContext> reflectionContext;
  std::unique_ptr<llvm::Module> module;
  hlsl::DxilModule *dxilModule = nullptr;

  ~LoadedDxilShader();
};

bool ReadFile(const std::string &path, std::vector<uint8_t> &data);
bool WriteFile(const std::string &path, const void *ptr, size_t size);
std::filesystem::path RepoRootPath();
std::string DefaultArtifactOutputPath(const std::string &inputPath,
                                      const std::string &suffix);
bool ExtractDxilProgramBitcode(const std::vector<uint8_t> &containerBytes,
                               DxilProgramBitcode &outBitcode);
std::unique_ptr<llvm::Module>
ParseDxilBitcode(const uint8_t *ptr, uint32_t size, llvm::LLVMContext &context);
bool LoadDxilState(llvm::Module &module, hlsl::DxilModule *&outDxilModule);
bool LoadShaderFromPath(const std::string &inputPath, LoadedDxilShader &shader,
                        bool restoreReflection = true);
bool VerifyModuleOrReport(llvm::Module &module);
bool ReloadPatchedContainer(const std::vector<uint8_t> &containerBytes,
                            llvm::LLVMContext &context,
                            std::unique_ptr<llvm::Module> &module,
                            hlsl::DxilModule *&dxilModule);
bool FindSrvByName(const hlsl::DxilModule &dxilModule, const std::string &name,
                   const hlsl::DxilResource **resourceOut);
bool FindCBufferByName(const hlsl::DxilModule &dxilModule,
                       const std::string &name,
                       const hlsl::DxilCBuffer **cbufferOut);
bool HasTypedHandleDxilOpOverloads(const llvm::Module &module);
unsigned CountDxOpCalls(const llvm::Function &function,
                        llvm::StringRef functionName);
unsigned CountIgnNoiseChains(llvm::Function &function);
unsigned CountBlueNoiseTextureLoads(llvm::Function &function,
                                    hlsl::DxilModule &dxilModule);
bool ReplaceIgnNoiseInComputeShaderWithTextureLoad(
    llvm::Module &module, hlsl::DxilModule &dxilModule,
    const TextureResourceDesc &textureDesc,
    const CBufferDesc &frameIndexCBufferDesc, bool traceEnabled = false);
bool ApplyComputeNoiseRewriteUsingRules(
    llvm::Module &module, hlsl::DxilModule &dxilModule,
    const TextureResourceDesc &textureDesc,
    const CBufferDesc &frameIndexCBufferDesc, bool traceEnabled = false);
DxilRecipeStep MakeExpectIgnCountStep(unsigned expectedCount,
                                      std::string name = "expect_ign_count");
DxilRecipeStep
MakeExpectBlueNoiseCountStep(unsigned expectedCount,
                             std::string name = "expect_blue_noise_count");
DxilRecipeStep MakeApplyComputeNoiseRewriteRulesStep(std::string name,
                                                     std::string textureId,
                                                     std::string cbufferId,
                                                     bool required = true);
