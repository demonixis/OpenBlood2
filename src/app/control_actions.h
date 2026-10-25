#pragma once
#include "app/movement_controls.h"
#include "game/menu.h"
#include "game/original/BloodShells.h"

namespace OpenBlood2
{
    // Shared by ordinary play and diagnostic play. Preferences are owned by the frontend;
    // commands still work without a menu in the diagnostic adapter.
    inline void apply_control_actions(MovementControls& controls, CBloodClientShell& shell, bool blocked,
                                      MenuSettings* settings = nullptr)
    {
        if (settings)
        {
            controls.run_lock = settings->run_lock;
            controls.mouse_look = settings->mouse_look;
            shell.m_bCrosshairOriginallyOn = settings->crosshair;
            shell.m_nCrosshairOriginalNum = DBYTE(settings->crosshair_number);
        }
        if (controls.sample_command(InputAction::RunLock, blocked))
        {
            controls.run_lock = !controls.run_lock;
            shell.SetControlNotice(controls.run_lock ? "Auto run ON" : "Auto run OFF");
            if (settings)
                settings->run_lock = controls.run_lock;
        }
        if (controls.sample_command(InputAction::Crosshair, blocked) && !shell.m_bZoomView)
        {
            if (controls.keys[SDL_SCANCODE_LSHIFT] || controls.keys[SDL_SCANCODE_RSHIFT])
                shell.m_nCrosshairOriginalNum = DBYTE((shell.m_nCrosshairOriginalNum + 1) % 42);
            else
                shell.m_bCrosshairOriginallyOn = !shell.m_bCrosshairOriginallyOn;
            if (settings)
            {
                settings->crosshair = shell.m_bCrosshairOriginallyOn;
                settings->crosshair_number = shell.m_nCrosshairOriginalNum;
            }
        }
        if (!shell.m_bZoomView)
        {
            shell.m_bShowCrosshair = shell.m_bCrosshairOriginallyOn;
            shell.m_nCrosshair = shell.m_nCrosshairOriginalNum;
        }
        shell.grab_down = controls.sample_press(InputAction::Grab, blocked);
        shell.taunt_down = controls.sample_press(InputAction::Taunt, blocked);
        shell.drop_weapon_requested |= controls.sample_command(InputAction::DropWeapon, blocked);
    }
} // namespace OpenBlood2
