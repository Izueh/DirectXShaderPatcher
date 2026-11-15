#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <dxp/ExportTypes.hpp>
#include "d3d11TokenizedProgramFormat.hpp"
#include "dxp/sm5/Model_impl.hpp"
#include "dxp/sm5/Sm51.hpp"
#include "dxp/sm5/step/AddResourceStep.hpp"

namespace dxp::sm5 {
using namespace dxp::sm5::model;

/// @brief DXBC shader program type. Mirrors @c D3D10_SB_TOKENIZED_PROGRAM_TYPE.
enum class ProgramType : std::uint32_t {
  PixelShader = 0,     ///< @c D3D10_SB_PIXEL_SHADER
  VertexShader = 1,    ///< @c D3D10_SB_VERTEX_SHADER
  GeometryShader = 2,  ///< @c D3D10_SB_GEOMETRY_SHADER
  HullShader = 3,      ///< @c D3D11_SB_HULL_SHADER
  DomainShader = 4,    ///< @c D3D11_SB_DOMAIN_SHADER
  ComputeShader = 5,   ///< @c D3D11_SB_COMPUTE_SHADER
};

struct ShaderProgram {
 public:
  ShaderProgram() : program_type(ProgramType::PixelShader) {}
  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;
  ShaderProgram(ShaderProgram&& /*other*/) noexcept;
  ShaderProgram& operator=(ShaderProgram&& /*other*/) noexcept;
  ~ShaderProgram() = default;

  struct DxbcContainerHeader {
    uint32_t signature;
    uint32_t hash[4];
    uint32_t one;
    uint32_t total_size_in_bytes;
    uint32_t chunk_count;
  };

  enum class ChunkKind : std::uint8_t {
    None,
    Shader,
    RDEF,
    PSIO,
    VSIO,
    GSIO,
    DSIO,
    HSIO,
    CSIO,
    SBIO,
    STAT,
    INFO,
    Flags,
    Type,
    Unknown,
  };

  struct DxbcChunk {
    uint32_t four_cc{};
    ChunkKind kind = ChunkKind::Unknown;
    std::vector<uint8_t> data;
    uint32_t offset_in_container = 0;
  };

  ProgramType program_type = ProgramType::PixelShader;
  uint32_t major_version = 0;
  uint32_t minor_version = 0;
  uint32_t total_length_in_dwords = 0;

  std::vector<Instruction> instructions;

  std::vector<ResourceDecl> resources;
  std::vector<SamplerDecl> samplers;
  std::vector<CBufferDecl> cbuffers;
  std::vector<ThreadGroupDecl> thread_groups;
  GlobalFlags global_flags;

  uint32_t temp_count = 0;
  std::vector<uint32_t> indexable_temps;

  /// @brief Resources outside space 0 (or away from their program register), by program register.
  /// The program is always 5.0-shaped, see sm51::PlacementMap. A non-empty map means the output is SM5.1.
  sm51::PlacementMap placements;
  /// @brief The input was SM5.1 (normalized to the 5.0-shaped program on load). The output stays 5.1.
  bool input_sm51 = false;
  /// @brief Write SM5.1 even when nothing needs it (the recipe's `upgrade_to_sm51: always`).
  bool force_sm51 = false;

  /// @brief Whether Serialize writes SM5.1.
  [[nodiscard]] bool EmitsSm51() const { return force_sm51 || input_sm51 || !placements.empty(); }

  /// @brief Register numbers the program declares in a family (the program's, not the real ones).
  [[nodiscard]] std::unordered_set<uint32_t> DeclaredProgramRegisters(sm51::RegClass cls) const;
  /// @brief Picks the real register for a resource in @p space: @p requested, or the first free one
  /// (the highest one at or below @p max_bind_point when @p reverse). Space 0 isn't handled here.
  [[nodiscard]] bool AllocatePlacedRegister(sm51::RegClass cls, uint32_t space, std::optional<uint32_t> requested, bool reverse,
                                            uint32_t max_bind_point, uint32_t& out_register, std::string& error) const;
  /// @brief A free program register for a resource that lives in another space.
  [[nodiscard]] uint32_t FindFreeProgramRegister(sm51::RegClass cls) const;

  /// @brief Adds an input signature declaration.
  bool AddInputDeclaration(const step::AddResourceStep::InputDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error);
  /// @brief Adds an output signature declaration.
  bool AddOutputDeclaration(const step::AddResourceStep::OutputDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error);
  /// @brief Adds a texture resource declaration.
  bool AddTextureDeclaration(const step::AddResourceStep::TextureDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error);
  /// @brief Adds a raw resource declaration.
  bool AddRawResourceDeclaration(const step::AddResourceStep::RawResourceDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error);
  /// @brief Adds a structured resource declaration.
  bool AddStructuredResourceDeclaration(const step::AddResourceStep::StructuredResourceDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error);
  /// @brief Adds a constant buffer declaration.
  bool AddCBufferDeclaration(const step::AddResourceStep::CBufferDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error);
  /// @brief Adds a sampler declaration.
  bool AddSamplerDeclaration(const step::AddResourceStep::SamplerDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error);
  /// @brief Adds a UAV declaration.
  bool AddUavDeclaration(const step::AddResourceStep::UavDecl& decl, uint32_t& out_register_index, uint32_t max_bind_point, std::string& error);

