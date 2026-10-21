#pragma once
#include "app/input_actions.h"
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
    // Device state behind InputBindings (default: the keyboard/mouse layout listed there).
    // Input state is cleared on focus loss; only events for this window participate.
    // Also testable headless.
    struct MovementControls
    {
        InputBindings bindings = InputBindings::keyboard_mouse();
        std::array<bool, SDL_SCANCODE_COUNT> keys{};
        int pending_weapon_slot = -1;
        // Default number-key layout: 1..9 then 0, top row or keypad.
        static int weapon_slot(SDL_Scancode code)
        {
            if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_0)
                return code - SDL_SCANCODE_1;
            if (code >= SDL_SCANCODE_KP_1 && code <= SDL_SCANCODE_KP_0)
                return code - SDL_SCANCODE_KP_1;
            return -1;
        }
        int sample_weapon_slot(bool text_editing)
        {
            const auto slot = std::exchange(pending_weapon_slot, -1);
            return focused && enabled && !text_editing ? slot : -1;
        }
        bool focused = false, enabled = true;
        // Press actions held since play last refused input (focus, inspector, text editing).
        std::array<bool, input_action_count> suppressed{};
        bool mouse_requested = false, mouse_captured = false;
        std::array<bool, SDL_BUTTON_X2> mouse_buttons{}; // Indexed by SDL button - 1.
        float mouse_x = 0, mouse_y = 0;
        float mouse_sensitivity = .0025f;
        bool invert_y = false;
        struct FireButtons
        {
            bool primary = false, alternate = false;
        };
        // OR of every bound source; mouse buttons count only while captured.
        bool held(InputAction action) const
        {
            for (const auto& b : bindings.entries)
                if (b.action == action &&
                    (b.source == InputSource::Key ? keys[unsigned(b.code)]
                                                  : mouse_captured && mouse_buttons[unsigned(b.code - 1)]))
                    return true;
            return false;
        }
        void release_mouse()
        {
            mouse_requested = false;
            mouse_buttons = {};
            mouse_x = mouse_y = 0;
            // A fire key held across the release may not start firing on its own.
            for (const auto action : {InputAction::FirePrimary, InputAction::FireAlternate})
                suppressed[std::size_t(action)] = held(action);
        }
        void request_play()
        {
            enabled = true;
            mouse_requested = true;
        }
        void player_changed(bool play = false)
        {
            pending_weapon_slot = -1;
            keys = {};
            suppressed = {};
            release_mouse();
            if (play)
                request_play();
        }
        bool sync_mouse(SDL_Window* window, bool player_ready, bool text_editing, std::string& error)
        {
            if (!enabled || !player_ready || text_editing || (!focused && mouse_captured))
                release_mouse();
            // The window may not have received its first focus event yet. Keep
            // an explicit play request pending; a real focus-loss event cancels it.
            if (!focused && mouse_requested)
                return true;
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
            return bindings.bound_if(InputSource::Key, event.key.scancode, [](InputAction action)
                                     { return info(action).kind != InputActionKind::Interface; });
        }
        void event(const SDL_Event& event, SDL_WindowID window, bool text_editing = false)
        {
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST && event.window.windowID == window)
            {
                focused = false;
                pending_weapon_slot = -1;
                keys = {};
                suppressed = {};
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
                    event.button.windowID == window && event.button.button >= SDL_BUTTON_LEFT &&
                    event.button.button <= SDL_BUTTON_X2)
                    mouse_buttons[event.button.button - 1u] = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
            }
            if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.windowID == window)
            {
                if (!focused && event.type == SDL_EVENT_KEY_DOWN)
                    return;
                const int code = event.key.scancode;
                if (code <= SDL_SCANCODE_UNKNOWN || code >= SDL_SCANCODE_COUNT)
                    return;
                // A held key after focus loss is not a fresh USE/fire/item press.
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.repeat && !keys[unsigned(code)] &&
                    bindings.bound_if(InputSource::Key, code,
                                      [](InputAction action) { return info(action).kind == InputActionKind::Press; }))
                    return;
                keys[unsigned(code)] = event.type == SDL_EVENT_KEY_DOWN;
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
                {
                    if (enabled && !text_editing)
                        for (const auto& b : bindings.entries)
                            if (b.source == InputSource::Key && b.code == code && weapon_slot_index(b.action) >= 0)
                            {
                                pending_weapon_slot = weapon_slot_index(b.action);
                                break;
                            }
                    if (code == SDL_SCANCODE_F2)
                        enabled = !enabled;
                    if (code == SDL_SCANCODE_F3)
                    {
                        if (mouse_requested)
                            release_mouse();
                        else
                            request_play();
                    }
                    if (bindings.bound(InputSource::Key, code, InputAction::Menu) ||
                        (code == SDL_SCANCODE_F1 && (mouse_requested || mouse_captured)))
                        release_mouse();
                }
            }
        }
        MovementInput sample(bool text_editing) const
        {
            if (!focused || !enabled || text_editing)
                return {};
            return {held(InputAction::MoveForward), held(InputAction::MoveBackward), held(InputAction::StrafeLeft),
                    held(InputAction::StrafeRight), held(InputAction::Run),          held(InputAction::Jump),
                    held(InputAction::Crouch)};
        }
        // Press actions: held state, refused (and re-armed only by a release) while play refuses input.
        bool sample_press(InputAction action, bool text_editing)
        {
            const bool down = held(action);
            auto& refused = suppressed[std::size_t(action)];
            if (!down)
                refused = false;
            if (!focused || !enabled || text_editing)
            {
                refused = down;
                return false;
            }
            return down && !refused;
        }
        bool sample_remote(bool text_editing)
        {
            return sample_press(InputAction::RemoteBomb, text_editing);
        }
        bool sample_detonate(bool text_editing)
        {
            return sample_press(InputAction::Detonate, text_editing);
        }
        bool sample_proximity(bool text_editing)
        {
            return sample_press(InputAction::ProximityBomb, text_editing);
        }
        bool sample_time_bomb(bool text_editing)
        {
            return sample_press(InputAction::TimeBomb, text_editing);
        }
        bool sample_flashlight(bool text_editing)
        {
            return sample_press(InputAction::Flashlight, text_editing);
        }
        bool sample_goggles(bool text_editing)
        {
            return sample_press(InputAction::NightGoggles, text_editing);
        }
        bool sample_eye(bool text_editing)
        {
            return sample_press(InputAction::TheEye, text_editing);
        }
        bool sample_binoculars(bool text_editing)
        {
            return sample_press(InputAction::Binoculars, text_editing);
        }
        bool sample_medkit(bool text_editing)
        {
            return sample_press(InputAction::Medkit, text_editing);
        }
        bool sample_use(bool text_editing)
        {
            return sample_press(InputAction::Use, text_editing);
        }
        FireButtons sample_fire(bool text_editing)
        {
            const bool primary = sample_press(InputAction::FirePrimary, text_editing);
            return {primary, sample_press(InputAction::FireAlternate, text_editing)};
        }
        // zoomed: original CBloodClientShell quarters mouse offsets while m_bZoomView (keys unchanged).
        void look(PlayerRotation& rotation, double seconds, bool text_editing, bool zoomed = false)
        {
            const float dx = std::exchange(mouse_x, 0), dy = std::exchange(mouse_y, 0);
            if (!focused || !enabled || text_editing)
                return;
            const auto dt = static_cast<float>(std::clamp(seconds, 0.0, 0.1));
            // Device mapping policy: radians per relative mouse unit, independent of frame duration.
            if (mouse_captured)
            {
                const float scale = zoomed ? mouse_sensitivity / 4 : mouse_sensitivity;
                rotation.yaw = std::remainder(rotation.yaw + dx * scale, 6.28318530718f);
                rotation.pitch += dy * scale * (invert_y ? -1.f : 1.f);
            }
            rotation.yaw += (int(held(InputAction::TurnRight)) - int(held(InputAction::TurnLeft))) * 2 * dt;
            rotation.pitch =
                std::clamp(rotation.pitch + (int(held(InputAction::LookDown)) - int(held(InputAction::LookUp))) * dt,
                           -1.48f, 1.48f);
        }
    };
} // namespace OpenBlood2
