#pragma once
#include "app/menu_input.h"
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
            std::vector<double> result{game.objects.time(), double(game.server.audio.instruments.sample_clock()),
                                       double(game.server.audio.mixed_frames)};
            for (const auto id : game.objects.snapshot())
            {
                const auto* r = game.objects.get(id);
                result.insert(result.end(),
                              {r->state.m_Pos.x, r->state.m_Pos.y, r->state.m_Pos.z, r->state.m_Rotation.m_Vec.x,
                               r->state.m_Rotation.m_Vec.y, r->state.m_Rotation.m_Vec.z, r->state.m_Rotation.m_Spin,
                               r->velocity.x, r->velocity.y, r->velocity.z, r->animation.time,
                               double(r->animation.revision), r->due, double(r->updates)});
            }
            for (const auto& [id, voice] : game.server.audio.voices())
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
            if (e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_WHEEL || e.type == SDL_EVENT_TEXT_INPUT)
                return true;
            if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED || e.type == SDL_EVENT_WINDOW_FOCUS_LOST)
                return e.window.data1 != int(device);
            return false;
        }
        bool confirmed = false;
        bool before(std::uint64_t frame, SDL_Window* window, const GameSession& game, const Menu& menu,
                    MenuInput& input, const MenuAudio& audio, std::string& error)
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
            if (frame == 4 && (!game.client_shell.m_bInWorld || menu.active || game.world_revision != 1))
                return fail("new game request");
            if (frame == 11)
            {
                if (!menu.active || !game.server.audio.paused())
                    return fail("pause admission");
                frozen_ = snapshot(game);
                ambient_frames_ = audio.mixer.mixed_frames;
            }
            if (frame > 11 && frame <= 207 && snapshot(game) != frozen_)
                return fail("world/audio changed during menu pause");
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
            if (frame == 215 && menu.page != MenuPage::Quit)
                return fail("quit confirmation");
            if (frame > 216)
                return fail("quit not executed");
            auto key = [&](SDL_Scancode code, bool repeat = false)
            {
                SDL_Event e{};
                e.type = SDL_EVENT_KEY_DOWN;
                e.key.windowID = SDL_GetWindowID(window);
                e.key.which = device;
                e.key.scancode = code;
                e.key.repeat = repeat;
                SDL_PushEvent(&e);
                e.type = SDL_EVENT_KEY_UP;
                e.key.repeat = false;
                SDL_PushEvent(&e);
            };
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
                case 10:
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 12:
                case 13:
                    key(SDL_SCANCODE_F10);
                    break;
                case 14:
                    key(SDL_SCANCODE_DOWN);
                    break;
                case 15:
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 16:
                    key(SDL_SCANCODE_DOWN); // Options opens on Display; Audio is next.
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
                    key(SDL_SCANCODE_DOWN); // Display -> Audio -> (disabled Controls) -> Mouse.
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
                    key(SDL_SCANCODE_ESCAPE);
                    break;
                case 213: // Options, the enabled in-world Credits row, Quit.
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    key(SDL_SCANCODE_DOWN);
                    break;
                case 214:
                    key(SDL_SCANCODE_RETURN);
                    break;
                case 215:
                    key(SDL_SCANCODE_UP);
                    break;
                case 216:
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
