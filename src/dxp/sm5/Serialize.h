#pragma once

#include "Model.h"

#include <cstdint>
#include <vector>

namespace dxp::sm5 {

std::vector<uint32_t> EncodeInstruction(const Instruction &instruction);

std::vector<uint32_t> EncodeOperand(const Operand &operand);

bool RebuildShaderChunk(const Program &program, std::vector<uint8_t> &outData);

} // namespace dxp::sm5
