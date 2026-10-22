# The setup screen's demo installation: miniz reads the ZIP on every platform; libcurl carries
# HTTPS on Linux (OpenSSL) and Windows (Schannel). Apple and Android use their system HTTPS stacks.
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
# miniz 3.1.1; exact archive verified at integration.
FetchContent_Declare(content_miniz
    URL https://codeload.github.com/richgel999/miniz/tar.gz/d10b03cc73475af673df40f06e5cefd1d5f940d9
    URL_HASH SHA256=f7a9a89e63300e66c8fa324f8e849e0f348153007a5033f53302632d229e6c80
    DOWNLOAD_NAME miniz-d10b03cc.tar.gz
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.cache/downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(content_miniz)

if(APPLE OR ANDROID)
    set(BLOOD2_CURL OFF)
else()
    set(BLOOD2_CURL ON)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
    set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
    set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    if(WIN32)
        set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
        set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    else()
        find_package(OpenSSL REQUIRED)
        set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
    endif()
    set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH OFF CACHE BOOL "" FORCE)
    set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
    set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
    set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
    set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
    set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
    set(HTTP_ONLY ON CACHE BOOL "" FORCE)
    # curl 8.20.0; exact archive verified at integration.
    FetchContent_Declare(content_curl
        URL https://codeload.github.com/curl/curl/tar.gz/6e3f8dc1f173b47de9a68516ce4b95bf25598c2f
        URL_HASH SHA256=1bbcbe03719ffd2293b8c71903ef977efcd1f7133255264e4c42171e50ccae68
        DOWNLOAD_NAME curl-6e3f8dc1.tar.gz
        DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.cache/downloads"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM EXCLUDE_FROM_ALL)
    # Do not let curl add its own tests to the game's CTest tree.
    set(content_testing "${BUILD_TESTING}")
    set(BUILD_TESTING OFF)
    FetchContent_MakeAvailable(content_curl)
    set(BUILD_TESTING "${content_testing}")
endif()
if(BLOOD2_SANITIZERS)
    blood2_project_options(miniz)
endif()

# Third-party notices beside the game (packaging copies this folder).
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/licenses")
set(blood2_notices "${content_miniz_SOURCE_DIR}/LICENSE|miniz.txt" "${imgui_source_SOURCE_DIR}/LICENSE.txt|imgui.txt"
    "${sdl3_SOURCE_DIR}/LICENSE.txt|SDL.txt" "${spdlog_SOURCE_DIR}/LICENSE|spdlog.txt"
    "${PROJECT_SOURCE_DIR}/licenses/DroidSans-Apache-2.0.txt|DroidSans-Apache-2.0.txt")
if(BLOOD2_CURL)
    list(APPEND blood2_notices "${content_curl_SOURCE_DIR}/COPYING|curl.txt")
endif()
foreach(pair IN LISTS blood2_notices)
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 source)
    list(GET parts 1 name)
    configure_file("${source}" "${PROJECT_BINARY_DIR}/licenses/${name}" COPYONLY)
endforeach()
