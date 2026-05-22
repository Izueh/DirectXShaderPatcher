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
  "src/dxp/sm5/Container.cpp"
  "src/dxp/sm5/Model.cpp"
  "src/dxp/sm5/Parse.cpp"
  "src/dxp/sm5/RecipeParse.cpp"
  "src/dxp/sm5/Serialize.cpp"
  "src/dxp/sm5/Match.cpp"
  "src/dxp/sm5/Rewrite.cpp"
  "src/dxp/sm5/Recipe.cpp"
  "src/dxp/sm5/Patch.cpp"
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

add_executable(dxil_patch_sm5_container_roundtrip_test
  "test/sm5_container_roundtrip.cpp"
)

add_executable(dxil_patch_sm5_replace_frc_with_mov_test
  "test/sm5_replace_frc_with_mov.cpp"
)

add_executable(dxil_patch_sm5_recipe_replace_frc_with_mov_test
  "test/sm5_recipe_replace_frc_with_mov.cpp"
)

add_executable(dxil_patch_sm5_recipe_emit_arbitrary_test
  "test/sm5_recipe_emit_arbitrary.cpp"
)

add_executable(dxil_patch_sm5_recipe_replace_capture_test
  "test/sm5_recipe_replace_capture.cpp"
)

add_executable(dxil_patch_sm5_recipe_match_sequence_test
  "test/sm5_recipe_match_sequence.cpp"
)

add_executable(dxil_patch_sm5_recipe_add_sampler_decl_test
  "test/sm5_recipe_add_sampler_decl.cpp"
)

add_executable(dxil_patch_sm5_recipe_objective_ign_texture_patch_test
  "test/sm5_recipe_objective_ign_texture_patch.cpp"
)

add_executable(dxil_patch_sm5_recipe_reserve_temps_test
  "test/sm5_recipe_reserve_temps.cpp"
)

add_executable(dxil_patch_sm5_recipe_custom_step_reserve_temps_test
  "test/sm5_recipe_custom_step_reserve_temps.cpp"
)

add_executable(dxil_patch_sm5_recipe_state_temp_emit_test
  "test/sm5_recipe_state_temp_emit.cpp"
)

add_executable(dxil_patch_sm5_recipe_pattern_prefilter_test
  "test/sm5_recipe_pattern_prefilter.cpp"
)

add_executable(dxil_patch_sm5_recipe_rule_predicate_test
  "test/sm5_recipe_rule_predicate.cpp"
)

add_executable(dxil_patch_sm5_recipe_parse_validation_test
  "test/sm5_recipe_parse_validation.cpp"
)

add_executable(dxil_patch_sm5_recipe_screen_uv_insert_test
  "test/sm5_recipe_screen_uv_insert.cpp"
)

add_executable(dxil_patch_sm5_recipe_resource_uav_decl_test
  "test/sm5_recipe_resource_uav_decl.cpp"
)

