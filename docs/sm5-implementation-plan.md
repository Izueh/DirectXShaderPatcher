# SM5 Implementation Plan

## Goal

Add Shader Model 5 support as a separate DXBC-focused library that can parse,
match, rewrite, and serialize SM5 bytecode using a declarative recipe surface
similar to the current SM6 system without forcing SM5 into LLVM-shaped
internals.

The implementation should favor a lossless token representation over direct
binary patching and over a high-level compiler IR.

The first concrete product objective for the SM5 patcher is the shader
`test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso`. It contains IGN,
and the SM5 implementation should be driven toward replicating the existing SM6
IGN patch behavior: replace the IGN path with a sample from a FAST noise
texture.

## Non-Goals

- Do not fold SM5 into the current `src/dxp/sm6` implementation.
- Do not build a full SSA or LLVM-like IR as the foundation for SM5 editing.
- Do not rely on in-place DWORD patching as the main mutation strategy.
- Do not introduce broad shared abstractions before both SM5 and SM6 have a
  proven overlap.

## Recommendation Summary

The core SM5 implementation should be built around a lossless token IR.

That means:

- Parse DXBC containers into known chunks plus pass-through unknown chunks.
- Parse the shader bytecode chunk into structured instructions and operands.
- Preserve enough original encoding detail for deterministic round-trip
  serialization.
- Perform declarative matching and rewriting against the structured token model.
- Re-encode the affected chunk and then rebuild the container instead of
  mutating bytes in place.
- Make the patcher responsible for updating chunk sizes, container-level size
  bookkeeping, and the final DXBC hash whenever content changes.

This keeps the implementation close to the real binary format while giving the
recipe engine stable objects to reason about.

## Why Not Binary-Direct Editing

Binary-direct mutation is attractive for trivial fixed-width edits, but it is a
poor foundation for a declarative language.

Problems:

- instruction widths vary
- declarations and executable instructions share one token stream
- many operands have nested or relative addressing encodings
- edits frequently change instruction lengths
- chunk lengths and offsets must be recomputed
- container-level size and checksum fields must remain consistent
- resource and reflection-related metadata can become inconsistent if the
  mutation layer is not structured

Binary surgery can still exist as an optimization for a few fixed-width cases,
but it should not be the primary execution model.

## Why Not a Full LLVM-Like IR

SM5 DXBC is already a structured token stream. Reconstructing a compiler IR with
SSA, type recovery, dominance, and canonical control flow would be a large and
separate compiler project.

That complexity is not justified for the patcher use case.

What the patcher needs most often is:

- ordered instructions
- structured operands
- stable captures for declarative matching
- safe range replacement and insertion
- resource declaration access
- round-trip serialization

Those needs are served directly by a token IR plus optional analyses.

## Concrete Objective

The implementation should not be developed against abstract mutation examples
alone. It should be driven by one real end-to-end target shader from the start.

Initial objective shader:

- `test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso`

Initial objective behavior:

- detect the IGN pattern in that SM5 pixel shader
- add or reference the resources needed for the replacement path
- replace IGN with a sample from a FAST noise texture
- make the SM5 output match the intended behavior of the existing SM6 IGN patch
  as closely as the target format allows

This objective should influence parser scope, matcher design, emit support, and
the first end-to-end tests. If a planned abstraction does not help reach this
target, it should be deferred.

## Proposed Architecture

Implement SM5 as a separate library rooted under its own implementation area.

Proposed layout:

```text
include/
  dxp/
    sm5/
      Container.h
      Parse.h
      Model.h
      Match.h
      Rewrite.h
      Recipe.h
      Patch.h
src/
  dxp/
    sm5/
      Container.cpp
      Parse.cpp
      Serialize.cpp
      Model.cpp
      Match.cpp
      Rewrite.cpp
      Recipe.cpp
      Patch.cpp
test/
  sm5_container_roundtrip.cpp
  sm5_token_parse.cpp
  sm5_match_basic.cpp
  sm5_rewrite_basic.cpp
  sm5_recipe_prefilter.cpp
  sm5_recipe_rewrite.cpp
```

This keeps SM5 separate from SM6 while leaving room for later convergence at a
small public facade if that becomes justified.

## Core Data Model

### 1. Container Layer

Represent the DXBC container as a chunked file rather than as a raw byte span.

Suggested types:

```text
Sm5Container
  header
  chunks[]

Sm5Chunk
  fourcc
  bytes
  parsedKind
  parsedPayload
```

Requirements:

- preserve chunk order
- preserve unknown chunks byte-for-byte
- parse and expose the shader chunk separately
- parse and expose reflection or resource-related chunks only when needed
- own container rebuild details centrally, including chunk sizes, container
  header bookkeeping, and checksum or hash recomputation

