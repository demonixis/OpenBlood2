#include "game/particle_streams.h"
#include "server_de.h"
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
    std::uint64_t ParticleStreams::publish(HOBJECT owner, const ParticleStreamSettings& settings, ServerDE& server,
                                           const Resources* resources, double now, std::string& error)
    {
        error.clear();
        ParticleStreamReport report;
        NodePose pose;
        if (!server.IsObjectAlive(owner) || !resources || server.GetObjectPos(owner, &pose.position) != DE_OK ||
            server.GetObjectRotation(owner, &pose.rotation) != DE_OK)
            error = "Particle stream owner/resources/pose unavailable";
        else if (active.size() >= 128 || next_id_ == UINT64_MAX ||
                 std::any_of(active.begin(), active.end(), [&](const auto& i) { return i.owner == owner; }))
            error = "Particle stream capacity exhausted or duplicate owner";
        else
        {
            report.source = server.GetObjectName(owner);
            CParticleStreamFX fx;
            const auto count = particle_count();
            if (fx.Init(settings, error) &&
                fx.CreateObject(*resources, pose, now, random_, error, count < 8192 ? 8192 - count : 0))
            {
                report.id = fx.id = next_id_++;
                active.push_back({owner, std::move(fx)});
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
    bool ParticleStreams::advance(double now, ServerDE& server)
    {
        bool valid = true;
        auto total = particle_count();
        for (auto it = active.begin(); it != active.end();)
        {
            auto& fx = it->effect;
            const auto others = total - fx.particles.size();
            NodePose pose;
            const bool live = server.IsObjectAlive(it->owner);
            const bool pose_ok = !live || (server.GetObjectPos(it->owner, &pose.position) == DE_OK &&
                                           server.GetObjectRotation(it->owner, &pose.rotation) == DE_OK);
            const auto flags = live ? server.GetObjectUserFlags(it->owner) : 0;
            if (pose_ok &&
                fx.Update(now, live ? &pose : nullptr, flags & 1u, random_, others < 8192 ? 8192 - others : 0))
            {
                total = others + fx.particles.size();
                ++it;
                continue;
            }
            total = others;
            if (!pose_ok)
                fx.error = "Particle stream owner pose unavailable";
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
