#include "game/save_slots.h"
#include "build_info.h"
#include "engine/atomic_file.h"
#include "engine/config.h"
#include "engine/save_objects.h"
#include "game/game_settings.h"
#include "game/server_adapter.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <utility>

// Slots, Quick and Restart (save_load_contracts lot 6): original LoadSave.cpp slot rules over
// self-contained container files, and the client-side halves of MenuSaveGame, MenuLoadGame and
// the level-entry autosave.
namespace OpenBlood2
{
    namespace
    {
        constexpr std::size_t quick_index = MAX_SAVESLOTS, current_index = MAX_SAVESLOTS + 1;
        bool valid_slot(int slot)
        {
            return slot == SLOT_CURRENT || slot == SLOT_QUICK || (slot >= 0 && slot < MAX_SAVESLOTS);
        }
        std::size_t index(int slot)
        {
            return slot == SLOT_CURRENT ? current_index : slot == SLOT_QUICK ? quick_index : std::size_t(slot);
        }
        // The kind a file of this slot must hold; a misplaced or renamed file is refused.
        bool kind_matches(int slot, const SaveMeta& meta)
        {
            return slot == SLOT_CURRENT ? meta.kind == SaveKind::LevelEntry
                   : slot == SLOT_QUICK ? meta.kind == SaveKind::Quick
                                        : meta.kind == SaveKind::Slot && meta.slot == slot;
        }
        std::string slot_label(int slot)
        {
            return slot == SLOT_CURRENT ? "The Restart save"
                   : slot == SLOT_QUICK ? "The quicksave"
                                        : "Slot " + std::to_string(slot + 1);
        }
        bool iequal(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() &&
                   std::equal(a.begin(), a.end(), b.begin(), [](char x, char y)
                              { return std::tolower(static_cast<unsigned char>(x)) ==
                                       std::tolower(static_cast<unsigned char>(y)); });
        }
        unsigned console_unsigned(GameSession& game, const char* name)
        {
            const float value = game.server.GetVarValueFloat(game.server.GetGameConVar(name));
            return value > 0 ? static_cast<unsigned>(value) : 0u;
        }
        // META shared by every kind: the world, its title, the console's game type and difficulty,
        // the player's character, the build and the simulated time.
        SaveMeta describe(GameSession& game, const GameSave& save, SaveKind kind, int slot, std::time_t now,
                          const std::string& version)
        {
            SaveMeta meta;
            meta.kind = kind;
            meta.slot = kind == SaveKind::Slot ? static_cast<std::uint8_t>(slot) : 0;
            meta.utc_seconds = now > 0 ? static_cast<std::uint64_t>(now) : 0;
            meta.world_path = save.world_path;
            meta.world_title = world_title(game.world.info, save.world_path).substr(0, save_format::max_world_title);
            meta.game_type = console_unsigned(game, "GAME_GAMETYPE");
            meta.difficulty = console_unsigned(game, "GAME_DIFFICULTY");
            const auto* player = game.server_shell.Player(game.local_client);
            meta.character = player ? player->m_nCharacter : 0;
            meta.version = version.empty() ? "unknown" : version.substr(0, save_format::max_version_text);
            meta.simulated_seconds = game.objects.time();
            // CSavedGameInfo::SetSlotName: the Current name (the world title), plus " [Mmm dd hh:mm:ss]"
            // for a slot or the quicksave; the level-entry snapshot keeps the bare title.
            meta.display_name = meta.world_title;
            if (kind != SaveKind::LevelEntry)
            {
                const auto suffix = " [" + save_timestamp(now) + "]";
                meta.display_name =
                    meta.display_name.substr(0, save_format::max_display_name - suffix.size()) + suffix;
            }
            if (meta.display_name.empty())
                meta.display_name = "Untitled";
            return meta;
        }
    } // namespace