Initially support these chunks explicitly:

- `SHDR` or `SHEX`
- `RDEF` if resource declaration edits are required
- input or output signature chunks only when a concrete recipe requires them

Unknown chunks should round-trip untouched.

### 2. Token Layer

Parse the shader chunk into a structured, lossless instruction stream.

Suggested types:

```text
Sm5Program
  programType
  majorVersion
  minorVersion
  instructions[]

Sm5Instruction
  opcode
  opcodeToken0
  extendedOpcodeTokens[]
  lengthInDwords
  class
  decodedControls
  operands[]
  customData
  sourceSpan

Sm5Operand
  operandToken0
  extendedOperandTokens[]
  type
  numComponents
  selectionMode
  swizzleOrMask
  indexDimension
  indices[]
  modifier
  minPrecision
  immediates[]
  relativeOperand
  sourceSpan
```

Important rule: the source of truth is the token IR, not a derived semantic
view.

The parser should decode fields defined in
`d3d11TokenizedProgramFormat.hpp`, but it should also keep enough raw material
to allow exact re-encoding even before every instruction family is fully
modeled.

### 3. Declaration Overlay

Build lightweight declaration views over the instruction stream.

Suggested derived views:

- resources by `t#`, `u#`, `s#`, `cb#`
- thread group dimensions
- temp register count
- indexable temp declarations
- global flags

These should be computed from the token IR and discarded or rebuilt when the
instruction stream changes.

### 4. Optional Analysis Layer

Do not build these before the mutation work proves they are needed.

Possible later analyses:

- basic block boundaries
- control-flow graph
- register def-use tracking
- declaration cross references into `RDEF`

These should be separate caches, not alternate ownership of the program.

## Declarative Language Strategy

Reuse the current YAML recipe document shape where it helps, but give SM5 its
own backend-native pattern vocabulary.

Good candidates for reuse from SM6:

- `prefilters`
- `rewrite_rules`
- step sequencing
- `First`, `Last`, and `MatchAll` application modes
- captures
- `Replace` and `ReplaceRange`
- resource declaration steps such as `add_texture` and `expect_cbuffer`

SM5-specific differences:

- opcodes should be SM5 opcodes from the tokenized program format, not LLVM or
  `dx.op`
- operands should describe registers, immediates, masks, swizzles, and relative
  indexing directly
- emits should build SM5 instructions and declarations, not calls into a higher
  IR

## Proposed SM5 Recipe Surface

### Match Language

Instruction matching should support:

- `opcode`
- optional opcode controls such as saturate, test boolean, return type, sync
  flags, or resource dimension when relevant
- operand patterns by operand index
- optional instruction capture name

Operand matching should support:

- operand kind or register type
- register index or multi-dimensional indices
- swizzle, mask, or selected component
- source modifier
- immediate values
- relative addressing shape
- capture name
- match-against-capture name

Declaration matching should support:

- resource declarations by register file, bind point, and dimension
- sampler declarations by mode
- constant buffer declarations by slot and size
- thread group declarations

### Emit Language

Instruction emission should support:

- opcode
- opcode controls
- ordered operands
- explicit replacement value or replacement range anchors

Operand emission should support:

- capture reuse
- immediate constants
- explicit register operands
- swizzle and mask construction
- relative index construction

### Example Shape

Illustrative YAML shape:

```yaml
rewrite_rules:
  - match:
      opcode: ld
      capture: ld0
      operands:
        - operand: 0
          kind: temp
          capture: dst
        - operand: 1
          kind: resource
          bind: 3
          space: 0
          capture: tex
    emit:
      - kind: instruction
        name: mov0
        opcode: mov
        operands:
          - operand: 0
            kind: capture
            capture: dst
          - operand: 1
            kind: immediate32
            values: [0, 0, 0, 0]
    replace: mov0
```

The exact syntax can evolve, but the design target is clear: recipes should
operate on decoded SM5 constructs, not on raw DWORD offsets.

## Mutation Model

Use a rewrite plan rather than mutating the token stream ad hoc during matching.

Suggested flow:

1. Parse container.
2. Parse shader chunk into token IR.
3. Run prefilters.
4. Collect matches.
5. Build rewrite actions.
6. Apply rewrite actions to the token IR.
7. Rebuild declaration overlays.
8. Serialize shader chunk.
9. Rebuild container, including updated chunk sizes and container-level size
  bookkeeping.
10. Recompute the DXBC hash over the rebuilt container image.
11. Validate round-trip invariants.

Suggested rewrite action types:

- replace one instruction
- replace instruction range
- insert before instruction
- insert after instruction
- remove instruction range
- append declaration
- rewrite declaration operands or controls

