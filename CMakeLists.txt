cmake_minimum_required(VERSION 3.15)
project(DirectXShaderPatcher LANGUAGES C CXX)

include(CTest)

# Use strict C++17 mode
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DXP_ENABLE_SDK_INSTALL "Enable install rules for the GitHub release SDK layout" OFF)
set(DXP_SDK_TOOLSET_SUBDIR "" CACHE STRING "Optional SDK library subdirectory suffix such as clang or msvc")
option(DXP_ENABLE_DEBUG_ITERATORS "Build Debug artifacts with MSVC debug iterators enabled (iterator debug level 2)" ON)

# DXC submodule paths
set(DXC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/DirectXShaderCompiler")
set(DXC_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/dxc")

if(WIN32 AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    # Find the Windows SDK root
    get_filename_component(SDK_ROOT "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]" ABSOLUTE)
    
    # Read the active SDK version
    if(NOT CMAKE_VERSION VERSION_LESS "3.19")
        set(SDK_VER ${CMAKE_WINDOWS_TARGET_PLATFORM_VERSION})
    endif()

    # Add mc.exe to PATH when available
    if(EXISTS "${SDK_ROOT}/bin/${SDK_VER}/x64/mc.exe")
        set(ENV{PATH} "${SDK_ROOT}/bin/${SDK_VER}/x64;$ENV{PATH}")
        message(STATUS "Dynamically mapped Windows SDK Tool path: ${SDK_ROOT}/bin/${SDK_VER}/x64")
    elseif(EXISTS "${SDK_ROOT}/bin/x64/mc.exe")
        set(ENV{PATH} "${SDK_ROOT}/bin/x64;$ENV{PATH}")
    endif()
endif()

# -----------------------------------------------------------------------------
# 1. Compiler Cache And Debug Info
# -----------------------------------------------------------------------------
find_program(SCCACHE_PROGRAM sccache)
if(SCCACHE_PROGRAM AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE_PROGRAM})
    message(STATUS "Compiler cache enabled: Using sccache")
endif()

# Use embedded debug info to avoid sccache/Ninja PDB contention
if(MSVC)
    add_compile_options(/Z7)
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
endif()

# -----------------------------------------------------------------------------
# 2. Clang-Cl Compatibility
# -----------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Allow this Clang version with the MSVC STL
    add_compile_definitions(_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)

    # Suppress third-party warning escalation
    add_compile_options(
        -Wno-error 
        -Wno-everything
    )
    
    # Disable DXC-injected Werror settings
    set(LLVM_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    set(CLANG_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
endif()

# -----------------------------------------------------------------------------
# 3. DXC Configuration
# -----------------------------------------------------------------------------
set(ENABLE_SPIRV_CODEGEN ON CACHE BOOL "" FORCE)
set(DXC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(HLSL_BUILD_DXIL_TESTS OFF CACHE BOOL "" FORCE)

# Keep Debug ABI compatible with normal MSVC Debug consumers.
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

if(EXISTS "${DXC_SOURCE_DIR}/CMakeLists.txt")
    # Treat DXC includes as SYSTEM throughout the subdirectory tree
    add_subdirectory("${DXC_SOURCE_DIR}" "${DXC_BINARY_DIR}" SYSTEM)
else()
    message(FATAL_ERROR "DXC submodule not found. Run: git submodule update --init --recursive")
endif()

# -----------------------------------------------------------------------------
# 4. Library Target
# -----------------------------------------------------------------------------
add_library(dxp_lib STATIC
    "external/DirectXShaderCompiler/lib/DxilHash/DxilHash.cpp"
  "src/dxp/sm6/Container.cpp"
  "src/dxp/sm6/Parse.cpp"
  "src/dxp/sm6/Patch.cpp"
  "src/dxp/sm6/Recipe.cpp"
  "src/dxp/sm6/Resources.cpp"
  "src/dxp/sm6/Transforms.cpp"
  "src/dxp/sm5/Container.cpp"
  "src/dxp/sm5/Model.cpp"
  "src/dxp/sm5/Parse.cpp"
  "src/dxp/sm5/RecipeParse.cpp"
  "src/dxp/sm5/Resources.cpp"
  "src/dxp/sm5/Serialize.cpp"
  "src/dxp/sm5/Transforms.cpp"
  "src/dxp/sm5/Recipe.cpp"
  "src/dxp/sm5/Patch.cpp"
)

# Mark DXC and WDK includes as SYSTEM for consumers
target_include_directories(dxp_lib SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${DXC_SOURCE_DIR}/include"
    "${DXC_SOURCE_DIR}/lib/Reflect"
    "${DXC_BINARY_DIR}/include"
    "${DXC_SOURCE_DIR}/external/llvm/include"
    "${DXC_BINARY_DIR}/external/llvm/include"
    "${DXC_SOURCE_DIR}/external/llvm/tools/clang/include"
    "${DXC_BINARY_DIR}/external/llvm/tools/clang/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/WDK/includes"
)

target_link_libraries(dxp_lib PRIVATE 
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
)

# -----------------------------------------------------------------------------
# 5. CLI Target
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/dxp.cpp")
    add_executable(dxp tools/dxp.cpp)
    target_link_libraries(dxp PRIVATE dxp_lib)

    if(WIN32)
        add_custom_command(TARGET dxp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:dxcompiler> $<TARGET_FILE_DIR:dxp>
            COMMENT "Copying dxcompiler binary next to dxp tool executable..."
        )
    endif()
endif()

if(DXP_ENABLE_SDK_INSTALL)
    set(DXP_SDK_COMPONENT "DirectXShaderPatcherSDK")
    set(DXP_CLI_COMPONENT "DirectXShaderPatcherCLI")
    set(DXP_SDK_ROOT "DirectXShaderPatcher_SDK")
    set(DXP_SDK_LIBRARY_DIR "${DXP_SDK_ROOT}/lib/$<CONFIG>")
    if(DXP_SDK_TOOLSET_SUBDIR)
        string(APPEND DXP_SDK_LIBRARY_DIR "/${DXP_SDK_TOOLSET_SUBDIR}")
    endif()
    set(DXP_SDK_RUNTIME_DIR "${DXP_SDK_ROOT}/bin")

    set(DXP_SDK_LIBRARY_TARGETS
        dxp_lib
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
    )

    set(DXP_SDK_MSBUILD_DEPENDENCIES
        "dxp_lib.lib;LLVMBitReader.lib;LLVMBitWriter.lib;LLVMCore.lib;LLVMDXIL.lib;LLVMDxilContainer.lib;LLVMDxilRootSignature.lib;LLVMDxcSupport.lib;LLVMHLSL.lib;LLVMPasses.lib;LLVMSupport.lib;dxcompiler.lib;Ole32.lib"
    )

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DirectXShaderPatcher.props.in"
        "${CMAKE_CURRENT_BINARY_DIR}/DirectXShaderPatcher.props"
        @ONLY
    )

    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION "${DXP_SDK_ROOT}/include"
        COMPONENT ${DXP_SDK_COMPONENT}
    )

    # Public SM6 headers expose DXC/LLVM types, so the SDK must also ship the
    # referenced third-party header trees.
    if(EXISTS "${DXC_SOURCE_DIR}/include/dxc")
        install(
            DIRECTORY "${DXC_SOURCE_DIR}/include/dxc"
            DESTINATION "${DXP_SDK_ROOT}/include"
            COMPONENT ${DXP_SDK_COMPONENT}
        )
    endif()

    if(EXISTS "${DXC_BINARY_DIR}/include/dxc")
        install(
            DIRECTORY "${DXC_BINARY_DIR}/include/dxc"
            DESTINATION "${DXP_SDK_ROOT}/include"
            COMPONENT ${DXP_SDK_COMPONENT}
        )
    endif()

    if(EXISTS "${DXC_SOURCE_DIR}/external/llvm/include/llvm")
        install(
            DIRECTORY "${DXC_SOURCE_DIR}/external/llvm/include/llvm"
            DESTINATION "${DXP_SDK_ROOT}/include"
            COMPONENT ${DXP_SDK_COMPONENT}
        )
    endif()

    if(EXISTS "${DXC_BINARY_DIR}/external/llvm/include/llvm")
        install(
            DIRECTORY "${DXC_BINARY_DIR}/external/llvm/include/llvm"
            DESTINATION "${DXP_SDK_ROOT}/include"
            COMPONENT ${DXP_SDK_COMPONENT}
        )
    endif()

    if(EXISTS "${DXC_BINARY_DIR}/include/llvm")
        install(
            DIRECTORY "${DXC_BINARY_DIR}/include/llvm"
            DESTINATION "${DXP_SDK_ROOT}/include"
            COMPONENT ${DXP_SDK_COMPONENT}
        )
    endif()

    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/DirectXShaderPatcher.props"
        DESTINATION "${DXP_SDK_ROOT}"
        COMPONENT ${DXP_SDK_COMPONENT}
    )

    install(
        TARGETS ${DXP_SDK_LIBRARY_TARGETS}
        ARCHIVE DESTINATION "${DXP_SDK_LIBRARY_DIR}" COMPONENT ${DXP_SDK_COMPONENT}
        LIBRARY DESTINATION "${DXP_SDK_LIBRARY_DIR}" COMPONENT ${DXP_SDK_COMPONENT}
    )

    install(
        TARGETS dxcompiler
        RUNTIME DESTINATION "${DXP_SDK_RUNTIME_DIR}" COMPONENT ${DXP_SDK_COMPONENT}
        LIBRARY DESTINATION "${DXP_SDK_RUNTIME_DIR}" COMPONENT ${DXP_SDK_COMPONENT}
        ARCHIVE DESTINATION "${DXP_SDK_LIBRARY_DIR}" COMPONENT ${DXP_SDK_COMPONENT}
    )

    if(TARGET dxp)
        install(
            TARGETS dxp
            RUNTIME DESTINATION "."
            COMPONENT ${DXP_CLI_COMPONENT}
        )

        install(
            TARGETS dxcompiler
            RUNTIME DESTINATION "."
            LIBRARY DESTINATION "."
            COMPONENT ${DXP_CLI_COMPONENT}
        )
    endif()
endif()

# -----------------------------------------------------------------------------
# 6. Test Harness
# -----------------------------------------------------------------------------
enable_testing()

# Copy dxcompiler.dll once into the shared build output
if(WIN32)
    add_custom_target(copy_dxcompiler_runtime ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:dxcompiler> $<TARGET_FILE_DIR:dxp_lib>
        COMMENT "Copying dxcompiler runtime dependency once to central build directory..."
    )
endif()

# Shared test support library
add_library(dxp_test_support STATIC
  "test/TestSupport.cpp"
  "test/ComputeNoiseSupport.cpp"
)
target_include_directories(dxp_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(dxp_test_support PUBLIC dxp_lib)

# Register either standalone tests or CLI-driven tests
function(dxp_add_test EXECUTABLE_NAME SOURCE_FILE)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}")
        message(WARNING "Test source file skipped (not found): ${SOURCE_FILE}")
        return()
    endif()

    # Remove the common test target prefix
    string(REPLACE "dxil_patch_" "" CLEAN_EXE_NAME "${EXECUTABLE_NAME}")

    # Build a descriptive test name
    set(TEST_RUN_NAME "${CLEAN_EXE_NAME}")
    list(LENGTH ARGN ARG_COUNT)
    if(ARG_COUNT GREATER 0)
        list(GET ARGN 0 SHADER_PATH)
        get_filename_component(SHADER_BASE ${SHADER_PATH} NAME_WE)
        set(TEST_RUN_NAME "${TEST_RUN_NAME}_${SHADER_BASE}")

        if(ARG_COUNT GREATER 1)
            list(GET ARGN 1 RECIPE_PATH)
            get_filename_component(RECIPE_BASE ${RECIPE_PATH} NAME_WE)
            set(TEST_RUN_NAME "${TEST_RUN_NAME}_with_${RECIPE_BASE}")
        endif()
    endif()

    # Reuse the CLI target for tools/dxp.cpp; otherwise build a test binary
    if(SOURCE_FILE STREQUAL "tools/dxp.cpp")
        set(RUN_BINARY "dxp")
    else()
        set(RUN_BINARY "${CLEAN_EXE_NAME}")
        if(NOT TARGET ${RUN_BINARY})
            add_executable(${RUN_BINARY} ${SOURCE_FILE})
            target_link_libraries(${RUN_BINARY} PRIVATE dxp_test_support)
            
            if(WIN32)
                add_dependencies(${RUN_BINARY} copy_dxcompiler_runtime)
            endif()
        endif()
    endif()

    # Add the CLI subcommand when the test runs through dxp
    set(RUN_COMMAND_ARGS ${ARGN})
    if(RUN_BINARY STREQUAL "dxp")
        if(TEST_RUN_NAME MATCHES "validate_sm5_recipe")
            list(INSERT RUN_COMMAND_ARGS 0 "validate")
            list(INSERT RUN_COMMAND_ARGS 0 "sm5")
        elseif(TEST_RUN_NAME MATCHES "validate_recipe")
            list(INSERT RUN_COMMAND_ARGS 0 "validate")
            list(INSERT RUN_COMMAND_ARGS 0 "sm6")
        elseif(TEST_RUN_NAME MATCHES "patch_sm5_recipe")
            list(INSERT RUN_COMMAND_ARGS 0 "patch")
            list(INSERT RUN_COMMAND_ARGS 0 "sm5")
        elseif(TEST_RUN_NAME MATCHES "patch_recipe")
            list(INSERT RUN_COMMAND_ARGS 0 "patch")
            list(INSERT RUN_COMMAND_ARGS 0 "sm6")
        endif()
    endif()

    # Avoid duplicate CTest names
    get_property(EXISTING_TESTS DIRECTORY PROPERTY TESTS)
    if("${TEST_RUN_NAME}" IN_LIST EXISTING_TESTS)
        set(COUNTER 2)
        set(SAFE_RUN_NAME "${TEST_RUN_NAME}_run${COUNTER}")
        while("${SAFE_RUN_NAME}" IN_LIST EXISTING_TESTS)
            math(EXPR COUNTER "${COUNTER} + 1")
            set(SAFE_RUN_NAME "${TEST_RUN_NAME}_run${COUNTER}")
        endwhile()
        set(TEST_RUN_NAME "${SAFE_RUN_NAME}")
    endif()

    # Register the test with CTest
    add_test(NAME ${TEST_RUN_NAME} COMMAND ${RUN_BINARY} ${RUN_COMMAND_ARGS})
endfunction()



# -----------------------------------------------------------------------------
# 7. Test Registration
# -----------------------------------------------------------------------------
    add_compile_definitions(NOMINMAX)
# SM6 declarative tests
dxp_add_test(dxil_patch_texture_add_test                            "test/texture_add_0x965B1360.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x965B1360.ps_6_6.cso")
dxp_add_test(dxil_patch_texture2darray_add_test                     "test/texture2darray_add_0x965B1360.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x965B1360.ps_6_6.cso")
dxp_add_test(dxil_patch_texture_uav_add_test                        "test/texture_uav_add_0x965B1360.cpp"     "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x965B1360.ps_6_6.cso")
dxp_add_test(dxil_patch_cbuffer_add_test                            "test/cbuffer_add_0x965B1360.cpp"         "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x965B1360.ps_6_6.cso")
dxp_add_test(dxil_patch_rendertarget_dsl_test                       "test/rendertarget_dsl_0x965B1360.cpp"    "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x965B1360.ps_6_6.cso")
dxp_add_test(dxil_patch_gatherforeground_isfast_test                "test/gatherforeground_isfast_0xAD818E14.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/GatherForeground_0xAD818E14.cs_6_6.cso")
dxp_add_test(dxil_patch_gatherforeground_isfast_test                "test/gatherforeground_isfast_0xAD818E14.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(dxil_patch_gatherforeground_isfast_test                "test/gatherforeground_isfast_0xAD818E14.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0xC4720FE8.cs_6_6.cso")
dxp_add_test(dxil_patch_gatherforeground_isfast_recipe_test         "test/gatherforeground_isfast_recipe.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/GatherForeground_0xAD818E14.cs_6_6.cso")
dxp_add_test(dxil_patch_gatherforeground_isfast_recipe_test         "test/gatherforeground_isfast_recipe.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(dxil_patch_gatherforeground_isfast_recipe_test         "test/gatherforeground_isfast_recipe.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0xC4720FE8.cs_6_6.cso")
dxp_add_test(dxil_patch_declarative_rewrite_recipe_test             "test/declarative_rewrite_recipe_0x56C468C3.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/frc_passthrough.recipe.yml")
dxp_add_test(dxil_patch_declarative_emit_resource_recipe_test       "test/declarative_emit_resource_recipe_0x56C468C3.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/threadid_to_cbuffer_emit.recipe.yml")
dxp_add_test(dxil_patch_declarative_ign_nested_recipe_test          "test/declarative_ign_nested_recipe_0x56C468C3.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/ign_nested_match.recipe.yml")
dxp_add_test(dxil_patch_declarative_ign_emit_recipe_test            "test/declarative_ign_emit_recipe_0x56C468C3.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/ign_subset_emit.recipe.yml")
dxp_add_test(dxil_patch_declarative_ign_emit_recipe_test            "test/declarative_ign_emit_recipe_0x56C468C3.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/ign_subset_emit_probe.recipe.yml")
dxp_add_test(dxil_patch_declarative_ign_emit_inmemory_recipe_test   "test/declarative_ign_emit_inmemory_recipe_0x56C468C3.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/ign_subset_emit.recipe.yml")
dxp_add_test(dxil_patch_declarative_blue_noise_emit_recipe_test     "test/declarative_blue_noise_emit_recipe_0x56C468C3.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/blue_noise_emit.recipe.yml")
dxp_add_test(dxil_patch_declarative_blue_noise_emit_recipe_test     "test/declarative_blue_noise_emit_recipe_0x56C468C3.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/blue_noise_emit_snapshot.recipe.yml")
dxp_add_test(dxil_patch_declarative_resource_name_match_recipe_test "test/declarative_resource_name_match_recipe_0x56C468C3.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/resource_name_match.recipe.yml")
dxp_add_test(dxil_patch_declarative_probe_recipe_test               "test/declarative_probe_recipe_0x56C468C3.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x56C468C3.cs_6_6.cso")
# SM5 tests
dxp_add_test(dxil_patch_sm5_container_roundtrip_test                "test/sm5_container_roundtrip.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_replace_frc_with_mov_test               "test/sm5_replace_frc_with_mov.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso"  "${CMAKE_CURRENT_BINARY_DIR}/test-output/0x7AFF256C.ps_5_0.frc_to_mov.patched.cso")
dxp_add_test(dxil_patch_sm5_recipe_replace_frc_with_mov_test        "test/sm5_recipe_replace_frc_with_mov.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/sm5_frc_to_mov.recipe.yml")
dxp_add_test(dxil_patch_sm5_recipe_emit_arbitrary_test              "test/sm5_recipe_emit_arbitrary.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_builder_capture_fields_test      "test/sm5_recipe_builder_capture_fields.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_capture_fields_literal_override_test  "test/sm5_recipe_capture_fields_literal_override.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_capture_reference_syntax_test  "test/sm5_recipe_capture_reference_syntax.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_invalid_resolved_emit_test       "test/sm5_recipe_invalid_resolved_emit.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_replace_capture_test             "test/sm5_recipe_replace_capture.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_match_sequence_test              "test/sm5_recipe_match_sequence.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_before_after_test                "test/sm5_recipe_before_after.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_add_sampler_decl_test            "test/sm5_recipe_add_sampler_decl.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_objective_ign_texture_patch_test "test/sm5_recipe_objective_ign_texture_patch.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/sm5_objective.recipe.yml"  "${CMAKE_CURRENT_BINARY_DIR}/test-output/0x7AFF256C.ps_5_0.objective.recipe.patched.cso")
dxp_add_test(dxil_patch_sm5_recipe_state_temp_emit_test             "test/sm5_recipe_state_temp_emit.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_context_variables_test           "test/sm5_recipe_context_variables.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_operand_index_capture_test       "test/sm5_recipe_operand_index_capture.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso"  "${CMAKE_CURRENT_BINARY_DIR}/test-output/0x7AFF256C.ps_5_0.operand_index_capture.patched.cso")
dxp_add_test(dxil_patch_sm5_recipe_rule_probe_test                  "test/sm5_recipe_rule_probe.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_rule_predicate_test              "test/sm5_recipe_rule_predicate.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_parse_validation_test            "test/sm5_recipe_parse_validation.cpp")
dxp_add_test(dxil_patch_sm5_recipe_screen_uv_insert_test            "test/sm5_recipe_screen_uv_insert.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/Physically_Based_Standard_Both_Shader_0x67A01779.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_resource_uav_decl_test           "test/sm5_recipe_resource_uav_decl.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso")
# CLI tool tests (dxp command)
dxp_add_test(dxp_validate_recipe_blue_noise_emit  "tools/dxp.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/blue_noise_emit.recipe.yml")
dxp_add_test(dxp_validate_sm5_recipe_noop         "tools/dxp.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/sm5_noop.recipe.yml")
dxp_add_test(dxp_patch_sm5_recipe_noop_0x7AFF256C  "tools/dxp.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/sm5_noop.recipe.yml"  "${CMAKE_CURRENT_BINARY_DIR}/test-output/0x7AFF256C.ps_5_0.noop.cli.patched.cso")
dxp_add_test(dxp_validate_sm5_recipe_objective  "tools/dxp.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/sm5_objective.recipe.yml")
dxp_add_test(dxp_patch_sm5_recipe_objective_0x7AFF256C  "tools/dxp.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/test/shaders/0x7AFF256C.ps_5_0.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/test/recipes/sm5_objective.recipe.yml"  "${CMAKE_CURRENT_BINARY_DIR}/test-output/0x7AFF256C.ps_5_0.objective.recipe.cli.patched.cso")
