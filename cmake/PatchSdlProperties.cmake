# Local one-character fix for pinned SDL 6defbc0129b5ba90bab3f6c67bbc1e9329e24ff1.
# SDL_SetupPropertyAssignment receives an output pointer: validate the lookup
# result, not that output argument. A late Wayland HDR callback during window
# destruction exposed this null dereference. Reassess when updating SDL.
set(path "${SDL_SOURCE_DIR}/src/SDL_properties.c")
file(SHA256 "${path}" hash)
set(original "67cc7103a51b067a767a2fa8c78154f1cd89b46809c045ded0db1651876171d9")
set(patched "b4f13bce3c6f5aa33f96ad5b23fd94f701a39e7618d8f6111be8489f5f4e995f")
if(hash STREQUAL patched)
    return()
endif()
if(NOT hash STREQUAL original)
    message(FATAL_ERROR "SDL property source changed; review the local stale-ID fix")
endif()
file(READ "${path}" source)
string(REPLACE
    "CHECK_PARAM(!properties) {\n        return SDL_InvalidParamError(\"props\");\n    }\n\n    SDL_LockMutex((*properties)->lock);"
    "CHECK_PARAM(!*properties) {\n        return SDL_InvalidParamError(\"props\");\n    }\n\n    SDL_LockMutex((*properties)->lock);"
    source "${source}")
file(WRITE "${path}" "${source}")
file(SHA256 "${path}" hash)
if(NOT hash STREQUAL patched)
    message(FATAL_ERROR "SDL property fix did not produce the expected source")
endif()
