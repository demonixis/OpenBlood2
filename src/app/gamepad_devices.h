#pragma once
#include "app/movement_controls.h"
#include <SDL3/SDL_gamepad.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace OpenBlood2
{
    // Opens gamepads as SDL reports them (including those present at startup) and closes
    // them on removal and destruction. MovementControls only sees their events.
    class GamepadDevices
    {
      public:
        GamepadDevices() = default;
        GamepadDevices(const GamepadDevices&) = delete;
        GamepadDevices& operator=(const GamepadDevices&) = delete;
        ~GamepadDevices()
        {
            for (auto* pad : open_)
                SDL_CloseGamepad(pad);
        }
        void event(const SDL_Event& e)
        {
            if (e.type == SDL_EVENT_GAMEPAD_ADDED && !SDL_GetGamepadFromID(e.gdevice.which))
            {
                if (open_.size() >= max_gamepads)
                {
                    spdlog::warn("Gamepad {} ignored: {} are already open", e.gdevice.which, max_gamepads);
                    return;
                }
                if (auto* pad = SDL_OpenGamepad(e.gdevice.which))
                {
                    open_.push_back(pad);
                    spdlog::info("Gamepad connected: {}", SDL_GetGamepadName(pad) ? SDL_GetGamepadName(pad) : "?");
                }
                else
                    spdlog::warn("Gamepad {} could not be opened: {}", e.gdevice.which, SDL_GetError());
            }
            if (e.type == SDL_EVENT_GAMEPAD_REMOVED)
                std::erase_if(open_,
                              [&e](SDL_Gamepad* pad)
                              {
                                  if (SDL_GetGamepadID(pad) != e.gdevice.which)
                                      return false;
                                  SDL_CloseGamepad(pad);
                                  spdlog::info("Gamepad disconnected: {}", e.gdevice.which);
                                  return true;
                              });
        }
        // After focus returns, background events may have been dropped: poll real state.
        void resync(MovementControls& controls) const
        {
            if (!controls.focused)
                return;
            for (auto* pad : open_)
            {
                GamepadState state{SDL_GetGamepadID(pad), {}, {}};
                for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
                    state.buttons[unsigned(i)] = SDL_GetGamepadButton(pad, SDL_GamepadButton(i));
                for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i)
                    state.axes[unsigned(i)] =
                        std::max(float(SDL_GetGamepadAxis(pad, SDL_GamepadAxis(i))) / 32767.f, -1.f);
                controls.set_gamepad(state);
            }
        }
        std::size_t count() const
        {
            return open_.size();
        }

      private:
        std::vector<SDL_Gamepad*> open_;
    };
} // namespace OpenBlood2
