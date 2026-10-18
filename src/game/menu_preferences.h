#pragma once
#include "game/menu.h"
namespace OpenBlood2
{
    bool menu_preferences_path(std::filesystem::path&, std::string&);
    // Missing file is the default. Malformed input never partially changes output.
    bool load_menu_preferences(const std::filesystem::path&, MenuSettings&, std::string&);
    bool save_menu_preferences(const std::filesystem::path&, const MenuSettings&, std::string&);
} // namespace OpenBlood2
