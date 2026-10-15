#include "app/movement_controls.h"
#include "build_info.h"
#include "engine/capture.h"
#include "engine/config.h"
#include "engine/runtime.h"
#include "game/server_adapter.h"

#include <SDL3/SDL_main.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <spdlog/spdlog.h>
#include <vector>

#if BLOOD2_INSPECTOR
#include "app/inspector.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#endif

namespace
{

    bool push_toggle(SDL_Window* window)
    {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.windowID = SDL_GetWindowID(window);
        event.key.key = SDLK_F1;
        event.key.scancode = SDL_SCANCODE_F1;
        event.key.down = true;
        if (!SDL_PushEvent(&event))
            return false;
        event.type = SDL_EVENT_KEY_UP;
        event.key.down = false;
        return SDL_PushEvent(&event);
    }

    int run(const OpenBlood2::Options& options, const OpenBlood2::Config& config, const OpenBlood2::DataStatus& data)
    {
        OpenBlood2::Runtime runtime;
        std::string error;
#ifdef NDEBUG
        constexpr bool gpu_debug = false;
#else
        constexpr bool gpu_debug = true;
#endif
        if (!runtime.initialize(gpu_debug, error))
        {
            spdlog::error("GPU startup failed: {}", error);
            return 3;
        }
        spdlog::info("SDL {}.{}.{} [{}]; video={}; GPU={}", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION,
                     OpenBlood2::build::sdl_revision, SDL_GetCurrentVideoDriver(), SDL_GetGPUDeviceDriver(runtime.gpu));
        spdlog::info("GPU device: {}", SDL_GetStringProperty(SDL_GetGPUDeviceProperties(runtime.gpu),
                                                             SDL_PROP_GPU_DEVICE_NAME_STRING, "unreported"));
        OpenBlood2::GameSession game;
        if (!game.initialize())
        {
            spdlog::error("Original game boundary initialization failed");
            return 4;
        }
        HOBJECT key = nullptr;
        OpenBlood2::ObjectSelection selection;
        if (options.gpu_smoke)
        {
            game.create_example("CameraSpot");
            key = game.create_example("Key");
            if (!key)
                return 4;
            selection.id = game.objects.get(key)->id;
        }
        else
        {
            if (!game.resources.mount_game(config, error) || !game.request_world(options.world, error))
            {
                spdlog::error("World inspection load failed: {}", error);
                return 5;
            }
            for (const auto& entry : game.entries)
                if (entry.object)
                {
                    selection.id = entry.object;
                    break;
                }
            spdlog::info("DAT v{}: {} source objects, {} original instances, {} BSP models", game.world.version,
                         game.world.objects.size(), game.objects.snapshot().size(), game.world.models.size());
            if (options.player_smoke)
            {
                if (!game.enter_player(error))
                {
                    spdlog::error("Player entry: {}", error);
                    return 6;
                }
                selection.id = game.objects.get(game.client_shell.player)->id;
            }
        }
        const auto original_selection = selection.id;
        bool selection_cleared = false, world_unloaded = false;
        const auto initial_objects = game.objects.snapshot().size(), initial_strings = game.server.string_count();
        const bool smoke = options.gpu_smoke || options.world_smoke;
        bool inspector_visible = BLOOD2_INSPECTOR && options.inspector;
#if BLOOD2_INSPECTOR
        OpenBlood2::Inspector inspector(runtime);
        inspector.select_models = options.world_smoke && !options.player_smoke;
        inspector.select_session = options.player_smoke && !options.weapons_smoke && !options.movement_smoke;
        inspector.select_weapons = options.weapons_smoke;
        inspector.select_movement = options.movement_smoke;
        if (!inspector.initialize(error))
        {
            spdlog::error("{}", error);
            return 3;
        }
#else
        (void)config;
        (void)data;
#endif
        bool done = false;
        bool resized = false;
        bool quit_received = false;
        bool captured = false;
        std::uint32_t toggles = 0;
        std::uint64_t frames = 0, ui_frames = 0, hidden_frames = 0;
        const auto started = SDL_GetTicks();
        auto previous_tick = SDL_GetTicksNS();
        double hidden_model_time = 0;
        bool model_advanced_hidden = false;
        std::uint64_t hidden_player_updates = 0;
        bool player_advanced_hidden = false;
        bool weapons_fired = false, weapon_advanced_hidden = false;
        std::uint64_t hidden_weapon_shots = 0;
        OpenBlood2::MovementControls controls;
        controls.focused = (SDL_GetWindowFlags(runtime.window) & SDL_WINDOW_INPUT_FOCUS) != 0;
        DVector hidden_position{};
        bool movement_hidden = false, movement_jumped = false, movement_landed = false;
        unsigned forward_frames = 0, jump_frames = 0;
        const bool extended_smoke = options.weapons_smoke || options.movement_smoke;
        const unsigned hide_frame = extended_smoke ? 40 : 4, show_frame = extended_smoke ? 160 : 8;
        const unsigned unload_frame = extended_smoke ? 210 : 16, reload_frame = extended_smoke ? 220 : 18,
                       quit_frame = extended_smoke ? 240 : 24;
        while (!done)
        {
#if BLOOD2_INSPECTOR
            controls.enabled = inspector.keyboard_movement;
#endif
            SDL_Event event{};
            while (SDL_PollEvent(&event))
            {
                controls.event(event, SDL_GetWindowID(runtime.window));
#if BLOOD2_INSPECTOR
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F2 && !event.key.repeat)
                    inspector.keyboard_movement = controls.enabled;
#endif
#if BLOOD2_INSPECTOR
                if (!controls.consumes(event, inspector_visible && ImGui::GetIO().WantTextInput))
                    ImGui_ImplSDL3_ProcessEvent(&event);
#endif
                if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                                     event.window.windowID == SDL_GetWindowID(runtime.window)))
                {
                    quit_received = true;
                    done = true;
                }
                if (event.type == SDL_EVENT_WINDOW_RESIZED && event.window.windowID == SDL_GetWindowID(runtime.window))
                    resized = true;
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
                {
#if BLOOD2_INSPECTOR
                    if (event.key.key == SDLK_F1)
                    {
                        inspector_visible = !inspector_visible;
                        ++toggles;
                    }
                    if (event.key.key == SDLK_ESCAPE && !ImGui::GetIO().WantCaptureKeyboard)
                        done = true;
#else
                    if (event.key.key == SDLK_ESCAPE)
                        done = true;
#endif
                }
            }
            if (done)
                break;
            if (smoke && SDL_GetTicks() - started > 15000)
            {
                spdlog::error("GPU smoke timed out before completing its frames/events");
                return 4;
            }
            const auto tick = SDL_GetTicksNS();
            const double delta = smoke ? 1.0 / 60.0 : static_cast<double>(tick - previous_tick) / 1e9;
            previous_tick = tick;
            game.client_shell.movement_input = {};
            bool text_editing = false;
