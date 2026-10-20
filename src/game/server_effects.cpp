#include "game/server_adapter.h"
#include <cmath>
namespace OpenBlood2
{
    std::uint64_t ServerAdapter::PublishWarpGate(HOBJECT owner, const WarpGateSettings& settings, std::string& error)
    {
        if (!trace_service("PublishWarpGate"))
            return 0;
        if (!client_resources || !objects_.sprites)
        {
            error = "Warp resources unavailable";
            return 0;
        }
        return warp_gates.publish(owner, settings, *this, *objects_.sprites, *client_resources, objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishLightning(HOBJECT source, const LightningSettings& settings, std::string& error)
    {
        if (!trace_service("PublishLightning"))
            return 0;
        if (!IsObjectAlive(source) || !client_resources)
        {
            error = "Lightning owner/resources unavailable";
            return 0;
        }
        return lightning.publish(settings, GetObjectName(source), *client_resources, objects_.time(), error);
    }

    std::uint64_t ServerAdapter::PublishSparks(HOBJECT source, const SparkSettings& settings, std::string& error)
    {
        if (!trace_service("PublishSparks"))
            return 0;
        error.clear();
        if (!IsObjectAlive(source) || !client_resources)
        {
            error = "Spark source/resources unavailable";
            return 0;
        }
        return sparks.publish(settings, GetObjectName(source), *client_resources, objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishClientBloodSplat(const BloodSplatSettings& settings, std::string_view source,
                                                         std::string& error)
    {
        if (!trace_service("PublishClientBloodSplat"))
            return 0;
        error.clear();
        if (!objects_.sprites)
        {
            error = "Blood splat store unavailable";
            return 0;
        }
        return blood_splats.publish(settings, source, *objects_.sprites, objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishClientBloodSpurt(const BloodSpurtSettings& settings, std::string_view source,
                                                         bool immediate, std::string& error)
    {
        if (!trace_service("PublishClientBloodSpurt"))
            return 0;
        error.clear();
        if (!client_resources)
        {
            error = "Blood spurt resources unavailable";
            return 0;
        }
        return blood_spurts.publish(settings, source, immediate, *client_resources, *this, objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishBloodSpurt(HOBJECT source, const BloodSpurtSettings& settings,
                                                   std::string& error)
    {
        if (!trace_service("PublishBloodSpurt"))
            return 0;
        error.clear();
        if (!IsObjectAlive(source))
        {
            error = "Blood spurt source unavailable";
            return 0;
        }
        return PublishClientBloodSpurt(settings, GetObjectName(source), false, error);
    }
    std::uint64_t ServerAdapter::PublishBloodSplat(HOBJECT source, const BloodSplatSettings& settings,
                                                   std::string& error)
    {
        if (!trace_service("PublishBloodSplat"))
            return 0;
        error.clear();
        if (!IsObjectAlive(source) || !objects_.sprites)
        {
            error = "Blood splat source/store unavailable";
            return 0;
        }
        return blood_splats.publish(settings, GetObjectName(source), *objects_.sprites, objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishFleshGibs(HOBJECT owner, DVector pos, DVector dir, DVector dims, float scale,
                                                  unsigned count, std::string& error)
    {
        if (!trace_service("PublishFleshGibs"))
            return 0;
        if (!client_resources || !objects_.models)
        {
            error = "Flesh-gib resources unavailable";
            return 0;
        }
        return flesh_gibs.publish(owner, pos, dir, dims, scale, count, *this, *objects_.models, *client_resources,
                                  objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishPolyGrid(HOBJECT owner, const PolyGridSettings& settings, std::string& error)
    {
        if (!trace_service("PublishPolyGrid"))
            return 0;
        if (!objects_.sprites)
        {
            error = "PolyGrid sprite store unavailable";
            return 0;
        }
        auto id = polygrids.publish(owner, settings, *this, *objects_.sprites, objects_.time(), error);
        if (!id)
            record_unavailable("PolyGrid: " + error);
        return id;
    }
    std::uint64_t ServerAdapter::PublishParticleStream(HOBJECT owner, const ParticleStreamSettings& settings,
                                                       std::string& error)
    {
        if (!trace_service("PublishParticleStream"))
            return 0;
        return particle_streams.publish(owner, settings, *this, client_resources, objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishObjectParticles(HOBJECT owner, unsigned type, std::string& error,
                                                        float lifetime_scale)
    {
        if (!trace_service("PublishObjectParticles"))
            return 0;
        return object_particles.publish(owner, type, *this, client_resources, objects_.time(), error, lifetime_scale);
    }
    std::uint64_t ServerAdapter::PublishLaserBeam(HOBJECT source, DVector from, DVector to, std::string& error,
                                                  DBYTE type)
    {
        if (!trace_service("PublishLaserBeam"))
            return 0;
        if (!IsObjectAlive(source) || !objects_.models || !client_resources)
        {
            error = "Laser source/model resources unavailable";
            return 0;
        }
        return laser_beams.publish(from, to, GetObjectName(source), *objects_.models, *client_resources,
                                   objects_.time(), error, type);
    }
    std::uint64_t ServerAdapter::PublishWeaponSprites(HOBJECT source, const WeaponSpriteSettings& settings,
                                                      std::string& error)
    {
        if (!trace_service("PublishWeaponSprites"))
            return 0;
        if (!IsObjectAlive(source) || !objects_.sprites)
        {
            error = "Weapon sprite source/store unavailable";
            return 0;
        }
        return weapon_sprites.publish(settings, GetObjectName(source), *objects_.sprites, objects_.time(), error);
    }

    // These copied, bounded records expose pending client FX/audio. They do not
    // enqueue gameplay and never retain a pointer into an original object.
    std::uint64_t ServerAdapter::PublishExplosionFragments(HOBJECT source, DDWORD type, DVector pos, DVector normal,
                                                           std::string& error)
    {
        if (!trace_service("PublishExplosionFragments"))
            return 0;
        if (!IsObjectAlive(source) || !objects_.models || !client_resources)
        {
            error = "Explosion fragment source/resources unavailable";
            return 0;
        }
        return explosion_fragments.publish(type, pos, normal, GetObjectName(source), *objects_.models,
                                           *client_resources, objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishExplosionFlames(HOBJECT source, DDWORD type, DVector position, DVector normal,
                                                        std::string& error)
    {
        if (!trace_service("PublishExplosionFlames"))
            return 0;
        if (!IsObjectAlive(source) || !client_resources)
        {
            error = "Explosion flame source/resources unavailable";
            return 0;
        }
        return explosion_flames.publish(type, position, normal, GetObjectName(source), *client_resources,
                                        objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishExplosionRings(HOBJECT source, DDWORD type, DVector position, DVector normal,
                                                       std::string& error)
    {
        if (!trace_service("PublishExplosionRings"))
            return 0;
        if (!IsObjectAlive(source) || !client_resources)
        {
            error = "Explosion ring source/resources unavailable";
            return 0;
        }
        return explosion_rings.publish(type, position, normal, GetObjectName(source), *client_resources,
                                       objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishExplosionLights(HOBJECT source, DDWORD type, DVector position, DVector normal,
                                                        std::string& error)
    {
        if (!trace_service("PublishExplosionLights"))
            return 0;
        if (!IsObjectAlive(source))
        {
            error = "Explosion light source unavailable";
            return 0;
        }
        return explosion_lights.publish(type, position, normal, GetObjectName(source), objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishExplosionModels(HOBJECT source, DDWORD type, DVector position, DVector normal,
                                                        std::string& error)
    {
        if (!trace_service("PublishExplosionModels"))
            return 0;
        if (!IsObjectAlive(source) || !objects_.models || !client_resources)
        {
            error = "Explosion source/model resources unavailable";
            return 0;
        }
        return explosion_models.publish(type, position, normal, GetObjectName(source), *objects_.models,
                                        *client_resources, objects_.time(), error);
    }
    std::uint64_t ServerAdapter::PublishExplosionSprites(HOBJECT source, DDWORD type, DVector position, DVector normal,
                                                         std::string& error)
    {
        if (!trace_service("PublishExplosionSprites"))
            return 0;
        if (!IsObjectAlive(source) || !objects_.sprites)
        {
            error = "Explosion source or sprite store unavailable";
            return 0;
        }
        return explosion_sprites.publish(type, position, normal, GetObjectName(source), *objects_.sprites,
                                         objects_.time(), error);
    }
    void ServerAdapter::RecordSceneExplosion(const SceneExplosionReport& effect)
    {
        trace_service("RecordSceneExplosion");
        if (!physics_vector(effect.position) || !physics_vector(effect.normal) || !std::isfinite(effect.damage) ||
            !std::isfinite(effect.radius) || effect.source.size() > 100 || effect.sound.size() > 100)
            return;
        if (scene_explosions.size() == 32)
            scene_explosions.erase(scene_explosions.begin());
        scene_explosions.push_back(effect);
    }
    void ServerAdapter::RecordDebrisEffect(const DebrisEffect& effect)
    {
        trace_service("RecordDebrisEffect");
        if (!physics_vector(effect.position) || !physics_vector(effect.velocity) ||
            !physics_vector(effect.dimensions) || !std::isfinite(effect.scale))
            return;
        if (debris_effects.size() == 64)
            debris_effects.erase(debris_effects.begin());
        debris_effects.push_back(effect);
    }
    void ServerAdapter::RecordExplosionEffect(const ExplosionEffect& effect)
    {
        trace_service("RecordExplosionEffect");
        if (!physics_vector(effect.position) || !physics_vector(effect.normal) || !std::isfinite(effect.damage) ||
            !std::isfinite(effect.radius))
            return;
        if (explosion_effects.size() == 32)
            explosion_effects.erase(explosion_effects.begin());
        explosion_effects.push_back(effect);
    }
} // namespace OpenBlood2
