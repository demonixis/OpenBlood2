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
        bool null_damager = false; // Original DamageObject guard suppresses delivery.
    };
    struct SceneExplosionReport
    {
        std::string source, sound, sound_error;
        DVector position{}, normal{};
        DDWORD type = 0;
        float damage = 0, radius = 0;
        RadiusDamageReport targets;
        std::uint64_t voice = 0;
        std::uint64_t fragment_event = 0;
        std::string fragment_error;
        std::uint64_t flame_event = 0;
        std::string flame_error;
        std::uint64_t ring_event = 0;
        std::string ring_error;
        std::uint64_t light_event = 0;
        std::string light_error;
        std::uint64_t model_event = 0;
        std::string model_error;
        std::uint64_t sprite_event = 0;
        std::string sprite_error;
        // Selected components execute; illumination and complete composite remain unavailable.
        bool client_fx_available = false;
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
