#pragma once
#include "app/input_actions.h"
#include "engine/atomic_file.h"
#include "engine/config.h"
#include "game/menu.h"
#include "game/original/MenuIds.h"
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_keyboard.h>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace OpenBlood2
{
    // One Controls row: an original CMenuControls action (string ID) mapped onto the action
    // layer, or a replacement row (literal) for an action the original reached another way.
    // Original actions without a port (Strafe, RunLock, TurnAround, DropWeapon, Grab, Message,
    // Taunt, MouseAimToggle, Crosshair) stay visible and disabled.
    struct ControlRow
    {
        unsigned string_id;
        std::string_view literal;
        std::optional<InputAction> action;
    };
    inline constexpr std::array<ControlRow, 48> control_rows{{
        {705, {}, InputAction::MoveForward},   // Forward
        {706, {}, InputAction::MoveBackward},  // Backward
        {707, {}, InputAction::TurnLeft},      // Left
        {708, {}, InputAction::TurnRight},     // Right
        {709, {}, std::nullopt},               // Strafe (modifier, not ported)
        {710, {}, InputAction::StrafeLeft},    // StrafeLeft
        {711, {}, InputAction::StrafeRight},   // StrafeRight
        {712, {}, InputAction::Run},           // Run
        {713, {}, std::nullopt},               // RunLock (always-run toggle, not ported)
        {714, {}, InputAction::Jump},          // Jump
        {715, {}, InputAction::Crouch},        // Duck
        {716, {}, std::nullopt},               // TurnAround
        {717, {}, InputAction::FirePrimary},   // Fire
        {718, {}, InputAction::FireAlternate}, // AltFire
        {719, {}, InputAction::NextWeapon},    // NextWeapon
        {720, {}, InputAction::PrevWeapon},    // PrevWeapon
        {721, {}, std::nullopt},               // DropWeapon
        {722, {}, InputAction::Use},           // Open
        {723, {}, InputAction::PrevItem},      // InventoryLeft
        {724, {}, InputAction::NextItem},      // InventoryRight
        {725, {}, InputAction::UseItem},       // UseInventory
        {726, {}, std::nullopt},               // Grab
        {727, {}, std::nullopt},               // Message
        {728, {}, std::nullopt},               // Taunt
        {729, {}, InputAction::LookUp},        // LookUp
        {730, {}, InputAction::LookDown},      // LookDown
        {731, {}, std::nullopt},               // MouseAimToggle
        {732, {}, std::nullopt},               // Crosshair
        {735, {}, InputAction::ProximityBomb}, // Proximities
        {736, {}, InputAction::RemoteBomb},    // Remotes
        {737, {}, InputAction::TimeBomb},      // Time
        {738, {}, InputAction::Detonate},      // Detonate
        {739, {}, InputAction::WeaponSlot1},   // Weapon_0 (defaults.cfg: key 1)
        {740, {}, InputAction::WeaponSlot2},
        {741, {}, InputAction::WeaponSlot3},
        {742, {}, InputAction::WeaponSlot4},
        {743, {}, InputAction::WeaponSlot5},
        {744, {}, InputAction::WeaponSlot6},
        {745, {}, InputAction::WeaponSlot7},
        {746, {}, InputAction::WeaponSlot8},
        {747, {}, InputAction::WeaponSlot9},
        {748, {}, InputAction::WeaponSlot10}, // Weapon_9 (key 0)
        // Replacement rows: actions of the action layer that the original menu did not list.
        {0, "Run Latch", InputAction::RunLatch},
        {0, "Flashlight", InputAction::Flashlight},
        {0, "Medkit", InputAction::Medkit},
        {0, "Binoculars", InputAction::Binoculars},
        {0, "Night Goggles", InputAction::NightGoggles},
        {0, "The Eye", InputAction::TheEye},
    }};

    // Actions the Controls page rebinds; menu/system and analog look bindings stay fixed.
    inline bool rebindable(InputAction action)
    {
        for (const auto& row : control_rows)
            if (row.action == action)
                return true;
        return false;
    }

    // Original device classes: CMenuControls keeps one control per device and action.
    enum class ControlDevice : std::uint8_t
    {
        Keyboard,
        Mouse,
        Gamepad
    };
    inline ControlDevice control_device(InputSource source)
    {
        switch (source)
        {
            case InputSource::Key:
                return ControlDevice::Keyboard;
            case InputSource::MouseButton:
            case InputSource::MouseWheel:
                return ControlDevice::Mouse;
            default:
                return ControlDevice::Gamepad;
        }
    }

    // CMenuControls::HandleDeviceTrack: the control leaves every other rebindable action, the
    // action loses its control of the same device, then the control is bound. Refused sources
    // (invalid or reserved) change nothing.
    inline bool assign_control(InputBindings& bindings, InputAction action, InputSource source, int code)
    {
        if (!rebindable(action) || !InputBindings::valid({source, code, action}))
            return false;
        const auto device = control_device(source);
        std::erase_if(bindings.entries,
                      [&](const InputBinding& b)
                      {
                          return rebindable(b.action) && ((b.source == source && b.code == code) ||
                                                          (b.action == action && control_device(b.source) == device));
                      });
        return bindings.add(source, code, action);
    }
    // CMenuControls::HandleKeyDown(VK_DELETE): unassign the row for every device.
    inline void clear_control(InputBindings& bindings, InputAction action)
    {
        if (rebindable(action))
            bindings.clear(action);
    }
    // Replacement RESTORE DEFAULTS row: rebindable actions return to InputBindings::defaults().
    inline void restore_controls(InputBindings& bindings)
    {
        std::erase_if(bindings.entries, [](const InputBinding& b) { return rebindable(b.action); });
        for (const auto& b : InputBindings::defaults().entries)
            if (rebindable(b.action))
                bindings.add(b.source, b.code, b.action);
    }

    // Display names. Keys follow the current keyboard layout (the original
    // ConvertKeyToCurrentKeyboard); mouse buttons keep the original "MBUTTON n"; gamepad controls
    // replace the original "J" + DirectInput name with "PAD" + the console-layout name.
    inline std::string ascii_upper(std::string_view text)
    {
        std::string result;
        for (unsigned char c : text)
        {
            if (c < 32 || c > 126)
                return {};
            result += char(c >= 'a' && c <= 'z' ? c - 32 : c);
        }
        return result;
    }
    inline std::string control_name(InputSource source, int code)
    {
        static constexpr std::array<const char*, SDL_GAMEPAD_BUTTON_COUNT> buttons{
            "A",  "B",  "X",        "Y",     "BACK",  "GUIDE", "START", "LS",   "RS",
            "LB", "RB", "UP",       "DOWN",  "LEFT",  "RIGHT", "MISC",  "P1",   "P2",
            "P3", "P4", "TOUCHPAD", "MISC2", "MISC3", "MISC4", "MISC5", "MISC6"};
        // Negative then positive direction of each axis (SDL: up and left are negative).
        static constexpr std::array<std::array<const char*, 2>, SDL_GAMEPAD_AXIS_COUNT> axes{{{"LS LEFT", "LS RIGHT"},
                                                                                              {"LS UP", "LS DOWN"},
                                                                                              {"RS LEFT", "RS RIGHT"},
                                                                                              {"RS UP", "RS DOWN"},
                                                                                              {"LT-", "LT"},
                                                                                              {"RT-", "RT"}}};
        switch (source)
        {
            case InputSource::Key:
            {
                auto name =
                    ascii_upper(SDL_GetKeyName(SDL_GetKeyFromScancode(SDL_Scancode(code), SDL_KMOD_NONE, false)));
                if (name.empty())
                    name = ascii_upper(SDL_GetScancodeName(SDL_Scancode(code)));
                return name.empty() ? "KEY " + std::to_string(code) : name;
            }
            case InputSource::MouseButton:
                // DirectInput order (left, right, middle, X1, X2), one-based like the original.
                return "MBUTTON " + std::to_string(code == SDL_BUTTON_RIGHT ? 2 : code == SDL_BUTTON_MIDDLE ? 3 : code);
            case InputSource::MouseWheel:
                return code ? "MWHEEL DOWN" : "MWHEEL UP";
            case InputSource::GamepadButton:
                return std::string("PAD ") + (unsigned(code) < buttons.size() ? buttons[code] : "?");
            case InputSource::GamepadAxisPositive:
            case InputSource::GamepadAxisNegative:
                return std::string("PAD ") +
                       (unsigned(code) < axes.size() ? axes[code][source == InputSource::GamepadAxisPositive] : "?");
        }
        return {};
    }
    // CMenuControls::SetControlText: keyboard, mouse then gamepad controls joined by " or ".
    inline std::string control_text(const InputBindings& bindings, InputAction action)
    {
        std::string text;
        for (const auto device : {ControlDevice::Keyboard, ControlDevice::Mouse, ControlDevice::Gamepad})
            for (const auto& b : bindings.entries)
                if (b.action == action && control_device(b.source) == device)
                    text += (text.empty() ? "" : " or ") + control_name(b.source, b.code);
        return text;
    }
    inline std::vector<MenuControlRow> control_menu_rows(const InputBindings& bindings)
    {
        std::vector<MenuControlRow> rows;
        for (const auto& row : control_rows)
            rows.push_back({row.string_id, std::string(row.literal),
                            row.action ? control_text(bindings, *row.action) : std::string{}, row.action.has_value()});
        return rows;
    }

    // controls.cfg beside preferences.cfg: "version=1" then one "bind=<action> <source> <code>"
    // line per rebindable binding. Loading starts from the defaults, replaces every rebindable
    // action with the file's lines (an absent action is unassigned) and changes nothing on error.
    inline constexpr std::array<const char*, 6> control_source_names{"key",   "mouse", "button",
                                                                     "wheel", "axis+", "axis-"};
    inline bool controls_path(std::filesystem::path& output, std::string& error)
    {
        auto* path = SDL_GetPrefPath("OpenBlood2", "Blood2");
        if (!path)
        {
            error = SDL_GetError();
            return false;
        }
        output = path_from_utf8(path) / "controls.cfg";
        SDL_free(path);
        return true;
    }
    inline bool load_controls(const std::filesystem::path& path, InputBindings& output, std::string& error)
    {
        error.clear();
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (ec)
        {
            error = "Cannot inspect controls: " + ec.message();
            return false;
        }
        if (!exists)
            return true;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size > 16 * 1024)
        {
            error = "Cannot read controls (16 KiB limit)";
            return false;
        }
        std::ifstream input(path);
        if (!input)
        {
            error = "Cannot open controls";
            return false;
        }
        auto result = output;
        std::erase_if(result.entries, [](const InputBinding& b) { return rebindable(b.action); });
        bool version = false;
        std::string line;
        while (std::getline(input, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty() || line.front() == '#')
                continue;
            if (line == "version=1" && !version)
            {
                version = true;
                continue;
            }
            const auto fail = [&](const char* why)
            {
                error = std::string("Invalid controls: ") + why;
                return false;
            };
            if (!version || !line.starts_with("bind="))
                return fail("expected version=1 then bind lines");
            const std::string_view rest = std::string_view(line).substr(5);
            const auto a = rest.find(' '), b = a == rest.npos ? rest.npos : rest.find(' ', a + 1);
            if (b == rest.npos)
                return fail("malformed bind line");
            const auto name = rest.substr(0, a), source = rest.substr(a + 1, b - a - 1), number = rest.substr(b + 1);
            std::size_t action = 0, kind = 0;
            while (action < input_action_count && input_action_info[action].name != name)
                ++action;
            while (kind < control_source_names.size() && control_source_names[kind] != source)
                ++kind;
            int code = 0;
            const auto parsed = std::from_chars(number.data(), number.data() + number.size(), code);
            if (action == input_action_count || !rebindable(InputAction(action)) ||
                kind == control_source_names.size() || parsed.ec != std::errc{} ||
                parsed.ptr != number.data() + number.size())
                return fail("unknown action, source or code");
            if (!result.add(InputSource(kind), code, InputAction(action)))
                return fail("refused binding");
        }
        if (input.bad() || !version)
        {
            error = "Incomplete controls";
            return false;
        }
        output = std::move(result);
        return true;
    }
    inline bool save_controls(const std::filesystem::path& path, const InputBindings& bindings, std::string& error)
    {
        error.clear();
        std::string text = "version=1\n";
        for (const auto& b : bindings.entries)
            if (rebindable(b.action))
                text += std::string("bind=") + std::string(info(b.action).name) + ' ' +
                        control_source_names[std::size_t(b.source)] + ' ' + std::to_string(b.code) + '\n';
        return write_file_atomic(path, std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()),
                                 "controls", error);
    }
} // namespace OpenBlood2
