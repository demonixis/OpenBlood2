// Selective original CNewStatusBar: InitStatTabs, AdjustRes, SetupTab, JustifySurface.
// Original locations/resources and composition order retained; typed surfaces replace the client ABI.
#include "game/original/NewStatusBar.h"
#include "game/original/WeaponDefs.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

bool CNewStatusBar::Init()
{
    if (initialized)
        return true;
    if (!canvas.load_font("interface/statusbar/fonts/big_numbers.pcx", "interface/statusbar/fonts/big_numbers.fnt",
                          big_numbers) ||
        !canvas.load_font("interface/statusbar/fonts/small_numbers.pcx", "interface/statusbar/fonts/small_numbers.fnt",
                          small_numbers))
        return false;
    InitStatTabs();
    initialized = canvas.error.empty();
    return initialized;
}
bool CNewStatusBar::Update(const OpenBlood2::PlayerStats& s, unsigned width, unsigned height)
{
    if (!initialized || width < 135 || height < 100)
        return false;
    const auto* weapon = weapon_definition(s.weapon);
    if (!weapon || !std::isfinite(s.health) || !std::isfinite(s.armor) || !std::isfinite(s.ammo) ||
        !std::isfinite(s.alternate_ammo) || !std::isfinite(s.max_health) || !std::isfinite(s.max_armor))
        return false;
    const bool resized = width != m_nScreenWidth || height != m_nScreenHeight;
    if (resized)
        AdjustRes(width, height);
    if (have_previous && previous == s && !resized)
        return true;
    // Source UpdateCurrentWeap: reserved inventory slots clear the weapon picture.
    m_stAmmo.hIcon = s.weapon_slot < 10 ? canvas.CreateSurfaceFromBitmap(weapon->m_szStatusIcon) : nullptr;
    if (!canvas.error.empty())
        return false;
    auto count = [](float v) { return unsigned(std::clamp(v, 0.f, 65535.f)); };
    m_stHealth.value = count(s.health);
    m_stHealth.max = count(s.max_health);
    m_stArmor.value = count(s.armor);
    m_stArmor.max = count(s.max_armor);
    m_stAmmo.value = count(std::min(s.ammo, 999.f));
    // BloodClientShell SMSG_PLAYERSTATSUPDATE suppresses identical alternate pools.
    m_stAltAmmo.value = s.alternate_ammo_type != s.ammo_type ? count(std::min(s.alternate_ammo, 999.f)) : 0;
    const unsigned include = m_nStatLevel > STATTAB_LOW ? TAB_INC_HIGH : TAB_INC_LOW;
    SetupTab(m_stHealth, &big_numbers, include);
    SetupTab(m_stArmor, &big_numbers, include);
    SetupTab(m_stAmmo, &big_numbers, include);
    SetupTab(m_stAltAmmo, &small_numbers, include);
    previous = s;
    have_previous = true;
    ++revision;
    return true;
}
void CNewStatusBar::AdjustRes(unsigned width, unsigned height)
{
    // Set the screen width and height variables to the new resolution
    m_nScreenWidth = width;
    m_nScreenHeight = height;

    // If we are lower than the prefered res, default to LOW stat mode
    if ((m_nScreenWidth < STATTAB_LOW_WIDTH) || (m_nScreenHeight < STATTAB_LOW_HEIGHT))
    {

        m_nStatLevel = STATTAB_LOW;
    }
    else
    {

        m_nStatLevel = STATTAB_HIGH;
    }

    // Adjust the stat tab locations to the new resolution
    m_stHealth.locX = 0;
    m_stHealth.locY = m_nScreenHeight - m_stHealth.sizeY;

    if (m_nStatLevel == STATTAB_HIGH)
        m_stArmor.locX = m_stHealth.sizeX;
    else
        m_stArmor.locX = m_stHealth.sizeX / 2;
    m_stArmor.locY = m_nScreenHeight - m_stArmor.sizeY;

    m_stAmmo.locX = m_nScreenWidth - m_stAmmo.sizeX;
    m_stAmmo.locY = m_nScreenHeight - m_stAmmo.sizeY;

    m_stAltAmmo.locX = m_stAmmo.locX + (m_stAmmo.sizeX / 2) - (m_stAltAmmo.sizeX / 2);
    m_stAltAmmo.locY = m_stAmmo.locY - m_stAltAmmo.sizeY - 3;
}
bool CNewStatusBar::InitStatTabs()
{
    // Health status tab
    m_stHealth.sizeX = 185;
    m_stHealth.sizeY = 55;

    m_stHealth.hTab = canvas.CreateSurface(m_stHealth.sizeX, m_stHealth.sizeY);
    m_stHealth.hBorder = canvas.CreateSurfaceFromBitmap("interface/statusbar/border_health.pcx");
    m_stHealth.hIcon = canvas.CreateSurfaceFromBitmap("interface/statusbar/icon_health.pcx");
    m_stHealth.hBar = canvas.CreateSurfaceFromBitmap("interface/statusbar/status_ticker.pcx");

    m_stHealth.borderX = 10;
    m_stHealth.borderY = 2;
    m_stHealth.borderJ = STATTAB_JUST_NW;

    m_stHealth.iconX = 32;
    m_stHealth.iconY = 7;
    m_stHealth.iconJ = STATTAB_JUST_NW;

    m_stHealth.barX = 115;
    m_stHealth.barY = 5;
    m_stHealth.barJ = STATTAB_JUST_NW;

    m_stHealth.valueX = 80;
    m_stHealth.valueY = 10;
    m_stHealth.valueJ = 1;
    m_stHealth.digits = 3;

    // Armor status tab
    m_stArmor.sizeX = 200;
    m_stArmor.sizeY = 55;

    m_stArmor.hTab = canvas.CreateSurface(m_stArmor.sizeX, m_stArmor.sizeY);
    m_stArmor.hBorder = canvas.CreateSurfaceFromBitmap("interface/statusbar/border_armor.pcx");
    m_stArmor.hIcon = canvas.CreateSurfaceFromBitmap("interface/statusbar/icon_armor.pcx");
    m_stArmor.hBar = canvas.CreateSurfaceFromBitmap("interface/statusbar/status_ticker.pcx");

    m_stArmor.borderX = 0;
    m_stArmor.borderY = 1;
    m_stArmor.borderJ = STATTAB_JUST_NW;

    m_stArmor.iconX = 45;
    m_stArmor.iconY = 7;
    m_stArmor.iconJ = STATTAB_JUST_NW;

    m_stArmor.barX = 125;
    m_stArmor.barY = 5;
    m_stArmor.barJ = STATTAB_JUST_NW;

    m_stArmor.valueX = 90;
    m_stArmor.valueY = 10;
    m_stArmor.valueJ = 1;
    m_stArmor.digits = 3;

    // Ammo status tab
    m_stAmmo.sizeX = 135;
    m_stAmmo.sizeY = 75;

    m_stAmmo.hTab = canvas.CreateSurface(m_stAmmo.sizeX, m_stAmmo.sizeY);
    m_stAmmo.hBorder = canvas.CreateSurfaceFromBitmap("interface/statusbar/border_ammo.pcx");
    m_stAmmo.hIcon = 0;
    m_stAmmo.hBar = 0;

    m_stAmmo.borderX = 8;
    m_stAmmo.borderY = 24;
    m_stAmmo.borderJ = STATTAB_JUST_NW;

    m_stAmmo.iconX = 63;
    m_stAmmo.iconY = 0;
    m_stAmmo.iconJ = STATTAB_JUST_N;

    m_stAmmo.valueX = 63;
    m_stAmmo.valueY = 32;
    m_stAmmo.valueJ = 1;
    m_stAmmo.digits = 3;

    // Alt Ammo status tab
    m_stAltAmmo.sizeX = 60;
    m_stAltAmmo.sizeY = 20;

    m_stAltAmmo.hTab = canvas.CreateSurface(m_stAmmo.sizeX, m_stAmmo.sizeY);
    m_stAltAmmo.hBorder = 0;
    // Build 108's demo has no raidcanister artwork. Keep its secondary count without
    // importing a retail image; all other requested HUD resources remain required.
    m_stAltAmmo.hIcon =
        demo_ ? nullptr : canvas.CreateSurfaceFromBitmap("interface/statusbar/weapons/raidcanister.pcx");
    m_stAltAmmo.hBar = 0;

    m_stAltAmmo.borderX = 0;
    m_stAltAmmo.borderY = 0;
    m_stAltAmmo.borderJ = 0;

    m_stAltAmmo.iconX = 0;
    m_stAltAmmo.iconY = 0;
    m_stAltAmmo.iconJ = STATTAB_JUST_NW;

    m_stAltAmmo.valueX = 35;
    m_stAltAmmo.valueY = 3;
    m_stAltAmmo.valueJ = 0;
    m_stAltAmmo.digits = 3;

    return true;
}
void CNewStatusBar::SetupTab(StatTab& stTab, const OpenBlood2::HudFont* font, unsigned nFlags)
{

    unsigned x, y;

    // Clear the surface to the transparent color
    if (stTab.hTab)
        canvas.FillRect(stTab.hTab, nullptr, m_hTransColor);

    // Draw the border in the offset position
    if (stTab.hBorder && (nFlags & TAB_INC_BORDER))
    {
        x = stTab.borderX;
        y = stTab.borderY;
        JustifySurface(stTab.hBorder, stTab.borderJ, x, y);
        canvas.DrawSurfaceToSurface(stTab.hTab, stTab.hBorder, nullptr, x, y);
    }

    // Draw the icon in the offset position
    if (stTab.hIcon && (nFlags & TAB_INC_ICON))
    {
        x = stTab.iconX;
        y = stTab.iconY;
        JustifySurface(stTab.hIcon, stTab.iconJ, x, y);
        canvas.DrawSurfaceToSurfaceTransparent(stTab.hTab, stTab.hIcon, nullptr, x, y, m_hTransColor);
    }

    // If there is a bar, draw the portion of it that should be drawn
    if (stTab.hBar && (nFlags & TAB_INC_BAR))
    {
        OpenBlood2::HudRect rect;
        unsigned width, height;

        canvas.GetSurfaceDims(stTab.hBar, &width, &height);

        rect.top = 0;
        rect.left = 0;
        rect.bottom = height;
        rect.right = stTab.max ? int(width * std::min(1.f, float(stTab.value) / float(stTab.max))) : 0;

        canvas.DrawSurfaceToSurfaceTransparent(stTab.hTab, stTab.hBar, &rect, stTab.barX, stTab.barY, m_hTransColor);
    }

    // Draw the value if needed
    if (stTab.digits && (nFlags & TAB_INC_VALUE))
    {
        // The source allocated digits+1 and used sprintf/delete: bounded stack storage fixes overflow/lifetime.
        char value[16];
        std::snprintf(value, sizeof(value), "%u", stTab.value);
        font->draw(*stTab.hTab, value, stTab.valueX, stTab.valueY, stTab.valueJ);
    }
}
void CNewStatusBar::JustifySurface(const OpenBlood2::HudSurface* surf, unsigned just, unsigned& x, unsigned& y)
{

    unsigned width, height;
    canvas.GetSurfaceDims(surf, &width, &height);

    switch (just)
    {
        case STATTAB_JUST_NW:
            return;
        case STATTAB_JUST_N:
            x -= (width / 2);
            return;
        case STATTAB_JUST_NE:
            x -= width;
            return;
        case STATTAB_JUST_E:
            x -= width;
            y -= (height / 2);
            return;
        case STATTAB_JUST_SE:
            x -= width;
            y -= height;
            return;
        case STATTAB_JUST_S:
            x -= (width / 2);
            y -= height;
            return;
        case STATTAB_JUST_SW:
            y -= height;
            return;
        case STATTAB_JUST_W:
            y -= (height / 2);
            return;
        case STATTAB_JUST_C:
            x -= (width / 2);
            y -= (height / 2);
            return;
        default:
            return;
    }
}

// Original SetTeamID/DisplayFlag: external multipatch PCX, cached owned surfaces.
bool CNewStatusBar::UpdateCTF(DBYTE team, DBYTE flag)
{
    if (team > 2 || flag > 2)
        return false;
    if (team == team_id && flag == flag_color)
        return true;
    const auto* next_team = team ? canvas.CreateSurfaceFromBitmap(team == 1 ? "interface_multipatch\\teamblue.pcx"
                                                                            : "interface_multipatch\\teamred.pcx")
                                 : nullptr;
    const auto* next_flag = flag ? canvas.CreateSurfaceFromBitmap(flag == 1 ? "interface_multipatch\\flagred.pcx"
                                                                            : "interface_multipatch\\flagblue.pcx")
                                 : nullptr;
    if ((team && !next_team) || (flag && !next_flag))
        return false;
    team_icon = next_team;
    flag_icon = next_flag;
    team_id = team;
    flag_color = flag;
    ++revision;
    return true;
}
