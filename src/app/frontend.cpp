#include "app/frontend.h"
#include "app/hud.h"
#include "app/menu_input.h"
#include "app/menu_renderer.h"
#include "app/menu_smoke.h"
#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "app/ui_runtime.h"
#include "engine/capture.h"
#include "game/menu_audio.h"
#include "game/menu_preferences.h"
#if BLOOD2_INSPECTOR
#include "app/inspector.h"
#endif
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <spdlog/spdlog.h>
#include <utility>

namespace OpenBlood2
{
    int run_frontend(const Options& options, const Config& config, const DataStatus& data)
    {
        std::string error;
        auto failed = [&](int code)
        {
            spdlog::error("Frontend: {}", error);
            return code;
        };
        Runtime runtime;
#ifdef NDEBUG
        constexpr bool gpu_debug = false;
#else
        constexpr bool gpu_debug = true;
#endif
        if (!runtime.initialize(gpu_debug, error))
            return failed(3);
        spdlog::info("GPU device: {}", SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu),
                                                             SDL_PROP_GPU_DEVICE_NAME_STRING, "unreported"));
        UiRuntime ui(runtime);
        if (!ui.initialize(error))
            return failed(3);
        Resources artwork;
        if (!artwork.mount_game(config, error))
            return failed(5);
        Menu menu;
        std::filesystem::path preferences;
        std::string preference_error;
        if (!options.menu_smoke && (!menu_preferences_path(preferences, preference_error) ||
                                    !load_menu_preferences(preferences, menu.settings, preference_error)))
            spdlog::warn("Preferences: {}", preference_error);
        menu.demo = config.demo;
        MenuCanvas canvas(artwork);
        if (!canvas.initialize(error))
            return failed(5);
        MenuRenderer menu_renderer(runtime.gpu);
        MenuAudio menu_audio(artwork);
        menu_audio.random_state = std::uint32_t(SDL_GetTicksNS());
        if (!menu_audio.mixer.open_output(error))
        {
            menu_audio.error = error;
            spdlog::warn("Menu audio output: {}", error);
        }
        auto game = std::make_unique<GameSession>();
        game->resources = artwork;
        if (!game->initialize())
        {
            error = "Cannot initialize original game services";
            return failed(4);
        }
        if (!game->server.audio.open_output(error))
            spdlog::warn("Game audio output: {}", error);
        if (options.view_world)
        {
            spdlog::info("Loading world: {} (direct entry)", options.world);
            if (!game->request_world(options.world, error) || !game->run_world(error))
                return failed(6);
            spdlog::info("World ready: {}", game->world_path);
        }
        else
            menu.open(false);
        if (!preference_error.empty())
            menu.open(game->client_shell.m_bInWorld);
        menu.error = preference_error;
        auto stored_settings = menu.settings;
        MenuInput input;
        input.controls.focused = (SDL_GetWindowFlags(runtime.window) & SDL_WINDOW_INPUT_FOCUS) != 0;
        input.reset(menu, game->client_shell.m_bInWorld);
        ObjectSelection selection;
        ScenePresenter presenter(runtime.gpu);
        presenter.model_shadows = options.model_shadows;
        HudRenderer hud(runtime.gpu);
#if BLOOD2_INSPECTOR
        Inspector inspector(runtime);
        inspector.world_mesh = &presenter.world_mesh;
        inspector.model_view = &presenter.model_scene;
        inspector.explosion_model_view = &presenter.explosion_model_scene;
        inspector.polygrid_view = &presenter.polygrid_scene;
        inspector.polygrid_render = &presenter.polygrid_renderer.stats;
        inspector.sprite_view = &presenter.sprite_scene;
        inspector.sprite_render = &presenter.sprite_renderer.stats;
        inspector.world_render = &presenter.world_renderer.stats;
        inspector.presentation = &presenter.stats;
        inspector.gpu_resources = &presenter.registry.stats;
        inspector.model_lighting = &presenter.model_lighting.stats;
        inspector.model_shadows = &presenter.shadow_stats;
#else
        (void)data;
