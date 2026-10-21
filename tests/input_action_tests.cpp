#include "app/menu_input.h"
#include <iostream>
#include <set>
#include <string>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    constexpr SDL_WindowID window = 8;
    SDL_Event key(SDL_Scancode code, bool down, bool repeat = false)
    {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.windowID = window;
        e.key.scancode = code;
        e.key.down = down;
        e.key.repeat = repeat;
        return e;
    }
    SDL_Event button(Uint8 code, bool down)
    {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.windowID = window;
        e.button.button = code;
        e.button.down = down;
        return e;
    }
    SDL_Event wheel(int ticks, bool flipped = false, SDL_WindowID id = window)
    {
        SDL_Event e{};
        e.type = SDL_EVENT_MOUSE_WHEEL;
        e.wheel.windowID = id;
        e.wheel.y = float(ticks);
        e.wheel.integer_y = ticks;
        e.wheel.direction = flipped ? SDL_MOUSEWHEEL_FLIPPED : SDL_MOUSEWHEEL_NORMAL;
        return e;
    }
    SDL_Event focus(bool active)
    {
        SDL_Event e{};
        e.type = active ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
        e.window.windowID = window;
        return e;
    }
    SDL_Event pad_button(SDL_GamepadButton code, bool down, SDL_JoystickID id = 3)
    {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
        e.gbutton.which = id;
        e.gbutton.button = Uint8(code);
        e.gbutton.down = down;
        return e;
    }
    SDL_Event pad_axis(SDL_GamepadAxis axis, float value, SDL_JoystickID id = 3)
    {
        SDL_Event e{};
        e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
        e.gaxis.which = id;
        e.gaxis.axis = Uint8(axis);
        e.gaxis.value = Sint16(value * 32767);
        return e;
    }
    SDL_Event pad_removed(SDL_JoystickID id = 3)
    {
        SDL_Event e{};
        e.type = SDL_EVENT_GAMEPAD_REMOVED;
        e.gdevice.which = id;
        return e;
    }
    std::size_t count(const InputBindings& b, InputAction action)
    {
        std::size_t n = 0;
        for (const auto& entry : b.entries)
            n += entry.action == action;
        return n;
    }

    bool table()
    {
        std::set<std::string_view> names;
        for (const auto& entry : input_action_info)
            CHECK(!entry.name.empty() && names.insert(entry.name).second);
        CHECK(names.size() == input_action_count);
        for (int i = 0; i < 10; ++i)
            CHECK(weapon_slot_index(InputAction(int(InputAction::WeaponSlot1) + i)) == i);
        CHECK(weapon_slot_index(InputAction::Use) == -1 && weapon_slot_index(InputAction::Menu) == -1);
        InputBindings b;
        CHECK(!b.add(InputSource::Key, SDL_SCANCODE_UNKNOWN, InputAction::Jump));
        CHECK(!b.add(InputSource::Key, SDL_SCANCODE_COUNT, InputAction::Jump));
        for (const auto reserved : {SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F10})
            CHECK(!b.add(InputSource::Key, reserved, InputAction::Jump));
        CHECK(!b.add(InputSource::MouseButton, 0, InputAction::FirePrimary));
        CHECK(!b.add(InputSource::MouseButton, SDL_BUTTON_X2 + 1, InputAction::FirePrimary));
        CHECK(!b.add(InputSource::MouseWheel, 2, InputAction::NextWeapon) &&
              !b.add(InputSource::MouseWheel, -1, InputAction::NextWeapon));
        CHECK(!b.add(InputSource::Key, SDL_SCANCODE_O, InputAction::Count));
        CHECK(b.entries.empty());
        CHECK(b.add(InputSource::Key, SDL_SCANCODE_O, InputAction::Jump));
        CHECK(b.add(InputSource::Key, SDL_SCANCODE_O, InputAction::Jump) && b.entries.size() == 1);
        for (int i = 0; b.entries.size() < InputBindings::max_entries; ++i)
            CHECK(b.add(InputSource::Key, SDL_SCANCODE_A + i % 26, InputAction(i / 26)));
        CHECK(!b.add(InputSource::Key, SDL_SCANCODE_Y, InputAction::QuickLoad));
        CHECK(b.add(InputSource::Key, SDL_SCANCODE_O, InputAction::Jump)); // Existing entry, still kept once.
        b.clear(InputAction::Jump);
        CHECK(count(b, InputAction::Jump) == 0 && !b.bound(InputSource::Key, SDL_SCANCODE_O, InputAction::Jump));
        return true;
    }

    // The default table reproduces the pre-action layout exactly.
    bool defaults()
    {
        const auto b = InputBindings::keyboard_mouse();
        const auto all = InputBindings::defaults();
        for (const auto& entry : all.entries)
            CHECK(InputBindings::valid(entry));
        for (std::size_t i = 0; i < input_action_count; ++i)
            CHECK(count(all, InputAction(i)) > 0);
        for (const auto& entry : b.entries)
            CHECK(entry.source == InputSource::Key || entry.source == InputSource::MouseButton ||
                  entry.source == InputSource::MouseWheel);
        CHECK(b.bound(InputSource::MouseWheel, 0, InputAction::PrevWeapon) &&
              b.bound(InputSource::MouseWheel, 1, InputAction::NextWeapon));
        CHECK(count(b, InputAction::MenuBack) == 0); // Esc already returns; MenuBack is the gamepad's B.
        const auto bound = [&b](SDL_Scancode code, InputAction action)
        { return b.bound(InputSource::Key, code, action); };
        CHECK(bound(SDL_SCANCODE_W, InputAction::MoveForward) && bound(SDL_SCANCODE_S, InputAction::MoveBackward) &&
              bound(SDL_SCANCODE_A, InputAction::StrafeLeft) && bound(SDL_SCANCODE_D, InputAction::StrafeRight));
        CHECK(bound(SDL_SCANCODE_LSHIFT, InputAction::Run) && bound(SDL_SCANCODE_RSHIFT, InputAction::Run) &&
              bound(SDL_SCANCODE_SPACE, InputAction::Jump) && bound(SDL_SCANCODE_LCTRL, InputAction::Crouch) &&
              bound(SDL_SCANCODE_RCTRL, InputAction::Crouch));
        CHECK(bound(SDL_SCANCODE_UP, InputAction::LookUp) && bound(SDL_SCANCODE_UP, InputAction::MenuUp) &&
              bound(SDL_SCANCODE_LEFT, InputAction::TurnLeft) && bound(SDL_SCANCODE_LEFT, InputAction::MenuLeft));
        CHECK(b.bound(InputSource::MouseButton, SDL_BUTTON_LEFT, InputAction::FirePrimary) &&
              b.bound(InputSource::MouseButton, SDL_BUTTON_RIGHT, InputAction::FireAlternate) &&
              bound(SDL_SCANCODE_F, InputAction::FirePrimary) && bound(SDL_SCANCODE_G, InputAction::FireAlternate));
        CHECK(bound(SDL_SCANCODE_ESCAPE, InputAction::Menu) && bound(SDL_SCANCODE_RETURN, InputAction::MenuAccept) &&
              bound(SDL_SCANCODE_KP_ENTER, InputAction::MenuAccept) && bound(SDL_SCANCODE_F5, InputAction::QuickSave) &&
              bound(SDL_SCANCODE_F6, InputAction::QuickLoad));
        // Original defaults.cfg for the commands added with the gamepad layout.
        CHECK(bound(SDL_SCANCODE_APOSTROPHE, InputAction::NextWeapon) &&
              bound(SDL_SCANCODE_SEMICOLON, InputAction::PrevWeapon) &&
              bound(SDL_SCANCODE_LEFTBRACKET, InputAction::PrevItem) &&
              bound(SDL_SCANCODE_RIGHTBRACKET, InputAction::NextItem) &&
              bound(SDL_SCANCODE_RETURN, InputAction::UseItem));
        // The console layout requested for the gamepad.
        const auto pad = [&all](SDL_GamepadButton code, InputAction action)
        { return all.bound(InputSource::GamepadButton, code, action); };
        const auto axis = [&all](SDL_GamepadAxis code, bool positive, InputAction action)
        {
            return all.bound(positive ? InputSource::GamepadAxisPositive : InputSource::GamepadAxisNegative, code,
                             action);
        };
        CHECK(axis(SDL_GAMEPAD_AXIS_LEFTY, false, InputAction::MoveForward) &&
              axis(SDL_GAMEPAD_AXIS_LEFTY, true, InputAction::MoveBackward) &&
              axis(SDL_GAMEPAD_AXIS_LEFTX, false, InputAction::StrafeLeft) &&
              axis(SDL_GAMEPAD_AXIS_LEFTX, true, InputAction::StrafeRight));
        CHECK(axis(SDL_GAMEPAD_AXIS_RIGHTX, true, InputAction::LookHorizontal) &&
              axis(SDL_GAMEPAD_AXIS_RIGHTY, true, InputAction::LookVertical));
        CHECK(pad(SDL_GAMEPAD_BUTTON_LEFT_STICK, InputAction::RunLatch) &&
              axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, true, InputAction::FireAlternate) &&
              axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, true, InputAction::FirePrimary));
        CHECK(pad(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, InputAction::PrevWeapon) &&
              pad(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, InputAction::NextWeapon) &&
              pad(SDL_GAMEPAD_BUTTON_START, InputAction::Menu));
        CHECK(pad(SDL_GAMEPAD_BUTTON_NORTH, InputAction::Jump) && pad(SDL_GAMEPAD_BUTTON_SOUTH, InputAction::Use) &&
              pad(SDL_GAMEPAD_BUTTON_SOUTH, InputAction::MenuAccept) &&
              pad(SDL_GAMEPAD_BUTTON_EAST, InputAction::MenuBack) &&
              pad(SDL_GAMEPAD_BUTTON_WEST, InputAction::UseItem) &&
              pad(SDL_GAMEPAD_BUTTON_BACK, InputAction::NextItem) &&
              pad(SDL_GAMEPAD_BUTTON_DPAD_UP, InputAction::Detonate));
        MovementControls c;
        c.event(focus(true), window);
        // Every play key of the previous fixed table is still consumed; interface keys are not.
        for (const auto code :
             {SDL_SCANCODE_W,      SDL_SCANCODE_A,     SDL_SCANCODE_S,     SDL_SCANCODE_D,  SDL_SCANCODE_LSHIFT,
              SDL_SCANCODE_RSHIFT, SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL, SDL_SCANCODE_F,  SDL_SCANCODE_G,
              SDL_SCANCODE_R,      SDL_SCANCODE_B,     SDL_SCANCODE_T,     SDL_SCANCODE_P,  SDL_SCANCODE_L,
              SDL_SCANCODE_H,      SDL_SCANCODE_N,     SDL_SCANCODE_I,     SDL_SCANCODE_Z,  SDL_SCANCODE_E,
              SDL_SCANCODE_SPACE,  SDL_SCANCODE_LEFT,  SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
              SDL_SCANCODE_1,      SDL_SCANCODE_0,     SDL_SCANCODE_KP_5})
            CHECK(c.consumes(key(code, true), false));
        CHECK(c.consumes(key(SDL_SCANCODE_RETURN, true), false)); // Now also original UseInventory.
        for (const auto code :
             {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F5, SDL_SCANCODE_F6, SDL_SCANCODE_F1, SDL_SCANCODE_O, SDL_SCANCODE_Q})
            CHECK(!c.consumes(key(code, true), false));
        for (int i = 0; i < 10; ++i)
        {
            c.event(key(SDL_Scancode(SDL_SCANCODE_1 + i), true), window);
            CHECK(MovementControls::weapon_slot(SDL_Scancode(SDL_SCANCODE_1 + i)) == i);
            CHECK(c.sample_weapon_slot(false) == i && c.sample_weapon_slot(false) == -1);
            c.event(key(SDL_Scancode(SDL_SCANCODE_KP_1 + i), true), window);
            CHECK(c.sample_weapon_slot(false) == i);
        }
        return true;
    }

    bool remap()
    {
        MovementControls c;
        c.event(focus(true), window);
        c.bindings.clear(InputAction::MoveForward);
        CHECK(c.bindings.add(InputSource::Key, SDL_SCANCODE_O, InputAction::MoveForward));
        c.event(key(SDL_SCANCODE_W, true), window);
        CHECK(!c.sample(false).forward && !c.consumes(key(SDL_SCANCODE_W, true), false));
        c.event(key(SDL_SCANCODE_O, true), window);
        CHECK(c.sample(false).forward && c.consumes(key(SDL_SCANCODE_O, true), false));
        c.event(key(SDL_SCANCODE_O, false), window);
        CHECK(!c.sample(false).forward);
        // Several sources drive one action: their OR.
        c.event(key(SDL_SCANCODE_LSHIFT, true), window);
        c.event(key(SDL_SCANCODE_RSHIFT, true), window);
        c.event(key(SDL_SCANCODE_LSHIFT, false), window);
        CHECK(c.sample(false).run);
        c.event(key(SDL_SCANCODE_RSHIFT, false), window);
        CHECK(!c.sample(false).run);
        // A rebound press action keeps the fresh-press guard after focus loss.
        CHECK(c.bindings.add(InputSource::Key, SDL_SCANCODE_O, InputAction::Use));
        c.event(key(SDL_SCANCODE_O, true), window);
        CHECK(c.sample_use(false));
        c.event(focus(false), window);
        c.event(focus(true), window);
        c.event(key(SDL_SCANCODE_O, true, true), window);
        CHECK(!c.sample_use(false) && !c.sample(false).forward);
        c.event(key(SDL_SCANCODE_O, false), window);
        c.event(key(SDL_SCANCODE_O, true), window);
        CHECK(c.sample_use(false) && c.sample(false).forward);
        c.event(key(SDL_SCANCODE_O, false), window);
        // Hold actions still resume from repeats after focus returns, as before.
        c.event(key(SDL_SCANCODE_SPACE, true), window);
        c.event(focus(false), window);
        c.event(focus(true), window);
        c.event(key(SDL_SCANCODE_SPACE, true, true), window);
        CHECK(c.sample(false).jump);
        c.event(key(SDL_SCANCODE_SPACE, false), window);
        // Weapon slots and look keys follow their bindings.
        CHECK(c.bindings.add(InputSource::Key, SDL_SCANCODE_Q, InputAction::WeaponSlot3));
        c.event(key(SDL_SCANCODE_Q, true), window);
        CHECK(c.sample_weapon_slot(false) == 2);
        c.event(key(SDL_SCANCODE_Q, true, true), window);
        CHECK(c.sample_weapon_slot(false) == -1);
        CHECK(c.bindings.add(InputSource::Key, SDL_SCANCODE_K, InputAction::TurnRight));
        c.event(key(SDL_SCANCODE_K, true), window);
        PlayerRotation r;
        c.look(r, .05, false);
        CHECK(r.yaw > .09f && r.yaw < .11f);
        c.event(key(SDL_SCANCODE_K, false), window);
        // Mouse buttons: any of the five, only while captured.
        c.bindings.clear(InputAction::FirePrimary);
        CHECK(c.bindings.add(InputSource::MouseButton, SDL_BUTTON_MIDDLE, InputAction::FirePrimary));
        c.event(button(SDL_BUTTON_MIDDLE, true), window);
        CHECK(!c.sample_fire(false).primary);
        c.mouse_requested = c.mouse_captured = true; // Pure mapping test.
        c.event(button(SDL_BUTTON_LEFT, true), window);
        CHECK(!c.sample_fire(false).primary);
        c.event(button(SDL_BUTTON_MIDDLE, true), window);
        CHECK(c.sample_fire(false).primary);
        c.event(button(SDL_BUTTON_MIDDLE, false), window);
        CHECK(!c.sample_fire(false).primary);
        // A key bound to fire, held across a mouse release, may not start firing.
        CHECK(c.bindings.add(InputSource::Key, SDL_SCANCODE_J, InputAction::FirePrimary));
        c.event(key(SDL_SCANCODE_J, true), window);
        c.release_mouse();
        CHECK(!c.sample_fire(false).primary);
        c.event(key(SDL_SCANCODE_J, false), window);
        CHECK(!c.sample_fire(false).primary);
        c.event(key(SDL_SCANCODE_J, true), window);
        CHECK(c.sample_fire(false).primary);
        // The menu action is what releases the mouse, not a hard-coded Escape.
        c.mouse_requested = true;
        c.bindings.clear(InputAction::Menu);
        CHECK(c.bindings.add(InputSource::Key, SDL_SCANCODE_BACKSPACE, InputAction::Menu));
        c.event(key(SDL_SCANCODE_ESCAPE, true), window);
        CHECK(c.mouse_requested);
        c.event(key(SDL_SCANCODE_BACKSPACE, true), window);
        CHECK(!c.mouse_requested);
        return true;
    }

    bool gamepad()
    {
        MovementControls c;
        c.event(focus(true), window);
        // Left stick: digital movement past the threshold, both directions of one axis.
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, -.4f), window);
        CHECK(!c.sample(false).forward);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, -.8f), window);
        CHECK(c.sample(false).forward && !c.sample(false).backward && !c.sample(false).run);
        // Diagonal: both axes past the threshold.
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTX, .7f), window);
        CHECK(c.sample(false).forward && c.sample(false).right);
        // L3 latches run while any movement is held, then the latch ends.
        c.event(pad_button(SDL_GAMEPAD_BUTTON_LEFT_STICK, true), window);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_LEFT_STICK, false), window);
        CHECK(c.sample(false).run);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTX, 0), window);
        CHECK(c.sample(false).run);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, 1), window); // Through to backward: still moving.
        CHECK(c.sample(false).backward && !c.sample(false).forward && c.sample(false).run);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, 0), window);
        CHECK(!c.sample(false).backward && !c.sample(false).run);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, -1), window);
        CHECK(c.sample(false).forward && !c.sample(false).run);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_LEFT_STICK, true), window); // Releasing the stick ends it.
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, 0), window);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_LEFT_STICK, false), window);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, -1), window);
        CHECK(!c.sample(false).run);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, 0), window);
        // Triggers fire like the F/G keys; Y jumps; R3 crouches.
        c.event(pad_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, .9f), window);
        CHECK(c.sample_fire(false).primary && !c.sample_fire(false).alternate);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, .9f), window);
        CHECK(c.sample_fire(false).alternate);
        CHECK(!c.sample_fire(true).primary); // Text editing refuses and suppresses...
        CHECK(!c.sample_fire(false).primary);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, .2f), window); // ...until released.
        CHECK(!c.sample_fire(false).primary);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1), window);
        CHECK(c.sample_fire(false).primary);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0), window);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0), window);
        CHECK(!c.sample_fire(false).primary && !c.sample_fire(false).alternate);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_DPAD_UP, true), window); // Detonate, a Press action.
        CHECK(c.sample_detonate(false) && c.sample_detonate(false));
        c.event(pad_button(SDL_GAMEPAD_BUTTON_DPAD_UP, false), window);
        CHECK(!c.sample_detonate(false));
        c.event(pad_button(SDL_GAMEPAD_BUTTON_NORTH, true), window);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_RIGHT_STICK, true), window);
        CHECK(c.sample(false).jump && c.sample(false).crouch);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_NORTH, false), window);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_RIGHT_STICK, false), window);
        // Bumpers, X and Back queue one command per press.
        c.event(pad_button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true), window);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true), window); // No new edge.
        CHECK(c.sample_command(InputAction::NextWeapon, false) && !c.sample_command(InputAction::NextWeapon, false));
        c.event(pad_button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, false), window);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, true), window);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_WEST, true), window);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_BACK, true), window);
        CHECK(!c.sample_command(InputAction::NextWeapon, false) && c.sample_command(InputAction::PrevWeapon, false) &&
              c.sample_command(InputAction::UseItem, false) && c.sample_command(InputAction::NextItem, false));
        for (const auto code : {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_BACK})
            c.event(pad_button(code, false), window);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true), window);
        CHECK(!c.sample_command(InputAction::NextWeapon, true)); // Text editing drops it.
        c.event(pad_button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, false), window);
        c.enabled = false;
        c.event(pad_button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true), window);
        c.enabled = true;
        CHECK(!c.sample_command(InputAction::NextWeapon, false)); // Not queued while disabled.
        c.event(pad_button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, false), window);
        // Right stick: analog look past the deadzone, squared, quartered when zoomed.
        PlayerRotation r;
        c.event(pad_axis(SDL_GAMEPAD_AXIS_RIGHTX, .15f), window);
        c.look(r, .1, false);
        CHECK(r.yaw == 0);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_RIGHTX, 1), window);
        c.look(r, .1, false);
        CHECK(std::abs(r.yaw - gamepad_yaw_speed * .1f) < .001f);
        r = {};
        c.event(pad_axis(SDL_GAMEPAD_AXIS_RIGHTX, -.6f), window);
        c.look(r, .1, true, true);
        CHECK(r.yaw == 0); // Text editing.
        c.look(r, .1, false, true);
        CHECK(std::abs(r.yaw + .25f * gamepad_yaw_speed * .25f * .1f) < .001f); // ((.6-.2)/.8)^2 = .25.
        c.event(pad_axis(SDL_GAMEPAD_AXIS_RIGHTX, 0), window);
        r = {};
        c.event(pad_axis(SDL_GAMEPAD_AXIS_RIGHTY, 1), window);
        c.look(r, .1, false);
        CHECK(std::abs(r.pitch - gamepad_pitch_speed * .1f) < .001f);
        c.invert_y = true;
        c.look(r, .1, false);
        CHECK(std::abs(r.pitch) < .001f);
        c.invert_y = false;
        c.event(pad_axis(SDL_GAMEPAD_AXIS_RIGHTY, 0), window);
        // Several gamepads: OR of their sources; removal releases one.
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, -1, 3), window);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, -1, 4), window);
        c.event(pad_removed(3), window);
        CHECK(c.sample(false).forward && c.gamepads.size() == 1);
        c.event(pad_removed(4), window);
        CHECK(!c.sample(false).forward && c.gamepads.empty());
        for (SDL_JoystickID id = 1; id <= 20; ++id)
            c.event(pad_button(SDL_GAMEPAD_BUTTON_SOUTH, true, id), window);
        CHECK(c.gamepads.size() == max_gamepads);
        for (SDL_JoystickID id = 1; id <= 20; ++id)
            c.event(pad_removed(id), window);
        // Player changes keep held movement but a held Press source needs a fresh press.
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, -1), window);
        c.event(pad_button(SDL_GAMEPAD_BUTTON_SOUTH, true), window);
        c.player_changed(true);
        CHECK(c.sample(false).forward && !c.sample_use(false));
        c.event(pad_button(SDL_GAMEPAD_BUTTON_SOUTH, false), window);
        CHECK(!c.sample_use(false));
        c.event(pad_button(SDL_GAMEPAD_BUTTON_SOUTH, true), window);
        CHECK(c.sample_use(false));
        // Focus loss drops gamepad state and ignores background events; a poll restores it.
        c.event(focus(false), window);
        CHECK(c.gamepads.empty() && !c.sample(false).forward);
        c.event(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, -1), window);
        CHECK(c.gamepads.empty());
        c.event(focus(true), window);
        GamepadState polled{3, {}, {}};
        polled.axes[SDL_GAMEPAD_AXIS_LEFTY] = -1;
        polled.buttons[SDL_GAMEPAD_BUTTON_SOUTH] = true;
        c.set_gamepad(polled);
        CHECK(c.sample(false).forward && !c.sample_use(false));
        return true;
    }

    // Wheel ticks cycle weapons only while the mouse is captured (play owns it).
    bool mouse_wheel()
    {
        MovementControls c;
        c.event(focus(true), window);
        c.event(wheel(1), window);
        CHECK(!c.sample_command(InputAction::PrevWeapon, false) && !c.consumes(wheel(1), false));
        c.mouse_requested = c.mouse_captured = true; // Pure mapping test.
        CHECK(c.consumes(wheel(1), false));
        c.event(wheel(1), window);
        CHECK(c.sample_command(InputAction::PrevWeapon, false) &&
              !c.sample_command(InputAction::NextWeapon, false));
        c.event(wheel(-3), window); // Several ticks in one event queue one command.
        CHECK(c.sample_command(InputAction::NextWeapon, false) &&
              !c.sample_command(InputAction::NextWeapon, false));
        c.event(wheel(1, true), window); // Natural scrolling: SDL reports flipped values.
        CHECK(c.sample_command(InputAction::NextWeapon, false));
        c.event(wheel(0), window); // A sub-tick trackpad delta.
        c.event(wheel(1, false, 99), window); // Another window.
        CHECK(!c.sample_command(InputAction::NextWeapon, false) && !c.sample_command(InputAction::PrevWeapon, false));
        c.event(wheel(1), window, true); // Text editing drops it.
        CHECK(!c.sample_command(InputAction::PrevWeapon, false));
        c.bindings.clear(InputAction::PrevWeapon);
        c.event(wheel(1), window);
        CHECK(!c.sample_command(InputAction::PrevWeapon, false));
        return true;
    }

    bool gamepad_menu()
    {
        Menu m;
        MenuInput input;
        input.controls.focused = true;
        const auto send = [&](const SDL_Event& e) { return input.event(e, window, m, true, true, false, 640, 480); };
        send(pad_button(SDL_GAMEPAD_BUTTON_START, true));
        CHECK(m.active && m.page == MenuPage::Main && m.selected == 0);
        send(pad_button(SDL_GAMEPAD_BUTTON_START, false));
        send(pad_button(SDL_GAMEPAD_BUTTON_DPAD_DOWN, true));
        const int first = m.selected;
        CHECK(first != 0);
        send(pad_button(SDL_GAMEPAD_BUTTON_DPAD_DOWN, false));
        send(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, .6f)); // One step per threshold crossing.
        send(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, .9f));
        const int second = m.selected;
        CHECK(second != first);
        send(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, 0));
        send(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, -.9f));
        CHECK(m.selected == first);
        send(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, 0));
        m.selected = 0;
        send(pad_button(SDL_GAMEPAD_BUTTON_SOUTH, true));
        CHECK(m.page == MenuPage::Solo);
        send(pad_button(SDL_GAMEPAD_BUTTON_SOUTH, false));
        send(pad_button(SDL_GAMEPAD_BUTTON_EAST, true));
        CHECK(m.active && m.page == MenuPage::Main);
        send(pad_button(SDL_GAMEPAD_BUTTON_EAST, false));
        // Start resumes; a trigger held into play needs a fresh press; B never opens the menu.
        send(pad_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1));
        send(pad_button(SDL_GAMEPAD_BUTTON_START, true));
        CHECK(!m.active && input.controls.mouse_requested);
        CHECK(!input.controls.sample_fire(false).primary);
        send(pad_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0));
        CHECK(!input.controls.sample_fire(false).primary);
        send(pad_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1));
        CHECK(input.controls.sample_fire(false).primary);
        send(pad_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0));
        send(pad_button(SDL_GAMEPAD_BUTTON_START, false));
        send(pad_button(SDL_GAMEPAD_BUTTON_EAST, true));
        CHECK(!m.active);
        // Bumpers reach play through the menu input path.
        send(pad_button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true));
        CHECK(input.controls.sample_command(InputAction::NextWeapon, false));
        // State changes while the menu is open are kept: no stuck stick after closing it.
        send(pad_button(SDL_GAMEPAD_BUTTON_START, true));
        send(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, -1));
        CHECK(m.active && input.controls.held(InputAction::MoveForward));
        send(pad_button(SDL_GAMEPAD_BUTTON_START, false));
        send(pad_axis(SDL_GAMEPAD_AXIS_LEFTY, 0));
        CHECK(!input.controls.held(InputAction::MoveForward));
        // Removal is processed even without focus.
        send(pad_axis(SDL_GAMEPAD_AXIS_LEFTX, 1));
        input.controls.focused = false;
        send(pad_removed());
        CHECK(input.controls.gamepads.empty());
        // The credits screen leaves with B or Start.
        input.controls.focused = true;
        input.credits_screen = true;
        send(pad_button(SDL_GAMEPAD_BUTTON_EAST, true));
        CHECK(input.credits_exit);
        return true;
    }

    bool menu()
    {
        Menu m;
        MenuInput input;
        input.controls.focused = true;
        auto& b = input.controls.bindings;
        const auto send = [&](SDL_Scancode code, bool repeat = false)
        { return input.event(key(code, true, repeat), window, m, true, true, false, 640, 480); };
        b.clear(InputAction::Menu);
        b.clear(InputAction::MenuDown);
        b.clear(InputAction::MenuAccept);
        CHECK(b.add(InputSource::Key, SDL_SCANCODE_M, InputAction::Menu));
        CHECK(b.add(InputSource::Key, SDL_SCANCODE_K, InputAction::MenuDown));
        CHECK(b.add(InputSource::Key, SDL_SCANCODE_J, InputAction::MenuAccept));
        CHECK(b.add(InputSource::Key, SDL_SCANCODE_BACKSPACE, InputAction::MenuBack));
        send(SDL_SCANCODE_ESCAPE);
        CHECK(!m.active);
        send(SDL_SCANCODE_M);
        CHECK(m.active && m.page == MenuPage::Main && m.selected == 0);
        send(SDL_SCANCODE_K);
        const int first = m.selected;
        send(SDL_SCANCODE_K, true); // Navigation repeats.
        CHECK(first != 0 && m.selected != first);
        m.selected = 0;
        send(SDL_SCANCODE_RETURN);
        CHECK(m.page == MenuPage::Main);
        send(SDL_SCANCODE_J, true); // Activation needs a fresh press.
        CHECK(m.page == MenuPage::Main);
        send(SDL_SCANCODE_J);
        CHECK(m.page == MenuPage::Solo);
        send(SDL_SCANCODE_BACKSPACE);
        CHECK(m.active && m.page == MenuPage::Main);
        send(SDL_SCANCODE_M);
        CHECK(!m.active && input.controls.mouse_requested);
        send(SDL_SCANCODE_BACKSPACE); // MenuBack never opens the menu from play.
        CHECK(!m.active);
        // Quick save/load follow their actions.
        b.clear(InputAction::QuickSave);
        CHECK(b.add(InputSource::Key, SDL_SCANCODE_F7, InputAction::QuickSave));
        send(SDL_SCANCODE_F5);
        CHECK(!input.quick_save);
        send(SDL_SCANCODE_F7);
        CHECK(input.quick_save && !input.quick_load);
        send(SDL_SCANCODE_F6);
        CHECK(input.quick_load);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const std::string mode = argv[1];
    if (mode == "table")
        return table() ? 0 : 1;
    if (mode == "defaults")
        return defaults() ? 0 : 1;
    if (mode == "remap")
        return remap() ? 0 : 1;
    if (mode == "menu")
        return menu() ? 0 : 1;
    if (mode == "gamepad")
        return gamepad() ? 0 : 1;
    if (mode == "mouse_wheel")
        return mouse_wheel() ? 0 : 1;
    if (mode == "gamepad_menu")
        return gamepad_menu() ? 0 : 1;
    return 2;
}
