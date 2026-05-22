# To Do Before Release
1. Define an explicit compatibility policy before release: version the public library API and recipe schema separately, aim for source compatibility across minor releases, and do not promise ABI stability yet unless we intentionally design for it.
2. If we add SM5 support, keep it in the same repository but behind a separate module/library boundary from the DXIL implementation, with only shared patch/recipe abstractions reused where the model genuinely overlaps.
3. Reorganize code into a more standard layour for libraries, possibly devide core library into multiple files with a common namespace (if it makes sense). Should also refactor test suite to reuse as much shared code as possible.
4. Rename things to be more idiomatic to both C++ and DirectXShaderCompiler and also properly descriptive.
5. Add a style guide for the code and properly format the code (should ideally match what is used in DirectXShaderCompiler)
6. Add example files for basic usage, should include the following:
    - A template Recipe file.
    - Example of using the Recipe builder to make patches.
    - Example of using the various helpers without the Recipe API.
7. Add README.md describing capabilities and other important information. A separate readme describing the API and recipe file format in detail.
8. Figure out licensing (Do we have to use same license as DirectXShaderCompiler library?)
9. Investigate the intermittent crash in `declarative_blue_noise_emit_recipe_0x56C468C3`. As of 2026-05-18, `ctest -C Debug --output-on-failure` in `build/msvc-debug` passes ~50% of the time (fails with segfault on the other ~50%). The crash is **NOT** ctest-specific — it also occurs when running the test executable directly, but at a lower frequency (~20% failure rate based on 10 runs). This suggests a race condition or use-after-free in the rewrite/pruning logic that manifests under timing variations.

# Ordered SM5 Implementation TODO
1. Create the SM5 module skeleton under `include/dxp/sm5` and `src/dxp/sm5` without introducing shared abstractions with the SM6 path yet.
2. Add a repeatable verification helper around `F:\software\decompiler\bin\cmd_Decompiler.exe -d` and capture baseline disassembly for `test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso`.
3. Implement DXBC container parsing with chunk table support and explicit handling for chunk sizes, container-level size bookkeeping, and DXBC hash recomputation during rebuild.
4. Implement shader chunk discovery for `SHDR` and `SHEX` and fail clearly when the target chunk is missing or unsupported.
5. Implement a read-only SM5 token walker that decodes version, length, opcode tokens, operand tokens, extended tokens, and custom-data blocks conservatively.
6. Define the lossless in-memory token model for programs, instructions, operands, and source spans so untouched shaders can be re-encoded deterministically.
7. Implement serialization for untouched SM5 shader chunks and require byte-identical round-trip on the initial fixture set.
8. Verify untouched round-trips primarily by comparing decompiler output before and after serialization, not only by comparing bytes.
9. Add declaration indexing for SRVs, UAVs, samplers, constant buffers, temps, indexable temps, thread group declarations, and global flags.
10. Add focused parser and container tests for the objective shader and at least one smaller control fixture.
11. Implement instruction matching primitives for SM5 opcodes and opcode controls such as saturate, test boolean, return type, and other relevant encoded fields.
12. Implement operand matching primitives for register type, indices, mask or swizzle, modifiers, immediates, and relative addressing.
13. Implement capture storage and match-against-capture support so declarative matching can anchor rewrites safely.
14. Build the first IGN-specific matcher coverage for `Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso` and prove it isolates the intended IGN region without matching unrelated code.
15. Implement rewrite planning with stable instruction identities rather than byte offsets.
16. Implement replace-one-instruction, replace-range, insert-before, and insert-after mutation support on the token IR.
17. Implement SM5 instruction emission helpers sufficient to express the FAST noise sample replacement path needed for the objective shader.
18. Rebuild the container after mutation, including updated chunk sizes, container-level size fields, and final DXBC hash recomputation.
19. Verify rewrites primarily by disassembling the original and patched shaders and diffing the instruction-level changes.
20. Implement the SM5 recipe model and YAML parsing while keeping the high-level workflow similar to SM6 but the opcode and operand vocabulary SM5-native.
21. Implement recipe execution with prefilters, ordered steps, rewrite application modes, and patch entry points for SM5 containers.
22. Create the first end-to-end SM5 recipe that targets `Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso` and replaces IGN with a FAST noise texture sample path.
23. Add resource or declaration mutation support only after the basic IGN rewrite path is stable, then verify declaration changes through disassembly and focused fixture tests.
24. Defer optional CFG, def-use, or reflection consistency analyses until a concrete SM5 recipe proves they are necessary.