    const char* save_build_version()
    {
        return build::version;
    }
    std::string save_slot_file(int slot)
    {
        if (slot == SLOT_CURRENT)
            return "current.ob2sav";
        if (slot == SLOT_QUICK)
            return "quick.ob2sav";
        if (slot >= 0 && slot < MAX_SAVESLOTS)
            return "slot" + std::to_string(slot) + ".ob2sav";
        return {};
    }
    bool save_directory(std::filesystem::path& output, std::string& error)
    {
        auto* path = SDL_GetPrefPath("OpenBlood2", "Blood2");
        if (!path)
        {
            error = SDL_GetError();
            return false;
        }
        output = path_from_utf8(path) / "saves" / "retail";
        SDL_free(path);
        return true;
    }
    std::string save_timestamp(std::time_t time)
    {
        std::tm local{};
#ifdef _WIN32
        if (localtime_s(&local, &time) != 0)
            return "??? ?? ??:??:??";
#else
        if (!localtime_r(&time, &local))
            return "??? ?? ??:??:??";
#endif
        static constexpr const char* months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        char text[32];
        // ctime's "%.3s%3d %.2d:%.2d:%.2d" after the weekday: the day is space-padded.
        std::snprintf(text, sizeof text, "%s%3d %02d:%02d:%02d", months[local.tm_mon % 12], local.tm_mday,
                      local.tm_hour, local.tm_min, local.tm_sec);
        return text;
    }
    std::string world_title(std::string_view info, std::string_view world_path)
    {
        // The engine's Parse: commands end at ';', tokens are separated by spaces and a quoted
        // token keeps its spaces. Every "worldname" match replaces the title, so the last wins.
        std::string title;
        std::vector<std::string> tokens;
        const auto finish = [&]
        {
            for (std::size_t i = 0; i + 1 < tokens.size(); ++i)
                if (iequal(tokens[i], "worldname"))
                    title = tokens[i + 1];
            tokens.clear();
        };
        std::string token;
        bool quoted = false, open = false;
        for (const char c : info)
        {
            if (quoted)
            {
                if (c == '"')
                    quoted = false;
                else
                    token += c;
                continue;
            }
            if (c == '"')
            {
                quoted = open = true;
                continue;
            }
            if (c == ';' || std::isspace(static_cast<unsigned char>(c)))
            {
                if (open)
                    tokens.push_back(std::exchange(token, {}));
                open = false;
                if (c == ';')
                    finish();
                continue;
            }
            token += c;
            open = true;
        }
        if (open)
            tokens.push_back(token);
        finish();
        if (title.empty())
        {
            // No title: the original uses the world file name it was given.
            const auto slash = world_path.find_last_of("/\\");
            auto name = world_path.substr(slash == std::string_view::npos ? 0 : slash + 1);
            if (const auto dot = name.rfind('.'); dot != std::string_view::npos)
                name = name.substr(0, dot);
            title = name;
        }
        return title;
    }

    bool save_container(const GameSave& save, const SaveMeta& meta, SaveContainer& out, std::string& error)
    {
        const auto& image = save.image;
        if (image.param != 0 || image.flags != (SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS) ||
            !image.console_saved)
        {
            error = "A slot file holds only a full save with its console";
            return false;
        }
        if (save.fingerprint.classes != image.classes)
        {
            error = "The fingerprint's class schema differs from the saved records";
            return false;
        }
        SaveContainer c;
        c.meta = meta;
        c.fingerprint = save.fingerprint;
        c.console = image.console;
        c.clock = image.clock;
        c.objects = image.objects;
        c.links = image.links;
        c.attachments = image.attachments;
        out = std::move(c);
        return true;
    }
    bool game_save_from(const SaveContainer& c, GameSave& out, std::string& error)
    {
        std::vector<SaveEngineRecord> records;
        SaveError decode;
        if (!decode_save_records(c.objects, records, decode))
        {
            error = "Saved objects: " + decode.text;
            return false;
        }
        GameSave save;
        save.world_path = c.meta.world_path;
        save.fingerprint = c.fingerprint;
        save.image.param = 0;
        save.image.flags = SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS;
        save.image.console_saved = true;
        save.image.console = c.console;
        save.image.classes = c.fingerprint.classes;
        save.image.clock = c.clock;
        save.image.objects = c.objects;
        save.image.links = c.links;
        save.image.attachments = c.attachments;
        save.image.records = static_cast<std::uint32_t>(records.size());
        out = std::move(save);
        return true;
    }

    SaveSlotInfo& SaveSlots::entry(int slot)
    {
        return infos_[index(slot)];
    }
    const SaveSlotInfo& SaveSlots::info(int slot) const
    {
        static const SaveSlotInfo empty;
        return valid_slot(slot) ? infos_[index(slot)] : empty;
    }
    void SaveSlots::scan(int slot)
    {
        auto& info = entry(slot);
        info = {};
        std::error_code ec;
        const auto file = path(slot);
        if (!std::filesystem::exists(file, ec) && !ec)
            return;
        if (!read_save_meta_file(file, info.meta, info.error))
            info.state = SaveSlotInfo::State::Unreadable;
        else if (!kind_matches(slot, info.meta))
        {
            info.state = SaveSlotInfo::State::Unreadable;
            info.error = {SaveFault::Format, "The file holds a different kind of save"};
        }
        else
            info.state = SaveSlotInfo::State::Ready;
    }
    void SaveSlots::refresh()
    {
        ++report.scans;
        if (!enabled())
            return;
        scan(SLOT_CURRENT);
        scan(SLOT_QUICK);
        for (int slot = 0; slot < MAX_SAVESLOTS; ++slot)
            scan(slot);
    }
    MenuSaveSlots SaveSlots::menu_rows() const
    {
        MenuSaveSlots rows;
        if (!enabled())
        {
            rows.unavailable = unavailable;
            return rows;
        }
        const auto row = [&](int slot)
        {
            const auto& slot_info = info(slot);
            MenuSaveRow out;
            out.loadable = slot_info.state != SaveSlotInfo::State::Empty;
            if (slot_info.state == SaveSlotInfo::State::Ready)
                out.name = slot_info.meta.display_name;
            else if (slot_info.state == SaveSlotInfo::State::Unreadable)
                out.name = std::string("<UNREADABLE: ") + save_fault_name(slot_info.error.fault) + ">";
            return out;
        };
        rows.restart = row(SLOT_CURRENT);
        rows.quick = row(SLOT_QUICK);
        for (int slot = 0; slot < MAX_SAVESLOTS; ++slot)
            rows.slots[std::size_t(slot)] = row(slot);
        return rows;
    }