#endif
        MenuSmoke smoke;
        bool done = false, captured = false, dismiss_error = false;
        std::uint64_t frames = 0;
        auto previous = SDL_GetTicksNS();
        ObjectId controlled{};
        std::string reported_audio;
        while (!done && (!options.frame_limit || frames < options.frame_limit))
        {
            const bool paused_at_start = menu.active;
            const auto tick = SDL_GetTicksNS();
            const double delta = options.menu_smoke ? 1. / 60 : app_frame_delta(double(tick - previous) / 1e9);
            previous = tick;
            if (menu.active && !canvas.compose(menu, 0, error))
                return failed(4);
            if (options.menu_smoke && !smoke.before(frames, runtime.window, *game, menu, input, menu_audio, error))
                return failed(4);
            SDL_Event event;
            int width = 0, height = 0;
            SDL_GetWindowSize(runtime.window, &width, &height);
            while (SDL_PollEvent(&event))
            {
                if (options.menu_smoke && MenuSmoke::unrelated(event))
                    continue;
                if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                                     event.window.windowID == SDL_GetWindowID(runtime.window)))
                {
                    done = true;
                    break;
                }
                if (!menu.error.empty())
                {
                    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
                        input.event(event, SDL_GetWindowID(runtime.window), menu, game->client_shell.m_bInWorld, false,
                                    BLOOD2_INSPECTOR && options.inspector, float(width), float(height));
                    ImGui_ImplSDL3_ProcessEvent(&event);
                    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                        event.key.scancode == SDL_SCANCODE_ESCAPE)
                        dismiss_error = true;
                    continue;
                }
                auto* player = game->server_shell.Player(game->local_client);
                const bool old_inspector = input.inspector, old_menu = menu.active;
                const bool forward = input.event(event, SDL_GetWindowID(runtime.window), menu,
                                                 game->client_shell.m_bInWorld, player && !player->m_damage.IsDead(),
                                                 BLOOD2_INSPECTOR && options.inspector, float(width), float(height));
                if (old_inspector != input.inspector || old_menu != menu.active)
                {
                    neutralize_app_input(game->client_shell);
                    ImGui::GetIO().ClearEventsQueue();
                    ImGui::GetIO().ClearInputKeys();
                    ImGui::GetIO().ClearInputMouse();
                }
                // SDL backend still needs platform/focus/resize events when its windows are hidden.
                const bool device_input = event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP ||
                                          event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_MOUSE_MOTION ||
                                          event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                                          event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                                          event.type == SDL_EVENT_MOUSE_WHEEL;
                if (forward || !device_input)
                    ImGui_ImplSDL3_ProcessEvent(&event);
            }
            if (done)
                break;
            // Freeze queued game output before any potentially slow preparation.
            if (!game->server.audio.pause_output(menu.active, error))
                return failed(4);
            auto command = menu.take_command();
            if (command.action == MenuAction::Quit)
                break;
            if (command.action == MenuAction::NewGame)
            {
                const char* world = first_level(config.demo);
                spdlog::info("New game requested: world={}; character={}; difficulty={}", world,
                             command.story ? 0 : int(command.character), int(command.difficulty));
                // Execute the owned profile request outside SDL event dispatch.
                if (!GameSession::start_new_game(game, world, DBYTE(command.difficulty),
                                                 DBYTE(command.story ? 0 : command.character), error))
                {
                    menu.error = error;
                    spdlog::error("New game failed: {}", error);
                }
                else
                {
                    spdlog::info("World ready: {}", game->world_path);
                    menu.error.clear();
                    menu.close();
                    selection.id = {};
                    controlled = {};
#if BLOOD2_INSPECTOR
                    inspector.forget_world();
#endif
                    input.reset(menu, true);
                }
                previous = SDL_GetTicksNS();
            }
#if BLOOD2_INSPECTOR
            if (auto level = std::exchange(inspector.start_level_request, {}); !level.empty())
            {
                spdlog::info("Inspector level requested: world={}; difficulty={}", level,
                             inspector.start_level_difficulty);
                if (!GameSession::start_new_game(game, level, DBYTE(inspector.start_level_difficulty), 0, error))
                {
                    inspector.world_error = error;
                    spdlog::error("Inspector level failed: {}", error);
                }
                else
                {
                    spdlog::info("World ready: {}", game->world_path);
                    inspector.world_error.clear();
                    menu.close();
                    selection.id = {};
                    controlled = {};
                    inspector.forget_world();
                    input.reset(menu, true);
                }
                previous = SDL_GetTicksNS();
            }
