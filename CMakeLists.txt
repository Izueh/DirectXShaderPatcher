cmake_minimum_required(VERSION 3.27)
project(DirectXShaderPatcher VERSION 0.1.1 LANGUAGES CXX)

# ─── Versioning ──────────────────────────────────────────────────────────────
# Single source of truth: project(VERSION) above. DXP_VERSION_STRING follows
# SemVer — plain "<ver>" for releases, "<ver>+snapshot.<sha>" for snapshot
# builds. Build metadata has equal precedence, so a snapshot compares equal to
# the release it derives from. Therefore the base version must be the NEXT
# release under development (e.g. bump to 0.1.1 right after tagging v0.1.0) so
# snapshots order above the shipped release.
set(DXP_SNAPSHOT_SHA "" CACHE STRING "Git commit SHA for snapshot builds; renders the version as <ver>+snapshot.<sha>")
if(DXP_SNAPSHOT_SHA)
    set(DXP_VERSION_STRING "${PROJECT_VERSION}+snapshot.${DXP_SNAPSHOT_SHA}")
else()
    set(DXP_VERSION_STRING "${PROJECT_VERSION}")
endif()
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/dxp/Version.hpp"
    @ONLY
)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Dynamic CRT (/MD): the SDK's static lib is consumed by DLL-based hosts
# (games, ReShade addons) that use /MD; a /MT lib would fail LNK2038 at link.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# Per-config MSVC debug information format. The base preset used to force
# "Embedded" (Z7) for every config, which inflated the Release static lib with
# full debug info (dxpatcher.lib was ~200MB). Debug keeps embedded symbols so
# the self-contained static lib remains debuggable; Release ships without debug
# info to keep the SDK library small. RelWithDebInfo keeps program database
# info (consumed at link time by the consumer's linker).
option(DXP_ENABLE_DEBUG_ITERATORS "Build Debug artifacts with MSVC debug iterators enabled (iterator debug level 2)" ON)

