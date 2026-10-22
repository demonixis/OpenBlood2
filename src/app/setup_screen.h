#pragma once
#include "app/demo_install.h"
#include "app/game_setup.h"
#include <SDL3/SDL_video.h>
#include <memory>

struct ImFont;

// The first screen of an unconfigured game and Options > SETUP (setup_screen_contracts): pick the
// Blood II installation and edition, download the free demo, add folders and data-only mods.
// Replacement UI (Dear ImGui), never shown by smokes or direct command-line configurations.
namespace OpenBlood2
{
    class SetupScreen
    {
      public:
        SetupScreen();
        ~SetupScreen();
        SetupScreen(const SetupScreen&) = delete;
        SetupScreen& operator=(const SetupScreen&) = delete;
        // Reads setup.cfg and scans. `running` (opened from the menu) is the game's configuration:
        // it is listed, preselected when nothing else is, and Back returns to it.
        bool open(const Config* running, std::string& error);
        enum class Result
        {
            None,
            Play, // config() is the chosen installation; setup.cfg is saved.
            Back, // Unchanged, back to the game that opened the screen.
            Quit
        };
        // One frame of the full-window screen. `window` parents native dialogs (desktop).
        Result draw(SDL_Window* window);
        const Config& config() const
        {
            return config_;
        }
        // A refusal from outside (the game could not mount the selection) shown on the screen.
        std::string message;
        ImFont* font = nullptr; // Proportional UI font; the default font when absent.
        bool quit_available = true;

      private:
        struct Dialog;
        std::unique_ptr<Dialog> dialog_;
        enum class Pick
        {
            Folder,
            Mod,
            Zip
        };
        Pick pick_ = Pick::Folder;
        SetupState state_, initial_;
        std::filesystem::path file_, user_;
        std::vector<SetupEntry> entries_;
        std::vector<Installation> running_;
        DemoInstaller installer_;
        Config config_;
        bool from_game_ = false;
        void rescan();
        bool save(std::string& error);
        void add_mods(const Installation&);
        void open_dialog(SDL_Window*, Pick);
        void poll();
    };
} // namespace OpenBlood2
