#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "dxp/PatchReport.h"

namespace dxp::sm5 {

struct Program;
struct Recipe;
struct RecipeContext;
struct RecipeStepResult;

struct DxbcContainerHeader {
  uint32_t Signature;
  uint32_t Hash[4];
  uint32_t One;
  uint32_t TotalSizeInBytes;
  uint32_t ChunkCount;
};

enum class ChunkKind {
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
  uint32_t FourCC;
  ChunkKind Kind;
  std::vector<uint8_t> Data;
  uint32_t OffsetInContainer;
};

struct Container {
  DxbcContainerHeader Header;
  std::vector<DxbcChunk> Chunks;
  std::vector<uint8_t> RawBytes;

  const DxbcChunk *FindChunk(ChunkKind kind) const;
  DxbcChunk *FindChunk(ChunkKind kind);

  const DxbcChunk *FindChunkByFourCC(uint32_t fourCC) const;
  DxbcChunk *FindChunkByFourCC(uint32_t fourCC);

  const DxbcChunk *GetShaderChunk() const;
  DxbcChunk *GetShaderChunk();
};

bool ParseDxbcContainer(const std::vector<uint8_t> &bytes,
                        Container &container);

bool SerializeDxbcContainer(const Container &container,
                            std::vector<uint8_t> &outBytes);

bool RecomputeDxbcHash(std::vector<uint8_t> &containerBytes);

/// @brief Executes a pre-compiled recipe against an already-parsed program.
/// Internal API — available for benchmarking. Not part of the public SDK surface.
bool ExecuteRecipe(
    Program &program, const Recipe &recipe,
    RecipeContext &context, dxp::PatchReport *outReport,
    const std::function<void(const std::string &, RecipeContext &)> *beforeStep,
    const std::function<void(const std::string &, const RecipeStepResult &, RecipeContext &)> *afterStep);

} // namespace dxp::sm5