Edits should be applied against stable instruction identities, not byte spans.

## Serialization Rules

The serializer should be deterministic and conservative.

Requirements:

- preserve original chunk order unless a specific edit requires change
- preserve unknown chunks byte-for-byte
- recompute chunk lengths and container offsets
- update any container-level size fields affected by chunk replacement or
  growth
- recompute the final DXBC hash after all content and offset updates are
  complete
- re-encode instructions from decoded fields rather than copying stale bytes
- preserve semantically irrelevant original details only when that falls out of
  the lossless model naturally

During early phases, byte-identical round-trip should be required for inputs
that were parsed without mutation.

## Verification Strategy

Verification should be centered on disassembly, not only on byte-level or
structural checks.

Primary verification command:

```text
F:\software\decompiler\bin\cmd_Decompiler.exe -d <shader.cso>
```

This should be the main verification mechanism throughout SM5 development.

That means:

- after parser work, use disassembly to confirm the decoded instruction stream
  matches the real shader structure
- after round-trip serialization, compare disassembly before and after and
  require no unintended instruction changes
- after rewrites, compare disassembly before and after and verify the intended
  instruction-level transformation actually occurred
- after declaration edits, use disassembly to verify the expected resource and
  declaration changes are visible in the output shader

Other checks remain useful, but they are secondary:

- byte-identical round-trip for untouched shaders
- focused unit tests for parser and matcher behavior
- container integrity checks such as chunk sizes, offsets, and DXBC hash

The development loop should prefer disassembly diffs over guessing from binary
bytes.

## Validation Strategy

Validation should be built into the implementation plan rather than deferred.

### Parser Validation

- decode known shader samples from `test/`
- verify version token, length token, and full instruction walk
- re-encode untouched shaders and require byte-identical shader chunks
- disassemble original and round-tripped shaders with
  `F:\software\decompiler\bin\cmd_Decompiler.exe -d` and require equivalent
  disassembly for untouched inputs

### Match Validation

- unit tests for single instruction opcode matching
- unit tests for operand mask, swizzle, and modifier matching
- unit tests for declaration matching by bind point and resource dimension

### Rewrite Validation

- replace one instruction with same-width encoding
- replace one instruction with different-width encoding
- replace instruction range
- insert a declaration and verify serialization
- verify each rewrite primarily by comparing pre- and post-patch disassembly

### Recipe Validation

- one prefilter-only recipe
- one simple rewrite recipe
- one declaration mutation recipe
- one negative test proving required matches fail cleanly
- one end-to-end IGN recipe against
  `test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso`
  that replaces IGN with a FAST noise sample path

## Phase Plan

### Phase 0: Foundations

Deliverables:

- `src/dxp/sm5` skeleton
- container reader for DXBC
- shader chunk locator
- test harness for SM5 fixtures
- scripted disassembly helper around
  `F:\software\decompiler\bin\cmd_Decompiler.exe -d` for verification

Exit criteria:

- can load a DXBC container and locate `SHDR` or `SHEX`
- can disassemble the objective shader fixture and archive its baseline output

### Phase 1: Lossless Parser and Serializer

Deliverables:

- token decoder for opcode tokens and operand tokens
- structured `Sm5Program`, `Sm5Instruction`, and `Sm5Operand`
- serializer for untouched programs
- baseline disassembly snapshots for the objective shader and at least one
  smaller control fixture

Exit criteria:

- untouched shader chunk round-trips byte-identically on the initial fixture set
- untouched round-trip preserves the decompiler output for the same fixture set

### Phase 2: Declaration Views

Deliverables:

- resource declaration indexing
- cbuffer and sampler declaration indexing
- simple public query helpers

Exit criteria:

- tests can query declarations without inspecting raw instructions

### Phase 3: Match Engine

Deliverables:

- instruction matcher
- operand matcher
- declaration matcher
- capture storage and replay
- first concrete matcher coverage for the IGN structure in
  `Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso`

Exit criteria:

- can collect matches for opcode and binding-based patterns on real fixtures
- can reliably isolate the IGN region in the objective shader without matching
  unrelated code

### Phase 4: Rewrite Engine

Deliverables:

- replace and replace-range support
- insert before or after support
- emitted SM5 instruction construction helpers
- enough emit support to express the FAST noise sample replacement path needed
  by the objective shader

Exit criteria:

- can rewrite a simple shader and re-serialize valid output
- can rewrite the IGN target region in the objective shader and verify the
  change in disassembly

### Phase 5: Recipe Layer

Deliverables:

- SM5 recipe model
- YAML parser for SM5 recipe files
- execution engine and patch entry point
- first SM5 recipe targeting the Aliens Fireteam volumetric IGN shader

