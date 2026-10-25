#pragma once
#include "engine/hud_surface.h"
#include "engine/string_resources.h"
#include "game/menu_resources.h"
#include <array>
#include <optional>

namespace OpenBlood2
{
    enum class MenuPage
    {
        Main,
        Solo,
        Character,
        Difficulty,
        Options,
        Audio,
        Mouse,
        Display,
        Controls,
        Vr,     // Replacement Options > VR page (XR sessions only).
        Cheats, // Replacement Options > CHEATS page.
        Save,
        Load,
        Quit
    };
    enum class MenuAction
    {
        None,
        Resume,
        NewGame,
        Credits, // Original MenuCredits: StartNewGame("Credits", CUSTOM, MEDIUM), or GS_CREDITS in a world.
        Quit,
        Settings,
        SaveGame,        // Original MenuSaveGame(slot): slot 0..7.
        LoadGame,        // Original MenuLoadGame(slot): SLOT_CURRENT (-2, Restart), SLOT_QUICK (-1) or 0..7.
        RestoreControls, // Replacement Controls row: the owner restores the default bindings.
        Setup,           // Replacement Options row: the owner shows the game data setup screen.
        Cheat,           // Replacement Cheats row: the owner sends CMSG_CHEAT (MenuCommand::cheat).
        Warp             // Replacement Cheats row: the owner starts a new game in MenuCommand::world.
    };
    enum class MenuCue
    {
        Select,
        Enter,
        Back
    };
    struct MenuCommand
    {
        MenuAction action = MenuAction::None;
        unsigned character = 0, difficulty = 1;
        bool story = true;
        int slot = 0;      // SaveGame/LoadGame only.
        int cheat = -1;    // Cheat only: the original CheatCodes value (game_settings.h).
        std::string world; // Warp only: the world to start.
    };
    // The owner's slot listing (CSavedGameInfo): names as the original menus print them, and
    // whether a row can be activated. Load rows are enabled for readable and unreadable files, so
    // an unreadable row reports its reason; empty rows are disabled.
    struct MenuSaveRow
    {
        std::string name = "<EMPTY SLOT>";
        bool loadable = false;
        bool operator==(const MenuSaveRow&) const = default;
    };
    struct MenuSaveSlots
    {
        MenuSaveRow restart, quick;
        std::array<MenuSaveRow, 8> slots;
        std::string unavailable; // Nonempty when saves cannot be used at all (shown on both pages).
        bool operator==(const MenuSaveSlots&) const = default;
    };
    // One CMenuControls row from the owner's bindings: the original action string (or a
    // replacement literal) and the bound sources as text ("W or MBUTTON 1"). Empty text shows the
    // original unassigned string; a disabled row is an original action without a port.
    struct MenuControlRow
    {
        unsigned string_id = 0;
        std::string literal;
        std::string binding;
        bool enabled = true;
        bool operator==(const MenuControlRow&) const = default;
    };
    struct MenuVideoCapabilities
    {
        bool window = false, vsync_off = false;
        std::vector<std::array<int, 2>> resolutions{{0, 0}};
        std::vector<int> frame_limits{30, 60, 120, 240, 0};
        // Millihertz preserves fractional runtime rates without floats in preferences.
        std::vector<int> refresh_rates{0};
        int current_refresh_rate = 0;
    };
    struct MenuSettings
    {
        bool sound = true, music = true, invert_y = false;
        // Original defaults.cfg: SoundVolume 50, MusicVolume 35 (BloodClientShell's fallback is also 35).
        int sound_volume = 50, music_volume = 35, sensitivity = 150;
        // Original Display DETAIL LEVEL (0 low, 1 medium, 2 high; detail*.cfg LightMap and
        // MaxModelShadows) and the replacement per-pixel model lighting option.
        int detail = 2;
        bool pixel_lighting = false;
        // Replacement renderer rows: 0 original shaders, 1 enhanced (physically based); the
        // enhanced lighting quality (0 standard, 1 high) and generated normal maps.
        int renderer = 0, quality = 0;
        bool normal_maps = true;
        // Replacement post-processing rows: anti-aliasing for both renderers (0 off, 1 FXAA,
        // 2 MSAA 2X, 3 MSAA 4X); bloom, the tonemap (0 off, 1 neutral, 2 filmic) and ambient
        // occlusion and screen-space reflections (0 off, 1 low, 2 high) for the enhanced renderer
        // only.
        int antialiasing = 0, tonemap = 1, ambient_occlusion = 2, reflections = 2;
        bool bloom = true;
        // Replacement VR rows: the snap-turn step in degrees (15, 30, 45, 60 or 90; 0 turns
        // smoothly), the aim laser, the aim reticle, and movement relative to the right hand
        // instead of the head.
        int vr_turn = 45;
        bool vr_laser = true, vr_crosshair = true, vr_move_hand = false;
        // Replacement Cheats rows: the master switch and god mode, which applies to the entered
        // player only while the switch is on.
        bool cheats = false, god_mode = false;
        bool fullscreen = false, vsync = true;
        int fullscreen_mode = 0;                         // 0 desktop borderless, 1 exclusive.
        int resolution_width = 0, resolution_height = 0; // 0/0: default window / desktop mode.
        int fps_limit = 0;                               // Software ceiling; zero leaves pacing to presentation.
        int vr_refresh_rate = 0;                         // Millihertz; zero asks the runtime to choose.
        int vr_render_scale = 100;                       // Percentage of recommended eye dimensions (50..100, step 10).

