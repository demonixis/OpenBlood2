// Selective original Blood II CNewStatusBar port. See original_hud_sources.json.
#pragma once
#include "engine/hud_surface.h"
#include "game/player_messages.h"
#include <functional>
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

#define WEAPTAB_SLOTS 10
#define WEAPTAB_SCROLL_TIME 0.25f

#define WEAPTAB_INVISIBLE 0
#define WEAPTAB_SCROLL_IN 1
#define WEAPTAB_STOPPED 2
#define WEAPTAB_SCROLL_OUT 3

#define OBJTAB_SCALE_TIME 0.10f
#define OBJTAB_SCALE_LEVELS 10

#define OBJTAB_INVISIBLE 0
#define OBJTAB_SCALE_UP 1
#define OBJTAB_STOPPED 2
#define OBJTAB_SCALE_DOWN 3

#define OBJTAB_TITLE_CENTER 135
#define OBJTAB_TITLE_TOP 35
#define OBJTAB_TEXT_LEFT 35
#define OBJTAB_TEXT_TOP 50
#define OBJTAB_TEXT_WIDTH 200

#define OBJICON_INVISIBLE 0
#define OBJICON_VISIBLE 1

#define BARTAB_DISP_TIME 5.0f

#define BARTAB_NONE 0
#define BARTAB_THROW 1
#define BARTAB_AIR 2
#define BARTAB_BOSS 3

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
    using TextPainter = std::function<void(OpenBlood2::HudSurface&, int font_height)>;
    void UpdateInv(const OpenBlood2::InventoryItems&, bool show, float time, const TextPainter& name_painter = {},
                   bool update_scroll = true);
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
    // Original ToggleWeapons (shell F1) and DrawWeaponTabs: the ten weapon slots slide in from the
    // right edge, high graphics only. Missing artwork leaves a slot empty (weapon_error).
    void ToggleWeapons(float time);
    void ResetWeapons(); // Original Reset on a level change: m_nWeapLevel = 0.
    struct WeaponLayout
    {
        bool visible = false;
        int x = 0;
        std::array<int, WEAPTAB_SLOTS> y{};
    };
    WeaponLayout DrawWeaponTabs(float time);
    const StatTab& weapon_tab(unsigned slot) const
    {
        return m_stWeapons[slot];
    }
    unsigned weapon_level() const
    {
        return m_nWeapLevel;
    }
    std::string weapon_error;
    // Original Reset on a level change (OnEnterWorld): weapon tabs, objectives page, the
    // updated-objectives icon and the power bar hide. The page keeps its last text.
    void Reset();
    // Original UpdateObjectives/InitObjectivesTab: null title or text (`valid` false) resets the
    // page to "No Current Objective" and hides the icon. Missing art or font leaves the page
    // unavailable (objectives_error), never failing the HUD.
    void UpdateObjectives(bool valid, const std::string& title, const std::string& text,
                          const TextPainter& title_painter = {}, const TextPainter& body_painter = {},
                          bool update_icon = true);
    // Original ToggleObjectives (shell Tab): true when the source plays missionobj.wav.
    bool ToggleObjectives(float time);
    struct ObjectivesLayout
    {
        bool page = false, icon = false;
        OpenBlood2::HudRect rect{}; // DrawObjectives' ScaleSurfaceToSurface destination.
        int icon_x = 0, icon_y = 0;
    };
    // Original DrawObjectives (scale up/down over 0.1 s) and DrawObjectivesIcon.
    ObjectivesLayout DrawObjectives(float time);
    const OpenBlood2::HudSurface* objectives_page() const
    {
        return m_hObjectives;
    }
    const OpenBlood2::HudSurface* objectives_icon() const
    {
        return m_hObjectivesIcon;
    }
    unsigned objectives_level() const
    {
        return m_nObjLevel;
    }
    std::string objectives_error;
    // Original SetPowerBarLevel/TurnOffPowerBar/DrawPowerBar: a 150x16 surface centred above the
    // ammunition tab, hidden five seconds after the last update. Missing art: power_error.
    void SetPowerBarLevel(DBYTE nBar, float fPercent, float time);
    void TurnOffPowerBar()
    {
        m_nPowerBar = BARTAB_NONE;
    }
    struct PowerBarLayout
    {
        bool visible = false;
        int x = 0, y = 0;
    };
    PowerBarLayout DrawPowerBar(float time);
    const OpenBlood2::HudSurface* power_bar() const
    {
        return m_hPowerBar;
    }
    DBYTE power_bar_type() const
    {
        return m_nPowerBar;
    }
    std::string power_error;
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
    void InitWeaponTabs();
    void InitPanels();
    bool InitObjectivesTab(const TextPainter& title_painter = {});
    void UpdateWeapons(const OpenBlood2::PlayerStats&);
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
    StatTab m_stWeapons[WEAPTAB_SLOTS]{};
    unsigned m_nCurrentWeapon = 0;
    unsigned m_nWeapLevel = WEAPTAB_INVISIBLE;
    float m_fWeapUpdateTime = 0, m_fWeapScrollRatio = 0;
    bool weapons_ready = false;
    OpenBlood2::HudSurface *m_hObjectives = nullptr, *m_hObjectivesIcon = nullptr, *m_hPowerBar = nullptr;
    const OpenBlood2::HudSurface *objectives_background = nullptr, *m_hHealthBar = nullptr, *m_hThrowBar = nullptr;
    OpenBlood2::HudFont objectives_font;
    unsigned m_nObjLevel = OBJTAB_INVISIBLE, m_nObjIcon = OBJICON_INVISIBLE;
    float m_fObjUpdateTime = 0, m_fObjScaleRatio = 0;
    OpenBlood2::HudRect m_rObjectives{};
    unsigned m_nObjectivesX = 100, m_nObjectivesY = 75;
    DBYTE m_nPowerBar = BARTAB_NONE;
    float m_fPowerBarUpdate = 0;
    int m_nPowerBarX = 0, m_nPowerBarY = 0; // Signed: no DDWORD wrap below 150 pixels.
    int power_fill = -1;                    // Last composed fill width and bar (revision only when pixels change).
    DBYTE power_drawn = BARTAB_NONE;
    OpenBlood2::HudFont big_numbers, small_numbers, stat_font;
    unsigned m_nScreenWidth = 0, m_nScreenHeight = 0, m_nStatLevel = STATTAB_HIGH;
    OpenBlood2::PlayerStats previous{};
    bool initialized = false, have_previous = false;
    bool demo_ = false;
    static constexpr auto m_hTransColor = OpenBlood2::hud_magenta;
};
