# Launcher-only HTTP/ZIP support. Does not change the game's SDL or link graph.
include(FetchContent)
find_package(OpenSSL REQUIRED)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH OFF CACHE BOOL "" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY ON CACHE BOOL "" FORCE)
# curl 8.20.0 and miniz 3.1.1; exact archives verified at integration.
FetchContent_Declare(launcher_curl
    URL https://codeload.github.com/curl/curl/tar.gz/6e3f8dc1f173b47de9a68516ce4b95bf25598c2f
    URL_HASH SHA256=1bbcbe03719ffd2293b8c71903ef977efcd1f7133255264e4c42171e50ccae68
    DOWNLOAD_NAME curl-6e3f8dc1.tar.gz
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.cache/downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM EXCLUDE_FROM_ALL)
# Do not let curl add its own tests to the launcher's CTest tree.
set(launcher_testing "${BUILD_TESTING}")
set(BUILD_TESTING OFF)
FetchContent_MakeAvailable(launcher_curl)
set(BUILD_TESTING "${launcher_testing}")
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(launcher_miniz
    URL https://codeload.github.com/richgel999/miniz/tar.gz/d10b03cc73475af673df40f06e5cefd1d5f940d9
    URL_HASH SHA256=f7a9a89e63300e66c8fa324f8e849e0f348153007a5033f53302632d229e6c80
    DOWNLOAD_NAME miniz-d10b03cc.tar.gz
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/.cache/downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE SYSTEM EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(launcher_miniz)