        bool operator==(const MenuSettings&) const = default;
    };
    struct MenuItem
    {
        unsigned string_id = 0;
        int command = -1;
        bool enabled = true;
        std::string literal;
        // Setting IDs: sound, volume, music, volume, sensitivity, invert Y, detail, lighting,
        // renderer, normal maps, VR turn, VR laser, VR crosshair, VR movement reference, cheats,
        // god mode, cheat level, anti-aliasing, bloom, tonemap, ambient occlusion, reflections,
        // fullscreen, fullscreen mode, resolution, vsync, frame limit, VR refresh rate, VR render scale.
        int setting = -1;
        HudRect rect{}, slider_rect{};
        std::string insertion; // FormatMessage %1!s! argument of the Load page's RESTART/QUICKLOAD strings.
        int cheat = -1;        // A Cheats action row's CheatCodes value.
    };
    // Typed service adapter for the selected original menu Build/OnCommand code.
    // No original client ABI or hidden dependency on ImGui/SDL input.
    class Menu
    {
      public:
        explicit Menu(MenuEdition edition = MenuEdition::Retail) : edition_(edition) {}
        bool demo() const
        {
            return edition_ == MenuEdition::Demo;
        }
        void open(bool in_world, bool alive = true);
        // Original single-player death USE: SetCurrentMenu(MENU_ID_LOAD_GAME, MENU_ID_LOAD_GAME), so
        // the Load page is the top level and Esc returns to the game.
        void open_load(bool in_world, bool alive);
        // Replaces the slot listing; a visible Save or Load page is rebuilt, keeping its selection.
        void set_saves(const MenuSaveSlots&);
        MenuSaveSlots saves;
        // Replaces the Controls rows; a visible Controls page is rebuilt, keeping its selection.
        void set_controls(std::vector<MenuControlRow>);
        std::vector<MenuControlRow> controls;
        // CMenuControls: the row tracking the next device input (its "=" column), or -1. The owner
        // binds or cancels; Back cancels too.
        int binding = -1;
        int first = 0; // First displayed row of a scrolled list (CLTGUIListCtrl start index).
        void show_promotion(MenuPromotion, bool exit = false);
        void dismiss_promotion();
        void end_demo();
        MenuPromotion promotion = MenuPromotion::None;
        bool promotion_exit = false;
        void close();
        void build(MenuPage);
        void back();
        void select(int direction);
        void activate();
        void adjust(int direction);
        void pointer(float x, float y, bool press, bool release, bool motion);
        void cancel_pointer()
        {
            pressed_ = -1;
        }
        MenuCommand take_command();
        bool active = false, in_world = false, alive = true;
        bool vr = false;    // An XR session: Options offers the replacement VR page.
        bool setup = false; // The owner offers Options > SETUP (the game data setup screen).
        // The Cheats page's LEVEL row: the owner's campaign worlds and the displayed one.
        void set_levels(std::vector<std::string>);
        std::vector<std::string> levels;
        int level = 0;
        std::string cheat_status;  // The last cheat's original message line, shown on the Cheats page.
        bool audio_failed = false; // Nonmodal application-owned audio status.
        MenuPage page = MenuPage::Main, top = MenuPage::Main;
        std::vector<MenuItem> items;
        std::vector<MenuCue> cues;
        MenuSettings settings;
        MenuVideoCapabilities video;
        std::string error;
        int selected = 0;
        unsigned character = 0;
        bool story = true;
        std::uint64_t revision = 0;
        bool enabled(const MenuItem& item) const
        {
            return item.enabled && !(item.setting == 1 && !settings.sound) && !(item.setting == 3 && !settings.music) &&
                   !((item.setting == 9 || (item.setting >= 18 && item.setting <= 21)) && !settings.renderer) &&
                   !((item.setting >= 22 && item.setting <= 24) && (!video.window || vr)) &&
                   !(item.setting == 23 && !settings.fullscreen) &&
                   !(item.setting == 24 && settings.fullscreen && settings.fullscreen_mode == 0) &&
                   !(item.setting == 25 && (vr || !video.vsync_off)) && !(item.setting == 26 && vr) &&
                   !(item.setting == 27 && video.refresh_rates.size() < 2) &&
                   !(page == MenuPage::Cheats && item.setting != 14 && !settings.cheats);
        }
        bool small() const;
        const char* title() const;

      private:
        const MenuEdition edition_;
        void AddLargeTextItemOption(unsigned string_id, int command);
        void setting(unsigned string_id, int id);
        void cue(MenuCue);
        int pressed_ = -1;
        MenuCommand pending_;
    };
    struct MenuViewport
    {
        float x = 0, y = 0, scale = 1;
        static MenuViewport fit(float width, float height);
        bool point(float& px, float& py) const;
    };
    class MenuCanvas
    {
      public:
        explicit MenuCanvas(const Resources& resources) : canvas(resources), resources_(resources) {}
        bool initialize(std::string& error);
        bool compose(Menu&, double ui_seconds, std::string& error);
        HudCanvas canvas;
        HudSurface* surface = nullptr;
        StringResources strings;
        std::uint64_t revision = 0;

      private:
        const Resources& resources_;
        HudFont small_, normal_, selected_, disabled_;
        const HudSurface* background_ = nullptr;
        const HudSurface* slider_ = nullptr;
        const HudSurface* thumb_ = nullptr;
        const HudSurface* up_arrow_ = nullptr;
        const HudSurface* down_arrow_ = nullptr;
        bool compose_controls(Menu&, std::string& error);
        std::map<std::string, const HudSurface*> titles_;
        std::map<MenuPromotion, const HudSurface*> promotions_;
        std::uint64_t composed_ = UINT64_MAX;
    };
} // namespace OpenBlood2
