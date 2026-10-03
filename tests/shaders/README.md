The SM6 rule regression sample is compiled ahead of time. Tests read the
checked-in `.cso`; they do not invoke DXC or require `dxcompiler.dll`.

Regenerate it from the repository root with DXC:

```powershell
dxc tests/shaders/sm6_rule_patterns.hlsl -T cs_6_6 -E main -Fo tests/shaders/sm6_rule_patterns.cs_6_6.cso
```

The sample has one `Frc` call, a `TextureLoad` at `t7`, cbuffer loads,
`extractvalue` instructions, and arithmetic that feeds a buffer write. These
patterns exercise recipe matching, replacement, environment constants and
extraction using live shader code.

`sm6_rule_edge_cases.cso` is a separate LLVM fixture for shapes DXC removes
when lowering HLSL: nested aggregates, arrays, vectors, i1/i8 arithmetic, and
numeric boundaries. It is not a GPU-valid shader. Its tests call step
`Execute()` and inspect replacements, operands, types, full extraction paths,
exports, errors, and LLVM verification. Test setup only finds existing
instructions; it does not insert IR.

Regenerate that fixture offline with the DXC optimizer/assembler:

```powershell
python tests/shaders/build_sm6_ir_fixture.py --dxopt path/to/dxopt.exe
```

Neither regeneration command is part of the test build or test run. SM6 tests
still use the LLVM/DXIL code linked into `dxpatcher`, which is also used by
production `Execute()`.
