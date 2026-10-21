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

#define INVTAB_DISP_TIME 5.0f
#define INVTAB_SCROLL_TIME 0.15f

#define INVTAB_INVISIBLE 0
#define INVTAB_SCROLL_IN 1
#define INVTAB_STOPPED 2
#define INVTAB_SCROLL_OUT 3

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
    explicit CNewStatusBar(const OpenBlood2::Resources& resources) : canvas(resources), demo_(resources.demo()) {}
    bool Init();
    bool UpdateCTF(DBYTE team, DBYTE flag);
    const OpenBlood2::HudSurface* team_icon = nullptr;
    const OpenBlood2::HudSurface* flag_icon = nullptr;
    DBYTE team_id = 0, flag_color = 0;
    bool Update(const OpenBlood2::PlayerStats&, unsigned width, unsigned height);
    // Original UpdateInv for one SMSG_ITEMCHANGED (show: ITEMF_SHOWITEMS). Missing item
    // pictures leave that tab empty, as CreateSurfaceFromBitmap's null did; see item_error.
    void UpdateInv(const OpenBlood2::InventoryItems&, bool show, float time);
    // Original DrawInvTabs scroll state at this time, then the tab screen positions.
    struct InventoryLayout
    {
        std::array<bool, 3> visible{}; // Previous, current (with its name), next item tabs.
        std::array<int, 3> x{}, y{};
        bool current_item = false, name = false; // Persistent current-item tab; its name in low detail.
        int name_x = 0, name_y = 0;
    };
    InventoryLayout DrawInvTabs(float time);
    std::array<const StatTab*, 4> item_tabs() const
    {
        return {&m_stItems[0], &m_stItems[1], &m_stItems[2], &m_stCurrentItem};
    }
    const OpenBlood2::HudSurface* item_name() const
    {
        return m_hItemName;
    }
    std::string item_error; // Last item picture or name font failure; never fails the HUD.
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
    void InitInvTabs();
    void AdjustRes(unsigned width, unsigned height);
    void SetupTab(StatTab&, const OpenBlood2::HudFont*, unsigned flags);
    void JustifySurface(const OpenBlood2::HudSurface*, unsigned just, unsigned& x, unsigned& y);
    StatTab m_stHealth{}, m_stArmor{}, m_stAmmo{}, m_stAltAmmo{};
    StatTab m_stItems[3]{}, m_stCurrentItem{};
    std::array<int, 3> item_loc_x{}, item_loc_y{}; // Signed: the side tabs start at x = -15.
    OpenBlood2::HudSurface* m_hItemName = nullptr;
    int m_nItemNameX = 0, m_nItemNameY = 0;
    unsigned m_nInvLevel = INVTAB_INVISIBLE;
    float m_fInvUpdateTime = 0, m_fInvScrollRatio = 0;
    bool inventory_ready = false;
    OpenBlood2::HudFont big_numbers, small_numbers, stat_font;
    unsigned m_nScreenWidth = 0, m_nScreenHeight = 0, m_nStatLevel = STATTAB_HIGH;
    OpenBlood2::PlayerStats previous{};
    bool initialized = false, have_previous = false;
    bool demo_ = false;
    static constexpr auto m_hTransColor = OpenBlood2::hud_magenta;
};
