# SM5 <-> SM6 Recipe Convergence Decisions and Plan

This document captures the target convergence model for SM5 and SM6 recipe schemas and execution behavior.

Scope:
- Unify naming and execution model.
- Remove SM5 portable/legacy duality.
- Define mandatory verification behavior.
- Define step ordering semantics.

Code sources referenced for current state:
- src/dxp/sm5/RecipeParse.cpp
- src/dxp/sm5/Recipe.cpp
- src/dxp/sm6/Parse.cpp
- src/dxp/sm6/Recipe.cpp

## 1) Final Policy Decisions

1. No backward compatibility:
   - Do not keep old names or legacy schema forms in SM5.
   - Remove portable/legacy distinction entirely.
2. Verification is mandatory in both SM5 and SM6 before output.
3. SM5 does not need a prune_dead_code step at this time.
4. Remove SM6 expect/assert-style steps from the DSL to keep the language lean.
5. Recipes use steps and preserve declaration order (no implicit type-based reordering).

## 2) Canonical Unified Naming

Use the following names as canonical across both systems where conceptually applicable.

| Concept | Canonical Name |
|---|---|
| Gate probe collection | prefilters |
| Rule execution mode | mode |
| Resource container | resources |
| Texture resources | resources.textures |
| UAV textures | resources.texture_uavs |
| CBuffers | resources.cbuffers |
| Samplers | resources.samplers |
| Binding register | binding.bind |
| Emit capture reference | capture |

SM5-specific declarations that cannot map 1:1 to SM6 IR can remain SM5-specific internally, but schema surface should prefer unified names.

## 3) Portable/Legacy Removal for SM5

Required simplifications:
- Remove acceptance and special handling of from_capture in favor of capture only.
- Remove legacy component selector forms in favor of components.kind + components.value only.
- Remove legacy-vs-portable parser branches and flags.
- Keep only one SM5 schema dialect for version 1.

## 4) Step Model and Ordering

Ordering model:
- Execute steps strictly in declaration order.
- Do not reorder by step kind.

Verification model:
- verify_module is mandatory but implicit.
- Remove explicit verify_module step from DSL.
- Engine performs one mandatory final module verification before success/output.

Notes:
- This preserves author intent for recipes that intentionally add resources only after a code match.
- prune_dead_code is not part of the SM5 step set for now.

## 5) Are SM6 Assert Steps Necessary?

Decision:
- No, expect/assert steps are not necessary for baseline correctness.
- Remove expect/assert steps from the canonical DSL.

Rationale:
- They only provide recipe-authored assertions over resource presence/binding.
- They are not required for correctness when mandatory final verification is enforced.
- Removing them reduces parser/runtime surface area.

## 6) Mapping Table (SM5 Current -> Target Converged)

| SM5 Current | Target |
|---|---|
| predicates | prefilters |
| application_mode | mode |
| texture_decls | resources.textures (+ add_texture step) |
| uav_decls | resources.texture_uavs (+ add_texture_uav step) |
| cbuffer_decls | resources.cbuffers (+ add_cbuffer step) |
| sampler_decls | resources.samplers (+ add_sampler step) |
| bind_point | binding.bind |
| from_capture | capture |
| explicit portable/legacy behavior | removed (single canonical behavior) |

## 7) Implementation Plan

1. Schema and parser hard cut:
   - Remove SM5 parser support for legacy forms and aliases.
   - Require canonical fields only.
2. SM5 step-kind model alignment:
   - Move SM5 to explicit step kinds for add/prefilter/apply.
   - Keep rewrite semantics, but attach them to apply_* steps.
3. Step execution order:
   - Keep execution in declaration order in both SM5 and SM6 paths.
   - Do not perform implicit type-based reordering.
4. Mandatory verification:
   - Ensure both SM5 and SM6 execute verification before returning success/output.
   - Do not expose verify_module as a DSL step kind.
5. SM5 pruning policy:
   - Do not add prune_dead_code to SM5 step set at this stage.
6. Assert policy:
   - Remove expect/assert step kinds from the DSL.
   - Keep runtime focused on rewrite/resource/verification only.
7. Documentation and recipe cleanup:
   - Update schema docs and recipe samples to canonical names and declaration-order execution.
8. Remove explicit verification step:
   - Remove verify_module from accepted step kinds in schema/parser.
   - Keep mandatory final verification in patch/execute pipeline only.
9. Remove assert steps from DSL:
   - Remove expect_texture / expect_texture_uav / expect_cbuffer from parser and schema docs.
   - Delete their runtime step constructors and references.

## 8) Acceptance Criteria

1. SM5 parser rejects non-canonical legacy field forms.
2. SM5 and SM6 both pass verification immediately before recipe success/output.
3. Steps execute in declaration order.
4. SM5 has no prune_dead_code step requirement.
5. verify_module is not an accepted DSL step kind; verification still always runs before output.
6. expect/assert step kinds are not accepted by the DSL.
