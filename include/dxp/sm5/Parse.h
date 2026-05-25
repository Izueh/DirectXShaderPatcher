#pragma once

#include "Container.h"
#include "Model.h"

#include <cstdint>
#include <vector>

namespace dxp::sm5 {

/// @brief Parses the shader chunk from a DXBC container.
/// @param container Parsed DXBC container.
/// @param program Receives the decoded SM5 program.
/// @return `true` on success, or `false` when the shader chunk is missing or
/// parsing fails.
bool ParseShaderChunk(const Container &container, Program &program);

} // namespace dxp::sm5
