// Selected SmokeTrailFX and SmokeTrailSegmentFX client clocks and puffs.
#include "client/legacy_smoke_trails.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
    void LegacySmokeTrails::emit(Segment& segment, DVector position, double now)
    {
        const unsigned steps = segment.last_add > 0 ? 8 : 1;
        const auto delta = vector_scale(vector_subtract(position, segment.last_position), 1.f / steps);
        auto point = segment.last_position;
        const auto life_offset = float(now - segment.last_add) / steps;
        auto draw = [&](float low, float high) { return std::uniform_real_distribution<float>(low, high)(random_); };
        for (unsigned step = 0; step < steps; ++step)
        {
            for (unsigned j = 0; j < segment.puff && segment.particles.size() < 2048; ++j)
            {
                const DVector drift{draw(-segment.drift, segment.drift), draw(5, 6), draw(-segment.drift, segment.drift)};
                const auto at = vector_add(point, {draw(-.5f, .5f), draw(-.5f, .5f), draw(-.5f, .5f)});
                // Source adds the absolute random colour to the low endpoint.
                // The shared particle frame saturates this overbright value.
                const float color = std::min(1.f, (175 + draw(175, 255)) / 255);
                segment.particles.push_back({at, drift, 10 + step * life_offset, {color, color, color}});
            }
            point = vector_add(point, delta);
        }
        segment.last_add = now;
        segment.last_position = position;
    }

    void LegacySmokeTrails::append(const std::map<std::uint64_t, LegacySmokeSource>& sources, const Resources& resources,
                                    ClientEffectsFrame& frame)
    {
        std::erase_if(last_segment_, [&](const auto& item) { return !sources.contains(item.first); });
        const auto now = frame.time;
        for (const auto& [id, source] : sources)
        {
            auto& last = last_segment_.try_emplace(id, -1).first->second;
            if (last >= 0 && now <= last + .1)
                continue;
            last = now;
            Segment segment;
            segment.owner = id;
            segment.start = segment.last_add = segment.last_update = now;
            segment.origin = source.position;
            segment.texture = "spritetextures/smoke32_2.dtx";
            if (vector_dot(source.velocity, source.velocity) < 1)
            {
                segment.gravity = 25;
                segment.interval = .1f;
                segment.radius = 500;
                segment.puff = 1;
                segment.lifetime = 1;
            }
            if (source.small)
            {
                segment.radius *= .5f;
                segment.drift = 5;
            }
            if (source.liquid)
            {
                segment.texture = "spritetextures/particletextures/particlebubble.dtx";
                if (!resources.find(segment.texture))
                    segment.texture = "spritetextures/particles/particlebubble.dtx"; // Retail archive location.
                segment.radius *= .25f;
                segment.gravity = 5;
                segment.puff *= 3;
            }
            if (!resources.find(segment.texture) || next_id_ == UINT64_MAX)
                continue;
            if (segments_.size() >= 128)
                segments_.erase(segments_.begin());
            segment.id = next_id_++;
            segments_.push_back(std::move(segment));
        }

        std::erase_if(segments_,
                      [&](Segment& segment)
                      {
                          const auto source = sources.find(segment.owner);
                          const bool fading = source == sources.end() || now > segment.start + .2;
                          if (fading && now > segment.last_add + segment.lifetime)
                              return true;
                          std::string error;
                          if (!advance_particles(segment.particles, segment.gravity, now - segment.last_update, error))
                              return true;
                          segment.last_update = now;
                          if (fading)
                              segment.alpha = std::clamp(1.f - float(now - segment.last_add) / segment.lifetime, 0.f, 1.f);
                          else if (now > segment.last_add + segment.interval)
                              emit(segment, vector_subtract(source->second.position, segment.origin), now);
                          if (!segment.particles.empty())
                          {
                              EffectParticles effect;
                              effect.texture = segment.texture;
                              effect.origin = segment.origin;
                              effect.half_extent = segment.radius / 640.f;
                              effect.alpha = segment.alpha;
                              effect.batched = true;
                              effect.particles = segment.particles;
                              frame.billboards.push_back({{EffectKind::LegacySmoke, segment.id}, std::move(effect)});
                          }
                          return false;
                      });
    }
}
