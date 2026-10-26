#pragma once
#include "engine/save_file.h"
#include "game/menu.h"
#include <array>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Slot, Quick and Restart files (save_load_contracts lot 6). Each original Save\SaveN,
// Save\Quick and Save\Current directory becomes one self-contained container (D1):
// slot0-slot7, quick and current (the level-entry snapshot Restart reads), all .ob2sav,
// under user_directory()/saves/retail/ (decision 1; saves/nightmares/ for that edition). The menu
// reads only the header and META of each file.
namespace OpenBlood2
{
    class GameSession;
    struct GameSave;

    // This build's OpenBlood2 version, written in META (informational; payload versions decide).
    const char* save_build_version();
    // Original LoadSave.h slot ids.
    inline constexpr int SLOT_CURRENT = -2, SLOT_QUICK = -1, MAX_SAVESLOTS = 8;
    // "slot0.ob2sav" .. "slot7.ob2sav", "quick.ob2sav", "current.ob2sav"; empty for any other id.
    std::string save_slot_file(int slot);
    // user_directory()/saves/retail (saves/nightmares for the Nightmares edition):
    // SDL_GetPrefPath("OpenBlood2", "Blood2") or BLOOD2_USER_DIR.
    bool save_directory(std::filesystem::path& output, std::string& error, bool nightmares = false);
    // ctime(&t) + 4, 15 characters ("Oct 20 14:05:09"), in local time: the original slot suffix.
    std::string save_timestamp(std::time_t);
    // Original GetWorldTitle: the "worldname" token of the DAT world info string, else the
    // world's file name without its directory and extension.
    std::string world_title(std::string_view info, std::string_view world_path);

    // META, FPRT and the image of a GameSave in one container, and back. The image must be a
    // full save (dwParam 0, console and portals); the reverse conversion rebuilds that image.
    bool save_container(const GameSave&, const SaveMeta&, SaveContainer&, std::string& error);
    bool game_save_from(const SaveContainer&, GameSave&, std::string& error);

    struct SaveSlotInfo
    {
        enum class State : std::uint8_t
        {
            Empty,
            Ready,
            Unreadable
        } state = State::Empty;
        SaveMeta meta;
        SaveError error; // Unreadable: the fault and its text.
    };
    // Inspection counters and the last outcome; never saved and never a load input.
    struct SaveSlotsReport
    {
        std::uint64_t scans = 0, saves = 0, save_refusals = 0, quick_saves = 0;
        std::uint64_t loads = 0, load_refusals = 0, quick_loads = 0, restarts = 0, empty_loads = 0;
        std::uint64_t level_entries = 0, level_entry_refusals = 0, level_entry_skips = 0, level_entry_clears = 0;
        std::string last_action, last_error, level_entry_error, file_error;
        std::vector<std::string> refused_classes; // The last save refused by class, "Class (support)".
        std::uint64_t last_bytes = 0;
    };

    class SaveSlots
    {
      public:
        SaveSlots() = default; // Disabled: every operation is refused with `unavailable`.
        explicit SaveSlots(std::filesystem::path directory) : directory_(std::move(directory)) {}
        bool enabled() const
        {
            return !directory_.empty();
        }
        const std::filesystem::path& directory() const
        {
            return directory_;
        }
        std::filesystem::path path(int slot) const
        {
            return directory_ / save_slot_file(slot);
        }
        std::string unavailable = "Saved games are unavailable."; // Shown while disabled.
        std::string version = save_build_version();               // META version text (1-64 bytes).
        // Reads the header and META of every file (original LoadInfo).
        void refresh();
        const SaveSlotInfo& info(int slot) const;
        MenuSaveSlots menu_rows() const;
        // The in-memory level-entry snapshot (complete container bytes), embedded as AUTO.
        const std::optional<std::vector<std::uint8_t>>& level_entry() const
        {
            return level_entry_;
        }

        // MenuSaveGame(slot) for 0..7 or SLOT_QUICK: GameSession::save_game, then the slot file
        // with META, FPRT and the level-entry snapshot as AUTO, written atomically. A refusal
        // keeps the previous file and lists unsupported classes by name.
        bool save(GameSession&, int slot, std::string& error, std::time_t now = std::time(nullptr));
        // The original CMSG_AUTOSAVE/SMSG_DOAUTOSAVE round trip after a NEW_GAME or NEW_LEVEL
        // entry: only while GAME_DOAUTOSAVE is set (never after a restore). Kept in memory and in
        // current.ob2sav; a refused snapshot removes both, so Restart never returns to another
        // level. Returns false only for a refusal (`error`); a skip returns true.
        bool capture_level_entry(GameSession&, std::string& error, std::time_t now = std::time(nullptr));
        // MenuNewGame's ClearSaveSlot(SLOT_CURRENT).
        bool clear_level_entry(std::string& error);
        enum class LoadResult : std::uint8_t
        {
            Loaded,
            Empty, // No file: the original returns FALSE silently.
            Refused
        };
        // MenuLoadGame(slot) for SLOT_CURRENT (Restart), SLOT_QUICK or 0..7: reads and validates
        // the whole file, then GameSession::load_game. Only after promotion does the level-entry
        // snapshot follow the loaded file (its AUTO, or the Restart file itself).
        LoadResult load(std::unique_ptr<GameSession>&, int slot, std::string& error);
        SaveSlotsReport report;

      private:
        std::filesystem::path directory_;
        std::array<SaveSlotInfo, MAX_SAVESLOTS + 2> infos_; // Slots, then quick, then current.
        std::optional<std::vector<std::uint8_t>> level_entry_;
        SaveSlotInfo& entry(int slot);
        void scan(int slot);
        bool remove_level_entry(std::string& error);
    };
} // namespace OpenBlood2
