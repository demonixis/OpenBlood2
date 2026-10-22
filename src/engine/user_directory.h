#pragma once
#include "engine/config.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <filesystem>
#include <string>
#include <system_error>

namespace OpenBlood2
{
    // The per-user directory holding preferences.cfg, controls.cfg, saves/ and logs/:
    // SDL_GetPrefPath("OpenBlood2", "Blood2"), or BLOOD2_USER_DIR when that variable is set and
    // nonempty (CTest gives every test its own, so a run never touches the real directory).
    // Like SDL_GetPrefPath, the directory is created.
    inline bool user_directory(std::filesystem::path& output, std::string& error)
    {
        if (const char* override_dir = SDL_getenv("BLOOD2_USER_DIR"); override_dir && *override_dir)
        {
            std::error_code ec;
            auto path = std::filesystem::absolute(path_from_utf8(override_dir), ec);
            if (!ec)
                std::filesystem::create_directories(path, ec);
            if (ec)
            {
                error = "Cannot create BLOOD2_USER_DIR " + path_text(path) + ": " + ec.message();
                return false;
            }
            output = path;
            return true;
        }
        auto* path = SDL_GetPrefPath("OpenBlood2", "Blood2");
        if (!path)
        {
            error = SDL_GetError();
            return false;
        }
        output = path_from_utf8(path);
        SDL_free(path);
        return true;
    }
} // namespace OpenBlood2
