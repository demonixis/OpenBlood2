#pragma once
#include "app/movement_controls.h"

namespace OpenBlood2
{
    // Temporary menu-free demo presentation. No resource/string/menu dependency.
    struct DemoFlow
    {
        enum class State
        {
            Playing,
            Paused,
            Finished
        };
        State state = State::Playing;
        bool quit = false;
        bool restart = false;
        bool paused() const
        {
            return state != State::Playing;
        }
        bool finished() const
        {
            return state == State::Finished;
        }
        void reset(MovementControls& controls, bool inspector, bool player) const
        {
            controls.player_changed(player && !paused() && !inspector);
        }
        bool event(const SDL_Event& e, SDL_WindowID window, MovementControls& controls, bool& inspector,
                   bool inspector_available, bool player, bool alive = true)
        {
            if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && e.key.windowID != window)
                return false;
            if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST && e.window.windowID == window)
            {
                controls.event(e, window);
                if (!finished())
                    state = State::Paused;
                reset(controls, inspector, player);
                return inspector;
            }
            if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED && e.window.windowID == window)
            {
                controls.event(e, window);
                return inspector;
            }
            const bool gamepad = MovementControls::gamepad_event(e);
            if (e.type == SDL_EVENT_GAMEPAD_REMOVED || !controls.focused)
            {
                if (gamepad)
                    controls.track(e);
                return false;
            }
            const auto edges = controls.track(e);
            const auto press = [&](InputAction action)
            {
                for (const auto& edge : edges)
                    if (edge.down && !edge.repeat && controls.bindings.matches(edge, action))
                        return true;
                return false;
            };
            const bool key_press = e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat;
            if (key_press || gamepad)
            {
                if (finished())
                {
                    quit |= (key_press && e.key.scancode == SDL_SCANCODE_SPACE) || press(InputAction::Menu) ||
                            press(InputAction::MenuAccept) || press(InputAction::MenuBack);
                    return false;
                }
                if (key_press && e.key.scancode == SDL_SCANCODE_F10)
                {
                    if (inspector_available)
                        inspector = !inspector;
                    reset(controls, inspector, player);
                    return false;
                }
                if (press(InputAction::Menu) && !inspector)
                {
                    state = paused() ? State::Playing : State::Paused;
                    reset(controls, inspector, player);
                    return false;
                }
                if (!inspector && player && !alive && press(InputAction::MenuAccept))
                {
                    restart = true; // App-level fresh entry, not a source save/respawn operation.
                    return false;
                }
            }
            if (finished())
                return false;
            if (inspector)
                return true;
            if (!paused())
            {
                // Keep diagnostic ownership shortcuts out of normal play.
                if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) &&
                    (e.key.scancode == SDL_SCANCODE_F1 || e.key.scancode == SDL_SCANCODE_F2 ||
                     e.key.scancode == SDL_SCANCODE_F3))
                    return false;
                if (gamepad)
                    controls.command_edges(edges, false);
                else
                    controls.event(e, window);
            }
            return false;
        }
    };
} // namespace OpenBlood2
