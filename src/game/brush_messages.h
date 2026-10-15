#pragma once
#include "basedefs_de.h"
#include <string>
namespace OpenBlood2
{
    // Retained diagnostic requests, not rendered debris or simulated fragments.
    struct BrushDestruction
    {
        std::string name;
        DVector position{}, direction{};
        float damage = 0;
        DDWORD debris_amount = 0;
    };
    struct RadiusDamageReport
    {
        unsigned candidates = 0, in_radius = 0, delivered = 0;
        bool cancelled = false;
    };
    struct DebrisEffect
    {
        std::string source;
        DVector position{}, velocity{}, dimensions{};
        DDWORD flags = 0;
        float scale = 1;
        DBYTE amount = 10;
        std::string model, texture, sound;
    };
    struct ExplosionEffect
    {
        std::string source;
        DVector position{}, normal{0, 1, 0};
        DDWORD type = 3; // Source EXP_GRENADE.
        float damage = 100, radius = 200, sound_radius = 1000;
        std::string sound = "Sounds\\Weapons\\c4\\explosion_1.wav";
        RadiusDamageReport targets;
    };
} // namespace OpenBlood2