#endif
            if (stored_settings != menu.settings)
            {
                stored_settings = menu.settings;
                if (!options.menu_smoke && !save_menu_preferences(preferences, menu.settings, error))
                {
                    menu.error = error;
                    spdlog::error("Save preferences failed: {}", error);
                }
            }
            const auto& settings = menu.settings;
            const float sound_gain = settings.sound ? settings.sound_volume / 50.f : 0;
            // Original SetMusicVolume takes 0..100 (100 = full); the original menu slider stops at 50.
            const float music_gain = settings.music ? settings.music_volume / 100.f : 0;
            if (!game->server.audio.set_gains(sound_gain, music_gain) ||
                !menu_audio.mixer.set_gains(sound_gain, music_gain))
            {
                error = "Cannot apply audio preferences";
                return failed(4);
            }
            input.controls.mouse_sensitivity = .0025f * (settings.sensitivity / 150.f);
            input.controls.invert_y = settings.invert_y;
            // Original detail*.cfg: LightMap 0 at low detail, MaxModelShadows 1 at high detail.
            presenter.lightmaps = settings.detail >= 1;
            presenter.model_shadows = options.model_shadows && settings.detail >= 2;
            presenter.per_pixel_lighting = settings.pixel_lighting;
            menu_audio.focus(menu.active);
            for (auto cue : menu.cues)
                menu_audio.play(cue);
            menu.cues.clear();
            menu_audio.advance(delta);
            if (menu_audio.error != reported_audio)
            {
                reported_audio = menu_audio.error;
                if (!reported_audio.empty())
                {
                    spdlog::warn("Menu audio: {}", reported_audio);
                    menu.audio_failed = true;
                    ++menu.revision;
                }
            }
            if (!game->server.audio.pause_output(menu.active, error))
                return failed(4);
            auto& shell = game->client_shell;
            if (menu.active || input.inspector)
                neutralize_app_input(shell);
            shell.movement_input = {};
            shell.weapon_input = {};
            shell.use_down = shell.medkit_down = shell.flashlight_down = shell.proximity_down = shell.remote_down =
                shell.detonate_down = false;
#if BLOOD2_INSPECTOR
            inspector.apply_commands(*game, selection);
            if (std::exchange(inspector.play_request, false) && shell.m_bInWorld)
            {
                menu.close();
                input.inspector = false;
                input.reset(menu, true);
                if (!game->server.audio.pause_output(false, error))
                    return failed(4);
            }
