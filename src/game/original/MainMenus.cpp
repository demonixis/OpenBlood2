// Selective original menu construction/navigation from ClientShellDLL/Menu*.cpp.
// See Documentation/implementation/sources/original_menu_sources.json for provenance.
#include "MenuIds.h"
#include "game/menu.h"
#include <algorithm>
#include <array>
#include <utility>
namespace OpenBlood2
{
    namespace
    {
        // Replacement commands without original counterparts: the Controls page's last row and
        // the Options page's VR row.
        constexpr int MENU_CMD_RESTORE_CONTROLS = -2, MENU_CMD_VR = -3;
        // VR turn steps in the order the row cycles them; 0 turns smoothly.
        constexpr std::array<int, 6> vr_turns{15, 30, 45, 60, 90, 0};
    } // namespace
    void Menu::AddLargeTextItemOption(unsigned id, int command)
    {
        items.push_back({id, command, true, {}, -1, {}});
    }
    void Menu::setting(unsigned id, int value)
    {
        items.push_back({id, -1, true, {}, value, {}});
    }
    void Menu::cue(MenuCue value)
    {
        if (cues.size() < 16)
            cues.push_back(value);
    }
    void Menu::show_promotion(MenuPromotion screen, bool exit)
    {
        if (!demo() || screen == MenuPromotion::None)
            return;
        promotion = screen;
        promotion_exit = exit;
        cancel_pointer();
        ++revision;
    }
    void Menu::dismiss_promotion()
    {
        if (promotion == MenuPromotion::None)
            return;
        if (promotion_exit)
        {
            pending_.action = MenuAction::Quit;
            promotion = MenuPromotion::None;
            promotion_exit = false;
            cancel_pointer();
            ++revision;
        }
        else
            build(MenuPage::Main); // Original OnEnterMenuState(GS_SPLASH) always returns to the root.
    }
    void Menu::end_demo()
    {
        if (!demo())
            return;
        open(false);
        cue(MenuCue::Enter); // Original EndGame: menu, enter01.wav, info splash.
        show_promotion(MenuPromotion::Info);
    }
    void Menu::build(MenuPage next)
    {
        // These retail pages have no demo entries or required resources.
        if (demo() &&
            (next == MenuPage::Character || next == MenuPage::Load || next == MenuPage::Save || next == MenuPage::Quit))
            next = MenuPage::Main;
        page = next;
        promotion = MenuPromotion::None;
        promotion_exit = false;
        items.clear();
        selected = 0;
        first = 0;
        binding = -1;
        pressed_ = -1;
        ++revision;
        switch (page)
        {
            case MenuPage::Main:
                AddLargeTextItemOption(IDS_MENU_MAIN_SINGLEPLAYER, MENU_CMD_SINGLE_PLAYER);
                AddLargeTextItemOption(IDS_MENU_MAIN_BLOODBATH, MENU_CMD_BLOODBATH);
                AddLargeTextItemOption(IDS_MENU_MAIN_OPTIONS, MENU_CMD_OPTIONS);
                AddLargeTextItemOption(demo() ? IDS_MENU_MAIN_PRIMEGIBS : IDS_MENU_MAIN_CREDITS, MENU_CMD_CREDITS);
                AddLargeTextItemOption(IDS_MENU_MAIN_QUIT, MENU_CMD_QUIT);
                break;
            case MenuPage::Solo:
                AddLargeTextItemOption(IDS_MENU_SINGLEPLAYER_CALEB, MENU_CMD_START_STORY);
                AddLargeTextItemOption(IDS_MENU_SINGLEPLAYER_CHOSEN, MENU_CMD_START_ACTION);
                if (demo())
                    break; // Original _DEMO omits load/save/custom, not disabled retail rows.
                AddLargeTextItemOption(IDS_MENU_SINGLEPLAYER_LOADGAME, MENU_CMD_LOAD_GAME_MENU);
                AddLargeTextItemOption(IDS_MENU_SINGLEPLAYER_SAVEGAME, MENU_CMD_SAVE_GAME_MENU);
                AddLargeTextItemOption(IDS_MENU_SINGLEPLAYER_CUSTOMLEVEL, MENU_CMD_CUSTOM_LEVEL);
                break;
            case MenuPage::Character:
                AddLargeTextItemOption(IDS_MENU_CHARSELECT_CALEB, MENU_CMD_START_CALEB);
                AddLargeTextItemOption(IDS_MENU_CHARSELECT_GABRIELLA, MENU_CMD_START_GABRIELLA);
                AddLargeTextItemOption(IDS_MENU_CHARSELECT_ISHMAEL, MENU_CMD_START_ISHMAEL);
                AddLargeTextItemOption(IDS_MENU_CHARSELECT_OPHELIA, MENU_CMD_START_OPHELIA);
                break;
            case MenuPage::Difficulty:
                AddLargeTextItemOption(IDS_MENU_DIFFICULTY_GENOCIDE, MENU_CMD_DIFFICULTY_EASY);
                AddLargeTextItemOption(IDS_MENU_DIFFICULTY_HOMICIDE, MENU_CMD_DIFFICULTY_MEDIUM);
                AddLargeTextItemOption(IDS_MENU_DIFFICULTY_SUICIDE, MENU_CMD_DIFFICULTY_HARD);
                selected = 1;
                break;
            case MenuPage::Options:
                AddLargeTextItemOption(IDS_MENU_OPTIONS_DISPLAY, MENU_CMD_DISPLAY);
                AddLargeTextItemOption(IDS_MENU_OPTIONS_AUDIO, MENU_CMD_AUDIO);
                AddLargeTextItemOption(IDS_MENU_OPTIONS_CONTROLS, MENU_CMD_CUSTOMIZE_CONTROLS);
                AddLargeTextItemOption(IDS_MENU_OPTIONS_MOUSE, MENU_CMD_MOUSE);
                AddLargeTextItemOption(IDS_MENU_OPTIONS_JOYSTICK, MENU_CMD_JOYSTICK);
                AddLargeTextItemOption(IDS_MENU_OPTIONS_KEYBOARD, MENU_CMD_KEYBOARD);
                if (vr)
                    items.push_back({0, MENU_CMD_VR, true, "VR", -1, {}});
                break;
            case MenuPage::Audio:
                setting(IDS_MENU_AUDIO_SOUND, 0);
                setting(IDS_MENU_AUDIO_SOUND_VOLUME, 1);
                AddLargeTextItemOption(IDS_MENU_AUDIO_16BIT_SOUND, -1);
                setting(IDS_MENU_AUDIO_MUSIC, 2);
                setting(IDS_MENU_AUDIO_MUSIC_VOLUME, 3);
                break;
            case MenuPage::Display:
                // Source CMenuDisplay offers renderer, resolution and DETAIL LEVEL; SDL owns the first
                // two. The renderer, lighting and normal-map rows are replacement options without
                // original strings.
                setting(IDS_MENU_DISPLAY_DETAIL_LOW + unsigned(std::clamp(settings.detail, 0, 2)), 6);
                items.push_back({0, -1, true, {}, 8, {}});
                items.push_back({0, -1, true, {}, 7, {}});
                items.push_back({0, -1, true, {}, 9, {}});
                break;
            case MenuPage::Vr:
                // Replacement rows, labelled like the Display page's replacement rows.
                items.push_back({0, -1, true, {}, 10, {}});
                items.push_back({0, -1, true, {}, 11, {}});
                items.push_back({0, -1, true, {}, 12, {}});
                break;
            case MenuPage::Mouse:
                setting(IDS_MENU_MOUSE_SENSITIVITY, 4);
                AddLargeTextItemOption(IDS_MENU_MOUSE_INPUTRATE, -1);
                setting(IDS_MENU_MOUSE_INVERT_YAXIS, 5);
                if (demo())
                    break; // Build 108 constructs three mouse rows; later source adds more.
                AddLargeTextItemOption(IDS_MENU_MOUSE_MOUSELOOK, -1);
                AddLargeTextItemOption(IDS_MENU_MOUSE_LOOKSPRING, -1);
                AddLargeTextItemOption(IDS_MENU_MOUSE_USEWHEEL, -1);
                break;
            case MenuPage::Controls:
                // CMenuControls::InitColumns: one MENU_CMD_CHANGE_CONTROL row per action, small font,
                // no spacing, no wrap; the replacement RESTORE DEFAULTS row follows.
                for (const auto& row : controls)
                    items.push_back({row.string_id, int(MENU_CMD_CHANGE_CONTROL), row.enabled, row.literal, -1, {}});
                items.push_back({0, MENU_CMD_RESTORE_CONTROLS, true, "Restore Defaults", -1, {}});
                break;
            case MenuPage::Load:
                // CMenuLoadGame::InitSavedGameSlots: "RESTART: %s", "QUICKLOAD: %s", then the slots.
                items.push_back({IDS_MENU_LOADGAME_RESTART, int(MENU_CMD_LOAD_SAVE_GAME), saves.restart.loadable,
                                 {}, -1, {}, {}, saves.restart.name});
                items.push_back({IDS_MENU_LOADGAME_QUICKLOAD, int(MENU_CMD_LOAD_SAVE_GAME), saves.quick.loadable,
                                 {}, -1, {}, {}, saves.quick.name});
                for (const auto& slot : saves.slots)
                    items.push_back({0, int(MENU_CMD_LOAD_SAVE_GAME), slot.loadable, slot.name, -1, {}});
                break;
            case MenuPage::Save:
                // CMenuSaveGame::InitSavedGameSlots: the eight slot names; any slot may be overwritten.
                for (const auto& slot : saves.slots)
                    items.push_back({0, int(MENU_CMD_SAVE_GAME), in_world && alive && saves.unavailable.empty(),
                                     slot.name, -1, {}});
                break;
            case MenuPage::Quit:
                items.push_back({0, int(MENU_CMD_EXIT), true, "Yes", -1, {}});
                items.push_back({0, int(MENU_CMD_KILL_MESSAGEBOX), true, "No", -1, {}});
                selected = 1;
                break;
        }
        for (auto& item : items)
        {
            if (item.setting >= 0)
                continue;
            switch (item.command)
            {
                case MENU_CMD_BLOODBATH:
                    item.enabled = demo();
                    break;
                case MENU_CMD_CUSTOMIZE_CONTROLS:
                    item.enabled = !demo() && !controls.empty(); // The demo has no action strings.
                    break;
                case -1:
                case MENU_CMD_CUSTOM_LEVEL:
                case MENU_CMD_JOYSTICK:
                case MENU_CMD_KEYBOARD:
                    item.enabled = false;
                    break;
                case MENU_CMD_SAVE_GAME_MENU:
                    item.enabled = in_world && alive;
                    break;
                default:
                    break;
            }
        }
        if (!items.empty() && !enabled(items[selected]))
            for (unsigned i = 0; i < items.size(); ++i)
                if (enabled(items[i]))
                {
                    selected = int(i);
                    break;
                }
    }
    void Menu::open(bool world, bool living)
    {
        in_world = world;
        alive = living;
        active = true;
        pending_ = {};
        error.clear();
        top = MenuPage::Main;
        build(MenuPage::Main);
    }
    void Menu::open_load(bool world, bool living)
    {
        open(world, living);
        if (demo())
            return; // Original _DEMO opens the main menu.
        top = MenuPage::Load;
        build(MenuPage::Load);
    }
    void Menu::set_saves(const MenuSaveSlots& next)
    {
        if (saves == next)
            return;
        saves = next;
        if (!active || (page != MenuPage::Save && page != MenuPage::Load))
            return;
        const int keep = selected;
        build(page);
        if (keep >= 0 && keep < int(items.size()) && enabled(items[keep]))
            selected = keep;
    }
    void Menu::set_controls(std::vector<MenuControlRow> next)
    {
        if (controls == next)
            return;
        controls = std::move(next);
        if (!active || (page != MenuPage::Controls && page != MenuPage::Options))
            return;
        const int keep = selected, top_row = first;
        build(page);
        if (keep >= 0 && keep < int(items.size()) && enabled(items[keep]))
        {
            selected = keep;
            first = top_row;
        }
    }
    void Menu::close()
    {
        active = false;
        binding = -1;
        promotion = MenuPromotion::None;
        promotion_exit = false;
        cancel_pointer();
        ++revision;
    }
    MenuCommand Menu::take_command()
    {
        return std::exchange(pending_, {});
    }
    void Menu::back()
    {
        if (!active)
            return;
        if (promotion != MenuPromotion::None)
        {
            dismiss_promotion();
            return;
        }
        cue(MenuCue::Back);
        if (binding >= 0)
        {
            // CMenuControls::HandleDeviceTrack: Escape cancels the binding and removes the "=".
            binding = -1;
            ++revision;
            return;
        }
        if (page == top)
        {
            // Original VK_ESCAPE on the top-level menu: MenuReturnToGame only in a world.
            if (in_world)
            {
                pending_.action = MenuAction::Resume;
                close();
            }
            return;
        }
        switch (page)
        {
            case MenuPage::Solo:
            case MenuPage::Options:
            case MenuPage::Quit:
                build(MenuPage::Main);
                break;
            case MenuPage::Difficulty:
                build(story ? MenuPage::Solo : MenuPage::Character);
                break;
            case MenuPage::Audio:
            case MenuPage::Mouse:
            case MenuPage::Display:
            case MenuPage::Controls:
            case MenuPage::Vr:
                build(MenuPage::Options);
                break;
            default:
                build(MenuPage::Solo);
                break;
        }
    }
    void Menu::select(int direction)
    {
        if (!active || promotion != MenuPromotion::None || items.empty() || binding >= 0)
            return;
        int next = selected;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            // CMenuControls::Build: SetScrollWrap(DFALSE) stops at either end.
            if (page == MenuPage::Controls && (direction < 0 ? next == 0 : next + 1 == int(items.size())))
                return;
            next = (next + int(items.size()) + (direction < 0 ? -1 : 1)) % int(items.size());
            if (enabled(items[next]))
            {
                if (selected != next)
                {
                    selected = next;
                    cue(MenuCue::Select);
                    ++revision;
                }
                return;
            }
        }
    }
    void Menu::activate()
    {
        if (active && promotion != MenuPromotion::None)
        {
            dismiss_promotion();
            return;
        }
        if (!active || promotion != MenuPromotion::None || selected < 0 || selected >= int(items.size()) ||
            !enabled(items[selected]) || binding >= 0 || pending_.action == MenuAction::NewGame ||
            pending_.action == MenuAction::Credits || pending_.action == MenuAction::Quit ||
            pending_.action == MenuAction::SaveGame || pending_.action == MenuAction::LoadGame)
            return;
        if (items[selected].setting >= 0)
        {
            adjust(1);
            return;
        }
        cue(MenuCue::Enter);
        const auto command = items[selected].command;
        switch (command)
        {
            case MENU_CMD_SINGLE_PLAYER:
                build(MenuPage::Solo);
                break;
            case MENU_CMD_OPTIONS:
                build(MenuPage::Options);
                break;
            case MENU_CMD_START_STORY:
                story = true;
                character = 0;
                build(MenuPage::Difficulty);
                break;
            case MENU_CMD_BLOODBATH:
                if (demo())
                    show_promotion(MenuPromotion::Bloodbath);
                break;
            case MENU_CMD_CREDITS:
                if (demo())
                    show_promotion(MenuPromotion::Info);
                else
                    pending_.action = MenuAction::Credits; // Owner: credits world, or text credits in a world.
                break;
            case MENU_CMD_START_ACTION:
                if (demo())
                {
                    show_promotion(MenuPromotion::Modes);
                    break;
                }
                story = false;
                build(MenuPage::Character);
                break;
            case MENU_CMD_START_CALEB:
                character = 0;
                build(MenuPage::Difficulty);
                break;
            case MENU_CMD_START_GABRIELLA:
                character = 3;
                build(MenuPage::Difficulty);
                break;
            case MENU_CMD_START_ISHMAEL:
                character = 2;
                build(MenuPage::Difficulty);
                break;
            case MENU_CMD_START_OPHELIA:
                character = 1;
                build(MenuPage::Difficulty);
                break;
            case MENU_CMD_DIFFICULTY_EASY:
            case MENU_CMD_DIFFICULTY_MEDIUM:
            case MENU_CMD_DIFFICULTY_HARD:
                pending_ = {MenuAction::NewGame, character, unsigned(command - MENU_CMD_DIFFICULTY_EASY), story};
                break;
            case MENU_CMD_AUDIO:
                build(MenuPage::Audio);
                break;
            case MENU_CMD_MOUSE:
                build(MenuPage::Mouse);
                break;
            case MENU_CMD_DISPLAY:
                build(MenuPage::Display);
                break;
            case MENU_CMD_SAVE_GAME_MENU:
                build(MenuPage::Save);
                break;
            case MENU_CMD_LOAD_GAME_MENU:
                build(MenuPage::Load);
                break;
            case MENU_CMD_SAVE_GAME: // CMenuSaveGame::OnCommand: MenuSaveGame(row).
                pending_.action = MenuAction::SaveGame;
                pending_.slot = selected;
                break;
            case MENU_CMD_LOAD_SAVE_GAME: // CMenuLoadGame::OnCommand: Restart, Quickload, then row - 2.
                pending_.action = MenuAction::LoadGame;
                pending_.slot = selected == 0 ? -2 : selected == 1 ? -1 : selected - 2;
                break;
            case MENU_CMD_QUIT:
                if (demo())
                    show_promotion(MenuPromotion::Info, true);
                else
                    build(MenuPage::Quit);
                break;
            case MENU_CMD_EXIT:
                pending_.action = MenuAction::Quit;
                break;
            case MENU_CMD_KILL_MESSAGEBOX:
                build(MenuPage::Main);
                break;
            case MENU_CMD_CUSTOMIZE_CONTROLS:
                build(MenuPage::Controls);
                break;
            case MENU_CMD_CHANGE_CONTROL: // CMenuControls::OnCommand: "=" and StartTrackingDevices.
                binding = selected;
                ++revision;
                break;
            case MENU_CMD_RESTORE_CONTROLS:
                pending_.action = MenuAction::RestoreControls;
                break;
            case MENU_CMD_VR:
                build(MenuPage::Vr);
                break;
            default:
                break;
        }
    }
    void Menu::adjust(int direction)
    {
        if (!active || promotion != MenuPromotion::None || selected < 0 || selected >= int(items.size()) ||
            !enabled(items[selected]) || binding >= 0)
            return;
        auto before = settings;
        switch (items[selected].setting)
        {
            case 0:
                settings.sound = !settings.sound;
                break;
            case 1:
                if (settings.sound)
                    settings.sound_volume = std::clamp(settings.sound_volume + direction * 5, 0, 50);
                break;
            case 2:
                settings.music = !settings.music;
                break;
            case 3:
                if (settings.music)
                    settings.music_volume = std::clamp(settings.music_volume + direction * 5, 0, 50);
                break;
            case 4:
                settings.sensitivity = std::clamp(settings.sensitivity + direction * 20, 100, 1500);
                break;
            case 5:
                settings.invert_y = !settings.invert_y;
                break;
            case 6:
                settings.detail = (std::clamp(settings.detail, 0, 2) + (direction < 0 ? 2 : 1)) % 3;
                items[selected].string_id = IDS_MENU_DISPLAY_DETAIL_LOW + unsigned(settings.detail);
                items[selected].literal.clear(); // Recomposed from the new original string.
                break;
            case 7: // The lighting profile of the selected renderer.
                if (settings.renderer)
                    settings.quality = !settings.quality;
                else
                    settings.pixel_lighting = !settings.pixel_lighting;
                break;
            case 8:
                settings.renderer = !settings.renderer;
                break;
            case 9:
                settings.normal_maps = !settings.normal_maps;
                break;
            case 10:
            {
                const auto at = std::find(vr_turns.begin(), vr_turns.end(), settings.vr_turn);
                const int index = at == vr_turns.end() ? 2 : int(at - vr_turns.begin());
                const int count = int(vr_turns.size());
                settings.vr_turn = vr_turns[std::size_t((index + (direction < 0 ? count - 1 : 1)) % count)];
                break;
            }
            case 11:
                settings.vr_laser = !settings.vr_laser;
                break;
            case 12:
                settings.vr_crosshair = !settings.vr_crosshair;
                break;
            default:
                return;
        }
        if (before != settings)
        {
            pending_.action = MenuAction::Settings;
            cue(MenuCue::Select);
            ++revision;
        }
    }
    void Menu::pointer(float x, float y, bool press, bool release, bool motion)
    {
        if (!active || promotion != MenuPromotion::None || binding >= 0)
            return;
        int hit = -1;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            auto r = items[i].rect;
            if (enabled(items[i]) && x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            {
                hit = int(i);
                break;
            }
        }
        if (motion && hit >= 0 && selected != hit)
        {
            selected = hit;
            cue(MenuCue::Select);
            ++revision;
        }
        if (press)
            pressed_ = hit;
        if (release)
        {
            const int pressed = std::exchange(pressed_, -1);
            if (hit >= 0 && pressed == hit)
            {
                selected = hit;
                const auto& item = items[hit];
                const auto r = item.slider_rect;
                if (r.right > r.left && x >= 280 && (item.setting != 1 || settings.sound) &&
                    (item.setting != 3 || settings.music))
                {
                    const int low = item.setting == 4 ? 100 : 0, high = item.setting == 4 ? 1500 : 50;
                    const int step = item.setting == 4 ? 20 : 5;
                    const float ratio = std::clamp((x - r.left) / float(r.right - r.left), 0.f, 1.f);
                    const int value = std::clamp(low + int((ratio * (high - low) + step / 2) / step) * step, low, high);
                    int& target = item.setting == 1   ? settings.sound_volume
                                  : item.setting == 3 ? settings.music_volume
                                                      : settings.sensitivity;
                    if (target != value)
                    {
                        target = value;
                        pending_.action = MenuAction::Settings;
                        cue(MenuCue::Select);
                        ++revision;
                    }
                }
                else
                    activate();
            }
        }
    }
    bool Menu::small() const
    {
        return page == MenuPage::Audio || page == MenuPage::Mouse || page == MenuPage::Display ||
               page == MenuPage::Controls || page == MenuPage::Vr || page == MenuPage::Save || page == MenuPage::Load ||
               page == MenuPage::Quit;
    }
    const char* Menu::title() const
    {
        switch (page)
        {
            case MenuPage::Options:
            case MenuPage::Audio:
            case MenuPage::Mouse:
            case MenuPage::Display:
            case MenuPage::Controls:
            case MenuPage::Vr:
                return "options.pcx";
            case MenuPage::Solo:
            case MenuPage::Character:
            case MenuPage::Difficulty:
                return "singleplayer.pcx";
            case MenuPage::Save:
                return "savegame.pcx";
            case MenuPage::Load:
                return "loadgame.pcx";
            default:
                return "mainmenu.pcx";
        }
    }
} // namespace OpenBlood2
