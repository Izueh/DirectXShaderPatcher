#pragma once

#include "Model.h"

#include <cstdint>
#include <vector>

namespace dxp::sm5 {

/// Serialize a parsed SM5 program back to bytecode.
/// Produces deterministic output suitable for lossless round-trip.
/// program: the parsed program to serialize
/// outBytes: output bytecode buffer
/// Returns false on serialization failure.
bool SerializeProgram(const Program &program, std::vector<uint8_t> &outBytes);

/// Serialize a single instruction to its encoded dword representation.
/// instruction: the instruction to encode
/// Returns the encoded dwords.
std::vector<uint32_t> EncodeInstruction(const Instruction &instruction);

/// Serialize a single operand to its encoded dword representation.
/// operand: the operand to encode
/// Returns the encoded dwords.
std::vector<uint32_t> EncodeOperand(const Operand &operand);

/// Rebuild a shader chunk from a program, including proper header.
/// The output includes the chunk payload only (not the DXBC chunk header).
/// program: the program to serialize
/// outData: output chunk data
/// Returns false on failure.
bool RebuildShaderChunk(const Program &program, std::vector<uint8_t> &outData);

} // namespace dxp::sm5
