#include "engine/texture.h"
#include "server/session.h"
#include <algorithm>
namespace OpenBlood2
{
    void ServerSession::prune_streams()
    {
        std::erase_if(stream_emitters_,
                      [&](const auto& stream)
                      {
                          const auto* object = game_.objects.get(stream.owner);
                          return !object || object->life != Life::Alive;
                      });
    }
    std::uint64_t ServerSession::capture_stream(ObjectId owner, const ParticleStreamSettings& settings,
                                                std::string& error)
    {
        error.clear();
        prune_streams();
        const auto* object = game_.objects.get(owner);
        if (!object || object->life != Life::Alive || !valid_particle_stream(settings) || !next_stream_ ||
            stream_emitters_.size() >= 128 ||
            std::any_of(stream_emitters_.begin(), stream_emitters_.end(),
                        [&](const auto& stream) { return stream.owner == owner; }))
        {
            error = "Invalid, duplicate or over-capacity server particle stream";
            return 0;
        }
        // Preserve strict resource admission without creating/advancing cosmetic particles.
        Texture image;
        if (!read_texture_resource(game_.resources, settings.texture, image, error))
            return 0;
        stream_emitters_.push_back({owner, settings});
        return next_stream_++;
    }
    void ServerSession::prune_particles()
    {
        const double now = game_.server.world_time();
        std::erase_if(particle_emitters_,
                      [&](const auto& p)
                      {
                          const auto* object = game_.objects.get(p.owner);
                          const auto duration = object_particle_duration(p.recipe.type);
                          return !object || object->life != Life::Alive ||
                                 (duration && float(now - p.recipe.created) > duration);
                      });
    }
    std::uint64_t ServerSession::capture_particle(ObjectId owner, std::string_view name, unsigned type, float scale,
                                                  DVector offset, std::string& error)
    {
        error.clear();
        prune_particles();
        ObjectParticleRecipe recipe{next_particle_, game_.server.world_time(), type, scale, offset, std::string(name)};
        const auto* object = game_.objects.get(owner);
        if (!object || object->life != Life::Alive || particle_emitters_.size() >= 128 ||
            !valid_object_particle_recipe(recipe))
        {
            error = "Invalid, exhausted or over-capacity object particle publication";
            return 0;
        }
        const auto texture = object_particle_texture(type);
        if (!game_.resources.find(texture))
        {
            error = "Object particle texture is not indexed: " + std::string(texture);
            return 0;
        }
        particle_emitters_.push_back({owner, std::move(recipe)});
        return next_particle_++;
    }
    bool ServerSession::begin_effects(std::uint64_t id)
    {
        const auto p = std::find_if(players_.begin(), players_.end(), [id](const auto& p) { return p.id == id; });
        if (p == players_.end() || p->effects_started)
            return false;
        p->effects_started = true;
        return true;
    }
    std::uint64_t ServerSession::capture_effect(ObjectId source, std::string_view name,
                                                const InstantEffectRecipe& settings, std::string& error)
    {
        error.clear();
        if (effect_events == UINT64_MAX)
        {
            error = "Invalid or exhausted authoritative effect publication";
            return 0;
        }
        // During a world load (or with nobody joined) there is no recipient: the
        // instant effect is discarded, never published into the server's own pools.
        if (std::none_of(players_.begin(), players_.end(), [](const auto& p) { return p.effects_started; }))
            return ++effect_events;
        Net::EffectEvent event{world_id_, 1, 1, source, std::string(name), settings};
        std::vector<std::uint8_t> encoded;
        if (effect_events == UINT64_MAX || !Net::encode_effect(event, encoded))
        {
            error = "Invalid or exhausted authoritative effect publication";
            return 0;
        }
        for (auto& player : players_)
        {
            // Baseline-ready admission gate. These are one-shot impact
            // messages: never replay old shots/casings to a late arrival.
            if (!player.effects_started)
                continue;
            if (!player.next_effect || player.effects.size() >= Net::effect_queue_limit ||
                encoded.size() > Net::effect_queue_bytes - player.effect_bytes)
            {
                // Cosmetic: a full recipient drops this effect (no sequence is
                // consumed) and stays connected; the shot and other peers are unchanged.
                ++player.effects_dropped;
                ++effect_drops;
                continue;
            }
            event.player = player.id;
            event.sequence = player.next_effect++;
            player.effects.push_back(event);
            player.effect_bytes += encoded.size();
        }
        return ++effect_events;
    }
    bool ServerSession::take_effect_events(std::uint64_t id, std::vector<Net::EffectEvent>& out, std::string& error)
    {
        error.clear();
        const auto p = std::find_if(players_.begin(), players_.end(), [id](const auto& p) { return p.id == id; });
        if (p == players_.end() || !p->effects_started)
        {
            error = "Effect recipient queue unavailable";
            return false;
        }
        out.clear();
        out.swap(p->effects);
        p->effect_bytes = 0;
        return true;
    }
} // namespace OpenBlood2
