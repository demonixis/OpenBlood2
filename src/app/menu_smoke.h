#pragma once
#include "app/menu_input.h"
#include "app/objectives_smoke.h"
#include "game/menu_audio.h"
#include "game/server_adapter.h"
#include <SDL3/SDL.h>
#include <vector>
namespace OpenBlood2
{
    // Explicit SDL-event diagnostic; never runs in normal play or old smokes.
    class MenuSmoke
    {
        std::vector<double> frozen_;
        double live_time_ = 0;
        std::uint64_t ambient_frames_ = 0;
        static std::vector<double> snapshot(const GameSession& game)
        {
            std::vector<double> result{game.objects.time(),
                                       double(game.client_session.audio.mixer.instruments.sample_clock()),
                                       double(game.client_session.audio.mixer.mixed_frames)};
            for (const auto id : game.objects.snapshot())
            {
                const auto* r = game.objects.get(id);
                result.insert(result.end(),
                              {r->state.m_Pos.x, r->state.m_Pos.y, r->state.m_Pos.z, r->state.m_Rotation.m_Vec.x,
                               r->state.m_Rotation.m_Vec.y, r->state.m_Rotation.m_Vec.z, r->state.m_Rotation.m_Spin,
                               r->velocity.x, r->velocity.y, r->velocity.z, r->animation.time,
                               double(r->animation.revision), r->due, double(r->updates)});
            }
            for (const auto& [id, voice] : game.client_session.audio.mixer.voices())
                result.insert(result.end(), {double(id), voice.cursor, double(voice.loops)});
            for (const auto& weapon : game.client_shell.weapons)
                result.push_back(weapon.animation.time);
            return result;
        }

