function(blood2_project_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(${target} PRIVATE "$<$<CONFIG:Release>:/Zi>")
        target_link_options(${target} PRIVATE "$<$<CONFIG:Release>:/DEBUG>")
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        # Retain source locations for post-mortem diagnosis without changing Release optimization.
        target_compile_options(${target} PRIVATE "$<$<CONFIG:Release>:-g1>")
        if(BLOOD2_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

# Gives every test of the calling directory its own BLOOD2_USER_DIR under the build tree, so no
# test reads or writes the real per-user preferences, controls, saves or logs. Deferred to the
# end of the directory so that it covers every add_test().
function(blood2_isolate_test_user_directories)
    cmake_language(DEFER CALL _blood2_set_test_user_directories)
endfunction()
function(_blood2_set_test_user_directories)
    get_property(tests DIRECTORY PROPERTY TESTS)
    foreach(test IN LISTS tests)
        set_property(TEST "${test}" APPEND PROPERTY ENVIRONMENT
            "BLOOD2_USER_DIR=${CMAKE_CURRENT_BINARY_DIR}/user-dirs/${test}")
    endforeach()
endfunction()
