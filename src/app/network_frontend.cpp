#include "app/network_frontend.h"
#include "app/client_scene.h"
#include "app/control_bindings.h"
#include "app/display_options.h"
#include "app/gamepad_devices.h"
#include "app/hud.h"
#include "app/menu_input.h"
#include "app/menu_renderer.h"
#include "app/network_controls.h"
#include "app/scene_presenter.h"
#include "app/scoreboard.h"
#include "app/ui_runtime.h"
#include "client/network_client.h"
#include "engine/capture.h"
#include "game/game_settings.h"
#include "game/localized_font.h"
#include "game/localized_signs.h"
#include "game/menu_preferences.h"
#include "game/original/ConversationTrigger.h"
#include "net/address.h"
#include "net/discovery.h"
#include "server/network.h"
#include <algorithm>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlgpu3.h>
#include <imgui.h>
#include <memory>
#include <spdlog/spdlog.h>

namespace OpenBlood2
{
    namespace
    {
        constexpr int resume_option = 0, leave_option = 1, subtitles_option = 2;

        // CLI sessions can still use the same menu controls if their data is missing the original menu art.
        void draw_network_menu_fallback(Menu& menu, std::string_view title, ImFont* font,
                                        std::string_view subtitles_label)
        {
            const auto* viewport = ImGui::GetMainViewport();
            const auto fit = MenuViewport::fit(viewport->Size.x, viewport->Size.y);
            auto* draw = ImGui::GetBackgroundDrawList();
            draw->AddRectFilled(viewport->Pos, {viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y},
                                IM_COL32(0, 0, 0, 255));
            const auto text = [&](std::string_view value, int y, float size, ImU32 color, float wrap = 0.f)
            {
                if (!value.empty())
                    draw->AddText(font, size * fit.scale,
                                  {viewport->Pos.x + fit.x + 60 * fit.scale, viewport->Pos.y + fit.y + y * fit.scale},
                                  color, value.data(), value.data() + value.size(), wrap * fit.scale);
            };
            text(title, 95, 36, IM_COL32(220, 40, 30, 255));
            for (std::size_t i = 0; i < menu.items.size(); ++i)
            {
                auto& item = menu.items[i];
                const int y = 175 + 40 * int(i);
                item.rect = {60, y, 580, y + 40};
                const auto label = item.setting == menu_subtitles_setting ? subtitles_label
                    : std::string_view(item.localized_literal.empty() ? item.literal : item.localized_literal);
                text(label, y, 26,
                     int(i) == menu.selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(185, 185, 185, 255));
            }
            text(menu.multiplayer_status, 385, 18, IM_COL32(220, 220, 220, 255), 520);
        }
    } // namespace

