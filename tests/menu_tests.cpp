#include "app/menu_input.h"
#include "app/menu_renderer.h"
#include "app/session_playback.h"
#include "app/ui_runtime.h"
#include "camera_fixture.h"
#include "engine/capture.h"
#include "game/menu_audio.h"
#include "game/original/CharacterProfiles.h"
#include "game/original/MenuIds.h"
#include "game/server_adapter.h"
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
        CHECK(menu.page == MenuPage::Display && menu.items.size() == 2 && menu.small());
        CHECK(menu.items[0].setting == 6 && menu.items[0].string_id == IDS_MENU_DISPLAY_DETAIL_HIGH &&
              menu.items[1].setting == 7 && !menu.items[1].string_id);
        menu.take_command();
        menu.selected = 0;
        menu.activate();
        CHECK(menu.settings.detail == 0 && menu.items[0].string_id == IDS_MENU_DISPLAY_DETAIL_LOW &&
              menu.take_command().action == MenuAction::Settings);
        menu.adjust(-1);
        CHECK(menu.settings.detail == 2 && menu.items[0].string_id == IDS_MENU_DISPLAY_DETAIL_HIGH);
        menu.adjust(-1);
        CHECK(menu.settings.detail == 1 && menu.items[0].string_id == IDS_MENU_DISPLAY_DETAIL_MEDIUM);
        menu.selected = 1;
        menu.activate();
        CHECK(menu.settings.pixel_lighting && menu.take_command().action == MenuAction::Settings);
        menu.adjust(1);
        CHECK(!menu.settings.pixel_lighting);
        menu.back();
        CHECK(menu.page == MenuPage::Options && !strcmp(menu.title(), "options.pcx"));
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
        for (unsigned page = 0; page <= unsigned(MenuPage::Quit); ++page)
        {
            menu.build(MenuPage(page));
            CHECK(canvas.compose(menu, 0, error));
            const auto rev = canvas.revision;
            CHECK(canvas.compose(menu, 0, error) && canvas.revision == rev);
            CHECK(canvas.surface->rgba.size() == 640 * 480 * 4);
            if (menu.page == MenuPage::Load)
                for (unsigned i = 0; i < 2; ++i)
                    CHECK(menu.items[i].literal.ends_with("unavailable") &&
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
    if (argc < 3)
        return 2;
    if (mode == "profiles")
        return profiles(path_from_utf8(argv[2])) ? 0 : 1;
    if (mode == "session")
        return session(path_from_utf8(argv[2])) ? 0 : 1;
    return artwork(path_from_utf8(argv[2]), mode == "gpu", argc > 3 ? path_from_utf8(argv[3]) : std::filesystem::path{})
               ? 0
               : 1;
}
