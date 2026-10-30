#include "client/effect_services.h"
#include "client/audio.h"
#include "client/original_effects.h"
namespace OpenBlood2
{
    SoundId EffectServices::sound(const SoundParameters& request, DVector position, std::string& error) const
    {
        error.clear();
        if (!audio || !resources)
        {
            error = "Client cosmetic audio receiver/resources unavailable";
            return 0;
        }
        return audio->play_effect(*resources, request, position, error);
    }
    std::uint64_t EffectServices::splat(const BloodSplatSettings& settings, std::string_view source,
                                        std::string& error) const
    {
        error.clear();
        if (!sprites)
        {
            error = "Blood splat store unavailable";
            return 0;
        }
        return effects.blood_splats.publish(settings, source, *sprites, time, error);
    }
    std::uint64_t EffectServices::spurt(const BloodSpurtSettings& settings, std::string_view source, bool immediate,
                                        std::string& error) const
    {
        error.clear();
        if (!resources)
        {
            error = "Blood spurt resources unavailable";
            return 0;
        }
        return effects.blood_spurts.publish(settings, source, immediate, *resources, *this, time, error);
    }
    std::uint64_t EffectServices::spark(const SparkSettings& settings, std::string_view source,
                                        std::string& error) const
    {
        error.clear();
        if (!resources)
        {
            error = "Spark resources unavailable";
            return 0;
        }
        return effects.sparks.publish(settings, source, *resources, time, error);
    }
    std::uint64_t EffectServices::weapon_sprite(const WeaponSpriteSettings& settings, std::string_view source,
                                                std::string& error) const
    {
        error.clear();
        if (!sprites)
        {
            error = "Weapon sprite store unavailable";
            return 0;
        }
        return effects.weapon_sprites.publish(settings, source, *sprites, time, error);
    }
} // namespace OpenBlood2
