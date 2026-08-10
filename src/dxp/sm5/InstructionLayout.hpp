#pragma once

// Instruction layout table derived from the dxbc-spirv reference implementation
// (github.com/doitsujin/dxbc-spirv, Copyright (c) 2025 Philip Rebohle), MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <array>
#include <cstdint>

#include "dxp/sm5/Model.hpp"

namespace dxp::sm5 {
using namespace dxp::sm5::model;

using Role = OperandRole;

// Flat instruction layout array indexed by Opcode value (0–235), generated from
// dxbc-spirv's g_instructionLayouts (dxbc/dxbc_parser.cpp). Mapping:
//   OperandKind::eDstReg → Role::Destination
//   OperandKind::eSrcReg / eImm32 → Role::Source
//   ir::ScalarType → OperandScalarType (per operand slot)
// Keep in lockstep with the Opcode enum (236 token opcodes, 0..235).
inline const std::array<InstructionLayout, kOpcodeCount> kInstructionLayouts = {{
    // 0: Add
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 1: And
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 2: Break
    {},
    // 3: BreakC
    {.roles = {Role::Source}, .types = {OperandScalarType::Bool}, .role_count = 1},
    // 4: Call
    {.roles = {Role::Source}, .types = {OperandScalarType::Unknown}, .role_count = 1},
    // 5: CallC
    {.roles = {Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::Unknown}, .role_count = 2},
    // 6: Case
    {.roles = {Role::Source}, .types = {OperandScalarType::U32}, .role_count = 1},
    // 7: Continue
    {},
    // 8: ContinueC
    {.roles = {Role::Source}, .types = {OperandScalarType::Bool}, .role_count = 1},
    // 9: Cut
    {},
    // 10: Default
    {},
    // 11: DerivRTX
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 12: DerivRTY
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 13: Discard
    {.roles = {Role::Source}, .types = {OperandScalarType::Bool}, .role_count = 1},
    // 14: Div
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 15: DP2
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 16: DP3
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 17: DP4
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 18: Else
    {},
    // 19: Emit
    {},
    // 20: EmitThenCut
    {},
    // 21: EndIf
    {},
    // 22: EndLoop
    {},
    // 23: EndSwitch
    {},
    // 24: Eq
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 25: Exp
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 26: Frc
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 27: Ftoi
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::I32, OperandScalarType::F32}, .role_count = 2},
    // 28: Ftou
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::F32}, .role_count = 2},
    // 29: Ge
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 30: IAdd
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 31: If
    {.roles = {Role::Source}, .types = {OperandScalarType::Bool}, .role_count = 1},
    // 32: IEq
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 33: IGe
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::I32, OperandScalarType::I32}, .role_count = 3},
    // 34: ILt
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::I32, OperandScalarType::I32}, .role_count = 3},
    // 35: IMad
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 36: IMax
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::I32, OperandScalarType::I32, OperandScalarType::I32}, .role_count = 3},
    // 37: IMin
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::I32, OperandScalarType::I32, OperandScalarType::I32}, .role_count = 3},
    // 38: IMul
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::I32, OperandScalarType::I32, OperandScalarType::I32, OperandScalarType::I32}, .role_count = 4},
    // 39: INe
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 40: INeg
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::I32, OperandScalarType::I32}, .role_count = 2},
    // 41: IShl
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 42: IShr
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::I32, OperandScalarType::I32, OperandScalarType::U32}, .role_count = 3},
    // 43: Itof
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::I32}, .role_count = 2},
    // 44: Label
    {.roles = {Role::Destination}, .types = {OperandScalarType::Unknown}, .role_count = 1},
    // 45: Ld
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::U32, OperandScalarType::Texture}, .role_count = 3},
    // 46: LdMs
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::Texture, OperandScalarType::U32}, .role_count = 4},
    // 47: Log
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 48: Loop
    {},
    // 49: Lt
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 50: Mad
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 4},
    // 51: Min
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 52: Max
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 53: CustomData
    {},
    // 54: Mov
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::Unknown}, .role_count = 2},
    // 55: MovC
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::Bool, OperandScalarType::Unknown, OperandScalarType::Unknown}, .role_count = 4},
    // 56: Mul
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 57: Ne
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 58: Nop
    {},
    // 59: Not
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32}, .role_count = 2},
    // 60: Or
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 61: Resinfo
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::Unknown}, .role_count = 3},
    // 62: Ret
    {},
    // 63: RetC
    {.roles = {Role::Source}, .types = {OperandScalarType::Bool}, .role_count = 1},
    // 64: RoundNe
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 65: RoundNi
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 66: RoundPi
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 67: RoundZ
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 68: Rsq
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 69: Sample
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler}, .role_count = 4},
    // 70: SampleC
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32}, .role_count = 5},
    // 71: SampleCLz
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32}, .role_count = 5},
    // 72: SampleL
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32}, .role_count = 5},
    // 73: SampleD
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 6},
    // 74: SampleB
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32}, .role_count = 5},
    // 75: Sqrt
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 76: Switch
    {.roles = {Role::Source}, .types = {OperandScalarType::U32}, .role_count = 1},
    // 77: Sincos
    {.roles = {Role::Destination, Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 3},
    // 78: UDiv
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 79: ULt
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 80: UGe
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 81: UMul
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 82: UMad
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 83: UMax
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 84: UMin
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 85: UShr
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 86: Utof
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::U32}, .role_count = 2},
    // 87: Xor
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 88: DclResource
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 89: DclConstantBuffer
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::CBuffer, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 90: DclSampler
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Sampler, OperandScalarType::U32}, .role_count = 2},
    // 91: DclIndexRange
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32}, .role_count = 2},
    // 92: DclGsOutputPrimitiveTopology
    {},
    // 93: DclGsInputPrimitive
    {},
    // 94: DclMaxOutputVertexCount
    {.roles = {Role::Source}, .types = {OperandScalarType::U32}, .role_count = 1},
    // 95: DclInput
    {.roles = {Role::Destination}, .types = {OperandScalarType::Unknown}, .role_count = 1},
    // 96: DclInputSgv
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32}, .role_count = 2},
    // 97: DclInputSiv
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32}, .role_count = 2},
    // 98: DclInputPs
    {.roles = {Role::Destination}, .types = {OperandScalarType::Unknown}, .role_count = 1},
    // 99: DclInputPsSgv
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32}, .role_count = 2},
    // 100: DclInputPsSiv
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32}, .role_count = 2},
    // 101: DclOutput
    {.roles = {Role::Destination}, .types = {OperandScalarType::Unknown}, .role_count = 1},
    // 102: DclOutputSgv
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32}, .role_count = 2},
    // 103: DclOutputSiv
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32}, .role_count = 2},
    // 104: DclTemps
    {.roles = {Role::Source}, .types = {OperandScalarType::U32}, .role_count = 1},
    // 105: DclIndexableTemp
    {.roles = {Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 106: DclGlobalFlags
    {},
    // 107: Reserved0
    {},
    // 108: Lod
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler}, .role_count = 4},
    // 109: Gather4
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler}, .role_count = 4},
    // 110: SamplePos
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::Unknown, OperandScalarType::U32}, .role_count = 3},
    // 111: SampleInfo
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown}, .role_count = 2},
    // 112: Reserved1
    {},
    // 113: HsDecls
    {},
    // 114: HsControlPointPhase
    {},
    // 115: HsForkPhase
    {},
    // 116: HsJoinPhase
    {},
    // 117: EmitStream
    {.roles = {Role::Destination}, .types = {OperandScalarType::Unknown}, .role_count = 1},
    // 118: CutStream
    {.roles = {Role::Destination}, .types = {OperandScalarType::Unknown}, .role_count = 1},
    // 119: EmitThenCutStream
    {.roles = {Role::Destination}, .types = {OperandScalarType::Unknown}, .role_count = 1},
    // 120: InterfaceCall
    {.roles = {Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown}, .role_count = 2},
    // 121: Bufinfo
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown}, .role_count = 2},
    // 122: DerivRTXCoarse
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 123: DerivRTXFine
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 124: DerivRTYCoarse
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 125: DerivRTYFine
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 126: Gather4C
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32}, .role_count = 5},
    // 127: Gather4PO
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::F32, OperandScalarType::I32, OperandScalarType::Texture, OperandScalarType::Sampler}, .role_count = 5},
    // 128: Gather4POC
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::F32, OperandScalarType::I32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32}, .role_count = 6},
    // 129: Rcp
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 130: F32ToF16
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::F32}, .role_count = 2},
    // 131: F16ToF32
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::U32}, .role_count = 2},
    // 132: UAddC
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 133: USubb
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 134: CountBits
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32}, .role_count = 2},
    // 135: FirstBitHi
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::I32, OperandScalarType::U32}, .role_count = 2},
    // 136: FirstBitLo
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::I32, OperandScalarType::U32}, .role_count = 2},
    // 137: FirstBitSHI
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::I32, OperandScalarType::I32}, .role_count = 2},
    // 138: UBFE
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 139: IBFE
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::I32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::I32}, .role_count = 4},
    // 140: BFI
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 5},
    // 141: BFRev
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32}, .role_count = 2},
    // 142: SwapC
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::Unknown, OperandScalarType::Bool, OperandScalarType::Unknown, OperandScalarType::Unknown}, .role_count = 5},
    // 143: DclStream
    {.roles = {Role::Destination}, .types = {OperandScalarType::Unknown}, .role_count = 1},
    // 144: DclFunctionBody
    {.roles = {Role::Source}, .types = {OperandScalarType::Unknown}, .role_count = 1},
    // 145: DclFunctionTable
    {.roles = {Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::Unknown}, .role_count = 2},
    // 146: DclInterface
    {.roles = {Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::Unknown, OperandScalarType::Unknown}, .role_count = 3},
    // 147: DclInputControlPointCount
    {},
    // 148: DclOutputControlPointCount
    {},
    // 149: DclTessDomain
    {},
    // 150: DclTessPartitioning
    {},
    // 151: DclTessOutputPrimitive
    {},
    // 152: DclHsMaxTessfactor
    {.roles = {Role::Source}, .types = {OperandScalarType::F32}, .role_count = 1},
    // 153: DclHsForkPhaseInstanceCount
    {.roles = {Role::Source}, .types = {OperandScalarType::U32}, .role_count = 1},
    // 154: DclHsJoinPhaseInstanceCount
    {.roles = {Role::Source}, .types = {OperandScalarType::U32}, .role_count = 1},
    // 155: DclThreadGroup
    {.roles = {Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 156: DclUnorderedAccessViewTyped
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Uav, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 157: DclUnorderedAccessViewRaw
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Uav, OperandScalarType::U32}, .role_count = 2},
    // 158: DclUnorderedAccessViewStructured
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Uav, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 159: DclThreadGroupSharedMemoryRaw
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32}, .role_count = 2},
    // 160: DclThreadGroupSharedMemoryStructured
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 161: DclResourceRaw
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::U32}, .role_count = 2},
    // 162: DclResourceStructured
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Texture, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 163: LdUavTyped
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::I32, OperandScalarType::Uav}, .role_count = 3},
    // 164: StoreUavTyped
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Uav, OperandScalarType::U32, OperandScalarType::Unknown}, .role_count = 3},
    // 165: LdRaw
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::Unknown}, .role_count = 3},
    // 166: StoreRaw
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::Unknown}, .role_count = 3},
    // 167: LdStructured
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::Unknown}, .role_count = 4},
    // 168: StoreStructured
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::Unknown}, .role_count = 4},
    // 169: AtomicAnd
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 170: AtomicOr
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 171: AtomicXor
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 172: AtomicCmpStore
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 173: AtomicIAdd
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 174: AtomicIMax
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 175: AtomicIMin
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 176: AtomicUMax
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 177: AtomicUMin
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 3},
    // 178: ImmAtomicAlloc
    {.roles = {Role::Destination, Role::Destination}, .types = {OperandScalarType::U32, OperandScalarType::Uav}, .role_count = 2},
    // 179: ImmAtomicConsume
    {.roles = {Role::Destination, Role::Destination}, .types = {OperandScalarType::U32, OperandScalarType::Uav}, .role_count = 2},
    // 180: ImmAtomicIAdd
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 181: ImmAtomicAnd
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 182: ImmAtomicOr
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 183: ImmAtomicXor
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 184: ImmAtomicExch
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 185: ImmAtomicCmpExch
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 5},
    // 186: ImmAtomicIMax
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 187: ImmAtomicIMin
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 188: ImmAtomicUMax
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 189: ImmAtomicUMin
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 190: Sync
    {},
    // 191: DAdd
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::F64, OperandScalarType::F64}, .role_count = 3},
    // 192: DMax
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::F64, OperandScalarType::F64}, .role_count = 3},
    // 193: DMin
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::F64, OperandScalarType::F64}, .role_count = 3},
    // 194: DMul
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::F64, OperandScalarType::F64}, .role_count = 3},
    // 195: DEq
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::F64, OperandScalarType::F64}, .role_count = 3},
    // 196: DGe
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::F64, OperandScalarType::F64}, .role_count = 3},
    // 197: DLt
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::F64, OperandScalarType::F64}, .role_count = 3},
    // 198: DNe
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::F64, OperandScalarType::F64}, .role_count = 3},
    // 199: DMov
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::F64}, .role_count = 2},
    // 200: DMovC
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::Bool, OperandScalarType::F64, OperandScalarType::F64}, .role_count = 4},
    // 201: DToF
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F64}, .role_count = 2},
    // 202: FToD
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::F32}, .role_count = 2},
    // 203: EvalSnapped
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::I32}, .role_count = 3},
    // 204: EvalSampleIndex
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::U32}, .role_count = 3},
    // 205: EvalCentroid
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F32, OperandScalarType::F32}, .role_count = 2},
    // 206: DclGsInstanceCount
    {.roles = {Role::Source}, .types = {OperandScalarType::U32}, .role_count = 1},
    // 207: Abort
    {},
    // 208: DebugBreak
    {},
    // 209: Reserved0209
    {},
    // 210: DDiv
    {.roles = {Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::F64, OperandScalarType::F64}, .role_count = 3},
    // 211: DFma
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::F64, OperandScalarType::F64, OperandScalarType::F64}, .role_count = 4},
    // 212: DRcp
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::F64}, .role_count = 2},
    // 213: MSAD
    {.roles = {Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32}, .role_count = 4},
    // 214: DToI
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::I32, OperandScalarType::F64}, .role_count = 2},
    // 215: DToU
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::U32, OperandScalarType::F64}, .role_count = 2},
    // 216: IToD
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::I32}, .role_count = 2},
    // 217: UToD
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::F64, OperandScalarType::U32}, .role_count = 2},
    // 218: Reserved0218
    {},
    // 219: Gather4Feedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler}, .role_count = 5},
    // 220: Gather4CFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32}, .role_count = 6},
    // 221: Gather4POFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::F32, OperandScalarType::I32, OperandScalarType::Texture, OperandScalarType::Sampler}, .role_count = 6},
    // 222: Gather4POCFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::F32, OperandScalarType::I32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32}, .role_count = 7},
    // 223: LdFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::Unknown}, .role_count = 4},
    // 224: LdMsFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::Texture, OperandScalarType::U32}, .role_count = 5},
    // 225: LdUavTypedFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::Uav}, .role_count = 4},
    // 226: LdRawFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::I32, OperandScalarType::U32}, .role_count = 4},
    // 227: LdStructuredFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::U32, OperandScalarType::Unknown}, .role_count = 5},
    // 228: SampleLFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32}, .role_count = 6},
    // 229: SampleCLzFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32}, .role_count = 6},
    // 230: SampleClampFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32}, .role_count = 6},
    // 231: SampleBClampFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 7},
    // 232: SampleDClampFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 8},
    // 233: SampleCClampFeedback
    {.roles = {Role::Destination, Role::Destination, Role::Source, Role::Source, Role::Source, Role::Source, Role::Source}, .types = {OperandScalarType::Unknown, OperandScalarType::U32, OperandScalarType::F32, OperandScalarType::Texture, OperandScalarType::Sampler, OperandScalarType::F32, OperandScalarType::F32}, .role_count = 7},
    // 234: CheckAccessFullyMapped
    {.roles = {Role::Destination, Role::Source}, .types = {OperandScalarType::Bool, OperandScalarType::U32}, .role_count = 2},
    // 235: Reserved0235
    {},
}};

static_assert(kInstructionLayouts.size() == kOpcodeCount);

}  // namespace dxp::sm5