    int run_network_session(const Options& options, const Config& config, UiRuntime& ui, std::string& error,
                            bool& quit, const LocalizedFont& localized_font, MenuSettings* shared_settings)
    {
        auto& runtime = ui.runtime;
        auto* localized_face = ui.localized;
        error.clear();
        quit = false;
        struct RestoreInput
        {
            SDL_Window* window;
            ~RestoreInput()
            {
                SDL_SetWindowRelativeMouseMode(window, false);
                SDL_StopTextInput(window);
                ImGui::GetIO().ClearInputKeys();
                ImGui::GetIO().ClearInputMouse();
            }
        } restore_input{runtime.window};
        const auto failed = [&]()
        {
            if (error.empty())
                error = SDL_GetError();
            if (error.empty())
                error = "Network session could not start or continue.";
            spdlog::error("Network session: {}", error);
            return 5;
        };
        Net::JoinProfile profile;
        profile.name = options.network_name;
        profile.team = options.network_team;
        profile.character = options.network_character;
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
            if (!network->discovery_error.empty())
                spdlog::warn("{}", network->discovery_error);
            address = {"127.0.0.1", network->port()};
            spdlog::info("Playable host: {}, UDP {}", options.host_world, network->port());
        }
        else if (!Net::parse_address(options.connect_address, options.network_port, address, error))
            return failed();

        ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);
        ScenePresenter presenter(runtime.gpu);
        GamepadDevices devices;
        MenuInput menu_input;
        auto& controls = menu_input.controls;
        NetworkControls actions;
        std::vector<Net::CommandAction> sampled_actions;
        MenuSettings local_settings;
        auto& settings = shared_settings ? *shared_settings : local_settings;
        std::filesystem::path preferences, bindings;
        if (!menu_preferences_path(preferences, error) ||
            (!shared_settings && !load_menu_preferences(preferences, settings, error)) ||
            !controls_path(bindings, error) || !load_controls(bindings, controls.bindings, error))
            return failed();
        Localization localization;
        std::string language_error;
        if (localized_face && !localization.load(settings.language, language_error))
            spdlog::warn("Language {}: {}. Using English.", language_tag(settings.language), language_error);
        const bool localized = localized_face && localization.language() != Language::English;
        HudRenderer hud(runtime.gpu);
        if (localized)
        {
            hud.set_localization(&localization, &localized_font);
            presenter.set_texture_decoration(sign_texture_decoration(localization, localized_font));
        }
        const auto translated = [&](std::string_view key)
        {
            const auto value = localization.text(key);
            return localized && localized_font.supports(value) ? value : std::string_view{};
        };
        const auto tr = [&](std::string_view key, std::string_view english)
        {
            const auto value = translated(key);
            return value.empty() ? english : value;
        };
        if (network)
        {
            auto suffix = tr("network.host_suffix", "(host)");
            if (suffix.size() > Net::discovery_name_limit - 5 || suffix.find('\t') != suffix.npos)
                suffix = "(host)";
            const auto limit = std::min<std::size_t>(20, Net::discovery_name_limit - 1 - suffix.size());
            // Fit the offer limit including the translated suffix and preserve UTF-8 character boundaries.
            auto name = profile.name.substr(0, limit);
            if (name.size() < profile.name.size())
                while (!name.empty() && (std::uint8_t(profile.name[name.size()]) & 0xC0) == 0x80)
                    name.pop_back();
            network->server_name = name + " " + std::string(suffix);
        }
        Menu menu;
        menu.settings = settings;
        DisplayOptions display(runtime);
        if (!display.apply(menu, error))
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
        if (!client.set_effect_detail(settings.detail))
        {
            error = "Invalid client effect detail preference";
            return failed();
        }
        ClientFragTable frag_table(client.resources());
        ScoreboardRenderer frag_renderer(runtime.gpu);
        ScoreboardControl scores;
        if (!client.prepare(config, error))
            return failed();
        MenuCanvas menu_canvas(client.resources());
        menu_canvas.localization = &localization;
        menu_canvas.localized_font = localized ? &localized_font : nullptr;
        MenuRenderer menu_renderer(runtime.gpu);
        std::string menu_error;
        bool native_menu = menu_canvas.initialize(menu_error);
        if (!native_menu)
            spdlog::warn("Multiplayer menu artwork: {}", menu_error);
        const auto unicode_text = [&](std::string_view text)
        {
            return std::ranges::any_of(text, [](unsigned char c) { return c >= 128; }) && localized_font.supports(text);
        };
        if (localized_font)
            frag_table.set_player_label([&](std::string_view name, HudSurface& image, const HudFont& bitmap,
                                           const HudRect& row)
            {
                if (!unicode_text(name))
                    return false;
                LocalizedBitmapLine(bitmap, localized_font, name).draw(image, row);
                return true;
            });
        if (localized && !localization_resource_overridden(client.resources(), "cres.dll"))
        {
            client.set_announcer_translations([&](unsigned id)
            {
                const auto value = localization.text(id);
                return localized_font.supports(value) ? value : std::string_view{};
            });
            frag_table.set_team_label([&](DBYTE team, HudSurface& image, const HudFont& bitmap, const HudRect& row)
            {
                const auto value = localization.text(team == 1 ? "network.blue_team" : "network.red_team");
                if (value.empty() || !localized_font.supports(value))
                    return false;
                LocalizedBitmapLine(bitmap, localized_font, value).draw(image, row);
                return true;
            });
        }
        if (!client.connect(address.host, address.port, profile, NetworkClient::Clock::now(), error))
            return failed();
        spdlog::info("Connecting to {}:{} as {}", address.host, address.port, profile.name);
        if (!client.audio().mixer.set_gains(settings.sound ? settings.sound_volume / 50.f : 0,
                                            settings.music ? settings.music_volume / 100.f : 0) ||
            !client.audio().mixer.open_output(error))
            return failed();
        client.audio().mixer.output_lead = AudioMixer::interactive_output_lead;
        ServerClock server_clock;
        auto previous = NetworkClient::Clock::now();
        bool paused = false, leaving = false, captured = false;
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
            menu_input.reset(menu, false);
            return true;
        };
        const auto sync_menu = [&]()
        {
            const bool ready = client.ready();
            if (ready && !paused && !leaving)
            {
                if (menu.active)
                {
                    menu.close();
                    menu_input.reset(menu, true);
                }
                return;
            }
            if (!menu.active)
            {
                menu.open(ready);
                menu.build(MenuPage::MultiplayerPause);
                menu.top = MenuPage::MultiplayerPause;
            }
            menu.in_world = ready;
            std::vector<MenuItem> rows;
            const auto add = [&](int command, std::string_view key, const char* label)
            {
                MenuItem item;
                item.command = command;
                item.literal = label;
                item.localized_literal = translated(key);
                rows.push_back(std::move(item));
            };
            if (ready && !leaving)
                add(resume_option, "network.resume", "Resume");
            if (!leaving)
            {
                if (server)
                    add(leave_option, "network.end_host", "End hosted game");
                else
                    add(leave_option, "network.leave_game", "Leave game");
            }
            if (ready && !leaving)
            {
                MenuItem item;
                item.command = subtitles_option;
                item.setting = menu_subtitles_setting;
                item.literal = "SUBTITLES";
                rows.push_back(std::move(item));
            }
            menu.set_multiplayer_rows(std::move(rows));
            std::string status(leaving ? tr("network.leaving", "Leaving...")
                               : ready ? tr("network.continues", "Game continues while this menu is open.")
                                       : tr("network.connecting", "Connecting / loading world..."));
            if (network && !network->discovery_error.empty())
            {
                status += '\n';
                status += localization.format("network.discovery_failed", "{0}", {network->discovery_error});
            }
            if (menu.multiplayer_status != status)
            {
                menu.multiplayer_status = std::move(status);
                ++menu.revision;
            }
        };
        sync_menu();
        while (client.phase() != NetworkClient::Phase::Closed)
        {
            const auto frame_start = SDL_GetTicksNS();
            int window_width = 0, window_height = 0;
            SDL_GetWindowSize(runtime.window, &window_width, &window_height);
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                ImGui_ImplSDL3_ProcessEvent(&event);
                devices.event(event);
                scores.event(event, window,
                             client.ready() && !client.changing_level() && controls.focused && !menu.active &&
                                 !leaving);
                if (event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == window))
                {
                    quit = true;
                    if (!leave())
                        return failed();
                }
                if (menu.active && !leaving)
                {
                    menu_input.event(event, window, menu, client.ready(), true, false, float(window_width),
                                     float(window_height));
                    const auto command = menu.take_command();
                    if (command.action == MenuAction::Resume ||
                        (command.action == MenuAction::MultiplayerOption && command.option == resume_option))
                    {
                        paused = false;
                        if (menu.active)
                        {
                            menu.close();
                            menu_input.reset(menu, client.ready());
                        }
                    }
                    else if (command.action == MenuAction::MultiplayerOption && command.option == leave_option &&
                             !leave())
                        return failed();
                    else if (command.action == MenuAction::Settings && settings.subtitles != menu.settings.subtitles)
                    {
                        settings.subtitles = menu.settings.subtitles;
                        std::string save_error;
                        if (!save_menu_preferences(preferences, settings, save_error))
                            spdlog::error("Save preferences failed: {}", save_error);
                    }
                    menu.cues.clear();
                }
                else
                {
                    if (!leaving && controls.focused &&
                        ((event.type == SDL_EVENT_KEY_DOWN && event.key.windowID == window && !event.key.repeat &&
                          event.key.scancode == SDL_SCANCODE_ESCAPE) ||
                         (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                          event.gbutton.button == SDL_GAMEPAD_BUTTON_START)))
                    {
                        paused = true;
                        sync_menu();
                        menu_input.reset(menu, false);
                    }
                    controls.enabled = !menu.active && !leaving;
                    controls.event(event, window, menu.active || leaving);
                }
                if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED && event.window.windowID == window)
                {
                    devices.resync(controls);
                    if (!menu.active && !leaving)
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
            const bool play_blocked = menu.active || leaving;
            controls.enabled = !play_blocked;
            const auto input = controls.sample(play_blocked);
            PlayerRotation aim{client.pitch(), client.yaw()};
            controls.look(aim, seconds, play_blocked, client.zoomed());
            if (!client.look(aim.pitch - client.pitch(), aim.yaw - client.yaw()))
                return failed();
            const bool commands_blocked = play_blocked || !client.ready() || client.changing_level();
            actions.sample(controls, commands_blocked, {client.pitch(), client.yaw()}, sampled_actions);
            for (const auto& action : sampled_actions)
                if (!client.command(action, error))
                    spdlog::info("Player command: {}", error);
            // Preserve reliable Control order: selection/use precede this frame's
            // weapon timeline. Existing bindings suppress held fire across menus/focus.
            const auto fire = controls.sample_fire(commands_blocked);
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
            sync_menu();
            if (!controls.sync_mouse(runtime.window, client.ready(), menu.active || leaving, error))
                return failed();
            if (client.phase() == NetworkClient::Phase::Closed)
                break;
            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            const bool visible = client.ready();
            const bool world_visible = visible && !menu.active;
            const auto& camera = client.camera();
            if (world_visible)
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
                const bool letterbox = camera.view.object && camera.view.settings.type == 1;
                draw_letterbox(letterbox);
                if (scores.visible)
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
                    auto* face = localized_face && (localized || unicode_text(line.text))
                                     ? localized_face : ImGui::GetFont();
                    ImGui::GetForegroundDrawList()->AddText(face, 16,
                                                            {origin.x + 1, y},
                                                            IM_COL32(255, 255, 255, 255), line.text.c_str());
                    y += 16;
                }
                if (settings.subtitles)
                {
                    const auto& speech = client.speech();
                    auto caption = speech.subtitle();
                    if (localized && !caption.english.empty() && caption.translated.empty())
                    {
                        const auto& file = speech.communication.message.file;
                        const auto* original = conversation_text(file);
                        if (original && caption.english == original &&
                            !localization_resource_overridden(client.resources(), file))
                            caption.translated = localization.dialogue(file);
                    }
                    ui.draw_subtitle(caption,
                                     letterbox ? CaptionPlacement::Letterbox : CaptionPlacement::Play, localized_font);
                }
            }
            if (menu.active)
            {
                if (native_menu && !menu_canvas.compose(menu, 0, menu_error))
                {
                    spdlog::warn("Multiplayer menu artwork: {}", menu_error);
                    native_menu = false;
                }
                if (native_menu)
                {
                    if (!menu_renderer.draw(menu_canvas, error))
                        return failed();
                }
                else
                {
                    const auto state = settings.subtitles ? tr("menu.on", "ON") : tr("menu.off", "OFF");
                    const auto subtitle_label = std::string(tr("menu.subtitles", "SUBTITLES")) + " [" +
                                                std::string(state) + "]";
                    draw_network_menu_fallback(menu, tr("network.title", "BLOODBATH"),
                                               localized ? localized_face : ImGui::GetFont(), subtitle_label);
                }
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
            if (world_visible)
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
            color.load_op = world_visible ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
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
        std::string effect_kinds;
        for (const auto count : client.effects().received_kinds)
        {
            if (!effect_kinds.empty())
                effect_kinds += '/';
            effect_kinds += std::to_string(count);
        }
        spdlog::info("Instant effects: received={}, failed={}, dropped={}, models={}, billboards={}, cosmetic_sounds={}, "
                     "audio_failures={}, kinds={}, error={}, audio_error={}",
                     client.effects().received, client.effects().failed, client.effects().dropped,
                     client.effects().scene().frame().models.size(),
                     client.effects().scene().frame().billboards.size(), client.audio().effects_started,
                     client.audio().effects_failed, effect_kinds, client.effects().last_error,
                     client.audio().last_effect_error);
        spdlog::info("Player commands: acknowledged={}, applied={}", client.command_acks, client.commands_applied);
        spdlog::info("Weapon input: frames={}, waiting_ticks={}", client.weapon_frames, client.weapon_wait_ticks);
        if (server)
            spdlog::info("Host stopped: ticks={}, discarded_ticks={}, admitted={}, departed={}, rotations={}",
                         server->total_ticks(), server_clock.discarded_ticks(), network->admitted, network->departed,
                         server->rotations);
        error = leaving ? std::string{} : client.reason();
        return 0;
    }
    int run_network_frontend(const Options& options, const Config& config)
    {
        Runtime runtime;
        std::string error;
        if (!runtime.initialize(false, error))
        {
            spdlog::error("{}", error);
            return 5;
        }
        LocalizedFont localized_font;
        UiRuntime ui(runtime);
        if (!ui.initialize(error))
        {
            spdlog::error("{}", error);
            return 5;
        }
        std::string font_error;
        if (!localized_font.load(font_error))
            spdlog::warn("Language font: {}. Using English.", font_error);
        else if (!ui.set_localized_font(localized_font))
            spdlog::warn("Cannot initialize the language font. Using English.");
        bool quit = false;
        return run_network_session(options, config, ui, error, quit, localized_font);
    }
} // namespace OpenBlood2
