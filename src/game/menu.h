#pragma once
#include "engine/hud_surface.h"
#include "engine/string_resources.h"
#include "game/menu_resources.h"
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
        Settings
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
        bool operator==(const MenuSettings&) const = default;
    };
    struct MenuItem
    {
        unsigned string_id = 0;
        int command = -1;
        bool enabled = true;
        std::string literal;
        // Setting IDs: sound, volume, music, volume, sensitivity, invert Y, detail, lighting,
        // renderer, normal maps.
        int setting = -1;
        HudRect rect{}, slider_rect{};
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
        bool audio_failed = false; // Nonmodal application-owned audio status.
        MenuPage page = MenuPage::Main;
        std::vector<MenuItem> items;
        std::vector<MenuCue> cues;
        MenuSettings settings;
        std::string error;
        int selected = 0;
        unsigned character = 0;
        bool story = true;
        std::uint64_t revision = 0;
        bool enabled(const MenuItem& item) const
        {
            return item.enabled && !(item.setting == 1 && !settings.sound) && !(item.setting == 3 && !settings.music) &&
                   !(item.setting == 9 && !settings.renderer);
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
        std::map<std::string, const HudSurface*> titles_;
        std::map<MenuPromotion, const HudSurface*> promotions_;
        std::uint64_t composed_ = UINT64_MAX;
    };
} // namespace OpenBlood2
