#include "app/save_flow.h"
#include "engine/atomic_file.h"
#include "engine/save_objects.h"
#include "game/game_settings.h"
#include "game/original/MenuIds.h"
#include "save_world_fixture.h"
#include <fstream>
#include <iostream>
#include "scratch_dir.h"

// Slots, Quick and Restart (save_load_contracts lot 6): slot naming, the META quick read, real
// files in a temporary preference directory, corrupt and mismatch refusals, F5/F6, the demo
// refusal, the level-entry snapshot, UI/input reset after a load and a menu traversal.
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    std::filesystem::path root;
    using save_world::object;
    using save_world::text;
    using save_world::vector;

    bool write_fixture()
    {
        return save_world::write_archive(
                   root / "player.rez",
                   {{"player", save_world::dat({object("WorldProperties", {text("Name", "WorldProperties0")}),
                                                object("GameStartPoint",
                                                       {text("Name", "start"), vector("Pos", {0, 10, 0})})})},
                    {"next", save_world::dat({object("GameStartPoint",
                                                     {text("Name", "arrival"), vector("Pos", {40, 10, 40})})})}}) &&
               save_world::write_archive(root / "mod.rez", {{"modlevel", save_world::dat({})}});
    }
    CPlayerObj* player(GameSession& g)
    {
        return g.server_shell.Player(g.local_client);
    }
    bool contains(const std::string& text, std::string_view part)
    {
        if (text.find(part) != std::string::npos)
            return true;
        std::cerr << "missing \"" << part << "\" in: " << text << '\n';
        return false;
    }
    // A fresh game in the synthetic world: NEW_GAME, so GAME_DOAUTOSAVE is 1.
    bool setup(std::unique_ptr<GameSession>& game, bool mod = false)
    {
        game = std::make_unique<GameSession>();
        std::string error;
        CHECK(game->initialize() && game->resources.mount(root / "player.rez", error));
        if (mod)
            CHECK(game->resources.mount(root / "mod.rez", error, MountRole::Mod));
        CHECK(game->request_world("player", error));
        if (!game->run_world(error))
        {
            std::cerr << "run_world: " << error << '\n';
            return false;
        }
        return player(*game) != nullptr;
    }
    // An empty bound session with the same mounts, as at the menu before any world.
    bool empty_session(std::unique_ptr<GameSession>& game, bool mod = false)
    {
        game = std::make_unique<GameSession>();
        std::string error;
        CHECK(game->initialize() && game->resources.mount(root / "player.rez", error));
        if (mod)
            CHECK(game->resources.mount(root / "mod.rez", error, MountRole::Mod));
        return true;
    }
    std::filesystem::path fresh(std::string_view name)
    {
        const auto dir = root / "prefs" / name / "saves" / "retail";
        OpenBlood2::Testing::remove_scratch_dir(dir);
        return dir;
    }
    float& health(GameSession& g)
    {
        return player(g)->m_damage.m_fHitPoints;
    }
    SDL_Event key(SDL_Scancode code, bool down = true)
    {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.windowID = 1;
        e.key.scancode = code;
        return e;
    }

    bool naming()
    {
        CHECK(save_slot_file(0) == "slot0.ob2sav" && save_slot_file(7) == "slot7.ob2sav");
        CHECK(save_slot_file(SLOT_QUICK) == "quick.ob2sav" && save_slot_file(SLOT_CURRENT) == "current.ob2sav");
        CHECK(save_slot_file(8).empty() && save_slot_file(-3).empty());
        std::filesystem::path dir;
        std::string error;
        CHECK(save_directory(dir, error) && dir.filename() == "retail" && dir.parent_path().filename() == "saves");
        CHECK(path_text(dir.parent_path().parent_path()).find("Blood2") != std::string::npos);
        SaveSlots slots(fresh("naming"));
        CHECK(slots.path(3) == slots.directory() / "slot3.ob2sav");
        // ctime + 4: "Mmm dd hh:mm:ss" with a space-padded day.
        std::tm when{};
        when.tm_year = 126;
        when.tm_mon = 9;
        when.tm_mday = 2;
        when.tm_hour = 7;
        when.tm_min = 5;
        when.tm_sec = 9;
        when.tm_isdst = -1;
        const auto stamp = save_timestamp(std::mktime(&when));
        CHECK(stamp == "Oct  2 07:05:09" && stamp.size() == 15);
        // Original GetWorldTitle: the last "worldname" token, quotes group, else the file name.
        CHECK(world_title("worldname \"Cabalco Transit System\"; loadscreen a.pcx", "worlds/01_subwaya.dat") ==
              "Cabalco Transit System");
        CHECK(world_title("WorldName first;worldname second", "worlds/x.dat") == "second");
        CHECK(world_title("ambient 1 1 1", "worlds/01_subwaya.dat") == "01_subwaya");
        CHECK(world_title("worldname", "worlds/sub/02_x.dat") == "02_x");
        return true;
    }

    bool meta()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        SaveSlots slots(fresh("meta"));
        std::string error;
        std::tm when{};
        when.tm_year = 126;
        when.tm_mon = 0;
        when.tm_mday = 15;
        when.tm_hour = 23;
        when.tm_min = 4;
        when.tm_sec = 5;
        when.tm_isdst = -1;
        const auto now = std::mktime(&when);
        CHECK(slots.save(*game, 5, error, now));
        CHECK(slots.save(*game, SLOT_QUICK, error, now));
        CHECK(std::filesystem::exists(slots.path(5)) && std::filesystem::exists(slots.path(SLOT_QUICK)));
        // The quick read touches only the header and META.
        SaveMeta read;
        SaveError fault;
        CHECK(read_save_meta_file(slots.path(5), read, fault));
        CHECK(read.kind == SaveKind::Slot && read.slot == 5 && read.world_path == "worlds/player.dat");
        CHECK(read.world_title == "player" && read.display_name == "player [Jan 15 23:04:05]");
        CHECK(read.game_type == GAMETYPE_SINGLE && read.difficulty == 1 && read.character == 0);
        CHECK(read.utc_seconds == std::uint64_t(now) && read.version == save_build_version());
        CHECK(read.simulated_seconds == game->objects.time());
        // A new listing reads the same names; empty slots keep the original "<EMPTY SLOT>".
        SaveSlots listing(slots.directory());
        listing.refresh();
        CHECK(listing.info(5).state == SaveSlotInfo::State::Ready && listing.info(5).meta == read);
        CHECK(listing.info(SLOT_QUICK).meta.kind == SaveKind::Quick);
        const auto rows = listing.menu_rows();
        CHECK(rows.slots[5].name == "player [Jan 15 23:04:05]" && rows.slots[5].loadable);
        CHECK(rows.slots[0].name == "<EMPTY SLOT>" && !rows.slots[0].loadable && !rows.restart.loadable);
        CHECK(rows.quick.loadable && rows.unavailable.empty());
        // The Load page formats RESTART/QUICKLOAD with the names; the Save page lists the slots.
        Menu menu;
        menu.open(true);
        menu.set_saves(rows);
        menu.build(MenuPage::Load);
        CHECK(menu.items.size() == 10 && menu.items[0].string_id == IDS_MENU_LOADGAME_RESTART &&
              menu.items[0].insertion == "<EMPTY SLOT>" && !menu.enabled(menu.items[0]));
        CHECK(menu.items[1].insertion == "player [Jan 15 23:04:05]" && menu.enabled(menu.items[1]));
        CHECK(menu.items[7].literal == "player [Jan 15 23:04:05]" && menu.enabled(menu.items[7]) &&
              !menu.enabled(menu.items[2]));
        menu.build(MenuPage::Save);
        CHECK(menu.items.size() == 8 && menu.enabled(menu.items[0]) && menu.items[5].literal == rows.slots[5].name);
        // Disabled slots refuse everything and say so on both pages.
        SaveSlots off;
        CHECK(!off.save(*game, 0, error) && error == off.unavailable && off.menu_rows().unavailable == off.unavailable);
        menu.set_saves(off.menu_rows());
        CHECK(menu.page == MenuPage::Save && !menu.enabled(menu.items[0]));
        return true;
    }

    bool roundtrip()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        SaveSlots slots(fresh("roundtrip"));
        std::string error;
        health(*game) = 61;
        CHECK(slots.save(*game, 2, error));
        GameSave first;
        CHECK(game->save_game(first, error));
        health(*game) = 12;
        for (int i = 0; i < 3; ++i)
            CHECK(game->step(.1));
        const auto revision = game->world_revision;
        // Through the real file, in a fresh listing (as after a restart of the application).
        SaveSlots reader(slots.directory());
        reader.refresh();
        CHECK(reader.load(game, 2, error) == SaveSlots::LoadResult::Loaded);
        CHECK(player(*game) && health(*game) == 61 && game->world_revision == revision + 1);
        CHECK(game->state == SessionState::Ready && game->client_shell.restored_entry && reader.report.loads == 1);
        // A save right after the load reproduces the objects, links and attachments.
        GameSave again;
        CHECK(game->save_game(again, error));
        CHECK(again.image.objects.size() == first.image.objects.size() && again.image.links == first.image.links &&
              again.image.attachments == first.image.attachments && again.fingerprint == first.fingerprint);
        // The previous file survives a refused save, and no temporary is left behind.
        player(*game)->m_bDead = true;
        CHECK(!reader.save(*game, 2, error) && contains(error, "live local player"));
        player(*game)->m_bDead = false;
        SaveMeta meta;
        SaveError fault;
        CHECK(read_save_meta_file(reader.path(2), meta, fault) && meta.slot == 2);
        for (const auto& item : std::filesystem::directory_iterator(reader.directory()))
            CHECK(item.path().extension() == ".ob2sav");
        // The session can be saved again into the same slot (atomic overwrite).
        CHECK(reader.save(*game, 2, error) && reader.report.saves == 1 && reader.report.save_refusals == 1);
        return true;
    }

    bool corrupt()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        SaveSlots slots(fresh("corrupt"));
        std::string error;
        CHECK(slots.save(*game, 0, error) && slots.save(*game, 1, error) && slots.save(*game, SLOT_QUICK, error));
        std::vector<std::uint8_t> bytes;
        CHECK(read_file_bounded(slots.path(0), save_format::max_file, bytes, error));
        // A flipped payload byte is a CRC refusal; the listing marks the slot unreadable only on load.
        auto flipped = bytes;
        flipped[flipped.size() / 2] ^= 0x40;
        CHECK(write_file_atomic(slots.path(0), flipped, "test", error));
        const auto revision = game->world_revision;
        health(*game) = 33;
        CHECK(slots.load(game, 0, error) == SaveSlots::LoadResult::Refused && contains(error, "Slot 1 is unreadable"));
        CHECK(game->world_revision == revision && health(*game) == 33); // The session is untouched (D4).
        // Truncation is refused by the quick read as well.
        CHECK(write_file_atomic(slots.path(1), std::span(bytes).first(bytes.size() - 7), "test", error));
        slots.refresh();
        CHECK(slots.info(1).state == SaveSlotInfo::State::Unreadable &&
              slots.info(1).error.fault == SaveFault::Truncated);
        CHECK(slots.menu_rows().slots[1].name == "<UNREADABLE: truncated>" && slots.menu_rows().slots[1].loadable);
        CHECK(slots.load(game, 1, error) == SaveSlots::LoadResult::Refused && contains(error, "truncated"));
        // A file in the wrong place (a quicksave renamed to slot 3) is refused by kind.
        std::filesystem::copy_file(slots.path(SLOT_QUICK), slots.path(3));
        slots.refresh();
        CHECK(slots.info(3).state == SaveSlotInfo::State::Unreadable &&
              slots.menu_rows().slots[3].name == "<UNREADABLE: format>");
        CHECK(slots.load(game, 3, error) == SaveSlots::LoadResult::Refused && contains(error, "different kind"));
        // Not a save at all.
        std::ofstream(slots.path(4), std::ios::binary) << "not a save";
        slots.refresh();
        CHECK(slots.info(4).state == SaveSlotInfo::State::Unreadable);
        // An empty slot is not an error.
        CHECK(slots.load(game, 6, error) == SaveSlots::LoadResult::Empty && slots.report.empty_loads == 1);
        CHECK(game->world_revision == revision && slots.report.loads == 0);
        return true;
    }

    bool mismatch()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        SaveSlots slots(fresh("mismatch"));
        std::string error;
        CHECK(slots.save(*game, 0, error));
        game.reset(); // One bound session at a time.
        // A mod added since the save: refused, naming the archive (decision 5).
        std::unique_ptr<GameSession> modded;
        CHECK(empty_session(modded, true));
        CHECK(slots.load(modded, 0, error) == SaveSlots::LoadResult::Refused && contains(error, "fingerprint") &&
              contains(error, "mod.rez"));
        CHECK(modded->world_path.empty() && modded->game_load.rejected == 1);
        // Another beta's payload versions: refused, naming the class and both builds (decision 6).
        SaveContainer container;
        SaveError fault;
        CHECK(read_save_file(slots.path(0), container, fault));
        for (auto& c : container.fingerprint.classes)
            if (c.name == "CPlayerObj")
                c.payload_version += 1;
        container.fingerprint.class_schema = class_schema_digest(container.fingerprint.classes);
        container.meta.version = "0.0.1";
        CHECK(write_save_file(slots.path(1), [&] {
            auto copy = container;
            copy.meta.slot = 1;
            return copy;
        }(), fault));
        modded.reset();
        std::unique_ptr<GameSession> plain;
        CHECK(empty_session(plain));
        CHECK(slots.load(plain, 1, error) == SaveSlots::LoadResult::Refused && contains(error, "CPlayerObj") &&
              contains(error, "made by OpenBlood2 0.0.1") && contains(error, save_build_version()));
        // The unchanged slot still loads from the menu with no world.
        CHECK(slots.load(plain, 0, error) == SaveSlots::LoadResult::Loaded && plain->world_path == "worlds/player.dat");
        return true;
    }

    bool keys()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        Menu menu;
        MenuInput input;
        input.controls.focused = true;
        // F5 in play when alive; F6 in play; neither in the menu, the inspector or the demo; F2/F3 do not save.
        CHECK(!input.event(key(SDL_SCANCODE_F5), 1, menu, true, true, false, 640, 480) && input.quick_save);
        CHECK(!input.event(key(SDL_SCANCODE_F6), 1, menu, true, true, false, 640, 480) && input.quick_load);
        input.quick_save = input.quick_load = false;
        auto repeat = key(SDL_SCANCODE_F5);
        repeat.key.repeat = true;
        input.event(repeat, 1, menu, true, true, false, 640, 480);
        input.event(key(SDL_SCANCODE_F5), 1, menu, true, false, false, 640, 480); // Dead.
        input.event(key(SDL_SCANCODE_F2), 1, menu, true, true, false, 640, 480);
        input.event(key(SDL_SCANCODE_F3), 1, menu, true, true, false, 640, 480);
        CHECK(!input.quick_save && !input.quick_load);
        input.event(key(SDL_SCANCODE_F6), 1, menu, true, false, false, 640, 480); // Dead: still quickloads.
        CHECK(input.quick_load);
        input.quick_load = false;
        menu.open(true);
        input.event(key(SDL_SCANCODE_F5), 1, menu, true, true, false, 640, 480);
        input.event(key(SDL_SCANCODE_F6), 1, menu, true, true, false, 640, 480);
        CHECK(!input.quick_save && !input.quick_load);
        menu.close();
        input.inspector = true;
        input.event(key(SDL_SCANCODE_F5), 1, menu, true, true, true, 640, 480);
        CHECK(!input.quick_save);
        input.inspector = false;
        Menu demo(MenuEdition::Demo);
        input.event(key(SDL_SCANCODE_F5), 1, demo, true, true, false, 640, 480);
        input.event(key(SDL_SCANCODE_F6), 1, demo, true, true, false, 640, 480);
        CHECK(!input.quick_save && !input.quick_load);
        input.event(key(SDL_SCANCODE_F5), 1, menu, false, false, false, 640, 480); // Not in a world.
        CHECK(!input.quick_save);

        // The flow: F6 with no quicksave shows a notice and changes nothing.
        SaveFlow flow;
        flow.slots = SaveSlots(fresh("keys"));
        std::string error;
        const auto revision = game->world_revision;
        CHECK(flow.quick_load(game, error) == SaveFlow::Outcome::Empty && flow.notice.text == "No quicksave to load.");
        CHECK(game->world_revision == revision);
        // F5 overwrites without confirmation, without pausing, and shows "Quicksave...".
        health(*game) = 70;
        CHECK(flow.quick_save(*game, error) == SaveFlow::Outcome::Saved && flow.notice.text == "Quicksave...");
        health(*game) = 71;
        CHECK(flow.quick_save(*game, error) == SaveFlow::Outcome::Saved && flow.slots.report.quick_saves == 2);
        CHECK(flow.notice.visible());
        flow.notice.advance(2);
        CHECK(!flow.notice.visible() && flow.notice.text.empty());
        // Die, then F6: the quicksave replaces the dead session.
        auto* p = player(*game);
        DVector pos;
        CHECK(game->server.GetObjectPos(p->m_hObject, &pos) == DE_OK);
        auto damage = game->server.StartMessageToObject(p, p->m_hObject, damage_message);
        CHECK(damage &&
              game->server.WriteDamage(damage, {{0, 0, 1}, pos, 10000, DamageType::Normal, p->m_hObject}) == DE_OK);
        CHECK(game->server.EndMessage(damage) == DE_OK && game->step(1. / 60) && player(*game)->m_bDead);
        CHECK(flow.quick_save(*game, error) == SaveFlow::Outcome::Failed && contains(error, "live local player"));
        CHECK(flow.quick_load(game, error) == SaveFlow::Outcome::Loaded && flow.notice.text == "Quickload...");
        CHECK(!player(*game)->m_bDead && health(*game) == 71 && flow.slots.report.quick_loads == 1);
        return true;
    }

    bool demo()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        game->resources.set_demo(true);
        SaveSlots slots(fresh("demo"));
        std::string error;
        CHECK(!slots.save(*game, 0, error) && contains(error, "demo") && !std::filesystem::exists(slots.path(0)));
        CHECK(slots.capture_level_entry(*game, error) == false && contains(error, "demo") && !slots.level_entry());
        game->resources.set_demo(false);
        CHECK(slots.save(*game, 0, error));
        game->resources.set_demo(true);
        CHECK(slots.load(game, 0, error) == SaveSlots::LoadResult::Refused && contains(error, "demo"));
        // The demo menus have no Load/Save rows; the application never enables slots for the demo.
        Menu menu(MenuEdition::Demo);
        menu.open(true);
        menu.build(MenuPage::Solo);
        CHECK(menu.items.size() == 2);
        menu.open_load(true, false);
        CHECK(menu.page == MenuPage::Main && menu.top == MenuPage::Main);
        return true;
    }

    bool restart()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        SaveFlow flow;
        flow.slots = SaveSlots(fresh("restart"));
        std::string error;
        // NEW_GAME: ClearSaveSlot(SLOT_CURRENT), then the level-entry snapshot.
        health(*game) = 90;
        CHECK(flow.level_entered(*game, true, error) && flow.slots.level_entry());
        const auto& current = flow.slots.info(SLOT_CURRENT);
        CHECK(current.state == SaveSlotInfo::State::Ready && current.meta.kind == SaveKind::LevelEntry &&
              current.meta.display_name == "player");
        // A slot embeds it as AUTO.
        health(*game) = 50;
        CHECK(flow.slots.save(*game, 0, error));
        SaveContainer container;
        SaveError fault;
        CHECK(read_save_file(flow.slots.path(0), container, fault) && container.level_entry &&
              *container.level_entry == *flow.slots.level_entry());
        // Restart returns to the level entry and can be repeated; a restore never refreshes it.
        health(*game) = 10;
        CHECK(flow.slots.load(game, SLOT_CURRENT, error) == SaveSlots::LoadResult::Loaded && health(*game) == 90);
        const auto snapshot = *flow.slots.level_entry();
        CHECK(flow.level_entered(*game, false, error) && flow.slots.report.level_entry_skips == 1 &&
              *flow.slots.level_entry() == snapshot);
        health(*game) = 11;
        CHECK(flow.slots.load(game, SLOT_CURRENT, error) == SaveSlots::LoadResult::Loaded && health(*game) == 90);
        CHECK(flow.slots.report.restarts == 2);
        // Menu Restart row.
        CHECK(flow.slots.menu_rows().restart.loadable && flow.slots.menu_rows().restart.name == "player");
        // A new level entry replaces it; loading slot 0 brings slot 0's AUTO back (the start of slot 0's level).
        WorldMessage level;
        CHECK(original_world_request("next", 0, LOADTYPE_NEW_LEVEL, 0, level));
        CHECK(game->queue_level_change(level, "arrival", error));
        CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::Committed);
        health(*game) = 77;
        CHECK(flow.level_entered(*game, false, error) &&
              flow.slots.info(SLOT_CURRENT).meta.world_path == "worlds/next.dat");
        CHECK(flow.slots.load(game, 0, error) == SaveSlots::LoadResult::Loaded && health(*game) == 50);
        CHECK(*flow.slots.level_entry() == *container.level_entry &&
              flow.slots.info(SLOT_CURRENT).meta.world_path == "worlds/player.dat");
        std::vector<std::uint8_t> file;
        CHECK(read_file_bounded(flow.slots.path(SLOT_CURRENT), save_format::max_file, file, error) &&
              file == *container.level_entry);
        CHECK(flow.slots.load(game, SLOT_CURRENT, error) == SaveSlots::LoadResult::Loaded && health(*game) == 90);
        // A slot saved without a snapshot clears Restart when loaded, so it never reaches another level.
        CHECK(flow.slots.clear_level_entry(error) && !std::filesystem::exists(flow.slots.path(SLOT_CURRENT)));
        CHECK(flow.slots.save(*game, 1, error));
        CHECK(flow.level_entered(*game, false, error)); // After a restore: skipped, still empty.
        CHECK(!flow.slots.level_entry());
        flow.slots.refresh();
        CHECK(!flow.slots.menu_rows().restart.loadable);
        // A refused snapshot removes the file too.
        CHECK(flow.slots.load(game, 0, error) == SaveSlots::LoadResult::Loaded &&
              std::filesystem::exists(flow.slots.path(SLOT_CURRENT)));
        game->server.SetGameConVar("GAME_DOAUTOSAVE", "1");
        player(*game)->m_bDead = true;
        CHECK(!flow.slots.capture_level_entry(*game, error) && contains(error, "Level-entry snapshot refused"));
        CHECK(!flow.slots.level_entry() && !std::filesystem::exists(flow.slots.path(SLOT_CURRENT)));
        player(*game)->m_bDead = false;
        CHECK(flow.slots.load(game, 1, error) == SaveSlots::LoadResult::Loaded && !flow.slots.level_entry());
        return true;
    }

    bool reset()
    {
        // After a promoted load, UI selections, queued commands and input ownership are cleared,
        // as after the NEW_LEVEL commit.
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        SaveFlow flow;
        flow.slots = SaveSlots(fresh("reset"));
        std::string error;
        CHECK(flow.slots.save(*game, 0, error));
        Menu menu;
        MenuInput input;
        input.controls.focused = true;
        ObjectSelection selection;
        selection.id = game->objects.get(player(*game)->m_hObject)->id;
        ObjectId controlled = selection.id;
        menu.open(true);
        menu.set_saves(flow.slots.menu_rows());
        menu.build(MenuPage::Load);
        menu.selected = 2;
        menu.activate();
        const auto command = menu.take_command();
        CHECK(command.action == MenuAction::LoadGame && command.slot == 0);
        auto& old_shell = game->client_shell;
        old_shell.use_down = old_shell.medkit_down = true;
        input.quick_save = input.quick_load = true;
        CHECK(flow.command(command, game, menu, error) == SaveFlow::Outcome::Loaded && !menu.active);
        auto& shell = game->client_shell;
        shell.use_down = shell.use_was_down = shell.detonate_down = true;
        shell.dead_use_menu = true;
        shell.movement_input.forward = true;
        reset_after_promotion(menu, input, selection, controlled, shell);
        CHECK(!selection.id && !controlled && !input.quick_save && !input.quick_load);
        CHECK(!shell.use_down && !shell.use_was_down && !shell.detonate_down && !shell.dead_use_menu &&
              !shell.movement_input.forward);
        CHECK(input.controls.mouse_requested && input.controls.pending_weapon_slot == -1);
        return true;
    }

    bool traversal()
    {
        // The retail menus as the frontend drives them: Solo -> Save -> slot, Solo -> Load -> slot,
        // death USE -> Load (top level) -> Esc returns to the game.
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        SaveFlow flow;
        flow.slots = SaveSlots(fresh("traversal"));
        std::string error;
        CHECK(flow.level_entered(*game, true, error));
        Menu menu;
        menu.open(true);
        flow.sync(menu);
        menu.activate(); // Single player.
        CHECK(menu.page == MenuPage::Solo && menu.items[3].command == int(MENU_CMD_SAVE_GAME_MENU) &&
              menu.enabled(menu.items[3]));
        menu.selected = 3;
        menu.activate();
        CHECK(menu.page == MenuPage::Save && menu.items.size() == 8 && menu.selected == 0);
        menu.select(1);
        menu.select(1);
        health(*game) = 44;
        menu.activate();
        auto command = menu.take_command();
        CHECK(command.action == MenuAction::SaveGame && command.slot == 2);
        CHECK(flow.command(command, game, menu, error) == SaveFlow::Outcome::Saved && !menu.active);
        // The rows follow the files.
        menu.open(true);
        menu.build(MenuPage::Save);
        flow.sync(menu);
        CHECK(menu.items[2].literal.starts_with("player [") && menu.items[0].literal == "<EMPTY SLOT>");
        menu.back();
        CHECK(menu.page == MenuPage::Solo);
        menu.selected = 2;
        menu.activate();
        CHECK(menu.page == MenuPage::Load && menu.enabled(menu.items[0]) && !menu.enabled(menu.items[1]) &&
              menu.items[0].insertion == "player" && menu.enabled(menu.items[4]));
        menu.selected = 4;
        health(*game) = 5;
        menu.activate();
        command = menu.take_command();
        CHECK(command.action == MenuAction::LoadGame && command.slot == 2);
        CHECK(flow.command(command, game, menu, error) == SaveFlow::Outcome::Loaded && !menu.active &&
              health(*game) == 44);
        // A refused save keeps the Save page for the modal, naming the reason.
        menu.open(true);
        menu.build(MenuPage::Save);
        player(*game)->m_bDead = true;
        menu.activate();
        CHECK(flow.command(menu.take_command(), game, menu, error) == SaveFlow::Outcome::Failed && menu.active &&
              menu.page == MenuPage::Save && contains(error, "live local player"));
        player(*game)->m_bDead = false;
        menu.close();
        // Death: USE opens the Load menu as the top level; Esc returns to the (dead) game.
        auto* p = player(*game);
        DVector pos;
        CHECK(game->server.GetObjectPos(p->m_hObject, &pos) == DE_OK);
        auto damage = game->server.StartMessageToObject(p, p->m_hObject, damage_message);
        CHECK(damage &&
              game->server.WriteDamage(damage, {{0, 0, 1}, pos, 10000, DamageType::Normal, p->m_hObject}) == DE_OK);
        CHECK(game->server.EndMessage(damage) == DE_OK && game->step(1. / 60) && player(*game)->m_bDead);
        CHECK(game->client_shell.death_camera.active && !game->client_shell.dead_use_menu);
        game->client_shell.use_down = true;
        CHECK(game->step(1. / 60) && game->client_shell.dead_use_menu && game->client_shell.dead_use_menus == 1);
        menu.open_load(true, false);
        flow.sync(menu);
        CHECK(menu.active && menu.page == MenuPage::Load && menu.top == MenuPage::Load && menu.enabled(menu.items[4]));
        menu.back();
        CHECK(!menu.active && menu.take_command().action == MenuAction::Resume);
        // From the death menu the Restart row returns to the level entry.
        menu.open_load(true, false);
        flow.sync(menu);
        menu.selected = 0;
        menu.activate();
        command = menu.take_command();
        CHECK(command.action == MenuAction::LoadGame && command.slot == SLOT_CURRENT);
        CHECK(flow.command(command, game, menu, error) == SaveFlow::Outcome::Loaded && !player(*game)->m_bDead);
        // The main menu's Esc still resumes from the Main page after open().
        menu.open(true);
        CHECK(menu.top == MenuPage::Main);
        menu.back();
        CHECK(!menu.active);
        return true;
    }

    // A saveable class with no save support: stays Unsupported whatever later lots declare.
    struct Unported : BaseClass
    {
        Unported() : BaseClass(OT_NORMAL) {}
        DDWORD EngineMessageFn(DDWORD id, void* data, float param) override
        {
            if (id == MID_INITIALUPDATE)
                g_pServerDE->SetObjectUserFlags(m_hObject, USRFLG_SAVEABLE);
            return BaseClass::EngineMessageFn(id, data, param);
        }
    };
    std::unique_ptr<BaseClass> construct_unported()
    {
        return std::make_unique<Unported>();
    }
    const ClassInfo unported_type{"SlotUnported", nullptr, construct_unported, {}, SaveSupport::Unsupported};

    bool refusal()
    {
        // A save that holds a class without save support is refused by name, keeping the file.
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        SaveSlots slots(fresh("refusal"));
        std::string error;
        CHECK(slots.save(*game, 0, error));
        std::vector<std::uint8_t> before;
        CHECK(read_file_bounded(slots.path(0), save_format::max_file, before, error));
        CHECK(game->objects.register_class(unported_type));
        CHECK(game->objects.create(game->objects.find_class("SlotUnported")));
        CHECK(game->objects.finish_frame());
        CHECK(!slots.save(*game, 0, error) &&
              contains(error, "These classes have no save support: SlotUnported (Unsupported)"));
        CHECK(slots.report.refused_classes == std::vector<std::string>{"SlotUnported (Unsupported)"});
        std::vector<std::uint8_t> after;
        CHECK(read_file_bounded(slots.path(0), save_format::max_file, after, error) && after == before);
        return true;
    }
    // Real data: SubwayA through the menu flow. Until the class lots land the save is refused by
    // class name, the snapshot is refused silently and Restart stays empty; nothing is written.
    bool private_world(const std::filesystem::path& data)
    {
        auto game = std::make_unique<GameSession>();
        std::string error;
        Config config;
        config.data_dir = data;
        CHECK(game->initialize() && game->resources.mount_game(config, error));
        CHECK(GameSession::start_new_game(game, "01_SubwayA", 1, 0, error));
        SaveFlow flow;
        flow.slots = SaveSlots(fresh("private"));
        const bool entered = flow.level_entered(*game, true, error);
        std::cout << "Level entry: " << (entered ? "saved" : error) << '\n';
        const bool saved = flow.slots.save(*game, 0, error);
        std::cout << "Slot 0: " << (saved ? "saved" : error) << '\n';
        if (!saved)
        {
            CHECK(!flow.slots.report.refused_classes.empty() && contains(error, "no save support"));
            CHECK(!std::filesystem::exists(flow.slots.path(0)));
            CHECK(!entered && !flow.slots.level_entry() && !std::filesystem::exists(flow.slots.path(SLOT_CURRENT)));
            return true;
        }
        // Every class supported: the slot loads back and Restart returns to the entry.
        CHECK(entered && flow.slots.load(game, 0, error) == SaveSlots::LoadResult::Loaded);
        CHECK(flow.slots.load(game, SLOT_CURRENT, error) == SaveSlots::LoadResult::Loaded);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    const std::string_view mode = argv[1];
    root = argv[2];
    if (!write_fixture())
        return 1;
    if (mode == "private")
        return argc > 3 && private_world(argv[3]) ? 0 : 1;
    const bool ok = mode == "naming"      ? naming()
                    : mode == "meta"      ? meta()
                    : mode == "roundtrip" ? roundtrip()
                    : mode == "corrupt"   ? corrupt()
                    : mode == "mismatch"  ? mismatch()
                    : mode == "keys"      ? keys()
                    : mode == "demo"      ? demo()
                    : mode == "restart"   ? restart()
                    : mode == "reset"     ? reset()
                    : mode == "traversal" ? traversal()
                    : mode == "refusal"   ? refusal()
                                          : false;
    return ok ? 0 : 1;
}