#if BLOOD2_INSPECTOR
            inspector.apply_commands(game, selection);
            controls.enabled = inspector.keyboard_movement;
            text_editing = inspector_visible && ImGui::GetIO().WantTextInput;
#endif
            auto keyboard = controls.sample(text_editing);
            auto& movement = game.client_shell.movement_input;
            movement.forward |= keyboard.forward;
            movement.backward |= keyboard.backward;
            movement.left |= keyboard.left;
            movement.right |= keyboard.right;
            movement.run |= keyboard.run;
            movement.jump |= keyboard.jump;
            if (!controls.focused && !smoke)
                movement = {};
            controls.look(game.client_shell.rotation, delta, text_editing);
            if (options.movement_smoke)
            {
                forward_frames += movement.forward;
                jump_frames += movement.jump;
            }
            if (options.weapons_smoke && game.local_client)
            {
                if (frames == 60 && !game.select_weapon(0, error))
                {
                    spdlog::error("{}", error);
                    return 6;
                }
                game.client_shell.weapon_input = {frames < 120, frames >= 120, false};
            }
            if (!game.step(delta))
            {
                spdlog::error("Object simulation step failed");
                return 4;
            }
            selection.refresh(game.objects);
            if (options.world_smoke && frames >= unload_frame && game.world_path.empty() &&
                game.server.string_count() == 0)
                world_unloaded = true;
            if (smoke && frames >= unload_frame && !selection.id && !game.objects.get(original_selection))
                selection_cleared = true;
