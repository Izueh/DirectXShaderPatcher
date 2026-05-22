#include "TestSupport.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/MemoryBuffer.h"

#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilContainerReader.h"

unsigned CountIgnNoiseChains(llvm::Function &function);
unsigned CountBlueNoiseTextureLoads(llvm::Function &function,
                  hlsl::DxilModule &dxilModule);
bool ReplaceIgnNoiseInComputeShaderWithTextureLoad(
  llvm::Module &module,
  hlsl::DxilModule &dxilModule,
  const TextureResourceDesc &textureDesc,
  const CBufferDesc &frameIndexCBufferDesc,
  bool traceEnabled);
bool ReplaceIgnNoiseInComputeShaderWithTextureLoadUsingRules(
  llvm::Module &module,
  hlsl::DxilModule &dxilModule,
  const TextureResourceDesc &textureDesc,
  const CBufferDesc &frameIndexCBufferDesc,
  bool traceEnabled);

namespace {

static void ReleaseDxilStateForModule(std::unique_ptr<llvm::Module> &module,
                                      hlsl::DxilModule *&dxilModule) {
  if (!module) {
    dxilModule = nullptr;
    return;
  }

  if (module->HasDxilModule()) {
    module->pfnRemoveGlobal = nullptr;
    module->pfnResetDxilModule = nullptr;
    module->SetDxilModule(nullptr);
  }

  dxilModule = nullptr;
  module.reset();
}

static bool ExtractProgramBitcodeFromContainerPart(
    const std::vector<uint8_t> &container,
    hlsl::DxilFourCC partKind,
    DxilProgramBitcode &out) {
  hlsl::DxilContainerReader reader;
  if (FAILED(reader.Load(container.data(), container.size()))) {
    std::cerr << "Failed to parse DXIL container header.\n";
    return false;
  }

  uint32_t partIndex = hlsl::DXIL_CONTAINER_BLOB_NOT_FOUND;
  if (FAILED(reader.FindFirstPartKind(partKind, &partIndex)) ||
      partIndex == hlsl::DXIL_CONTAINER_BLOB_NOT_FOUND) {
    return false;
  }

  const void *partData = nullptr;
  uint32_t partSize = 0;
  if (FAILED(reader.GetPartContent(partIndex, &partData, &partSize)) ||
      !partData || partSize < sizeof(hlsl::DxilProgramHeader)) {
    std::cerr << "DXIL container part is missing or malformed.\n";
    return false;
  }

  const hlsl::DxilProgramHeader *programHeader =
      reinterpret_cast<const hlsl::DxilProgramHeader *>(partData);
  if (!hlsl::IsValidDxilProgramHeader(programHeader, partSize)) {
    std::cerr << "DXIL program header validation failed for container part.\n";
    return false;
  }

  out.ptr = reinterpret_cast<const uint8_t *>(
      hlsl::GetDxilBitcodeData(programHeader));
  out.size = hlsl::GetDxilBitcodeSize(programHeader);
  return true;
}

} // namespace

LoadedDxilShader::~LoadedDxilShader() {
  ReleaseDxilStateForModule(module, dxilModule);
}

ScopedCoInitialize::ScopedCoInitialize() {
  HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  initialized_ = SUCCEEDED(result);
}

ScopedCoInitialize::~ScopedCoInitialize() {
  if (initialized_)
    CoUninitialize();
}

bool ReadFile(const std::string &path, std::vector<uint8_t> &data) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return false;

  file.seekg(0, std::ios::end);
  std::streamoff size = file.tellg();
  if (size < 0)
    return false;
  file.seekg(0, std::ios::beg);

  data.resize(static_cast<size_t>(size));
  if (size > 0)
    file.read(reinterpret_cast<char *>(data.data()), size);

  return !!file;
}

