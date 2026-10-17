#pragma once
#include "game/player_messages.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace OpenBlood2
{
    // Physical keys: WASD on QWERTY, ZQSD on AZERTY. Input state is cleared on
    // focus loss; only events for this window participate. Also testable headless.
    struct MovementControls
    {
        std::array<bool, SDL_SCANCODE_COUNT> keys{};
        bool focused = false, enabled = true, use_suppressed = false;
        bool medkit_suppressed = false;
        bool mouse_requested = false, mouse_captured = false;
        std::array<bool, 2> mouse_buttons{}, fire_suppressed{};
        float mouse_x = 0, mouse_y = 0;
        struct FireButtons
        {
            bool primary = false, alternate = false;
        };
        void release_mouse()
        {
            mouse_requested = false;
            mouse_buttons = {};
            mouse_x = mouse_y = 0;
            fire_suppressed = {keys[SDL_SCANCODE_F], keys[SDL_SCANCODE_G]};
        }
        void player_changed()
        {
            keys = {};
            use_suppressed = medkit_suppressed = false;
            release_mouse();
        }
        bool sync_mouse(SDL_Window* window, bool player_ready, bool text_editing, std::string& error)
        {
            if (!focused || !enabled || !player_ready || text_editing)
                release_mouse();
            if (mouse_requested == mouse_captured)
                return true;
            if (!SDL_SetWindowRelativeMouseMode(window, mouse_requested))
            {
                error = SDL_GetError();
                release_mouse();
                return false;
            }
            mouse_captured = mouse_requested;
            mouse_buttons = {};
            mouse_x = mouse_y = 0;
            error.clear();
            return true;
        }
        bool consumes(const SDL_Event& event, bool text_editing) const
        {
            if (mouse_captured && (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                                   event.type == SDL_EVENT_MOUSE_BUTTON_UP))
                return true;
            if (!focused || !enabled || text_editing ||
                (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP))
                return false;
            switch (event.key.scancode)
            {
                case SDL_SCANCODE_W:
                case SDL_SCANCODE_A:
                case SDL_SCANCODE_S:
                case SDL_SCANCODE_D:
                case SDL_SCANCODE_LSHIFT:
                case SDL_SCANCODE_RSHIFT:
                case SDL_SCANCODE_LCTRL:
                case SDL_SCANCODE_RCTRL:
                case SDL_SCANCODE_F:
                case SDL_SCANCODE_G:
                case SDL_SCANCODE_H:
                case SDL_SCANCODE_E:
                case SDL_SCANCODE_SPACE:
                case SDL_SCANCODE_LEFT:
                case SDL_SCANCODE_RIGHT:
                case SDL_SCANCODE_UP:
                case SDL_SCANCODE_DOWN:
                    return true;
                default:
                    return false;
            }
        }
        void event(const SDL_Event& event, SDL_WindowID window)
        {
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST && event.window.windowID == window)
            {
                focused = false;
                keys = {};
                release_mouse();
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED && event.window.windowID == window)
                focused = true;
            if (focused && enabled && mouse_captured)
            {
                if (event.type == SDL_EVENT_MOUSE_MOTION && event.motion.windowID == window &&
                    std::isfinite(event.motion.xrel) && std::isfinite(event.motion.yrel))
                {
                    mouse_x = std::clamp(mouse_x + event.motion.xrel, -10000.f, 10000.f);
                    mouse_y = std::clamp(mouse_y + event.motion.yrel, -10000.f, 10000.f);
                }
                if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
                    event.button.windowID == window)
                {
                    if (event.button.button == SDL_BUTTON_LEFT)
                        mouse_buttons[0] = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                    if (event.button.button == SDL_BUTTON_RIGHT)
                        mouse_buttons[1] = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                }
            }
            if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.windowID == window)
            {
                if (!focused && event.type == SDL_EVENT_KEY_DOWN)
                    return;
                const int code = event.key.scancode;
                if ((code == SDL_SCANCODE_H || code == SDL_SCANCODE_E || code == SDL_SCANCODE_F ||
                     code == SDL_SCANCODE_G) &&
                    event.type == SDL_EVENT_KEY_DOWN && event.key.repeat && !keys[static_cast<unsigned>(code)])
                    return; // A held key after focus loss is not a fresh USE press.
                if (code > SDL_SCANCODE_UNKNOWN && code < SDL_SCANCODE_COUNT)
                    keys[static_cast<unsigned>(code)] = event.type == SDL_EVENT_KEY_DOWN;
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
                {
                    if (code == SDL_SCANCODE_F2)
                        enabled = !enabled;
                    if (code == SDL_SCANCODE_F3)
                    {
                        if (mouse_requested)
                            release_mouse();
                        else
                            mouse_requested = true;
                    }
                    if (code == SDL_SCANCODE_ESCAPE || (code == SDL_SCANCODE_F1 && (mouse_requested || mouse_captured)))
                        release_mouse();
                }
            }
        }
        MovementInput sample(bool text_editing) const
        {
            if (!focused || !enabled || text_editing)
                return {};
            return {keys[SDL_SCANCODE_W],
                    keys[SDL_SCANCODE_S],
                    keys[SDL_SCANCODE_A],
                    keys[SDL_SCANCODE_D],
                    keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT],
                    keys[SDL_SCANCODE_SPACE],
                    keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]};
        }
        bool sample_medkit(bool text_editing)
        {
            if (!keys[SDL_SCANCODE_H])
                medkit_suppressed = false;
            if (!focused || !enabled || text_editing)
            {
                medkit_suppressed = keys[SDL_SCANCODE_H];
                return false;
            }
            return keys[SDL_SCANCODE_H] && !medkit_suppressed;
        }
        bool sample_use(bool text_editing)
        {
            if (!keys[SDL_SCANCODE_E])
                use_suppressed = false;
            if (!focused || !enabled || text_editing)
            {
                use_suppressed = keys[SDL_SCANCODE_E];
                return false;
            }
            return keys[SDL_SCANCODE_E] && !use_suppressed;
        }
        FireButtons sample_fire(bool text_editing)
        {
            const std::array<bool, 2> held{keys[SDL_SCANCODE_F] || (mouse_captured && mouse_buttons[0]),
                                           keys[SDL_SCANCODE_G] || (mouse_captured && mouse_buttons[1])};
            std::array<bool, 2> result{};
            for (unsigned i = 0; i < 2; ++i)
            {
                if (!held[i])
                    fire_suppressed[i] = false;
                if (!focused || !enabled || text_editing)
                    fire_suppressed[i] = held[i];
                else
                    result[i] = held[i] && !fire_suppressed[i];
            }
            return {result[0], result[1]};
        }
        void look(PlayerRotation& rotation, double seconds, bool text_editing)
        {
            const float dx = std::exchange(mouse_x, 0), dy = std::exchange(mouse_y, 0);
            if (!focused || !enabled || text_editing)
                return;
            const auto dt = static_cast<float>(std::clamp(seconds, 0.0, 0.1));
            // Device mapping policy: radians per relative mouse unit, independent of frame duration.
            if (mouse_captured)
            {
                rotation.yaw = std::remainder(rotation.yaw + dx * .0025f, 6.28318530718f);
                rotation.pitch += dy * .0025f;
            }
            rotation.yaw += (int(keys[SDL_SCANCODE_RIGHT]) - int(keys[SDL_SCANCODE_LEFT])) * 2 * dt;
            rotation.pitch = std::clamp(
                rotation.pitch + (int(keys[SDL_SCANCODE_DOWN]) - int(keys[SDL_SCANCODE_UP])) * dt, -1.48f, 1.48f);
        }
    };
} // namespace OpenBlood2
