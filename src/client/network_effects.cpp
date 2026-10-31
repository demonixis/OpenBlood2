#include "client/network_effects.h"
#include "client/effect_queries.h"
#include "client/effect_services.h"
#include "client/effect_snapshot.h"
#include <cmath>
namespace OpenBlood2
{
    void NetworkEffects::clear()
    {
        pending_.clear();
        pending_bytes_ = 0;
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
        const bool splats_ok = runtime_.blood_splats.advance(time_);
        const bool sprites_ok = runtime_.weapon_sprites.advance(time_);
        runtime_.shotgun_effects.advance(time_, services);
        // Child muzzle smoke and underwater bubbles use the same original pools
        // and update order as the solo session. No second movement integration.
        const bool sparks_ok = runtime_.sparks.advance(time_);
        const bool blood_ok = runtime_.blood_spurts.advance(time_, services);
        if (!splats_ok || !sprites_ok || !sparks_ok || !blood_ok || !runtime_.shotgun_effects.update_error.empty())
        {
            ++failed;
            last_error = runtime_.shotgun_effects.update_error + runtime_.weapon_sprites.update_error +
                         runtime_.sparks.update_error + runtime_.blood_splats.update_error +
                         runtime_.blood_spurts.update_error;
        }
        ClientEffectsFrame frame;
        frame.world = owner_.world;
        frame.sequence = scene_.frame().sequence + 1;
        frame.time = time_;
        append_runtime_effects(runtime_, world, frame);
        return scene_.receive(std::move(frame), error);
    }
} // namespace OpenBlood2
