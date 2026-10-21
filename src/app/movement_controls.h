#pragma once
#include "app/input_actions.h"
#include "game/player_messages.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_mouse.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace OpenBlood2
{
    // Device policy for gamepads (replacement values, not retail configuration).
    inline constexpr float gamepad_digital_threshold = .5f; // Axis as a button: |value| above this.
    inline constexpr float gamepad_look_deadzone = .2f;     // Analog look: rescaled past this, then squared.
    inline constexpr float gamepad_yaw_speed = 3.5f;        // Radians per second at full deflection.
    inline constexpr float gamepad_pitch_speed = 2.5f;
    inline constexpr std::size_t max_gamepads = 8;

    struct GamepadState
    {
        SDL_JoystickID id = 0;
        std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
        std::array<float, SDL_GAMEPAD_AXIS_COUNT> axes{}; // Normalized to [-1, 1].
    };

    // Device state behind InputBindings (default: keyboard/mouse plus the console gamepad
    // layout listed there). Input state is cleared on focus loss; only events for this
    // window participate. Gamepad state is event-driven and read from every open gamepad.
    // Also testable headless.
    struct MovementControls
    {
        InputBindings bindings = InputBindings::defaults();
        std::array<bool, SDL_SCANCODE_COUNT> keys{};
        std::vector<GamepadState> gamepads; // At most max_gamepads, in first-event order.
        int pending_weapon_slot = -1;
        std::array<bool, input_action_count> pending{}; // Command actions queued for the frame.
        bool run_latched = false;
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
        // One queued Command action (next/previous weapon, inventory cycling and use).
        bool sample_command(InputAction action, bool text_editing)
        {
            return std::exchange(pending[std::size_t(action)], false) && focused && enabled && !text_editing;
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
        static bool gamepad_event(const SDL_Event& e)
        {
            return e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP ||
                   e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION || e.type == SDL_EVENT_GAMEPAD_REMOVED;
        }
        bool source_held(const InputBinding& b) const
        {
            switch (b.source)
            {
                case InputSource::Key:
                    return keys[unsigned(b.code)];
                case InputSource::MouseButton:
                    return mouse_captured && mouse_buttons[unsigned(b.code - 1)];
                case InputSource::GamepadButton:
                    for (const auto& pad : gamepads)
                        if (pad.buttons[unsigned(b.code)])
                            return true;
                    return false;
                case InputSource::GamepadAxisPositive:
                case InputSource::GamepadAxisNegative:
                    for (const auto& pad : gamepads)
                    {
                        const float v = pad.axes[unsigned(b.code)];
                        if (b.source == InputSource::GamepadAxisPositive ? v > gamepad_digital_threshold
                                                                         : v < -gamepad_digital_threshold)
                            return true;
                    }
                    return false;
            }
            return false;
        }
        // OR of every bound source; mouse buttons count only while captured.
        bool held(InputAction action) const
        {
            for (const auto& b : bindings.entries)
                if (b.action == action && source_held(b))
                    return true;
            return false;
        }
        // Sum of every bound axis past the deadzone, squared for fine aim, in [-1, 1].
        float analog(InputAction action) const
        {
            float sum = 0;
            for (const auto& b : bindings.entries)
                if (b.action == action && gamepad_axis(b.source))
                    for (const auto& pad : gamepads)
                    {
                        const float v = pad.axes[unsigned(b.code)], magnitude = std::abs(v);
                        if (magnitude <= gamepad_look_deadzone)
                            continue;
                        const float shaped =
                            std::pow((magnitude - gamepad_look_deadzone) / (1 - gamepad_look_deadzone), 2.f);
                        sum += (v < 0) == (b.source == InputSource::GamepadAxisPositive) ? -shaped : shaped;
                    }
            return std::clamp(sum, -1.f, 1.f);
        }
        bool moving() const
        {
            return held(InputAction::MoveForward) || held(InputAction::MoveBackward) || held(InputAction::StrafeLeft) ||
                   held(InputAction::StrafeRight);
        }
        // A held Press source may not start its action after play resumes (a gamepad
        // button that closed the menu, a trigger held into it).
        void rearm_presses()
        {
            for (std::size_t i = 0; i < input_action_count; ++i)
                if (input_action_info[i].kind == InputActionKind::Press)
                    suppressed[i] = held(InputAction(i));
        }
        // Replaces one gamepad's state from a poll (focus regained); never produces edges.
        void set_gamepad(const GamepadState& state)
        {
            auto* pad = find_gamepad(state.id, true);
            if (!pad)
                return;
            *pad = state;
            rearm_presses();
            if (run_latched && !moving())
                run_latched = false;
        }
        GamepadState* find_gamepad(SDL_JoystickID id, bool add)
        {
            for (auto& pad : gamepads)
                if (pad.id == id)
                    return &pad;
            if (!add || gamepads.size() >= max_gamepads)
                return nullptr;
            return &gamepads.emplace_back(GamepadState{id, {}, {}});
        }
        // Updates gamepad state and returns the digital edges of this event; key and
        // mouse-button events are stateless here (their state changes in event()).
        InputEdges track(const SDL_Event& e)
        {
            if (!gamepad_event(e))
                return InputEdges::from(e);
            InputEdges edges;
            if (e.type == SDL_EVENT_GAMEPAD_REMOVED)
            {
                std::erase_if(gamepads, [&e](const GamepadState& pad) { return pad.id == e.gdevice.which; });
                if (run_latched && !moving())
                    run_latched = false;
                return edges; // Removal releases its sources without generating commands.
            }
            if (!focused)
                return edges;
            if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
            {
                auto* pad = find_gamepad(e.gaxis.which, true);
                if (!pad || e.gaxis.axis >= SDL_GAMEPAD_AXIS_COUNT)
                    return edges;
                const float old = pad->axes[e.gaxis.axis];
                const float now = std::max(float(e.gaxis.value) / 32767.f, -1.f);
                pad->axes[e.gaxis.axis] = now;
                for (const bool positive : {false, true})
                {
                    const auto side = [positive](float v)
                    { return positive ? v > gamepad_digital_threshold : v < -gamepad_digital_threshold; };
                    if (side(old) != side(now))
                        edges.push({positive ? InputSource::GamepadAxisPositive : InputSource::GamepadAxisNegative,
                                    e.gaxis.axis, side(now), false});
                }
            }
            else
            {
                auto* pad = find_gamepad(e.gbutton.which, true);
                const bool down = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
                if (!pad || e.gbutton.button >= SDL_GAMEPAD_BUTTON_COUNT || pad->buttons[e.gbutton.button] == down)
                    return edges;
                pad->buttons[e.gbutton.button] = down;
                edges.push({InputSource::GamepadButton, e.gbutton.button, down, false});
            }
            if (run_latched && !moving())
                run_latched = false;
            return edges;
        }
        // Fresh down edges: weapon slots and Command actions are queued while play accepts
        // input, RunLatch latches run, Menu releases the mouse.
        void command_edges(const InputEdges& edges, bool text_editing)
        {
            for (const auto& edge : edges)
            {
                if (!edge.down || edge.repeat)
                    continue;
                bool slot = false;
                for (const auto& b : bindings.entries)
                {
                    if (b.source != edge.source || b.code != edge.code)
                        continue;
                    const auto kind = info(b.action).kind;
                    if (enabled && !text_editing)
                    {
                        if (weapon_slot_index(b.action) >= 0 && !slot)
                        {
                            pending_weapon_slot = weapon_slot_index(b.action);
                            slot = true;
                        }
                        else if (kind == InputActionKind::Command && weapon_slot_index(b.action) < 0)
                            pending[std::size_t(b.action)] = true;
                        else if (kind == InputActionKind::Latch)
                            run_latched = true;
                    }
                    if (b.action == InputAction::Menu)
                        release_mouse();
                }
            }
            if (run_latched && !moving())
                run_latched = false;
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
        // Gamepad state survives (no stale-state risk while focused); its held Press
        // sources must be released first, and held movement resumes immediately.
        void player_changed(bool play = false)
        {
            pending_weapon_slot = -1;
            pending = {};
            run_latched = false;
            keys = {};
            suppressed = {};
            release_mouse();
            rearm_presses();
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
                pending = {};
                run_latched = false;
                keys = {};
                gamepads.clear(); // Background gamepad events may be lost: never keep stale state.
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
                {
                    mouse_buttons[event.button.button - 1u] = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                    command_edges(InputEdges::from(event), text_editing);
                }
            }
            if (gamepad_event(event))
            {
                command_edges(track(event), text_editing);
                return;
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
                if (run_latched && !moving())
                    run_latched = false;
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
                {
                    command_edges(InputEdges::from(event), text_editing);
                    if (code == SDL_SCANCODE_F2)
                        enabled = !enabled;
                    if (code == SDL_SCANCODE_F3)
                    {
                        if (mouse_requested)
                            release_mouse();
                        else
                            request_play();
                    }
                    if (code == SDL_SCANCODE_F1 && (mouse_requested || mouse_captured))
                        release_mouse();
                }
            }
        }
        MovementInput sample(bool text_editing) const
        {
            if (!focused || !enabled || text_editing)
                return {};
            return {
                held(InputAction::MoveForward), held(InputAction::MoveBackward),       held(InputAction::StrafeLeft),
                held(InputAction::StrafeRight), held(InputAction::Run) || run_latched, held(InputAction::Jump),
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
            // Gamepad look: radians per second, quartered while zoomed like mouse offsets.
            const float stick = zoomed ? .25f : 1.f;
            if (const float x = analog(InputAction::LookHorizontal))
                rotation.yaw = std::remainder(rotation.yaw + x * gamepad_yaw_speed * stick * dt, 6.28318530718f);
            rotation.pitch +=
                analog(InputAction::LookVertical) * gamepad_pitch_speed * stick * dt * (invert_y ? -1.f : 1.f);
            rotation.yaw += (int(held(InputAction::TurnRight)) - int(held(InputAction::TurnLeft))) * 2 * dt;
            rotation.pitch =
                std::clamp(rotation.pitch + (int(held(InputAction::LookDown)) - int(held(InputAction::LookUp))) * dt,
                           -1.48f, 1.48f);
        }
    };
} // namespace OpenBlood2