set(DXP_PROJECT_INCLUDE_DIRS
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

set(DXP_EXTERNAL_INCLUDE_DIRS
  "${DXC_SOURCE_DIR}/include"
  "${DXC_BINARY_DIR}/include"
  "${DXC_BINARY_DIR}/tools/clang/include"
  "${DXC_SOURCE_DIR}/tools/clang/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/WDK/includes"
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

target_compile_definitions(dxil_patch_sm5_container_roundtrip_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_replace_frc_with_mov_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_replace_frc_with_mov_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_emit_arbitrary_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_replace_capture_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_match_sequence_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_add_sampler_decl_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_objective_ign_texture_patch_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_reserve_temps_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_custom_step_reserve_temps_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_state_temp_emit_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_pattern_prefilter_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_rule_predicate_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_parse_validation_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_screen_uv_insert_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_compile_definitions(dxil_patch_sm5_recipe_resource_uav_decl_test PRIVATE
  NOMINMAX
  $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
  $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

target_include_directories(dxp_lib PUBLIC
  ${DXP_PROJECT_INCLUDE_DIRS}
)
target_include_directories(dxp_lib SYSTEM PUBLIC
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_test_support SYSTEM PUBLIC
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxp SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_texture_add_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_texture2darray_add_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_texture_uav_add_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_cbuffer_add_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_rendertarget_dsl_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_gatherforeground_isfast_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_gatherforeground_isfast_compare_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_gatherforeground_isfast_recipe_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_declarative_rewrite_recipe_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_declarative_emit_resource_recipe_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_declarative_ign_nested_recipe_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_declarative_ign_emit_recipe_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_declarative_ign_emit_inmemory_recipe_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_declarative_blue_noise_emit_recipe_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_declarative_resource_name_match_recipe_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_declarative_prefilter_recipe_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_container_roundtrip_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_replace_frc_with_mov_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_replace_frc_with_mov_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_emit_arbitrary_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_replace_capture_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_match_sequence_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_add_sampler_decl_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_objective_ign_texture_patch_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_reserve_temps_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_custom_step_reserve_temps_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_state_temp_emit_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_pattern_prefilter_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_rule_predicate_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_parse_validation_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_screen_uv_insert_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
)

target_include_directories(dxil_patch_sm5_recipe_resource_uav_decl_test SYSTEM PRIVATE
  ${DXP_EXTERNAL_INCLUDE_DIRS}
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

target_link_libraries(dxil_patch_sm5_container_roundtrip_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_replace_frc_with_mov_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_replace_frc_with_mov_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_emit_arbitrary_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_replace_capture_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_match_sequence_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_add_sampler_decl_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_objective_ign_texture_patch_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_reserve_temps_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_custom_step_reserve_temps_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_state_temp_emit_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_pattern_prefilter_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_rule_predicate_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_parse_validation_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_screen_uv_insert_test PRIVATE
  dxil_patch_test_support
)

target_link_libraries(dxil_patch_sm5_recipe_resource_uav_decl_test PRIVATE
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

set_target_properties(dxil_patch_sm5_container_roundtrip_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_replace_frc_with_mov_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_replace_frc_with_mov_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_emit_arbitrary_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_replace_capture_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_match_sequence_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_add_sampler_decl_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_objective_ign_texture_patch_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_reserve_temps_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_custom_step_reserve_temps_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_state_temp_emit_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_pattern_prefilter_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_rule_predicate_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_parse_validation_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_screen_uv_insert_test PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set_target_properties(dxil_patch_sm5_recipe_resource_uav_decl_test PROPERTIES
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

add_test(
  NAME dxp_validate_recipe_blue_noise_emit
  COMMAND dxp validate
          "${CMAKE_CURRENT_SOURCE_DIR}/recipes/blue_noise_emit.recipe.yml"
)

add_test(
  NAME dxp_validate_sm5_recipe_noop
  COMMAND dxp validate-sm5
          "${CMAKE_CURRENT_SOURCE_DIR}/test/sm5_noop.recipe.yml"
)

add_test(
  NAME dxp_patch_sm5_recipe_noop_aliens_0x7AFF256C
  COMMAND dxp patch-sm5
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/test/sm5_noop.recipe.yml"
          "${CMAKE_CURRENT_BINARY_DIR}/test-output/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.noop.cli.patched.cso"
)

add_test(
  NAME dxp_validate_sm5_recipe_objective
  COMMAND dxp validate-sm5
          "${CMAKE_CURRENT_SOURCE_DIR}/test/sm5_objective.recipe.yml"
)

add_test(
  NAME dxp_patch_sm5_recipe_objective_aliens_0x7AFF256C
  COMMAND dxp patch-sm5
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/test/sm5_objective.recipe.yml"
          "${CMAKE_CURRENT_BINARY_DIR}/test-output/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.objective.recipe.cli.patched.cso"
)

add_test(
  NAME dxp_validate_recipe_invalid
  COMMAND ${CMAKE_COMMAND}
          -DDXP_EXE=$<TARGET_FILE:dxp>
          -DRECIPE_PATH=${CMAKE_CURRENT_SOURCE_DIR}/test/invalid_recipe.recipe.yml
          -P ${CMAKE_CURRENT_SOURCE_DIR}/test/RunDxpInvalidRecipeValidation.cmake
)

add_test(
  NAME sm5_container_roundtrip_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_container_roundtrip_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
)

add_test(
  NAME sm5_replace_frc_with_mov_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_replace_frc_with_mov_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
          "${CMAKE_CURRENT_BINARY_DIR}/test-output/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.frc_to_mov.patched.cso"
)

add_test(
  NAME sm5_recipe_replace_frc_with_mov_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_replace_frc_with_mov_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/test/sm5_frc_to_mov.recipe.yml"
)

add_test(
  NAME sm5_recipe_emit_arbitrary_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_emit_arbitrary_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
)

add_test(
  NAME sm5_recipe_replace_capture_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_replace_capture_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
)

add_test(
  NAME sm5_recipe_match_sequence_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_match_sequence_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
)

add_test(
  NAME sm5_recipe_add_sampler_decl_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_add_sampler_decl_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
)

add_test(
  NAME sm5_recipe_objective_ign_texture_patch_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_objective_ign_texture_patch_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
          "${CMAKE_CURRENT_SOURCE_DIR}/test/sm5_objective.recipe.yml"
          "${CMAKE_CURRENT_BINARY_DIR}/test-output/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.objective.recipe.patched.cso"
)

add_test(
  NAME sm5_recipe_reserve_temps_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_reserve_temps_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
)

add_test(
  NAME sm5_recipe_custom_step_reserve_temps_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_custom_step_reserve_temps_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
)

add_test(
  NAME sm5_recipe_state_temp_emit_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_state_temp_emit_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
)

add_test(
  NAME sm5_recipe_pattern_prefilter_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_pattern_prefilter_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
)

add_test(
  NAME sm5_recipe_rule_predicate_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_rule_predicate_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
)

add_test(
  NAME sm5_recipe_parse_validation
  COMMAND dxil_patch_sm5_recipe_parse_validation_test
)

add_test(
  NAME sm5_recipe_screen_uv_insert_physically_based_standard_0x67A01779
  COMMAND dxil_patch_sm5_recipe_screen_uv_insert_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Physically_Based_Standard_Both_Shader_0x67A01779.ps_5_0.cso"
)

add_test(
  NAME sm5_recipe_resource_uav_decl_aliens_0x7AFF256C
  COMMAND dxil_patch_sm5_recipe_resource_uav_decl_test
          "${CMAKE_CURRENT_SOURCE_DIR}/test/Aliens_Fireteam_Volumetric_IGN_0x7AFF256C.ps_5_0.cso"
)
