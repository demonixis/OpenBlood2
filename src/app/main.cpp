#include "app/movement_controls.h"
#include "brush_smoke.h"
#include "build_info.h"
#include "camera_smoke.h"
#include "cinematic_smoke.h"
#include "engine/capture.h"
#include "engine/config.h"
#include "engine/runtime.h"
#include "game/original/Door.h"
#include "game/server_adapter.h"
#include "interaction_smoke.h"
#include "keyframe_smoke.h"
#include "light_smoke.h"
#include "objectives_smoke.h"
#include "pickup_smoke.h"
#include "trigger_smoke.h"

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
        if ((options.cinematics_smoke || (!options.gpu_smoke && !options.world_smoke)) &&
            !game.server.audio.open_output(error))
        {
            spdlog::warn("Audio output unavailable; cue clocks remain offline: {}", error);
            if (options.cinematics_smoke)
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
                if ((options.weapons_smoke || options.movement_smoke || options.interaction_smoke) &&
                    !OpenBlood2::finish_camera_intro_for_smoke(game))
                {
                    spdlog::error("Authored intro camera did not release player control");
                    return 4;
                }
                selection.id = game.objects.get(game.client_shell.player)->id;
            }
        }
        OpenBlood2::ObjectId smoke_door{}, smoke_trigger{};
        bool key_unlocked_hidden = false;
        DVector hidden_door_position{};
        bool door_moved_hidden = false;
        if (options.doors_smoke)
        {
            for (auto id : game.objects.snapshot())
            {
                auto* r = game.objects.get(id);
                const auto* door = dynamic_cast<const Door*>(r->game.get());
                if (door && door->speed > 0 && door->move_dist > 0 && door->open_delay < .05f)
                {
                    smoke_door = selection.id = id;
                    auto text = game.server.CreateString("TRIGGER");
                    auto message = game.server.StartMessageToObject(nullptr, r->handle, 1000);
                    if (!message || game.server.WriteToMessageHString(message, text) != DE_OK ||
                        game.server.EndMessage(message) != DE_OK)
                        return 4;
                    game.server.FreeString(text);
                    break;
                }
            }
            if (!smoke_door)
                return 4;
        }
        if (options.triggers_smoke)
        {
            if (!OpenBlood2::prepare_trigger_smoke(game, smoke_door, smoke_trigger))
            {
                spdlog::error("Trigger smoke preparation failed");
                return 4;
            }
            selection.id = smoke_trigger;
        }
        if (options.interaction_smoke)
        {
            if (!OpenBlood2::prepare_interaction_smoke(game, smoke_door))
                return 4;
            selection.id = smoke_door;
        }
        OpenBlood2::ObjectId smoke_brush{}, smoke_explosion{}, smoke_body{};
        bool brush_hidden = false;
        if (options.brushes_smoke)
        {
            if (!OpenBlood2::prepare_brush_smoke(game, smoke_brush, smoke_explosion, smoke_body))
                return 4;
            selection.id = smoke_brush;
        }
        OpenBlood2::ObjectId smoke_pickup{};
        bool pickup_hidden = false, objective_hidden = false;
        if (options.pickups_smoke)
        {
            if (!OpenBlood2::prepare_pickup_smoke(game, smoke_pickup))
                return 4;
            selection.id = smoke_pickup;
        }
        OpenBlood2::ObjectId smoke_camera{};
        bool camera_hidden = false;
        if (options.cameras_smoke)
        {
            if (!OpenBlood2::prepare_camera_smoke(game, smoke_camera))
                return 4;
            selection.id = smoke_camera;
        }
        OpenBlood2::ObjectId smoke_cinematic{};
        bool cinematic_hidden = false;
        if (options.cinematics_smoke)
        {
            if (!OpenBlood2::prepare_cinematic_smoke(game, smoke_cinematic))
                return 4;
            selection.id = smoke_cinematic;
        }
        OpenBlood2::ObjectId smoke_light{};
        bool light_hidden = false;
        if (options.lights_smoke)
        {
            if (!OpenBlood2::prepare_light_smoke(game, smoke_light))
                return 4;
            selection.id = smoke_light;
        }
        OpenBlood2::ObjectId smoke_keyframe{};
        bool keyframe_hidden = false;
        if (options.keyframes_smoke)
        {
            if (!OpenBlood2::prepare_keyframe_smoke(game, smoke_keyframe, options.worldpaths_smoke))
                return 4;
            selection.id = smoke_keyframe;
        }
        // Compare reload lifetime at the same phase: original controllers copy
        // and consume their Key objects on the first normal simulation update.
        if (options.world_smoke && !game.step(1.0 / 60))
            return 4;
        bool interaction_hidden = false;
        const auto original_selection = selection.id;
        const bool conveyor_smoke = options.containers_smoke && game.world_path == "worlds/01_subwaya.dat";
        bool selection_cleared = false, world_unloaded = false;
        const auto initial_objects = game.objects.snapshot().size(), initial_strings = game.server.string_count();
        const bool smoke = options.gpu_smoke || options.world_smoke;
        bool inspector_visible = BLOOD2_INSPECTOR && options.inspector;
