#pragma once
#include "app/control_bindings.h"
#include "app/demo_flow.h"
#include "app/message_input.h"
#include "app/movement_controls.h"
#include "app/touch_controls.h"
#include "game/menu.h"
#include "game/original/Credits.h"
#include <vector>
namespace OpenBlood2
{
    // Normal interactive input. Smoke tests deliberately retain their original adapter.
    struct MenuInput
    {
        MovementControls controls;
        MessageInput message;
        TouchControls touch; // On-screen virtual gamepad for direct touch screens.
        bool inspector = false;
        bool demo_mode = false;
        DemoFlow demo;
        // Original GS_CREDITS: the in-world text-only credits screen pauses play.
        bool credits_screen = false;
        // Owner-set CBloodClientShell::m_bRollCredits: key downs also reach the roll.
        bool roll_credits = false;
        bool credits_exit = false;
        // Original GS_LOADINGLEVEL/GS_WAITING: the loading screen pauses play; while waiting, the
        // next fresh key continues (other devices: any button or tap).
        bool loading_screen = false, loading_wait = false, loading_continue = false;
        // Original startup title.pcx splash (Splash_Display, then TimedKeyWait on Esc, Return or Space).
        bool title_screen = false;
        // Options > SETUP: the replacement setup screen over the paused menu owns all input.
        bool setup_screen = false;
        // Original F5/F6 (OnKeyDown, retail only): owned requests consumed at the frame boundary.
        bool quick_save = false, quick_load = false;
        std::vector<int> credit_keys; // Original VK codes for CCredits::HandleInput.
        static int credits_vk(SDL_Scancode code)
        {
            switch (code)
            {
                case SDL_SCANCODE_UP:
                    return CREDITS_VK_UP;
                case SDL_SCANCODE_DOWN:
                    return CREDITS_VK_DOWN;
                case SDL_SCANCODE_HOME:
                    return CREDITS_VK_HOME;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                    return CREDITS_VK_RETURN;
                case SDL_SCANCODE_SPACE:
                    return CREDITS_VK_SPACE;
                case SDL_SCANCODE_PAGEDOWN:
                    return CREDITS_VK_NEXT;
                case SDL_SCANCODE_PAUSE:
                    return CREDITS_VK_PAUSE;
                case SDL_SCANCODE_ESCAPE:
                    return CREDITS_VK_ESCAPE;
                default:
                    return 0;
            }
        }
        bool paused(const Menu& menu) const
        {
            return demo_mode ? demo.paused()
                             : menu.active || credits_screen || loading_screen || title_screen || setup_screen;
        }
        void reset(Menu& menu, bool player)
        {
            message.cancel();
            message.submitted.reset();
            // Ownership changes release the on-screen controls without edges.
            touch.release();
            std::erase_if(controls.gamepads, [](const GamepadState& pad) { return pad.id == touch_gamepad_id; });
            if (demo_mode)
            {
                demo.reset(controls, inspector, player);
                return;
            }
            menu.cancel_pointer();
            if (menu.binding >= 0)
            {
                menu.binding = -1; // Ownership changes end device tracking.
                ++menu.revision;
            }
            quick_save = quick_load = false;
            controls.player_changed(player && !menu.active && !credits_screen && !loading_screen && !inspector);
        }
        // Fresh (non-repeated) down edge of an action in this event.
        bool pressed(const InputEdges& edges, InputAction action) const
        {
            for (const auto& edge : edges)
                if (edge.down && !edge.repeat && controls.bindings.matches(edge, action))
                    return true;
            return false;
        }
        // Down edge including key repeats (menu navigation).
        bool navigated(const InputEdges& edges, InputAction action) const
        {
            for (const auto& edge : edges)
                if (edge.down && controls.bindings.matches(edge, action))
                    return true;
            return false;
        }
        // Original promotion keys: Esc, Space or Enter; other devices use the menu actions.
        bool dismisses_promotion(const SDL_Event& e, const InputEdges& edges) const
        {
            return (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_SPACE) ||
                   pressed(edges, InputAction::Menu) || pressed(edges, InputAction::MenuAccept) ||
                   pressed(edges, InputAction::MenuBack);
        }
        // The Controls rows follow the current bindings.
        void refresh_controls(Menu& menu) const
        {
            menu.set_controls(control_menu_rows(controls.bindings));
        }
        // CMenuControls::HandleDeviceTrack while a row shows "=": the next fresh key, mouse
        // button, wheel tick or gamepad button/axis edge binds it. Escape (any Menu source)
        // cancels; Tab (as in the original) and the quick save/load keys are ignored, as are
        // reserved keys; touch-emulated clicks never bind.
        void track_control(const SDL_Event& e, SDL_WindowID window, const InputEdges& edges, Menu& menu)
        {
            if (((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
                 e.button.which == SDL_TOUCH_MOUSEID) ||
                (e.type == SDL_EVENT_MOUSE_WHEEL && e.wheel.windowID != window))
                return;
            const auto& bindings = controls.bindings;
            for (const auto& edge : edges)
            {
                if (!edge.down || edge.repeat)
                    continue;
                if (bindings.matches(edge, InputAction::Menu))
                {
                    menu.back();
                    return;
                }
                if ((edge.source == InputSource::Key && edge.code == SDL_SCANCODE_TAB) ||
                    bindings.matches(edge, InputAction::QuickSave) || bindings.matches(edge, InputAction::QuickLoad))
                    continue;
                const auto row = std::size_t(menu.binding);
                if (row < control_rows.size() && control_rows[row].action &&
                    assign_control(controls.bindings, *control_rows[row].action, edge.source, edge.code))
                {
                    menu.binding = -1;
                    ++menu.revision;
                    refresh_controls(menu);
                    return;
                }
            }
        }
        // True means the caller should forward this event to ImGui's inspector.
        bool event(const SDL_Event& e, SDL_WindowID window, Menu& menu, bool player, bool alive,
                   bool inspector_available, float width, float height)
        {
            // Any other device hides the on-screen controls until the next touch.
            if (e.type == SDL_EVENT_KEY_DOWN ||
                (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.which != SDL_TOUCH_MOUSEID) ||
                (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && e.gbutton.which != touch_gamepad_id))
                touch.visible = false;
            // Direct-touch fingers on the on-screen controls become virtual gamepad events, which
            // take the ordinary gamepad path below; other fingers keep the camera drag.
            if ((e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_MOTION ||
                 e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED) &&
                e.tfinger.windowID == window && e.tfinger.touchID != SDL_MOUSE_TOUCHID &&
                controls.touch_screen(e.tfinger.touchID) && height > 0)
            {
                const bool play = player && controls.focused && controls.enabled && !paused(menu) && !inspector;
                std::vector<SDL_Event> synthetic;
                if (touch.event(e, width / height, play, synthetic))
                {
                    for (const auto& s : synthetic)
                        event(s, window, menu, player, alive, inspector_available, width, height);
                    return false;
                }
            }
            if (demo_mode)
                return demo.event(e, window, controls, inspector, inspector_available, player, alive);
            if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && e.key.windowID != window)
                return false;
            if ((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
                e.button.windowID != window)
                return false;
            if (e.type == SDL_EVENT_TEXT_INPUT && e.text.windowID != window)
                return false;
            if (e.type == SDL_EVENT_MOUSE_MOTION && e.motion.windowID != window)
                return false;
            if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST && e.window.windowID == window)
            {
                controls.event(e, window);
                menu.cancel_pointer();
                if (player && !menu.active && !credits_screen && !loading_screen && !inspector)
                    menu.open(true, alive);
                reset(menu, player);
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
                    controls.track(e); // Removal releases its sources even while unfocused.
                return false;
            }
            // Gamepad state changes here, before any early return, so a menu or the
            // inspector never leaves stale axes or buttons behind.
            const auto edges = controls.track(e);
            const auto press = [&](InputAction action) { return pressed(edges, action); };
            const bool button = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.windowID == window) ||
                                e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            if (title_screen)
            {
                if ((e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat &&
                     (e.key.scancode == SDL_SCANCODE_ESCAPE || e.key.scancode == SDL_SCANCODE_RETURN ||
                      e.key.scancode == SDL_SCANCODE_SPACE)) ||
                    (e.type != SDL_EVENT_KEY_DOWN && button))
                {
                    title_screen = false;
                    reset(menu, player);
                }
                return false;
            }
            if (menu.promotion != MenuPromotion::None)
            {
                if (dismisses_promotion(e, edges))
                {
                    menu.dismiss_promotion();
                    reset(menu, player);
                }
                return false; // Promotions accept source keys only; never leak to the underlying menu/game.
            }
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_F10)
            {
                if (inspector_available)
                {
                    inspector = !inspector;
                    reset(menu, player);
                }
                return false;
            }
            if (inspector)
                return true;
            if (loading_screen)
            {
                loading_continue |= loading_wait && ((e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) || button);
                return false;
            }
            if (menu.active && menu.page == MenuPage::Controls && menu.binding >= 0)
            {
                track_control(e, window, edges, menu);
                return false;
            }
            // Original OnKeyDown: the roll sees every key down (repeats included) before the
            // menu/game; GS_CREDITS consumes keys, and Esc leaves it.
            if (e.type == SDL_EVENT_KEY_DOWN && (roll_credits || credits_screen))
                if (const int vk = credits_vk(e.key.scancode))
                {
                    if (credits_screen && vk == CREDITS_VK_ESCAPE)
                        credits_exit = credits_exit || !e.key.repeat;
                    else
                        credit_keys.push_back(vk);
                }
            // Other devices drive the roll through the menu actions' original keys.
            if (gamepad && (roll_credits || credits_screen))
            {
                if (press(InputAction::Menu) || press(InputAction::MenuBack))
                {
                    if (credits_screen)
                        credits_exit = true;
                    else
                        credit_keys.push_back(CREDITS_VK_ESCAPE);
                }
                if (press(InputAction::MenuAccept))
                    credit_keys.push_back(CREDITS_VK_RETURN);
                if (navigated(edges, InputAction::MenuUp))
                    credit_keys.push_back(CREDITS_VK_UP);
                if (navigated(edges, InputAction::MenuDown))
                    credit_keys.push_back(CREDITS_VK_DOWN);
            }
            if (credits_screen)
                return false;
            if (message.active)
            {
                controls.event(e, window, true); // Track releases; no play commands from typed keys.
                if (gamepad && (press(InputAction::Menu) || press(InputAction::MenuBack)))
                    message.cancel();
                else
                    message.event(e);
                if (!message.active)
                    controls.player_changed(player);
                return false;
            }
            if (!menu.active && player && press(InputAction::Message))
            {
                reset(menu, false);
                message.open();
                return false;
            }
            if (press(InputAction::Menu) || (menu.active && press(InputAction::MenuBack)))
            {
                if (menu.active)
                    menu.back();
                else
                    menu.open(player, alive);
                reset(menu, player);
                return false;
            }
            if (menu.demo() && !menu.active && player && !alive && press(InputAction::MenuAccept))
            {
                demo.restart = true; // Shared owned retry hook; future checkpoint restore replaces the app handler.
                return false;
            }
            if (menu.active)
            {
                if (e.type == SDL_EVENT_KEY_DOWN && menu.page == MenuPage::Quit && !e.key.repeat &&
                    (e.key.scancode == SDL_SCANCODE_Y || e.key.scancode == SDL_SCANCODE_N))
                {
                    menu.selected = e.key.scancode == SDL_SCANCODE_Y ? 0 : 1;
                    menu.activate();
                    return false;
                }
                // CMenuControls VK_DELETE unassigns the row; Backspace too (labelled delete on Macs).
                if (e.type == SDL_EVENT_KEY_DOWN && menu.page == MenuPage::Controls &&
                    (e.key.scancode == SDL_SCANCODE_DELETE || e.key.scancode == SDL_SCANCODE_BACKSPACE) &&
                    std::size_t(menu.selected) < control_rows.size() && control_rows[menu.selected].action)
                {
                    clear_control(controls.bindings, *control_rows[menu.selected].action);
                    refresh_controls(menu);
                    return false;
                }
                // Navigation repeats while a key is held; activation needs a fresh press.
                if (navigated(edges, InputAction::MenuUp))
                    menu.select(-1);
                if (navigated(edges, InputAction::MenuDown))
                    menu.select(1);
                if (navigated(edges, InputAction::MenuLeft))
                    menu.adjust(-1);
                if (navigated(edges, InputAction::MenuRight))
                    menu.adjust(1);
                if (press(InputAction::MenuAccept))
                    menu.activate();
                if (e.type == SDL_EVENT_MOUSE_MOTION ||
                    ((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
                     e.button.button == SDL_BUTTON_LEFT))
                {
                    const bool motion = e.type == SDL_EVENT_MOUSE_MOTION;
                    float x = motion ? e.motion.x : e.button.x, y = motion ? e.motion.y : e.button.y;
                    if (!MenuViewport::fit(width, height).point(x, y))
                        x = y = -1;
                    menu.pointer(x, y, e.type == SDL_EVENT_MOUSE_BUTTON_DOWN, e.type == SDL_EVENT_MOUSE_BUTTON_UP,
                                 motion);
                }
                return false;
            }
            // F5 quicksaves without pausing when in the world and alive; F6 quickloads in play,
            // dead or alive. The single-player check belongs to the save itself (D12).
            const auto& bindings = controls.bindings;
            bool quick_save_key = false, quick_load_key = false;
            for (const auto& edge : edges)
            {
                quick_save_key |= bindings.matches(edge, InputAction::QuickSave);
                quick_load_key |= bindings.matches(edge, InputAction::QuickLoad);
            }
            if (!menu.demo() && (press(InputAction::QuickSave) || press(InputAction::QuickLoad)))
            {
                if (press(InputAction::QuickSave) && player && alive)
                    quick_save = true;
                if (press(InputAction::QuickLoad) && player)
                    quick_load = true;
                return false;
            }
            if (quick_save_key || quick_load_key)
                return false;
            // Diagnostic control shortcuts do not change interactive ownership.
            if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) &&
                (e.key.scancode == SDL_SCANCODE_F1 || e.key.scancode == SDL_SCANCODE_F2 ||
                 e.key.scancode == SDL_SCANCODE_F3))
                return false;
            if (gamepad)
                controls.command_edges(edges, false);
            else
                controls.event(e, window);
            return false;
        }
    };
} // namespace OpenBlood2
