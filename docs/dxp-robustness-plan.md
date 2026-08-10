# DXP Robustness Plan — major bug-fix patch

Status: **v0.1.2 iteration approved — implementation in progress.**
Scope: `DirectXShaderPatcher` (dxp) SM5 recipe engine, plus the release/delivery
path into Luma-Framework's vcpkg port.

---

## v0.1.2 iteration — decisions locked (supersedes scoping below)

Target: close the extended-opcode gap that made Flugan fail to disassemble
dxp-emitted shaders. Reproduced concretely: the non-canonical bare `ld`
(`0700002d`, no ResourceDim + ResourceReturnType extended pair — what dxp
emitted before the Fix-A diff) makes Flugan (`f:/software/decompiler/bin/
cmd_Decompiler.exe -d`) fail with `Error: 80004005`; the canonical form
round-trips. Fixed scope:

1. **Flugan gate in CI** — `cmd_Decompiler.exe -d` is headless (writes `.asm`
   beside the input, nonzero exit + `80004005` on failure). Gate: every
   patched corpus shader must disassemble (exit 0) and re-parse with dxp.
2. **`sample_controls {u,v,w}` is IN** the A2 YAML emit form (4-bit 2's
   complement offsets), alongside `resource_dim` / `resource_return_type` /
   raw forms.
3. **Strict errors** — incorrect emits are hard errors, at recipe-validate
   time where the recipe alone suffices, at apply time (exact step/rule/
   operand path) otherwise. No escape hatch.
4. **Error-struct migration is complete, breaking is acceptable** — all 139
   `std::expected<T, std::string>` sites across 20 files migrate to
   `std::expected<T, dxp::Error>`; NO `std::string` compatibility path. The
   API break rides the coordinated vcpkg port bump (Luma rebuilds the addon
   against the new SDK).

Order: land Fix A + tests → structured extended-opcode decode → strict
emit/validate errors (validate-first) → A2 YAML emit forms → Flugan +
determinism + self-round-trip gates → full error-struct migration → tag
`v0.1.2` → port bump → Luma rebuild + in-game verify.

---

Original plan (below) is retained for reference; items not listed in the
v0.1.2 iteration (ld_structured/ld_raw/resinfo emit, CLI dump, SM6, and the
out-of-scope list) remain deferred.

---

## 0. Context — fixes shipped this session and how they interlock

Three fixes landed (in two repos) to make the FFXV recipe system production-safe:

| Fix | Where | What |
|---|---|---|
| A. Canonical `ld` encoding | dxp `Model.cpp` + `ApplyRuleStep.cpp` | Recipe-emitted `ld` now serializes with the canonical `ResourceDim + ResourceReturnType` extended-opcode pair (previously bare `0700002d`; now `0900002d 80000202 00155543`); handle-resolved cbuffer operands carry the element index (INDEX_2D). Verified against the HLSL compiler's output. |
| B. Clone pipeline resolution | Luma `core.hpp` `OnBindPipeline` | Cloned (async/sync-patched) pipelines now resolve to their cached pipeline on bind, so per-draw consumers see the **original** shader hash instead of `UINT64_MAX`. |
| C. Per-draw binding path | Luma FFXV `main.cpp` | O(1) hash-map binding lookup (was O(n) vector scan under mutex), redundant `PSSet*` skipped, DEVELOPMENT diagnostics. |

How they work together:

