#include "app/frontend.h"
#include "app/cheat_flow.h"
#include "app/demo_menu_smoke.h"
#include "app/demo_smoke.h"
#include "app/gamepad_devices.h"
#include "app/hud.h"
#include "app/loading_flow.h"
#include "app/menu_input.h"
#include "app/menu_renderer.h"
#include "app/menu_smoke.h"
#include "app/save_flow.h"
#include "app/scene_presenter.h"
#include "app/session_playback.h"
#include "app/setup_screen.h"
#include "app/ui_runtime.h"
#include "engine/capture.h"
#include "game/menu_audio.h"
#include "game/menu_preferences.h"
#if BLOOD2_INSPECTOR
#include "app/inspector.h"
#endif
#if BLOOD2_XR
#include "app/xr_frontend.h"
#endif
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <spdlog/spdlog.h>
#include <utility>

namespace OpenBlood2
{
#if !BLOOD2_XR
    class XrFrontend; // XR builds only; the frontend passes no session elsewhere.
#endif
    namespace
    {
        // run_game's requests to run_frontend.
        constexpr int restart_game = -1, return_to_setup = -2;

        // The setup screen before any game: until Play, Quit or a window close.
        SetupScreen::Result run_setup(const Options& options, Runtime& runtime, [[maybe_unused]] XrFrontend* xr,
                                      SetupScreen& setup, std::string& error)
        {
            // --frames ends the screen like a Quit; --capture saves its third frame (desktop).
            std::uint64_t frames = 0;
#if BLOOD2_XR
            // The XR panel needs a session and presenter to draw around; neither holds a world.
            std::unique_ptr<GameSession> idle;
            std::unique_ptr<ScenePresenter> presenter;
            if (xr)
            {
                idle = std::make_unique<GameSession>();
                presenter = std::make_unique<ScenePresenter>(runtime.gpu);
                xr->menu_hands = false; // The Beretta hands need game data.
            }
#endif
            for (;; ++frames)
            {
                if (options.frame_limit && frames >= options.frame_limit)
                    return SetupScreen::Result::Quit;
#if BLOOD2_XR
                if (xr)
                {
                    const auto begun = xr->begin(error);
                    if (begun == XrFrontend::Begin::Failed)
                        return SetupScreen::Result::None;
                    if (begun == XrFrontend::Begin::Exit)
                        return SetupScreen::Result::Quit;
                    if (begun == XrFrontend::Begin::Idle)
                    {
                        SDL_Event idle_event;
                        while (SDL_PollEvent(&idle_event))
                            if (idle_event.type == SDL_EVENT_QUIT || idle_event.type == SDL_EVENT_TERMINATING)
                                return SetupScreen::Result::Quit;
                        SDL_Delay(10);
                        continue;
                    }
                }
#endif
                SDL_Event event;
                while (SDL_PollEvent(&event))
                {
                    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_TERMINATING ||
                        (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                         event.window.windowID == SDL_GetWindowID(runtime.window)))
                        return SetupScreen::Result::Quit;
                    ImGui_ImplSDL3_ProcessEvent(&event);
                }
#if BLOOD2_XR
                if (xr) // The laser on the panel arrives as mouse events in canvas pixels.
                    for (auto& e : xr->menu_events(true, SDL_GetWindowID(runtime.window)))
                        ImGui_ImplSDL3_ProcessEvent(&e);
#endif
                ImGui_ImplSDLGPU3_NewFrame();
                ImGui_ImplSDL3_NewFrame();
#if BLOOD2_XR
                if (xr)
                {
                    ImGui::GetIO().DisplaySize = {float(XrFrontend::ui_width), float(XrFrontend::ui_height)};
                    ImGui::GetIO().DisplayFramebufferScale = {1, 1};
                }
#endif
                ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
                ImGui::NewFrame();
                const auto result = setup.draw(runtime.window);
#if BLOOD2_XR
                if (xr && xr->pointer())
                {
                    const auto& at = *xr->pointer();
                    auto* draw = ImGui::GetForegroundDrawList();
                    draw->AddCircleFilled({at[0], at[1]}, 4, IM_COL32(230, 40, 25, 255));
                    draw->AddCircle({at[0], at[1]}, 6, IM_COL32(255, 255, 255, 200), 0, 1.5f);
                }
#endif
                ImGui::Render();
                const auto draw_ui = [&](SDL_GPUCommandBuffer* commands, SDL_GPUTexture* texture)
                {
                    ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
                    SDL_GPUColorTargetInfo color{};
                    color.texture = texture;
                    color.load_op = SDL_GPU_LOADOP_CLEAR;
                    color.store_op = SDL_GPU_STOREOP_STORE;
                    color.clear_color = {0, 0, 0, xr ? 0.f : 1.f};
                    auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
                    if (!pass)
                        return false;
                    ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
                    SDL_EndGPURenderPass(pass);
                    return true;
                };
#if BLOOD2_XR
                if (xr)
                {
                    if (!xr->render(*idle, *presenter, false, true, 1, draw_ui, error))
                        return SetupScreen::Result::None;
                }
                else
#endif
                {
                    auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
                    SDL_GPUTexture* target = nullptr;
                    Uint32 w = 0, h = 0;
                    if (!commands || !SDL_WaitAndAcquireGPUSwapchainTexture(commands, runtime.window, &target, &w, &h))
                    {
                        error = SDL_GetError();
                        if (commands)
                            SDL_CancelGPUCommandBuffer(commands);
                        return SetupScreen::Result::None;
                    }
                    Capture capture;
                    const bool save = target && frames == 2 && !options.capture_file.empty();
                    if ((save &&
                         !capture.initialize(runtime.gpu, SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window),
                                             w, h)) ||
                        (target && !draw_ui(commands, save ? capture.texture : target)) ||
                        (save && !capture.enqueue(commands, target, w, h)))
                    {
                        error = SDL_GetError();
                        SDL_CancelGPUCommandBuffer(commands);
                        return SetupScreen::Result::None;
                    }
                    if (!SDL_SubmitGPUCommandBuffer(commands) ||
                        (save && !capture.save(path_text(options.capture_file).c_str())))
                    {
                        error = SDL_GetError();
                        return SetupScreen::Result::None;
                    }
                    if (!target)
                        SDL_Delay(10);
                }
                if (result != SetupScreen::Result::None)
                    return result;
            }
        }

        // One game run with one configuration, from the title screen to Quit. The same setup screen
        // is Options > SETUP: Play with a new choice returns restart_game with `next`.
        int run_game(const Options& options, const Config& config, Runtime& runtime, [[maybe_unused]] UiRuntime& ui,
                     [[maybe_unused]] XrFrontend* xr, SetupScreen& setup, bool by_setup, Config& next,
                     std::string& refusal)
        {
            std::string error;
            auto failed = [&](int code)
            {
                spdlog::error("Frontend: {}", error);
                return code;
            };
#if BLOOD2_XR
            const bool xr_mode = xr != nullptr;
            if (xr)
                xr->menu_hands = true;
#else
            constexpr bool xr_mode = false;
#endif
            DataStatus data;
            Resources artwork;
            if (!inspect_data(config, data, error) || !artwork.mount_game(config, error))
            {
                if (!by_setup)
                    return failed(5);
                // The setup screen shows why its choice cannot be played.
                spdlog::warn("Game data refused: {}", error);
                refusal = error;
                return return_to_setup;
            }
            if (by_setup)
                spdlog::info("Game data from setup.cfg: {} ({}, {} mods)", path_text(config.data_dir),
                             config.demo ? "demo" : "retail", config.overlays.size());
            // Keep the accepted menu-free diagnostic/direct route available independently.
            const bool menu_enabled = !config.demo || (!options.view_world && !options.demo_smoke);
            Menu menu(config.demo ? MenuEdition::Demo : MenuEdition::Retail);
            menu.vr = xr_mode;
            menu.setup = true;
            std::filesystem::path preferences;
            std::string preference_error;
            if (!options.menu_smoke && !options.demo_smoke &&
                (!menu_preferences_path(preferences, preference_error) ||
                 !load_menu_preferences(preferences, menu.settings, preference_error)))
                spdlog::warn("Preferences: {}", preference_error);
            // Saved games (decision 1): the per-user directory, never in smoke runs or the demo (decision 7).
            SaveFlow saves;
            if (config.demo)
                saves.slots.unavailable = "The demo edition has no saved games.";
            else if (options.menu_smoke || options.demo_smoke)
                saves.slots.unavailable = "Saved games are disabled in diagnostic runs.";
            else if (std::filesystem::path directory; save_directory(directory, error))
            {
                saves.slots = SaveSlots(directory);
                saves.slots.refresh();
                spdlog::info("Saved games: {}", path_text(directory));
            }
            else
            {
                saves.slots.unavailable = "Saved games are unavailable: " + error;
                spdlog::warn("{}", saves.slots.unavailable);
            }
            saves.sync(menu);
            // Options > CHEATS: the LEVEL row's campaign worlds and the persisted god mode.
            CheatFlow cheat_flow;
            menu.set_levels(cheat_levels(artwork));
            // The level-entry snapshot after a NEW_GAME or NEW_LEVEL entry (original autosave).
            const auto level_entered = [&](GameSession& session, bool new_game)
            {
                std::string why;
                if (!saves.slots.enabled())
                    return;
                if (saves.level_entered(session, new_game, why))
                    spdlog::info("Level-entry snapshot: {}",
                                 saves.slots.level_entry() ? "saved for Restart" : "not taken (no autosave requested)");
                else
                    spdlog::warn("Level-entry snapshot unavailable; Restart is empty: {}", why);
            };
            std::unique_ptr<MenuCanvas> canvas;
            std::unique_ptr<MenuAudio> menu_audio;
            MenuRenderer menu_renderer(runtime.gpu), title_renderer(runtime.gpu), loading_renderer(runtime.gpu);
            HudCanvas end_canvas(artwork);
            const HudSurface* end_screen = nullptr;
            if (menu_enabled)
            {
                canvas = std::make_unique<MenuCanvas>(artwork);
                if (!canvas->initialize(error))
                    return failed(5);
                menu_audio = std::make_unique<MenuAudio>(artwork);
                if (config.demo && !menu_audio->validate_resources(error))
                    return failed(5);
                menu_audio->random_state = std::uint32_t(SDL_GetTicksNS());
                if (!menu_audio->mixer.open_output(error))
                {
                    menu_audio->error = error;
                    spdlog::warn("Menu audio output: {}", error);
                }
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
            // A frame longer than one step's audio must not starve the device (carried by output transfers).
            game->server.audio.output_lead = AudioMixer::interactive_output_lead;
            if (options.view_world || options.demo_smoke)
            {
                spdlog::info("Loading world: {} (direct entry)", options.world);
                if (!game->request_world(options.world, error))
                    return failed(6);
                game->server_shell.inspection_start = options.view_world;
                if (!game->run_world(error))
                    return failed(6);
                spdlog::info("World ready: {}", game->world_path);
                level_entered(*game, false);
            }
            else
                menu.open(false);
            if (menu_enabled && !preference_error.empty())
                menu.open(game->client_shell.m_bInWorld);
            menu.error = menu_enabled ? preference_error : std::string{};
            auto stored_settings = menu.settings;
            MenuInput input;
            // controls.cfg beside the preferences, under the same smoke-run exclusion.
            std::filesystem::path controls_file;
            if (!options.menu_smoke && !options.demo_smoke)
            {
                std::string why;
                if (!controls_path(controls_file, why) || !load_controls(controls_file, input.controls.bindings, why))
                {
                    spdlog::warn("Controls: {}", why);
                    if (menu_enabled && menu.error.empty())
                    {
                        menu.open(game->client_shell.m_bInWorld);
                        menu.error = why;
                    }
                }
            }
            auto stored_controls = input.controls.bindings.entries;
            input.refresh_controls(menu);
            GamepadDevices gamepads;
            input.demo_mode = config.demo && !menu_enabled;
            input.controls.focused = (SDL_GetWindowFlags(runtime.window) & SDL_WINDOW_INPUT_FOCUS) != 0;
#if BLOOD2_XR
            if (xr)
                input.controls.focused = true; // Session focus replaces window focus.
#endif
            input.reset(menu, game->client_shell.m_bInWorld);
            // The original loading screen (StartNewWorld) on normal menu launches; the menu-free demo,
            // direct entry and inspector levels keep their immediate loads.
            LoadingFlow loading(artwork);
            // Original startup: title.pcx stretched to the screen for six seconds, or until Esc, Return
            // or Space (no movies are played yet). Direct entry (runworld) and smokes skip it.
            HudSurface title;
            double title_remaining = 0;
            if (menu_enabled && !options.view_world && !options.menu_smoke)
            {
                HudCanvas splash(artwork);
                if (const auto* image = splash.CreateSurfaceFromBitmap("screens/title.pcx"))
                {
                    title = {LoadingScreen::width, LoadingScreen::height,
                             std::vector<std::uint8_t>(std::size_t(LoadingScreen::width) * LoadingScreen::height * 4)};
                    hud_scale(title, *image);
                    title_remaining = 6;
                    input.title_screen = true;
                    input.reset(menu, false);
                }
                else
                    spdlog::warn("Title screen: {}", splash.error);
            }
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
            inspector.materials = &presenter.material_report;
#else
            (void)data;
#endif
            MenuSmoke smoke;
            DemoSmoke demo_smoke;
            DemoMenuSmoke demo_menu_smoke;
            bool done = false, captured = false, dismiss_error = false;
            // Options > SETUP over the paused menu (input.setup_screen); its result applies next frame.
            auto setup_result = SetupScreen::Result::None;
            std::uint64_t frames = 0;
            auto previous = SDL_GetTicksNS();
            ObjectId controlled{};
            std::string reported_audio;
            std::uint64_t reported_credits_failures = 0;
            while (!done && (!options.frame_limit || frames < options.frame_limit))
            {
                if (const auto result = std::exchange(setup_result, SetupScreen::Result::None);
                    result != SetupScreen::Result::None)
                {
                    if (result == SetupScreen::Result::Quit)
                        break;
                    if (result == SetupScreen::Result::Play)
                    {
                        spdlog::info("Setup changed the game data; restarting the game");
                        next = setup.config();
                        return restart_game;
                    }
                    input.setup_screen = false;
                    ImGui::GetIO().ClearInputKeys();
                    ImGui::GetIO().ClearInputMouse();
                    input.reset(menu, game->client_shell.m_bInWorld);
                }
#if BLOOD2_XR
                if (xr)
                {
                    const auto begun = xr->begin(error);
                    if (begun == XrFrontend::Begin::Failed)
                        return failed(3);
                    if (begun == XrFrontend::Begin::Exit)
                        break;
                    if (begun == XrFrontend::Begin::Idle)
                    {
                        SDL_Event idle;
                        while (SDL_PollEvent(&idle))
                            done |= idle.type == SDL_EVENT_QUIT || idle.type == SDL_EVENT_TERMINATING;
                        SDL_Delay(10);
                        continue;
                    }
                }
#endif
                const bool paused_at_start = input.paused(menu);
                [[maybe_unused]] bool level_changed = false;
                const auto tick = SDL_GetTicksNS();
                double delta =
                    options.menu_smoke || options.demo_smoke ? 1. / 60 : app_frame_delta(double(tick - previous) / 1e9);
#if BLOOD2_XR
                if (xr)
                    delta = xr->delta();
#endif
                previous = tick;
                saves.sync(menu);
                cheat_flow.update(*game, menu);
                saves.notice.advance(delta);
                if (menu.active && !canvas->compose(menu, 0, error))
                    return failed(4);
                if (options.menu_smoke &&
                    !(config.demo
                          ? demo_menu_smoke.before(frames, runtime.window, *game, menu, input, *menu_audio, error)
                          : smoke.before(frames, runtime.window, *game, menu, input, *menu_audio, error)))
                    return failed(4);
                if (options.demo_smoke)
                {
                    if (canvas || menu_audio || menu.active)
                    {
                        error = "Demo unexpectedly initialized a menu or menu mixer";
                        return failed(4);
                    }
                    if (!demo_smoke.before(frames, runtime.window, *game, input, error))
                        return failed(4);
                }
                input.roll_credits = game->client_shell.m_bInWorld && game->client_shell.m_bRollCredits;
                SDL_Event event;
                int width = 0, height = 0;
                SDL_GetWindowSize(runtime.window, &width, &height);
#if BLOOD2_XR
                if (xr) // Pointer events arrive in canvas pixels.
                {
                    width = int(XrFrontend::ui_width);
                    height = int(XrFrontend::ui_height);
                }
#endif
                // One event through the menu/game input; false on a quit request.
                const auto dispatch = [&](SDL_Event& event)
                {
                    if ((options.menu_smoke || options.demo_smoke) && MenuSmoke::unrelated(event))
                        return true;
                    if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                                         event.window.windowID == SDL_GetWindowID(runtime.window)))
                        return false;
                    gamepads.event(event);
                    if (input.setup_screen)
                    {
                        ImGui_ImplSDL3_ProcessEvent(&event);
                        return true;
                    }
                    if (!menu.error.empty())
                    {
                        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
                            input.event(event, SDL_GetWindowID(runtime.window), menu, game->client_shell.m_bInWorld,
                                        false, BLOOD2_INSPECTOR && options.inspector, float(width), float(height));
                        ImGui_ImplSDL3_ProcessEvent(&event);
                        for (const auto& edge : input.controls.track(event))
                            dismiss_error |= edge.down && !edge.repeat &&
                                             (input.controls.bindings.matches(edge, InputAction::Menu) ||
                                              input.controls.bindings.matches(edge, InputAction::MenuBack));
                        return true;
                    }
                    auto* player = game->server_shell.Player(game->local_client);
                    const bool old_inspector = input.inspector, old_menu = input.paused(menu);
                    const bool forward =
                        input.event(event, SDL_GetWindowID(runtime.window), menu, game->client_shell.m_bInWorld,
                                    player && !player->m_damage.IsDead(), BLOOD2_INSPECTOR && options.inspector,
                                    float(width), float(height));
                    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
                        gamepads.resync(input.controls);
                    if (old_inspector != input.inspector || old_menu != input.paused(menu))
                    {
                        neutralize_app_input(game->client_shell);
                        ImGui::GetIO().ClearEventsQueue();
                        ImGui::GetIO().ClearInputKeys();
                        ImGui::GetIO().ClearInputMouse();
                    }
                    // SDL backend still needs platform/focus/resize events when its windows are hidden.
                    const bool device_input =
                        event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP ||
                        event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_MOUSE_MOTION ||
                        event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                        event.type == SDL_EVENT_MOUSE_WHEEL;
                    if (forward || !device_input)
                        ImGui_ImplSDL3_ProcessEvent(&event);
                    return true;
                };
                while (SDL_PollEvent(&event))
                {
#if BLOOD2_XR
                    // The session's focus replaces the window's (the headset window flips at launch).
                    if (xr &&
                        (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_FOCUS_GAINED))
                        continue;
                    done |= event.type == SDL_EVENT_TERMINATING;
#endif
                    if (!dispatch(event))
                    {
                        done = true;
                        break;
                    }
                }
#if BLOOD2_XR
                if (xr && !done)
                {
                    // Session focus changes and controller menu navigation enter as ordinary events.
                    std::vector<SDL_Event> synthetic;
                    if (const auto focus = xr->take_focus_change())
                    {
                        SDL_Event e{};
                        e.type = *focus ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
                        e.window.windowID = SDL_GetWindowID(runtime.window);
                        synthetic.push_back(e);
                    }
                    // The selection laser on the panel (mouse events in canvas pixels), then the virtual
                    // gamepad; a light pulse when the laser moves the selection.
                    const bool navigating = input.paused(menu) || !menu.error.empty() || input.demo.finished();
                    for (const auto& e : xr->menu_events(navigating, SDL_GetWindowID(runtime.window)))
                        synthetic.push_back(e);
                    const auto hovered = std::pair{menu.page, menu.selected};
                    for (auto& e : synthetic)
                        dispatch(e);
                    if (xr->pointer() && menu.active && std::pair{menu.page, menu.selected} != hovered)
                        xr->vibrate(XrFrontend::pointer_hand, .25f, 8'000'000);
                }
#endif
                if (done || input.demo.quit)
                    break;
                if (input.title_screen && (title_remaining -= delta) <= 0)
                {
                    input.title_screen = false;
                    input.reset(menu, false);
                }
                if (std::exchange(input.loading_continue, false) && loading.waiting)
                {
                    // OnKeyDown in GS_WAITING: GS_PLAYING and PauseGame(FALSE).
                    loading.finish();
                    input.loading_screen = input.loading_wait = false;
                    input.reset(menu, game->client_shell.m_bInWorld);
                    previous = SDL_GetTicksNS();
                }
                {
                    auto& credits = game->client_shell.m_Credits;
                    for (const int key : std::exchange(input.credit_keys, {}))
                        credits.HandleInput(key);
                    if (std::exchange(input.credits_exit, false) && input.credits_screen)
                    {
                        // Original GS_CREDITS Esc in a world: Term, GS_PLAYING, PauseGame(FALSE).
                        credits.Term();
                        input.credits_screen = false;
                        neutralize_app_input(game->client_shell);
                        input.reset(menu, game->client_shell.m_bInWorld);
                    }
                }
                // Freeze queued game output before any potentially slow preparation.
                if (!game->server.audio.pause_output(input.paused(menu), error))
                    return failed(4);
                if (std::exchange(input.demo.restart, false))
                {
                    const auto world = game->world_path;
                    const auto difficulty =
                        DBYTE(game->server.GetVarValueFloat(game->server.GetGameConVar("GAME_DIFFICULTY")));
                    if (!GameSession::start_new_game(game, world, difficulty, 0, error))
                        spdlog::error("Demo restart retained current session: {}", error);
                    else
                    {
                        spdlog::info("Demo restarted with initial Caleb equipment: {}", game->world_path);
                        input.demo.state = DemoFlow::State::Playing;
                        selection.id = {};
                        controlled = {};
                        input.reset(menu, true);
#if BLOOD2_INSPECTOR
                        inspector.forget_world();
#endif
                    }
                    previous = SDL_GetTicksNS();
                }
                auto command = menu.take_command();
                if (command.action == MenuAction::Quit)
                    break;
                if (command.action == MenuAction::Cheat)
                {
                    const bool applied = cheat_flow.command(*game, menu, command);
                    spdlog::info("Cheat {} {}: {}", command.cheat, applied ? "applied" : "refused", menu.cheat_status);
                }
                if (command.action == MenuAction::Warp)
                {
                    // A new game in the chosen world with the current character, at the current
                    // difficulty (medium before any game).
                    command.character = game->client_shell.entry_profile.character;
                    command.difficulty =
                        game->client_shell.m_bInWorld
                            ? unsigned(game->server.GetVarValueFloat(game->server.GetGameConVar("GAME_DIFFICULTY")))
                            : unsigned(DIFFICULTY_MEDIUM);
                }
                if (command.action == MenuAction::Setup)
                {
                    std::string why;
                    if (setup.open(&config, why))
                    {
                        setup.quit_available = true;
                        input.setup_screen = true;
                        ImGui::GetIO().ClearInputKeys();
                        ImGui::GetIO().ClearInputMouse();
                    }
                    else
                        menu.error = why;
                }
                if (command.action == MenuAction::RestoreControls)
                {
                    restore_controls(input.controls.bindings);
                    input.refresh_controls(menu);
                }
                if (command.action == MenuAction::Credits && game->client_shell.m_bInWorld)
                {
                    // Original MenuCredits in a game: the "lame" text-only GS_CREDITS screen.
                    auto& shell = game->client_shell;
                    shell.m_Credits.Term();
                    if (!shell.InitCredits(true, error))
                    {
                        shell.m_Credits.Term();
                        menu.error = error;
                        spdlog::error("Credits failed: {}", error);
                    }
                    else
                    {
                        spdlog::info("Credits screen: {} credits from cres.dll TEXT/CREDITS; font request {} {}x{} "
                                     "drawn with the replacement UI font",
                                     shell.m_Credits.GetNumCredits(), shell.m_Credits.font_name,
                                     shell.m_Credits.font_width, shell.m_Credits.font_height);
                        menu.close();
                        input.credits_screen = true;
                        neutralize_app_input(shell);
                        input.reset(menu, true);
                    }
                    previous = SDL_GetTicksNS();
                }
                // The world requests below load immediately, or after one presented loading screen.
                const auto start_world = [&](const MenuCommand& request)
                {
                    const bool credits = request.action == MenuAction::Credits;
                    const char* what = credits                              ? "Credits"
                                       : request.action == MenuAction::Warp ? "Level warp"
                                                                            : "New game";
                    const auto start = menu_world_start(*game, request, first_level(config.demo));
                    spdlog::info("{} requested: world={}; character={}; difficulty={}; type={}", what, start.world,
                                 int(start.character), int(start.difficulty), int(start.game_type));
                    // Execute the owned profile request outside SDL event dispatch.
                    const bool started = start_menu_world(game, start, error);
                    if (!started)
                    {
                        menu.error = error;
                        spdlog::error("{} failed: {}", what, error);
                    }
                    else
                    {
                        spdlog::info("World ready: {}", game->world_path);
                        level_entered(*game, !credits);
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
                    return started;
                };
                // Save/Load rows, F5 and F6: owned requests consumed here, at the frame boundary.
                const auto loaded = [&](const char* what)
                {
                    spdlog::info("{} loaded: {} ({} records)", what, game->world_path, game->game_load.records);
                    menu.error.clear();
                    reset_after_promotion(menu, input, selection, controlled, game->client_shell);
#if BLOOD2_INSPECTOR
                    inspector.forget_world();
#endif
                    previous = SDL_GetTicksNS();
                };
                const auto refused = [&](const char* what)
                {
                    spdlog::warn("{} refused: {}", what, error);
                    if (!menu.active) // Pause under the modal, as the menu does.
                    {
                        auto* owner = game->server_shell.Player(game->local_client);
                        menu.open(game->client_shell.m_bInWorld, owner && !owner->m_damage.IsDead());
                        input.reset(menu, game->client_shell.m_bInWorld);
                    }
                    menu.error = error;
                };
                const auto load_slot = [&](const MenuCommand& request)
                {
                    const auto outcome = saves.command(request, game, menu, error);
                    if (outcome == SaveFlow::Outcome::Saved)
                    {
                        spdlog::info("Saved game: {}", saves.slots.report.last_action);
                        input.reset(menu, true);
                    }
                    else if (outcome == SaveFlow::Outcome::Loaded)
                        loaded("Saved game");
                    else if (outcome == SaveFlow::Outcome::Failed)
                        refused(request.action == MenuAction::SaveGame ? "Save" : "Load");
                    return outcome == SaveFlow::Outcome::Loaded;
                };
                const auto quick_load = [&]
                {
                    const auto quick = saves.quick_load(game, error);
                    if (quick == SaveFlow::Outcome::Loaded)
                        loaded("Quickload");
                    else if (quick == SaveFlow::Outcome::Failed)
                        refused("Quickload");
                    return quick == SaveFlow::Outcome::Loaded;
                };
                const auto change_level = [&]
                {
                    const auto change = GameSession::process_level_change(game, error);
                    level_changed = change == LevelChangeResult::Committed;
                    if (change == LevelChangeResult::Committed)
                    {
                        spdlog::info("NEW_LEVEL committed: {} -> {} / start {}", game->level_change.source_world,
                                     game->world_path, game->level_change.selected_start);
                        reset_after_promotion(menu, input, selection, controlled, game->client_shell);
                        level_entered(*game, false);
#if BLOOD2_INSPECTOR
                        inspector.forget_world();
#endif
                        previous = SDL_GetTicksNS();
                    }
                    else if (change == LevelChangeResult::Rejected)
                        spdlog::warn("Level change retained world: {}", error);
                    return level_changed;
                };
                // A saved world's loading screen, when the slot can be read at all.
                const auto saved_world = [&](int slot) -> std::string
                {
                    const auto& info = saves.slots.info(slot);
                    return info.state == SaveSlotInfo::State::Ready ? info.meta.world_path : std::string{};
                };
                const auto show_loading =
                    [&](LoadingFlow::Request kind, std::string_view world, bool action, const MenuCommand& request = {})
                {
                    if (!menu_enabled || world.empty())
                        return false;
                    if (!loading.begin(kind, requested_world(world), action, request))
                    {
                        spdlog::warn("Loading screen unavailable; loading without it: {}", loading.error);
                        return false;
                    }
                    input.loading_screen = true;
                    input.loading_wait = false;
                    input.reset(menu, game->client_shell.m_bInWorld);
                    return true;
                };
                // The previous frame presented the loading screen (original PreLoadWorld): load now.
                if (const auto request = loading.request; request != LoadingFlow::Request::None)
                {
                    bool success = false;
                    if (request == LoadingFlow::Request::MenuWorld)
                        success = start_world(loading.command);
                    else if (request == LoadingFlow::Request::SaveLoad)
                        success = load_slot(loading.command);
                    else if (request == LoadingFlow::Request::QuickLoad)
                        success = quick_load();
                    else
                        success = change_level();
                    loading.loaded(success);
                    input.loading_screen = loading.visible();
                    input.loading_wait = loading.waiting;
                    input.loading_continue = false;
                    input.reset(menu, game->client_shell.m_bInWorld);
                    if (loading.waiting)
                        spdlog::info("Loading screen waits for a key: {} (load screen ID {})",
                                     loading.screen.info().title, loading.screen.info().load_screen_id);
                }
                else if (command.action == MenuAction::NewGame || command.action == MenuAction::Credits ||
                         command.action == MenuAction::Warp)
                {
                    const auto start = menu_world_start(*game, command, first_level(config.demo));
                    if (!show_loading(LoadingFlow::Request::MenuWorld, start.world ? start.world : "",
                                      start.game_type == GAMETYPE_ACTION, command))
                        start_world(command);
                }
                else if (command.action != MenuAction::LoadGame ||
                         !show_loading(LoadingFlow::Request::SaveLoad, saved_world(command.slot), false, command))
                    load_slot(command);
                if (std::exchange(input.quick_save, false))
                {
                    if (saves.quick_save(*game, error) == SaveFlow::Outcome::Saved)
                        spdlog::info("Quicksave: {}", game->world_path);
                    else
                        refused("Quicksave");
                }
                if (std::exchange(input.quick_load, false) &&
                    !show_loading(LoadingFlow::Request::QuickLoad, saved_world(SLOT_QUICK), false))
                    quick_load();
#if BLOOD2_INSPECTOR
                if (auto level = std::exchange(inspector.start_level_request, {}); !level.empty())
                {
                    spdlog::info("Inspector level requested: world={}; difficulty={}", level,
                                 inspector.start_level_difficulty);
                    if (!GameSession::start_new_game(game, level, DBYTE(inspector.start_level_difficulty), 0, error,
                                                     true))
                    {
                        inspector.world_error = error;
                        spdlog::error("Inspector level failed: {}", error);
                    }
                    else
                    {
                        spdlog::info("World ready: {}", game->world_path);
                        level_entered(*game, false);
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
                    if (!options.menu_smoke && !options.demo_smoke &&
                        !save_menu_preferences(preferences, menu.settings, error))
                    {
                        menu.error = error;
                        spdlog::error("Save preferences failed: {}", error);
                    }
                }
                if (stored_controls != input.controls.bindings.entries)
                {
                    stored_controls = input.controls.bindings.entries;
                    if (!controls_file.empty() && !save_controls(controls_file, input.controls.bindings, error))
                    {
                        menu.error = error;
                        spdlog::error("Save controls failed: {}", error);
                    }
                }
                const auto& settings = menu.settings;
                const float sound_gain = settings.sound ? settings.sound_volume / 50.f : 0;
                // Original SetMusicVolume takes 0..100 (100 = full); the original menu slider stops at 50.
                const float music_gain = settings.music ? settings.music_volume / 100.f : 0;
                if (!game->server.audio.set_gains(sound_gain, music_gain) ||
                    (menu_audio && !menu_audio->mixer.set_gains(sound_gain, music_gain)))
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
                presenter.shading = !settings.renderer ? SurfaceShading::Original
                                    : settings.quality ? SurfaceShading::High
                                                       : SurfaceShading::Standard;
                presenter.normal_maps = settings.normal_maps;
#if BLOOD2_XR
                if (xr)
                    xr->set_options({float(settings.vr_turn) * 0.017453293f, settings.vr_laser, settings.vr_crosshair,
                                     settings.vr_move_hand});
#endif
                if (menu_audio)
                {
                    menu_audio->focus(menu.active && menu.promotion == MenuPromotion::None && !loading.visible() &&
                                      !input.title_screen);
                    for (auto cue : menu.cues)
                        menu_audio->play(cue);
                    menu.cues.clear();
                    // DrawLoadingScreen in GS_WAITING: esc01.wav once.
                    if (loading.waiting && !std::exchange(loading.screen.played_waiting_sound, true))
                        menu_audio->play(MenuCue::Back);
                    menu_audio->advance(delta);
                    if (menu_audio->error != reported_audio)
                    {
                        reported_audio = menu_audio->error;
                        if (!reported_audio.empty())
                        {
                            spdlog::warn("Menu audio: {}", reported_audio);
                            menu.audio_failed = true;
                            ++menu.revision;
                        }
                    }
                }
                if (!game->server.audio.pause_output(input.paused(menu), error))
                    return failed(4);
                auto& shell = game->client_shell;
                if (input.paused(menu) || input.inspector)
                    neutralize_app_input(shell);
                shell.movement_input = {};
                shell.weapon_input = {};
                shell.goggles_down = shell.eye_down = shell.binoculars_down = shell.use_down = shell.medkit_down =
                    shell.flashlight_down = shell.proximity_down = shell.remote_down = shell.detonate_down = false;
#if BLOOD2_INSPECTOR
                inspector.apply_commands(*game, selection);
                if (std::exchange(inspector.play_request, false) && shell.m_bInWorld)
                {
                    if (config.demo)
                        input.demo.state = DemoFlow::State::Playing;
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
                const bool blocked = input.paused(menu) || input.inspector;
                input.controls.enabled = !blocked;
#if BLOOD2_XR
                if (xr)
                    xr->apply_controls(*game, !blocked && !menu.active); // Replaces the desktop sampling below.
                else
#endif
                    if (!input.controls.sync_mouse(runtime.window, shell.m_bInWorld, blocked, error))
                {
                    if (!menu_enabled)
                        return failed(4);
                    menu.open(shell.m_bInWorld);
                    menu.error = "Cannot capture mouse: " + error;
                    input.reset(menu, shell.m_bInWorld);
                }
                if (!blocked && !menu.active && !xr_mode)
                {
                    shell.movement_input = input.controls.sample(false);
                    const auto fire = input.controls.sample_fire(false);
                    shell.weapon_input.primary = fire.primary;
                    shell.weapon_input.alternate = fire.alternate;
                    shell.weapon_input.all_hands = true; // Original HandleFiring: both carried hands.
                    shell.use_down = input.controls.sample_use(false);
                    shell.medkit_down = input.controls.sample_medkit(false);
                    shell.flashlight_down = input.controls.sample_flashlight(false);
                    shell.binoculars_down = input.controls.sample_binoculars(false);
                    shell.goggles_down = input.controls.sample_goggles(false);
                    shell.eye_down = input.controls.sample_eye(false);
                    shell.proximity_down = input.controls.sample_proximity(false);
                    shell.remote_down = input.controls.sample_remote(false);
                    shell.detonate_down = input.controls.sample_detonate(false);
                    shell.time_bomb_down = input.controls.sample_time_bomb(false);
                    shell.next_item_requested |= input.controls.sample_command(InputAction::NextItem, false);
                    shell.prev_item_requested |= input.controls.sample_command(InputAction::PrevItem, false);
                    shell.use_item_requested |= input.controls.sample_command(InputAction::UseItem, false);
                    if (const int slot = input.controls.sample_weapon_slot(false); slot >= 0)
                        if (!game->command_weapon(DBYTE(slot), error))
                            spdlog::info("Weapon selection: {}", error);
                    for (const bool next : {true, false})
                        if (input.controls.sample_command(next ? InputAction::NextWeapon : InputAction::PrevWeapon,
                                                          false))
                            if (!game->cycle_weapon(next, error))
                                spdlog::info("Weapon selection: {}", error);
                    input.controls.look(shell.rotation, delta, false, shell.m_bZoomView);
                }
                if (!input.paused(menu) && !paused_at_start)
                {
                    if (!step_app_session(*game, delta))
                    {
                        spdlog::error("Simulation failed: world={}, frame={}, delta={}, time={}", game->world_path,
                                      frames, delta, game->objects.time());
                        for (const auto& diagnostic : game->objects.diagnostics())
                            spdlog::error("Object: {}", diagnostic);
                        for (const auto& diagnostic : game->trace)
                            spdlog::error("Session: {}", diagnostic);
                        for (const auto& diagnostic : game->step_errors())
                            spdlog::error("{}", diagnostic);
                        error = "Simulation step failed";
                        return failed(4);
                    }
                    if (std::exchange(game->client_shell.world_exit.demo_end_pending, false))
                    {
                        if (!menu_enabled)
                        {
                            end_screen = end_canvas.CreateSurfaceFromBitmap("screens/info.pcx");
                            if (!end_screen)
                            {
                                error = end_canvas.error;
                                return failed(5);
                            }
                        }
                        if (!game->unload_world())
                        {
                            error = "Cannot end the demo during object callbacks";
                            return failed(4);
                        }
                        if (menu_enabled)
                            menu.end_demo();
                        else
                            input.demo.state = DemoFlow::State::Finished;
                        input.inspector = false;
                        input.reset(menu, false);
                        neutralize_app_input(game->client_shell);
                        if (!game->server.audio.pause_output(true, error))
                            return failed(4);
                        spdlog::info("Demo completed: original EndGame / screens/info.pcx / {}",
                                     menu_enabled ? "return to menu" : "direct exit");
                    }
                    // Original NEW_LEVEL StartNewWorld: the loading screen first, the commit next frame.
                    WorldRequest destination;
                    std::string ignored;
                    const bool action =
                        int(game->server.GetVarValueFloat(game->server.GetGameConVar("GAME_GAMETYPE"))) ==
                        GAMETYPE_ACTION;
                    if (!game->pending_level_change ||
                        !parse_world_request(game->pending_level_change->message, destination, ignored) ||
                        !show_loading(LoadingFlow::Request::LevelChange, destination.filename, action))
                        change_level();
                    // Original single-player death USE: GS_MENU, PauseGame and the Load menu (_DEMO: none here;
                    // the demo keeps its Enter retry).
                    if (std::exchange(game->client_shell.dead_use_menu, false) && menu_enabled && !menu.demo() &&
                        game->client_shell.m_bInWorld)
                    {
                        const auto type =
                            int(game->server.GetVarValueFloat(game->server.GetGameConVar("GAME_GAMETYPE")));
                        if (type == GAMETYPE_SINGLE || type == GAMETYPE_ACTION || type == GAMETYPE_CUSTOM)
                        {
                            menu.open_load(true, false);
                            saves.sync(menu);
                            input.reset(menu, true);
                        }
                    }
                }
#if BLOOD2_XR
                if (xr)
                    xr->after_step(*game, level_changed);
#endif
                if (input.credits_screen)
                    game->client_shell.m_Credits.Update(static_cast<float>(delta)); // Original UpdateCredits.
                if (game->client_shell.credits_failures != reported_credits_failures)
                {
                    reported_credits_failures = game->client_shell.credits_failures;
                    spdlog::warn("Credits roll: {}", game->client_shell.credits_error);
                }
                selection.refresh(game->objects);
                // A waiting loading screen still prepares the loaded world behind it.
                const bool screen_shown = input.title_screen || loading.visible();
                if (loading.waiting)
                {
                    loading.clock += delta;
                    if (!loading.screen.draw(true, loading.clock, error))
                        return failed(4);
                }
                if (!menu.active && !input.credits_screen && !input.demo.finished() && !input.title_screen)
                {
#if BLOOD2_XR
                    if (xr)
                        xr->prepare(*game, presenter);
#endif
                    if (!presenter.update(*game, error))
                        return failed(5);
                }
                ImGui_ImplSDLGPU3_NewFrame();
                ImGui_ImplSDL3_NewFrame();
#if BLOOD2_XR
                if (xr_mode)
                {
                    // The XR UI canvas has fixed pixels, independent of the (unpresented) window.
                    ImGui::GetIO().DisplaySize = {float(XrFrontend::ui_width), float(XrFrontend::ui_height)};
                    ImGui::GetIO().DisplayFramebufferScale = {1, 1};
                }
#endif
                ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);
                if (input.inspector || input.setup_screen)
                    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                if (input.setup_screen)
                    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
                ImGui::NewFrame();
                if (input.demo.finished())
                {
                    if (!end_screen || !menu_renderer.draw_surface(*end_screen, 1, error))
                        return failed(4);
                }
                else if (input.title_screen)
                {
                    if (!title_renderer.draw_surface(title, 1, error))
                        return failed(4);
                }
                else if (loading.visible())
                {
                    if (!loading_renderer.draw_surface(loading.screen.surface(), loading.screen.revision(), error))
                        return failed(4);
                }
                else if (input.setup_screen)
                    setup_result = setup.draw(runtime.window);
                else if (menu.active)
                {
                    if (!canvas->compose(menu, 0, error) || !menu_renderer.draw(*canvas, error))
                        return failed(4);
                }
                else if (input.credits_screen)
                    draw_credits(game->client_shell.m_Credits);
                else
                {
                    // The original rolls credits inside Start3D/End3D, below fades and the status bar.
                    if (game->client_shell.m_bInWorld && game->client_shell.m_bRollCredits)
                        draw_credits(game->client_shell.m_Credits);
                    if (!xr_mode) // XR fades the scene, not the head-locked canvas.
                        draw_screen_fade(game->client_shell.fade.Scale(), game->client_shell.m_bInWorld);
                    const auto& client = game->client_shell;
                    // XR aims with the controller (laser and reticle): no screen-centred crosshair.
                    auto state = hud_state(client.stats, client.m_bInWorld, client.view.object != nullptr,
                                           client.death_camera.active, client.m_bShowCrosshair && !xr_mode,
                                           client.m_nCrosshair, client.ctf.team, client.ctf.flag_icon);
                    state.items = client.inventory_items;
                    state.item_messages = client.inventory_item_messages;
                    state.item_shows = client.inventory_show_requests;
                    state.time = float(game->objects.time());
                    if (!hud.draw(state, game->resources, error))
                        return failed(4);
                    if (input.touch.visible && client.m_bInWorld && !xr_mode)
                        draw_touch_controls(input.touch);
                    if (saves.notice.visible())
                    {
                        const auto& io = ImGui::GetIO();
                        const auto size = ImGui::CalcTextSize(saves.notice.text.c_str());
                        const ImVec2 at{(io.DisplaySize.x - size.x) * .5f, io.DisplaySize.y * .25f};
                        auto* draw = ImGui::GetForegroundDrawList();
                        draw->AddText({at.x + 1, at.y + 1}, IM_COL32(0, 0, 0, 255), saves.notice.text.c_str());
                        draw->AddText(at, IM_COL32(255, 255, 255, 255), saves.notice.text.c_str());
                    }
                }
#if BLOOD2_XR
                if (xr && xr->pointer())
                {
                    // The laser's cursor on the canvas.
                    const auto& at = *xr->pointer();
                    auto* draw = ImGui::GetForegroundDrawList();
                    draw->AddCircleFilled({at[0], at[1]}, 4, IM_COL32(230, 40, 25, 255));
                    draw->AddCircle({at[0], at[1]}, 6, IM_COL32(255, 255, 255, 200), 0, 1.5f);
                }
#endif
                if (!menu.error.empty())
                {
                    ImGui::OpenPopup("Unable to complete request");
                    if (ImGui::BeginPopupModal("Unable to complete request", nullptr,
                                               ImGuiWindowFlags_AlwaysAutoResize))
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
                inspector.saves = &saves.slots;
                inspector.save_notice = saves.notice.visible() ? saves.notice.text : std::string{};
                const bool was_visible = input.inspector;
                if (!xr_mode)
                    inspector.draw(config, data, input.inspector, frames, *game, selection);
                if (was_visible && !input.inspector)
                    input.reset(menu, game->client_shell.m_bInWorld);
#endif
                ImGui::Render();
#if BLOOD2_XR
                if (xr)
                {
                    // The paused world stays behind the menu panel; ImGui draws the canvas (menu or HUD).
                    const bool world = !input.credits_screen && !input.demo.finished() && !screen_shown &&
                                       game->client_shell.m_bInWorld;
                    const bool panel = menu.active || input.credits_screen || input.demo.finished() || screen_shown;
                    const float scale = game->client_shell.fade.Scale();
                    const float fade =
                        game->client_shell.m_bInWorld && std::isfinite(scale) ? std::clamp(scale, 0.f, 1.f) : 1;
                    const auto canvas = [&](SDL_GPUCommandBuffer* commands, SDL_GPUTexture* texture)
                    {
                        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
                        SDL_GPUColorTargetInfo color{};
                        color.texture = texture;
                        color.load_op = SDL_GPU_LOADOP_CLEAR;
                        color.store_op = SDL_GPU_STOREOP_STORE;
                        color.clear_color = {0, 0, 0, 0};
                        auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
                        if (!pass)
                            return false;
                        ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
                        SDL_EndGPURenderPass(pass);
                        return true;
                    };
                    if (!xr->render(*game, presenter, world, panel, fade, canvas, error))
                        return failed(3);
                    if (frames % 360 == 0)
                    {
                        const auto* data = ImGui::GetDrawData();
                        const auto& hands = xr->frame().hands;
                        std::string weapons;
                        for (const auto& entry : presenter.model_scene.entries)
                            if (entry.label.ends_with("view weapon"))
                                weapons += " [" + entry.label + (entry.error.empty() ? "" : ": " + entry.error) + "]";
                        spdlog::info("XR frontend: frame {}, {} canvas, {} world, menu {}, {} UI vertices in {} lists, "
                                     "{} draw calls, fade {:.2f}, controllers {}/{}, {} close models{}",
                                     frames, panel ? "panel" : "HUD",
                                     world && presenter.render_world ? "with" : "without", menu.active,
                                     data ? data->TotalVtxCount : 0, data ? data->CmdListsCount : 0,
                                     presenter.stats.draw_calls, fade, hands[0].grip_valid, hands[1].grip_valid,
                                     presenter.model_scene.close_draws.size(), weapons);
                    }
                    ++frames;
                    continue;
                }
#endif
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
                const bool phase_capture =
                    (options.menu_smoke && config.demo && DemoMenuSmoke::capture(frames)) ||
                    (options.menu_smoke && !config.demo &&
                     (frames == 4 || frames == 8 || frames == 17 || frames == 32 || frames == 23 || frames == 120 ||
                      frames == 208 || frames == 214)) ||
                    (options.demo_smoke && (frames == 4 || frames == 14 || frames == 45 || frames == 62));
                const bool save =
                    target && ((frames >= 2 && !captured) || phase_capture) && !options.capture_file.empty();
                if (save && !capture.initialize(runtime.gpu,
                                                SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window), w, h))
                {
                    error = SDL_GetError();
                    SDL_CancelGPUCommandBuffer(commands);
                    return failed(3);
                }
                if (target)
                {
                    auto* color_target = save ? capture.texture : target;
                    const bool world = !menu.active && !input.credits_screen && !input.demo.finished() &&
                                       !screen_shown && presenter.render_world;
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
                        phase_capture ? options.capture_file.parent_path() / (path_text(options.capture_file.stem()) +
                                                                              "-" + std::to_string(frames) + ".png")
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
            if (options.demo_smoke && !demo_smoke.confirmed)
            {
                error = "Demo diagnostic did not complete both worlds and the ending";
                return failed(4);
            }
            if (options.demo_smoke)
                spdlog::info("Demo SDL traversal passed: no menu resources/mixer, pause/resume/focus, authored exits, "
                             "info.pcx and quit");
            if (options.menu_smoke && !(config.demo ? demo_menu_smoke.confirmed : smoke.confirmed))
            {
                error = "Menu smoke did not reach quit confirmation";
                return failed(4);
            }
            if (options.menu_smoke && config.demo)
                spdlog::info(
                    "Demo menu traversal passed: source pages/promotions, supported options, pause/audio/focus, "
                    "death/Enter retry, transfer, EndGame return, new game after end, quit promotion");
            if (options.menu_smoke && !config.demo)
                spdlog::info("Menu traversal passed: startup, pause snapshots, independent audio, options, resize, "
                             "inspector, resume and quit");
            spdlog::info("Frontend complete: {} frames, world {}, menu audio starts {}/failures {}", frames,
                         game->world_path, menu_audio ? menu_audio->starts : 0, menu_audio ? menu_audio->failures : 0);
            return 0;
        }
    } // namespace

    int run_frontend(const Options& options, const std::optional<Config>& configured)
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
#if BLOOD2_XR
        // --xr: the OpenXR session owns the device; the window only carries SDL input/ImGui and
        // nothing is presented to it.
        std::unique_ptr<XrFrontend> xr;
        if (options.xr_world)
        {
            SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0"); // OpenXR keeps polling while paused.
            xr = std::make_unique<XrFrontend>();
            if (!runtime.initialize_window(error) || !xr->create(gpu_debug, error))
                return failed(3);
            runtime.set_device(xr->device());
        }
        else
#endif
            if (!runtime.initialize(gpu_debug, error))
            return failed(3);
        spdlog::info("GPU device: {}", SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu),
                                                             SDL_PROP_GPU_DEVICE_NAME_STRING, "unreported"));
        UiRuntime ui(runtime);
