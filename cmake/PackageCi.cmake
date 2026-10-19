cmake_minimum_required(VERSION 3.28)

# Run from the repository root after both CI presets have passed their tests.
if(NOT PLATFORM MATCHES "^(linux-x64|windows-x64|macos-arm64)$")
  message(FATAL_ERROR "Set PLATFORM to linux-x64, windows-x64 or macos-arm64")
endif()
get_filename_component(source "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(build "${source}/build/ci")
set(name "OpenBlood2-${PLATFORM}")
set(package "${build}/dist/${name}")
set(suffix "")
if(CMAKE_HOST_WIN32)
  set(suffix .exe)
endif()
if(EXISTS "${package}")
  message(FATAL_ERROR "Package directory already exists: ${package}; remove it before repackaging")
endif()
file(MAKE_DIRECTORY "${package}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${build}/launcher"
  --prefix "${package}" --component Launcher COMMAND_ERROR_IS_FATAL ANY)
file(COPY "${build}/game/blood2${suffix}" "${source}/LICENSE" DESTINATION "${package}")
file(COPY "${source}/docs/ci_builds.md" DESTINATION "${package}")
file(COPY "${source}/docs/implementation/provenance.md" DESTINATION "${package}/docs/implementation")
file(COPY "${source}/config/blood2.example.ini" DESTINATION "${package}/config")
if(NOT EXISTS "$ENV{OPENSSL_LICENSE}")
  message(FATAL_ERROR "Set OPENSSL_LICENSE to the installed OpenSSL license file")
endif()
file(COPY_FILE "$ENV{OPENSSL_LICENSE}" "${package}/licenses/OpenSSL.txt")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  # Debian's copyright file references the full license at this path.
  file(COPY /usr/share/common-licenses/Apache-2.0 DESTINATION "${package}/licenses")
endif()
if(CMAKE_HOST_WIN32)
  # Both project code and the hosted OpenSSL static libraries use the MSVC DLL CRT.
  file(TO_CMAKE_PATH "$ENV{VCToolsRedistDir}" redist)
  file(GLOB runtime "${redist}/x64/Microsoft.VC*.CRT/*.dll")
  if(NOT runtime)
    message(FATAL_ERROR "No x64 MSVC runtime found under VCToolsRedistDir")
  endif()
  file(COPY ${runtime} DESTINATION "${package}")
endif()
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${source}"
  OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
file(WRITE "${package}/BUILD.txt" "OpenBlood2\nPlatform: ${PLATFORM}\nConfiguration: Release\nCommit: ${revision}\n")

# Check the staged executables as well as the build-tree binaries.
execute_process(COMMAND "${package}/blood2${suffix}" --launcher-info
  WORKING_DIRECTORY "${package}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${package}/blood2-launcher${suffix}" --help
  WORKING_DIRECTORY "${package}" COMMAND_ERROR_IS_FATAL ANY)
if(CMAKE_HOST_WIN32)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${name}.zip" --format=zip "${name}"
    WORKING_DIRECTORY "${build}/dist" COMMAND_ERROR_IS_FATAL ANY)
else()
  # Tar preserves Unix executable permissions across artifact upload/download.
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar czf "${name}.tar.gz" "${name}"
    WORKING_DIRECTORY "${build}/dist" COMMAND_ERROR_IS_FATAL ANY)
endif()