  /// @brief Finds the next available bind point for textures.
  [[nodiscard]] unsigned FindNextAvailableTexture(unsigned preferred = 0, bool from_high = false) const;
  /// @brief Finds the next available bind point for samplers.
  [[nodiscard]] unsigned FindNextAvailableSampler(unsigned preferred = 0, bool from_high = false) const;
  /// @brief Finds the next available bind point for constant buffers.
  [[nodiscard]] unsigned FindNextAvailableCBuffer(unsigned preferred = 0, bool from_high = false) const;
  /// @brief Finds the next available bind point for UAVs.
  [[nodiscard]] unsigned FindNextAvailableUAV(unsigned preferred = 0, bool from_high = false) const;
  /// @brief Finds the next available bind point for input signature registers.
  [[nodiscard]] unsigned FindNextAvailableInput(unsigned preferred = 0, bool from_high = false) const;
  /// @brief Finds the next available bind point for output signature registers.
  [[nodiscard]] unsigned FindNextAvailableOutput(unsigned preferred = 0, bool from_high = false) const;

  /// @brief Ensures a DCL_TEMPS declaration covering temp_count + template_pool_size
  /// exists (updates an existing one, or inserts one after the last declaration).
  /// The template pool registers live above the declared temp run (reuse pool
  /// model) and must be covered by dcl_temps when template temps are used.
  void EnsureTempDeclaration(uint32_t template_pool_size = 0);

  /// @brief Allocates a bind point, optionally auto-assigning to the next available slot.
  static bool AllocateBindPoint(const std::unordered_set<uint32_t>& occupied, bool auto_bind, uint32_t requested, bool reverse,
                                uint32_t& resolved_register_index, std::string& error);
  /// @brief Records a named binding and checks for duplicates.
  static bool RecordNamedBinding(std::unordered_map<std::string, uint32_t>& bindings, const std::string& handle,
                                 uint32_t register_index, const char* kind, std::string& error);
  /// @brief Finds the index after the last declaration of the given opcode.
  uint32_t FindInsertAfterLastDeclaration(Opcode opcode);
  /// @brief Builds a DCL_TEMPS instruction for the given temp count.
  static Instruction BuildTempDeclaration(uint32_t temp_count);
  /// @brief Builds a DCL_CONSTANT_BUFFER declaration.
  static Instruction BuildConstantBufferDeclaration(const step::AddResourceStep::CBufferDecl& decl);
  /// @brief Builds a DCL_RESOURCE (texture) declaration.
  static Instruction BuildTextureDeclaration(const step::AddResourceStep::TextureDecl& decl);
  /// @brief Builds a DCL_INPUT_PS declaration.
  static Instruction BuildInputDeclaration(const step::AddResourceStep::InputDecl& decl);
  /// @brief Builds a DCL_OUTPUT declaration.
  static Instruction BuildOutputDeclaration(const step::AddResourceStep::OutputDecl& decl);
  /// @brief Builds a DCL_SAMPLER declaration.
  static Instruction BuildSamplerDeclaration(const step::AddResourceStep::SamplerDecl& decl);
  /// @brief Builds a DCL_RESOURCE_RAW declaration.
  static Instruction BuildRawResourceDeclaration(const step::AddResourceStep::RawResourceDecl& decl);
  /// @brief Builds a DCL_RESOURCE_STRUCTURED declaration.
  static Instruction BuildStructuredResourceDeclaration(const step::AddResourceStep::StructuredResourceDecl& decl);
  /// @brief Builds a DCL_UNORDERED_ACCESS_VIEW declaration.
  static Instruction BuildUavDeclaration(const step::AddResourceStep::UavDecl& decl);
  /// @brief Creates a component mode token for single-component selection.
  static uint32_t MakeSelectComponentMode(uint32_t component);
  /// @brief Creates a constant buffer declaration operand.
  static Operand MakeConstantBufferDeclarationOperand(uint32_t register_index, uint32_t element_count);
  /// @brief Creates a sampler operand.
  static Operand MakeSamplerOperand(uint32_t register_index);
  /// @brief Creates a resource operand.
  static Operand MakeResourceOperand(uint32_t register_index);
  /// @brief Creates an input operand.
  static Operand MakeInputOperand(uint32_t register_index);
  /// @brief Creates an output operand.
  static Operand MakeOutputOperand(uint32_t register_index);
  /// @brief Creates a UAV operand.
  static Operand MakeUavOperand(uint32_t register_index);
  /// @brief Encodes raw tokens and sets the instruction length.
  static Instruction FinalizeInstruction(Instruction instruction);
  /// @brief Load from raw DXBC container bytes, returning the parsed program or
  /// a specific error message (no global streams are touched).
  static std::expected<ShaderProgram, std::string> FromBytes(std::span<const uint8_t> bytes);

  /// @brief Serialize back to DXBC bytecode (rebuilds shader chunk, recomputes hash).
  /// @return Serialized bytes, or a specific error message.
  std::expected<std::vector<uint8_t>, std::string> Serialize();

  /// @brief Build a container report.
  dxp::PatchContainerReport BuildReport();

  /// @brief Count opcodes in the parsed instruction list. Returns named counts for all opcodes.
  [[nodiscard]] std::unordered_map<std::string, int32_t> GetOpcodeCounts() const;

  /// @brief Return ordered list of opcodes from the parsed instruction list.
  [[nodiscard]] std::vector<Opcode> GetInstructionOpcodes() const;

 private:
  DxbcContainerHeader header = {};
  std::vector<DxbcChunk> chunks;

  [[nodiscard]] const DxbcChunk* FindChunk(ChunkKind kind) const;
  DxbcChunk* FindChunk(ChunkKind kind);
  [[nodiscard]] const DxbcChunk* FindChunkByFourCC(uint32_t four_cc) const;
  DxbcChunk* FindChunkByFourCC(uint32_t four_cc);
  [[nodiscard]] const DxbcChunk* GetShaderChunk() const;
  DxbcChunk* GetShaderChunk();

  [[nodiscard]] std::vector<uint8_t> SerializeBitcode() const;

  static bool ParseProgram(std::span<const uint8_t> data, ShaderProgram& program);
};

}  // namespace dxp::sm5