      public:
        static constexpr Uint32 device = 0xB200;
        // Keep compositor/physical device noise from changing the scripted traversal.
        static bool unrelated(const SDL_Event& e)
        {
            if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP)
                return e.key.which != device;
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP)
                return e.button.which != device;
            if (e.type == SDL_EVENT_TEXT_INPUT)
                return e.text.timestamp != device;
            if (e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_WHEEL)
                return true;
            if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED || e.type == SDL_EVENT_WINDOW_FOCUS_LOST)
                return e.window.data1 != int(device);
            return false;
        }
        bool confirmed = false;
        static bool trigger_caption(GameSession& game, std::string_view name, std::string& error)
        {
            for (auto id : game.objects.snapshot())
            {
                const auto* entry = game.objects.get(id);
                if (!same_name(entry->state.m_Name, name))
                    continue;
                auto text = game.server.CreateString("TRIGGER");
                auto message = game.server.StartMessageToObject(nullptr, entry->handle, 1000);
                const bool sent = message && game.server.WriteToMessageHString(message, text) == DE_OK &&
                                  game.server.EndMessage(message) == DE_OK;
                game.server.FreeString(text);
                if (sent)
                    return true;
                break;
            }
            error = "Cannot trigger the authored dialogue for menu smoke: " + std::string(name);
            return false;
        }
        bool before(std::uint64_t frame, SDL_Window* window, GameSession& game, const Menu& menu, MenuInput& input,
                    const MenuAudio& audio, std::string& error)
        {
            auto fail = [&](const char* why)
            {
                error = "Menu smoke frame " + std::to_string(frame) + ": " + why;
                return false;
            };
            if (!menu.error.empty())
                return fail(menu.error.c_str());
            if (frame == 0)
            {
                if (!menu.active || game.client_shell.m_bInWorld || input.inspector)
                    return fail("initial ownership");
                SDL_Event event{};
                event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
                event.window.windowID = SDL_GetWindowID(window);
                event.window.data1 = int(device);
                SDL_PushEvent(&event);
            }
            // The loading screen is presented before the load, then waits for a key (SubwayA's
            // load screen ID pauses into GS_WAITING).
            if (frame == 4 && (!input.loading_screen || input.loading_wait || game.client_shell.m_bInWorld))
                return fail("loading screen before the new game");
            if (frame == 5 && (!game.client_shell.m_bInWorld || menu.active || game.world_revision != 1 ||
                               !input.loading_wait || !game.client_session.audio.mixer.paused()))
                return fail("new game request and waiting screen");
            if (frame == 6 && input.loading_screen)
                return fail("waiting screen continue");
            if (frame == 11)
            {
                if (!menu.active || !game.client_session.audio.mixer.paused())
                    return fail("pause admission");
                frozen_ = snapshot(game);
                ambient_frames_ = audio.mixer.mixed_frames;
            }
            if (frame > 11 && frame <= 207 && snapshot(game) != frozen_)
                return fail("world/audio changed during menu pause");
            if (frame == 16 && (menu.page != MenuPage::Options || menu.settings.language != Language::Korean ||
                                menu.items[menu.selected].setting != menu_language_setting))
                return fail("Options language selection");
            // Options > SETUP opens the setup screen over the paused menu; Esc returns to Options.
            if (frame == 30 && (menu.page != MenuPage::Main || input.setup_screen))
                return fail("main menu before setup");
            if (frame == 32 && !input.setup_screen)
                return fail("Options > SETUP");
            if (frame == 34 && (input.setup_screen || menu.page != MenuPage::Options))
                return fail("setup screen Back");
            if (menu.multiplayer)
            {
                if (frame == 39 && (menu.page != MenuPage::Bloodbath || menu.items.size() != 3))
                    return fail("native Bloodbath hub");
                if (frame == 40 && menu.page != MenuPage::MultiplayerJoin)
                    return fail("native Join page");
                if (frame == 42 && (!menu.multiplayer_edit || !SDL_TextInputActive(window) ||
                                    !menu.multiplayer_edit->starts_with("example.test:28900")))
                    return fail("address editor and SDL text input");
                if (frame == 43 && (menu.page != MenuPage::MultiplayerJoin || SDL_TextInputActive(window) ||
                                    menu.items.front().literal.find("127.0.0.1:27888") == std::string::npos))
                    return fail("gamepad Back cancels editing without leaving Join");
                if (frame == 46 && (SDL_TextInputActive(window) ||
                                    menu.items.front().literal.find("example.test:28900") == std::string::npos))
                    return fail("Enter applies the edited address");
                if (frame == 47 && menu.page != MenuPage::MultiplayerLan)
                    return fail("native LAN browser");
                if (frame == 52 && (menu.page != MenuPage::MultiplayerHost || menu.items.size() != 13))
                    return fail("CTF host rules");
                if (frame == 53 && menu.page != MenuPage::MultiplayerRotation)
                    return fail("native map rotation");
                if (frame == 54 && menu.items.size() != 5)
                    return fail("adding a map to rotation");
                if (frame == 57 && menu.page != MenuPage::Main)
                    return fail("Bloodbath Back returns to the paused solo menu");
            }
            if (frame == 207)
            {
                if (audio.mixer.mixed_frames <= ambient_frames_ + 48000 * 3)
                    return fail("menu audio did not continue");
                if (menu.settings.sound_volume != 45 || menu.settings.sensitivity != 170 || !menu.settings.invert_y)
                    return fail("option routing");
                live_time_ = game.objects.time();
            }
            if (frame == 208 && game.objects.time() != live_time_)
                return fail("resume frame caught up paused time");
            if (frame >= 209 && frame <= 212)
            {
                if (game.objects.time() <= live_time_ || game.objects.time() > live_time_ + .02)
                    return fail("resume/inspector clock");
                live_time_ = game.objects.time();
            }
            if (frame == 213 && (!input.message.active || !SDL_TextInputActive(window) || menu.active))
                return fail("message editor ownership");
            if (frame == 214 && input.message.text != "Control test")
                return fail("SDL message text");
            if (frame == 215 && (input.message.active || SDL_TextInputActive(window) ||
                                 game.client_shell.m_MessageMgr.lines().empty() ||
                                 game.client_shell.m_MessageMgr.lines().back() != ":Control test"))
                return fail("local message delivery and text input release");
            if (frame == 217 &&
                (!menu.settings.run_lock || menu.settings.crosshair || game.client_shell.m_bShowCrosshair))
                return fail("control commands through the frontend");
            if (frame == 235 && (!game.client_shell.third_person || game.client_shell.control_notice != "3인칭"))
                return fail("Korean camera notice");
            if (frame == 236 && (game.client_shell.third_person || game.client_shell.control_notice != "1인칭"))
                return fail("Korean first-person notice");
            if (frame == 239 && menu.page != MenuPage::Quit)
                return fail("quit confirmation");
            if (frame > 240)
                return fail("quit not executed");
            auto key = [&](SDL_Scancode code, bool repeat = false)
            {
                SDL_Event e{};
                e.type = SDL_EVENT_KEY_DOWN;
                e.key.windowID = SDL_GetWindowID(window);
                e.key.which = device;
                e.key.scancode = code;
                e.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false); // Dear ImGui reads keycodes.
                e.key.repeat = repeat;
                SDL_PushEvent(&e);
                e.type = SDL_EVENT_KEY_UP;
                e.key.repeat = false;
                SDL_PushEvent(&e);
            };
            const auto text_input = [&](const char* value)
            {
                SDL_Event e{};
                e.type = SDL_EVENT_TEXT_INPUT;
                e.text.timestamp = device;
                e.text.windowID = SDL_GetWindowID(window);
                e.text.text = value;
                SDL_PushEvent(&e);
            };
            if (menu.multiplayer)
            {
                switch (frame)
                {
                    case 39:
                    case 40:
                    case 43:
                    case 45:
                        key(SDL_SCANCODE_RETURN);
                        break;
                    case 41:
                    case 44:
                        text_input("example.test:28900");
                        break;
                    case 42:
                    {
                        SDL_Event e{};
                        e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
                        e.gbutton.which = device;
                        e.gbutton.button = SDL_GAMEPAD_BUTTON_EAST;
                        SDL_PushEvent(&e);
                        e.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
                        SDL_PushEvent(&e);
                        break;
                    }
                    case 46:
                        key(SDL_SCANCODE_DOWN);
                        key(SDL_SCANCODE_DOWN);
                        key(SDL_SCANCODE_RETURN);
                        break;
                    case 47:
                    case 48:
                    case 54:
                    case 55:
                    case 56:
                        key(SDL_SCANCODE_ESCAPE);
                        break;
                    case 49:
                    case 53:
                        key(SDL_SCANCODE_DOWN);
                        key(SDL_SCANCODE_RETURN);
                        break;
                    case 50:
                    case 51:
                        key(SDL_SCANCODE_RIGHT);
                        break;
                    case 52:
                        key(SDL_SCANCODE_UP);
                        key(SDL_SCANCODE_UP);
                        key(SDL_SCANCODE_UP);
                        key(SDL_SCANCODE_RETURN);
                        break;
                    default:
                        break;
                }
            }
            switch (frame)
            {
                case 1:
                {
                    int w = 0, h = 0;
                    SDL_GetWindowSize(window, &w, &h);
                    const auto fit = MenuViewport::fit(float(w), float(h));
                    const auto r = menu.items[0].rect;
                    SDL_Event e{};
                    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                    e.button.windowID = SDL_GetWindowID(window);
                    e.button.which = device;
                    e.button.button = SDL_BUTTON_LEFT;
                    e.button.x = fit.x + (r.left + 2) * fit.scale;
                    e.button.y = fit.y + (r.top + 2) * fit.scale;
                    SDL_PushEvent(&e);
                    e.type = SDL_EVENT_MOUSE_BUTTON_UP;
                    SDL_PushEvent(&e);
                    break;
                }
                case 2:
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 3:
                    key(SDL_SCANCODE_RETURN);
                    key(SDL_SCANCODE_RETURN, true);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 5:
                    key(SDL_SCANCODE_SPACE);
                    break;
                case 10:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 12:
                case 13:
                    key(SDL_SCANCODE_F10);
                    break;
                case 14:
                    if (menu.multiplayer)
                        key(SDL_SCANCODE_DOWN); // Bloodbath is now available.
                    key(SDL_SCANCODE_DOWN);
                    break;
                case 15:
                    key(SDL_SCANCODE_RETURN);
                    key(SDL_SCANCODE_UP); // CHEATS, then LANGUAGE.
                    key(SDL_SCANCODE_UP);
                    key(SDL_SCANCODE_LEFT); // English wraps back to Korean.
                    break;
                case 16:
                    key(SDL_SCANCODE_DOWN); // LANGUAGE -> CHEATS -> Display -> Audio.
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 17:
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_LEFT);
                    break;
                case 18:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 19:
                    key(SDL_SCANCODE_DOWN); // Display -> Audio -> Controls -> Mouse.
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    break;
                case 20:
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 21:
                    key(SDL_SCANCODE_RIGHT);
                    break;
                case 22:
                    key(SDL_SCANCODE_DOWN);
                    break;
                case 23:
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 24:
                case 25:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 30:
                    if (menu.multiplayer)
                        key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 31: // Display, Audio, Controls, Mouse, (disabled Joystick, Keyboard), SETUP.
                    for (int i = 0; i < 4; ++i)
                        key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 32:
                case 35:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 36:
                    if (menu.multiplayer)
                        key(SDL_SCANCODE_DOWN);
                    break;
                case 37:
                    if (menu.multiplayer)
                        key(SDL_SCANCODE_RETURN);
                    break;
                case 120:
                    SDL_SetWindowSize(window, 900, 500);
                    break;
                case 160:
                    SDL_SetWindowSize(window, 1200, 800);
                    break;
                case 207:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 209:
                case 211:
                    key(SDL_SCANCODE_F10);
                    break;
                case 212:
                    if (!assign_control(input.controls.bindings, InputAction::Message, InputSource::Key,
                                        SDL_SCANCODE_T) ||
                        !assign_control(input.controls.bindings, InputAction::RunLock, InputSource::Key,
                                        SDL_SCANCODE_R) ||
                        !assign_control(input.controls.bindings, InputAction::Crosshair, InputSource::Key,
                                        SDL_SCANCODE_H))
                        return fail("control command bindings");
                    key(SDL_SCANCODE_T);
                    break;
                case 213:
                    text_input("Control test");
                    break;
                case 214:
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 216:
                    key(SDL_SCANCODE_R);
                    key(SDL_SCANCODE_H);
                    break;
                case 221:
                    if (!show_authored_objective_for_smoke(game, 2, error))
                        return false;
                    key(SDL_SCANCODE_TAB);
                    break;
                case 231:
                    key(SDL_SCANCODE_TAB);
                    break;
                case 234:
                case 235:
                    key(SDL_SCANCODE_C);
                    break;
                case 236:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 237: // Bloodbath, Options, the enabled in-world Credits row, Quit.
                    if (menu.multiplayer)
                        key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    break;
                case 238:
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 239:
                    key(SDL_SCANCODE_UP);
                    break;
                case 240:
                    key(SDL_SCANCODE_RETURN);
                    confirmed = true;
                    break;
                default:
                    break;
            }
            return true;
        }
    };
} // namespace OpenBlood2
