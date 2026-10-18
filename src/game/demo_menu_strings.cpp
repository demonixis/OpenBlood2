#include "game/demo_menu_strings.h"
#include "game/original/MenuIds.h"
#include <array>
#include <string_view>
namespace OpenBlood2
{
    namespace
    {
        struct Label
        {
            unsigned id;
            std::size_t offset; // File offset of the NUL-terminated literal in .data.
        };
        // Build order of CMenuMain, CMenuOptions, CMenuSinglePlayer, CMenuDifficulty,
        // CMenuDisplay (detail rows) and CMenuAudio; each literal has one code reference.
        constexpr std::array<Label, 24> labels{
            {{IDS_MENU_MAIN_SINGLEPLAYER, 0x94408},   {IDS_MENU_MAIN_BLOODBATH, 0x94418},
             {IDS_MENU_MAIN_OPTIONS, 0x94424},        {IDS_MENU_MAIN_PRIMEGIBS, 0x9442c},
             {IDS_MENU_MAIN_QUIT, 0x94438},           {IDS_MENU_OPTIONS_DISPLAY, 0x944a8},
             {IDS_MENU_OPTIONS_AUDIO, 0x944b0},       {IDS_MENU_OPTIONS_CONTROLS, 0x944b8},
             {IDS_MENU_OPTIONS_MOUSE, 0x944c4},       {IDS_MENU_OPTIONS_JOYSTICK, 0x944cc},
             {IDS_MENU_OPTIONS_KEYBOARD, 0x944d8},    {IDS_MENU_SINGLEPLAYER_CALEB, 0x94530},
             {IDS_MENU_SINGLEPLAYER_CHOSEN, 0x94538}, {IDS_MENU_DIFFICULTY_GENOCIDE, 0x94104},
             {IDS_MENU_DIFFICULTY_HOMICIDE, 0x94110}, {IDS_MENU_DIFFICULTY_SUICIDE, 0x9411c},
             {IDS_MENU_DISPLAY_DETAIL_LOW, 0x94170},  {IDS_MENU_DISPLAY_DETAIL_MEDIUM, 0x94184},
             {IDS_MENU_DISPLAY_DETAIL_HIGH, 0x9419c}, {IDS_MENU_AUDIO_SOUND, 0x9457c},
             {IDS_MENU_AUDIO_SOUND_VOLUME, 0x94584},  {IDS_MENU_AUDIO_16BIT_SOUND, 0x94594},
             {IDS_MENU_AUDIO_MUSIC, 0x945a4},         {IDS_MENU_AUDIO_MUSIC_VOLUME, 0x945ac}}};
        constexpr std::size_t build_size = 719872, version_offset = 0x932d4;
        constexpr std::string_view build_version = "DEMO v1.0 (Build 108)";
        // A literal starts after padding/NUL and is printable ASCII ending in NUL.
        bool literal(std::span<const std::uint8_t> bytes, std::size_t offset, std::string& out)
        {
            out.clear();
            if (offset == 0 || offset >= bytes.size() || bytes[offset - 1] != 0)
                return false;
            for (std::size_t i = offset; i < bytes.size() && i - offset <= 64; ++i)
            {
                if (bytes[i] == 0)
                    return !out.empty();
                if (bytes[i] < 0x20 || bytes[i] > 0x7e)
                    return false;
                out.push_back(char(bytes[i]));
            }
            return false;
        }
    } // namespace
    bool read_demo_menu_strings(std::span<const std::uint8_t> cshell, StringResources& output, std::string& error)
    {
        output = {};
        error.clear();
        std::string text;
        if (cshell.size() != build_size || !literal(cshell, version_offset, text) || text != build_version)
        {
            error = "Unsupported demo cshell.dll build; menu labels are mapped for DEMO v1.0 (Build 108) only";
            return false;
        }
        StringResources strings;
        for (const auto& label : labels)
        {
            if (!literal(cshell, label.offset, text))
            {
                error = "Malformed demo menu label for string " + std::to_string(label.id);
                return false;
            }
            strings.strings.emplace(label.id, std::move(text));
        }
        output = std::move(strings);
        return true;
    }
} // namespace OpenBlood2
