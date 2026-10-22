#pragma once
#include "engine/config.h"
#include <optional>

// Game data setup (setup_screen_contracts): which Blood II installation and edition to play and
// its data-only REZ mods, kept in setup.cfg in the per-user directory. Headless, so tests drive
// persistence, detection and validation without a window.
namespace OpenBlood2
{
    enum class Edition : std::uint8_t
    {
        Retail,
        Demo
    };
    struct Installation
    {
        Edition edition = Edition::Retail;
        std::filesystem::path directory; // Absolute; holds BLOOD2.REZ (retail) or B2DEMO.REZ (demo).
        bool operator==(const Installation&) const = default;
    };
    struct SetupMod
    {
        Installation installation; // The installation the mod belongs to.
        std::filesystem::path archive;
        bool enabled = false;
        bool operator==(const SetupMod&) const = default;
    };
    struct SetupState
    {
        std::optional<Installation> selected;
        std::vector<Installation> added; // Folders the player chose; detected ones are rescanned.
        std::vector<SetupMod> mods;      // In load order (later entries take priority).
        bool operator==(const SetupState&) const = default;
    };
    inline constexpr std::size_t max_setup_installations = 32, max_setup_mods = 24;
    // UTF-8 without control characters, at most `limit` bytes.
    bool valid_setup_text(std::string_view text, std::size_t limit);
    // user_directory()/setup.cfg.
    bool setup_path(std::filesystem::path& output, std::string& error);
    // A missing file is the empty state; malformed, unknown or duplicate keys fail.
    bool load_setup(const std::filesystem::path&, SetupState&, std::string& error);
    bool save_setup(const std::filesystem::path&, const SetupState&, std::string& error);

    // Installations in one folder: retail when it holds BLOOD2.REZ, demo when it holds B2DEMO.REZ
    // (case-insensitive; a folder with both lists both).
    std::vector<Installation> folder_installations(const std::filesystem::path& directory);
    // Where this platform looks without being told: downloaded demos (user directory content/),
    // the executable's folder (desktop), Documents and its folders (iOS) and /sdcard/openblood2
    // with its folders (Android).
    std::vector<std::filesystem::path> setup_search_folders(const std::filesystem::path& user_directory);
    // The folder downloaded demos are published in: user directory content/.
    std::filesystem::path setup_content_folder(const std::filesystem::path& user_directory);

    struct SetupEntry
    {
        Installation installation;
        bool added = false; // Chosen by the player (removable), else detected.
        DataStatus status;
        std::string error; // inspect_data's refusal; empty when playable.
    };
    // Added, `extra` (the running game's) and detected installations, deduplicated, each checked
    // with inspect_data.
    std::vector<SetupEntry> scan_setup(const SetupState&, const std::filesystem::path& user_directory,
                                       const std::vector<Installation>& extra = {});
    // The game configuration of an installation and its enabled mods, in order.
    Config setup_config(const SetupState&, const Installation&);
    // The selected installation when it is still playable.
    bool setup_ready(const SetupState&, Config& config, std::string& error);
    // Android: whether the shared folders are readable (all-files access); true elsewhere.
    bool storage_access();
    // Android: opens the system page that grants all-files access; nothing elsewhere.
    void request_storage_access();
} // namespace OpenBlood2
