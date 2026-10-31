#pragma once
#include "game/ballistic_settings.h"
#include <optional>
#include <string>
#include <variant>
namespace OpenBlood2
{
    struct ExplosionSpriteSettings
    {
        DVector position{}, normal{0, 1, 0}, scale1{}, scale2{1, 1, 1};
        float duration = 1, alpha = 1;
        DBYTE waveform = 0, fade = 0, align = 0;
        // Null uses source default; an authored empty string is not null.
        std::optional<std::string> resource;
    };
    struct SmokeImpactSettings
    {
        DVector position{}, normal{}, velocity{}, deceleration{}, gravity{}, scale1{}, scale2{}, color{};
        float offset = 0, duration = .3f, delay = 0, alpha = 0;
        bool fade = false, rotate = false, move = false, update_scale = true;
        std::optional<std::string> resource;
    };
    // Selected original metal/flesh sparks, without generic fade-color mode.
    struct SparkSettings
    {
        DVector position{}, direction{}, color1{}, color2{};
        unsigned count = 0;
        float duration = 0, emission_radius = 0, radius = 0, gravity = 0;
        std::string texture;
    };
    // Original flesh recipe: one non-bouncing, half-gravity emitter.
    struct BloodSpurtSettings
    {
        DVector position{}, min_velocity{}, max_velocity{};
        float lifetime = 2, fade_time = .5f;
        unsigned texture = 1;
    };
    struct BloodSplatSettings
    {
        DVector position{};
        DRotation rotation{};
        float scale = .1f, grow_scale = .02f;
        std::optional<std::string> resource;
    };
    struct WeaponSpriteSettings
    {
        std::optional<ExplosionSpriteSettings> flash;
        std::optional<SmokeImpactSettings> smoke;
        bool bubbles_unavailable = false;
    };
    struct BloodSpurtRecipe
    {
        BloodSpurtSettings settings;
        bool immediate = false;
    };
    enum class ExplosionComponent : std::uint8_t
    {
        Models,
        Sprites,
        Rings,
        Fragments,
        Flames,
        Lights
    };
    struct ExplosionRecipe
    {
        ExplosionComponent component = ExplosionComponent::Models;
        std::uint32_t type = 0;
        DVector position{}, normal{0, 1, 0};
    };
    struct FleshRecipe
    {
        DVector position{}, velocity{}, dimensions{};
        float scale = 1;
        unsigned count = 0;
    };
    // Only original message inputs, never report IDs/errors or decoded assets.
    struct DebrisRecipe
    {
        DVector position{}, velocity{}, dimensions{};
        std::uint32_t flags = 0;
        float scale = 1;
        std::uint8_t amount = 10;
        std::string model, texture, sound;
    };
    struct LaserRecipe
    {
        DVector source{}, destination{};
        std::uint8_t type = 0;
    };
    struct LightningSettings
    {
        DVector source{}, destination{};
        DBYTE shape = 2, form = 1, type = 2;
    };
    using InstantEffectRecipe =
        std::variant<ShotgunEffectSettings, WeaponSpriteSettings, SparkSettings, BloodSpurtRecipe, BloodSplatSettings,
                     ExplosionRecipe, FleshRecipe, DebrisRecipe, LaserRecipe, LightningSettings>;
} // namespace OpenBlood2
