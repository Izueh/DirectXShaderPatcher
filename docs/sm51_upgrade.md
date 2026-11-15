# SM5.1 output and register spaces (SM5 recipes)

D3D12 can bind a resource by register *and space*, but a DXBC SM5.0 shader can't name a space: everything is in space 0.
A host that owns its own space (Luma keeps its constant buffers in space 100, so they never clash with a game's
registers) needs the patched shader to declare its resources there. SM5.1 DXBC can, and D3D12 accepts SM5.0 and SM5.1
stages in one pipeline, so only the patched stage has to change.

DXP does this for SM5 recipes: give a declaration a `space` and the output is written as SM5.1 by itself.

```yaml
version: 1
env:
  LUMA_SPACE: 100            # the host can set it per run: PatchOptions::SetEnv("LUMA_SPACE", 100u)
steps:
  - kind: add_resource
    name: add_luma
    temps: [scratch]
    cbuffers:
      - handle: luma
        elements: 1
        register_index: 13   # b13 ...
        space: LUMA_SPACE    # ... in space 100 (a number works too)
```

`new_bindings["luma"]` reports `register_index: 13, space: 100`. The game's own resources keep their registers, in space 0.

## The `upgrade_to_sm51` option

A top-level recipe key (`Recipe::SetSm51Upgrade` from C++):

| value | output |
|---|---|
| `auto` (default) | SM5.1 only when a declaration has a non-zero `space`. Every other recipe keeps writing SM5.0, byte for byte as before |
| `always` | SM5.1 whenever the recipe changes the shader, even with everything in space 0 |
| `never` | never; a declaration with a non-zero `space` is an error |

A recipe that changes nothing passes the input through as it is, whatever the option says. Only SM5 input can be upgraded.

## What changes in the shader

* The version token is 5.1. Each resource, sampler, UAV and constant buffer declaration takes the 5.1 form: a range id,
  lower and upper bound (one register per range), the space. Range ids are dense per register family, in declaration order.
* Every operand that names one of them gets its range id as a first index (`t7` becomes `T1[7]`, `cb1[r1.z + 0]` becomes
  `CB0[1][r1.z + 0]`).
* The resource-dimension and return-type extended opcode tokens of `sample`, `ld` and friends are dropped, as fxc does
  for 5.1 (the declaration carries them). Sample offsets stay.
* The RDEF chunk (reflection) is dropped. D3D12 doesn't read it, and the 5.0 one wouldn't describe the shader.
* The container hash is signed again as always.

The output of the upgrade is what fxc writes for the same HLSL compiled as `ps_5_1`: the tests compare them.

## Recipes on 5.1 input

Recipes chain (a host runs each enabled recipe on the previous one's output), so a recipe also reads SM5.1. While it runs, the
shader looks like SM5.0 to its rules: a resource in another space gets a free register of its own in the program, which
rules and `handle:` operands use, and the real space and register are kept aside and restored when the shader is written.
So `match`, `capture` and `emit` never see range ids, and a recipe that works on SM5.0 works on the SM5.1 output of
another.

`add_resource` allocates per space: `register_index` is taken as it is (an error when that register is already used in the
space, naming it), a missing one takes the lowest free register of the space (the highest with `reverse_bind`).

## Limits

* A range of registers (`t0[0:3]`) and an indexed resource (`t[r0.x]`) are SM5.1-only and aren't read: the input is refused
  with a message naming the register. DXBC compilers write them only for bindless HLSL.
* Class linkage (interfaces, function tables) has no SM5.1 form and is refused.
* SM6 recipes don't have this: DXIL names a space natively.
