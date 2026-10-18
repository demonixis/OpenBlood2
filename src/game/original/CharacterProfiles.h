#pragma once
#include "game/player_messages.h"
#include <array>
namespace OpenBlood2
{
    // PlayerObj::gCharacterValues and BloodClientShell::SetCharacter.
    // Default skins only; animation-driven body dimensions remain authoritative.
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