#endif
            const auto* object = game->objects.get(shell.player);
            const auto id = object ? object->id : ObjectId{};
            if (id != controlled)
            {
                controlled = id;
                input.reset(menu, bool(id));
            }
            const bool blocked = menu.active || input.inspector;
            input.controls.enabled = !blocked;
            if (!input.controls.sync_mouse(runtime.window, shell.m_bInWorld, blocked, error))
            {
                menu.open(shell.m_bInWorld);
                menu.error = "Cannot capture mouse: " + error;
                input.reset(menu, shell.m_bInWorld);
            }
            if (!blocked && !menu.active)
            {
                shell.movement_input = input.controls.sample(false);
                const auto fire = input.controls.sample_fire(false);
                shell.weapon_input.primary = fire.primary;
                shell.weapon_input.alternate = fire.alternate;
                shell.use_down = input.controls.sample_use(false);
                shell.medkit_down = input.controls.sample_medkit(false);
                shell.flashlight_down = input.controls.sample_flashlight(false);
                shell.binoculars_down = input.controls.sample_binoculars(false);
                shell.proximity_down = input.controls.sample_proximity(false);
                shell.remote_down = input.controls.sample_remote(false);
                shell.detonate_down = input.controls.sample_detonate(false);
                if (const int slot = input.controls.sample_weapon_slot(false); slot >= 0)
                    if (!game->command_weapon(DBYTE(slot), error))
                        spdlog::info("Weapon selection: {}", error);
                input.controls.look(shell.rotation, delta, false, shell.m_bZoomView);
            }
            if (!menu.active && !paused_at_start)
            {
                if (!step_app_session(*game, delta))
                {
                    spdlog::error("Simulation failed: world={}, frame={}, delta={}, time={}", game->world_path, frames,
                                  delta, game->objects.time());
                    for (const auto& diagnostic : game->objects.diagnostics())
                        spdlog::error("Object: {}", diagnostic);
                    for (const auto& diagnostic : game->trace)
                        spdlog::error("Session: {}", diagnostic);
                    for (const auto& diagnostic : game->step_errors())
                        spdlog::error("{}", diagnostic);
                    error = "Simulation step failed";
                    return failed(4);
                }
                const auto change = GameSession::process_level_change(game, error);
                if (change == LevelChangeResult::Committed)
                {
                    spdlog::info("NEW_LEVEL committed: {} -> {} / start {}", game->level_change.source_world,
                                 game->world_path, game->level_change.selected_start);
                    selection.id = {};
                    controlled = {};
                    input.reset(menu, true);
#if BLOOD2_INSPECTOR
                    inspector.forget_world();
#endif
                    previous = SDL_GetTicksNS();
                }
                else if (change == LevelChangeResult::Rejected)
                    spdlog::warn("Level change retained world: {}", error);
            }
            selection.refresh(game->objects);
            if (!menu.active && !presenter.update(*game, error))
                return failed(5);
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
            if (input.inspector)
                ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::NewFrame();
            if (menu.active)
            {
                if (!canvas.compose(menu, 0, error) || !menu_renderer.draw(canvas, error))
                    return failed(4);
            }
            else
            {
                draw_screen_fade(game->client_shell.fade.Scale(), game->client_shell.m_bInWorld);
                const auto& client = game->client_shell;
                if (!hud.draw(hud_state(client.stats, client.m_bInWorld, client.view.object != nullptr,
                                        client.death_camera.active, client.m_bShowCrosshair, client.m_nCrosshair),
                              game->resources, error))
                    return failed(4);
            }
            if (!menu.error.empty())
            {
                ImGui::OpenPopup("Unable to complete request");
                if (ImGui::BeginPopupModal("Unable to complete request", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::PushTextWrapPos(500);
                    ImGui::TextWrapped("%s", menu.error.c_str());
                    ImGui::PopTextWrapPos();
                    if (ImGui::Button("Close") || std::exchange(dismiss_error, false))
                    {
                        menu.error.clear();
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }
#if BLOOD2_INSPECTOR
            inspector.hud_status = hud.status_bar();
            inspector.mouse_captured = input.controls.mouse_captured;
            inspector.waiting_for_player = waiting_for_player(*game);
            const bool was_visible = input.inspector;
            inspector.draw(config, data, input.inspector, frames, *game, selection);
            if (was_visible && !input.inspector)
                input.reset(menu, game->client_shell.m_bInWorld);
#endif
            ImGui::Render();
            auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            if (!commands)
            {
                error = SDL_GetError();
                return failed(3);
            }
            SDL_GPUTexture* target = nullptr;
            Uint32 w = 0, h = 0;
            if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, runtime.window, &target, &w, &h))
            {
                error = SDL_GetError();
                SDL_CancelGPUCommandBuffer(commands);
                return failed(3);
            }
            Capture capture;
            const bool phase_capture = options.menu_smoke && (frames == 8 || frames == 17 || frames == 23 ||
                                                              frames == 120 || frames == 208 || frames == 214);
            const bool save = target && ((frames >= 2 && !captured) || phase_capture) && !options.capture_file.empty();
            if (save &&
                !capture.initialize(runtime.gpu, SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window), w, h))
            {
                error = SDL_GetError();
                SDL_CancelGPUCommandBuffer(commands);
                return failed(3);
            }
            if (target)
            {
                auto* color_target = save ? capture.texture : target;
                const bool world = !menu.active && presenter.render_world;
                if (world &&
                    !presenter.record(*game, commands, color_target,
                                      SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window), w, h, error))
                {
                    SDL_CancelGPUCommandBuffer(commands);
                    return failed(3);
                }
                ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
                SDL_GPUColorTargetInfo color{};
                color.texture = color_target;
                color.load_op = world ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
                color.store_op = SDL_GPU_STOREOP_STORE;
                auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
                if (!pass)
                {
                    error = SDL_GetError();
                    SDL_CancelGPUCommandBuffer(commands);
                    return failed(3);
                }
                ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
                SDL_EndGPURenderPass(pass);
                if (save && !capture.enqueue(commands, target, w, h))
                {
                    error = SDL_GetError();
                    SDL_CancelGPUCommandBuffer(commands);
                    return failed(3);
                }
            }
            if (!SDL_SubmitGPUCommandBuffer(commands))
            {
                error = SDL_GetError();
                return failed(3);
            }
            if (save)
            {
                const auto capture_path =
                    phase_capture ? options.capture_file.parent_path() /
                                        (path_text(options.capture_file.stem()) + "-" + std::to_string(frames) + ".png")
                                  : options.capture_file;
                if (!capture.save(path_text(capture_path).c_str()))
                {
                    error = SDL_GetError();
                    return failed(3);
                }
                captured = true;
            }
            ++frames;
            if (!target)
                SDL_Delay(10);
        }
        if (options.menu_smoke && !smoke.confirmed)
        {
            error = "Menu smoke did not reach quit confirmation";
            return failed(4);
        }
        if (options.menu_smoke)
            spdlog::info("Menu traversal passed: startup, pause snapshots, independent audio, options, resize, "
                         "inspector, resume and quit");
        spdlog::info("Frontend complete: {} frames, world {}, menu audio starts {}/failures {}", frames,
                     game->world_path, menu_audio.starts, menu_audio.failures);
        return 0;
    }
} // namespace OpenBlood2
