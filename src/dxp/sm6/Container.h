#pragma once

#include "../../../include/dxp/PatchReport.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "dxc/DXIL/DxilModule.h"
#include "dxc/DxilContainer/DxilContainer.h"

namespace dxp::sm6 {

struct DxilProgramBitcode {
  const uint8_t *ptr = nullptr;
  uint32_t size = 0;
};

bool ExtractProgramBitcodeFromContainerPart(
    const std::vector<uint8_t> &container, hlsl::DxilFourCC partKind,
    DxilProgramBitcode &out);
bool BuildDxilContainerReport(const std::vector<uint8_t> &containerBytes,
                dxp::PatchContainerReport &report);
bool ExtractDxilProgramBitcode(const std::vector<uint8_t> &containerBytes,
                               DxilProgramBitcode &outBitcode);
std::unique_ptr<llvm::Module>
ParseDxilBitcode(const uint8_t *ptr, uint32_t size, llvm::LLVMContext &context);
bool LoadDxilState(llvm::Module &module, hlsl::DxilModule *&outDxilModule);
bool VerifyModuleOrReport(llvm::Module &module);

} // namespace dxp::sm6