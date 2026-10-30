#pragma once
#include "game/player_messages.h"
#include <array>
namespace OpenBlood2
{
    // PlayerObj::gCharacterValues and BloodClientShell::SetCharacter.
    // Solo defaults; the retail multiplayer skin table is separate below.
    // Animation-driven body dimensions remain authoritative.
    struct CharacterProfile
    {
        const char* model;
        const char* skin;
        const char* sound_root;
        float height, eye;
        DBYTE strength, speed, resistance, magic;
        char hands;
        bool female;
    };
    inline constexpr std::array<CharacterProfile, 4> character_profiles{
        {{"Models\\Characters\\Caleb.abc", "Skins\\Characters\\Caleb1.dtx", "sounds\\chosen\\caleb", 82, 36, 5, 3, 3, 1,
          'C', false},
         {"Models\\Characters\\Ophelia.abc", "Skins\\Characters\\Ophelia.dtx", "sounds\\chosen\\ophelia", 76, 32, 2, 5,
          1, 4, 'O', true},
         {"Models\\Characters\\Ishmael.abc", "Skins\\Characters\\Ishmael.dtx", "sounds\\chosen\\ishmael", 82, 36, 1, 3,
          3, 5, 'I', false},
         {"Models\\Characters\\Gabriella.abc", "Skins\\Characters\\Gabriella.dtx", "sounds\\chosen\\gabriella", 88, 42,
          5, 1, 5, 1, 'G', true}}};
    inline const CharacterProfile* character_profile(unsigned character)
    {
        return character < character_profiles.size() ? &character_profiles[character] : nullptr;
    }
    // Original PlayerObj::gCharacterSkins. The first entry stays the default solo skin.
    inline constexpr std::array<std::array<const char*, 7>, 4> character_skins{
        {{{"Skins\\Characters\\Caleb1.dtx", "Skins\\Characters\\CalMULTI_R.dtx", "Skins\\Characters\\CalMULTI_B.dtx",
           "Skins\\Characters\\CalMULTI_G.dtx", "Skins\\Characters\\CalMULTI_Y.dtx", "Skins\\Characters\\CalTEAM_B.dtx",
           "Skins\\Characters\\CalTEAM_R.dtx"}},
         {{"Skins\\Characters\\Ophelia.dtx", "Skins\\Characters\\OphMULTI_R.dtx", "Skins\\Characters\\OphMULTI_B.dtx",
           "Skins\\Characters\\OphMULTI_G.dtx", "Skins\\Characters\\OphMULTI_Y.dtx", "Skins\\Characters\\OphTEAM_B.dtx",
           "Skins\\Characters\\OphTEAM_R.dtx"}},
         {{"Skins\\Characters\\Ishmael.dtx", "Skins\\Characters\\IshMULTI_R.dtx", "Skins\\Characters\\IshMULTI_B.dtx",
           "Skins\\Characters\\IshMULTI_G.dtx", "Skins\\Characters\\IshMULTI_Y.dtx", "Skins\\Characters\\IshTEAM_B.dtx",
           "Skins\\Characters\\IshTEAM_R.dtx"}},
         {{"Skins\\Characters\\Gabriella.dtx", "Skins\\Characters\\GabMULTI_R.dtx", "Skins\\Characters\\GabMULTI_B.dtx",
           "Skins\\Characters\\GabMULTI_G.dtx", "Skins\\Characters\\GabMULTI_Y.dtx", "Skins\\Characters\\GabTEAM_B.dtx",
           "Skins\\Characters\\GabTEAM_R.dtx"}}}};
    inline const char* character_skin(unsigned character, unsigned skin)
    {
        return character < character_skins.size() && skin < character_skins[character].size()
                   ? character_skins[character][skin]
                   : nullptr;
    }
    inline bool select_character(unsigned character, PlayerInit& output)
    {
        const auto* p = character_profile(character);
        if (!p)
            return false;
        output = {};
        output.character = DBYTE(character);
        output.strength = p->strength;
        output.speed = p->speed;
        output.resistance = p->resistance;
        output.magic = p->magic;
        return true;
    }
} // namespace OpenBlood2
