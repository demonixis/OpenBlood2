cmake_minimum_required(VERSION 3.28)

# Run from the repository root after both CI presets have passed their tests.
if(NOT PLATFORM MATCHES "^(linux-x64|windows-x64|macos-arm64)$")
  message(FATAL_ERROR "Set PLATFORM to linux-x64, windows-x64 or macos-arm64")
endif()
get_filename_component(source "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(build "${source}/build/ci")
# project(VERSION) is the single version source (Documentation/packaging.md).
file(STRINGS "${source}/CMakeLists.txt" project_line REGEX "^project\\(Blood2 VERSION ")
string(REGEX REPLACE "^project\\(Blood2 VERSION ([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" version "${project_line}")
if(NOT version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  message(FATAL_ERROR "Cannot read the project version from CMakeLists.txt")
endif()
# A release tag must name exactly this version.
if("$ENV{GITHUB_REF_TYPE}" STREQUAL "tag" AND NOT "$ENV{GITHUB_REF_NAME}" STREQUAL "v${version}")
  message(FATAL_ERROR "Tag $ENV{GITHUB_REF_NAME} does not match project version ${version} (expected v${version})")
endif()
set(name "OpenBlood2-${version}-${PLATFORM}")
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
file(COPY "${source}/Documentation/ci_builds.md" DESTINATION "${package}")
file(COPY "${source}/Documentation/implementation/provenance.md" DESTINATION "${package}/Documentation/implementation")
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
  # Do not accidentally satisfy missing package DLLs from a developer PATH.
  set(run_environment "${CMAKE_COMMAND}" -E env "PATH=$ENV{SystemRoot}/System32")
endif()
if(CMAKE_HOST_APPLE)
  foreach(executable IN ITEMS blood2 blood2-launcher)
    execute_process(COMMAND otool -L "${package}/${executable}"
      OUTPUT_VARIABLE linked COMMAND_ERROR_IS_FATAL ANY)
    string(REGEX MATCHALL "\n[ \t]+[^\n]+" dependencies "${linked}")
    foreach(dependency IN LISTS dependencies)
      string(STRIP "${dependency}" dependency)
      if(NOT dependency MATCHES "^(/usr/lib/|/System/Library/)")
        message(FATAL_ERROR "Non-system dependency in ${executable}: ${dependency}")
      endif()
    endforeach()
  endforeach()
endif()
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${source}"
  OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
file(WRITE "${package}/BUILD.txt" "OpenBlood2 ${version}\nPlatform: ${PLATFORM}\nConfiguration: Release\nCommit: ${revision}\n")

# Check the staged executables as well as the build-tree binaries.
execute_process(COMMAND ${run_environment} "${package}/blood2${suffix}" --launcher-info
  WORKING_DIRECTORY "${package}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND ${run_environment} "${package}/blood2-launcher${suffix}" --help
  WORKING_DIRECTORY "${package}" COMMAND_ERROR_IS_FATAL ANY)
if(CMAKE_HOST_WIN32)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${name}.zip" --format=zip "${name}"
    WORKING_DIRECTORY "${build}/dist" COMMAND_ERROR_IS_FATAL ANY)
else()
  # Tar preserves Unix executable permissions across artifact upload/download.
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar czf "${name}.tar.gz" "${name}"
    WORKING_DIRECTORY "${build}/dist" COMMAND_ERROR_IS_FATAL ANY)
endif()