    bool SaveSlots::save(GameSession& game, int slot, std::string& error, std::time_t now)
    {
        error.clear();
        report.refused_classes.clear();
        report.last_action = "Save " + (valid_slot(slot) ? save_slot_file(slot) : std::string("?"));
        const auto refused = [&](std::string why)
        {
            error = std::move(why);
            report.last_error = error;
            ++report.save_refusals;
            return false;
        };
        if (!enabled())
            return refused(unavailable);
        if (slot != SLOT_QUICK && (slot < 0 || slot >= MAX_SAVESLOTS))
            return refused("Unknown save slot");
        GameSave save;
        std::string why;
        if (!game.save_game(save, why))
        {
            // SaveObjects refuses listed classes without save support by name (save/load lot 4).
            const auto& classes = game.server.last_save.refused;
            if (why.starts_with("SaveObjects: ") && !classes.empty())
            {
                report.refused_classes = classes;
                std::string names;
                for (const auto& name : classes)
                    names += (names.empty() ? "" : ", ") + name;
                return refused("This game cannot be saved yet. These classes have no save support: " + names);
            }
            return refused("Save refused: " + why);
        }
        SaveContainer container;
        const auto meta =
            describe(game, save, slot == SLOT_QUICK ? SaveKind::Quick : SaveKind::Slot, slot, now, version);
        if (!save_container(save, meta, container, why))
            return refused(why);
        container.level_entry = level_entry_;
        std::vector<std::uint8_t> bytes;
        SaveError written;
        if (!encode_save(container, bytes, written))
            return refused(std::string("The save exceeds the file format (") + save_fault_name(written.fault) +
                           "): " + written.text);
        std::string text;
        if (!write_file_atomic(path(slot), bytes, "save", text, {.remove_stale_temporaries = true}))
            return refused("Cannot write " + save_slot_file(slot) + ": " + text);
        entry(slot) = {SaveSlotInfo::State::Ready, meta, {}};
        report.last_bytes = bytes.size();
        report.last_error.clear();
        ++report.saves;
        if (slot == SLOT_QUICK)
            ++report.quick_saves;
        return true;
    }

    bool SaveSlots::remove_level_entry(std::string& error)
    {
        level_entry_.reset();
        entry(SLOT_CURRENT) = {};
        if (!enabled())
            return true;
        std::error_code ec;
        std::filesystem::remove(path(SLOT_CURRENT), ec);
        if (ec)
        {
            error = "Cannot remove " + save_slot_file(SLOT_CURRENT) + ": " + ec.message();
            report.file_error = error;
            return false;
        }
        return true;
    }
    bool SaveSlots::clear_level_entry(std::string& error)
    {
        error.clear();
        ++report.level_entry_clears;
        report.level_entry_error.clear();
        return remove_level_entry(error);
    }
    bool SaveSlots::capture_level_entry(GameSession& game, std::string& error, std::time_t now)
    {
        error.clear();
        // Original CMSG_AUTOSAVE: the server answers SMSG_DOAUTOSAVE only while GAME_DOAUTOSAVE is
        // set, which LoadWorld clears for both restore types.
        if (!enabled() || game.server.GetVarValueFloat(game.server.GetGameConVar("GAME_DOAUTOSAVE")) == 0)
        {
            ++report.level_entry_skips;
            return true;
        }
        const auto refused = [&](std::string why)
        {
            std::string removal;
            remove_level_entry(removal);
            error = std::move(why);
            if (!removal.empty())
                error += "; " + removal;
            report.level_entry_error = error;
            ++report.level_entry_refusals;
            return false;
        };
        // The snapshot belongs to the frame boundary after initialization: deferred removals that
        // initialization left are finished first, as that boundary would.
        if (game.objects.pending_count() && !game.objects.busy() && !game.objects.finish_frame())
            return refused("Level-entry snapshot refused: the entry frame cannot be finished");
        GameSave save;
        std::string why;
        if (!game.save_game(save, why, SAVETYPE_AUTO))
            return refused("Level-entry snapshot refused: " + why);
        SaveContainer container;
        const auto meta = describe(game, save, SaveKind::LevelEntry, 0, now, version);
        if (!save_container(save, meta, container, why))
            return refused(why);
        std::vector<std::uint8_t> bytes;
        SaveError encoded;
        if (!encode_save(container, bytes, encoded) || bytes.size() > save_format::max_level_entry)
            return refused("The level-entry snapshot exceeds its bound: " + encoded.text);
        // Kept in memory even if the file write fails, so later slots still embed it.
        level_entry_ = bytes;
        entry(SLOT_CURRENT) = {SaveSlotInfo::State::Ready, meta, {}};
        ++report.level_entries;
        report.level_entry_error.clear();
        std::string text;
        if (!write_file_atomic(path(SLOT_CURRENT), bytes, "save", text, {.remove_stale_temporaries = true}))
        {
            error = report.file_error = "Cannot write " + save_slot_file(SLOT_CURRENT) + ": " + text;
            return false;
        }
        return true;
    }

