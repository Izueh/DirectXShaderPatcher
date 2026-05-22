# SM5 Tokenizer Spec Assessment Checklist

Date: 2026-05-21
Scope: assess parser/model/serializer coverage against external/WDK/includes/d3d11TokenizedProgramFormat.hpp.

## Executive Status

- Current state is robust partial semantic support plus strong raw-token round-trip behavior.
- Full tokenizer-spec semantic coverage is not implemented yet.
- Most shader instructions can still be parsed and emitted safely when left unmodified because raw instruction tokens are preserved.

## Evidence Anchors

- Opcode name and parse table is a curated subset: src/dxp/sm5/Model.cpp.
- Generic instruction parser plus overlay extraction: src/dxp/sm5/Parse.cpp.
- Serializer prefers raw tokens, with custom semantic encode for a few declarations: src/dxp/sm5/Serialize.cpp.
- Data model currently stores limited semantic fields and raw tokens: include/dxp/sm5/Model.h.
- Spec source: external/WDK/includes/d3d11TokenizedProgramFormat.hpp.

## Coverage Checklist

Legend: [x] implemented, [~] partial, [ ] missing

### 1) Opcode universe and names

- [x] Generic opcode field decode from token bits.
- [~] String name mapping and text parse support for opcodes.
- [ ] Full name table coverage for all D3D10, D3D10.1, and D3D11 opcodes in spec.

Notes:
- Model opcode table has about 61 mapped names.
- Spec exposes far more opcodes across D3D10 and D3D11 opcode families.

### 2) Instruction controls and extended opcode tokens

- [x] Base controls extracted (saturate, test boolean, precise values, resinfo return type, sync flags).
- [x] Extended opcode token chain detected and captured as extended opcode types.
- [~] Extended opcode payload semantics interpreted per opcode family.
- [ ] Full per-opcode validation of control-field legality and encoding rules.

### 3) Operand decode

- [x] Operand type, index dimension, common immediate index forms, relative addressing recursion.
- [x] Operand extended tokens retained in raw tokens.
- [~] Operand modifier extraction from extended operand token.
- [~] Component selection handling (current decode path is swizzle-centric and not fully mode-aware).
- [ ] Full mode-aware decode/encode of mask, swizzle, and select1 component selectors.
- [ ] Immediate64 literal operand payload decode into semantic fields.

### 4) Operand encode

- [x] Raw operand-token passthrough when available.
- [~] Constructed operand token encoding for common immediate32 and indexed forms.
- [ ] Full index representation matrix support for all immediate and relative combinations.
- [ ] Full immediate64 literal encode path.
- [ ] Full fidelity for extended operand variants beyond modifier.

### 5) Declaration semantic overlays in Program model

- [x] dcl_temps, dcl_resource, dcl_constant_buffer, dcl_sampler, dcl_thread_group basic extraction.
- [~] dcl_global_flags currently wired through sync-flags field path.
- [ ] Full DCL family overlays (input/output semantic variants, index range, GS topology, tessellation, interfaces, UAV/resource raw/structured, TGSM declarations, stream/function declarations).

### 6) Declaration encoding for constructed instructions

- [x] dcl_resource semantic encoder.
- [x] dcl_constant_buffer semantic encoder.
- [x] dcl_temps semantic encoder.
- [ ] dcl_sampler semantic encoder.
- [ ] Full semantic encoders for all remaining declaration opcodes.

### 7) Custom data blocks

- [x] Custom data instruction block retained as raw payload.
- [ ] Class-specific custom data parsing by D3D10_SB_CUSTOMDATA_CLASS.
- [ ] Class-specific custom data construction/serialization helpers.

### 8) Program model completeness

- [x] Lossless containers for raw tokens on instructions and operands.
- [~] Selected declaration data surfaced in Program fields.
- [ ] First-class data structures for full declaration/control semantic space.
- [ ] Model-level invariants and validation for spec constraints.

### 9) Validation and test coverage

- [x] Tests exist for recipe runtime capabilities added in current phases.
- [~] Parser/serializer tests for currently supported declaration subset.
- [ ] Spec-conformance matrix tests for opcode/declaration/operand/customdata categories.
- [ ] Round-trip and mutate-then-emit golden tests for newly added semantic paths.

## Gap Risk Assessment

- Low risk for no-op round-trip of many unknown instructions due to raw-token preservation.
- Medium risk when mutating instructions that depend on unsupported semantic encode paths.
- Higher risk for uncommon operand encodings, immediate64 literals, and advanced D3D11 declaration families.

## Prioritized Implementation Plan

### Phase A: Safe completeness of text-facing opcode support

- Expand src/dxp/sm5/Model.cpp opcode name table to cover the full opcode enum set used in spec.
- Add tests that parse and print every known opcode name accepted by recipe text.

### Phase B: Operand correctness foundation

- Implement explicit operand component selector mode decode and encode.
- Add immediate64 literal decode/encode support.
- Add tests for index representation combinations and nested relative addressing.

### Phase C: Declaration family expansion

- Add semantic overlays and encoders for missing DCL families in priority order:
  1) sampler/input/output semantic variants,
  2) GS/tessellation declarations,
  3) UAV/resource raw/structured and TGSM declarations,
  4) interface/function/stream declarations.
- Add per-family parse and serialize golden tests.

### Phase D: Extended opcode and custom-data semantics

- Add opcode-family-specific handling for extended opcode payload semantics.
- Add class-aware custom-data decode helpers while keeping raw fallback.
- Add mutate-and-reencode tests for each supported custom-data class.

### Phase E: Conformance harness

- Build a tokenizer conformance checklist test target mapped to spec sections.
- Require green status for all supported sections and explicit skip list for intentional non-goals.

## Definition of Done for Full Semantic Coverage Claim

All of the following should hold:

- Full opcode text mapping and parsing coverage.
- Full operand component/index/immediate semantics decode and encode.
- Full DCL opcode semantic decode and encode coverage.
- Extended opcode payload semantics handled where spec defines them.
- Custom-data classes decoded or intentionally marked opaque with tests.
- Conformance checklist tests passing in CI with no unknown semantic gaps.
