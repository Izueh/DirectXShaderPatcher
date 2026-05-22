#include "../../../include/dxp/sm6/Patch.h"

#include "../../../include/dxp/sm6/Recipe.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// NOLINTBEGIN(misc-include-cleaner)
#include <ObjIdl.h>
#include <atlbase.h>
#include "dxc/dxcapi.h"
// NOLINTEND(misc-include-cleaner)

#include "Container.h"

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/raw_ostream.h"

#include "dxc/DXIL/DxilModule.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilContainerAssembler.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"

using llvm::LLVMContext;
using llvm::Module;

// Windows COM entry points and ATL smart pointers are reported as
// direct-include misses here even with explicit SDK headers in this toolchain
// setup. NOLINTBEGIN(misc-include-cleaner)
// NOLINTBEGIN(llvm-prefer-static-over-anonymous-namespace)
namespace {

static void ReleaseDxilStateForModule(std::unique_ptr<llvm::Module> &module,
                                      hlsl::DxilModule *&dxilModule) {
  if (!module) {
    dxilModule = nullptr;
    return;
  }

  if (module->HasDxilModule()) {
    // DXC teardown is unstable for mutated modules in this workflow. Detach the
    // embedded DxilModule so llvm::Module can be destroyed without invoking the
    // vendored reset hook.
    module->pfnRemoveGlobal = nullptr;
    module->pfnResetDxilModule = nullptr;
    module->SetDxilModule(nullptr);
  }

  dxilModule = nullptr;
  module.reset();
}

class ScopedPatchCoInitialize {
public:
  ScopedPatchCoInitialize() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    shouldUninitialize_ = !DXC_FAILED(hr);
  }

  ~ScopedPatchCoInitialize() {
    if (shouldUninitialize_)
      CoUninitialize();
  }

private:
  bool shouldUninitialize_ = false;
};

static void TracePatchMessage(bool traceEnabled, const char *message) {
  if (traceEnabled)
    std::cerr << "[trace] " << message << "\n";
}

static bool
CreateMemoryStreamFromBytes(IMalloc *mallocInterface,
                            const std::vector<uint8_t> &bytes,
                            CComPtr<hlsl::AbstractMemoryStream> &stream) {
  if (DXC_FAILED(hlsl::CreateMemoryStream(mallocInterface, &stream)) ||
      !stream) {
    std::cerr << "CreateMemoryStream failed.\n";
    return false;
  }

  if (!bytes.empty()) {
    ULONG written = 0;
    const HRESULT hr =
        stream->Write(bytes.data(), static_cast<ULONG>(bytes.size()), &written);
    if (DXC_FAILED(hr) || written != bytes.size()) {
      std::cerr << "Failed to write bytes into memory stream.\n";
      return false;
    }
  }

  const LARGE_INTEGER zero = {};
  if (DXC_FAILED(stream->Seek(zero, STREAM_SEEK_SET, nullptr))) {
    std::cerr << "Failed to rewind memory stream.\n";
    return false;
  }

  return true;
}

static bool ValidatePatchedContainerOrReport(std::vector<uint8_t> &container) {
  CComPtr<IDxcUtils> dxcUtils;
  if (DXC_FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils))) ||
      !dxcUtils) {
    std::cerr << "Failed to create IDxcUtils for DXIL validation.\n";
    return false;
  }

  CComPtr<IDxcValidator> validator;
  if (DXC_FAILED(
          DxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&validator))) ||
      !validator) {
    std::cerr << "Failed to create IDxcValidator for DXIL validation.\n";
    return false;
  }

  CComPtr<IDxcBlobEncoding> containerBlob;
  if (DXC_FAILED(dxcUtils->CreateBlob(container.data(),
                                      static_cast<UINT32>(container.size()), 0,
                                      &containerBlob)) ||
      !containerBlob) {
    std::cerr << "Failed to create DXIL blob for validation.\n";
    return false;
  }

  CComPtr<IDxcOperationResult> validationResult;
  if (DXC_FAILED(validator->Validate(containerBlob,
                                     DxcValidatorFlags_InPlaceEdit,
                                     &validationResult)) ||
      !validationResult) {
    std::cerr << "DXIL validator invocation failed.\n";
    return false;
  }

  HRESULT validationStatus = E_FAIL;
  if (DXC_FAILED(validationResult->GetStatus(&validationStatus))) {
    std::cerr << "Failed to query DXIL validator status.\n";
    return false;
  }

  if (DXC_FAILED(validationStatus)) {
    std::cerr << "Patched container failed DXIL validation.\n";

    CComPtr<IDxcBlobEncoding> errorBlob;
    if (SUCCEEDED(validationResult->GetErrorBuffer(&errorBlob)) && errorBlob) {
      CComPtr<IDxcBlobUtf8> errorText;
      if (SUCCEEDED(dxcUtils->GetBlobAsUtf8(errorBlob, &errorText)) &&
          errorText && errorText->GetStringLength() != 0) {
        std::cerr.write(errorText->GetStringPointer(),
                        static_cast<std::streamsize>(
                            errorText->GetStringLength()));
        if (errorText->GetStringPointer()[errorText->GetStringLength() - 1] !=
            '\n') {
          std::cerr << '\n';
        }
      }
    }

    return false;
  }

  const uint8_t *validatedBytes =
      reinterpret_cast<const uint8_t *>(containerBlob->GetBufferPointer());
  const size_t validatedSize = static_cast<size_t>(containerBlob->GetBufferSize());
  container.assign(validatedBytes, validatedBytes + validatedSize);
  return true;
}

} // namespace
// NOLINTEND(llvm-prefer-static-over-anonymous-namespace)
// NOLINTEND(misc-include-cleaner)

