#pragma once
#include "app/movement_controls.h"
#include "game/menu.h"
namespace OpenBlood2
{
    // Normal interactive input. Smoke tests deliberately retain their original adapter.
    struct MenuInput
    {
        MovementControls controls;
        bool inspector = false;
        void reset(Menu& menu, bool player)
        {
            menu.cancel_pointer();
            controls.player_changed(player && !menu.active && !inspector);
        }
        // True means the caller should forward this event to ImGui's inspector.
        bool event(const SDL_Event& e, SDL_WindowID window, Menu& menu, bool player, bool alive,
                   bool inspector_available, float width, float height)
        {
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
                if (player && !menu.active && !inspector)
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
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_ESCAPE)
            {
                if (menu.active)
                    menu.back();
                else
                    menu.open(player, alive);
                reset(menu, player);
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
                        case SDL_SCANCODE_SPACE: // Original Splash_OnKeyDown also accepts VK_SPACE.
                            if (!e.key.repeat)
                                menu.dismiss_splash();
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