bool WriteFile(const std::string &path, const void *ptr, size_t size) {
  const std::filesystem::path outputPath(path);
  const std::filesystem::path parentPath = outputPath.parent_path();
  if (!parentPath.empty()) {
    std::error_code error;
    if (!std::filesystem::create_directories(parentPath, error) && error)
      return false;
  }

  std::ofstream file(path, std::ios::binary);
  if (!file)
    return false;

  if (size > 0) {
    file.write(reinterpret_cast<const char *>(ptr),
               static_cast<std::streamsize>(size));
  }

  return !!file;
}

std::filesystem::path RepoRootPath() {
  return std::filesystem::path(__FILE__).parent_path().parent_path();
}

std::string DefaultArtifactOutputPath(const std::string &inputPath,
                                      const std::string &suffix) {
  std::filesystem::path inputFile(inputPath);
  const std::filesystem::path stem = inputFile.stem();
  return (RepoRootPath() / "artifacts" / "test-output" /
          (stem.string() + suffix))
      .string();
}

bool ExtractDxilProgramBitcode(const std::vector<uint8_t> &containerBytes,
                               DxilProgramBitcode &outBitcode) {
  if (!ExtractProgramBitcodeFromContainerPart(containerBytes,
                                              hlsl::DFCC_DXIL,
                                              outBitcode)) {
    std::cerr << "DXIL part not found in container.\n";
    return false;
  }

  return true;
}

std::unique_ptr<llvm::Module>
ParseDxilBitcode(const uint8_t *ptr,
                 uint32_t size,
                 llvm::LLVMContext &context) {
  auto modOrErr = llvm::parseBitcodeFile(
      llvm::MemoryBufferRef(
          llvm::StringRef(reinterpret_cast<const char *>(ptr), size),
          "dxil-program"),
      context);

  if (!modOrErr) {
    std::cerr << "Failed to parse DXIL bitcode: "
              << modOrErr.getError().message() << "\n";
    return nullptr;
  }

  return std::move(modOrErr.get());
}

bool LoadDxilState(llvm::Module &module, hlsl::DxilModule *&outDxilModule) {
  hlsl::DxilModule &dxilModule = module.GetOrCreateDxilModule();
  outDxilModule = &dxilModule;

  if (dxilModule.HasMetadataErrors())
    std::cerr << "DXIL metadata load reported non-fatal errors.\n";

  return true;
}

bool LoadShaderForMutation(const std::string &inputPath,
                           LoadedDxilShader &shader,
                           bool restoreReflection) {
  if (!ReadFile(inputPath, shader.inputBytes)) {
    std::cerr << "Failed to read input shader: " << inputPath << "\n";
    return false;
  }

  DxilProgramBitcode dxilBitcode;
  if (!ExtractDxilProgramBitcode(shader.inputBytes, dxilBitcode)) {
    std::cerr << "Failed to extract DXIL program bitcode.\n";
    return false;
  }

  shader.module = ParseDxilBitcode(dxilBitcode.ptr, dxilBitcode.size, shader.context);
  if (!shader.module) {
    std::cerr << "Failed to parse DXIL bitcode.\n";
    return false;
  }

  if (!LoadDxilState(*shader.module, shader.dxilModule) ||
      shader.dxilModule == nullptr) {
    std::cerr << "Failed to load DxilModule state.\n";
    return false;
  }

  if (restoreReflection) {
    shader.reflectionContext = std::make_unique<llvm::LLVMContext>();
    RestoreOriginalResourceReflection(shader.inputBytes,
                                      *shader.dxilModule,
                                      *shader.reflectionContext);
  } else {
    shader.reflectionContext.reset();
  }

  return true;
}

bool VerifyModuleOrReport(llvm::Module &module) {
  std::string verificationErrors;
  llvm::raw_string_ostream verificationStream(verificationErrors);
  if (!llvm::verifyModule(module, &verificationStream))
    return true;

  verificationStream.flush();
  std::cerr << "Patched module failed LLVM verification.\n";
  if (!verificationErrors.empty())
    std::cerr << verificationErrors;
  return false;
}

