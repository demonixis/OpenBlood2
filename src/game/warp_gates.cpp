#include "warp_gates.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    std::size_t WarpGates::particle_count() const
    {
        std::size_t count = 0;
        for (const auto& gate : active)
            for (const auto& system : gate.particles)
                count += system.particles.size();
        return count;
    }
    std::uint64_t WarpGates::publish(const EffectOwner& owner, const WarpGateSettings& settings, SpriteStore& store,
                                     const Resources& resources, double now, std::string& error)
    {
        error.clear();
        WarpGateReport report;
        WarpGateInstance gate;
        const auto& pose = owner.pose;
        if (!std::isfinite(now) || now < 0)
            error = "Invalid warp gate publication clock";
        else if (!owner)
            error = "Warp owner pose unavailable";
        else if (active.size() >= 128 || next_id_ == UINT64_MAX)
            error = "Warp gate capacity exceeded";
        else if (valid_warp_gate(settings, error))
        {
            report.id = gate.id = next_id_++;
            report.source = owner.name;
            gate.owner = owner.id;
            gate.sprite_ready = gate.sprite.CreateObject(settings, store, pose, now);
            if (!gate.sprite_ready)
                error = "sprite: " + gate.sprite.error;
            for (unsigned i = 0; i < 2; ++i)
            {
                gate.particle_ready[i] = gate.particles[i].CreateObject(
                    settings.particles[i], settings.ramp_up, settings.ramp_down, store, resources, pose, now);
                if (!gate.particle_ready[i])
                    error += "; PS" + std::to_string(i + 1) + ": " + gate.particles[i].error;
            }
            active.push_back(std::move(gate)); // Independent original child publications survive sibling failure.
            ++started;
        }
        report.error = error;
        if (!error.empty())
            ++failed;
        const auto id = report.id;
        if (reports.size() == 64)
            reports.erase(reports.begin());
        reports.push_back(std::move(report));
        return id;
    }
    bool WarpGates::advance(double now, const ClientWorld& world)
    {
        bool valid = true;
        std::erase_if(active,
                      [&](auto& gate)
                      {
                          if (!world.get(gate.owner))
                          {
                              for (auto& report : reports)
                                  if (report.id == gate.id)
                                      report.ended = true;
                              ++expired;
                              return true;
                          }
                          return false;
                      });
        for (auto& gate : active)
        {
            const auto* owner = world.get(gate.owner);
            std::string failure;
            if (!owner)
                failure = "Warp owner pose unavailable";
            else
            {
                const auto& pose = owner->pose;
                const bool on = (owner->user_flags & 1u) != 0;
                if (gate.sprite_ready && !gate.sprite.Update(now, pose, on))
                {
                    failure = gate.sprite.error;
                    gate.sprite_ready = false;
                }
                for (unsigned i = 0; i < 2; ++i)
                {
                    auto& p = gate.particles[i];
                    const auto others = particle_count() - p.particles.size();
                    const auto budget = others <= 8192 ? std::min<std::size_t>(2048, 8192 - others) : 0;
                    if (gate.particle_ready[i] && !p.Update(now, pose, on, random_, budget))
                    {
                        failure += "; PS" + std::to_string(i + 1) + ": " + p.error;
                        gate.particle_ready[i] = false;
                    }
                }
            }
            if (!failure.empty())
            {
                ++failed;
                update_error = failure;
                valid = false;
                for (auto& report : reports)
                    if (report.id == gate.id)
                        report.error = failure;
            }
        }
        return valid;
    }
    void WarpGates::clear()
    {
        active.clear();
        reports.clear();
        update_error.clear();
        started = expired = failed = 0;
        random_.seed(1); // Identity counters survive unload.
    }
} // namespace OpenBlood2
