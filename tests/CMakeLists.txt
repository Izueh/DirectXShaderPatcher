# Test helper function — creates test executables and registers them with CTest
function(dxp_add_test EXECUTABLE_NAME SOURCE_FILE)
    if(DXP_ARCH STREQUAL "x86")
        return()
    endif()

    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE}")
        message(WARNING "Test source file skipped (not found): ${SOURCE_FILE}")
        return()
    endif()

    string(REPLACE "dxil_patch_" "" CLEAN_EXE_NAME "${EXECUTABLE_NAME}")

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

    if(SOURCE_FILE STREQUAL "tools/dxp.cpp")
        set(RUN_BINARY "dxp")
    else()
        set(RUN_BINARY "${CLEAN_EXE_NAME}")
        if(NOT TARGET ${RUN_BINARY})
            add_executable(${RUN_BINARY} ${SOURCE_FILE})
            target_include_directories(${RUN_BINARY} PRIVATE
                ${CMAKE_SOURCE_DIR}
                ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/src
                ${DXC_SOURCE_DIR}/include
                ${DXC_BINARY_DIR}/include
                ${CMAKE_SOURCE_DIR}/external
                ${CMAKE_SOURCE_DIR}/external/WDK/includes
                ${CMAKE_SOURCE_DIR}/external/glaze/include
            )
            target_compile_definitions(${RUN_BINARY} PRIVATE NOMINMAX)
            target_link_libraries(${RUN_BINARY} PRIVATE dxpatcher)

            if(WIN32)
                add_dependencies(${RUN_BINARY} copy_dxcompiler_runtime)
            endif()
        endif()
        # Test outputs (patched shaders) go next to the test binary in the
        # per-config build directory, never into the source tree.
        target_compile_definitions(${RUN_BINARY} PRIVATE
            DXP_TEST_OUTPUT_DIR="$<TARGET_FILE_DIR:${RUN_BINARY}>")
    endif()

    set(RUN_COMMAND_ARGS ${ARGN})
    if(RUN_BINARY STREQUAL "dxp")
        if(TEST_RUN_NAME MATCHES "validate_sm5_recipe")
            list(INSERT RUN_COMMAND_ARGS 0 "validate")
            list(INSERT RUN_COMMAND_ARGS 0 "sm5")
        elseif(TEST_RUN_NAME MATCHES "validate_recipe")
            list(INSERT RUN_COMMAND_ARGS 0 "validate")
            list(INSERT RUN_COMMAND_ARGS 0 "sm6")
        elseif(TEST_RUN_NAME MATCHES "patch_sm5_recipe")
            # CLI expects: patch <recipe> <input> [output] [--log-level <level>]
            # Test args come as: <input> <recipe> [output] — swap first two
            list(GET ARGN 0 _arg_input)
            list(GET ARGN 1 _arg_recipe)
            list(GET ARGN 2 _arg_output)
            set(RUN_COMMAND_ARGS "sm5" "patch" "${_arg_recipe}" "${_arg_input}")
            if(DEFINED _arg_output)
                list(APPEND RUN_COMMAND_ARGS "${_arg_output}")
            endif()
        endif()
    endif()

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

    add_test(NAME ${TEST_RUN_NAME} COMMAND ${RUN_BINARY} ${RUN_COMMAND_ARGS})
endfunction()

# All E2E Tests (x64 only — test support library depends on SM6)
if(NOT DXP_ARCH STREQUAL "x86")
dxp_add_test(dxil_patch_texture_add_test                            "texture_add_0x965B1360.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x965B1360.ps_6_6.cso")

