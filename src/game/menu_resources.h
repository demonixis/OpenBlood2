#pragma once
#include "engine/resources.h"
#include "engine/string_resources.h"
#include <array>
#include <span>
#include <string_view>

namespace OpenBlood2
{
    enum class MenuEdition
    {
        Retail,
        Demo
    };
    enum class MenuPromotion
    {
        None,
        Bloodbath,
        Info,
        Modes
    };
    struct MenuText
    {
        unsigned id;
        std::string_view text;
    };
    // Finite menu-only edition boundary. No DLL offsets, global string fallback or game localization.
    struct MenuResources
    {
        MenuEdition edition;
        std::array<const char*, 5> titles;
        std::array<const char*, 4> ambience;
        std::array<const char*, 3> cues{"select01.wav", "enter01.wav", "esc01.wav"};
        std::array<const char*, 3> promotions{};
        const char* small_bitmap = "interface/fonts/MenuFont1.pcx";
        const char* small_font = "interface/fonts/MenuFont1.fnt";
        const char* large_font = "interface/mainmenus/mm_font.fnt";
        std::array<const char*, 3> large_bitmaps{"interface/mainmenus/mm_font_01.pcx",
                                                 "interface/mainmenus/mm_font_05.pcx",
                                                 "interface/mainmenus/mm_font_dis.pcx"};
        const char* background = "interface/mainmenus/background.pcx";
        const char* slider = "interface/mainmenus/SliderBar.pcx";
        const char* thumb = "interface/mainmenus/SliderTab.pcx";
        // CMainMenus scroll arrows (CMenuBase::RenderArrows), drawn by the retail Controls page.
        const char* up_arrow = "interface/mainmenus/UpArrow.pcx";
        const char* down_arrow = "interface/mainmenus/DownArrow.pcx";
        int title_x = 82, title_y = 52, list_y = 140;
        const char* title(std::string_view semantic) const;
        const char* promotion(MenuPromotion) const;
    };
    const MenuResources& menu_resources(MenuEdition);
    bool load_menu_strings(const Resources&, StringResources&, std::string& error);
    // Shared validation also permits negative tests without corrupting a commercial DLL.
    bool validate_menu_strings(MenuEdition, const StringResources&, std::string& error);
    bool make_demo_menu_strings(std::span<const MenuText>, StringResources&, std::string& error);
    std::span<const MenuText> demo_menu_text();
} // namespace OpenBlood2
