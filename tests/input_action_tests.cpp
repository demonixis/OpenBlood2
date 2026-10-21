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
    SDL_Event focus(bool active)
    {
        SDL_Event e{};
        e.type = active ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
        e.window.windowID = window;
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
        for (const auto& entry : b.entries)
            CHECK(InputBindings::valid(entry));
        for (std::size_t i = 0; i < input_action_count; ++i)
            CHECK(InputAction(i) == InputAction::MenuBack || count(b, InputAction(i)) > 0);
        CHECK(count(b, InputAction::MenuBack) == 0); // Esc already returns; MenuBack is for the gamepad lot.
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
        for (const auto code : {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RETURN, SDL_SCANCODE_F5, SDL_SCANCODE_F6,
                                SDL_SCANCODE_F1, SDL_SCANCODE_O, SDL_SCANCODE_Q})
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
    return 2;
}
