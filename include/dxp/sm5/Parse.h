#pragma once

#include "Model.h"
#include "Container.h"

#include <cstdint>
#include <string>
#include <vector>

namespace dxp::sm5 {

/// Parse a shader chunk from a DXBC container and populate the program.
/// Returns false if the shader chunk is missing or parse fails.
bool ParseShaderChunk(const Container &container, Program &program);

/// Parse a shader program from raw bytecode data.
/// data: pointer to shader bytecode (VerTok at start)
/// size: size in bytes
bool ParseProgram(const uint8_t *data, uint32_t size, Program &program);

/// Get the shader bytecode pointer and size from a container.
/// Returns {nullptr, 0} if no shader chunk found.
std::pair<const uint8_t *, uint32_t> GetShaderBytecode(const Container &container);

} // namespace dxp::sm5
