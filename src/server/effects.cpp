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
            if (!player.effects_started || player.effects_failed)
                continue;
            if (!player.next_effect || player.effects.size() >= Net::effect_queue_limit ||
                encoded.size() > Net::effect_queue_bytes - player.effect_bytes)
            {
                player.effects_failed = true;
                continue; // One slow recipient cannot alter the shot or other peers.
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
        if (p == players_.end() || !p->effects_started || p->effects_failed)
        {
            error = "Effect recipient queue unavailable or over capacity";
            return false;
        }
        out.clear();
        out.swap(p->effects);
        p->effect_bytes = 0;
        return true;
    }
} // namespace OpenBlood2
