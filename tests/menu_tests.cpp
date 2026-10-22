#include "app/cheat_flow.h"
#include "app/menu_input.h"
#include "app/menu_renderer.h"
#include "app/session_playback.h"
#include "app/ui_runtime.h"
#include "camera_fixture.h"
#include "engine/capture.h"
#include "game/menu_audio.h"
#include "game/original/AI_Mgr.h"
#include "game/original/CharacterProfiles.h"
#include "game/original/MenuIds.h"
#include "game/server_adapter.h"
#include "string_fixture.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <iostream>
int test_menu_preferences(int, char**);
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << " " << error << '\n';                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool state()
    {
        std::string error;
        Menu menu;
        menu.open(false);
        CHECK(menu.items.size() == 5 && menu.items[0].string_id == IDS_MENU_MAIN_SINGLEPLAYER);
        menu.back();
        CHECK(menu.active && menu.take_command().action == MenuAction::None);
        menu.activate();
        CHECK(menu.page == MenuPage::Solo && menu.items.size() == 5);
        menu.selected = 0;
        menu.activate();
        CHECK(menu.page == MenuPage::Difficulty && menu.selected == 1);
        menu.activate();
        const auto start = menu.take_command();
        CHECK(start.action == MenuAction::NewGame && start.character == 0 && start.story && start.difficulty == 1);
        CHECK(menu.take_command().action == MenuAction::None);
        menu.back();
        menu.selected = 1;
        menu.activate();
        CHECK(menu.page == MenuPage::Character);
        for (int i = 0; i < 4; ++i)
        {
            menu.build(MenuPage::Character);
            menu.selected = i;
            menu.activate();
            menu.selected = 2;
            menu.activate();
            auto command = menu.take_command();
            CHECK(command.action == MenuAction::NewGame && !command.story && command.difficulty == 2);
            CHECK(command.character == unsigned(i == 1 ? 3 : i == 3 ? 1 : i));
        }
        menu.open(true);
        menu.items[0].rect = {100, 100, 300, 140};
        menu.pointer(120, 110, false, true, false);
        CHECK(menu.page == MenuPage::Main);
        menu.pointer(120, 110, true, false, false);
        menu.pointer(500, 110, false, true, false);
        CHECK(menu.page == MenuPage::Main);
        menu.pointer(120, 110, true, false, false);
        menu.cancel_pointer();
        menu.pointer(120, 110, false, true, false);
        CHECK(menu.page == MenuPage::Main);
        menu.pointer(120, 110, true, false, false);
        menu.pointer(120, 110, false, true, false);
        CHECK(menu.page == MenuPage::Solo);
        menu.back();
        menu.back();
        CHECK(!menu.active && menu.take_command().action == MenuAction::Resume);
        menu.open(false);
        CHECK(menu.items[3].string_id == IDS_MENU_MAIN_CREDITS && menu.enabled(menu.items[3]));
        menu.selected = 3;
        menu.activate();
        CHECK(menu.page == MenuPage::Main && menu.take_command().action == MenuAction::Credits);
        menu.open(true);
        CHECK(menu.enabled(menu.items[3])); // In a world the owner opens the text-only GS_CREDITS screen.
        menu.selected = 3;
        menu.activate();
        CHECK(menu.take_command().action == MenuAction::Credits);
        menu.open(false);
        menu.selected = 4;
        menu.activate();
        CHECK(menu.page == MenuPage::Quit && menu.selected == 1);
        menu.activate();
        CHECK(menu.page == MenuPage::Main && menu.take_command().action == MenuAction::None);
        menu.selected = 4;
        menu.activate();
        menu.selected = 0;
        menu.activate();
        CHECK(menu.take_command().action == MenuAction::Quit);
        menu.build(MenuPage::Save);
        CHECK(menu.items.size() == 8);
        for (int i = 0; i < 8; ++i)
        {
            menu.selected = i;
            menu.activate();
            CHECK(menu.take_command().action == MenuAction::None);
        }
        menu.build(MenuPage::Load);
        CHECK(menu.items.size() == 10);
        menu.build(MenuPage::Audio);
        menu.selected = 1;
        menu.adjust(-1);
        CHECK(menu.settings.sound_volume == 45 && menu.take_command().action == MenuAction::Settings);
        menu.selected = 0;
        menu.adjust(1);
        menu.selected = 1;
        menu.adjust(-1);
        CHECK(menu.settings.sound_volume == 45);
        menu.build(MenuPage::Mouse);
        menu.selected = 0;
        for (int i = 0; i < 100; ++i)
            menu.adjust(-1);
        CHECK(menu.settings.sensitivity == 100);
        for (int i = 0; i < 100; ++i)
            menu.adjust(1);
        CHECK(menu.settings.sensitivity == 1500);
        // Display: original DETAIL LEVEL strings cycle low/medium/high; LIGHTING toggles.
        menu.build(MenuPage::Options);
        CHECK(menu.items[0].command == int(MENU_CMD_DISPLAY) && menu.enabled(menu.items[0]) && menu.selected == 0);
        menu.activate();
        CHECK(menu.page == MenuPage::Display && menu.items.size() == 8 && menu.small());
        CHECK(menu.items[0].setting == 6 && menu.items[0].string_id == IDS_MENU_DISPLAY_DETAIL_HIGH &&
              menu.items[1].setting == 8 && !menu.items[1].string_id && menu.items[2].setting == 7 &&
              !menu.items[2].string_id && menu.items[3].setting == 9 && !menu.enabled(menu.items[3]));
        // Post-processing: TONEMAP, BLOOM and AMBIENT OCCLUSION need the enhanced renderer;
        // ANTI-ALIASING serves both.
        CHECK(menu.items[4].setting == 19 && !menu.enabled(menu.items[4]) && menu.items[5].setting == 18 &&
              !menu.enabled(menu.items[5]) && menu.items[6].setting == 20 && !menu.enabled(menu.items[6]) &&
              menu.items[7].setting == 17 && menu.enabled(menu.items[7]));
        menu.selected = 7;
        menu.activate();
        CHECK(menu.settings.antialiasing == 1 && menu.take_command().action == MenuAction::Settings);
        menu.adjust(1);
        menu.adjust(1);
        CHECK(menu.settings.antialiasing == 3);
        menu.adjust(1);
        CHECK(menu.settings.antialiasing == 0);
        menu.adjust(-1);
        CHECK(menu.settings.antialiasing == 3);
        menu.adjust(1);
        menu.selected = 4;
        menu.activate(); // Disabled with the original renderer.
        CHECK(menu.settings.tonemap == 1);
        menu.take_command();
        menu.take_command();
        menu.selected = 0;
        menu.activate();
        CHECK(menu.settings.detail == 0 && menu.items[0].string_id == IDS_MENU_DISPLAY_DETAIL_LOW &&
              menu.take_command().action == MenuAction::Settings);
        menu.adjust(-1);
        CHECK(menu.settings.detail == 2 && menu.items[0].string_id == IDS_MENU_DISPLAY_DETAIL_HIGH);
        menu.adjust(-1);
        CHECK(menu.settings.detail == 1 && menu.items[0].string_id == IDS_MENU_DISPLAY_DETAIL_MEDIUM);
        menu.selected = 2;
        menu.activate();
        CHECK(menu.settings.pixel_lighting && menu.take_command().action == MenuAction::Settings);
        menu.adjust(1);
        CHECK(!menu.settings.pixel_lighting);
        // RENDERER switches the LIGHTING row to the enhanced profiles and enables NORMAL MAPS.
        menu.selected = 1;
        menu.activate();
        CHECK(menu.settings.renderer == 1 && menu.enabled(menu.items[3]) &&
              menu.take_command().action == MenuAction::Settings);
        menu.selected = 2;
        menu.activate();
        CHECK(menu.settings.quality == 1 && !menu.settings.pixel_lighting);
        menu.selected = 3;
        menu.activate();
        CHECK(!menu.settings.normal_maps);
        menu.activate();
        CHECK(menu.enabled(menu.items[4]) && menu.enabled(menu.items[5]));
        menu.selected = 4;
        menu.activate();
        CHECK(menu.settings.tonemap == 2);
        menu.adjust(1);
        CHECK(menu.settings.tonemap == 0);
        menu.adjust(-1);
        menu.adjust(-1);
        CHECK(menu.settings.tonemap == 1);
        menu.selected = 5;
        menu.activate();
        CHECK(!menu.settings.bloom);
        menu.activate();
        CHECK(menu.settings.bloom);
        CHECK(menu.enabled(menu.items[6]) && menu.settings.ambient_occlusion == 2);
        menu.selected = 6;
        menu.activate();
        CHECK(menu.settings.ambient_occlusion == 0);
        menu.adjust(-1);
        CHECK(menu.settings.ambient_occlusion == 2);
        menu.adjust(-1);
        CHECK(menu.settings.ambient_occlusion == 1);
        menu.adjust(1);
        menu.selected = 1;
        menu.activate();
        CHECK(menu.settings.renderer == 0 && !menu.enabled(menu.items[3]) && menu.settings.quality == 1 &&
              menu.settings.normal_maps);
        menu.selected = 3;
        menu.activate(); // Disabled with the original renderer.
        CHECK(menu.settings.normal_maps);
        menu.settings.quality = 0;
        menu.take_command();
        menu.back();
        CHECK(menu.page == MenuPage::Options && !strcmp(menu.title(), "options.pcx"));
        // CMenuControls: owner rows, no scroll wrap, Back cancels tracking before leaving.
        CHECK(!menu.enabled(menu.items[2]));
        menu.set_controls({{705, {}, "W", true}, {709, {}, {}, false}, {706, {}, {}, true}});
        CHECK(menu.page == MenuPage::Options && menu.enabled(menu.items[2]));
        menu.selected = 2;
        menu.activate();
        CHECK(menu.page == MenuPage::Controls && menu.small() && menu.items.size() == 4 &&
              menu.items[0].command == int(MENU_CMD_CHANGE_CONTROL) && !menu.enabled(menu.items[1]) &&
              menu.items[3].literal == "Restore Defaults" && !strcmp(menu.title(), "options.pcx"));
        menu.select(-1);
        CHECK(menu.selected == 0); // No wrap at the top.
        menu.select(1);
        CHECK(menu.selected == 2); // The disabled Strafe row is skipped.
        menu.select(1);
        menu.select(1);
        CHECK(menu.selected == 3); // No wrap at the bottom.
        menu.selected = 2;
        menu.activate();
        CHECK(menu.binding == 2);
        menu.select(-1);
        menu.adjust(1);
        menu.activate();
        CHECK(menu.selected == 2 && menu.binding == 2 && menu.take_command().action == MenuAction::None);
        menu.set_controls({{705, {}, "W", true}, {709, {}, {}, false}, {706, {}, "S", true}});
        CHECK(menu.page == MenuPage::Controls && menu.selected == 2 && menu.binding < 0);
        menu.activate();
        menu.back();
        CHECK(menu.page == MenuPage::Controls && menu.binding < 0);
        menu.selected = 3;
        menu.activate();
        CHECK(menu.take_command().action == MenuAction::RestoreControls);
        menu.back();
        CHECK(menu.page == MenuPage::Options);
        menu.set_controls({});
        // XR sessions add the replacement VR page: the turn step cycles 15..90 degrees then smooth,
        // the aim laser and crosshair toggle. Desktop Options has no VR row; CHEATS is always last.
        CHECK(menu.items.size() == 7 && menu.items[6].literal == "CHEATS");
        menu.vr = true;
        menu.build(MenuPage::Options);
        CHECK(menu.items.size() == 8 && menu.items[6].literal == "VR" && menu.enabled(menu.items[6]) &&
              menu.items[7].literal == "CHEATS");
        menu.selected = 6;
        menu.activate();
        CHECK(menu.page == MenuPage::Vr && menu.small() && menu.items.size() == 4 &&
              !strcmp(menu.title(), "options.pcx") && menu.items[0].setting == 10 && menu.items[1].setting == 11 &&
              menu.items[2].setting == 12 && menu.items[3].setting == 13);
        menu.selected = 0;
        menu.activate();
        CHECK(menu.settings.vr_turn == 60 && menu.take_command().action == MenuAction::Settings);
        menu.adjust(1);
        menu.adjust(1);
        CHECK(menu.settings.vr_turn == 0);
        menu.adjust(1);
        CHECK(menu.settings.vr_turn == 15);
        menu.adjust(-1);
        CHECK(menu.settings.vr_turn == 0);
        menu.selected = 1;
        menu.activate();
        menu.selected = 2;
        menu.activate();
        CHECK(!menu.settings.vr_laser && !menu.settings.vr_crosshair);
        menu.selected = 3;
        menu.activate();
        CHECK(menu.settings.vr_move_hand);
        menu.back();
        CHECK(menu.page == MenuPage::Options);
        menu.vr = false;
        menu.settings.vr_turn = 45;
        menu.settings.vr_laser = menu.settings.vr_crosshair = true;
        menu.settings.vr_move_hand = false;
        menu.take_command();
        // Replacement Options > CHEATS: the switch gates every other row; god mode is a preference;
        // the action rows need a living player in a world; LEVEL cycles the owner's worlds and GO TO
        // LEVEL requests a new game there.
        menu.build(MenuPage::Options);
        menu.selected = 6;
        menu.activate();
        CHECK(menu.page == MenuPage::Cheats && menu.small() && !strcmp(menu.title(), "options.pcx") &&
              menu.items.size() == 10 && menu.items[0].setting == 14 && menu.items[1].setting == 15 &&
              menu.items[2].cheat == CHEAT_KFA && menu.items[6].cheat == CHEAT_ALLKEYS &&
              menu.items[7].cheat == CHEAT_KILLALLAI && menu.items[8].setting == 16 &&
              menu.items[9].literal == "GO TO LEVEL");
        for (std::size_t i = 1; i < menu.items.size(); ++i)
            CHECK(!menu.enabled(menu.items[i]));
        menu.select(1);
        CHECK(menu.selected == 0);
        menu.activate();
        CHECK(menu.settings.cheats && menu.take_command().action == MenuAction::Settings);
        CHECK(menu.enabled(menu.items[1]) && !menu.enabled(menu.items[2]) && !menu.enabled(menu.items[8]) &&
              !menu.enabled(menu.items[9]));
        menu.set_levels({"01_subwaya", "02_cityhub", "credits"});
        CHECK(menu.page == MenuPage::Cheats && menu.enabled(menu.items[8]) && menu.enabled(menu.items[9]));
        menu.selected = 1;
        menu.activate();
        CHECK(menu.settings.god_mode && menu.take_command().action == MenuAction::Settings);
        menu.selected = 8;
        menu.activate();
        menu.adjust(-1);
        menu.adjust(-1);
        CHECK(menu.level == 2 && menu.take_command().action == MenuAction::None); // Not a preference.
        menu.adjust(1);
        CHECK(menu.level == 0);
        menu.adjust(1);
        menu.selected = 9;
        menu.activate();
        auto warp = menu.take_command();
        CHECK(warp.action == MenuAction::Warp && warp.world == "02_cityhub");
        menu.open(true, true);
        menu.build(MenuPage::Cheats);
        for (int i = 2; i < 8; ++i)
            CHECK(menu.enabled(menu.items[i]));
        menu.selected = 2;
        menu.activate();
        auto kfa = menu.take_command();
        CHECK(kfa.action == MenuAction::Cheat && kfa.cheat == CHEAT_KFA);
        menu.selected = 6;
        menu.activate();
        CHECK(menu.take_command().cheat == CHEAT_ALLKEYS);
        menu.open(true, false);
        menu.build(MenuPage::Cheats);
        CHECK(!menu.enabled(menu.items[2]) && menu.enabled(menu.items[1]));
        menu.back();
        CHECK(menu.page == MenuPage::Options);
        menu.build(MenuPage::Cheats);
        menu.activate(); // The switch off greys the rows again and keeps god mode stored.
        CHECK(!menu.settings.cheats && menu.settings.god_mode && !menu.enabled(menu.items[1]));
        menu.settings.god_mode = false;
        menu.set_levels({});
        menu.take_command();
        menu.open(false);
        auto fit = MenuViewport::fit(1920, 1080);
        float x = 240, y = 0;
        CHECK(fit.point(x, y) && x == 0 && y == 0);
        x = 200;
        y = 20;
        CHECK(!fit.point(x, y));
        fit = MenuViewport::fit(320, 240);
        x = 90;
        y = 70;
        CHECK(fit.point(x, y) && x == 180 && y == 140);
        MenuInput input;
        input.controls.focused = true;
        menu.open(true);
        SDL_Event key{};
        key.type = SDL_EVENT_KEY_DOWN;
        key.key.windowID = 42;
        key.key.scancode = SDL_SCANCODE_F10;
        CHECK(!input.event(key, 42, menu, true, true, true, 640, 480) && input.inspector && menu.active);
        key.key.repeat = true;
        input.event(key, 42, menu, true, true, true, 640, 480);
        CHECK(input.inspector);
        key.key.repeat = false;
        input.event(key, 42, menu, true, true, true, 640, 480);
        CHECK(!input.inspector && menu.active);
        key.key.scancode = SDL_SCANCODE_ESCAPE;
        input.event(key, 42, menu, true, true, true, 640, 480);
        CHECK(!menu.active && input.controls.mouse_requested);
        key.key.scancode = SDL_SCANCODE_W;
        input.event(key, 42, menu, true, true, true, 640, 480);
        CHECK(input.controls.sample(false).forward);
        key.key.scancode = SDL_SCANCODE_F10;
        input.event(key, 42, menu, true, true, true, 640, 480);
        CHECK(input.inspector && !input.controls.sample(false).forward && !input.controls.mouse_requested);
        key.key.scancode = SDL_SCANCODE_F;
        CHECK(input.event(key, 42, menu, true, true, true, 640, 480));
        CHECK(!input.controls.sample_fire(false).primary);
        input.inspector = false;
        menu.close();
        input.reset(menu, true);
        SDL_Event focus{};
        focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        focus.window.windowID = 42;
        input.event(focus, 42, menu, true, true, true, 640, 480);
        CHECK(menu.active && !input.controls.focused && !input.controls.sample(false).forward);
        focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        input.event(focus, 42, menu, true, true, true, 640, 480);
        key.key.scancode = SDL_SCANCODE_F10;
        input.event(key, 42, menu, true, true, false, 640, 480);
        CHECK(!input.inspector && menu.active);
        key.key.scancode = SDL_SCANCODE_RETURN;
        key.key.repeat = true;
        input.event(key, 42, menu, true, true, false, 640, 480);
        CHECK(menu.page == MenuPage::Main);
        Resources no_audio_resources;
        MenuAudio missing_audio(no_audio_resources);
        // Explicit missing resources: no fabricated successful voice and no navigation dependency.
        missing_audio.focus(true);
        CHECK(missing_audio.failures == 1 && !missing_audio.ambient && !missing_audio.error.empty());
        HudSurface bitmap{1, 1, std::vector<std::uint8_t>(4)};
        HudFont font;
        std::vector<std::uint8_t> widths(190);
        widths[0] = 1;
        widths[188] = 2;
        CHECK(!font.read_widths(widths, bitmap, error));
        CHECK(font.read_widths(widths, bitmap, error, 1));
        widths[188] = 3;
        CHECK(!font.read_widths(widths, bitmap, error, 1));
        widths[188] = 0;
        widths[2] = 2;
        CHECK(!font.read_widths(widths, bitmap, error, 1));
        Resources missing;
        MenuCanvas canvas(missing);
        CHECK(!canvas.initialize(error) && !error.empty());
        menu.open(false);
        menu.audio_failed = true;
        menu.activate();
        CHECK(menu.page == MenuPage::Solo && menu.error.empty());
        return true;
    }
    bool session(const std::filesystem::path& content)
    {
        std::string error;
        Config config;
        config.data_dir = content;
        Resources resources;
        CHECK(resources.mount_game(config, error));
        MenuAudio audio(resources);
        audio.random_state = 1;
        CHECK(audio.mixer.open_output(error));
        audio.focus(true);
        CHECK(audio.ambient && audio.starts == 1 && audio.failures == 0);
        audio.focus(true);
        CHECK(audio.starts == 1);
        audio.play(MenuCue::Select);
        audio.play(MenuCue::Enter);
        audio.play(MenuCue::Back);
        CHECK(audio.starts == 4 && audio.failures == 0);
        for (int i = 0; i < 40; ++i)
            audio.play(MenuCue::Select);
        CHECK(audio.mixer.voices().size() <= 16 && audio.mixer.get(audio.ambient) && audio.failures == 0);
        AudioMixer audible, quiet, music_off;
        SoundRequest cue;
        cue.resource = "sounds/interface/mainmenus/select01.wav";
        cue.space = SoundSpace::Local;
        SoundId quiet_id = 0;
        for (auto* mixer : {&audible, &quiet, &music_off})
        {
            mixer->inspect_pcm = true;
            auto id = mixer->play(resources, nullptr, cue, {}, error);
            CHECK(id);
            if (mixer == &quiet)
                quiet_id = id;
        }
        CHECK(quiet.set_gains(0, 1) && music_off.set_gains(1, 0));
        for (auto* mixer : {&audible, &quiet, &music_off})
            CHECK(mixer->advance(.05, {}, {}));
        CHECK(audible.peak > 0 && quiet.peak == 0 && quiet.get(quiet_id)->cursor > 0 &&
              audible.mixed == music_off.mixed);
        auto game = std::make_unique<GameSession>();
        game->resources = resources;
        CHECK(game->initialize());
        CHECK(game->server.audio.open_output(error));
        CHECK(GameSession::start_new_game(game, "01_SubwayA", 1, 0, error));
        CHECK(game->client_shell.m_bInWorld);
        CHECK(step_app_session(*game, 1. / 60));
        auto& shell = game->client_shell;
        shell.use_down = shell.use_was_down = shell.medkit_was_down = shell.flashlight_was_down = true;
        shell.proximity_was_down = shell.remote_was_down = shell.detonate_was_down = true;
        shell.movement_input.forward = shell.weapon_input.primary = shell.move_mgr.m_bLastJumpCommand = true;
        const auto before_release = game->objects.time();
        neutralize_app_input(shell);
        CHECK(!shell.use_down && !shell.use_was_down && !shell.medkit_was_down && !shell.flashlight_was_down &&
              !shell.proximity_was_down && !shell.remote_was_down && !shell.detonate_was_down &&
              !shell.movement_input.forward && !shell.weapon_input.primary && !shell.move_mgr.m_bLastJumpCommand);
        CHECK(game->objects.time() == before_release);
        auto* previous = game.get();
        const auto player = game->client_shell.player;
        const auto time = game->objects.time();
        CHECK(!GameSession::start_new_game(game, "missing-menu-world", 1, 0, error));
        CHECK(game.get() == previous && game->client_shell.player == player && game->objects.time() == time);
        const auto mixed = game->server.audio.mixed_frames;
        const auto ambient_cursor = audio.mixer.get(audio.ambient)->cursor;
        CHECK(game->server.audio.pause_output(true, error));
        for (int i = 0; i < 180; ++i)
        {
            CHECK(audio.advance(1. / 60));
            CHECK(game->server.audio.advance(1. / 60, {}, {}));
        }
        CHECK(game->objects.time() == time && game->server.audio.mixed_frames == mixed);
        CHECK(audio.mixer.get(audio.ambient)->cursor != ambient_cursor);
        CHECK(game->server.audio.pause_output(false, error));
        CHECK(step_app_session(*game, 1. / 60));
        CHECK(game->objects.time() > time && game->objects.time() < time + .02);
        CHECK(GameSession::start_new_game(game, "01_SubwayA", 1, 0, error));
        CHECK(game->client_shell.m_bInWorld && game->server.audio.has_output());
        audio.focus(false);
        CHECK(!audio.ambient);
        return true;
    }
    // Options > CHEATS on the retail data: the original ProcessCheat branches through CMSG_CHEAT,
    // the persisted god mode, ALL KEYS, the level list, the warp and the game-type refusal.
    bool cheats(const std::filesystem::path& content)
    {
        std::string error;
        Config config;
        config.data_dir = content;
        Resources resources;
        CHECK(resources.mount_game(config, error));
        Menu menu;
        CheatFlow flow;
        menu.set_levels(cheat_levels(resources));
        CHECK(menu.levels.size() == 36 && menu.levels.front() == "01_subwaya" &&
              std::find(menu.levels.begin(), menu.levels.end(), "24b_arena") != menu.levels.end() &&
              std::find(menu.levels.begin(), menu.levels.end(), "credits") != menu.levels.end());
        auto game = std::make_unique<GameSession>();
        game->resources = resources;
        CHECK(game->initialize());
        CHECK(GameSession::start_new_game(game, "01_SubwayA", 2, 0, error));
        CHECK(step_app_session(*game, 1. / 60));
        menu.level = 5;
        flow.update(*game, menu);
        CHECK(menu.level == 0);
        auto* player = game->server_shell.Player(game->local_client);
        CHECK(player && !player->m_damage.m_bGodMode);
        // God mode follows GOD MODE only while CHEATS is on, once per change.
        menu.settings.god_mode = true;
        flow.update(*game, menu);
        CHECK(!player->m_damage.m_bGodMode && flow.sent == 0);
        menu.settings.cheats = true;
        flow.update(*game, menu);
        flow.update(*game, menu);
        CHECK(player->m_damage.m_bGodMode && menu.cheat_status == "You are a GOD." && flow.sent == 1);
        menu.settings.cheats = false;
        flow.update(*game, menu);
        CHECK(!player->m_damage.m_bGodMode && menu.cheat_status == "God mode off. Help!" && flow.sent == 2);
        // KFA: full ammo (strength column), then the source list into the ten slots; the slots
        // fill before the minigun, napalm cannon and sniper rifle, as in the original.
        auto& inventory = player->m_InventoryMgr;
        MenuCommand request;
        request.action = MenuAction::Cheat;
        request.cheat = CHEAT_KFA;
        CHECK(flow.command(*game, menu, request) && menu.cheat_status == "Monolith Rulez...");
        for (unsigned slot = 0; slot < 10; ++slot)
            CHECK(inventory.GetWeapon(slot));
        for (DBYTE weapon : {WEAP_SHOTGUN, WEAP_ASSAULTRIFLE, WEAP_TESLACANNON, WEAP_DEATHRAY})
            CHECK(inventory.HasWeapon(weapon));
        CHECK(!inventory.HasWeapon(WEAP_SNIPERRIFLE));
        for (DBYTE ammo = AMMO_BULLET; ammo <= AMMO_FOCUS; ++ammo)
            CHECK(inventory.GetAmmoCount(ammo) == inventory.MaxAmmo(ammo));
        CHECK(inventory.GetWeapon(10) && inventory.GetAmmoCount(10) == 10 && inventory.GetAmmoCount(12) == 10);
        inventory.SetAmmoCount(AMMO_SHELL, 1);
        request.cheat = CHEAT_AMMO;
        CHECK(flow.command(*game, menu, request) &&
              inventory.GetAmmoCount(AMMO_SHELL) == inventory.MaxAmmo(AMMO_SHELL));
        // HEALTH (CDestructable::Reset), GIVEALLINV.
        player->m_damage.m_fHitPoints = 7;
        player->m_damage.m_fArmorPoints = 0;
        request.cheat = CHEAT_HEALTH;
        CHECK(flow.command(*game, menu, request) && menu.cheat_status == "You have full health.");
        CHECK(player->m_damage.m_fHitPoints == player->m_damage.m_fMaxHitPoints &&
              player->m_damage.m_fArmorPoints == player->m_damage.m_fMaxArmorPoints);
        request.cheat = CHEAT_GIVEALLINV;
        CHECK(flow.command(*game, menu, request));
        CHECK(inventory.medkit && inventory.flashlight && inventory.goggles && inventory.eye && inventory.binoculars);
        // ALL KEYS: the keys this world's pickups still offer, once.
        const auto key = game->objects.create(game->objects.find_class("KeyPickup"), {}, Creation::World,
                                              {{"Name", std::string("cheat_key")},
                                               {"ObjectName", std::string("RedKey")},
                                               {"UseCount", std::int32_t(1)},
                                               {"Pos", DVector{0, 0, 0}},
                                               {"ModelFile", std::string("models/characters/caleb.abc")},
                                               {"ModelSkin", std::string("skins/characters/caleb1.dtx")}});
        CHECK(key);
        request.cheat = CHEAT_ALLKEYS;
        CHECK(flow.command(*game, menu, request) && player->cheat_keys_added >= 1 &&
              inventory.QueryKey("RedKey") == InventoryResult::Ok);
        CHECK(flow.command(*game, menu, request) && player->cheat_keys_added == 1); // Used up, offered again.
        CHECK(flow.command(*game, menu, request) && player->cheat_keys_added == 0 &&
              menu.cheat_status == "No other key in this level.");
        // KILLALLAI: every damageable NPC, active or asleep, receives the original KILL trigger.
        std::vector<HOBJECT> npcs;
        for (auto h = game->server.GetNextObject(nullptr); h; h = game->server.GetNextObject(h))
            npcs.push_back(h);
        for (auto h = game->server.GetNextInactiveObject(nullptr); h; h = game->server.GetNextInactiveObject(h))
            npcs.push_back(h);
        std::erase_if(npcs,
                      [&](HOBJECT h)
                      {
                          const auto* ai = dynamic_cast<AI_Mgr*>(game->server.HandleToObject(h));
                          return !ai || ai->m_damage.m_bDead || ai->m_damage.m_bGodMode || !ai->m_damage.can_damage;
                      });
        CHECK(!npcs.empty());
        request.cheat = CHEAT_KILLALLAI;
        CHECK(flow.command(*game, menu, request) && menu.cheat_status == "...Let Tchernobog sort'em out.");
        for (int i = 0; i < 60; ++i)
            CHECK(step_app_session(*game, 1. / 60));
        for (auto h : npcs)
            if (const auto* ai = dynamic_cast<AI_Mgr*>(game->server.HandleToObject(h)))
                CHECK(ai->m_damage.m_bDead);
        std::cout << "Cheats: " << npcs.size() << " NPCs killed; " << player->cheats_applied << " cheats applied\n";
        // GO TO LEVEL: a new game at the current difficulty; LEVEL follows the world and god mode
        // returns on the new player while the switch asks for it.
        menu.settings.cheats = true;
        request = {};
        request.action = MenuAction::Warp;
        request.world = "02_cityhub";
        request.character = game->client_shell.entry_profile.character;
        request.difficulty = unsigned(game->server.GetVarValueFloat(game->server.GetGameConVar("GAME_DIFFICULTY")));
        CHECK(request.difficulty == 2);
        const auto start = menu_world_start(*game, request, "01_SubwayA");
        CHECK(start.game_type == GAMETYPE_SINGLE && start.difficulty == 2 && !strcmp(start.world, "02_cityhub"));
        CHECK(start_menu_world(game, start, error) && game->world_path == "worlds/02_cityhub.dat");
        CHECK(step_app_session(*game, 1. / 60));
        flow.update(*game, menu);
        player = game->server_shell.Player(game->local_client);
        CHECK(menu.levels[std::size_t(menu.level)] == "02_cityhub" && player && player->m_damage.m_bGodMode);
        // No cheats outside GAMETYPE_SINGLE (IsMultiplayerGame), and a refusal is not repeated.
        CHECK(GameSession::start_new_game(game, "Credits", DIFFICULTY_MEDIUM, 0, error, false, GAMETYPE_CUSTOM));
        const auto sent = flow.sent;
        flow.update(*game, menu);
        flow.update(*game, menu);
        CHECK(flow.sent == sent + 1 && menu.cheat_status == "Cheats are unavailable in this game.");
        request = {};
        request.action = MenuAction::Cheat;
        request.cheat = CHEAT_AMMO;
        CHECK(!flow.command(*game, menu, request) &&
              game->server_shell.Player(game->local_client)->cheats_refused == 2);
        return true;
    }
    std::span<const std::uint8_t> bytes(std::string_view text)
    {
        return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
    }
    std::string_view line(const CCredits& credits, int credit, int string)
    {
        const auto* c = credits.GetCredit(credit);
        const auto* s = c ? c->GetString(string) : nullptr;
        return s ? std::string_view(*s) : std::string_view("<none>");
    }
    // Original CCredits/CCredit parsing, timers, keys and the TEXT resource reader (synthetic).
    bool credits_text()
    {
        std::string error;
        CCredits credits;
        CHECK(!credits.IsInited() && credits.IsDone());
        // "##" separators, leading newlines skipped, blank lines collapse, >TIME: hold override,
        // ">TIME:" alone is text, the block after the last "##" is never added.
        CHECK(credits.Init(bytes("A\r\n\r\nB  \r\n>TIME:1.5\r\n##\r\n\r\nC\n>TIME:\n>TIME:abc\n##D##tail"), CM_CREDITS,
                           true, 640, &error));
        CHECK(credits.IsInited() && !credits.IsDone() && credits.GetNumCredits() == 3);
        CHECK(credits.GetCredit(0)->GetNumStrings() == 2 && line(credits, 0, 0) == "A" && line(credits, 0, 1) == "B  ");
        CHECK(credits.GetCredit(0)->GetHoldTime() == 1.5f);
        CHECK(credits.GetCredit(1)->GetNumStrings() == 2 && line(credits, 1, 1) == ">TIME:");
        CHECK(credits.GetCredit(1)->GetHoldTime() == 0.f); // atof("abc"): not > 0, so the default hold applies.
        CHECK(line(credits, 2, 0) == "D" && credits.font_width == 14 && credits.font_height == 22);
        // ">END" ends parsing at its "##"; a NUL ends the resource; an empty first line is a string.
        CHECK(credits.Init(bytes("\r\nX##>END##Y##"), CM_CREDITS, true, 640, &error) && credits.GetNumCredits() == 1);
        CHECK(credits.GetCredit(0)->GetNumStrings() == 2 && line(credits, 0, 0).empty() && line(credits, 0, 1) == "X");
        CHECK(credits.Init(bytes(std::string_view("P##\0Q##", 7)), CM_CREDITS, true, 640, &error) &&
              credits.GetNumCredits() == 1);
        // MAX_CREDIT_STRINGS 12 (a later >TIME: is also dropped) and MAX_CREDITS 256.
        std::string many;
        for (int i = 0; i < 14; ++i)
            many += "L" + std::to_string(i) + "\n";
        many += ">TIME:9\n##";
        CHECK(credits.Init(bytes(many), CM_CREDITS, true, 640, &error));
        CHECK(credits.GetCredit(0)->GetNumStrings() == 12 && line(credits, 0, 11) == "L11" &&
              credits.GetCredit(0)->GetHoldTime() == -1.f && credits.dropped_strings == 3);
        std::string hundreds;
        for (int i = 0; i < 300; ++i)
            hundreds += "x##";
        CHECK(credits.Init(bytes(hundreds), CM_CREDITS, true, 640, &error));
        CHECK(credits.GetNumCredits() == MAX_CREDITS && credits.dropped_credits == 44);
        // Original fixed buffers overflow; the port fails explicitly with an inited, empty roll.
        CHECK(!credits.Init(bytes(std::string(256, 'w') + "##"), CM_CREDITS, true, 640, &error) && !error.empty());
        CHECK(credits.IsInited() && credits.GetNumCredits() == 0);
        CHECK(!credits.Init(bytes(std::string(1024, 'w') + "##"), CM_CREDITS, true, 640, &error));
        CHECK(credits.Init(bytes(std::string(255, 'w') + "##"), CM_CREDITS, true, 640, &error));
        credits.Update(1);
        CHECK(credits.drawn.credit == nullptr); // Empty Update is harmless.
        CHECK(credits.Init(bytes("a##"), CM_CREDITS, true, 360, &error) && credits.font_height == 14);
        CHECK(credits.Init(bytes("a##"), CM_CREDITS, true, 361, &error) && credits.font_height == 16);

        // Clear-screen state machine: fade-in 0.5 s, hold (>TIME or 4 s), fade-out 0.4 s,
        // hold-out 2 s; red ramp 0..1..0; then the next credit; endless wrap in CM_CREDITS.
        CHECK(credits.Init(bytes("A\n>TIME:1.5\n##B##"), CM_CREDITS, true, 640, &error));
        const auto* first = credits.GetCredit(0);
        credits.Update(0.25f);
        CHECK(first->GetState() == CS_FADEIN && !credits.drawn.credit && first->GetTimer() == 0.5f);
        credits.Update(0.25f);
        CHECK(credits.drawn.credit == first && credits.drawn.color == 0.5f);
        credits.Update(0.25f);
        CHECK(credits.drawn.color == 1.f && first->GetState() == CS_HOLDIN && first->GetTimer() == 1.5f);
        credits.Update(1.f);
        CHECK(credits.drawn.color == 1.f && first->GetTimer() == 0.5f);
        credits.Update(0.5f);
        CHECK(first->GetState() == CS_FADEOUT && first->GetTimer() == 0.4f);
        credits.Update(0.1f);
        CHECK(std::abs(credits.drawn.color - 0.75f) < 1e-6f);
        credits.Update(1.f);
        CHECK(credits.drawn.color == 0.f && first->GetState() == CS_HOLDOUT && first->GetTimer() == 2.f);
        credits.Update(1.5f);
        CHECK(!credits.drawn.credit && first->GetState() == CS_HOLDOUT);
        credits.Update(0.5f);
        CHECK(first->GetState() == CS_DONE && credits.GetCurrentCredit() == 0);
        credits.Update(0.f);
        CHECK(credits.GetCurrentCredit() == 1 && credits.GetCredit(1)->GetState() == CS_START);
        credits.Update(0.f);
        credits.Update(0.5f);
        credits.Update(0.f);
        CHECK(credits.GetCredit(1)->GetState() == CS_HOLDIN && credits.GetCredit(1)->GetTimer() == 4.f);
        // Return/Space/PgDn: next credit straight to HOLDIN; the last wraps to the first.
        credits.HandleInput(CREDITS_VK_RETURN);
        CHECK(credits.GetCurrentCredit() == 0 && first->GetState() == CS_HOLDIN && first->GetTimer() == 1.5f);
        credits.HandleInput(CREDITS_VK_NEXT);
        CHECK(credits.GetCurrentCredit() == 1);
        credits.HandleInput(CREDITS_VK_SPACE);
        CHECK(credits.GetCurrentCredit() == 0 && !credits.IsDone());
        // Speed keys scale the frame time; Pause freezes it; Escape only sets the done flag.
        for (int i = 0; i < 40; ++i)
            credits.HandleInput(CREDITS_VK_UP);
        CHECK(credits.GetSpeed() >= MAX_CREDITS_SPEED - 1e-4f && credits.GetSpeed() < MAX_CREDITS_SPEED + .2f);
        for (int i = 0; i < 40; ++i)
            credits.HandleInput(CREDITS_VK_DOWN);
        CHECK(credits.GetSpeed() <= MIN_CREDITS_SPEED + 1e-4f && credits.GetSpeed() > MIN_CREDITS_SPEED - .2f);
        credits.HandleInput(CREDITS_VK_HOME);
        CHECK(credits.GetSpeed() == 1.f);
        credits.HandleInput(CREDITS_VK_UP);
        credits.HandleInput(CREDITS_VK_UP);
        credits.HandleInput(CREDITS_VK_UP);
        credits.HandleInput(CREDITS_VK_UP);
        credits.HandleInput(CREDITS_VK_UP);
        const float speed = credits.GetSpeed(), before = first->GetTimer();
        credits.Update(0.125f);
        CHECK(std::abs(first->GetTimer() - (before - 0.125f * speed)) < 1e-5f);
        credits.HandleInput(CREDITS_VK_PAUSE);
        CHECK(credits.IsPaused());
        credits.Update(0.5f);
        CHECK(std::abs(first->GetTimer() - (before - 0.125f * speed)) < 1e-5f && credits.drawn.credit == first);
        credits.HandleInput(CREDITS_VK_PAUSE);
        credits.HandleInput(CREDITS_VK_ESCAPE);
        CHECK(credits.IsDone() && !credits.IsPaused());
        // Re-init resets speed and pause.
        credits.HandleInput(CREDITS_VK_PAUSE);
        CHECK(credits.Init(bytes("A##"), CM_CREDITS, true, 640, &error) && credits.GetSpeed() == 1.f &&
              !credits.IsPaused());

        // Roll over the world: the fade-out stops at the 0.1 minimum.
        CHECK(credits.Init(bytes("A##B##"), CM_CREDITS, false, 640, &error) && !credits.IsClearingScreen());
        credits.HandleInput(CREDITS_VK_RETURN);
        credits.HandleInput(CREDITS_VK_RETURN); // Back to credit 0 in HOLDIN (4 s default).
        CHECK(first->GetState() == CS_HOLDIN && first->GetTimer() == 4.f);
        credits.Update(4.f);
        CHECK(first->GetState() == CS_FADEOUT);
        credits.Update(0.375f);
        CHECK(credits.drawn.color == 0.1f && first->GetState() == CS_HOLDOUT && first->GetTimer() == 2.f);

        // Intro mode keys: only the HOLDIN fade-out request; it finishes after the last credit.
        CHECK(credits.Init(bytes("A##"), CM_INTRO, true, 640, &error));
        credits.HandleInput(CREDITS_VK_UP);
        credits.HandleInput(CREDITS_VK_RETURN);
        CHECK(credits.GetSpeed() == 1.f && first->GetState() == CS_START);
        for (int i = 0; i < 4; ++i)
            credits.Update(0.5f);
        CHECK(first->GetState() == CS_HOLDIN);
        credits.HandleInput(CREDITS_VK_SPACE);
        CHECK(first->GetState() == CS_FADEOUT);
        credits.Term();
        CHECK(!credits.IsInited() && credits.GetNumCredits() == 0);

        // Vertical layout: CCredit::Draw centers the block; each string centered on its line.
        CHECK(credit_line_center(240, 1, 0, 22, 4) == 240 && credit_line_center(240, 2, 0, 22, 4) == 227 &&
              credit_line_center(240, 2, 1, 22, 4) == 253 && credit_line_center(240, 3, 0, 22, 4) == 214);

        // Named TEXT resource reader: data only, case-insensitive names, single language.
        const std::string payload = "Name\r\n>TIME:2\r\n##\r\n>END\r\n";
        auto pe = test_strings::text_pe(u"TEXT", u"CREDITS", payload);
        std::vector<std::uint8_t> text;
        CHECK(read_named_resource(pe.b, "text", "Credits", text, error));
        CHECK(std::string(text.begin(), text.end()) == payload);
        CHECK(credits.Init(text, CM_CREDITS, true, 640, &error) && credits.GetNumCredits() == 1 &&
              credits.GetCredit(0)->GetHoldTime() == 2.f);
        CHECK(!read_named_resource(pe.b, "TEXT", "INTRO", text, error) && text.empty() && !error.empty());
        CHECK(!read_named_resource(pe.b, "BITMAP", "CREDITS", text, error));
        CHECK(!read_named_resource(test_strings::text_pe(u"TEXT", u"CREDITS", payload, 2).b, "TEXT", "CREDITS", text,
                                   error));
        CHECK(!read_named_resource(test_strings::text_pe(u"TEXT", u"CR\u00c9DITS", payload).b, "TEXT", "CREDITS", text,
                                   error));
        for (std::size_t i = 0; i < pe.b.size(); ++i)
            CHECK(!read_named_resource(std::span(pe.b).first(i), "TEXT", "CREDITS", text, error));
        auto bad = pe;
        bad.patch(0x200 + 98, 0x10); // Data entry RVA (after the 72-byte directories and two names) before the section.
        CHECK(!read_named_resource(bad.b, "TEXT", "CREDITS", text, error));

        // Input: roll keys are queued for CCredits as well as the game; GS_CREDITS consumes them.
        Menu menu;
        MenuInput input;
        input.controls.focused = true;
        SDL_Event key{};
        key.type = SDL_EVENT_KEY_DOWN;
        key.key.windowID = 7;
        key.key.scancode = SDL_SCANCODE_UP;
        input.roll_credits = true;
        input.event(key, 7, menu, true, true, false, 640, 480);
        key.key.scancode = SDL_SCANCODE_PAGEDOWN;
        key.key.repeat = true;
        input.event(key, 7, menu, true, true, false, 640, 480);
        CHECK(input.credit_keys == std::vector<int>({CREDITS_VK_UP, CREDITS_VK_NEXT}) && !input.paused(menu));
        input.credit_keys.clear();
        key.key.repeat = false;
        key.key.scancode = SDL_SCANCODE_ESCAPE;
        input.event(key, 7, menu, true, true, false, 640, 480);
        CHECK(menu.active && input.credit_keys == std::vector<int>({CREDITS_VK_ESCAPE}) && !input.credits_exit);
        // Keys still reach the roll from the in-world menu, like the original OnKeyDown.
        input.credit_keys.clear();
        key.key.scancode = SDL_SCANCODE_HOME;
        input.event(key, 7, menu, true, true, false, 640, 480);
        CHECK(input.credit_keys == std::vector<int>({CREDITS_VK_HOME}));
        menu.close();
        input.credit_keys.clear();
        input.roll_credits = false;
        input.credits_screen = true;
        CHECK(input.paused(menu));
        key.key.scancode = SDL_SCANCODE_SPACE;
        input.event(key, 7, menu, true, true, false, 640, 480);
        key.key.scancode = SDL_SCANCODE_W;
        input.event(key, 7, menu, true, true, false, 640, 480);
        key.key.scancode = SDL_SCANCODE_ESCAPE;
        input.event(key, 7, menu, true, true, false, 640, 480);
        CHECK(input.credit_keys == std::vector<int>({CREDITS_VK_SPACE}) && input.credits_exit && !menu.active);
        SDL_Event focus{};
        focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        focus.window.windowID = 7;
        input.event(focus, 7, menu, true, true, false, 640, 480);
        CHECK(!menu.active); // Focus loss does not stack the menu over GS_CREDITS.
        return true;
    }
    // Menu Credits -> original MenuCredits world request -> custom-type entry into the credits world.
    bool credits(const std::filesystem::path& content)
    {
        std::string error;
        Config config;
        config.data_dir = content;
        auto game = std::make_unique<GameSession>();
        CHECK(game->resources.mount_game(config, error) && game->initialize());
        Menu menu;
        menu.open(false);
        menu.selected = 3;
        menu.activate();
        auto command = menu.take_command();
        CHECK(command.action == MenuAction::Credits);
        auto start = menu_world_start(*game, command, "01_SubwayA");
        CHECK(std::string_view(start.world) == "Credits" && start.difficulty == DIFFICULTY_MEDIUM &&
              start.character == 0 && start.game_type == GAMETYPE_CUSTOM);
        CHECK(start_menu_world(game, start, error));
        CHECK(game->world_path == "worlds/credits.dat" && game->client_shell.m_bInWorld &&
              game->client_shell.m_bRollCredits);
        CHECK(game->server.GetVarValueFloat(game->server.GetGameConVar("GAME_GAMETYPE")) == GAMETYPE_CUSTOM);
        CHECK(game->server.GetVarValueFloat(game->server.GetGameConVar("GAME_DIFFICULTY")) == DIFFICULTY_MEDIUM);
        CHECK(game->startup.accepted && game->startup.player_entered && game->startup.unknown == 0 &&
              game->startup.failed == 0);
        // The installed cres.dll TEXT/CREDITS: 87 credits before ">END"; IDS_CREDITSFONT (206)
        // names the GDI font the original requested.
        {
            std::vector<std::uint8_t> dll, text;
            StringResources strings;
            CHECK(game->resources.read("cres.dll", dll, error) &&
                  read_named_resource(dll, "TEXT", "CREDITS", text, error));
            CHECK(read_string_resources(dll, strings, error) && strings.strings.at(IDS_CREDITSFONT) == "Arial");
            CCredits parsed;
            CHECK(parsed.Init(text, CM_CREDITS, true, 640, &error) && parsed.GetNumCredits() == 87);
            CHECK(parsed.dropped_credits == 0 && parsed.dropped_strings == 0);
            CHECK(parsed.GetCredit(0)->GetNumStrings() == 1 && line(parsed, 0, 0) == " " &&
                  parsed.GetCredit(0)->GetHoldTime() == 0.1f);
            CHECK(line(parsed, 1, 0) == "Blood 2: The Chosen" &&
                  line(parsed, 1, 1) == "is powered/transfused by LithTech" &&
                  parsed.GetCredit(1)->GetHoldTime() == 4.f);
            CHECK(line(parsed, 86, 0) == "Monolith still rulez!!!" && parsed.GetCredit(86)->GetHoldTime() == 5.f);
            int strings_total = 0;
            for (int i = 0; i < parsed.GetNumCredits(); ++i)
            {
                strings_total += parsed.GetCredit(i)->GetNumStrings();
                CHECK(parsed.GetCredit(i)->GetHoldTime() > 0); // Every installed block sets >TIME.
            }
            CHECK(strings_total > 87 * 2);
        }
        // Roll over the credits world: Init on the first frame, then two CCredits::Update
        // calls per frame (GS_PLAYING branch and the post-RenderCamera [blg] call).
        auto& roll = game->client_shell.m_Credits;
        CHECK(!roll.IsInited());
        const auto time = game->objects.time();
        CHECK(step_app_session(*game, 1. / 60));
        CHECK(roll.IsInited() && !roll.IsClearingScreen() && roll.GetNumCredits() == 87 && roll.updates == 1 &&
              roll.font_name == "Arial" && game->client_shell.credits_failures == 0);
        CHECK(step_app_session(*game, 1. / 60) && roll.updates == 3);
        // Credit 0: 0.5 s fade-in + 0.1 s hold + 0.4 s fade-out (0.1 floor) + 2 s hold-out,
        // at double rate: about 1.5 s of play, then credit 1 fades in.
        bool drew_second = false;
        for (int i = 0; i < 298; ++i)
        {
            CHECK(step_app_session(*game, 1. / 60));
            if (roll.drawn.credit == roll.GetCredit(1))
            {
                drew_second = true;
                CHECK(roll.drawn.color >= 0 && roll.drawn.color <= 1);
            }
            CHECK(roll.drawn.credit == nullptr || roll.drawn.color >= 0.1f ||
                  roll.drawn.credit->GetState() == CS_FADEIN);
        }
        CHECK(drew_second && roll.GetCurrentCredit() >= 1 && roll.updates == 599);
        CHECK(game->objects.time() > time + 4.9 && game->client_shell.m_bInWorld);
        // In-world menu Credits: the text-only screen re-inits the same CCredits with a clear.
        CHECK(game->client_shell.InitCredits(true, error) && roll.IsClearingScreen() && roll.GetCurrentCredit() == 0);
        roll.Update(1.f / 60);
        CHECK(roll.GetCredit(0)->GetState() == CS_FADEIN);
        roll.Term(); // Esc: Term; the next play frame starts the roll from the first credit.
        CHECK(step_app_session(*game, 1. / 60) && roll.IsInited() && !roll.IsClearingScreen() && roll.updates == 1);
        auto* player = game->server_shell.Player(game->local_client);
        CHECK(player && !player->m_damage.IsDead());
        // Original Esc in the credits world opens the in-world main menu; Resume returns to it.
        menu.open(true, true);
        CHECK(menu.active && menu.page == MenuPage::Main && menu.enabled(menu.items[3]));
        menu.back();
        CHECK(!menu.active && menu.take_command().action == MenuAction::Resume);
        // The Credits request keeps the current configured character (no SetCharacterInfo).
        CHECK(GameSession::start_new_game(game, "01_SubwayA", 1, 2, error));
        CHECK(!game->client_shell.m_bRollCredits);
        start = menu_world_start(*game, command, "01_SubwayA");
        CHECK(start.character == 2);
        // Failures are explicit and retain the current session.
        auto* previous = game.get();
        CHECK(!GameSession::start_new_game(game, "Credits", 1, 0, error, false, GAMETYPE_BLOODBATH) && !error.empty() &&
              game.get() == previous);
        start.world = "missing-credits-world";
        CHECK(!start_menu_world(game, start, error) && !error.empty() && game.get() == previous);
        WorldMessage message;
        WorldRequest request;
        CHECK(original_world_request("Credits", GAMETYPE_CUSTOM, LOADTYPE_NEW_LEVEL, 1, message));
        CHECK(!parse_world_request(message, request, error));
        CHECK(original_world_request("Credits", GAMETYPE_CUSTOM, LOADTYPE_NEW_GAME, 1, message));
        CHECK(parse_world_request(message, request, error) && request.filename == "worlds/credits.dat");
        CHECK(start_menu_world(game, menu_world_start(*game, command, "01_SubwayA"), error));
        CHECK(game->client_shell.m_bRollCredits && game->server_shell.Player(game->local_client)->m_nCharacter == 2);
        return true;
    }
    bool profiles(const std::filesystem::path& content)
    {
        std::string error;
        Config config;
        config.data_dir = content;
        auto game = std::make_unique<GameSession>();
        CHECK(game->resources.mount_game(config, error) && game->initialize());
        for (unsigned c = 0; c < 4; ++c)
        {
            CHECK(GameSession::start_new_game(game, "01_SubwayA", DBYTE(c % 3), DBYTE(c), error));
            auto* player = game->server_shell.Player(game->local_client);
            const auto& p = character_profiles[c];
            CHECK(player && player->m_nCharacter == c && player->eye_level == p.eye);
            CHECK(player->m_nAttribStrength == p.strength && player->m_nAttribSpeed == p.speed &&
                  player->m_nAttribResistance == p.resistance && player->m_nAttribMagic == p.magic);
            CHECK(game->server.GetVarValueFloat(game->server.GetGameConVar("GAME_DIFFICULTY")) == c % 3);
            CHECK(game->step(1. / 60));
            CHECK(test_camera::complete_subway_intro(*game));
            CHECK(player->m_Anim_Sound.GetSoundRoot() == p.sound_root);
            player->m_eLastSurface = SURFTYPE_STONE;
            std::string foot;
            CHECK(player->GetFootstepSound(foot));
            CHECK(foot.ends_with(p.female ? "f.wav" : "m.wav"));
            CHECK(player->PlayPlayerSound(foot, 400, 70, false, 0));
            CHECK(player->GetLandingSound(foot) && player->PlayPlayerSound(foot, 400, 70, false, 0));
            game->client_shell.movement_input.crouch = true;
            for (int i = 0; i < 10; ++i)
                CHECK(game->step(1. / 60));
            CHECK(player->crouched && player->eye_level == 12);
            game->client_shell.movement_input = {};
            for (int i = 0; i < 10; ++i)
                CHECK(game->step(1. / 60));
            CHECK(!player->crouched && player->eye_level == p.eye);
            CHECK(player->m_InventoryMgr.ObtainWeapon(WEAP_BERETTA, 1) == InventoryResult::Ok);
            CHECK(game->client_shell.SyncWeapons(game->models, game->resources, game->server_shell, error));
            for (const auto& hand : game->client_shell.weapons)
                CHECK(hand.model && hand.character == c &&
                      hand.skin_path.find(std::string(1, p.hands) + "_") != std::string::npos);
            auto& voices = game->client_shell.voice_manager;
            CHECK(!voices.groups[c][11].sounds.empty());
            CHECK(voices.PlaySound(player->m_hObject, voices.groups[c][11].sounds.front(), false));
            CHECK(game->server.audio.get(voices.voice) && game->server.audio.get(voices.voice)->clip->frames() > 0);
            CHECK(game->command_weapon(0, error));
            const auto& weapon = game->client_shell.weapons[0];
            CHECK(weapon.character == c && weapon.skin_path.find(std::string(1, p.hands) + "_") != std::string::npos);
            CHECK(weapon.model_path.find(std::string(1, p.hands) + "_") != std::string::npos);
            PlayerRestoreState saved;
            CHECK(game->capture_player(saved, error) && game->leave_player() && game->restore_player(saved, error));
            CHECK(game->server_shell.Player(game->local_client)->m_nCharacter == c);
            WorldMessage message;
            CHECK(original_world_request("02_CityHub", 0, LOADTYPE_NEW_LEVEL, 1, message));
            CHECK(game->queue_level_change(message, "01_egressa", error));
            CHECK(GameSession::process_level_change(game, error) == LevelChangeResult::Committed);
            player = game->server_shell.Player(game->local_client);
            CHECK(player->m_nCharacter == c && player->eye_level == p.eye);
            CHECK(game->server.GetVarValueFloat(game->server.GetGameConVar("GAME_DIFFICULTY")) == c % 3);
            DVector pos;
            CHECK(game->server.GetObjectPos(player->m_hObject, &pos) == DE_OK);
            auto damage = game->server.StartMessageToObject(player, player->m_hObject, damage_message);
            CHECK(damage && game->server.WriteDamage(
                                damage, {{0, 0, 1}, pos, 10000, DamageType::Normal, player->m_hObject}) == DE_OK);
            CHECK(game->server.EndMessage(damage) == DE_OK && game->step(1. / 60));
            CHECK(player->m_bDead && player->death_error.empty() && game->client_shell.death_camera.active);
            std::cout << "Profile " << c << ": resources, crouch, hands, restore, CityHub transfer and death passed\n";
        }
        return true;
    }
    // Original _DEMO CMenuMain/CMenuSinglePlayer branches and Splash_OnKeyDown.
    bool artwork(const std::filesystem::path& content, bool gpu, const std::filesystem::path& output)
    {
        std::string error;
        Resources resources;
        Config config;
        config.data_dir = content;
        CHECK(resources.mount_game(config, error));
        Menu menu;
        menu.open(true);
        MenuCanvas canvas(resources);
        CHECK(canvas.initialize(error));
        Runtime runtime;
        HudCanvas fonts(resources);
        HudFont large;
        const auto& art = menu_resources(MenuEdition::Retail);
        CHECK(fonts.load_font(art.large_bitmaps[0], art.large_font, large, 1) && large.height == 31);
        if (gpu)
            CHECK(runtime.initialize(true, error));
        std::unique_ptr<UiRuntime> ui;
        std::unique_ptr<MenuRenderer> renderer;
        if (gpu)
        {
            ui = std::make_unique<UiRuntime>(runtime);
            CHECK(ui->initialize(error));
            renderer = std::make_unique<MenuRenderer>(runtime.gpu);
            std::cout << "GPU: "
                      << SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu), SDL_PROP_GPU_DEVICE_NAME_STRING,
                                               "unknown")
                      << '\n';
            std::filesystem::create_directories(output);
        }
        menu.controls = control_menu_rows(InputBindings::defaults());
        menu.vr = true; // The XR Options row and VR page fit the original canvas too.
        menu.setup = true;
        menu.settings.cheats = true; // The Cheats page with its rows enabled and a level.
        menu.set_levels({"01_subwaya"});
        for (unsigned page = 0; page <= unsigned(MenuPage::Quit); ++page)
        {
            menu.build(MenuPage(page));
            if (menu.page == MenuPage::Controls)
            {
                // Every original action string resolves; the list scrolls inside its 280 pixels.
                CHECK(menu.items.size() == control_rows.size() + 1);
                menu.selected = int(menu.items.size()) - 1;
                ++menu.revision;
                CHECK(canvas.compose(menu, 0, error) && menu.first > 0);
                for (std::size_t i = 0; i < menu.items.size(); ++i)
                {
                    const auto& item = menu.items[i];
                    CHECK(!item.literal.empty() && item.rect.bottom <= 420);
                    CHECK((item.rect.bottom > 0) == (int(i) >= menu.first && item.rect.top >= 140));
                    std::cout << item.literal << " = "
                              << (i < menu.controls.size() ? menu.controls[i].binding : std::string{}) << '\n';
                }
                menu.selected = 0;
                menu.binding = 0;
                ++menu.revision;
                CHECK(canvas.compose(menu, 0, error) && menu.first == 0);
            }
            CHECK(canvas.compose(menu, 0, error));
            const auto rev = canvas.revision;
            CHECK(canvas.compose(menu, 0, error) && canvas.revision == rev);
            CHECK(canvas.surface->rgba.size() == 640 * 480 * 4);
            if (menu.page == MenuPage::Load)
                for (unsigned i = 0; i < 2; ++i)
                    CHECK(menu.items[i].literal.ends_with("<EMPTY SLOT>") &&
                          menu.items[i].literal.find('%') == std::string::npos);
            for (const auto& item : menu.items)
                CHECK(item.rect.bottom <= 425 && item.rect.right <= 625);
            if (!gpu)
                continue;
            SDL_Event event;
            while (SDL_PollEvent(&event))
                ImGui_ImplSDL3_ProcessEvent(&event);
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            CHECK(renderer->draw(canvas, error));
            ImGui::Render();
            auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            CHECK(commands);
            SDL_GPUTexture* swapchain = nullptr;
            Uint32 w = 0, h = 0;
            CHECK(SDL_WaitAndAcquireGPUSwapchainTexture(commands, runtime.window, &swapchain, &w, &h) && swapchain);
            Capture capture;
            CHECK(capture.initialize(runtime.gpu, SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window), w, h));
            ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
            SDL_GPUColorTargetInfo color{};
            color.texture = capture.texture;
            color.load_op = SDL_GPU_LOADOP_CLEAR;
            color.store_op = SDL_GPU_STOREOP_STORE;
            auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
            CHECK(pass);
            ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
            SDL_EndGPURenderPass(pass);
            CHECK(capture.enqueue(commands, swapchain, w, h));
            CHECK(SDL_SubmitGPUCommandBuffer(commands));
            CHECK(capture.save(path_text(output / ("menu-" + std::to_string(page) + ".png")).c_str()));
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string mode = argv[1];
    if (mode == "preferences")
        return test_menu_preferences(argc - 1, argv + 1);
    if (mode == "state")
        return state() ? 0 : 1;
    if (mode == "credits_text")
        return credits_text() ? 0 : 1;
    if (argc < 3)
        return 2;
    if (mode == "profiles")
        return profiles(path_from_utf8(argv[2])) ? 0 : 1;
    if (mode == "credits")
        return credits(path_from_utf8(argv[2])) ? 0 : 1;
    if (mode == "session")
        return session(path_from_utf8(argv[2])) ? 0 : 1;
    if (mode == "cheats")
        return cheats(path_from_utf8(argv[2])) ? 0 : 1;
    return artwork(path_from_utf8(argv[2]), mode == "gpu", argc > 3 ? path_from_utf8(argv[3]) : std::filesystem::path{})
               ? 0
               : 1;
}
