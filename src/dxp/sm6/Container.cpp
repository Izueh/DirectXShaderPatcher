#include "Container.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include "dxc/DXIL/DxilModule.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilContainerReader.h"
#include "dxc/Support/Global.h"

namespace dxp::sm6 {

bool ExtractProgramBitcodeFromContainerPart(
    const std::vector<uint8_t> &container, hlsl::DxilFourCC partKind,
    DxilProgramBitcode &out) {
  hlsl::DxilContainerReader reader;
  if (DXC_FAILED(reader.Load(container.data(), container.size()))) {
    std::cerr << "Failed to parse DXIL container header.\n";
    return false;
  }

  uint32_t partIndex = hlsl::DXIL_CONTAINER_BLOB_NOT_FOUND;
  if (DXC_FAILED(reader.FindFirstPartKind(partKind, &partIndex)) ||
      partIndex == hlsl::DXIL_CONTAINER_BLOB_NOT_FOUND) {
    return false;
  }

  const void *partData = nullptr;
  uint32_t partSize = 0;
  if (DXC_FAILED(reader.GetPartContent(partIndex, &partData, &partSize)) ||
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

bool ExtractDxilProgramBitcode(const std::vector<uint8_t> &containerBytes,
                               DxilProgramBitcode &outBitcode) {
  if (!ExtractProgramBitcodeFromContainerPart(containerBytes, hlsl::DFCC_DXIL,
                                              outBitcode)) {
    std::cerr << "DXIL part not found in container.\n";
    return false;
  }

  return true;
}

std::unique_ptr<llvm::Module> ParseDxilBitcode(const uint8_t *ptr,
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

} // namespace dxp::sm6