#if BLOOD2_INSPECTOR
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            if (controls.enabled)
                ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
            else
                ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::NewFrame();
            inspector.draw(config, data, inspector_visible, frames, game, selection);
            ImGui::Render();
#endif
            SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            if (!commands)
            {
                spdlog::error("Command buffer: {}", SDL_GetError());
                return 3;
            }
            SDL_GPUTexture* swapchain = nullptr;
            Uint32 width = 0, height = 0;
            if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, runtime.window, &swapchain, &width, &height))
            {
                spdlog::error("Swapchain: {}", SDL_GetError());
                SDL_CancelGPUCommandBuffer(commands);
                return 3;
            }
            OpenBlood2::Capture capture;
            const bool capture_frame =
                swapchain && frames >= (extended_smoke ? 180u : 2u) && !captured && !options.capture_file.empty();
            if (capture_frame &&
                !capture.initialize(runtime.gpu, SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window), width,
                                    height))
            {
                spdlog::error("Capture target: {}", SDL_GetError());
                SDL_SubmitGPUCommandBuffer(commands);
                return 3;
            }
            if (swapchain)
            {
#if BLOOD2_INSPECTOR
                ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
#endif
                SDL_GPUColorTargetInfo color{};
                color.texture = capture_frame ? capture.texture : swapchain;
                color.clear_color = {0.025f, 0.035f, 0.05f, 1.0f};
                color.load_op = SDL_GPU_LOADOP_CLEAR;
                color.store_op = SDL_GPU_STOREOP_STORE;
                SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
                if (!pass)
                {
                    spdlog::error("Render pass: {}", SDL_GetError());
                    SDL_SubmitGPUCommandBuffer(commands);
                    return 3;
                }
#if BLOOD2_INSPECTOR
                ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
                if (ImGui::GetDrawData()->TotalVtxCount > 0)
                    ++ui_frames;
#endif
                SDL_EndGPURenderPass(pass);
                if (capture_frame && !capture.enqueue(commands, swapchain))
                {
                    spdlog::error("Capture copy: {}", SDL_GetError());
                    SDL_SubmitGPUCommandBuffer(commands);
                    return 3;
                }
            }
            if (!SDL_SubmitGPUCommandBuffer(commands))
            {
                spdlog::error("GPU submit: {}", SDL_GetError());
                return 3;
            }
            if (!swapchain)
            {
                SDL_Delay(10);
                continue;
            }
            if (capture_frame)
            {
                if (!capture.save(OpenBlood2::path_text(options.capture_file).c_str()))
                {
                    spdlog::error("Capture save: {}", SDL_GetError());
                    return 3;
                }
                captured = true;
                spdlog::info("Captured GPU frame to {}", OpenBlood2::path_text(options.capture_file));
            }
            ++frames;
            if (!inspector_visible)
                ++hidden_frames;
            if (smoke)
            {
                bool event_ok = true;
                if (options.movement_smoke)
                {
                    auto key_event = [&](SDL_Scancode code, bool down)
                    {
                        SDL_Event key{};
                        key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                        key.key.windowID = SDL_GetWindowID(runtime.window);
                        key.key.scancode = code;
                        key.key.down = down;
                        return SDL_PushEvent(&key);
                    };
                    if (frames == 2)
                    {
                        SDL_Event focus{};
                        focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
                        focus.window.windowID = SDL_GetWindowID(runtime.window);
                        event_ok = SDL_PushEvent(&focus) && key_event(SDL_SCANCODE_F2, true) &&
                                   key_event(SDL_SCANCODE_F2, false);
                    }
                    if (frames == 45)
                        event_ok = key_event(SDL_SCANCODE_W, true) && key_event(SDL_SCANCODE_SPACE, true);
                    if (frames == 55)
                        event_ok = key_event(SDL_SCANCODE_SPACE, false);
                    if (frames == 100)
                        event_ok = key_event(SDL_SCANCODE_W, false);
                    if (frames == hide_frame)
                        hidden_position = game.client_shell.position;
                    if (frames == show_frame)
                    {
                        const auto p = game.client_shell.position;
                        movement_hidden = std::hypot(p.x - hidden_position.x, p.z - hidden_position.z) > 1;
                        movement_jumped = game.client_shell.move_mgr.jumps > 0;
                        movement_landed =
                            game.client_shell.move_mgr.landings > 0 && game.client_shell.move_mgr.m_bOnGround;
                        spdlog::info("Movement: hidden distance={}, jumps={}, landings={}, ground={}, forward "
                                     "frames={}, jump frames={}",
                                     std::hypot(p.x - hidden_position.x, p.z - hidden_position.z),
                                     game.client_shell.move_mgr.jumps, game.client_shell.move_mgr.landings,
                                     game.client_shell.move_mgr.m_bOnGround, forward_frames, jump_frames);
                    }
                }
                if (options.world_smoke && frames == hide_frame)
                    hidden_model_time = game.preview_animation.time;
                if (options.world_smoke && frames == show_frame)
                    model_advanced_hidden = game.preview_model && game.preview_animation.time > hidden_model_time;
                if (options.player_smoke && frames == hide_frame)
                    hidden_player_updates = game.server_shell.Player(game.local_client)->updates;
                if (options.player_smoke && frames == show_frame)
                    player_advanced_hidden =
                        game.server_shell.Player(game.local_client)->updates > hidden_player_updates;
                if (options.weapons_smoke && frames == hide_frame)
                    hidden_weapon_shots =
                        game.server_shell.Player(game.local_client)->m_InventoryMgr.GetWeapon(1)->shots;
                if (options.weapons_smoke && frames == show_frame)
                {
                    const auto& inv = game.server_shell.Player(game.local_client)->m_InventoryMgr;
                    weapon_advanced_hidden =
                        inv.GetWeapon(1)->shots > hidden_weapon_shots && inv.GetWeapon(0)->shots > 0;
                }
                if (options.weapons_smoke && frames == unload_frame)
                {
                    const auto& inv = game.server_shell.Player(game.local_client)->m_InventoryMgr;
                    weapons_fired =
                        inv.GetWeapon(1)->shots > 0 && inv.GetWeapon(0)->shots > 0 && inv.GetAmmoCount(1) < 50;
                }
                if (frames == hide_frame || frames == show_frame)
                    event_ok = push_toggle(runtime.window);
                if (frames == 12)
                {
                    resized = false;
                    event_ok = SDL_SetWindowSize(runtime.window, 1000, 680);
                }
                if (frames == 16 && options.gpu_smoke)
                {
#if BLOOD2_INSPECTOR
                    inspector.remove_request = selection.id; // Same queued command as the UI button.
#else
                    game.objects.remove(key);
#endif
                }
                if (options.world_smoke && (frames == unload_frame || frames == reload_frame))
                {
#if BLOOD2_INSPECTOR
                    if (frames == unload_frame)
                        inspector.unload_request = true;
                    else
                    {
                        inspector.reload_request = true;
                        inspector.enter_player_request = options.player_smoke;
                    }
#else
                    if (frames == unload_frame)
                        game.unload_world();
                    else if (!game.request_world(options.world, error))
                    {
                        spdlog::error("{}", error);
                        return 5;
                    }
                    else if (options.player_smoke && !game.enter_player(error))
                    {
                        spdlog::error("{}", error);
                        return 6;
                    }
#endif
                }
                if (frames == quit_frame)
                {
                    SDL_Event quit{};
                    quit.type = SDL_EVENT_QUIT;
                    event_ok = SDL_PushEvent(&quit);
                }
                if (!event_ok)
                {
                    spdlog::error("Smoke event injection failed: {}", SDL_GetError());
                    return 4;
                }
            }
            if (options.frame_limit != 0 && frames >= options.frame_limit)
                done = true;
        }
        spdlog::info("Shutdown: frames={}, UI frames={}, hidden frames={}, toggles={}, resize={}, quit={}", frames,
                     ui_frames, hidden_frames, toggles, resized, quit_received);
        if (smoke)
        {
            int width = 0, height = 0;
            SDL_GetWindowSize(runtime.window, &width, &height);
            const bool ui_ok =
                BLOOD2_INSPECTOR ? toggles == 2 && ui_frames > 0 && hidden_frames > 0 : ui_frames == 0 && toggles == 0;
            if (frames < quit_frame || !resized || !quit_received || width != 1000 || height != 680 || !ui_ok ||
                !selection_cleared ||
                (options.gpu_smoke && (game.server.string_count() != 0 || game.objects.snapshot().size() != 1)) ||
                (options.weapons_smoke && (!weapons_fired || !weapon_advanced_hidden)) ||
                (options.movement_smoke &&
                 (!movement_hidden || !movement_jumped || !movement_landed || !game.client_shell.move_mgr.ready() ||
                  forward_frames != 55 || jump_frames != 10)) ||
                (options.player_smoke && (!player_advanced_hidden || !game.client_shell.m_bInWorld ||
                                          !game.server_shell.Player(game.local_client))) ||
                (options.world_smoke &&
                 (!world_unloaded || !model_advanced_hidden || !game.preview_model || !game.server.collision_ready ||
                  game.world_path.empty() || game.objects.snapshot().size() != initial_objects ||
                  game.server.string_count() != initial_strings)))
            {
                spdlog::error("GPU smoke failed its render/toggle/resize/quit assertions");
                return 4;
            }
            spdlog::info("GPU smoke passed: rendering, toggle, resize, quit, original destruction, string release and "
                         "selection invalidation");
            if (options.world_smoke)
                spdlog::info("World smoke passed: unload/reload, original instance/string counts restored, "
                             "collision/model resources restored, preview animation advanced while hidden");
            if (options.weapons_smoke)
                spdlog::info("Weapon smoke passed: animation-key shots, bullets consumed, knife strikes, firing while "
                             "hidden, unload/re-entry");
            if (options.movement_smoke)
                spdlog::info("Movement smoke passed: SDL commands, hidden walking/jump/landing, unload/re-entry");
            if (options.player_smoke)
                spdlog::info(
                    "Player smoke passed: shell/player entry, independent updates while hidden, teardown and re-entry");
        }
        if (!options.capture_file.empty() && !captured)
        {
            spdlog::error("Window closed before a frame could be captured");
            return 4;
        }
        return 0;
    }

} // namespace

