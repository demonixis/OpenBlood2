cmake_minimum_required(VERSION 3.28)

# Use the configured, pinned source without another download or a vendored copy.
set(original "e5ef4b92edf2798db6f7c473f59bb362025b52eb0d4b5da044f49365f482a51a")
set(patched "be01f50d395cb9904a49a9e8a11f07be3d49ad7ccc1d372824bfbd21bb76b20d")
set(relative "src/gpu/vulkan/SDL_gpu_vulkan.c")
file(SHA256 "${SDL_SOURCE_DIR}/${relative}" hash)
if(NOT hash STREQUAL patched)
    message(FATAL_ERROR "Test requires the exact patched SDL source")
endif()
file(READ "${SDL_SOURCE_DIR}/${relative}" patched_source)
string(REPLACE
    "    if (createInfo.format == 0) { /* OpenBlood2: keep an explicit runtime format. */\n        createInfo.format = SDLToVK_TextureFormat[format];\n    }\n"
    "    createInfo.format = SDLToVK_TextureFormat[format];\n"
    original_source "${patched_source}")

foreach(case IN ITEMS original patched)
    set(directory "${ROOT}/${case}")
    set(path "${directory}/${relative}")
    file(CONFIGURE OUTPUT "${path}" CONTENT "${${case}_source}" @ONLY NEWLINE_STYLE LF)
    file(SHA256 "${path}" hash)
    if(NOT hash STREQUAL "${${case}}")
        message(FATAL_ERROR "Invalid ${case} fixture")
    endif()
    # The second invocation exercises the already-patched no-op path.
    foreach(pass RANGE 1 2)
        execute_process(COMMAND "${CMAKE_COMMAND}" "-DSDL_SOURCE_DIR=${directory}" -P "${PATCH}"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
        if(NOT result STREQUAL "0")
            message(FATAL_ERROR "${case} pass ${pass}: ${output}${error}")
        endif()
        file(SHA256 "${path}" hash)
        if(NOT hash STREQUAL patched)
            message(FATAL_ERROR "${case} pass ${pass} did not produce the patched bytes")
        endif()
    endforeach()
endforeach()

# An unexpected upstream edit is rejected and left untouched.
set(directory "${ROOT}/modified")
set(path "${directory}/${relative}")
file(CONFIGURE OUTPUT "${path}" CONTENT "${original_source}\n/* Unexpected upstream edit. */\n" @ONLY NEWLINE_STYLE LF)
file(SHA256 "${path}" before)
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSDL_SOURCE_DIR=${directory}" -P "${PATCH}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result STREQUAL "0" OR NOT error MATCHES "SDL Vulkan backend changed")
    message(FATAL_ERROR "Modified input was not rejected by the hash guard: ${output}${error}")
endif()
file(SHA256 "${path}" after)
if(NOT before STREQUAL after)
    message(FATAL_ERROR "Rejected input was modified")
endif()
