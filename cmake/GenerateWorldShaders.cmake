# Explicit offline regeneration; normal builds use the checked-in SPIR-V headers.
find_program(GLSLANG glslangValidator REQUIRED)
set(shader_dir "${CMAKE_CURRENT_LIST_DIR}/../src/engine/shaders")
foreach(stage vert frag)
    execute_process(COMMAND "${GLSLANG}" -V --target-env vulkan1.0
        --vn "world_${stage}" -o "${shader_dir}/world_${stage}.h" "${shader_dir}/world.${stage}"
        COMMAND_ERROR_IS_FATAL ANY)
endforeach()
