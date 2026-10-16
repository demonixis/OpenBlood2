#include "game/server_adapter.h"
#include <cmath>
namespace OpenBlood2
{
    // These copied, bounded records expose pending client FX/audio. They do not
    // enqueue gameplay and never retain a pointer into an original object.
    std::uint64_t ServerAdapter::PublishExplosionModels(HOBJECT source, DDWORD type, DVector position, DVector normal,
                                                        std::string& error)
    {
        trace_service("PublishExplosionModels (partial high-detail composite)");
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
        trace_service("PublishExplosionSprites (partial high-detail composite)");
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
