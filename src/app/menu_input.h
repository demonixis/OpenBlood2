#pragma once
#include "app/demo_flow.h"
#include "app/movement_controls.h"
#include "game/menu.h"
#include "game/original/Credits.h"
#include <vector>
namespace OpenBlood2
{
    // Normal interactive input. Smoke tests deliberately retain their original adapter.
    struct MenuInput
    {
        MovementControls controls;
        bool inspector = false;
        bool demo_mode = false;
        DemoFlow demo;
        // Original GS_CREDITS: the in-world text-only credits screen pauses play.
        bool credits_screen = false;
        // Owner-set CBloodClientShell::m_bRollCredits: key downs also reach the roll.
        bool roll_credits = false;
        bool credits_exit = false;
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
            return demo_mode ? demo.paused() : menu.active || credits_screen;
        }
        void reset(Menu& menu, bool player)
        {
            if (demo_mode)
            {
                demo.reset(controls, inspector, player);
                return;
            }
            menu.cancel_pointer();
            quick_save = quick_load = false;
            controls.player_changed(player && !menu.active && !credits_screen && !inspector);
        }
        // True means the caller should forward this event to ImGui's inspector.
        bool event(const SDL_Event& e, SDL_WindowID window, Menu& menu, bool player, bool alive,
                   bool inspector_available, float width, float height)
        {
            if (demo_mode)
                return demo.event(e, window, controls, inspector, inspector_available, player, alive);
            if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && e.key.windowID != window)
                return false;
            if ((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
                e.button.windowID != window)
                return false;
            if (e.type == SDL_EVENT_MOUSE_MOTION && e.motion.windowID != window)
                return false;
            if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST && e.window.windowID == window)
            {
                controls.event(e, window);
                menu.cancel_pointer();
                if (player && !menu.active && !credits_screen && !inspector)
                    menu.open(true, alive);
                reset(menu, player);
                return inspector;
            }
            if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED && e.window.windowID == window)
            {
                controls.event(e, window);
                return inspector;
            }
            if (!controls.focused)
                return false;
            if (menu.promotion != MenuPromotion::None)
            {
                if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat &&
                    (e.key.scancode == SDL_SCANCODE_ESCAPE || e.key.scancode == SDL_SCANCODE_SPACE ||
                     e.key.scancode == SDL_SCANCODE_RETURN || e.key.scancode == SDL_SCANCODE_KP_ENTER))
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
            if (credits_screen)
                return false;
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_ESCAPE)
            {
                if (menu.active)
                    menu.back();
                else
                    menu.open(player, alive);
                reset(menu, player);
                return false;
            }
            if (menu.demo() && !menu.active && player && !alive && e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat &&
                (e.key.scancode == SDL_SCANCODE_RETURN || e.key.scancode == SDL_SCANCODE_KP_ENTER))
            {
                demo.restart = true; // Shared owned retry hook; future checkpoint restore replaces the app handler.
                return false;
            }
            if (menu.active)
            {
                if (e.type == SDL_EVENT_KEY_DOWN)
                {
                    if (menu.page == MenuPage::Quit && !e.key.repeat &&
                        (e.key.scancode == SDL_SCANCODE_Y || e.key.scancode == SDL_SCANCODE_N))
                    {
                        menu.selected = e.key.scancode == SDL_SCANCODE_Y ? 0 : 1;
                        menu.activate();
                        return false;
                    }
                    switch (e.key.scancode)
                    {
                        case SDL_SCANCODE_UP:
                            menu.select(-1);
                            break;
                        case SDL_SCANCODE_DOWN:
                            menu.select(1);
                            break;
                        case SDL_SCANCODE_LEFT:
                            menu.adjust(-1);
                            break;
                        case SDL_SCANCODE_RIGHT:
                            menu.adjust(1);
                            break;
                        case SDL_SCANCODE_RETURN:
                        case SDL_SCANCODE_KP_ENTER:
                            if (!e.key.repeat)
                                menu.activate();
                            break;
                        default:
                            break;
                    }
                }
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
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && !menu.demo() &&
                (e.key.scancode == SDL_SCANCODE_F5 || e.key.scancode == SDL_SCANCODE_F6))
            {
                if (e.key.scancode == SDL_SCANCODE_F5 && player && alive)
                    quick_save = true;
                if (e.key.scancode == SDL_SCANCODE_F6 && player)
                    quick_load = true;
                return false;
            }
            if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) &&
                (e.key.scancode == SDL_SCANCODE_F5 || e.key.scancode == SDL_SCANCODE_F6))
                return false;
            // Diagnostic control shortcuts do not change interactive ownership.
            if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) &&
                (e.key.scancode == SDL_SCANCODE_F1 || e.key.scancode == SDL_SCANCODE_F2 ||
                 e.key.scancode == SDL_SCANCODE_F3))
                return false;
            controls.event(e, window);
            return false;
        }
    };
} // namespace OpenBlood2
