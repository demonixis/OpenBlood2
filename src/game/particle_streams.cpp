#include "game/particle_streams.h"
#include <algorithm>
namespace OpenBlood2
{
    std::size_t ParticleStreams::particle_count() const
    {
        std::size_t n = 0;
        for (const auto& i : active)
            n += i.effect.particles.size();
        return n;
    }
    std::uint64_t ParticleStreams::publish(const EffectOwner& owner, const ParticleStreamSettings& settings,
                                           const Resources* resources, double now, std::string& error)
    {
        error.clear();
        ParticleStreamReport report;
        const auto& pose = owner.pose;
        if (!owner || !resources)
            error = "Particle stream owner/resources/pose unavailable";
        else if (active.size() >= 128 || next_id_ == UINT64_MAX ||
                 std::any_of(active.begin(), active.end(), [&](const auto& i) { return i.owner == owner.id; }))
            error = "Particle stream capacity exhausted or duplicate owner";
        else
        {
            report.source = owner.name;
            CParticleStreamFX fx;
            const auto count = particle_count();
            if (fx.Init(settings, error) &&
                fx.CreateObject(*resources, pose, now, random_, error, count < 8192 ? 8192 - count : 0))
            {
                report.id = fx.id = next_id_++;
                active.push_back({owner.id, std::move(fx)});
                ++started;
            }
        }
        if (!error.empty())
            ++failed;
        report.error = error;
        const auto id = report.id;
        if (reports.size() == 128)
            reports.erase(reports.begin());
        reports.push_back(std::move(report));
        return id;
    }
    bool ParticleStreams::advance(double now, const ClientWorld& world)
    {
        bool valid = true;
        auto total = particle_count();
        for (auto it = active.begin(); it != active.end();)
        {
            auto& fx = it->effect;
            const auto others = total - fx.particles.size();
            const auto* owner = world.get(it->owner);
            const auto flags = owner ? owner->user_flags : 0;
            if (fx.Update(now, owner ? &owner->pose : nullptr, flags & 1u, random_, others < 8192 ? 8192 - others : 0))
            {
                total = others + fx.particles.size();
                ++it;
                continue;
            }
            total = others;
            if (fx.error.empty())
                ++expired;
            else
            {
                ++failed;
                valid = false;
                update_error = fx.error;
            }
            for (auto& report : reports)
                if (report.id == fx.id)
                {
                    report.error = fx.error;
                    report.removed = true;
                }
            it = active.erase(it);
        }
        return valid;
    }
    void ParticleStreams::clear()
    {
        active.clear();
        reports.clear();
        update_error.clear();
        started = expired = failed = 0;
        random_.seed(1); // IDs survive reload.
    }
} // namespace OpenBlood2
