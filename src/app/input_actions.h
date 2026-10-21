#pragma once
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace OpenBlood2
{
    // Logical commands. Devices reach play and the menu only through InputBindings;
    // the original game-facing flags (MovementInput, use_down, ...) are unchanged.
    enum class InputAction : std::uint8_t
    {
        MoveForward,
        MoveBackward,
        StrafeLeft,
        StrafeRight,
        Run,
        Jump,
        Crouch,
        TurnLeft,
        TurnRight,
        LookUp,
        LookDown,
        FirePrimary,
        FireAlternate,
        Use,
        Flashlight,
        Medkit,
        Binoculars,
        NightGoggles,
        TheEye,
        ProximityBomb,
        TimeBomb,
        RemoteBomb,
        Detonate,
        WeaponSlot1,
        WeaponSlot2,
        WeaponSlot3,
        WeaponSlot4,
        WeaponSlot5,
        WeaponSlot6,
        WeaponSlot7,
        WeaponSlot8,
        WeaponSlot9,
        WeaponSlot10,
        Menu,
        MenuUp,
        MenuDown,
        MenuLeft,
        MenuRight,
        MenuAccept,
        MenuBack,
        QuickSave,
        QuickLoad,
        Count
    };
    inline constexpr std::size_t input_action_count = std::size_t(InputAction::Count);

    enum class InputActionKind : std::uint8_t
    {
        Hold,     // Level state; a held key resumes after focus returns (movement, look keys).
        Press,    // Level state that must start with a fresh press while play accepts input.
        Slot,     // Non-repeated down edge queued for the frame (weapon selection).
        Interface // Menu/system command; never consumed from the inspector or held for play.
    };

    struct InputActionInfo
    {
        std::string_view name; // Stable identifier for future preference files.
        InputActionKind kind;
    };

    inline constexpr std::array<InputActionInfo, input_action_count> input_action_info{{
        {"move_forward", InputActionKind::Hold},
        {"move_backward", InputActionKind::Hold},
        {"strafe_left", InputActionKind::Hold},
        {"strafe_right", InputActionKind::Hold},
        {"run", InputActionKind::Hold},
        {"jump", InputActionKind::Hold},
        {"crouch", InputActionKind::Hold},
        {"turn_left", InputActionKind::Hold},
        {"turn_right", InputActionKind::Hold},
        {"look_up", InputActionKind::Hold},
        {"look_down", InputActionKind::Hold},
        {"fire_primary", InputActionKind::Press},
        {"fire_alternate", InputActionKind::Press},
        {"use", InputActionKind::Press},
        {"flashlight", InputActionKind::Press},
        {"medkit", InputActionKind::Press},
        {"binoculars", InputActionKind::Press},
        {"night_goggles", InputActionKind::Press},
        {"the_eye", InputActionKind::Press},
        {"proximity_bomb", InputActionKind::Press},
        {"time_bomb", InputActionKind::Press},
        {"remote_bomb", InputActionKind::Press},
        {"detonate", InputActionKind::Press},
        {"weapon_slot_1", InputActionKind::Slot},
        {"weapon_slot_2", InputActionKind::Slot},
        {"weapon_slot_3", InputActionKind::Slot},
        {"weapon_slot_4", InputActionKind::Slot},
        {"weapon_slot_5", InputActionKind::Slot},
        {"weapon_slot_6", InputActionKind::Slot},
        {"weapon_slot_7", InputActionKind::Slot},
        {"weapon_slot_8", InputActionKind::Slot},
        {"weapon_slot_9", InputActionKind::Slot},
        {"weapon_slot_10", InputActionKind::Slot},
        {"menu", InputActionKind::Interface},
        {"menu_up", InputActionKind::Interface},
        {"menu_down", InputActionKind::Interface},
        {"menu_left", InputActionKind::Interface},
        {"menu_right", InputActionKind::Interface},
        {"menu_accept", InputActionKind::Interface},
        {"menu_back", InputActionKind::Interface},
        {"quick_save", InputActionKind::Interface},
        {"quick_load", InputActionKind::Interface},
    }};

    constexpr const InputActionInfo& info(InputAction action)
    {
        return input_action_info[std::size_t(action)];
    }
    constexpr int weapon_slot_index(InputAction action)
    {
        return action >= InputAction::WeaponSlot1 && action <= InputAction::WeaponSlot10
                   ? int(action) - int(InputAction::WeaponSlot1)
                   : -1;
    }

    enum class InputSource : std::uint8_t
    {
        Key,        // SDL_Scancode: physical position (WASD on QWERTY is ZQSD on AZERTY).
        MouseButton // SDL_BUTTON_LEFT..SDL_BUTTON_X2; play reads it only while the mouse is captured.
    };

    struct InputBinding
    {
        InputSource source;
        int code;
        InputAction action;
        bool operator==(const InputBinding&) const = default;
    };

    // Many-to-many: several sources may drive one action (held state is their OR) and
    // one source may serve play and menu actions; the active context picks the action.
    struct InputBindings
    {
        static constexpr std::size_t max_entries = 256;
        std::vector<InputBinding> entries;

        // F1/F2/F3/F10 stay inspector/diagnostic shortcuts handled before bindings.
        static bool reserved_key(int code)
        {
            return code == SDL_SCANCODE_F1 || code == SDL_SCANCODE_F2 || code == SDL_SCANCODE_F3 ||
                   code == SDL_SCANCODE_F10;
        }
        static bool valid(const InputBinding& b)
        {
            if (std::size_t(b.action) >= input_action_count)
                return false;
            switch (b.source)
            {
                case InputSource::Key:
                    return b.code > SDL_SCANCODE_UNKNOWN && b.code < SDL_SCANCODE_COUNT && !reserved_key(b.code);
                case InputSource::MouseButton:
                    return b.code >= SDL_BUTTON_LEFT && b.code <= SDL_BUTTON_X2;
            }
            return false;
        }
        // Refuses invalid, reserved or excess entries; a duplicate is accepted and kept once.
        bool add(InputSource source, int code, InputAction action)
        {
            const InputBinding b{source, code, action};
            if (!valid(b))
                return false;
            for (const auto& entry : entries)
                if (entry == b)
                    return true;
            if (entries.size() >= max_entries)
                return false;
            entries.push_back(b);
            return true;
        }
        void clear(InputAction action)
        {
            std::erase_if(entries, [action](const InputBinding& b) { return b.action == action; });
        }
        bool bound(InputSource source, int code, InputAction action) const
        {
            for (const auto& b : entries)
                if (b.source == source && b.code == code && b.action == action)
                    return true;
            return false;
        }
        template <class Predicate> bool bound_if(InputSource source, int code, Predicate&& predicate) const
        {
            for (const auto& b : entries)
                if (b.source == source && b.code == code && predicate(b.action))
                    return true;
            return false;
        }
        // Key and mouse-button events only; window, focus and repeat checks stay with the caller.
        bool matches(const SDL_Event& e, InputAction action) const
        {
            if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP)
                return bound(InputSource::Key, e.key.scancode, action);
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP)
                return bound(InputSource::MouseButton, e.button.button, action);
            return false;
        }

        // The pre-action layout, unchanged: WASD/ZQSD, Shift run, Space jump, Ctrl crouch,
        // arrows look, F/G or left/right mouse fire, E use, item letters, 1..0 and keypad
        // slots; Esc menu, arrows/Enter menu navigation, F5/F6 quick save/load.
        static InputBindings keyboard_mouse()
        {
            InputBindings b;
            const auto key = [&b](SDL_Scancode code, InputAction action) { b.add(InputSource::Key, code, action); };
            key(SDL_SCANCODE_W, InputAction::MoveForward);
            key(SDL_SCANCODE_S, InputAction::MoveBackward);
            key(SDL_SCANCODE_A, InputAction::StrafeLeft);
            key(SDL_SCANCODE_D, InputAction::StrafeRight);
            key(SDL_SCANCODE_LSHIFT, InputAction::Run);
            key(SDL_SCANCODE_RSHIFT, InputAction::Run);
            key(SDL_SCANCODE_SPACE, InputAction::Jump);
            key(SDL_SCANCODE_LCTRL, InputAction::Crouch);
            key(SDL_SCANCODE_RCTRL, InputAction::Crouch);
            key(SDL_SCANCODE_LEFT, InputAction::TurnLeft);
            key(SDL_SCANCODE_RIGHT, InputAction::TurnRight);
            key(SDL_SCANCODE_UP, InputAction::LookUp);
            key(SDL_SCANCODE_DOWN, InputAction::LookDown);
            key(SDL_SCANCODE_F, InputAction::FirePrimary);
            key(SDL_SCANCODE_G, InputAction::FireAlternate);
            b.add(InputSource::MouseButton, SDL_BUTTON_LEFT, InputAction::FirePrimary);
            b.add(InputSource::MouseButton, SDL_BUTTON_RIGHT, InputAction::FireAlternate);
            key(SDL_SCANCODE_E, InputAction::Use);
            key(SDL_SCANCODE_L, InputAction::Flashlight);
            key(SDL_SCANCODE_H, InputAction::Medkit);
            key(SDL_SCANCODE_Z, InputAction::Binoculars);
            key(SDL_SCANCODE_N, InputAction::NightGoggles);
            key(SDL_SCANCODE_I, InputAction::TheEye);
            key(SDL_SCANCODE_P, InputAction::ProximityBomb);
            key(SDL_SCANCODE_T, InputAction::TimeBomb);
            key(SDL_SCANCODE_R, InputAction::RemoteBomb);
            key(SDL_SCANCODE_B, InputAction::Detonate);
            for (int i = 0; i < 10; ++i)
            {
                const auto slot = InputAction(int(InputAction::WeaponSlot1) + i);
                key(SDL_Scancode(SDL_SCANCODE_1 + i), slot); // 1..9 then 0.
                key(SDL_Scancode(SDL_SCANCODE_KP_1 + i), slot);
            }
            key(SDL_SCANCODE_ESCAPE, InputAction::Menu);
            key(SDL_SCANCODE_UP, InputAction::MenuUp);
            key(SDL_SCANCODE_DOWN, InputAction::MenuDown);
            key(SDL_SCANCODE_LEFT, InputAction::MenuLeft);
            key(SDL_SCANCODE_RIGHT, InputAction::MenuRight);
            key(SDL_SCANCODE_RETURN, InputAction::MenuAccept);
            key(SDL_SCANCODE_KP_ENTER, InputAction::MenuAccept);
            key(SDL_SCANCODE_F5, InputAction::QuickSave);
            key(SDL_SCANCODE_F6, InputAction::QuickLoad);
            return b;
        }
    };
} // namespace OpenBlood2
