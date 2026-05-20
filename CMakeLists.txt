cmake_minimum_required(VERSION 3.25)

project(DirectXShaderPatcher LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DXC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/DirectXShaderCompiler")
set(DXC_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/dxc")

if(NOT EXISTS "${DXC_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "DirectXShaderCompiler submodule is missing. Run `git submodule update --init --recursive` first.")
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

add_subdirectory("${DXC_SOURCE_DIR}" "${DXC_BINARY_DIR}")

add_library(dxp_lib STATIC
  "src/dxp/sm6/Container.cpp"
  "src/dxp/sm6/Parse.cpp"
  "src/dxp/sm6/Patch.cpp"
  "src/dxp/sm6/Recipe.cpp"
  "src/dxp/sm6/Resources.cpp"
  "src/dxp/sm6/Transforms.cpp"
)

add_library(dxil_patch_test_support STATIC
  "test/TestSupport.cpp"
  "test/ComputeNoiseSupport.cpp"
)

add_executable(dxp
  "tools/dxp.cpp"
)

add_executable(dxil_patch_texture_add_test
  "test/texture_add_0x965B1360.cpp"
)

add_executable(dxil_patch_texture2darray_add_test
  "test/texture2darray_add_0x965B1360.cpp"
)

add_executable(dxil_patch_texture_uav_add_test
  "test/texture_uav_add_0x965B1360.cpp"
)

add_executable(dxil_patch_cbuffer_add_test
  "test/cbuffer_add_0x965B1360.cpp"
)

add_executable(dxil_patch_rendertarget_dsl_test
  "test/rendertarget_dsl_0x965B1360.cpp"
)

add_executable(dxil_patch_gatherforeground_isfast_test
  "test/gatherforeground_isfast_0xAD818E14.cpp"
)

add_executable(dxil_patch_gatherforeground_isfast_compare_test
  "test/gatherforeground_isfast_rule_compare.cpp"
)

add_executable(dxil_patch_gatherforeground_isfast_recipe_test
  "test/gatherforeground_isfast_recipe.cpp"
)

add_executable(dxil_patch_declarative_rewrite_recipe_test
  "test/declarative_rewrite_recipe_0x56C468C3.cpp"
)

add_executable(dxil_patch_declarative_emit_resource_recipe_test
  "test/declarative_emit_resource_recipe_0x56C468C3.cpp"
)

add_executable(dxil_patch_declarative_ign_nested_recipe_test
  "test/declarative_ign_nested_recipe_0x56C468C3.cpp"
)

add_executable(dxil_patch_declarative_ign_emit_recipe_test
  "test/declarative_ign_emit_recipe_0x56C468C3.cpp"
)

add_executable(dxil_patch_declarative_ign_emit_inmemory_recipe_test
  "test/declarative_ign_emit_inmemory_recipe_0x56C468C3.cpp"
)

add_executable(dxil_patch_declarative_blue_noise_emit_recipe_test
  "test/declarative_blue_noise_emit_recipe_0x56C468C3.cpp"
)

add_executable(dxil_patch_declarative_resource_name_match_recipe_test
  "test/declarative_resource_name_match_recipe_0x56C468C3.cpp"
)

add_executable(dxil_patch_declarative_prefilter_recipe_test
  "test/declarative_prefilter_recipe_0x56C468C3.cpp"
)

target_compile_definitions(dxp_lib PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_test_support PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxp PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_texture_add_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_texture2darray_add_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_texture_uav_add_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_cbuffer_add_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_rendertarget_dsl_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_gatherforeground_isfast_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_gatherforeground_isfast_compare_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_gatherforeground_isfast_recipe_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_declarative_rewrite_recipe_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_declarative_emit_resource_recipe_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_declarative_ign_nested_recipe_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_declarative_ign_emit_recipe_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_declarative_ign_emit_inmemory_recipe_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_declarative_blue_noise_emit_recipe_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_declarative_resource_name_match_recipe_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_declarative_prefilter_recipe_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_include_directories(dxp_lib PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_test_support PUBLIC
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxp PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_texture_add_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_texture2darray_add_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_texture_uav_add_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_cbuffer_add_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_rendertarget_dsl_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_gatherforeground_isfast_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_gatherforeground_isfast_compare_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_gatherforeground_isfast_recipe_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_declarative_rewrite_recipe_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_declarative_emit_resource_recipe_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_declarative_ign_nested_recipe_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_declarative_ign_emit_recipe_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_declarative_ign_emit_inmemory_recipe_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_declarative_blue_noise_emit_recipe_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_declarative_resource_name_match_recipe_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_include_directories(dxil_patch_declarative_prefilter_recipe_test PRIVATE
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
)

target_link_libraries(dxp_lib PUBLIC
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

target_link_libraries(dxil_patch_test_support PUBLIC
  dxp_lib
)

target_link_libraries(dxp PRIVATE
  dxp_lib
)

set_target_properties(dxp PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(dxil_patch_texture_add_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_texture2darray_add_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_texture_uav_add_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_cbuffer_add_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_rendertarget_dsl_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_gatherforeground_isfast_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_gatherforeground_isfast_compare_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_gatherforeground_isfast_recipe_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_declarative_rewrite_recipe_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_declarative_emit_resource_recipe_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_declarative_ign_nested_recipe_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_declarative_ign_emit_recipe_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_declarative_ign_emit_inmemory_recipe_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_declarative_blue_noise_emit_recipe_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_declarative_resource_name_match_recipe_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_declarative_prefilter_recipe_test PRIVATE
  dxil_patch_test_support
)

set_target_properties(dxil_patch_texture_add_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_texture2darray_add_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_texture_uav_add_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_cbuffer_add_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_rendertarget_dsl_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_gatherforeground_isfast_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_gatherforeground_isfast_compare_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_gatherforeground_isfast_recipe_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_declarative_rewrite_recipe_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_declarative_emit_resource_recipe_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_declarative_ign_nested_recipe_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_declarative_ign_emit_recipe_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_declarative_ign_emit_inmemory_recipe_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_declarative_blue_noise_emit_recipe_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_declarative_resource_name_match_recipe_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_declarative_prefilter_recipe_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(
  NAME texture_add_0x965B1360
  COMMAND dxil_patch_texture_add_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x965B1360.ps_6_6.cso"
)

add_test(
  NAME texture2darray_add_0x965B1360
  COMMAND dxil_patch_texture2darray_add_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x965B1360.ps_6_6.cso"
)

add_test(
  NAME texture_uav_add_0x965B1360
  COMMAND dxil_patch_texture_uav_add_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x965B1360.ps_6_6.cso"
)

add_test(
  NAME cbuffer_add_0x965B1360
  COMMAND dxil_patch_cbuffer_add_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x965B1360.ps_6_6.cso"
)

add_test(
  NAME rendertarget_dsl_0x965B1360
  COMMAND dxil_patch_rendertarget_dsl_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x965B1360.ps_6_6.cso"
)

add_test(
  NAME gatherforeground_isfast_0xAD818E14
  COMMAND dxil_patch_gatherforeground_isfast_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/GatherForeground_0xAD818E14.cs_6_6.cso"
)

add_test(
  NAME gatherforeground_isfast_0x56C468C3
  COMMAND dxil_patch_gatherforeground_isfast_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
)

add_test(
  NAME gatherforeground_isfast_0xC4720FE8
  COMMAND dxil_patch_gatherforeground_isfast_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0xC4720FE8.cs_6_6.cso"
)

add_test(
  NAME gatherforeground_isfast_compare_0xAD818E14
  COMMAND dxil_patch_gatherforeground_isfast_compare_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/GatherForeground_0xAD818E14.cs_6_6.cso"
)

add_test(
  NAME gatherforeground_isfast_compare_0x56C468C3
  COMMAND dxil_patch_gatherforeground_isfast_compare_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
)

add_test(
  NAME gatherforeground_isfast_compare_0xC4720FE8
  COMMAND dxil_patch_gatherforeground_isfast_compare_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0xC4720FE8.cs_6_6.cso"
)

add_test(
  NAME gatherforeground_isfast_recipe_0xAD818E14
  COMMAND dxil_patch_gatherforeground_isfast_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/GatherForeground_0xAD818E14.cs_6_6.cso"
)

add_test(
  NAME gatherforeground_isfast_recipe_0x56C468C3
  COMMAND dxil_patch_gatherforeground_isfast_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
)

add_test(
  NAME gatherforeground_isfast_recipe_0xC4720FE8
  COMMAND dxil_patch_gatherforeground_isfast_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0xC4720FE8.cs_6_6.cso"
)

add_test(
  NAME declarative_rewrite_recipe_0x56C468C3
  COMMAND dxil_patch_declarative_rewrite_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/recipes/frc_passthrough.recipe.yml"
)

add_test(
  NAME declarative_emit_resource_recipe_0x56C468C3
  COMMAND dxil_patch_declarative_emit_resource_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/recipes/threadid_to_cbuffer_emit.recipe.yml"
)

add_test(
  NAME declarative_ign_nested_recipe_0x56C468C3
  COMMAND dxil_patch_declarative_ign_nested_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/recipes/ign_nested_match.recipe.yml"
)

add_test(
  NAME declarative_ign_emit_recipe_0x56C468C3
  COMMAND dxil_patch_declarative_ign_emit_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/recipes/ign_subset_emit.recipe.yml"
)

add_test(
  NAME declarative_ign_emit_prefilter_recipe_0x56C468C3
  COMMAND dxil_patch_declarative_ign_emit_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/recipes/ign_subset_emit_prefilter.recipe.yml"
)

add_test(
  NAME declarative_ign_emit_inmemory_recipe_0x56C468C3
  COMMAND dxil_patch_declarative_ign_emit_inmemory_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/recipes/ign_subset_emit.recipe.yml"
)

add_test(
  NAME declarative_blue_noise_emit_recipe_0x56C468C3
  COMMAND dxil_patch_declarative_blue_noise_emit_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/recipes/blue_noise_emit.recipe.yml"
)

add_test(
  NAME declarative_blue_noise_emit_snapshot_recipe_0x56C468C3
  COMMAND dxil_patch_declarative_blue_noise_emit_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/recipes/blue_noise_emit_snapshot.recipe.yml"
)

add_test(
  NAME declarative_resource_name_match_recipe_0x56C468C3
  COMMAND dxil_patch_declarative_resource_name_match_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/recipes/resource_name_match.recipe.yml"
)

add_test(
  NAME declarative_prefilter_recipe_0x56C468C3
  COMMAND dxil_patch_declarative_prefilter_recipe_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/0x56C468C3.cs_6_6.cso"
)