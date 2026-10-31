#include "client/network_effects.h"
#include "client/effect_queries.h"
#include "client/effect_services.h"
#include "client/effect_snapshot.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    void NetworkEffects::clear()
    {
        pending_.clear();
        pending_bytes_ = 0;
        stream_owners_.clear();
        highest_particle_ = 0;
        particle_time_ = 0;
        runtime_.reset_world();
        scene_ = {};
        last_error.clear();
    }
    bool NetworkEffects::begin(const Net::Joined& owner)
    {
        if (!owner.world || !owner.player || !owner.object || !owner.object.generation || owner.world <= owner_.world)
            return false;
        clear();
        owner_ = owner;
        sequence_ = 0;
        time_ = 0;
        runtime_.begin_world(owner.world);
        return scene_.begin(owner.world);
    }
    bool NetworkEffects::receive(const Net::EffectEvent& event, std::string& error)
    {
        error.clear();
        if (!Net::valid_effect(event) || event.world != owner_.world || event.player != owner_.player ||
            scene_.frame().world != owner_.world)
        {
            error = "Invalid effect recipient/world";
            return false;
        }
        if (event.sequence <= sequence_)
            return true;
        std::vector<std::uint8_t> encoded;
        if (sequence_ == UINT64_MAX || event.sequence != sequence_ + 1 || pending_.size() >= Net::effect_queue_limit ||
            !Net::encode_effect(event, encoded) || encoded.size() > Net::effect_queue_bytes - pending_bytes_)
        {
            error = "Effect sequence gap or pending capacity exhausted";
            return false;
        }
        pending_.push_back(event);
        pending_bytes_ += encoded.size();
        sequence_ = event.sequence;
        ++received;
        ++received_kinds[event.recipe.index()];
        return true;
    }
    bool NetworkEffects::advance(double seconds, const ClientWorld& world, const ClientCollisionWorld& collision,
                                 ModelStore& models, SpriteStore& sprites, const Resources& resources,
                                 EffectAudio& audio, std::string& error)
    {
        error.clear();
        if (world.frame().world != owner_.world || scene_.frame().world != owner_.world || !std::isfinite(seconds) ||
            seconds < 0 || seconds > 1 || time_ + seconds > 1e12)
        {
            error = "Invalid network cosmetic clock/world";
            return false;
        }
        ClientEffectQueries queries(&collision);
        // Execute publications at this tick's time, like the solo server update.
        // An immediate blood update consumes this frame once; its .001 deadline
        // prevents the ordinary pool advance from integrating it a second time.
        time_ += seconds;
        EffectServices services{queries, runtime_, &resources, &sprites, &audio, time_, seconds};
        // Descriptors arrive atomically with reliable membership, including late joins.
        // Remember failed/expired starts as well, so snapshots never restart an emitter.
        std::erase_if(stream_owners_, [&](auto id) { return !world.get(id) || !world.get(id)->particle_stream; });
        auto& streams = runtime_.particle_streams;
        std::erase_if(streams.active,
                      [&](const auto& stream)
                      {
                          if (world.get(stream.owner) && world.get(stream.owner)->particle_stream)
                              return false;
                          ++streams.expired;
                          for (auto& report : streams.reports)
                              if (report.id == stream.effect.id)
                                  report.removed = true;
                          return true;
                      });
        for (const auto& object : world.frame().objects)
        {
            if (!object.particle_stream ||
                std::find(stream_owners_.begin(), stream_owners_.end(), object.id) != stream_owners_.end())
                continue;
            stream_owners_.push_back(object.id);
            auto settings = *object.particle_stream;
            // Arrival starts a fresh local emitter in the current visible state;
            // the source's creation-time On flag may have changed before admission.
            settings.on = (object.user_flags & 1u) != 0;
            std::string detail;
            const EffectOwner source{world.reference(object.id), object.pose, object.user_flags,
                                     "Network particle stream"};
            if (!runtime_.particle_streams.publish(source, settings, &resources, time_, detail))
            {
                ++failed;
                last_error = detail;
            }
        }

        // Finite effects retain their authored age; long-lived trails start fresh.
        // Membership can create an owner ahead of the delayed interpolation clock.
        particle_time_ = std::max(particle_time_ + seconds, world.frame().time);
        for (const auto& object : world.frame().objects)
            for (const auto& p : object.object_particles)
                particle_time_ = std::max(particle_time_, p.created);
        if (!std::isfinite(particle_time_) || particle_time_ > 1e12)
        {
            error = "Network object particle clock exhausted";
            return false;
        }
        auto& particles = runtime_.object_particles;
        std::erase_if(particles.active,
                      [&](const auto& p)
                      {
                          if (world.get(p.owner))
                              return false;
                          ++particles.expired;
                          return true;
                      });
        auto highest_particle = highest_particle_;
        for (const auto& object : world.frame().objects)
            for (const auto& p : object.object_particles)
            {
                highest_particle = std::max(highest_particle, p.id);
                if (p.id <= highest_particle_)
                    continue;
                const auto duration = object_particle_duration(p.type);
                if (duration && float(particle_time_ - p.created) > duration)
                    continue;
                const EffectOwner source{world.reference(object.id), object.pose, object.user_flags, p.source};
                std::string detail;
                if (!particles.publish(source, p.type, &resources, duration ? p.created : particle_time_, detail,
                                       p.lifetime_scale, p.offset))
                {
                    ++failed;
                    last_error = detail;
                }
            }
        highest_particle_ = highest_particle;

        for (const auto& event : pending_)
        {
            // Detached impact effects survive their source's removal. Identity
            // is provenance, never a request to resolve/run its server object.
            const EffectOwner owner{{owner_.world, event.source}, {}, 0, event.name};
            std::string detail;
            std::visit(
                [&](const auto& recipe)
                {
                    using T = std::decay_t<decltype(recipe)>;
                    if constexpr (std::is_same_v<T, ShotgunEffectSettings>)
                        runtime_.shotgun_effects.publish(owner, recipe, models, sprites, resources, time_, services,
                                                         detail);
                    else if constexpr (std::is_same_v<T, WeaponSpriteSettings>)
                        runtime_.weapon_sprites.publish(recipe, event.name, sprites, time_, detail);
                    else if constexpr (std::is_same_v<T, SparkSettings>)
                        runtime_.sparks.publish(recipe, event.name, resources, time_, detail);
                    else if constexpr (std::is_same_v<T, BloodSpurtRecipe>)
                        runtime_.blood_spurts.publish(recipe.settings, event.name, recipe.immediate, resources,
                                                      services, time_, detail);
                    else if constexpr (std::is_same_v<T, BloodSplatSettings>)
                        runtime_.blood_splats.publish(recipe, event.name, sprites, time_, detail);
                    else if constexpr (std::is_same_v<T, ExplosionRecipe>)
                    {
                        switch (recipe.component)
                        {
                            case ExplosionComponent::Models:
                                runtime_.explosion_models.publish(recipe.type, recipe.position, recipe.normal,
                                                                  event.name, models, resources, time_, detail);
                                break;
                            case ExplosionComponent::Sprites:
                                runtime_.explosion_sprites.publish(recipe.type, recipe.position, recipe.normal,
                                                                   event.name, sprites, time_, detail);
                                break;
                            case ExplosionComponent::Rings:
                                runtime_.explosion_rings.publish(recipe.type, recipe.position, recipe.normal,
                                                                 event.name, resources, time_, detail);
                                break;
                            case ExplosionComponent::Fragments:
                                runtime_.explosion_fragments.publish(recipe.type, recipe.position, recipe.normal,
                                                                     event.name, models, resources, time_, detail);
                                break;
                            case ExplosionComponent::Flames:
                                runtime_.explosion_flames.publish(recipe.type, recipe.position, recipe.normal,
                                                                  event.name, resources, time_, detail);
                                break;
                            case ExplosionComponent::Lights:
                                runtime_.explosion_lights.publish(recipe.type, recipe.position, recipe.normal,
                                                                  event.name, time_, detail);
                                break;
                        }
                    }
                    else if constexpr (std::is_same_v<T, FleshRecipe>)
                        runtime_.flesh_gibs.publish(owner, recipe.position, recipe.velocity, recipe.dimensions,
                                                    recipe.scale, recipe.count, services, models, resources, time_,
                                                    detail);
                    else if constexpr (std::is_same_v<T, DebrisRecipe>)
                    {
                        DebrisEffect effect;
                        effect.source = event.name;
                        effect.position = recipe.position;
                        effect.velocity = recipe.velocity;
                        effect.dimensions = recipe.dimensions;
                        effect.flags = recipe.flags;
                        effect.scale = recipe.scale;
                        effect.amount = recipe.amount;
                        effect.model = recipe.model;
                        effect.texture = recipe.texture;
                        effect.sound = recipe.sound;
                        runtime_.destruction_debris.publish_debris(effect, debris_detail_, services, models, resources,
                                                                   time_, detail);
                    }
                    else if constexpr (std::is_same_v<T, LaserRecipe>)
                        runtime_.laser_beams.publish(recipe.source, recipe.destination, event.name, models, resources,
                                                     time_, detail, recipe.type);
                    else if constexpr (std::is_same_v<T, LightningSettings>)
                        runtime_.lightning.publish(recipe, event.name, resources, time_, detail);
                },
                event.recipe);
            if (!detail.empty())
            {
                ++failed;
                last_error = detail;
            }
        }
        pending_.clear();
        pending_bytes_ = 0;
        // Same relative order as the solo session. Cosmetic failures retain
        // their own diagnostics and do not cancel another component's update.
        auto step = [&](bool ok, const std::string& detail)
        {
            if (!ok)
            {
                ++failed;
                last_error = detail;
            }
        };
        step(runtime_.explosion_sprites.advance(time_), runtime_.explosion_sprites.update_error);
        step(runtime_.blood_splats.advance(time_), runtime_.blood_splats.update_error);
        step(runtime_.weapon_sprites.advance(time_), runtime_.weapon_sprites.update_error);
        step(runtime_.explosion_models.advance(time_), runtime_.explosion_models.update_error);
        step(runtime_.laser_beams.advance(time_), runtime_.laser_beams.update_error);
        step(runtime_.explosion_lights.advance(time_), runtime_.explosion_lights.update_error);
        step(runtime_.explosion_rings.advance(time_), runtime_.explosion_rings.update_error);
        step(runtime_.explosion_flames.advance(time_), runtime_.explosion_flames.update_error);
        step(runtime_.flesh_gibs.advance(time_, services), runtime_.flesh_gibs.update_error);
        step(runtime_.destruction_debris.advance(time_, services), runtime_.destruction_debris.update_error);
        runtime_.shotgun_effects.advance(time_, services);
        step(runtime_.shotgun_effects.update_error.empty(), runtime_.shotgun_effects.update_error);
        step(runtime_.sparks.advance(time_), runtime_.sparks.update_error);
        step(runtime_.lightning.advance(time_), runtime_.lightning.update_error);
        step(runtime_.blood_spurts.advance(time_, services), runtime_.blood_spurts.update_error);
        step(runtime_.explosion_fragments.advance(time_, queries, true), runtime_.explosion_fragments.update_error);
        step(runtime_.object_particles.advance(particle_time_, world, true), runtime_.object_particles.update_error);
        step(runtime_.particle_streams.advance(time_, world), runtime_.particle_streams.update_error);
        ClientEffectsFrame frame;
        frame.world = owner_.world;
        frame.sequence = scene_.frame().sequence + 1;
        frame.time = time_;
        append_runtime_effects(runtime_, world, frame);
        return scene_.receive(std::move(frame), error);
    }
} // namespace OpenBlood2