#if BLOOD2_XR
        XrFrontend* session = xr.get();
        if (!ui.initialize(error, xr ? xr->ui_format() : SDL_GPU_TEXTUREFORMAT_INVALID))
#else
        XrFrontend* session = nullptr;
        if (!ui.initialize(error))
#endif
            return failed(3);
        SetupScreen setup;
        setup.font = ui.proportional;
        // A configuration from the command line, environment or blood2.local.ini skips the screen,
        // and so does a stored choice that is still playable.
        std::optional<Config> next = configured;
        bool by_setup = !configured;
        if (!next)
        {
            Config stored;
            SetupState state;
            std::filesystem::path file;
            std::string why;
            if (setup_path(file, why) && load_setup(file, state, why) && setup_ready(state, stored, why))
                next = std::move(stored);
            else if (!why.empty())
                spdlog::info("Setup: {}", why);
        }
        std::string refusal;
        for (;;)
        {
            if (!next)
            {
                if (!setup.open(nullptr, error))
                    return failed(2);
                if (!refusal.empty())
                    setup.message = std::exchange(refusal, {});
                spdlog::info("Setup screen shown");
                const auto result = run_setup(options, runtime, session, setup, error);
                if (result == SetupScreen::Result::Quit)
                    return 0;
                if (result != SetupScreen::Result::Play)
                    return failed(3);
                next = setup.config();
            }
            Config chosen;
            const int code = run_game(options, *next, runtime, ui, session, setup, by_setup, chosen, refusal);
            if (code == restart_game)
                next = std::move(chosen);
            else if (code == return_to_setup)
                next.reset();
            else
                return code;
            by_setup = true;
        }
    }
} // namespace OpenBlood2
