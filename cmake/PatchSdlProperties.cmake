# Local one-character fixes for pinned SDL 6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1.
# SDL_SetupPropertyAssignment and SDL_SetupPropertyQuery receive an output pointer:
# validate the lookup result, not that output argument. A late Wayland HDR callback
# during window destruction exposed the assignment null dereference. Metal debug
# devices query an uninitialized sampler property ID during blit setup, exposing
# the same defect in queries. Upstream main still has both; reassess when updating SDL.
set(path "${SDL_SOURCE_DIR}/src/SDL_properties.c")
file(SHA256 "${path}" hash)
set(original "67cc7103a51b067a767a2fa8c78154f1cd89b46809c045ded0db1651876171d9")
set(assignment_only "b4f13bce3c6f5aa33f96ad5b23fd94f701a39e7618d8f6111be8489f5f4e995f")
set(patched "811c3da3a1c8c12d2357524c92d56cc15a2185c5708abb7069fa5bbf2f59febe")
if(hash STREQUAL patched)
    return()
endif()
if(NOT hash STREQUAL original AND NOT hash STREQUAL assignment_only)
    message(FATAL_ERROR "SDL property source changed; review the local stale-ID fixes")
endif()
file(READ "${path}" source)
if(hash STREQUAL original)
    string(REPLACE
        "CHECK_PARAM(!properties) {\n        return SDL_InvalidParamError(\"props\");\n    }\n\n    SDL_LockMutex((*properties)->lock);"
        "CHECK_PARAM(!*properties) {\n        return SDL_InvalidParamError(\"props\");\n    }\n\n    SDL_LockMutex((*properties)->lock);"
        source "${source}")
endif()
string(REPLACE
    "(const void **)properties);\n    CHECK_PARAM(!properties) {\n        return false;\n    }\n\n    SDL_LockMutex((*properties)->lock);\n    return true;"
    "(const void **)properties);\n    CHECK_PARAM(!*properties) {\n        return false;\n    }\n\n    SDL_LockMutex((*properties)->lock);\n    return true;"
    source "${source}")
# Keep the pinned bytes identical on Windows, Linux and macOS. file(WRITE)
# uses native newlines on Windows; the exact output hash remains checked below.
file(CONFIGURE OUTPUT "${path}" CONTENT "${source}" @ONLY NEWLINE_STYLE LF)
file(SHA256 "${path}" hash)
if(NOT hash STREQUAL patched)
    message(FATAL_ERROR "SDL property fixes did not produce the expected source")
endif()
