# Prefilter Future Plan

This note captures possible future directions for expanding recipe prefilters and related matcher domains. It is intentionally forward-looking only. The current baseline remains:

- prefilters are cheap
- prefilters are side-effect free
- prefilters can stop the recipe early
- prefilters should stay structurally simpler than rewrite rules unless a concrete use case requires more

## Current Direction

The current design is sufficient for now if prefilters are treated as a fast triage mechanism for live patching. They should answer "is this shader even worth attempting to patch?" before the more expensive rewrite matching and mutation path runs.

This means prefilters should prefer:

- shallow IR signatures
- a small number of exact constants
- lightweight resource or module checks when those become necessary

They should not become a second full rewrite DSL unless real use cases justify that complexity.

## Match Domain Guideline

If future scenarios need to match things beyond IR call structure, add explicit match domains instead of stretching the IR matcher until it becomes ambiguous.

The working rule should be:

- operand pattern kinds describe IR operand matching only
- prefilter kinds describe what graph, table, or object model is being queried

This avoids overloading the existing IR matcher with unrelated concepts such as metadata traversal or shader-level properties.

## Candidate Future Prefilter Domains

These are the first domains worth considering if future use cases require more than the current IR call-pattern prefiltering.

### 1. IR Call Pattern

Use for cheap structural signatures in LLVM IR / DXIL call form.

Good fit for:

- opcode and operand shape checks
- exact integer constants
- shallow nested DXIL op structure
- resource-handle shape checks when the handle appears directly in the matched call tree

This is the current prefilter baseline and should remain the default.

### 2. Resource Pattern

Use for questions answered by `hlsl::DxilModule` resource tables rather than instruction walking.

Good fit for:

- SRV / UAV / CBuffer / Sampler presence
- resource kind checks
- bind / space checks
- exact resource name checks
- resource name regex checks

This is a strong candidate for future prefilter support because it is still cheap and often useful for triage.

### 3. Module Property Pattern

Use for broad shader-level properties rather than specific instruction shapes.

Good fit for:

- shader stage
- shader model
- entry-point level properties
- coarse counts or feature presence checks

This is also a reasonable future prefilter domain because it is cheap and easy to reason about.

### 4. Metadata Pattern

Use only when a real use case needs to inspect LLVM or DXIL metadata directly.

Possible future subdomains:

- named module metadata
- instruction-attached metadata
- DXIL-specific metadata structures

This should not be added until a concrete case proves it is needed. Metadata matching is a different model from IR operand matching and should be represented explicitly if it is ever introduced.

## Recommended Modeling Approach

If additional domains are introduced later, model them as an explicit tagged prefilter representation instead of trying to force everything into `DxilCallPattern` or `DxilOperandPattern`.

Conceptually:

```cpp
enum class DxilPrefilterKind {
  IrCall,
  Resource,
  ModuleProperty,
  NamedMetadata,
  InstructionMetadata,
};
```

Then give each domain its own compact pattern structure and dispatch in the prefilter executor based on `DxilPrefilterKind`.

This keeps each matcher coherent and makes schema validation much easier.

## Schema Direction If Expanded Later

If the recipe schema grows beyond IR-only prefilters, the preferred direction is to let users choose the match domain explicitly.

Example shape:

```yaml
prefilters:
  - id: ign_signature
    kind: ir_call
    opcode: Frc
    operands: []

  - id: has_blue_noise_srv
    kind: resource
    resource_class: SRV
    resource_name_like: "BlueNoise.*"

  - id: is_compute_shader
    kind: module_property
    property: shader_stage
    equals: compute
```

And the step stays simple:

```yaml
steps:
  - kind: prefilter
    patterns:
      - ign_signature
      - has_blue_noise_srv
```

The step should remain domain-agnostic. It should only orchestrate early exit, not encode domain-specific logic itself.

## Relationship To Rewrite Matching

Future prefilter domains should not automatically become available inside rewrite rules.

The decision should be made per domain:

- add to `prefilter` when the signal is cheap and useful for rejecting non-target shaders
- add to `rewrite` only when it helps safely identify mutation targets or enforce correctness guards

Examples:

- resource matching may be useful in both prefilter and rewrite
- module-property matching may be useful in both prefilter and rewrite
- metadata checks may remain prefilter-only if they are too coarse or indirect to safely anchor a rewrite
- shallow IR signatures may be acceptable for prefilter but still too weak for actual mutation targeting

The implementation guideline is:

- implement shared matching helpers once
- expose them to prefilter first
- expose them to rewrite only when a concrete mutation use case requires it

## Bit Pattern Guidance

If future users ask for "bit pattern" matching while working on LLVM IR / DXIL objects, prefer representing that as semantic constant matching rather than raw serialized byte search.

Examples:

- match `llvm::ConstantInt` values directly
- for float identity, compare `llvm::ConstantFP` values by their exact bit representation
- for aggregates, inspect constant vector or struct elements rather than serialized container bytes

Raw DXIL byte-pattern scanning only makes sense against serialized bitcode or container data, not the live IR / `DxilModule` object graph.

## Trigger For Expansion

Do not expand this system preemptively. Revisit this plan only when one of these happens:

- a live patch path needs a cheap resource or shader-property gate that IR shape matching cannot express cleanly
- a rewrite needs stronger non-IR constraints for safety
- a concrete metadata-based use case appears and cannot be represented through existing `DxilModule` APIs or IR structure

Until then, keep prefilters narrow and biased toward fast structural IR probes.