int main(int argc, char** argv)
{
    spdlog::set_pattern("[%l] %v");
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);
    OpenBlood2::Options options;
    std::string error;
    if (!OpenBlood2::parse_options(args, options, error))
    {
        spdlog::error("{}", error);
        return 2;
    }
    if (options.help)
    {
        std::cout << "Blood II reconstruction " << OpenBlood2::build::version
                  << " (M2 in progress)\n"
                     "  --config FILE       Configuration file (default: blood2.local.ini in working directory)\n"
                     "  --data-dir DIR      External Blood II directory; overrides BLOOD2_DATA_DIR and config\n"
                     "  --check-data        Check file metadata without opening a window or parsing archives\n"
                     "  --world NAME        World resource (default: 01_SubwayA)\n"
                     "  --inspect-world     Parse/mount/create original subset headlessly; does not run a game\n"
                     "  --inspect-player    Exercise original shell/player entry headlessly; not playable\n"
                     "  --start-game        Check session startup; fails explicitly on missing M2 services\n"
                     "  --no-inspector      Start with the inspector hidden\n"
                     "  --frames N          Exit after N presented frames\n"
                     "  --capture FILE.png  Save the third GPU frame after UI layout has settled\n"
                     "  --gpu-smoke         Exercise GPU presentation, inspector toggle, resize and quit\n"
                     "  --world-smoke       GPU lifecycle with real world unload/reload assertions\n"
                     "  --player-smoke      GPU shell/player entry, hidden updates and unload/re-entry\n"
                     "  --weapons-smoke     GPU Beretta/knife firing, hidden simulation and reload\n"
                     "  --movement-smoke    GPU SDL movement, jump/landing, hidden simulation and reload\n"
                     "  --help              Show this help\n";
        return 0;
    }
    OpenBlood2::Config config;
    OpenBlood2::DataStatus data;
    const char* environment = std::getenv("BLOOD2_DATA_DIR");
    if (!OpenBlood2::load_config(options, environment ? environment : "", config, error) ||
        !OpenBlood2::inspect_data(config, data, error))
    {
        spdlog::error("{}", error);
        return 2;
    }
    spdlog::info("External data: {} ({})", OpenBlood2::path_text(config.data_dir), config.source);
    for (const auto& archive : data.archives)
    {
        spdlog::info("{}: {}, {} bytes", archive.name, archive.present ? "detected" : "absent (optional)",
                     archive.bytes);
    }
    if (!data.readme_version.empty())
        spdlog::info("Readme declares: {}", data.readme_version);
    if (options.check_data)
    {
        spdlog::info("Metadata only; no archives mounted, no game objects or resources loaded");
        return 0;
    }
    if (options.inspect_world || options.start_game)
    {
        OpenBlood2::GameSession game;
        if (!game.initialize() || !game.resources.mount_game(config, error) ||
            !game.request_world(options.world, error))
        {
            spdlog::error("World load: {}", error);
            return 5;
        }
        const auto* resource = game.resources.find(game.world_path);
        spdlog::info("World {} from {} ({} overrides)", game.world_path,
                     OpenBlood2::path_text(resource->archive.filename()), resource->overrides);
        spdlog::info("{} resources in {} mounts; DAT v{}: {} objects, {} original instances, {} BSP models",
                     game.resources.index().size(), game.resources.mounts().size(), game.world.version,
                     game.world.objects.size(), game.objects.snapshot().size(), game.world.models.size());
        std::map<std::string, std::size_t> counts;
        for (std::size_t i = 0; i < game.entries.size(); ++i)
            ++counts[game.world.objects[i].class_name + " / " + OpenBlood2::entry_state_name(game.entries[i].state)];
        for (const auto& [name, count] : counts)
            spdlog::info("{}: {}", name, count);
        if (options.inspect_player && !game.enter_player(error))
        {
            spdlog::error("Player entry: {}", error);
            return 6;
        }
        if (!game.step(1.0 / 60.0))
            return 5;
        if (options.start_game && !game.run_world(error))
        {
            spdlog::error("{}", error);
            return 6;
        }
        if (options.inspect_player)
            spdlog::info("Original shell/player entry inspection succeeded: CPlayerObj at {}, {}, {}; full gameplay "
                         "remains blocked",
                         game.client_shell.position.x, game.client_shell.position.y, game.client_shell.position.z);
        else
            spdlog::info("World inspection succeeded; full gameplay remains blocked");
        game.unload_world();
        if (game.server.string_count() || game.server.list_count() || game.server.message_count() ||
            game.server.client_count())
            return 5;
        return 0;
    }
    return run(options, config, data);
}
