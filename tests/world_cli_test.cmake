execute_process(COMMAND "${APP}" --config "${CONFIG}" --data-dir "${DATA_DIR}" --inspect-world
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0 OR NOT out MATCHES "580 objects, 227 original instances, 116 BSP models")
    message(FATAL_ERROR "World inspection: ${status}: ${out}${err}")
endif()
execute_process(COMMAND "${APP}" --config "${CONFIG}" --data-dir "${DATA_DIR}" --inspect-player
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0 OR NOT out MATCHES "Original shell/player entry inspection succeeded")
    message(FATAL_ERROR "Original player entry failed: ${status}: ${out}${err}")
endif()
execute_process(COMMAND "${APP}" --config "${CONFIG}" --data-dir "${DATA_DIR}" --start-game
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0 OR NOT out MATCHES "Original session startup validated: player associated before first update" OR
   NOT out MATCHES "bounded headless check, not full M2 acceptance")
    message(FATAL_ERROR "Checked startup failed: ${status}: ${out}${err}")
endif()
execute_process(COMMAND "${APP}" --config "${CONFIG}" --data-dir "${DATA_DIR}" --inspect-world --world ../outside
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 5)
    message(FATAL_ERROR "Unsafe world path accepted: ${status}: ${out}${err}")
endif()