bool ReloadPatchedContainer(const std::vector<uint8_t> &containerBytes,
                            llvm::LLVMContext &context,
                            std::unique_ptr<llvm::Module> &module,
                            hlsl::DxilModule *&dxilModule) {
  DxilProgramBitcode patchedDxilBitcode;
  if (!ExtractDxilProgramBitcode(containerBytes, patchedDxilBitcode)) {
    std::cerr << "Failed to extract DXIL bitcode from patched container.\n";
    return false;
  }

  module = ParseDxilBitcode(patchedDxilBitcode.ptr,
                            patchedDxilBitcode.size,
                            context);
  if (!module) {
    std::cerr << "Failed to parse patched DXIL bitcode.\n";
    return false;
  }

  if (!LoadDxilState(*module, dxilModule) || dxilModule == nullptr) {
    std::cerr << "Failed to load DxilModule from patched container.\n";
    return false;
  }

  return true;
}

bool FindSrvByName(const hlsl::DxilModule &dxilModule,
                   const std::string &name,
                   const hlsl::DxilResource **resourceOut) {
  for (const auto &srv : dxilModule.GetSRVs()) {
    if (srv->GetGlobalName() == name) {
      if (resourceOut != nullptr)
        *resourceOut = srv.get();
      return true;
    }
  }

  if (resourceOut != nullptr)
    *resourceOut = nullptr;
  return false;
}

bool FindCBufferByName(const hlsl::DxilModule &dxilModule,
                       const std::string &name,
                       const hlsl::DxilCBuffer **cbufferOut) {
  for (const auto &cbuffer : dxilModule.GetCBuffers()) {
    if (cbuffer->GetGlobalName() == name) {
      if (cbufferOut != nullptr)
        *cbufferOut = cbuffer.get();
      return true;
    }
  }

  if (cbufferOut != nullptr)
    *cbufferOut = nullptr;
  return false;
}

bool HasTypedHandleDxilOpOverloads(const llvm::Module &module) {
  for (const llvm::Function &function : module) {
    const llvm::StringRef name = function.getName();
    if (name == "dx.op.createHandleFromBinding.dx.types.Handle" ||
        name == "dx.op.annotateHandle.dx.types.Handle") {
      return true;
    }
  }

  return false;
}

unsigned CountDxOpCalls(const llvm::Function &function,
                        llvm::StringRef functionName) {
  unsigned count = 0;
  for (const llvm::BasicBlock &basicBlock : function) {
    for (const llvm::Instruction &instruction : basicBlock) {
      const llvm::CallInst *call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      const llvm::Function *callee = call != nullptr ? call->getCalledFunction()
                                                     : nullptr;
      if (callee != nullptr && callee->getName() == functionName)
        ++count;
    }
  }

  return count;
}

bool ApplyComputeNoiseRewriteUsingRules(llvm::Module &module,
                                        hlsl::DxilModule &dxilModule,
                                        const TextureResourceDesc &textureDesc,
                                        const CBufferDesc &frameIndexCBufferDesc,
                                        bool traceEnabled) {
  return ReplaceIgnNoiseInComputeShaderWithTextureLoadUsingRules(
      module,
      dxilModule,
      textureDesc,
      frameIndexCBufferDesc,
      traceEnabled);
}

DxilRecipeStep MakeExpectIgnCountStep(unsigned expectedCount,
                                      std::string name) {
  return MakeCustomRecipeStep(
      std::move(name),
      [expectedCount](DxilRecipeContext &context) -> DxilRecipeStepResult {
        if (context.dxilModule == nullptr) {
          return MakeRecipeStepFailure(
              context,
              "expect_ign_count: recipe context is missing DXIL module");
        }

        context.entryFunction = context.dxilModule->GetEntryFunction();
        if (context.entryFunction == nullptr) {
          return MakeRecipeStepFailure(
              context,
              "expect_ign_count: failed to locate DXIL entry function");
        }

        const unsigned actualCount = CountIgnNoiseChains(*context.entryFunction);
        if (actualCount != expectedCount) {
          return MakeRecipeStepFailure(
              context,
              "expect_ign_count: expected IGN count " +
                  std::to_string(expectedCount) + ", found " +
                  std::to_string(actualCount));
        }

        return MakeRecipeStepSuccess(false, 0, false);
      });
}