dxp_add_test(dxil_patch_rendertarget_dsl_test                       "rendertarget_dsl_0x965B1360.cpp"    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x965B1360.ps_6_6.cso")
dxp_add_test(dxil_patch_sm6_gatherforeground_isfast_recipe_test     "sm6_gatherforeground_isfast_recipe.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/GatherForeground_0xAD818E14.cs_6_6.cso")
dxp_add_test(dxil_patch_sm6_gatherforeground_isfast_recipe_test     "sm6_gatherforeground_isfast_recipe.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(dxil_patch_sm6_gatherforeground_isfast_recipe_test     "sm6_gatherforeground_isfast_recipe.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0xC4720FE8.cs_6_6.cso")
dxp_add_test(dxil_patch_sm6_frc_passthrough                 "sm6_frc_passthrough.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(dxil_patch_sm6_group_id_to_cbuffer             "sm6_group_id_to_cbuffer.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(dxil_patch_sm6_deep_nested_match               "sm6_deep_nested_match.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(dxil_patch_sm6_complex_emit                    "sm6_complex_emit.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(dxil_patch_sm6_blue_noise_emit                 "sm6_blue_noise_emit.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(dxil_patch_sm6_identity_rewrite                "sm6_identity_rewrite.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(dxil_patch_sm5_container_roundtrip_test                "sm5_container_roundtrip.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_roundtrip_byte_identical_test           "sm5_roundtrip_byte_identical.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_replace_frc_with_mov_test        "sm5_recipe_replace_frc_with_mov.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/recipes/sm5_frc_to_mov.recipe.yml")
dxp_add_test(dxil_patch_sm5_recipe_emit_arbitrary_test              "sm5_recipe_emit_arbitrary.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_builder_capture_fields_test      "sm5_recipe_builder_capture_fields.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_capture_fields_literal_override_test  "sm5_recipe_capture_fields_literal_override.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_capture_reference_syntax_test  "sm5_recipe_capture_reference_syntax.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_invalid_resolved_emit_test       "sm5_recipe_invalid_resolved_emit.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_replace_capture_test             "sm5_recipe_replace_capture.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_match_sequence_test              "sm5_recipe_match_sequence.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_before_after_test                "sm5_recipe_before_after.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_add_sampler_decl_test            "sm5_recipe_add_sampler_decl.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_objective_ign_texture_patch_test "sm5_recipe_objective_ign_texture_patch.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/recipes/sm5_objective.recipe.yml")
dxp_add_test(dxil_patch_sm5_recipe_state_temp_emit_test             "sm5_recipe_state_temp_emit.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_context_variables_test           "sm5_recipe_context_variables.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_condition_missing_operand_test   "sm5_recipe_condition_missing_operand.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_operand_index_capture_test       "sm5_recipe_operand_index_capture.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_cross_step_capture_test          "sm5_recipe_cross_step_capture.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_match_captured_test               "sm5_recipe_match_captured_test.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_rule_predicate_test              "sm5_recipe_rule_predicate.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_parse_validation_test            "sm5_recipe_parse_validation.cpp")
dxp_add_test(sm5_check_steps_test                                       "sm5_check_steps_test.cpp")
dxp_add_test(dxil_patch_sm5_recipe_instruction_capture_test         "sm5_recipe_instruction_capture.cpp")
dxp_add_test(dxil_patch_sm5_recipe_screen_uv_insert_test            "sm5_recipe_screen_uv_insert.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Physically_Based_Standard_Both_Shader_0x67A01779.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_resource_uav_decl_test           "sm5_recipe_resource_uav_decl.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_operand_role_conversion_test       "sm5_recipe_operand_role_conversion.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0xCDF14206.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_emit_relative_operand_test         "sm5_recipe_emit_relative_operand.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_match_relative_operand_test        "sm5_recipe_match_relative_operand.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(dxil_patch_sm5_recipe_relative_operand_validation_test   "sm5_recipe_relative_operand_validation.cpp")
dxp_add_test(dxil_patch_sm5_recipe_match_immediate_shorthand_test       "sm5_recipe_match_immediate_shorthand.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(sm6_check_steps_test  "sm6_check_steps_test.cpp")
dxp_add_test(dxil_patch_sm5_recipe_resource_export_test  "sm5_recipe_resource_export.cpp")
dxp_add_test(dxil_patch_sm5_recipe_export_validation_test  "sm5_recipe_export_validation.cpp")
dxp_add_test(sm5_resource_capture_sample_test            "sm5_resource_capture_sample.cpp")
dxp_add_test(sm5_resource_capture_uav_test               "sm5_resource_capture_uav.cpp")
dxp_add_test(sm5_resource_capture_sampler_test           "sm5_resource_capture_sampler.cpp")
dxp_add_test(sm5_immediate_capture_constants_test        "sm5_immediate_capture_constants.cpp")
dxp_add_test(sm5_export_temp_no_crash_test               "sm5_export_temp_no_crash.cpp")
dxp_add_test(sm6_rewrite_semantics_test                     "sm6_rewrite_semantics_test.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(sm6_typed_constant_test                        "sm6_typed_constant_test.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(sm5_export_immediate_types_test      "sm5_export_immediate_types.cpp")
dxp_add_test(dxil_patch_sm6_recipe_resource_export_test  "sm6_recipe_resource_export.cpp")
dxp_add_test(sm6_recipe_export_validation_test           "sm6_recipe_export_validation.cpp")
dxp_add_test(sm6_resource_capture_rawbufferload_test     "sm6_resource_capture_rawbufferload.cpp")
dxp_add_test(sm6_resource_capture_textureload_test       "sm6_resource_capture_textureload.cpp")
dxp_add_test(sm6_resource_capture_samplelevel_test       "sm6_resource_capture_samplelevel.cpp")
dxp_add_test(sm6_immediate_capture_frc_test              "sm6_immediate_capture_frc.cpp")
dxp_add_test(sm6_export_non_resource_no_crash_test       "sm6_export_non_resource_no_crash.cpp")
endif()

# CLI tool tests (always defined — no-op for x86)
# CLI smoke test — dxp --version must exit 0 (exit-code check; guards the flag wiring).
if(NOT DXP_ARCH STREQUAL "x86")
    add_test(NAME dxp_version COMMAND dxp --version)
endif()

dxp_add_test(dxp_validate_sm5_recipe_noop         "tools/dxp.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/recipes/sm5_noop.recipe.yml")
dxp_add_test(dxp_patch_sm5_recipe_noop_0x7AFF256C  "tools/dxp.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/recipes/sm5_noop.recipe.yml"  "${CMAKE_CURRENT_BINARY_DIR}/test-output/0x7AFF256C.ps_5_0.noop.cli.patched.cso")
dxp_add_test(dxp_validate_sm5_recipe_objective  "tools/dxp.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/recipes/sm5_objective.recipe.yml")
dxp_add_test(dxp_patch_sm5_recipe_objective_0x7AFF256C  "tools/dxp.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso"  "${CMAKE_CURRENT_SOURCE_DIR}/recipes/sm5_objective.recipe.yml"  "${CMAKE_CURRENT_BINARY_DIR}/test-output/0x7AFF256C.ps_5_0.objective.recipe.cli.patched.cso")

# Recipe thread-safety / reuse tests — one shared recipe run concurrently over
# several distinct shaders must stay byte-identical to a single-threaded
# baseline (atomic validation cache + per-call PatchOptions + per-thread
# LLVMContext on SM6).
dxp_add_test(sm5_recipe_thread_safety_test "sm5_recipe_thread_safety_test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Physically_Based_Standard_Both_Shader_0x67A01779.ps_5_0.cso" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0xCDF14206.ps_5_0.cso" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/relative_index_test.cs_5_0.cso")
dxp_add_test(sm6_recipe_thread_safety_test "sm6_recipe_thread_safety_test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/GatherForeground_0xAD818E14.cs_6_6.cso" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0xC4720FE8.cs_6_6.cso" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x965B1360.ps_6_6.cso")
dxp_add_test(sm5_recipe_logging_test "sm5_recipe_logging_test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(sm6_recipe_logging_test "sm6_recipe_logging_test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(sm5_recipe_required_no_match_test "sm5_recipe_required_no_match_test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(sm6_recipe_required_no_match_test "sm6_recipe_required_no_match_test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(sm5_recipe_add_resource_tolerance_test "sm5_recipe_add_resource_tolerance_test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x7AFF256C.ps_5_0.cso")
dxp_add_test(sm6_recipe_error_contract_test "sm6_recipe_error_contract_test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")
dxp_add_test(sm6_recipe_match_captured_test "sm6_recipe_match_captured_test.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/0x56C468C3.cs_6_6.cso")

# Benchmarks (conditional)
if(DXP_BUILD_BENCHMARKS)
    add_executable(sm5_benchmarks
        "benchmarks/benchmark_helpers.cpp"
        "benchmarks/sm5_benchmarks.cpp"
    )
    target_include_directories(sm5_benchmarks PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/external/glaze/include)
    target_link_libraries(sm5_benchmarks PRIVATE dxpatcher benchmark::benchmark)

    if(WIN32)
        add_dependencies(sm5_benchmarks copy_dxcompiler_runtime)
    endif()

    add_test(NAME sm5_benchmarks COMMAND sm5_benchmarks --benchmark_min_time=0.01s)
endif()
