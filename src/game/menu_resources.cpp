#include "game/menu_resources.h"
#include "game/original/MenuIds.h"
#include <algorithm>

namespace OpenBlood2
{
    namespace
    {
        // ClientRes.rc semantic IDs, with reviewed Build 108 literal casing/wording.
        // Imported source text, not extracted commercial bytes; see demo_menu_sources.json.
        constexpr MenuText demo_text[]{{500, "SINGLE PLAYER"},
                                       {501, "BLOODBATH"},
                                       {502, "OPTIONS"},
                                       {503, "PRIME GIBS"},
                                       {505, "QUIT"},
                                       {513, "GENOCIDE"},
                                       {514, "HOMICIDE"},
                                       {515, "SUICIDE"},
                                       {516, "DETAIL LEVEL [LOW]"},
                                       {517, "DETAIL LEVEL [MEDIUM]"},
                                       {518, "DETAIL LEVEL [HIGH]"},
                                       {521, "DISPLAY"},
                                       {522, "AUDIO"},
                                       {523, "CONTROLS"},
                                       {524, "MOUSE"},
                                       {525, "JOYSTICK"},
                                       {526, "KEYBOARD"},
                                       {527, "CALEB"},
                                       {528, "CHOSEN"},
                                       {532, "Sound"},
                                       {533, "Sound Volume"},
                                       {534, "16 Bit Sounds"},
                                       {535, "Music"},
                                       {536, "Music Volume"},
                                       {537, "Sensitivity"},
                                       {538, "Input [Normal]"},
                                       {539, "Invert Y-Axis"}};
        constexpr unsigned common_ids[]{500, 501, 502, 505, 513, 514, 515, 516, 517, 518, 521, 522, 523,
                                        524, 525, 526, 527, 528, 532, 533, 534, 535, 536, 537, 538, 539};
        constexpr unsigned retail_ids[]{504, 509, 510, 511, 512, 529, 530, 531, 540, 541, 547, 548, 561, 600,
                                        // CMenuControls: g_nControlActionStringID (733/734 unused) and
                                        // IDS_MENU_KEY_UNASSIGNED.
                                        705, 706, 707, 708, 709, 710, 711, 712, 713, 714, 715, 716, 717, 718,
                                        719, 720, 721, 722, 723, 724, 725, 726, 727, 728, 729, 730, 731, 732,
                                        735, 736, 737, 738, 739, 740, 741, 742, 743, 744, 745, 746, 747, 748,
                                        749};
    } // namespace
    const MenuResources& menu_resources(MenuEdition edition)
    {
        static const MenuResources retail{MenuEdition::Retail,
                                          {"interface/mainmenus/mainmenu.pcx", "interface/mainmenus/singleplayer.pcx",
                                           "interface/mainmenus/options.pcx", "interface/mainmenus/loadgame.pcx",
                                           "interface/mainmenus/savegame.pcx"},
                                          {"ambient01.wav", "ambient02.wav", "ambient03.wav", "ambient04.wav"}};
        static const MenuResources demo{MenuEdition::Demo,
                                        {"interface/mainmenus/mainmenu_title.pcx",
                                         "interface/mainmenus/singlepla_title.pcx",
                                         "interface/mainmenus/options_title.pcx", nullptr, nullptr},
                                        {"ambient02.wav", "ambient02.wav", "ambient04.wav", "ambient04.wav"},
                                        {"select01.wav", "enter01.wav", "esc01.wav"},
                                        {"screens/bloodbath.pcx", "screens/info.pcx", "screens/modes.pcx"}};
        // _ADDON MainMenus/MenuBase: the add-on background behind every page.
        static const MenuResources nightmares = []
        {
            auto value = retail;
            value.edition = MenuEdition::Nightmares;
            value.background = "interface_ao/background.pcx";
            return value;
        }();
        return edition == MenuEdition::Demo ? demo : edition == MenuEdition::Nightmares ? nightmares : retail;
    }
    const char* MenuResources::title(std::string_view semantic) const
    {
        constexpr std::string_view keys[]{"mainmenu.pcx", "singleplayer.pcx", "options.pcx", "loadgame.pcx",
                                          "savegame.pcx"};
        for (unsigned i = 0; i < titles.size(); ++i)
            if (semantic == keys[i])
                return titles[i];
        return nullptr;
    }
    const char* MenuResources::promotion(MenuPromotion value) const
    {
        const auto i = static_cast<unsigned>(value);
        return i >= 1 && i <= promotions.size() ? promotions[i - 1] : nullptr;
    }
    std::span<const MenuText> demo_menu_text()
    {
        return demo_text;
    }
    bool validate_menu_strings(MenuEdition edition, const StringResources& strings, std::string& error)
    {
        error.clear();
        const auto require = [&](unsigned id)
        {
            auto i = strings.strings.find(id);
            if (i == strings.strings.end() || i->second.empty() || i->second.size() > 256 ||
                !std::all_of(i->second.begin(), i->second.end(), [](unsigned char c) { return c >= 32 && c <= 126; }))
            {
                error = "Missing/unsupported menu string " + std::to_string(id);
                return false;
            }
            return true;
        };
        for (auto id : common_ids)
            if (!require(id))
                return false;
        if (edition == MenuEdition::Demo)
            return require(IDS_MENU_MAIN_PRIMEGIBS);
        for (auto id : retail_ids)
            if (!require(id))
                return false;
        return edition != MenuEdition::Nightmares || require(IDS_MENU_SINGLEPLAYER_NIGHTMARES);
    }
    bool make_demo_menu_strings(std::span<const MenuText> text, StringResources& out, std::string& error)
    {
        out = {};
        StringResources candidate;
        candidate.language = 1033; // Reviewed English source/demo edition, not automatic language detection.
        for (const auto& entry : text)
        {
            if (std::none_of(std::begin(demo_text), std::end(demo_text),
                             [&](const auto& known) { return known.id == entry.id; }) ||
                !candidate.strings.emplace(entry.id, entry.text).second)
            {
                error = "Unknown/duplicate demo menu string " + std::to_string(entry.id);
                return false;
            }
        }
        if (!validate_menu_strings(MenuEdition::Demo, candidate, error))
            return false;
        out = std::move(candidate);
        return true;
    }
    bool load_menu_strings(const Resources& resources, StringResources& out, std::string& error)
    {
        out = {};
        if (resources.demo())
            return make_demo_menu_strings(demo_text, out, error);
        std::vector<std::uint8_t> bytes;
        const auto* dll = resources.find("cres.dll");
        StringResources candidate;
        if (!dll || dll->size > 32 * 1024 * 1024)
        {
            error = "Missing/oversized menu strings: cres.dll";
            return false;
        }
        if (!resources.read("cres.dll", bytes, error) || !read_string_resources(bytes, candidate, error) ||
            !validate_menu_strings(menu_edition(resources), candidate, error))
            return false;
        out = std::move(candidate);
        return true;
    }
} // namespace OpenBlood2