#if BLOOD2_INSPECTOR
        OpenBlood2::Inspector inspector(runtime);
        inspector.select_models =
            options.world_smoke && !options.player_smoke && !options.doors_smoke && !options.brushes_smoke;
        inspector.select_session = options.player_smoke && !options.weapons_smoke && !options.movement_smoke &&
                                   !options.containers_smoke && !options.triggers_smoke && !options.interaction_smoke &&
                                   !options.pickups_smoke && !options.objectives_smoke && !options.cameras_smoke &&
                                   !options.keyframes_smoke && !options.lights_smoke && !options.cinematics_smoke;
        inspector.select_weapons = options.weapons_smoke;
        inspector.select_movement = options.movement_smoke;
        inspector.select_containers = options.containers_smoke;
        inspector.select_brushes = options.brushes_smoke;
        inspector.select_pickups = options.pickups_smoke;
        inspector.select_objectives = options.objectives_smoke;
        inspector.select_cameras = options.cameras_smoke;
        inspector.select_cinematics = options.cinematics_smoke;
        inspector.cinematic_selection = smoke_cinematic;
        inspector.select_lights = options.lights_smoke;
        inspector.light_selection = smoke_light;
        inspector.select_keyframes = options.keyframes_smoke;
        inspector.keyframe_selection = smoke_keyframe;
        inspector.brush_selection = smoke_body;
        inspector.select_doors = options.doors_smoke || options.interaction_smoke;
        inspector.keyboard_movement = options.interaction_smoke;
        inspector.select_triggers = options.triggers_smoke;
        inspector.trigger_selection = smoke_trigger;
        inspector.door_selection = smoke_door;
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
        controls.enabled = options.interaction_smoke;
        controls.focused = (SDL_GetWindowFlags(runtime.window) & SDL_WINDOW_INPUT_FOCUS) != 0;
        DVector hidden_position{};
        bool movement_hidden = false, movement_jumped = false, movement_landed = false;
        unsigned forward_frames = 0, jump_frames = 0, crouch_frames = 0, run_frames = 0;
        bool crouched_hidden = false, stood_after_crouch = false, locomotion_pose = false;
        bool saw_walk = false, saw_run = false, saw_jump = false;
        bool container_hidden = false;
        float initial_air = 100;
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
            if (frames == reload_frame + 1 &&
                (options.weapons_smoke || options.movement_smoke || options.interaction_smoke) &&
                !OpenBlood2::finish_camera_intro_for_smoke(game))
                return 4;
            if (options.interaction_smoke && frames == reload_frame + 1)
                if (!OpenBlood2::prepare_interaction_smoke(game, smoke_door))
                    return 4;
            if (options.brushes_smoke && frames == reload_frame + 1)
                if (!OpenBlood2::prepare_brush_smoke(game, smoke_brush, smoke_explosion, smoke_body))
                    return 4;
            if (options.cinematics_smoke && frames == reload_frame + 1 &&
                !OpenBlood2::prepare_cinematic_smoke(game, smoke_cinematic))
                return 4;
            if (options.lights_smoke && frames == reload_frame + 1 &&
                !OpenBlood2::prepare_light_smoke(game, smoke_light))
                return 4;
            if (options.keyframes_smoke && frames == reload_frame + 1 &&
                !OpenBlood2::prepare_keyframe_smoke(game, smoke_keyframe, options.worldpaths_smoke))
                return 4;
            if (options.cameras_smoke && frames == reload_frame + 1 &&
                !OpenBlood2::prepare_camera_smoke(game, smoke_camera))
                return 4;
            if (options.objectives_smoke && frames == reload_frame + 1 && !OpenBlood2::objective_smoke(game))
                return 4;
            if (options.pickups_smoke && frames == reload_frame + 1)
                if (!OpenBlood2::prepare_pickup_smoke(game, smoke_pickup))
                    return 4;
            game.client_shell.use_down = controls.sample_use(text_editing);
            auto keyboard = controls.sample(text_editing);
            auto& movement = game.client_shell.movement_input;
            movement.forward |= keyboard.forward;
            movement.backward |= keyboard.backward;
            movement.left |= keyboard.left;
            movement.right |= keyboard.right;
            movement.run |= keyboard.run;
            movement.jump |= keyboard.jump;
            movement.crouch |= keyboard.crouch;
            if (!controls.focused && !smoke)
                movement = {};
            controls.look(game.client_shell.rotation, delta, text_editing);
            if (options.movement_smoke)
            {
                forward_frames += movement.forward;
                jump_frames += movement.jump;
                crouch_frames += movement.crouch;
                run_frames += movement.run;
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
            if (options.containers_smoke && frames == 2)
            {
                auto* player = game.server_shell.Player(game.local_client);
                DVector position = conveyor_smoke ? DVector{-32, 80.001f, 0} : DVector{0, 80, 0};
                DVector dimensions;
                OpenBlood2::CollisionHit hit;
                const auto code = conveyor_smoke ? CC_CONVEYOR : CC_WATER;
                if (!player || game.server.TeleportObject(player->m_hObject, &position) != DE_OK ||
                    game.server.GetObjectDims(player->m_hObject, &dimensions) != DE_OK ||
                    game.collision.sweep(position, position, dimensions, hit) != OpenBlood2::CollisionQuery::Miss ||
                    !player->TeleportClientToServerPos() || !game.pump_player_messages(error) ||
                    !player->UpdateContainerState() || player->containers.count == 0 ||
                    player->containers.body[0].code != code ||
                    (!conveyor_smoke && player->containers.head.code != code))
                {
                    spdlog::error("Container smoke requires CityHub canal or SubwayA conveyor geometry");
                    return 6;
                }
                initial_air = player->m_fAirLevel;
#if BLOOD2_INSPECTOR
                inspector.container_selection = game.objects.get(player->containers.body[0].object)->id;
#endif
            }
            if (!game.step(delta))
            {
                spdlog::error("Object simulation step failed");
                return 4;
            }
            if (options.movement_smoke && game.local_client)
            {
                const auto* player = game.server_shell.Player(game.local_client);
                const auto* record = player ? game.objects.get(player->m_hObject) : nullptr;
                if (player && record)
                {
                    saw_walk |= player->move_state == CPlayerObj::MoveState::Walk;
                    saw_run |= player->move_state == CPlayerObj::MoveState::Run;
                    saw_jump |= player->move_state == CPlayerObj::MoveState::Jump;
                    if (frames > 105 && frames < 125)
                        crouched_hidden |= player->crouched && player->eye_level == 12;
                    if (frames > 130 && frames < 160)
                        stood_after_crouch |= !player->crouched && player->eye_level == 36;
                    if (frames == 180 && record->model)
                    {
                        std::vector<DVector> vertices;
                        locomotion_pose = player->crouched &&
                                          sample_vertices(*record->model, record->animation.animation,
                                                          record->animation.time, vertices) &&
                                          vertices.size() == record->model->vertices.size();
                    }
                }
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
                swapchain &&
                frames >= (extended_smoke ? 180u
                           : (options.containers_smoke || options.doors_smoke || options.triggers_smoke ||
                              options.interaction_smoke || options.brushes_smoke || options.pickups_smoke ||
                              options.objectives_smoke || options.cameras_smoke || options.keyframes_smoke ||
                              options.lights_smoke || options.cinematics_smoke)
                               ? 14u
                               : 2u) &&
                !captured && !options.capture_file.empty();
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
                    if (frames == 70)
                        event_ok = key_event(SDL_SCANCODE_LSHIFT, true);
                    if (frames == 90)
                        event_ok = key_event(SDL_SCANCODE_LSHIFT, false);
                    if (frames == 100)
                        event_ok = key_event(SDL_SCANCODE_W, false) && key_event(SDL_SCANCODE_LCTRL, true);
                    if (frames == 125 || frames == 190)
                        event_ok = key_event(SDL_SCANCODE_LCTRL, false);
                    if (frames == 170)
                        event_ok = key_event(SDL_SCANCODE_LCTRL, true);
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
                if ((options.doors_smoke || options.triggers_smoke) && frames == hide_frame)
                {
                    hidden_door_position = game.objects.get(smoke_door)->state.m_Pos;
                    if (options.triggers_smoke)
                        key_unlocked_hidden = OpenBlood2::activate_trigger_smoke(game, smoke_trigger);
                }
                if ((options.doors_smoke || options.triggers_smoke) && frames == show_frame)
                {
                    const auto delta =
                        OpenBlood2::vector_subtract(game.objects.get(smoke_door)->state.m_Pos, hidden_door_position);
                    door_moved_hidden = OpenBlood2::vector_dot(delta, delta) > .01f;
                }
                if (options.interaction_smoke && (frames == hide_frame || frames == show_frame - 1))
                {
                    if (frames == hide_frame)
                    {
                        SDL_Event focus{};
                        focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
                        focus.window.windowID = SDL_GetWindowID(runtime.window);
                        if (!SDL_PushEvent(&focus))
                            return 4;
                    }
                    SDL_Event use{};
                    use.type = frames == hide_frame ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                    use.key.windowID = SDL_GetWindowID(runtime.window);
                    use.key.key = SDLK_E;
                    use.key.scancode = SDL_SCANCODE_E;
                    use.key.down = frames == hide_frame;
                    event_ok = SDL_PushEvent(&use);
                }
                if (options.cinematics_smoke && frames == show_frame - 1)
                    cinematic_hidden = OpenBlood2::check_cinematic_smoke(game, smoke_cinematic);
                if (options.lights_smoke && frames == show_frame - 1)
                    light_hidden = OpenBlood2::check_light_smoke(game, smoke_light);
                if (options.keyframes_smoke && frames == show_frame - 1)
                    keyframe_hidden = OpenBlood2::check_keyframe_smoke(game, smoke_keyframe, options.worldpaths_smoke);
                if (options.cameras_smoke && frames == show_frame - 1)
                    camera_hidden = OpenBlood2::check_camera_smoke(game, smoke_camera);
                if (options.objectives_smoke && frames == show_frame - 1)
                    objective_hidden = OpenBlood2::objective_smoke(game);
                if (options.pickups_smoke && frames == show_frame - 1)
                    pickup_hidden = OpenBlood2::collect_pickup_smoke(game, smoke_pickup);
                if (options.brushes_smoke)
                {
                    if (frames == hide_frame && !OpenBlood2::command_brush_smoke(game, smoke_explosion, "KILL"))
                        return 4;
                    if (frames == hide_frame || frames == 11)
                        if (!OpenBlood2::command_brush_smoke(game, smoke_brush, "ON"))
                            return 4;
                    if (frames == show_frame)
                    {
                        brush_hidden = OpenBlood2::check_brush_smoke(game, smoke_brush, smoke_body);
                        if (!OpenBlood2::command_brush_smoke(game, smoke_brush, "OFF"))
                            return 4;
                    }
                }
                if (options.interaction_smoke && frames == show_frame)
                    interaction_hidden = OpenBlood2::check_interaction_smoke(game, smoke_door);
                if (conveyor_smoke && frames == hide_frame)
                    hidden_position = game.client_shell.position;
                if (options.containers_smoke && frames == show_frame)
                {
                    const auto* player = game.server_shell.Player(game.local_client);
                    container_hidden = conveyor_smoke ? player && game.client_shell.move_mgr.m_bBodyOnConveyor &&
                                                            game.client_shell.move_mgr.m_bUseGravity &&
                                                            game.client_shell.position.z < hidden_position.z - .05f
                                                      : player && player->m_fAirLevel < initial_air &&
                                                            player->containers.head.code == CC_WATER &&
                                                            game.client_shell.move_mgr.m_bBodyInLiquid &&
                                                            !game.client_shell.move_mgr.m_bUseGravity;
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
                (options.containers_smoke && !container_hidden) || (options.doors_smoke && !door_moved_hidden) ||
                (options.triggers_smoke && (!door_moved_hidden || !key_unlocked_hidden)) ||
                (options.cinematics_smoke &&
                 (!cinematic_hidden || !OpenBlood2::check_cinematic_smoke(game, smoke_cinematic))) ||
                (options.lights_smoke && (!light_hidden || !OpenBlood2::check_light_smoke(game, smoke_light))) ||
                (options.keyframes_smoke &&
                 (!keyframe_hidden ||
                  !OpenBlood2::check_keyframe_smoke(game, smoke_keyframe, options.worldpaths_smoke))) ||
                (options.cameras_smoke && (!camera_hidden || !OpenBlood2::check_camera_smoke(game, smoke_camera))) ||
                (options.objectives_smoke && !objective_hidden) || (options.pickups_smoke && !pickup_hidden) ||
                (options.interaction_smoke && !interaction_hidden) || (options.brushes_smoke && !brush_hidden) ||
                (options.weapons_smoke && (!weapons_fired || !weapon_advanced_hidden)) ||
                (options.movement_smoke &&
                 (!movement_hidden || !movement_jumped || !movement_landed || !game.client_shell.move_mgr.ready() ||
                  forward_frames != 55 || jump_frames != 10 || crouch_frames != 45 || run_frames != 20 ||
                  !crouched_hidden || !stood_after_crouch || !locomotion_pose || !saw_walk || !saw_run || !saw_jump)) ||
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
            if (options.lights_smoke)
                spdlog::info("LIGHTS smoke: 12 authored SubwayA point lights follow original keyframes while UI hidden "
                             "and after reload; evaluated radius/color checked; world illumination unavailable");
            if (options.cinematics_smoke)
                spdlog::info("CINEMATICS smoke: original cue and decoded samples submitted to SDL stream with hidden "
                             "UI and reload; dry output only");
            if (options.worldpaths_smoke)
                spdlog::info(
                    "WORLDPATHS smoke: original SubwayA train follows keys, posed collision checked while UI hidden "
                    "and after reload; angular paths and world rendering remain unavailable");
            else if (options.keyframes_smoke)
                spdlog::info("KEYFRAMES smoke: authored camera path and owned key lifetime checked while UI hidden and "
                             "after reload; missing Gideon message exposed");
            if (options.cameras_smoke)
                spdlog::info("CAMERAS smoke: original camera tracks player while UI hidden; selected view, movement "
                             "gate and reload checked");
            if (options.objectives_smoke)
                spdlog::info("OBJECTIVES smoke: original trigger and localized client text delivered with hidden UI; "
                             "unload/reload checked");
            if (options.pickups_smoke)
                spdlog::info("PICKUPS smoke: original ammo accepted during hidden UI; unload/reload checked");
            if (options.weapons_smoke)
                spdlog::info("Weapon smoke passed: animation-key shots, bullets consumed, knife strikes, firing while "
                             "hidden, unload/re-entry");
            if (options.movement_smoke)
                spdlog::info("Movement smoke passed: SDL walk/run/jump/crouch, hidden stance changes, live ABC pose, "
                             "unload/re-entry");
            if (options.brushes_smoke)
                spdlog::info("Brush smoke passed: original ON/OFF ramps, hidden rotation/rider transport and explosive "
                             "chain, dynamic box gravity/impulse, inspection, reload");
            if (options.interaction_smoke)
                spdlog::info("Interaction smoke passed: SDL E press edge, USE key denial/unlock, rotation while "
                             "hidden, inspection, reload");
            if (options.triggers_smoke)
                spdlog::info("Trigger smoke passed: missing-key denial, real key pickup, consumption/unlock while "
                             "hidden, door motion, inspector, reload");
            if (options.doors_smoke)
                spdlog::info(
                    "Door smoke passed: original trigger, translation while hidden, inspection, unload/reload");
            if (options.containers_smoke)
                spdlog::info("Container smoke passed: {}, MoveMgr while hidden, volume inspector, unload/re-entry",
                             conveyor_smoke ? "Subway conveyor current and gravity" : "CityHub canal and air");
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
                     "  --doors-smoke       GPU original sliding door inspection\n"
                     "  --brushes-smoke GPU original rotating brush inspection\n"
                     "  --interaction-smoke GPU original USE and rotating door inspection\n"
                     "  --pickups-smoke   GPU original ammo pickup, hidden collection and reload\n"
                     "  --objectives-smoke GPU original objectives and localized text, hidden delivery/reload\n"
                     "  --cinematics-smoke  GPU original cue, WAV mixing, SDL output and reload\n"
                     "  --lights-smoke      GPU original light state, keyframe placement and reload\n"
                     "  --worldpaths-smoke  GPU original translating train path, collision and reload\n"
                     "  --keyframes-smoke   GPU original camera key path, key lifetime and reload\n"
                     "  --cameras-smoke     GPU original camera tracking, player gates and reload\n"
                     "  --triggers-smoke    GPU Trigger, SelfTrigger and key inspection\n"
                     "  --containers-smoke  GPU liquid/air inspection; pass --world 02_CityHub\n"
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
