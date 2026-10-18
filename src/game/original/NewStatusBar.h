// Selective original Blood II CNewStatusBar port. See original_hud_sources.json.
#pragma once
#include "engine/hud_surface.h"
#include "game/player_messages.h"
#define TAB_INC_NONE 0x00
#define TAB_INC_BORDER 0x01
#define TAB_INC_ICON 0x02
#define TAB_INC_BAR 0x04
#define TAB_INC_VALUE 0x08
#define TAB_INC_LOW 0x0A  // Icon and number
#define TAB_INC_HIGH 0x0F // Everything

//**********************************************************

#define STATTAB_JUST_NW 0
#define STATTAB_JUST_N 1
#define STATTAB_JUST_NE 2
#define STATTAB_JUST_E 3
#define STATTAB_JUST_SE 4
#define STATTAB_JUST_S 5
#define STATTAB_JUST_SW 6
#define STATTAB_JUST_W 7
#define STATTAB_JUST_C 8

#define STATTAB_NONE 0
#define STATTAB_LOW 1
#define STATTAB_HIGH 2

#define STATTAB_LOW_WIDTH 512
#define STATTAB_LOW_HEIGHT 384

struct StatTab
{

    OpenBlood2::HudSurface* hTab;    // The composite surface for the tab
    OpenBlood2::HudSurface* hBorder; // The border graphic to use for the tab
    OpenBlood2::HudSurface* hIcon;   // The icon to place within the border
    OpenBlood2::HudSurface* hBar;    // If a bar is used... what's the graphic

    unsigned borderX; // X Offset of the border
    unsigned borderY; // Y Offset of the border
    char borderJ;     // Justification of the border

    unsigned iconX; // X Offset of the icon
    unsigned iconY; // Y Offset of the icon
    char iconJ;     // Justification of the icon

    unsigned barX; // X Offset of the bar
    unsigned barY; // Y Offset of the bar
    char barJ;     // Justification of the bar

    unsigned sizeX; // Width of the tab
    unsigned sizeY; // Height of the tab

    unsigned locX; // X Screen position
    unsigned locY; // Y Screen position

    unsigned value; // Value to display within the tab
    unsigned max;   // Maximum value

    unsigned valueX; // X Offset of the value
    unsigned valueY; // Y Offset of the value
    char valueJ;     // Justification of the value
    char digits;     // Number of digits of the value to display
};

class CNewStatusBar
{
  public:
    explicit CNewStatusBar(const OpenBlood2::Resources& resources) : canvas(resources) {}
    bool Init();
    bool Update(const OpenBlood2::PlayerStats&, unsigned width, unsigned height);
    std::array<const StatTab*, 4> tabs() const
    {
        return {&m_stHealth, &m_stArmor, &m_stAmmo, &m_stAltAmmo};
    }
    OpenBlood2::HudCanvas canvas;
    std::uint64_t revision = 0;
    bool high_graphics() const
    {
        return m_nStatLevel == STATTAB_HIGH;
    }

  private:
    bool InitStatTabs();
    void AdjustRes(unsigned width, unsigned height);
    void SetupTab(StatTab&, const OpenBlood2::HudFont*, unsigned flags);
    void JustifySurface(const OpenBlood2::HudSurface*, unsigned just, unsigned& x, unsigned& y);
    StatTab m_stHealth{}, m_stArmor{}, m_stAmmo{}, m_stAltAmmo{};
    OpenBlood2::HudFont big_numbers, small_numbers;
    unsigned m_nScreenWidth = 0, m_nScreenHeight = 0, m_nStatLevel = STATTAB_HIGH;
    OpenBlood2::PlayerStats previous{};
    bool initialized = false, have_previous = false;
    static constexpr auto m_hTransColor = OpenBlood2::hud_magenta;
};