DxilLoadedShaderState::~DxilLoadedShaderState() {
  ReleaseDxilStateForModule(module, dxilModule);
  reflectionContext.reset();
}

bool LoadDxilContainerForMutation(const void *containerData,
                                  size_t containerSize,
                                  DxilLoadedShaderState &shader,
                                  bool restoreReflection) {
  shader.module.reset();
  shader.reflectionContext.reset();
  shader.dxilModule = nullptr;

  if (containerData == nullptr && containerSize != 0) {
    std::cerr
        << "LoadDxilContainerForMutation received a null container pointer.\n";
    return false;
  }

  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(containerData);
  shader.inputBytes.assign(bytes, bytes + containerSize);

  dxp::sm6::DxilProgramBitcode dxilBitcode;
  if (!dxp::sm6::ExtractProgramBitcodeFromContainerPart(
          shader.inputBytes, hlsl::DFCC_DXIL, dxilBitcode)) {
    std::cerr << "Failed to extract DXIL program bitcode.\n";
    return false;
  }

  shader.module = dxp::sm6::ParseDxilBitcode(dxilBitcode.ptr, dxilBitcode.size,
                                             shader.context);
  if (!shader.module) {
    std::cerr << "Failed to parse DXIL bitcode.\n";
    return false;
  }

  if (!dxp::sm6::LoadDxilState(*shader.module, shader.dxilModule) ||
      shader.dxilModule == nullptr) {
    std::cerr << "Failed to load DxilModule state.\n";
    return false;
  }

  if (restoreReflection) {
    shader.reflectionContext = std::make_unique<LLVMContext>();
    RestoreOriginalResourceReflection(shader.inputBytes, *shader.dxilModule,
                                      *shader.reflectionContext);
  }

  return true;
}

bool LoadDxilContainerForMutation(const std::vector<uint8_t> &containerBytes,
                                  DxilLoadedShaderState &shader,
                                  bool restoreReflection) {
  return LoadDxilContainerForMutation(
      containerBytes.data(), containerBytes.size(), shader, restoreReflection);
}

bool ReloadDxilContainerFromMemory(const std::vector<uint8_t> &containerBytes,
                                   LLVMContext &context,
                                   std::unique_ptr<Module> &module,
                                   hlsl::DxilModule *&dxilModule) {
  dxp::sm6::DxilProgramBitcode patchedDxilBitcode;
  if (!dxp::sm6::ExtractProgramBitcodeFromContainerPart(
          containerBytes, hlsl::DFCC_DXIL, patchedDxilBitcode)) {
    std::cerr << "Failed to extract DXIL bitcode from patched container.\n";
    return false;
  }

  module = dxp::sm6::ParseDxilBitcode(patchedDxilBitcode.ptr,
                                      patchedDxilBitcode.size, context);
  if (!module) {
    std::cerr << "Failed to parse patched DXIL bitcode.\n";
    return false;
  }

  if (!dxp::sm6::LoadDxilState(*module, dxilModule) || dxilModule == nullptr) {
    std::cerr << "Failed to load DxilModule from patched container.\n";
    return false;
  }

  return true;
}

