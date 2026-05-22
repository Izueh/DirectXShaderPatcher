execute_process(
  COMMAND "${DXP_EXE}" validate "${RECIPE_PATH}"
  RESULT_VARIABLE dxp_status
  OUTPUT_VARIABLE dxp_stdout
  ERROR_VARIABLE dxp_stderr
)

if(dxp_status EQUAL 0)
  message(FATAL_ERROR
    "Expected recipe validation to fail for '${RECIPE_PATH}', but dxp exited successfully.\n"
    "stdout:\n${dxp_stdout}\n"
    "stderr:\n${dxp_stderr}"
  )
endif()

set(dxp_output "${dxp_stdout}${dxp_stderr}")
if(NOT dxp_output MATCHES "Recipe validation failed:")
  message(FATAL_ERROR
    "Expected dxp validation failure output for '${RECIPE_PATH}'.\n"
    "stdout:\n${dxp_stdout}\n"
    "stderr:\n${dxp_stderr}"
  )
endif()