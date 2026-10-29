// Selective original CNewStatusBar: InitStatTabs, InitInvTabs, InitWeaponTabs, AdjustRes, UpdateInv,
// UpdateWeap/UpdateCurrentWeap, ToggleWeapons, DrawInvTabs, DrawWeaponTabs, SetupTab, JustifySurface.
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
    if (initialized)
    {
        InitInvTabs();
        InitWeaponTabs();
    }
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
    if (!have_previous || previous.weapon_slots != s.weapon_slots || previous.weapon_slot != s.weapon_slot)
        UpdateWeapons(s);
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

    if (m_nStatLevel == STATTAB_LOW)
        m_nInvLevel = INVTAB_INVISIBLE;

    // Adjust the weapon tabs: ten slots stacked around the vertical centre at the right edge
    unsigned offset = (m_nScreenHeight / 2) - (m_stWeapons[0].sizeY * 5);
    for (int i = 0; i < WEAPTAB_SLOTS; i++)
    {
        m_stWeapons[i].locX = m_nScreenWidth - m_stWeapons[i].sizeX;
        m_stWeapons[i].locY = offset;
        offset += m_stWeapons[i].sizeY;
    }

    // Adjust the inventory items tabs
    const int screen_height = int(m_nScreenHeight);
    item_loc_x[1] = 0;
    item_loc_y[1] = (screen_height / 2) - int(m_stItems[1].sizeY / 2);
    item_loc_x[0] = -15;
    item_loc_y[0] = item_loc_y[1] - int(m_stItems[1].sizeY);
    item_loc_x[2] = -15;
    item_loc_y[2] = item_loc_y[1] + int(m_stItems[1].sizeY);

    // Adjust the current item tab locations to the new resolution
    m_stCurrentItem.locX = 20;
    m_stCurrentItem.locY = m_nScreenHeight - m_stHealth.sizeY - m_stCurrentItem.sizeY;

    if (m_nStatLevel == STATTAB_HIGH)
    {
        m_nItemNameX = int(m_stItems[1].sizeX) + 10;
        m_nItemNameY = (screen_height / 2) - int(stat_font.height / 2);
    }
    else
    {
        m_nItemNameX = int(m_stCurrentItem.locX + m_stCurrentItem.sizeX) + 10;
        m_nItemNameY = int(m_stCurrentItem.locY + (m_stCurrentItem.sizeY / 2)) - int(stat_font.height / 2);
    }
}
void CNewStatusBar::InitInvTabs()
{
    // The source creates these with the other tabs. Here a missing picture or font only
    // disables the item tabs (item_error); the health/armor/ammo tabs stay required.
    for (int i = 0; i < 3; i++)
    {
        m_stItems[i].sizeX = 75;
        m_stItems[i].sizeY = 50;

        m_stItems[i].hTab = canvas.CreateSurface(m_stItems[i].sizeX, m_stItems[i].sizeY);
        m_stItems[i].hBorder = canvas.CreateSurfaceFromBitmap("interface/statusbar/border_inv.pcx");
        m_stItems[i].hIcon = 0;
        m_stItems[i].hBar = 0;

        m_stItems[i].borderX = 0;
        m_stItems[i].borderY = 4;
        m_stItems[i].borderJ = STATTAB_JUST_NW;

        m_stItems[i].iconX = 50;
        m_stItems[i].iconY = 18;
        m_stItems[i].iconJ = STATTAB_JUST_C;

        m_stItems[i].valueX = 50;
        m_stItems[i].valueY = 29;
        m_stItems[i].valueJ = 1; // CF_JUSTIFY_CENTER
        m_stItems[i].digits = 3;
    }

    m_stCurrentItem.sizeX = 50;
    m_stCurrentItem.sizeY = 45;

    m_stCurrentItem.hTab = canvas.CreateSurface(m_stCurrentItem.sizeX, m_stCurrentItem.sizeY);
    m_stCurrentItem.hBorder = canvas.CreateSurfaceFromBitmap("interface/statusbar/border_current.pcx");
    m_stCurrentItem.hIcon = 0;
    m_stCurrentItem.hBar = 0;

    m_stCurrentItem.borderX = 2;
    m_stCurrentItem.borderY = 0;
    m_stCurrentItem.borderJ = STATTAB_JUST_NW;

    m_stCurrentItem.iconX = 25;
    m_stCurrentItem.iconY = 14;
    m_stCurrentItem.iconJ = STATTAB_JUST_C;

    m_stCurrentItem.valueX = 25;
    m_stCurrentItem.valueY = 25;
    m_stCurrentItem.valueJ = 1; // CF_JUSTIFY_CENTER
    m_stCurrentItem.digits = 3;

    m_hItemName = canvas.CreateSurface(100, 14);
    if (canvas.load_font("interface/statusbar/fonts/stat_font_1.pcx", "interface/statusbar/fonts/stat_font_1.fnt",
                         stat_font))
        stat_font.space_width = stat_font.height / 4;
    inventory_ready = canvas.error.empty();
    if (!inventory_ready)
    {
        item_error = canvas.error;
        canvas.error.clear();
    }
    else if (m_hItemName)
        canvas.FillRect(m_hItemName, nullptr, m_hTransColor);
}
void CNewStatusBar::InitWeaponTabs()
{
    for (int i = 0; i < WEAPTAB_SLOTS; i++)
    {
        m_stWeapons[i].sizeX = 150;
        m_stWeapons[i].sizeY = 25;

        m_stWeapons[i].hTab = canvas.CreateSurface(m_stWeapons[i].sizeX, m_stWeapons[i].sizeY);
        m_stWeapons[i].hBorder = canvas.CreateSurfaceFromBitmap("interface/statusbar/border_weapon.pcx");
        m_stWeapons[i].hIcon = 0;
        m_stWeapons[i].hBar = 0;

        m_stWeapons[i].borderX = 122;
        m_stWeapons[i].borderY = 3;
        m_stWeapons[i].borderJ = STATTAB_JUST_NW;

        m_stWeapons[i].iconX = 125;
        m_stWeapons[i].iconY = 4;
        m_stWeapons[i].iconJ = STATTAB_JUST_NE;

        m_stWeapons[i].value = i + 1;
        m_stWeapons[i].valueX = 150;
        m_stWeapons[i].valueY = 0;
        m_stWeapons[i].valueJ = 2; // CF_JUSTIFY_RIGHT
        m_stWeapons[i].digits = 2;
    }
    weapons_ready = canvas.error.empty();
    if (!weapons_ready)
    {
        weapon_error = canvas.error;
        canvas.error.clear();
        return;
    }
    for (auto& tab : m_stWeapons)
        SetupTab(tab, &small_numbers, TAB_INC_HIGH);
}
void CNewStatusBar::UpdateWeapons(const OpenBlood2::PlayerStats& s)
{
    if (!weapons_ready)
        return;
    // Source UpdateCurrentWeap: a reserved inventory slot keeps the previous highlight.
    if (s.weapon_slot < WEAPTAB_SLOTS)
        m_nCurrentWeapon = s.weapon_slot;
    for (unsigned i = 0; i < WEAPTAB_SLOTS; i++)
    {
        // UpdateWeap's normal picture; the current slot uses the highlighted one when it exists.
        const auto* weapon = s.weapon_slots[i] ? weapon_definition(s.weapon_slots[i]) : nullptr;
        const char* picture = nullptr;
        if (weapon)
            picture =
                i == m_nCurrentWeapon && weapon->m_szStatusIconH ? weapon->m_szStatusIconH : weapon->m_szStatusIcon;
        m_stWeapons[i].hIcon = picture && *picture ? canvas.CreateSurfaceFromBitmap(picture) : nullptr;
        if (picture && *picture && !m_stWeapons[i].hIcon)
        {
            weapon_error = canvas.error;
            canvas.error.clear();
        }
        SetupTab(m_stWeapons[i], &small_numbers, TAB_INC_HIGH);
    }
}
void CNewStatusBar::ToggleWeapons(float time)
{
    if (m_nWeapLevel == WEAPTAB_INVISIBLE)
    {
        m_nWeapLevel = WEAPTAB_SCROLL_IN;
        m_fWeapUpdateTime = time;
    }
    else if (m_nWeapLevel == WEAPTAB_STOPPED)
    {
        m_nWeapLevel = WEAPTAB_SCROLL_OUT;
        m_fWeapUpdateTime = time;
    }
}
void CNewStatusBar::ResetWeapons()
{
    m_nWeapLevel = WEAPTAB_INVISIBLE;
    m_fWeapScrollRatio = 0;
}
CNewStatusBar::WeaponLayout CNewStatusBar::DrawWeaponTabs(float time)
{
    WeaponLayout layout;
    // Draw: if(m_nWeapLevel && m_bHighGraphics) DrawWeaponTabs();
    if (!weapons_ready || !m_nWeapLevel || m_nStatLevel != STATTAB_HIGH)
        return layout;

    // Handle the scrolling in and out of the weapon tabs
    switch (m_nWeapLevel)
    {
        case WEAPTAB_SCROLL_IN:
            if (time - m_fWeapUpdateTime > WEAPTAB_SCROLL_TIME)
            {
                m_fWeapScrollRatio = 1.0f;
                m_nWeapLevel = WEAPTAB_STOPPED;
                m_fWeapUpdateTime = time;
            }
            else
                m_fWeapScrollRatio = (time - m_fWeapUpdateTime) / WEAPTAB_SCROLL_TIME;
            break;

        case WEAPTAB_STOPPED:
            break;

        case WEAPTAB_SCROLL_OUT:
            if (time - m_fWeapUpdateTime > WEAPTAB_SCROLL_TIME)
            {
                m_fWeapScrollRatio = 0.0f;
                m_nWeapLevel = WEAPTAB_INVISIBLE;
                m_fWeapUpdateTime = time;
            }
            else
                m_fWeapScrollRatio = 1.0f - (time - m_fWeapUpdateTime) / WEAPTAB_SCROLL_TIME;

            break;
    }

    layout.visible = m_nWeapLevel != WEAPTAB_INVISIBLE; // The final scroll-out frame is off screen.
    layout.x = int(unsigned(m_stWeapons[0].sizeX - (m_stWeapons[0].sizeX * m_fWeapScrollRatio)) + m_stWeapons[0].locX);
    for (int i = 0; i < WEAPTAB_SLOTS; i++)
        layout.y[i] = int(m_stWeapons[i].locY);
    return layout;
}
void CNewStatusBar::UpdateInv(const OpenBlood2::InventoryItems& items, bool bShow, float time)
{
    if (!inventory_ready)
        return;
    const auto picture = [this](const std::string& name) -> OpenBlood2::HudSurface*
    {
        if (name.empty())
            return nullptr;
        const std::string path = "interface/statusbar/items/" + name;
        auto* surface = canvas.CreateSurfaceFromBitmap(path.c_str());
        if (!surface)
        {
            item_error = canvas.error;
            canvas.error.clear();
        }
        return surface;
    };

    // Fill in the name of the current object
    canvas.FillRect(m_hItemName, nullptr, m_hTransColor);
    if (items.current && !items.current->name.empty())
        stat_font.draw(*m_hItemName, items.current->name, 0, 0, 0);

    // Currently selected item, the highlighted item in the inv tabs, previous and next
    m_stCurrentItem.hIcon = items.current ? picture(items.current->icon) : nullptr;
    m_stCurrentItem.value = items.current ? items.current->charges : 0;
    m_stItems[1].hIcon = items.current ? picture(items.current->highlighted_icon) : nullptr;
    m_stItems[1].value = m_stCurrentItem.value;
    m_stItems[0].hIcon = items.previous ? picture(items.previous->icon) : nullptr;
    m_stItems[0].value = items.previous ? items.previous->charges : 0;
    m_stItems[2].hIcon = items.next ? picture(items.next->icon) : nullptr;
    m_stItems[2].value = items.next ? items.next->charges : 0;

    // Update the time so it stays on the screen for a while
    switch (m_nInvLevel)
    {
        case INVTAB_INVISIBLE:
            if (bShow)
            {
                m_nInvLevel = INVTAB_SCROLL_IN;
                m_fInvUpdateTime = time;
            }
            break;

        case INVTAB_SCROLL_IN:
            break;

        case INVTAB_STOPPED:
            m_fInvUpdateTime = time;
            break;

        case INVTAB_SCROLL_OUT:
            m_nInvLevel = INVTAB_SCROLL_IN;
            m_fInvUpdateTime = time;
            break;
    }

    // Setup the items tabs with the new information
    for (int i = 0; i < 3; i++)
        if (m_stItems[i].hIcon)
            SetupTab(m_stItems[i], &stat_font, TAB_INC_HIGH);
    // DrawStatTabs re-sets the current item tab every frame; its inputs only change here.
    if (m_stCurrentItem.hIcon)
        SetupTab(m_stCurrentItem, &stat_font, TAB_INC_HIGH);
    ++revision;
}
CNewStatusBar::InventoryLayout CNewStatusBar::DrawInvTabs(float time)
{
    InventoryLayout layout;
    if (!inventory_ready)
        return layout;
    const bool high = m_nStatLevel == STATTAB_HIGH; // m_bHighGraphics follows the same resolution test.
    layout.current_item = m_stCurrentItem.hIcon != nullptr;
    layout.name = layout.current_item && !high;
    layout.name_x = m_nItemNameX;
    layout.name_y = m_nItemNameY;
    if (!m_nInvLevel || !high)
        return layout;

    // Handle the scrolling in and out of the inventory item tabs
    switch (m_nInvLevel)
    {
        case INVTAB_SCROLL_IN:
            if (time - m_fInvUpdateTime > INVTAB_SCROLL_TIME)
            {
                m_fInvScrollRatio = 1.0f;
                m_nInvLevel = INVTAB_STOPPED;
                m_fInvUpdateTime = time;
            }
            else
                m_fInvScrollRatio = (time - m_fInvUpdateTime) / INVTAB_SCROLL_TIME;
            break;

        case INVTAB_STOPPED:
            if (time - m_fInvUpdateTime > INVTAB_DISP_TIME)
            {
                m_nInvLevel = INVTAB_SCROLL_OUT;
                m_fInvUpdateTime = time;
            }
            break;
        case INVTAB_SCROLL_OUT:
            if (time - m_fInvUpdateTime > INVTAB_SCROLL_TIME)
            {
                m_fInvScrollRatio = 0.0f;
                m_nInvLevel = INVTAB_INVISIBLE;
                m_fInvUpdateTime = time;
            }
            else
                m_fInvScrollRatio = 1.0f - (time - m_fInvUpdateTime) / INVTAB_SCROLL_TIME;

            break;
    }

    // Previous, current (with the item name) and next item, sliding in from the left edge
    for (int i = 0; i < 3; i++)
        if (m_stItems[i].hIcon)
        {
            layout.visible[i] = true;
            layout.x[i] = int((item_loc_x[i] + int(m_stItems[i].sizeX)) * m_fInvScrollRatio) - int(m_stItems[i].sizeX);
            layout.y[i] = item_loc_y[i];
        }
    if (layout.visible[1])
        layout.name = true;
    return layout;
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
