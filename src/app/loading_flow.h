#pragma once
#include "game/menu.h"
#include "game/original/LoadingScreen.h"
#include <string_view>

// Application half of the original loading screen (loading_screen_contracts): StartNewWorld's
// GS_LOADINGLEVEL shows the screen for one presented frame before the synchronous load (the
// original PreLoadWorld), and a world with a load-screen ID then waits in GS_WAITING, paused,
// for a key. Headless, so the menu traversal smoke and tests drive it without a GPU.
namespace OpenBlood2
{
    struct LoadingFlow
    {
        explicit LoadingFlow(const Resources& resources) : screen(resources) {}
        enum class Request : std::uint8_t
        {
            None,
            MenuWorld,  // A NewGame or Credits menu command.
            SaveLoad,   // A Load menu row (slots, Restart).
            QuickLoad,  // F6.
            LevelChange // The session's pending NEW_LEVEL.
        };
        LoadingScreen screen;
        Request request = Request::None; // Executed at the next frame boundary.
        MenuCommand command;             // MenuWorld and SaveLoad.
        bool waiting = false;            // GS_WAITING after the load.
        double clock = 0;                // UI time for the waiting prompt's pulse.
        std::uint64_t shown = 0, waits = 0, failures = 0;
        std::string error; // The last loading-screen failure (the load then proceeds without it).
        bool visible() const
        {
            return request != Request::None || waiting;
        }
        // StartNewWorld's GetWorldTitle and first DrawLoadingScreen. False keeps the request
        // immediate (the screen is cosmetic): the caller loads without it.
        bool begin(Request kind, std::string_view world, bool action, const MenuCommand& source = {})
        {
            if (!screen.begin(world, action, error))
            {
                ++failures;
                return false;
            }
            request = kind;
            command = source;
            waiting = false;
            ++shown;
            return true;
        }
        // After the deferred load: GS_WAITING when the world names a load screen ID (PauseGame).
        void loaded(bool success)
        {
            request = Request::None;
            waiting = success && screen.info().load_screen_id != 0;
            clock = 0;
            if (waiting)
                ++waits;
            else
                screen.end();
        }
        void finish()
        {
            request = Request::None;
            waiting = false;
            screen.end();
        }
    };
    // A world name as StartNewWorld received it, from a normalized "worlds/name.dat" path.
    inline std::string_view requested_world(std::string_view path)
    {
        if (path.size() > 7 && path.substr(0, 7) == "worlds/")
            path.remove_prefix(7);
        if (path.size() > 4 && path.substr(path.size() - 4) == ".dat")
            path.remove_suffix(4);
        return path;
    }
} // namespace OpenBlood2
