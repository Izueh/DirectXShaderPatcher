#pragma once

#include <atlbase.h>

#include <combaseapi.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <dxp/sm6/ResourceTypes.hpp>
#include "d3d11TokenizedProgramFormat.hpp"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResource.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

// COM initialization helper — ensures CoInitializeEx is called once per thread
class ScopedCoInitialize {
 public:
  ScopedCoInitialize() {
    const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    initialized = SUCCEEDED(result);
  }

  ScopedCoInitialize(const ScopedCoInitialize&) = delete;
  ScopedCoInitialize& operator=(const ScopedCoInitialize&) = delete;
  ScopedCoInitialize(ScopedCoInitialize&&) = delete;
  ScopedCoInitialize& operator=(ScopedCoInitialize&&) = delete;

  ~ScopedCoInitialize() {
    if (initialized) {
      CoUninitialize();
    }
  }

  [[nodiscard]] bool IsInitialized() const {
    return initialized;
  }

 private:
  bool initialized = false;
};

// Read an entire file into a byte vector
inline bool ReadFile(const std::string& path, std::vector<uint8_t>& data) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  if (size < 0) {
    return false;
  }
  file.seekg(0, std::ios::beg);
  data.resize(static_cast<size_t>(size));
  if (size > 0) {
    file.read(reinterpret_cast<char*>(data.data()), size);
  }
  return !!file;
}

// Write bytes to a file, creating parent directories as needed
inline bool WriteFile(const std::string& path, const void* ptr, size_t size) {
  const std::filesystem::path output_path(path);
  const std::filesystem::path parent_path = output_path.parent_path();
  if (!parent_path.empty()) {
    std::error_code error;
    if (!std::filesystem::create_directories(parent_path, error) && error) {
      return false;
    }
  }
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  if (size > 0) {
    file.write(reinterpret_cast<const char*>(ptr), static_cast<std::streamsize>(size));
  }
  return !!file;
}

// Return the repository root directory (parent of tests/)
inline std::filesystem::path RepoRootPath() {
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

// Return the default artifact output path — next to the test binary in the
// per-config build directory (DXP_TEST_OUTPUT_DIR is injected by CMake), or
// artifacts/test-output under the repo root as a fallback.
inline std::string DefaultArtifactOutputPath(const std::string& input_path, const std::string& suffix) {
  const std::filesystem::path input_file(input_path);
  const std::filesystem::path stem = input_file.stem();
#ifdef DXP_TEST_OUTPUT_DIR
  return (std::filesystem::path(DXP_TEST_OUTPUT_DIR) / (stem.string() + suffix)).string();
#else
  return (RepoRootPath() / "artifacts" / "test-output" / (stem.string() + suffix)).string();
#endif
}
