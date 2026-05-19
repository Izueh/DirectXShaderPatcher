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

} // namespace
// NOLINTEND(llvm-prefer-static-over-anonymous-namespace)
// NOLINTEND(misc-include-cleaner)

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

  DxilLoadedShaderState shader;
  if (!LoadDxilContainerForMutation(inputData, inputSize, shader,
                                    options.restoreReflection)) {
    return false;
  }

  TracePatchMessage(traceEnabled, "patch: execute recipe");
  if (!ExecuteDxilRecipe(recipe, *shader.module, *shader.dxilModule,
                         options.recipeExecutionOptions, outContext)) {
    return false;
  }

  if (options.refreshResources) {
    TracePatchMessage(traceEnabled, "patch: refresh resources");
    RefreshDxilAfterResourceMutation(
        *shader.dxilModule, options.recipeExecutionOptions.traceEnabled);
  }

  TracePatchMessage(traceEnabled, "patch: verify module");
  if (options.verifyModule && !dxp::sm6::VerifyModuleOrReport(*shader.module))
    return false;

  TracePatchMessage(traceEnabled, "patch: serialize container");
  return SerializePatchedContainer(*shader.dxilModule,
                                   SerializeModuleToBitcode(*shader.module),
                                   outputContainer);
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