    SaveSlots::LoadResult SaveSlots::load(std::unique_ptr<GameSession>& game, int slot, std::string& error)
    {
        error.clear();
        report.last_action = "Load " + (valid_slot(slot) ? save_slot_file(slot) : std::string("?"));
        const auto refused = [&](std::string why)
        {
            error = std::move(why);
            report.last_error = error;
            ++report.load_refusals;
            return LoadResult::Refused;
        };
        if (!enabled())
            return refused(unavailable);
        if (!valid_slot(slot) || !game)
            return refused("Unknown save slot");
        const auto file = path(slot);
        std::error_code ec;
        if (!std::filesystem::exists(file, ec))
        {
            if (ec)
                return refused("Cannot inspect " + save_slot_file(slot) + ": " + ec.message());
            entry(slot) = {};
            ++report.empty_loads;
            return LoadResult::Empty;
        }
        // 1. Read and validate the whole file, including its AUTO, before anything changes.
        std::vector<std::uint8_t> bytes;
        std::string text;
        if (!read_file_bounded(file, save_format::max_file, bytes, text))
            return refused("Cannot read " + save_slot_file(slot) + ": " + text);
        SaveContainer container;
        SaveError decode;
        if (!decode_save(bytes, container, decode))
        {
            entry(slot) = {SaveSlotInfo::State::Unreadable, {}, decode};
            return refused(slot_label(slot) + " is unreadable (" + save_fault_name(decode.fault) + "): " + decode.text);
        }
        if (!kind_matches(slot, container.meta))
            return refused(slot_label(slot) + " holds a different kind of save");
        GameSave save;
        if (!game_save_from(container, save, text))
            return refused(slot_label(slot) + ": " + text);
        // 2-12. The candidate transaction: any failure leaves the current session untouched.
        if (!GameSession::load_game(game, save, text))
        {
            std::string why = slot_label(slot) + " cannot be loaded (" + game->game_load.stage + "): " + text;
            if (game->game_load.stage == "fingerprint" && container.meta.version != version)
                why += " The save was made by OpenBlood2 " + container.meta.version + "; this is " +
                       (version.empty() ? "unknown" : version) + ".";
            return refused(why);
        }
        // Promoted. The Restart source follows the loaded file: Restart keeps its own snapshot, a
        // slot or the quicksave brings its AUTO, or none (so Restart cannot reach another level).
        entry(slot) = {SaveSlotInfo::State::Ready, container.meta, {}};
        report.file_error.clear();
        if (slot == SLOT_CURRENT)
            level_entry_ = std::move(bytes);
        else if (container.level_entry)
        {
            level_entry_ = container.level_entry;
            SaveMeta meta;
            SaveError read;
            if (read_save_meta(*level_entry_, level_entry_->size(), meta, read))
                entry(SLOT_CURRENT) = {SaveSlotInfo::State::Ready, meta, {}};
            if (!write_file_atomic(path(SLOT_CURRENT), *level_entry_, "save", text, {.remove_stale_temporaries = true}))
                report.file_error = "Cannot write " + save_slot_file(SLOT_CURRENT) + ": " + text;
        }
        else
            remove_level_entry(text);
        report.last_error.clear();
        ++report.loads;
        if (slot == SLOT_QUICK)
            ++report.quick_loads;
        if (slot == SLOT_CURRENT)
            ++report.restarts;
        return LoadResult::Loaded;
    }
} // namespace OpenBlood2
