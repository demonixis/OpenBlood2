#include "app/network_frontend.h"
#include "app/client_scene.h"
#include "app/control_bindings.h"
#include "app/display_options.h"
#include "app/gamepad_devices.h"
#include "app/hud.h"
#include "app/movement_controls.h"
#include "app/network_controls.h"
#include "app/scene_presenter.h"
#include "app/scoreboard.h"
#include "app/ui_runtime.h"
#include "client/network_client.h"
#include "engine/capture.h"
#include "game/game_settings.h"
#include "game/menu_preferences.h"
#include "net/address.h"
#include "server/network.h"
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlgpu3.h>
#include <imgui.h>
#include <memory>
#include <spdlog/spdlog.h>

namespace OpenBlood2
{
    int run_network_frontend(const Options& options, const Config& config)
    {
        std::string error;
        const auto failed = [&]()
        {
            spdlog::error("Network session: {}", error.empty() ? SDL_GetError() : error);
            return 5;
        };
        Net::JoinProfile profile;
        profile.name = options.network_name;
        profile.team = options.network_team;
        if (!Net::valid_profile(profile))
        {
            error = "Invalid multiplayer player profile";
            return failed();
        }
        Net::ServerAddress address;
        // A remote client never constructs a server. The playable host owns exactly
        // one simulation and admits its local client through the same ENet path.
        std::unique_ptr<ServerSession> server;
        std::unique_ptr<ServerNetwork> network;
        if (!options.host_world.empty())
        {
            server = std::make_unique<ServerSession>();
            if (!server->start(config, options.host_world, options.network_mode, error, options.network_rules))
                return failed();
            network = std::make_unique<ServerNetwork>(*server);
            if (!network->listen("0.0.0.0", options.network_port, unsigned(Net::max_players), error))
                return failed();
            address = {"127.0.0.1", network->port()};
            spdlog::info("Playable host: {}, UDP {}", options.host_world, network->port());
        }
        else if (!Net::parse_address(options.connect_address, options.network_port, address, error))
            return failed();

        Runtime runtime;
        if (!runtime.initialize(false, error))
            return failed();
        UiRuntime ui(runtime);
        if (!ui.initialize(error))
            return failed();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        ScenePresenter presenter(runtime.gpu);
        HudRenderer hud(runtime.gpu);
        GamepadDevices devices;
        MovementControls controls;
        NetworkControls actions;
        std::vector<Net::CommandAction> sampled_actions;
        MenuSettings settings;
        std::filesystem::path preferences, bindings;
        if (!menu_preferences_path(preferences, error) || !load_menu_preferences(preferences, settings, error) ||
            !controls_path(bindings, error) || !load_controls(bindings, controls.bindings, error))
            return failed();
        Menu display_menu;
        display_menu.settings = settings;
        DisplayOptions display(runtime);
        if (!display.apply(display_menu, error))
            return failed();
        controls.mouse_sensitivity = .0025f * (settings.sensitivity / 150.f);
        controls.invert_y = settings.invert_y;
        controls.run_lock = settings.run_lock;
        controls.mouse_look = settings.mouse_look;
        controls.focused = (SDL_GetWindowFlags(runtime.window) & SDL_WINDOW_INPUT_FOCUS) != 0;
        presenter.lightmaps = settings.detail >= 1;
        presenter.model_shadows = options.model_shadows && settings.detail >= 2;
        presenter.per_pixel_lighting = settings.pixel_lighting;
        presenter.shading = !settings.renderer ? SurfaceShading::Original
                            : settings.quality ? SurfaceShading::High
                                               : SurfaceShading::Standard;
        presenter.normal_maps = settings.normal_maps;
        presenter.post = {Antialiasing(settings.antialiasing), settings.bloom, Tonemap(settings.tonemap),
                          AmbientOcclusion(settings.ambient_occlusion), Reflections(settings.reflections)};
        const auto window = SDL_GetWindowID(runtime.window);
        const auto format = SDL_GetGPUSwapchainTextureFormat(runtime.gpu, runtime.window);
        NetworkClient client;
        ClientFragTable frag_table(client.resources());
        ScoreboardRenderer frag_renderer(runtime.gpu);
        ScoreboardControl scores;
        if (!client.prepare(config, error) ||
            !client.connect(address.host, address.port, profile, NetworkClient::Clock::now(), error))
            return failed();
        spdlog::info("Connecting to {}:{} as {}", address.host, address.port, profile.name);
        if (!client.audio().mixer.set_gains(settings.sound ? settings.sound_volume / 50.f : 0,
                                            settings.music ? settings.music_volume / 100.f : 0) ||
            !client.audio().mixer.open_output(error))
            return failed();
        client.audio().mixer.output_lead = AudioMixer::interactive_output_lead;
        ServerClock server_clock;
        auto previous = NetworkClient::Clock::now();
        bool menu = false, leaving = false, captured = false;
        std::uint64_t entered_world = 0, weapon_wait_seen = 0, weapon_frames_seen = 0;
        bool weapon_stalled = false;
        unsigned frames = 0;
        std::vector<Net::PrivateEvent> events;
        const auto leave = [&]()
        {
            if (leaving)
                return true;
            if (!client.disconnect(NetworkClient::Clock::now()))
            {
                error = "Cannot begin client departure";
                return false;
            }
            leaving = true;
            return true;
        };
        while (client.phase() != NetworkClient::Phase::Closed)
        {
            const auto frame_start = SDL_GetTicksNS();
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                ImGui_ImplSDL3_ProcessEvent(&event);
                devices.event(event);
                scores.event(event, window,
                             client.ready() && !client.changing_level() && controls.focused && !menu && !leaving);
                if (event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == window))
                    if (!leave())
                        return failed();
                if ((event.type == SDL_EVENT_KEY_DOWN && event.key.windowID == window && !event.key.repeat &&
                     event.key.scancode == SDL_SCANCODE_ESCAPE) ||
                    (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && event.gbutton.button == SDL_GAMEPAD_BUTTON_START))
                {
                    menu = !menu;
                    if (!menu)
                        controls.request_play();
                }
                controls.enabled = !menu && !leaving;
                controls.event(event, window, menu);
                if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED && event.window.windowID == window)
                {
                    devices.resync(controls);
                    if (!menu)
                        controls.request_play();
                }
            }
            const auto now = NetworkClient::Clock::now();
            const double seconds = std::chrono::duration<double>(now - previous).count();
            previous = now;
            // Focus and menus gate commands only; neither suspends the server.
            if (server)
            {
                unsigned ticks = 0;
                if (!server_clock.advance(seconds, ticks))
                {
                    error = "Invalid host clock";
                    return failed();
                }
                for (unsigned i = 0; i < ticks; ++i)
                    if (!server->tick(error))
                        return failed();
                if (!network->pump(now, error))
                    return failed();
            }
            controls.enabled = !menu && !leaving;
            const auto input = controls.sample(menu);
            PlayerRotation aim{client.pitch(), client.yaw()};
            controls.look(aim, seconds, menu, client.zoomed());
            if (!client.look(aim.pitch - client.pitch(), aim.yaw - client.yaw()))
                return failed();
            actions.sample(controls, menu || leaving || !client.ready() || client.changing_level(),
                           {client.pitch(), client.yaw()}, sampled_actions);
            for (const auto& action : sampled_actions)
                if (!client.command(action, error))
                    spdlog::info("Player command: {}", error);
            // Preserve reliable Control order: selection/use precede this frame's
            // weapon timeline. Existing bindings suppress held fire across menus/focus.
            const auto fire = controls.sample_fire(menu || leaving || !client.ready() || client.changing_level());
            if (!client.update(now, input, controls.enabled && controls.focused, error, {fire.primary, fire.alternate}))
            {
                if (server)
                {
                    std::optional<Net::WeaponFrameAck> ack;
                    server->weapon_frame_ack(client.joined().player, ack);
                    spdlog::error(
                        "Host clock: ticks={}, discarded={}, state_updates={}, state_drops={}, local_processed={}",
                        server->ticks(), server_clock.discarded_ticks(), network->state_updates, network->state_drops,
                        ack ? ack->sequence : 0);
                }
                return failed();
            }
            // A full ACK window pauses only the weapon timeline; name each stall
            // once. It ends when a weapon frame is sent again.
            if (client.weapon_wait_ticks != weapon_wait_seen && !weapon_stalled)
            {
                spdlog::warn("Weapon input waits for server acknowledgements (frames={})", client.weapon_frames);
                weapon_stalled = true;
            }
            else if (client.weapon_frames != weapon_frames_seen)
                weapon_stalled = false;
            weapon_wait_seen = client.weapon_wait_ticks;
            weapon_frames_seen = client.weapon_frames;
            // HUD/camera and logical sound events are applied by the endpoint.
            // Speech consumes original combat announcements; music remains separate.
            client.take_events(events);
            if (client.ready() && entered_world != client.joined().world)
            {
                entered_world = client.joined().world;
                scores = {};
                controls.request_play();
                spdlog::info("Network world ready: {}, player {}", client.offer().path, client.joined().player);
            }
            for (const auto& applied : events)
                scores.receive(applied);
            if (client.changing_level())
                scores.visible = true;
            if (!controls.sync_mouse(runtime.window, client.ready(), menu || leaving, error))
                return failed();
            if (client.phase() == NetworkClient::Phase::Closed)
                break;
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            const bool visible = client.ready();
            const auto& camera = client.camera();
            if (visible)
            {
                const auto& values = client.hud();
                auto state =
                    hud_state(values.stats, true, bool(camera.view.object), camera.death.active,
                              !scores.visible && (settings.crosshair || client.zoom_crosshair()),
                              client.zoom_crosshair() ? client.zoom_crosshair() : DBYTE(settings.crosshair_number),
                              client.players().team(), client.players().flag_icon());
                state.items = values.items;
                state.item_messages = values.item_messages;
                state.item_shows = values.item_shows;
                state.time = float(client.time());
                state.world_revision = client.joined().world;
                if (!hud.draw(state, client.resources(), error))
                    return failed();
                draw_letterbox(camera.view.object && camera.view.settings.type == 1);
                if (scores.visible && !menu && !leaving)
                {
                    const auto size = ImGui::GetMainViewport()->Size;
                    const auto density = ImGui::GetIO().DisplayFramebufferScale;
                    const auto width = unsigned(size.x * density.x), height = unsigned(size.y * density.y);
                    const bool composed =
                        team_game_type(client.players().mode())
                            ? frag_table.compose_teams(client.players().roster(), client.players().team_frags(),
                                                       scores.team_column, width, height, error)
                            : frag_table.compose(client.players().scores(), width, height, error);
                    if (!composed || !frag_renderer.draw(frag_table, error))
                        return failed();
                }
                const auto origin = ImGui::GetMainViewport()->Pos;
                float y = origin.y;
                for (const auto& line : client.speech().notices())
                {
                    // Original MessageMgr coordinates/line height; the portable
                    // UI font replaces its Windows Arial surface renderer.
                    ImGui::GetForegroundDrawList()->AddText(ImGui::GetFont(), 16, {origin.x + 1, y},
                                                            IM_COL32(255, 255, 255, 255), line.text.c_str());
                    y += 16;
                }
            }
            if (menu || !visible || leaving)
            {
                ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, {0.5f, 0.5f});
                ImGui::Begin("Multiplayer", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
                ImGui::TextUnformatted(leaving   ? "Leaving..."
                                       : visible ? "Game continues while this menu is open."
                                                 : "Connecting / loading world...");
                if (!leaving)
                {
                    if (visible && ImGui::Button("Resume"))
                    {
                        menu = false;
                        controls.request_play();
                    }
                    if (ImGui::Button(server ? "End hosted game" : "Leave game"))
                        if (!leave())
                            return failed();
                }
                ImGui::End();
            }
            ImGui::Render();
            auto* commands = SDL_AcquireGPUCommandBuffer(runtime.gpu);
            if (!commands)
                return failed();
            SDL_GPUTexture* swapchain = nullptr;
            Uint32 width = 0, height = 0;
            if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, runtime.window, &swapchain, &width, &height))
            {
                SDL_CancelGPUCommandBuffer(commands);
                return failed();
            }
            if (!swapchain)
            {
                SDL_SubmitGPUCommandBuffer(commands);
                SDL_Delay(1);
                continue;
            }
            Capture capture;
            const bool last = visible && options.frame_limit && frames + 1 >= options.frame_limit;
            const bool capture_frame = !captured && !options.capture_file.empty() &&
                                       (last || (visible && !options.frame_limit && frames >= 2));
            if (capture_frame && !capture.initialize(runtime.gpu, format, width, height))
            {
                SDL_CancelGPUCommandBuffer(commands);
                return failed();
            }
            auto* target = capture_frame ? capture.texture : swapchain;
            if (visible)
            {
                ClientScene scene{client.geometry(), client.resources(), client.world(), client.effects().scene()};
                scene.world_revision = client.joined().world;
                scene.world_path = client.offer().path;
                scene.view = camera.view;
                scene.in_world = true;
                scene.player_visible = camera.player_visible;
                scene.weapons_visible = camera.weapons_visible;
                scene.pitch = client.pitch();
                scene.yaw = client.yaw();
                scene.camera_position = camera.camera_position;
                scene.fov_x = client.fov_x();
                scene.weapons = {&client.weapons().views()[0], &client.weapons().views()[1]};
                if (!presenter.update(scene, error) ||
                    !presenter.record(scene, commands, target, format, width, height, error))
                {
                    SDL_CancelGPUCommandBuffer(commands);
                    return failed();
                }
            }
            ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), commands);
            SDL_GPUColorTargetInfo color{};
            color.texture = target;
            color.load_op = visible ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
            color.store_op = SDL_GPU_STOREOP_STORE;
            color.clear_color = {0, 0, 0, 1};
            auto* pass = SDL_BeginGPURenderPass(commands, &color, 1, nullptr);
            if (!pass)
            {
                SDL_CancelGPUCommandBuffer(commands);
                return failed();
            }
            ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), commands, pass);
            SDL_EndGPURenderPass(pass);
            if (capture_frame && !capture.enqueue(commands, swapchain, width, height))
            {
                SDL_CancelGPUCommandBuffer(commands);
                return failed();
            }
            if (!SDL_SubmitGPUCommandBuffer(commands) ||
                (capture_frame && !capture.save(path_text(options.capture_file).c_str())))
                return failed();
            captured |= capture_frame;
            if (visible)
                ++frames;
            if (last && !leave())
                return failed();
            DisplayOptions::pace(frame_start, settings.fps_limit);
        }
        controls.release_mouse();
        if (network && !network->close())
        {
            error = "Cannot close hosted game";
            return failed();
        }
        SDL_WaitForGPUIdle(runtime.gpu);
        spdlog::info("Network session ended: frames={}, reports={}, states={}, discarded_ticks={}, reason={}", frames,
                     client.reports, client.state_packets, client.discarded_ticks, client.reason());
        spdlog::info(
            "Ballistic effects: received={}, failed={}, models={}, billboards={}, cosmetic_sounds={}, error={}",
            client.effects().received, client.effects().failed, client.effects().scene().frame().models.size(),
            client.effects().scene().frame().billboards.size(), client.audio().effects_started,
            client.effects().last_error);
        spdlog::info("Player commands: acknowledged={}, applied={}", client.command_acks, client.commands_applied);
        spdlog::info("Weapon input: frames={}, waiting_ticks={}", client.weapon_frames, client.weapon_wait_ticks);
        if (server)
            spdlog::info("Host stopped: ticks={}, discarded_ticks={}, admitted={}, departed={}, rotations={}",
                         server->total_ticks(), server_clock.discarded_ticks(), network->admitted, network->departed,
                         server->rotations);
        return 0;
    }
} // namespace OpenBlood2