Exit criteria:

- one declarative rewrite recipe passes end to end
- the first declarative SM5 IGN-to-FAST-noise recipe passes end to end on
  `Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso`

### Phase 6: Declaration Mutation

Deliverables:

- add or rewrite resource declarations
- update any required declaration-adjacent metadata

Exit criteria:

- one resource-oriented recipe passes end to end

### Phase 7: Optional Analyses

Only add this phase when concrete recipes require it.

Possible scope:

- CFG
- def-use tracking
- reflection consistency helpers

## Initial Public and Internal APIs

Keep the public surface small.

Candidate API shape:

```text
bool ParseSm5Container(...)
bool PatchSm5ContainerInMemory(...)
bool ParseSm5RecipeFile(...)
```

Internal helpers should stay internal until the model stabilizes.

Useful internal seams:

- `ParseSm5Program`
- `SerializeSm5Program`
- `CollectSm5Matches`
- `ApplySm5RewriteRulesOnce`
- `ApplySm5RewriteRulesMatchAll`

## Concrete File Plan

### `Container.cpp`

Responsibilities:

- DXBC header parsing
- chunk table parsing
- known chunk lookup
- container rebuild after chunk replacement
- update chunk sizes and any container-level size bookkeeping
- recompute the final DXBC hash once the rebuilt byte image is finalized

### `Parse.cpp`

Responsibilities:

- decode version and length tokens
- decode instruction stream
- decode declarations and operands
- decode custom-data blocks conservatively

### `Serialize.cpp`

Responsibilities:

- encode instructions and operands
- encode shader chunk
- preserve untouched unknown chunks

### `Match.cpp`

Responsibilities:

- instruction and operand pattern matching
- capture collection
- declaration-based matching

### `Rewrite.cpp`

Responsibilities:

- emitted instruction builders
- rewrite action application
- range replacement logic

### `Recipe.cpp`

Responsibilities:

- recipe execution context
- prefilter handling
- rewrite step orchestration
- mutation bookkeeping

### `Patch.cpp`

Responsibilities:

- top-level load, mutate, serialize, and validate flow

## Use of Existing References

Use existing references selectively.

- `external/WDK/includes/d3d11TokenizedProgramFormat.hpp` should be treated as
  the token decoding contract.
- DXC `dxilconv` and `ShaderBinary` headers should be used as correctness
  references for opcode metadata and operand rules.
- The SM6 recipe surface should be treated as product inspiration, not as an
  implementation dependency.

Avoid mirroring DXC internals unless a specific parser or serializer detail
requires it.

## Risks and Mitigations

### Risk: Partial decoding blocks round-trip

Mitigation:

- keep raw token spans during the first parser milestone
- fail closed on unsupported instruction forms during mutation
- allow parse-only support to land before rewrite support

### Risk: Reflection or resource metadata drifts from declarations

Mitigation:

- separate instruction rewriting from declaration mutation
- add resource declaration edits only after declaration indexing is stable
- gate declaration mutation with focused tests on fixtures that inspect the
  related metadata

### Risk: Recipe DSL becomes too SM6-shaped

Mitigation:

- reuse only the high-level workflow shape
- define SM5-native opcode and operand nouns early
- test the DSL on one purely token-level rewrite before adding conveniences

## Initial Milestone Backlog

Recommended first implementation tickets:

1. Create `src/dxp/sm5` and `include/dxp/sm5` skeletons.
2. Add a helper that runs
  `F:\software\decompiler\bin\cmd_Decompiler.exe -d` and captures baseline
  disassembly for `Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso`.
3. Implement DXBC container parsing and shader chunk lookup.
4. Implement a read-only token walker for `SHDR` and `SHEX`.
5. Add a serializer that round-trips untouched shader chunks.
6. Add declaration indexing for resource and cbuffer declarations.
7. Add opcode and operand match primitives with captures.
8. Add IGN-specific pattern coverage for the objective shader.
9. Add replace-one-instruction and replace-range rewrite support.
10. Add enough SM5 emit support to construct the FAST noise sample path.
11. Add one end-to-end declarative rewrite test for the objective shader.

## Success Criteria

The SM5 implementation is on the right track when all of the following are
true:

- untouched DXBC fixtures round-trip cleanly
- untouched DXBC fixtures preserve decompiler output
- the matcher operates on structured instructions instead of raw byte offsets
- one real SM5 recipe can rewrite the Aliens Fireteam IGN shader end to end
- declaration edits do not require binary-direct patch logic
- the SM5 library remains separate from the SM6 LLVM pipeline

At that point, future overlap between SM5 and SM6 can be evaluated based on
evidence instead of being forced up front.