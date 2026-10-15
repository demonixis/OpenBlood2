#pragma once
#include "game/player_messages.h"
#include <SDL3/SDL_events.h>
#include <algorithm>
#include <array>

namespace OpenBlood2
{
    // Physical keys: WASD on QWERTY, ZQSD on AZERTY. Input state is cleared on
    // focus loss; only events for this window participate. Also testable headless.
    struct MovementControls
    {
        std::array<bool, SDL_SCANCODE_COUNT> keys{};
        bool focused = false, enabled = false;
        bool consumes(const SDL_Event& event, bool text_editing) const
        {
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
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED && event.window.windowID == window)
                focused = true;
            if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.windowID == window)
            {
                if (!focused && event.type == SDL_EVENT_KEY_DOWN)
                    return;
                const int code = event.key.scancode;
                if (code > SDL_SCANCODE_UNKNOWN && code < SDL_SCANCODE_COUNT)
                    keys[static_cast<unsigned>(code)] = event.type == SDL_EVENT_KEY_DOWN;
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && code == SDL_SCANCODE_F2)
                    enabled = !enabled;
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
        void look(PlayerRotation& rotation, double seconds, bool text_editing) const
        {
            if (!focused || !enabled || text_editing)
                return;
            const auto dt = static_cast<float>(std::clamp(seconds, 0.0, 0.1));
            rotation.yaw += (int(keys[SDL_SCANCODE_RIGHT]) - int(keys[SDL_SCANCODE_LEFT])) * 2 * dt;
            rotation.pitch = std::clamp(
                rotation.pitch + (int(keys[SDL_SCANCODE_DOWN]) - int(keys[SDL_SCANCODE_UP])) * dt, -1.48f, 1.48f);
        }
    };
} // namespace OpenBlood2
