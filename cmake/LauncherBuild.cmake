include(ExternalProject)

# A second CMake tree keeps native dialogs and HTTP/ZIP dependencies out of the
# game's SDL configuration and link graph. Use the same compiler/toolchain.
set(launcher_cmake_args
    "-DCMAKE_BUILD_TYPE:STRING=$<CONFIG>"
    "-DBLOOD2_BUILD_GAME:BOOL=OFF"
    "-DBLOOD2_BUILD_LAUNCHER:BOOL=ON"
    "-DBLOOD2_INSPECTOR:BOOL=OFF"
    "-DBLOOD2_GPU_TESTS:BOOL=OFF"
    "-DBLOOD2_SANITIZERS:BOOL=${BLOOD2_SANITIZERS}"
    "-DBUILD_TESTING:BOOL=OFF")
set(launcher_forward_variables
    CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_MAKE_PROGRAM CMAKE_TOOLCHAIN_FILE
    CMAKE_C_COMPILER_LAUNCHER CMAKE_CXX_COMPILER_LAUNCHER
    CMAKE_SYSROOT CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_OSX_SYSROOT
    CMAKE_PREFIX_PATH CMAKE_FIND_ROOT_PATH CMAKE_MSVC_RUNTIME_LIBRARY OPENSSL_ROOT_DIR)
if(CMAKE_CROSSCOMPILING)
    list(APPEND launcher_forward_variables CMAKE_SYSTEM_NAME CMAKE_SYSTEM_VERSION CMAKE_SYSTEM_PROCESSOR)
endif()
foreach(prefix IN ITEMS CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS)
    list(APPEND launcher_forward_variables ${prefix})
    foreach(config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        list(APPEND launcher_forward_variables ${prefix}_${config})
    endforeach()
endforeach()
foreach(variable IN LISTS launcher_forward_variables)
    if(DEFINED ${variable})
        string(REPLACE ";" "|" value "${${variable}}")
        list(APPEND launcher_cmake_args "-D${variable}:STRING=${value}")
    endif()
endforeach()

ExternalProject_Add(blood2_launcher
    SOURCE_DIR "${PROJECT_SOURCE_DIR}"
    BINARY_DIR "${PROJECT_BINARY_DIR}/launcher"
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    LIST_SEPARATOR "|"
    CMAKE_ARGS ${launcher_cmake_args}
    # Re-enter the incremental child build on every parent build, so source edits
    # and deleted outputs are noticed. Bound nested parallelism independently.
    BUILD_ALWAYS TRUE
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config "$<CONFIG>"
        --target blood2_launcher --parallel 2
    INSTALL_COMMAND "${CMAKE_COMMAND}" --install <BINARY_DIR> --config "$<CONFIG>"
        --prefix "$<TARGET_FILE_DIR:blood2_app>" --component Launcher
    USES_TERMINAL_CONFIGURE TRUE
    USES_TERMINAL_BUILD TRUE
    USES_TERMINAL_INSTALL TRUE)

# Include the launcher in both the default build and explicit game builds.
add_dependencies(blood2_app blood2_launcher)