set(DXC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/DirectXShaderCompiler")
set(DXC_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/dxc")

if(WIN32 AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    get_filename_component(SDK_ROOT "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE)
    
    if(NOT CMAKE_VERSION VERSION_LESS "3.19")
        set(SDK_VER ${CMAKE_WINDOWS_TARGET_PLATFORM_VERSION})
    endif()

    if(EXISTS "${SDK_ROOT}/bin/${SDK_VER}/x64/mc.exe")
        set(ENV{PATH} "${SDK_ROOT}/bin/${SDK_VER}/x64;$ENV{PATH}")
        message(STATUS "Dynamically mapped Windows SDK Tool path: ${SDK_ROOT}/bin/${SDK_VER}/x64")
    elseif(EXISTS "${SDK_ROOT}/bin/x64/mc.exe")
        set(ENV{PATH} "${SDK_ROOT}/bin/x64;$ENV{PATH}")
    endif()
endif()

find_program(SCCACHE_PROGRAM sccache)
if(SCCACHE_PROGRAM AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_PROGRAM})
    message(STATUS "Compiler cache enabled: Using sccache")
endif()

if(MSVC)
    # Debug embeds full symbols so the self-contained static lib stays
    # debuggable for consumers; Release ships without debug info to keep the
    # SDK library small. CMake 4.4 ignores the per-config _<CONFIG> debug-format
    # variables (the base applies one format to every config), so the Debug flag
    # is added explicitly per config and the base is cleared.
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "" CACHE STRING "" FORCE)
    add_compile_options($<$<CONFIG:Debug>:/Z7>)
    # No /FS is needed anywhere: the DXC/LLVM sub-build is forced to /Z7 below
    # (embedded debug info, no shared PDB), our Release targets have no debug
    # info, and Debug uses /Z7. Note: appending /FS to CMAKE_C_FLAGS_RELEASE
    # used to corrupt the C Release flags (dropping /O2 /DNDEBUG), which broke
    # the DXC build (ConvertUTF.c compiled with asserts on, mismatched llvm_assert
    # linkage).

    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_definitions(_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)

    add_compile_options(
        -Wno-error 
        -Wno-everything
    )
    set(LLVM_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    set(CLANG_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
endif()

set(ENABLE_SPIRV_CODEGEN ON CACHE BOOL "" FORCE)
set(DXC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(HLSL_BUILD_DXIL_TESTS OFF CACHE BOOL "" FORCE)

# VS Insiders / newer MSVC: allow STL template specializations for DXC compatibility
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/Wv:18>)

if(DXP_ENABLE_DEBUG_ITERATORS)
    set(HLSL_ENABLE_DEBUG_ITERATORS ON CACHE BOOL "" FORCE)
else()
    set(HLSL_ENABLE_DEBUG_ITERATORS OFF CACHE BOOL "" FORCE)
endif()

set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLVM_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(CLANG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLANG_ENABLE_ARCMT OFF CACHE BOOL "" FORCE)
set(CLANG_ENABLE_STATIC_ANALYZER OFF CACHE BOOL "" FORCE)
set(CLANG_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(LLVM_DEFAULT_TARGET_TRIPLE "dxil-ms-dx" CACHE STRING "" FORCE)
set(LLVM_ENABLE_EH ON CACHE BOOL "" FORCE)
set(LLVM_ENABLE_RTTI ON CACHE BOOL "" FORCE)
set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL "" FORCE)
set(LLVM_OPTIMIZED_TABLEGEN OFF CACHE BOOL "" FORCE)
set(LLVM_TARGETS_TO_BUILD None CACHE STRING "" FORCE)
set(HLSL_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(HLSL_BUILD_DXILCONV OFF CACHE BOOL "" FORCE)
set(SPIRV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(HLSL_DISABLE_SOURCE_GENERATION ON CACHE BOOL "" FORCE)

# DXC/LLVM (x64 only — not needed for SM5/x86)
if(DXP_ARCH STREQUAL "x86")
    message(STATUS "DXP: x86 build — skipping DXC/LLVM")
elseif(EXISTS "${DXC_SOURCE_DIR}/CMakeLists.txt")
    # DXC/LLVM appends /Zi with a shared /Fd PDB in Release; under sccache that
    # races (C1041) even with /FS. Give the DXC sub-build /Z7 (embedded debug
    # info, no shared PDB) by setting a non-empty debug-info format for its
    # directory scope only, then reset so DXP's own Release targets stay lean.
    set(_dxp_dbgfmt_prev "${CMAKE_MSVC_DEBUG_INFORMATION_FORMAT}")
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded")
    add_subdirectory("${DXC_SOURCE_DIR}" "${DXC_BINARY_DIR}" SYSTEM)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "${_dxp_dbgfmt_prev}")
    unset(_dxp_dbgfmt_prev)
else()
    message(FATAL_ERROR "DXC submodule not found. Run: git submodule update --init --recursive")
endif()

# Glaze header-only library (git submodule)
add_library(glaze INTERFACE)
target_include_directories(glaze SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/external/glaze/include")

# Value types header-only library (git submodule) — std::indirect-style deep-copying pointer.
# Appears in public headers (OperandIndexPattern::relative_operand), so it is installed with the SDK.
add_library(value_types INTERFACE)
target_include_directories(value_types SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/external")

option(DXP_BUILD_BENCHMARKS "Build benchmark targets (requires internet for FetchContent)" OFF)
option(DXP_BUILD_CLI "Build the dxp command-line tool" ON)
if(DXP_BUILD_BENCHMARKS)
    message(STATUS "Google Benchmark: enabled")
    set(BENCHMARK_ENABLE_TESTING OFF CACHE INTERNAL "")
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE INTERNAL "")
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE INTERNAL "")
    set(BENCHMARK_ENABLE_EXCEPTIONS ON CACHE INTERNAL "")

    FetchContent_Declare(
        google_benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.5
    )
    FetchContent_MakeAvailable(google_benchmark)

    target_compile_options(benchmark PRIVATE /EHsc)

    target_compile_definitions(benchmark PRIVATE NDEBUG)
    target_compile_options(benchmark PRIVATE /O2)
else()
    message(STATUS "Google Benchmark: disabled (set DXP_BUILD_BENCHMARKS=ON to enable)")
endif()

if(MSVC)
    add_compile_options(/permissive- /W4 /Zc:preprocessor /bigobj)
endif()

# SM5 sources (always included)
set(DXP_SM5_SOURCES
  "src/dxp/sm5/Model.cpp"
  "src/dxp/sm5/ShaderProgram.cpp"
  "src/dxp/sm5/step/ApplyRuleStep.cpp"
  "src/dxp/sm5/step/AddResourceStep.cpp"
  "src/dxp/sm5/step/CheckOpcodeCountStep.cpp"
  "src/dxp/sm5/step/CheckResourceCountStep.cpp"
  "src/dxp/sm5/step/CheckShaderVersionStep.cpp"
  "src/dxp/sm5/Recipe.cpp"
  "src/dxp/Condition.cpp"
)

# SM6 sources (x64 only — DXIL/LLVM dependent)
set(DXP_SM6_SOURCES
  "src/dxp/sm6/Recipe.cpp"
  "src/dxp/sm6/ShaderProgram.cpp"
  "src/dxp/sm6/step/AddResourceStep.cpp"
  "src/dxp/sm6/step/ApplyRuleStep.cpp"
  "src/dxp/sm6/step/CheckOpcodeCountStep.cpp"
  "src/dxp/sm6/step/CheckResourceCountStep.cpp"
  "src/dxp/sm6/step/CheckShaderVersionStep.cpp"
)

# Conditionally add SM6 based on architecture preset variable
if(DXP_ARCH STREQUAL "x86")
  message(STATUS "DXP: x86 build — SM5 only (no LLVM/DXIL dependency)")
else()
  list(APPEND DXP_SOURCES ${DXP_SM6_SOURCES})
  message(STATUS "DXP: x64 build — SM5 + SM6 enabled")
endif()

add_library(dxpatcher STATIC
    "external/DirectXShaderCompiler/lib/DxilHash/DxilHash.cpp"
    ${DXP_SM5_SOURCES}
    ${DXP_SOURCES}
)

target_include_directories(dxpatcher
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/WDK/includes
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# DXC/LLVM dependencies (x64 only)
if(DXP_ARCH STREQUAL "x86")
    target_include_directories(dxpatcher SYSTEM PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/external/glaze/include"
    )
    target_link_libraries(dxpatcher PRIVATE glaze value_types Ole32)
else()
    target_include_directories(dxpatcher SYSTEM PUBLIC
        "${DXC_SOURCE_DIR}/include"
        "${DXC_BINARY_DIR}/include"
    )
    target_link_libraries(dxpatcher PRIVATE 
        LLVMBitReader
        LLVMBitWriter
        LLVMCore
        LLVMDXIL
        LLVMDxilContainer
        LLVMDxilRootSignature
        LLVMDxcSupport
        LLVMHLSL
        LLVMPasses
        LLVMSupport
        dxcompiler
        Ole32
        glaze
        value_types
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/dxp.cpp" AND NOT DXP_ARCH STREQUAL "x86")
    add_executable(dxp tools/dxp.cpp)
    target_include_directories(dxp PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/glaze/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/external"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/WDK/includes"
    )
    target_link_libraries(dxp PRIVATE dxpatcher)

    if(WIN32)
        add_custom_command(TARGET dxp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:dxcompiler> $<TARGET_FILE_DIR:dxp>
            COMMENT "Copying dxcompiler binary next to dxp tool executable..."
        )
    endif()
endif()

# Schema generator — regenerates docs/sm*_recipe_schema.json from the glaze meta
# that defines the actual YAML surface. Run as `gen_schema --check` in CI to keep
# the committed schemas in sync (see the schema_sync ctest).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_schema.cpp" AND NOT DXP_ARCH STREQUAL "x86")
    add_executable(gen_schema tools/gen_schema.cpp)
    target_include_directories(gen_schema PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/glaze/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/external"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/WDK/includes"
        "${DXC_SOURCE_DIR}/include"
        "${DXC_BINARY_DIR}/include"
    )
    if(NOT DXP_ARCH STREQUAL "x86")
        target_compile_definitions(gen_schema PRIVATE NOMINMAX)
    endif()
    add_test(NAME schema_sync
        COMMAND gen_schema --check
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

    set(DXP_SDK_COMPONENT "DirectXShaderPatcherSDK")
    set(DXP_CLI_COMPONENT "DirectXShaderPatcherCLI")
    set(DXP_SDK_ROOT ".")
    set(DXP_ARCH_SUBDIR "x64")
    if(DXP_ARCH STREQUAL "x86")
        set(DXP_ARCH_SUBDIR "x86")
    endif()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DirectXShaderPatcher.props.in"
        "${CMAKE_CURRENT_BINARY_DIR}/DirectXShaderPatcher.props"
        @ONLY
    )

    # Install public headers — SM5 always, SM6 x64 only (SM6 requires DXC/LLVM)
    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/dxp/sm5/"
        DESTINATION "${DXP_SDK_ROOT}/${DXP_ARCH_SUBDIR}/include/dxp/sm5"
        COMPONENT ${DXP_SDK_COMPONENT}
    )
    if(DXP_ARCH STREQUAL "x86")
        # x86: SM5 only, no DXC/LLVM
    else()
        install(
            DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/dxp/sm6/"
            DESTINATION "${DXP_SDK_ROOT}/${DXP_ARCH_SUBDIR}/include/dxp/sm6"
            COMPONENT ${DXP_SDK_COMPONENT}
        )
    endif()
    # Common headers (always installed). Note: must declare the SDK component,
    # otherwise they land in the "Unspecified" component and never reach the
    # package when installing with --component DirectXShaderPatcherSDK.
    install(
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/include/dxp/Condition.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/dxp/ExportTypes.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/dxp/Logging.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/dxp/PatchOptions.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/dxp/RecipeReport.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/dxp/StepResults.hpp"
            "${CMAKE_CURRENT_BINARY_DIR}/include/dxp/Version.hpp"
        DESTINATION "${DXP_SDK_ROOT}/${DXP_ARCH_SUBDIR}/include/dxp/"
        COMPONENT ${DXP_SDK_COMPONENT}
    )

    # value_types/indirect.h is required by the public API (std::indirect-style member);
    # installed at the include root so `#include "indirect.h"` resolves for consumers.
    install(
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/external/value_types/indirect.h"
        DESTINATION "${DXP_SDK_ROOT}/${DXP_ARCH_SUBDIR}/include/value_types/"
        COMPONENT ${DXP_SDK_COMPONENT}
    )

    # Libraries (Debug/Release via generator expression for multi-config)
    install(
        TARGETS dxpatcher
        EXPORT DirectXShaderPatcherSDKTargets
        ARCHIVE DESTINATION "${DXP_ARCH_SUBDIR}/$<CONFIG>/lib"
        COMPONENT ${DXP_SDK_COMPONENT}
    )

    # No DXC/LLVM headers — public API is self-contained

    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/DirectXShaderPatcher.props"
        DESTINATION "${DXP_SDK_ROOT}"
        COMPONENT ${DXP_SDK_COMPONENT}
    )

    # dxcompiler.dll (x64 only, Debug/Release via generator expression)
    if(DXP_ARCH STREQUAL "x86")
        # x86: no dxcompiler
    else()
        install(
            FILES "${DXC_BINARY_DIR}/$<CONFIG>/bin/dxcompiler.dll"
            DESTINATION "${DXP_ARCH_SUBDIR}/$<CONFIG>/bin"
            COMPONENT ${DXP_SDK_COMPONENT}
        )
        # Third-party notice for the shipped dxcompiler.dll (DXC submodule)
        install(
            FILES "${CMAKE_CURRENT_SOURCE_DIR}/external/DirectXShaderCompiler/ThirdPartyNotices.txt"
            DESTINATION "${DXP_ARCH_SUBDIR}/$<CONFIG>/bin"
            COMPONENT ${DXP_SDK_COMPONENT}
        )
    endif()

    if(TARGET dxp)
        install(
            TARGETS dxp
            RUNTIME DESTINATION "${DXP_ARCH_SUBDIR}/$<CONFIG>/bin"
            COMPONENT ${DXP_CLI_COMPONENT}
        )

        install(
            FILES "${DXC_BINARY_DIR}/$<CONFIG>/bin/dxcompiler.dll"
            DESTINATION "${DXP_ARCH_SUBDIR}/$<CONFIG>/bin"
            COMPONENT ${DXP_CLI_COMPONENT}
        )
        install(
            FILES "${CMAKE_CURRENT_SOURCE_DIR}/external/DirectXShaderCompiler/ThirdPartyNotices.txt"
            DESTINATION "${DXP_ARCH_SUBDIR}/$<CONFIG>/bin"
            COMPONENT ${DXP_CLI_COMPONENT}
        )
    endif()

if(WIN32)
    if(NOT DXP_ARCH STREQUAL "x86")
        add_custom_target(copy_dxcompiler_runtime ALL
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:dxcompiler> $<TARGET_FILE_DIR:dxpatcher>
            COMMENT "Copying dxcompiler runtime dependency once to central build directory..."
        )
    endif()
endif()

# Enable testing
enable_testing()
add_subdirectory(tests)
