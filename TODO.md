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