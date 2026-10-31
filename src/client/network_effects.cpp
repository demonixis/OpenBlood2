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
    bool NetworkEffects::receive(const Net::BallisticEvent& event, std::string& error)
    {
        error.clear();
        if (!Net::valid_ballistic(event) || event.world != owner_.world || event.player != owner_.player ||
            scene_.frame().world != owner_.world)
        {
            error = "Invalid ballistic recipient/world";
            return false;
        }
        if (event.sequence <= sequence_)
            return true;
        if (sequence_ == UINT64_MAX || event.sequence != sequence_ + 1 || pending_.size() >= Net::ballistic_queue_limit)
        {
            error = "Ballistic sequence gap or pending capacity exhausted";
            return false;
        }
        pending_.push_back(event);
        sequence_ = event.sequence;
        ++received;
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
        EffectServices services{queries, runtime_, &resources, &sprites, &audio, time_, seconds};
        for (const auto& event : pending_)
        {
            // Detached impact effects survive their source's removal. Identity
            // is provenance, never a request to resolve/run its server object.
            const EffectOwner owner{{owner_.world, event.source}, {}, 0, event.name};
            std::string detail;
            runtime_.shotgun_effects.publish(owner, event.settings, models, sprites, resources, time_, services,
                                             detail);
            if (!detail.empty())
            {
                ++failed;
                last_error = detail;
            }
        }
        pending_.clear();
        time_ += seconds;
        services.time = time_;
        const bool sprites_ok = runtime_.weapon_sprites.advance(time_);
        runtime_.shotgun_effects.advance(time_, services);
        // Child muzzle smoke and underwater bubbles use the same original pools
        // and update order as the solo session. No second movement integration.
        const bool sparks_ok = runtime_.sparks.advance(time_);
        if (!sprites_ok || !sparks_ok || !runtime_.shotgun_effects.update_error.empty())
        {
            ++failed;
            last_error = runtime_.shotgun_effects.update_error + runtime_.weapon_sprites.update_error +
                         runtime_.sparks.update_error;
        }
        ClientEffectsFrame frame;
        frame.world = owner_.world;
        frame.sequence = scene_.frame().sequence + 1;
        frame.time = time_;
        append_runtime_effects(runtime_, world, frame);
        return scene_.receive(std::move(frame), error);
    }
} // namespace OpenBlood2