1. The recipe emits instructions (A's input side) → dxp serializes the DXBC
   (A's output side). Before A, the emitted `ld` lacked the extended tokens the
   HLSL compiler always emits: driver-valid but non-canonical (Flugan `-V`
   round-trip failed, and any strict consumer could misbehave).
2. The patched shader replaces the original via a **cloned pipeline** (async
   recipe provider). The game binds the original pipeline; Luma rebinds the
   clone per draw. Before B, the clone's bind event couldn't resolve the
   pipeline → per-draw consumers got `UINT64_MAX` hashes → the FFXV binding
   path (C) silently skipped → fast_noise SRV never bound → `ld` returned 0 →
   `discard` never fired → overdraw → perf collapse + GPU TDR.
3. C keeps the per-draw cost flat as recipe coverage grows (334 shaders in the
   FFXV corpus, 186 in-game).

Delivery gap: the FFXV addon links the **prebuilt vcpkg SDK (dxp 0.1.1)**.
Fixes B and C ship in the addon (deployed). Fix A ships in the dxp repo only;
it reaches the game after a dxp release tag + vcpkg port bump.

---

## 1. Goals

1. Recipe-emitted bytecode is **canonical and spec-compliant by default** — no
   silent fallbacks to non-canonical encodings.
2. Recipes (YAML) and C++ callers can **express extended opcodes explicitly**
   and have them validated, not just synthesized.
3. Wrong emits become **early, precise errors** (validate-time where possible,
   runtime with exact step/rule/operand paths otherwise) instead of silent
   mis-encodings or runtime driver surprises.
4. dxp's error model is **production-grade**: stable categories/codes,
   structured reports, warnings, deterministic output.

---

## 2. Workstream A — Extended opcode support (YAML + C++ API + docs)

### A1. Structured extended-opcode model

The WDK header (`d3d11TokenizedProgramFormat.hpp`, lines 524–558) fully
documents the token layout: type in bits 0–5, per-type payloads in bits 6–30,
chaining bit 31, max 3 simultaneous extended opcodes. dxp currently stores them
**opaquely** (`Instruction::controls.extended_op_codes` = raw `uint32` vector;
`ExtendedOpcodeType` enum exists for matching). Add structured payloads:

```cpp
struct SampleControlsPayload { int32_t u, v, w; };              // 4-bit 2's complement
struct ResourceDimPayload    { uint32_t dimension; uint32_t structure_stride; };
struct ResourceReturnTypePayload { std::array<uint32_t, 4> component_types; };
```

- Decoders: `ParseExtendedOpcodeToken(token)` → `{type, payload}` so
  validation, reporting, and the CLI can interpret tokens (today only raw).
- Keep the raw `uint32` escape hatch for unknown/future tokens.

### A2. YAML emit support (the asymmetric gap)

`InstructionPattern` (match side) already has `extended_opcodes`; `EmitPattern`
(emit side) does **not**. Add it with structured forms:

```yaml
- opcode: ld
  extended_opcodes:
    - resource_dim: texture2darray        # dimension enum (D3D10_SB_RESOURCE_DIMENSION)
    - resource_return_type: [float, float, float, float]
- opcode: sample
  extended_opcodes:
    - sample_controls: { u: 0, v: 0, w: 0 }
    - resource_dim: texture2d
    - resource_return_type: [float, float, float, float]
```

- `resource_dim` accepts the enum names (`texture2d`, `texture2darray`,
  `structured_buffer`, …) or raw values; `sample_controls` accepts
  `u/v/w` offsets (default 0); `resource_return_type` accepts
  `[unorm, snorm, sint, uint, float, mixed]`.
- A raw `extended_opcodes: [0x80000000]` form remains for exactness.
- Regenerate the JSON schema (`tools/gen_schema.cpp`, `schema_sync` ctest) and
  update `docs/sm5_recipe_schema.md`.

### A3. C++ API

- Typed builders on `Instruction`:
  `AddExtendedResourceDim(dim, stride)`,
  `AddExtendedResourceReturnType(types[4])`,
  `AddExtendedSampleControls(u,v,w)`, `AddExtendedRaw(token)`.
- Chaining-bit management (bit 31) centralized in one serializer helper so
  callers can't produce inconsistent chains; debug asserts on encode.
- `EmitPattern` gains the same `extended_opcodes` vector as
  `InstructionPattern`.

### A4. Explicit-vs-synthesized policy (replaces the current implicit path)

When emitting a resource-access opcode (`ld`, `sample`, `sample_b/c/c_lz/d/l`,
`gather4`, `gather4_c`):

1. Recipe specifies `extended_opcodes` → use **verbatim**.
2. Otherwise → auto-synthesize from the resolved resource declaration (current
   behavior — the `StampResourceAccessControls` path) but emit a **warning**
   in the recipe report so authors know the encoding was inferred.
3. Resource undeclared / unmatchable → **hard validation error** (no more
   silent bare `ld`).

### A5. Documentation

- New `docs/extended-opcodes.md`: token layout tables, per-opcode required
  extended sets (what the HLSL compiler emits for `ld` vs `sample` vs
  `ld_structured` — including the structured-buffer stride case which the
  current synthesis does **not** cover), worked examples, and the
  explicit-vs-synthesized rules.
- Note: `ld_structured` / `ld_raw` / `resinfo` also carry extended tokens;
  recipes cannot emit them today — decide whether to support emitting them
  (needs structured-buffer stride handling in the synthesis path).

---

## 3. Workstream B — Validation against wrong emits

dxp already has: per-opcode operand role/type tables
(`InstructionLayout.hpp`, 236 opcodes, derived from dxbc-spirv), a parse-time
rule `Validate` (names, capture refs, handle refs, export_as, rewrite modes),
`ValidateOperandRole` at runtime, and a small negative-test suite. Gaps:

### B1. Emit-time structural validation (every emitted instruction)

- Operand count vs layout `role_count`; role check (dest vs src) per slot.
- Component-mode rules: destination must be mask-mode (no swizzle on dest);
  source swizzle/mask legality per role; component count sanity.
- Register bounds: emitted temp index < final `dcl_temps`; resource/sampler/
  cbuffer/UAV registers must resolve to a program declaration; input/output
  registers within the declared signature; cbuffer element index < declared
  element count.
- Immediate type checks against the layout's scalar type (u32 vs f32 vs i32),
  and range checks where defined (e.g. sample-control offsets must fit 4 bits).
- Opcode validity: known SM5 opcode, not reserved, valid for the target shader
  model.
- Resource-access opcodes: the resource operand must exist **and** the declared
  dimension must be compatible with the opcode (`ld` on 2D/2DArray/3D,
  `sample` on 2D/cube/…, `ld_structured` requires structured).
- Rewrite sanity: `match_all` with overlapping windows, `replace_range`
  bounds vs match length (exists — extend to emit count arithmetic).

### B2. Rule-level validation (validate time, before any shader runs)

- Emit operand/index captures must reference captures produced by the rule's
  own match (exists for instruction captures — extend to operand + index
  captures, and cross-step capture ordering).
- Emit opcode + operand arity checked against the layout table.
- Probe steps (`rewrite_mode: none`) must not reference `insert_index`;
  conditions must reference existing steps; no condition cycles.
- Validate that the emit of a resource-access opcode satisfies A4's
  explicit-or-declarable rule — **at validate time** where possible.

### B3. Negative-test matrix + corpus gate

- One test per rule above in the existing `sm5_parse_validation_*` /
  emit-validation test style, plus positive tests for each new YAML form.
- New CI job: patch the Luma FFXV corpus (852 PS shaders) with the shipped
  recipe and assert: zero warnings, Flugan `-V` passes on every patched
  output, and repeated runs are **byte-identical** (determinism).

---

## 4. Workstream C — Error detection, propagation, production-readiness

### C1. Error model

- Introduce `dxp::Error { ErrorCategory category; uint32_t code; std::string step; std::string rule_path; std::string message; }` with categories: `Parse`, `Validate`, `Runtime`, `ShaderFormat`, `Resource`, `Internal`.
- Migrate `std::expected<T, std::string>` returns to `std::expected<T, Error>`
  (string construction kept as a convenience path during migration).
- Standardize message format: `step 'name' (kind): rule.match[i].operands[j]: <message>`.
- Extend `RecipeReport`: per-step outcomes, warnings (including synthesized
  extended opcodes), shader context (hash/size), timing.

### C2. Runtime hardening

- Parser bounds hardening: every DXBC parsing loop bounded by container size /
  declared counts (token counts, operand counts, index counts) — no allocation
  or iteration driven by hostile lengths. Add malformed-corpus tests
  (truncated tokens, bad opcodes, absurd operand counts) + a libFuzzer harness.
- Serializer invariants (debug asserts): encoded length == declared length,
  operand token counts, extended-opcode chain-bit consistency, index-dimension
  vs index-token-count agreement (the bug class that produced the bare `ld`).
- Determinism test: same input + recipe → identical bytes (already true —
  make it a regression test).

### C3. CLI & diagnostics

- `dxp sm5 validate` runs the **full semantic validation** (B1/B2) plus the
  existing schema checks, reporting per-rule findings.
- New `dxp sm5 dump <shader>`: canonical annotated disassembly (extended
  opcodes decoded, not raw) — useful for debugging patched output.
- Warnings flow through `PatchOptions::logger` at `Warning` level; per-step
  results structured for host integrations.

### C4. CI

- Extend `.github/workflows/*`: build all four presets, `ctest -C Release -V`,
  schema_sync check, corpus gate (B3), malformed-corpus tests, determinism
  test.

---

## 5. Delivery path

1. **dxp repo**: land A → B → C in that order, tests green at each step.
   A is additive (schema extension); B may surface quirks in existing recipes
   (fix the shipped FFXV recipe if so); C is the largest churn (error-model
   migration) — keep the public API additive so the linked addon stays
   compatible until the coordinated bump.
2. **Release**: cut a tag → CI produces the `ninja-x64` SDK zip (the vcpkg
   port downloads it by tag + SHA512).
3. **Luma-Framework**: bump `Source/External/vcpkg-ports/dxp/portfile.cmake`
   (VERSION + SHA512), rebuild the FFXV addon, re-verify the in-game
   signatures (bind diagnostics + no TDR).
4. **Luma core/FFXV**: no further code expected — B and C are already in.

Risks: schema changes must stay backward-compatible with version `1` recipes
(additive keys only); the error-model migration touches every step
implementation (mitigate with the compat string path); the corpus gate may
require recipe adjustments if B1 surfaces pre-existing non-canonical emits.

---

## 6. Out of scope / deferred

- SM6 (DXIL): different instruction encoding; separate workstream later.
- New recipe language features (loops, functions, multi-shader rules).
- Engine performance work beyond the per-draw binding fix already shipped.
- `ld_structured`/`ld_raw`/`resinfo` emit support (scoped decision in A5).
