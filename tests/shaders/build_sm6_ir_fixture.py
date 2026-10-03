"""Offline assembler for LLVM shapes that HLSL/DXC lowers away.

This is never run by CMake or CTest. See README.md for regeneration.
"""

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def generate_ir():
    lines = [
        '%Inner = type { float, i32 }',
        '%Outer = type { %Inner, i32 }',
        '%ArrOuter = type { [3 x float] }',
        '%opaque_t = type opaque',
        'declare void @opaque_fixture(%opaque_t*)',
        'declare float @dx.op.unary.f32(i32, float)',
        'define void @main() {',
        '  %frc = call float @dx.op.unary.f32(i32 22, float 0.375)',
    ]
    counter = 0

    def binary(prefix, ty, lhs, rhs, opcode=None):
        nonlocal counter
        counter += 1
        name = f'{prefix}_{counter}'
        if opcode is None:
            opcode = 'fadd' if any(ty == item or ty.endswith(f'x {item}>') for item in ('half', 'float', 'double')) else 'add'
        lines.append(f'  %{name} = {opcode} {ty} {lhs}, {rhs}')
        return f'%{name}'

    def vector(ty, values):
        return '<' + ', '.join(f'{ty} {value}' for value in values) + '>'

    # Consumer pairs let Execute's replacement be observed without test-created IR.
    for ty in ('i1', 'i8', 'i16', 'i32', 'i64', 'half', 'float', 'double'):
        zero = '0.0' if ty in ('half', 'float', 'double') else '0'
        for _ in range(2):
            original = binary('width_original', ty, zero, zero)
            binary('width_user', ty, original, zero)
        for _ in range(16):
            original = binary('resolution_original', ty, zero, zero)
            binary('resolution_user', ty, original, zero)

    for ty, values in (('i32', ('1', '7', '3', '4')), ('i32', ('1', '7', '3', '4')),
                       ('i32', ('7',) * 4), ('float', ('0.25', '0.375'))):
        binary('resolution_match', f'<{len(values)} x {ty}>', vector(ty, values), 'zeroinitializer')
    binary('resolution_match', '<2 x float>', 'zeroinitializer', 'zeroinitializer', 'fadd')

    for ty in ('i1', 'i8', 'i16', 'i32', 'i64', 'half', 'float', 'double'):
        floating = ty in ('half', 'float', 'double')
        bits = {'half': 16, 'float': 32, 'double': 64}.get(ty, int(ty[1:]) if not floating else 0)
        first = '0.375' if floating else '1'
        second = '-0.0' if floating else str(1 << (bits - 1))
        zero = '0.0' if floating else '0'
        binary('export_fixture', ty, first, zero)
        for values in ((first,) * 3, (first, second, zero)):
            binary('export_fixture', f'<3 x {ty}>', vector(ty, values), 'zeroinitializer', 'fadd' if floating else 'add')
        binary('export_fixture', f'<3 x {ty}>', 'zeroinitializer', 'zeroinitializer', 'fadd' if floating else 'add')

    for bits in (1, 8, 16, 32, 64):
        ty = f'i{bits}'
        maximum = str((1 << bits) - 1)
        binary('literal_fixture', ty, maximum, '0', 'xor')
        binary('literal_fixture', f'<2 x {ty}>', vector(ty, (maximum, '0')), 'zeroinitializer', 'xor')
        if bits == 64:
            binary('literal_fixture', ty, str((1 << 63) - 1), '0', 'xor')
    binary('false_fixture', 'i1', '0', '0', 'xor')
    lines.extend(['  ret void', '}', 'define void @array_matching() {'])
    binary('array_scalar_add', 'i8', '1', '0')
    binary('array_short_add', '<2 x i8>', vector('i8', ('1', '2')), 'zeroinitializer')
    add = binary('array_add', '<4 x i8>', vector('i8', ('1', '2', '3', '4')), 'zeroinitializer')
    binary('array_mul', '<4 x i8>', add, 'zeroinitializer', 'mul')
    binary('array_fadd', 'double', '0.375', '0.0')
    binary('array_vector_fadd', '<2 x double>', vector('double', ('0.25', '0.375')), 'zeroinitializer')
    binary('array_long_fadd', '<3 x double>', vector('double', ('0.25', '0.375', '0.5')), 'zeroinitializer')
    binary('rounded_float', 'float', '0x3FB99999A0000000', '0.25', 'frem')
    binary('unsigned_bits', 'i8', '255', '0', 'xor')
    lines.extend(['  ret void', '}', 'define void @extract_matching(%Outer %outer, %ArrOuter %arr_outer, [10 x i32] %index_array) {'])
    for aggregate, path in (('%Outer', '0'), ('%Outer', '0, 1'), ('%Outer', '0, 0'),
                            ('%ArrOuter', '0, 1'), ('{ float, i32 }', '1')):
        counter += 1
        lines.append(f'  %extract_fixture_{counter} = extractvalue {aggregate} undef, {path}')
    binary('non_extract', 'i32', '7', '0')
    for ty in ('float', 'i32', 'i64', '<4 x float>', '[3 x float]', '%Inner', '%Outer'):
        for _ in range(4):
            counter += 1
            lines.append(f'  %extract_fixture_{counter} = extractvalue {{ {ty} }} undef, 0')
        for _ in range(24 if ty == 'i32' else 4):
            counter += 1
            lines.append(f'  %extract_original_{counter} = extractvalue {{ {ty} }} undef, 0')
            lines.append(f'  %extract_user_{counter} = insertvalue {{ {ty} }} undef, {ty} %extract_original_{counter}, 0')
    lines.extend([
        '  ret void', '}',
        '!dx.version = !{!0}', '!dx.valver = !{!0}', '!dx.shaderModel = !{!1}',
        '!dx.entryPoints = !{!2}', '!0 = !{i32 1, i32 6}',
        '!1 = !{!"cs", i32 6, i32 6}',
        '!2 = !{void ()* @main, !"main", null, null, !3}',
        '!3 = !{i32 4, !4}', '!4 = !{i32 1, i32 1, i32 1}',
    ])
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dxopt', required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as temp:
        source = Path(temp) / 'sm6_rule_edge_cases.ll'
        bitcode = source.with_suffix('.bc')
        source.write_text(generate_ir(), encoding='utf-8')
        subprocess.run([str(args.dxopt.resolve()), f'-o={bitcode}', str(source), '-verify'], check=True)
        code = bitcode.read_bytes()
    # Minimal DXIL container. LLVM shape fixtures are not GPU-valid shaders;
    # Execute tests inspect LLVM output and never serialize these fixtures.
    size = 24 + len(code)
    padding = bytes(-size % 4)
    program = struct.pack('<II4sIII', (5 << 16) | 0x66, (size + len(padding)) // 4,
                          b'DXIL', 0x106, 16, len(code)) + code + padding
    part = b'DXIL' + struct.pack('<I', len(program)) + program
    container = b'DXBC' + bytes(16) + struct.pack('<HHIII', 1, 0, 36 + len(part), 1, 36) + part
    Path(__file__).with_name('sm6_rule_edge_cases.cso').write_bytes(container)


if __name__ == '__main__':
    main()
