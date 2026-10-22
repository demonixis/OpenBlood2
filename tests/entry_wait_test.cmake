# A normal launch opens the original menu: no world is loaded or advanced before
# a player enters (New Game and --view-world enter the player before the first tick).
execute_process(COMMAND "${APP}" --config "${CONFIG}" --data-dir "${DATA_DIR}" --frames 12 --capture "${CAPTURE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}${err}")
if(NOT status EQUAL 0 OR NOT "${out}${err}" MATCHES "Frontend complete: 12 frames, world , ")
    message(FATAL_ERROR "Normal launch loaded a world before player entry: ${status}")
endif()
if(out MATCHES "Validation Error|VUID-" OR err MATCHES "Validation Error|VUID-")
    message(FATAL_ERROR "GPU validation failed")
endif()
if(NOT EXISTS "${CAPTURE}")
    message(FATAL_ERROR "No inspection capture")
endif()
file(SIZE "${CAPTURE}" bytes)
if(bytes LESS 1000)
    message(FATAL_ERROR "Inspection capture is empty")
endif()
