#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <dxp/ExportTypes.hpp>
#include <dxp/sm6/ResourceTypes.hpp>
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilResourceBinding.h"
#include "dxc/DxilContainer/DxilContainer.h"
//clang-format off
// WinIncludes must precede MSFileSystem.h — it declares the Win32 types the
// MSFileSystem interface uses (HANDLE, LPCWSTR, ...).
#include <dxc/Support/WinIncludes.h>
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MSFileSystem.h"
//clang-format on

namespace dxp::sm6 {

/// @brief SM6 (DXIL) shader program.
struct ShaderProgram {
 public:
  ShaderProgram() = default;
  ~ShaderProgram();

  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;
  ShaderProgram(ShaderProgram&& /*other*/) noexcept;
  ShaderProgram& operator=(ShaderProgram&& /*other*/) noexcept;

  /// @brief Load from raw DXIL container bytes, returning the parsed program or
  /// a specific error message (no global streams are touched).
  ///
  /// The module is parsed into a per-thread LLVMContext (reused across calls on
  /// the same thread, matching DXC's thread-confined compilation model).
  /// ShaderProgram instances are therefore thread-affine: create, use, and
  /// destroy a program on the same thread.
  static std::expected<void, std::string> FromBytes(std::span<const uint8_t> bytes, ShaderProgram& out,
                                                    bool restore_reflection = true);

  /// @brief Load using an externally-owned LLVMContext.
  ///
  /// The caller must keep the context alive for at least as long as the
  /// ShaderProgram and must not use the context from another thread while the
  /// program is alive.
  static std::expected<void, std::string> FromBytes(std::span<const uint8_t> bytes, ShaderProgram& out,
                                                    llvm::LLVMContext& external_context,
                                                    bool restore_reflection = true);

  /// @brief Reload module state from stored input bytes.
  std::expected<void, std::string> Reload();

  /// @brief Build a DXIL container report.
  static std::expected<void, std::string> BuildContainerReport(std::span<const uint8_t> container_bytes,
                                                               dxp::PatchContainerReport& report);

  std::expected<void, std::string> AddCBuffer(const CBufferDesc& desc);
  std::expected<void, std::string> AddTextureSRV(const TextureResourceDesc& desc);
  std::expected<void, std::string> AddTextureUAV(const TextureResourceDesc& desc);
  std::expected<void, std::string> AddTexture2DSRV(const TextureResourceDesc& desc);
  std::expected<void, std::string> AddSampler(const SamplerDesc& desc);

  /// @brief Serialize to DXIL container bytes.
  std::expected<std::vector<uint8_t>, std::string> Serialize();

  /// @brief Count DXIL and LLVM opcodes in the entry function.
  [[nodiscard]] std::pair<std::unordered_map<std::string, int32_t>, std::unordered_map<std::string, int32_t>> GetOpcodeCounts() const;

  unsigned FindNextAvailableTexture(unsigned space = 0, unsigned preferred = 0);
  unsigned FindNextAvailableUAV(unsigned space = 0, unsigned preferred = 0);
  unsigned FindNextAvailableCBuffer(unsigned space, unsigned preferred = 0);
  unsigned FindNextAvailableSampler(unsigned space, unsigned preferred = 0);
  unsigned FindNextAvailableInput();
  unsigned FindNextAvailableOutput();

  /// @brief Create an LLVM handle value for a DXIL resource.
  llvm::Value* CreateResourceHandle(const hlsl::DxilResourceBase& resource,
                                    const hlsl::DxilResourceBinding& binding);
  bool AddInputSignature(const std::string& semantic_name, hlsl::CompType::Kind comp_type,
                         unsigned vector_size, unsigned register_index,
                         hlsl::InterpolationMode interp_mode);
  bool AddOutputSignature(const std::string& semantic_name, hlsl::CompType::Kind comp_type,
                          unsigned vector_size, unsigned register_index);

  [[nodiscard]] std::expected<void, std::string> Verify() const;

  /// @brief Prune dead code from a single instruction tree.
  static void PruneInstruction(llvm::Instruction* instruction);

  /// @brief Prune all dead code from the entry function.
  void PruneDeadCode() const;

  [[nodiscard]] llvm::Module* GetModule() const {
    return module.get();
  }
  [[nodiscard]] hlsl::DxilModule* GetDxilModule() const {
    return dxil_module;
  }
  [[nodiscard]] llvm::Function* GetEntryFunction() const;
  [[nodiscard]] std::span<const uint8_t> GetInputBytes() const {
    return input_bytes;
  }

  /// @brief Non-fatal issues collected while loading (e.g. DXIL metadata
  /// warnings). Empty unless a load produced warnings. The recipe engine
  /// forwards these to the execution log at Warning level.
  std::vector<std::string> warnings;

  /// Convert user-facing resource binding to hlsl::DxilResourceBinding.
  static hlsl::DxilResourceBinding ToDxilBinding(const ResourceBindingDesc& desc);

 private:
  std::vector<uint8_t> input_bytes;
  std::unique_ptr<llvm::Module> module;
  hlsl::DxilModule* dxil_module = nullptr;

  struct DxilProgramBitcode {
    const uint8_t* ptr = nullptr;
    uint32_t size = 0;
  };

  bool ExtractBitcodeFromContainer(hlsl::DxilFourCC part_kind, DxilProgramBitcode& out) const;
  bool ExtractDxilBitcode(DxilProgramBitcode& out) const;

  std::expected<void, std::string> ParseBitcode(const DxilProgramBitcode& bitcode, llvm::LLVMContext& parse_context);
  bool LoadState();
  void RestoreReflection();

  std::vector<uint8_t> SerializeBitcode();
  std::expected<void, std::string> SerializeContainer(std::span<const uint8_t> bitcode, std::vector<uint8_t>& output_container);

  template <typename TResource>
  unsigned FindNextAvailable(const std::vector<std::unique_ptr<TResource>>& resources, unsigned space,
                             unsigned preferred);
};

/// @brief Find a DXIL resource by register index and space.
const hlsl::DxilResourceBase* FindResourceByRegisterIndex(hlsl::DxilModule& dxil_module,
                                                          hlsl::DXIL::ResourceClass resource_class,
                                                          unsigned register_index, unsigned space);

/// @brief Per-thread DXC runtime state that LLVM code relies on: the thread
/// file system (needed by raw_fd_ostream — without it llvm::errs() and file
/// I/O silently fail) and the thread malloc allocator.
///
/// The state is initialized once per thread and lives until thread exit —
/// tearing it down mid-execution would leave DXC-owned allocations, created
/// under the thread malloc, to be freed under a different allocator. Call
/// DxcRuntime::Ensure() on every thread that loads or serializes a
/// ShaderProgram before any DXC/LLVM work.
struct DxcRuntime {
  /// @brief Initialize this thread's runtime if not yet done. Returns the
  /// setup error, empty on success.
  static const std::string& Ensure();

  DxcRuntime() { Ensure(); }
};

}  // namespace dxp::sm6