bool PatchDxilContainerInMemory(const DxilRecipe &recipe, const void *inputData,
                                size_t inputSize,
                                std::vector<uint8_t> &outputContainer,
                                const DxilContainerPatchOptions &options,
                                DxilRecipeContext *outContext) {
  const ScopedPatchCoInitialize coinit;
  const bool traceEnabled = options.recipeExecutionOptions.traceEnabled;

  TracePatchMessage(traceEnabled, "patch: load container");

  std::unique_ptr<DxilLoadedShaderState> shader =
      std::make_unique<DxilLoadedShaderState>();
  if (!LoadDxilContainerForMutation(inputData, inputSize, *shader,
                                    options.restoreReflection)) {
    return false;
  }

  TracePatchMessage(traceEnabled, "patch: execute recipe");
  DxilRecipeContext localContext;
  DxilRecipeContext *recipeContext = outContext != nullptr ? outContext : &localContext;
  if (!ExecuteDxilRecipe(recipe, *shader->module, *shader->dxilModule,
                         options.recipeExecutionOptions, recipeContext)) {
    return false;
  }

  const bool shouldRefreshResources = recipeContext->resourceBindingsChanged &&
                                      !recipeContext->resourcesRefreshed;
  if (shouldRefreshResources) {
    TracePatchMessage(traceEnabled, "patch: refresh resources");
    RefreshDxilAfterResourceMutation(
        *shader->dxilModule, options.recipeExecutionOptions.traceEnabled);
    recipeContext->resourcesRefreshed = true;
  }

  // Always refresh the OP cache before serialization — pruning may have
  // left stale function pointers in the cache.
  {
    hlsl::OP *op = shader->dxilModule->GetOP();
    if (op)
      op->RefreshCache();
  }

  if (!recipeContext->moduleVerified) {
    if (!dxp::sm6::VerifyModuleOrReport(*shader->module))
      return false;
    recipeContext->moduleVerified = true;
  }

  bool ok = SerializePatchedContainer(*shader->dxilModule,
                                      SerializeModuleToBitcode(*shader->module),
                                      outputContainer);
  if (ok && recipeContext->moduleModified)
    ok = ValidatePatchedContainerOrReport(outputContainer);
  if (ok)
    shader.release();
  return ok;
}

bool PatchDxilContainerInMemory(const DxilRecipe &recipe,
                                const std::vector<uint8_t> &inputContainer,
                                std::vector<uint8_t> &outputContainer,
                                const DxilContainerPatchOptions &options,
                                DxilRecipeContext *outContext) {
  return PatchDxilContainerInMemory(recipe, inputContainer.data(),
                                    inputContainer.size(), outputContainer,
                                    options, outContext);
}

std::vector<uint8_t> SerializeModuleToBitcode(Module &module) {
  std::string bitcodeBytes;
  llvm::raw_string_ostream outputStream(bitcodeBytes);
  llvm::WriteBitcodeToFile(&module, outputStream);
  outputStream.flush();
  return std::vector<uint8_t>(bitcodeBytes.begin(), bitcodeBytes.end());
}

bool SerializePatchedContainer(hlsl::DxilModule &dxilModule,
                               const std::vector<uint8_t> &moduleBitcode,
                               std::vector<uint8_t> &outputContainer) {
  CComPtr<IMalloc> mallocInterface;
  // NOLINTNEXTLINE(misc-include-cleaner)
  if (DXC_FAILED(::CoGetMalloc(1, &mallocInterface)) || !mallocInterface) {
    std::cerr << "CoGetMalloc failed.\n";
    return false;
  }

  CComPtr<hlsl::AbstractMemoryStream> moduleBitcodeStream;
  if (!CreateMemoryStreamFromBytes(mallocInterface, moduleBitcode,
                                   moduleBitcodeStream)) {
    std::cerr << "CreateMemoryStreamFromBytes failed\n";
    return false;
  }

  CComPtr<hlsl::AbstractMemoryStream> outputStream;
  if (DXC_FAILED(hlsl::CreateMemoryStream(mallocInterface, &outputStream)) ||
      !outputStream) {
    std::cerr << "Failed to create output stream.\n";
    return false;
  }

  const hlsl::SerializeDxilFlags flags = hlsl::SerializeDxilFlags::None;

  hlsl::SerializeDxilContainerForModule(&dxilModule, moduleBitcodeStream,
                                        nullptr, outputStream, "", flags,
                                        nullptr, nullptr, nullptr, nullptr, 0);

  if (outputStream->GetPtr() == nullptr || outputStream->GetPtrSize() == 0) {
    std::cerr << "DXIL container serialization produced no output.\n";
    return false;
  }

  outputContainer.assign(outputStream->GetPtr(),
                         outputStream->GetPtr() + outputStream->GetPtrSize());
  return true;
}

void RestoreOriginalResourceReflection(const std::vector<uint8_t> &inputBytes,
                                       hlsl::DxilModule &targetDxilModule,
                                       LLVMContext &reflectionContext) {
  dxp::sm6::DxilProgramBitcode reflectionBitcode;
  if (!dxp::sm6::ExtractProgramBitcodeFromContainerPart(
          inputBytes, hlsl::DFCC_ShaderStatistics, reflectionBitcode)) {
    return;
  }

  const std::unique_ptr<Module> reflectionModule = dxp::sm6::ParseDxilBitcode(
      reflectionBitcode.ptr, reflectionBitcode.size, reflectionContext);
  if (!reflectionModule)
    return;

  hlsl::DxilModule *reflectionDxilModule = nullptr;
  if (!dxp::sm6::LoadDxilState(*reflectionModule, reflectionDxilModule) ||
      reflectionDxilModule == nullptr) {
    return;
  }

  targetDxilModule.RestoreResourceReflection(*reflectionDxilModule);
}