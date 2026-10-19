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
            if (!controls.focused)
                return false;
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat)
            {
                const auto key = e.key.scancode;
                if (finished())
                {
                    quit |= key == SDL_SCANCODE_ESCAPE || key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER ||
                            key == SDL_SCANCODE_SPACE;
                    return false;
                }
                if (key == SDL_SCANCODE_F10)
                {
                    if (inspector_available)
                        inspector = !inspector;
                    reset(controls, inspector, player);
                    return false;
                }
                if (key == SDL_SCANCODE_ESCAPE && !inspector)
                {
                    state = paused() ? State::Playing : State::Paused;
                    reset(controls, inspector, player);
                    return false;
                }
                if (!inspector && player && !alive && (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER))
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
                controls.event(e, window);
            }
            return false;
        }
    };
} // namespace OpenBlood2