DxilRecipeStep MakeExpectBlueNoiseCountStep(unsigned expectedCount,
                                            std::string name) {
  return MakeCustomRecipeStep(
      std::move(name),
      [expectedCount](DxilRecipeContext &context) -> DxilRecipeStepResult {
        if (context.dxilModule == nullptr) {
          return MakeRecipeStepFailure(
              context,
              "expect_blue_noise_count: recipe context is missing DXIL module");
        }

        context.entryFunction = context.dxilModule->GetEntryFunction();
        if (context.entryFunction == nullptr) {
          return MakeRecipeStepFailure(
              context,
              "expect_blue_noise_count: failed to locate DXIL entry function");
        }

        const unsigned actualCount =
            CountBlueNoiseTextureLoads(*context.entryFunction, *context.dxilModule);
        if (actualCount != expectedCount) {
          return MakeRecipeStepFailure(
              context,
              "expect_blue_noise_count: expected BlueNoise load count " +
                  std::to_string(expectedCount) + ", found " +
                  std::to_string(actualCount));
        }

        return MakeRecipeStepSuccess(false, 0, false);
      });
}

DxilRecipeStep MakeApplyComputeNoiseRewriteRulesStep(
    std::string name,
    std::string textureId,
    std::string cbufferId,
    bool required) {
  return MakeCustomRecipeStep(
      std::move(name),
      [textureId = std::move(textureId),
       cbufferId = std::move(cbufferId),
       required](DxilRecipeContext &context) -> DxilRecipeStepResult {
        if (context.module == nullptr || context.dxilModule == nullptr) {
          return MakeRecipeStepFailure(
              context,
              "compute_noise_rules: recipe context is missing module state");
        }

        auto textureIt = context.textures.find(textureId);
        if (textureIt == context.textures.end()) {
          return MakeRecipeStepFailure(
              context,
              "compute_noise_rules: unknown texture id '" + textureId + "'");
        }

        auto cbufferIt = context.cbuffers.find(cbufferId);
        if (cbufferIt == context.cbuffers.end()) {
          return MakeRecipeStepFailure(
              context,
              "compute_noise_rules: unknown cbuffer id '" + cbufferId + "'");
        }

        context.entryFunction = context.dxilModule->GetEntryFunction();
        if (context.entryFunction == nullptr) {
          return MakeRecipeStepFailure(
              context,
              "compute_noise_rules: failed to locate DXIL entry function");
        }

        const unsigned ignCount = CountIgnNoiseChains(*context.entryFunction);
        const unsigned blueNoiseCount =
            CountBlueNoiseTextureLoads(*context.entryFunction, *context.dxilModule);
        const unsigned matchCount = ignCount + blueNoiseCount;
        if (matchCount == 0) {
          if (required) {
            return MakeRecipeStepFailure(
                context,
                "compute_noise_rules: no IGN or BlueNoise patterns were found");
          }
          return MakeRecipeStepSuccess(false, 0, false);
        }

        if (!ApplyComputeNoiseRewriteUsingRules(*context.module,
                                                *context.dxilModule,
                                                textureIt->second,
                                                cbufferIt->second,
                                                context.traceEnabled)) {
          return MakeRecipeStepFailure(
              context,
              "compute_noise_rules: rule-based compute rewrite failed");
        }

        return MakeRecipeStepSuccess(true, matchCount, true);
      });
}
