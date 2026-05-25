#pragma once

#include "Model.h"

#include <cstdint>
#include <vector>

namespace dxp::sm5 {

/// @brief Encodes one instruction into its DWORD representation.
/// @param instruction Instruction to encode.
/// @return Encoded instruction DWORDs.
std::vector<uint32_t> EncodeInstruction(const Instruction &instruction);

/// @brief Encodes one operand into its DWORD representation.
/// @param operand Operand to encode.
/// @return Encoded operand DWORDs.
std::vector<uint32_t> EncodeOperand(const Operand &operand);

/// @brief Rebuilds a shader chunk payload from a decoded program.
/// @param program Program to serialize.
/// @param outData Receives the rebuilt chunk payload bytes.
/// @return `true` on success, or `false` when serialization fails.
bool RebuildShaderChunk(const Program &program, std::vector<uint8_t> &outData);

} // namespace dxp::sm5
