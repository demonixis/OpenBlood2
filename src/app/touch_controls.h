#pragma once
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_touch.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

namespace OpenBlood2
{
    // On-screen touch controls (replacement policy, no original counterpart). They are a virtual
    // gamepad: the floating left stick and the buttons emit ordinary gamepad events for this
    // fixed joystick ID, so the console layout and its rebinding apply unchanged. Positions and
    // radii are in window-height units, anchored to the left or right edge and the bottom.
    inline constexpr SDL_JoystickID touch_gamepad_id = 0x7FFFFFF0;
    inline constexpr float touch_stick_radius = .12f; // Full deflection, in window heights.

    struct TouchControl
    {
        const char* label;
        bool right;         // Anchored to the right edge (else the left one).
        float x, y, radius; // Center offset from the anchor edge and from the bottom.
        int button;         // SDL_GamepadButton, or -1 for a trigger axis.
        int axis;           // SDL_GamepadAxis of a trigger, or -1.
    };
    inline constexpr std::array<TouchControl, 10> touch_layout{{
        {"FIRE", true, .20f, .24f, .10f, -1, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER},
        {"ALT", true, .43f, .13f, .07f, -1, SDL_GAMEPAD_AXIS_LEFT_TRIGGER},
        {"JUMP", true, .13f, .47f, .07f, SDL_GAMEPAD_BUTTON_NORTH, -1},
        {"DUCK", true, .36f, .37f, .065f, SDL_GAMEPAD_BUTTON_RIGHT_STICK, -1},
        {"USE", true, .63f, .11f, .06f, SDL_GAMEPAD_BUTTON_SOUTH, -1},
        {"WEAPON", true, .10f, .86f, .06f, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, -1},
        {"ITEM", true, .26f, .86f, .06f, SDL_GAMEPAD_BUTTON_WEST, -1},
        {"NEXT", true, .42f, .86f, .055f, SDL_GAMEPAD_BUTTON_BACK, -1},
        {"RUN", false, .10f, .55f, .06f, SDL_GAMEPAD_BUTTON_LEFT_STICK, -1},
        {"MENU", false, .08f, .90f, .05f, SDL_GAMEPAD_BUTTON_START, -1},
    }};

    struct TouchControls
    {
        static constexpr std::size_t max_fingers = 10;
        static constexpr int stick = -1;
        // Shown from the first direct touch until another device is used.
        bool visible = false;
        struct Finger
        {
            SDL_FingerID id;
            int control; // touch_layout index, or stick.
        };
        std::vector<Finger> fingers;
        std::array<bool, touch_layout.size()> pressed{};
        // The stick's origin (where its finger landed) and current offset, in window heights.
        float origin_x = 0, origin_y = 0, stick_x = 0, stick_y = 0;
        bool stick_active = false;

        static SDL_Event button_event(int button, bool down)
        {
            SDL_Event e{};
            e.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
            e.gbutton.which = touch_gamepad_id;
            e.gbutton.button = Uint8(button);
            e.gbutton.down = down;
            return e;
        }
        static SDL_Event axis_event(int axis, float value)
        {
            SDL_Event e{};
            e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
            e.gaxis.which = touch_gamepad_id;
            e.gaxis.axis = Uint8(axis);
            e.gaxis.value = Sint16(std::lround(std::clamp(value, -1.f, 1.f) * 32767.f));
            return e;
        }
        static float center_x(const TouchControl& c, float aspect)
        {
            return c.right ? aspect - c.x : c.x;
        }
        // The control under a point (window-height units), with a quarter-radius margin.
        static int hit(float x, float y, float aspect)
        {
            for (std::size_t i = 0; i < touch_layout.size(); ++i)
            {
                const auto& c = touch_layout[i];
                const float dx = x - center_x(c, aspect), dy = y - (1 - c.y), r = c.radius * 1.25f;
                if (dx * dx + dy * dy <= r * r)
                    return int(i);
            }
            return -2;
        }
        void press(int control, bool down, std::vector<SDL_Event>& out)
        {
            auto& state = pressed[std::size_t(control)];
            if (state == down)
                return;
            state = down;
            const auto& c = touch_layout[std::size_t(control)];
            out.push_back(c.axis >= 0 ? axis_event(c.axis, down ? 1.f : 0.f) : button_event(c.button, down));
        }
        void move_stick(float x, float y, std::vector<SDL_Event>& out)
        {
            const float old_x = stick_x, old_y = stick_y;
            stick_x = std::clamp((x - origin_x) / touch_stick_radius, -1.f, 1.f);
            stick_y = std::clamp((y - origin_y) / touch_stick_radius, -1.f, 1.f);
            if (stick_x != old_x)
                out.push_back(axis_event(SDL_GAMEPAD_AXIS_LEFTX, stick_x));
            if (stick_y != old_y)
                out.push_back(axis_event(SDL_GAMEPAD_AXIS_LEFTY, stick_y));
        }
        // A finger event of this window while play accepts input. A finger landing on a control
        // holds it until it lifts; one landing elsewhere in the left half owns the floating stick;
        // other fingers are not consumed (the right half drags the camera). Synthetic gamepad
        // events go to `out`; true means the event was consumed.
        bool event(const SDL_Event& e, float aspect, bool play, std::vector<SDL_Event>& out)
        {
            const auto& f = e.tfinger;
            if (!std::isfinite(aspect) || aspect <= 0 || !std::isfinite(f.x) || !std::isfinite(f.y))
                return false;
            const float x = f.x * aspect, y = f.y;
            auto owned =
                std::find_if(fingers.begin(), fingers.end(), [&f](const Finger& g) { return g.id == f.fingerID; });
            if (e.type == SDL_EVENT_FINGER_DOWN)
            {
                if (!play || owned != fingers.end())
                    return false;
                visible = true;
                const int control = hit(x, y, aspect);
                if (control < 0 && (f.x >= .5f || stick_active))
                    return f.x < .5f; // A second left finger is swallowed; right fingers look.
                if (fingers.size() >= max_fingers)
                    return true;
                fingers.push_back({f.fingerID, control >= 0 ? control : stick});
                if (control >= 0)
                    press(control, true, out);
                else
                {
                    stick_active = true;
                    origin_x = x;
                    origin_y = y;
                    stick_x = stick_y = 0;
                }
                return true;
            }
            if (owned == fingers.end())
                return false;
            const int control = owned->control;
            if (e.type == SDL_EVENT_FINGER_MOTION)
            {
                if (control == stick)
                    move_stick(x, y, out);
                return true;
            }
            if (e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED)
            {
                fingers.erase(owned);
                if (control == stick)
                {
                    stick_active = false;
                    move_stick(origin_x, origin_y, out);
                }
                else
                    press(control, false, out);
                return true;
            }
            return false;
        }
        // Ownership changes forget every finger without events; the caller drops the virtual
        // gamepad state with them.
        void release()
        {
            fingers.clear();
            pressed = {};
            stick_active = false;
            stick_x = stick_y = 0;
        }
    };
} // namespace OpenBlood2
