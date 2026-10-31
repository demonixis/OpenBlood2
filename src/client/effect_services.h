#pragma once
#include "client/effect_audio.h"
#include "client/effect_queries.h"
#include "engine/audio.h"
namespace OpenBlood2
{
    class OriginalEffects;
    class SpriteStore;
    struct BloodSplatSettings;
    struct BloodSpurtSettings;
    struct SparkSettings;
    struct WeaponSpriteSettings;
    // One synchronous cosmetic phase. Queries can use a stable client scene, or
    // the immediate local creation bridge; outputs always belong to this client.
    // No original handles, gameplay callbacks or server RNG are exposed.
    struct EffectServices
    {
        EffectQueries& queries;
        OriginalEffects& effects;
        const Resources* resources;
        SpriteStore* sprites;
        EffectAudio* audio;
        double time, frame_time;
        SoundId sound(const SoundParameters&, DVector, std::string&) const;
        std::uint64_t splat(const BloodSplatSettings&, std::string_view, std::string&) const;
        std::uint64_t spurt(const BloodSpurtSettings&, std::string_view, bool, std::string&) const;
        std::uint64_t spark(const SparkSettings&, std::string_view, std::string&) const;
        std::uint64_t weapon_sprite(const WeaponSpriteSettings&, std::string_view, std::string&) const;
    };
} // namespace OpenBlood2
