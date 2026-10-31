#include "client/world.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace OpenBlood2
{
    bool ClientWorld::begin(std::uint64_t world)
    {
        if (!world || world <= frame_.world)
            return false;
        frame_ = {};
        frame_.world = world;
        return true;
    }
    bool ClientWorld::apply(ClientWorldFrame&& next, std::string& error)
    {
        error.clear();
        if (!next.world || next.world != frame_.world || !next.sequence || next.sequence <= frame_.sequence)
            error = "Stale client world/frame";
        else if (!std::isfinite(next.time) || next.time < 0 || next.time < frame_.time)
            error = "Invalid client world clock";
        else if (next.objects.size() > object_limit || next.polygrids.size() > object_limit ||
                 next.particle_streams.size() > object_limit)
            error = "Client world frame limit exceeded";
        else
        {
            std::size_t streams = 0;
            std::array<std::uint64_t, 128> particles{};
            std::size_t particle_count = 0;
            ObjectId previous;
            for (const auto& object : next.objects)
            {
                if (!object.id || object.id.slot >= object_limit || !object.id.generation ||
                    (previous && previous.slot >= object.id.slot))
                {
                    error = "Invalid or unordered client object identity";
                    break;
                }
                if (object.particle_stream && (++streams > 128 || !valid_particle_stream(*object.particle_stream)))
                {
                    error = "Invalid or excessive client particle stream descriptors";
                    break;
                }
                if (object.object_particles.size() > particles.size() - particle_count)
                {
                    error = "Client object particle limit exceeded";
                    break;
                }
                std::uint64_t previous_particle = 0;
                for (const auto& p : object.object_particles)
                {
                    // Presentation frames may interpolate before the latest object's creation time.
                    if (!valid_object_particle_recipe(p) || p.id <= previous_particle)
                    {
                        error = "Invalid client object particle descriptor";
                        break;
                    }
                    previous_particle = p.id;
                    particles[particle_count++] = p.id;
                }
                if (!error.empty())
                    break;
                previous = object.id;
            }
            const auto end = particles.begin() + particle_count;
            std::sort(particles.begin(), end);
            if (std::adjacent_find(particles.begin(), end) != end)
                error = "Duplicate client object particle identity";
        }
        if (!error.empty())
            return false;
        // The caller receives the replaced frame, so a local publisher can reuse its storage.
        std::swap(frame_, next);
        return true;
    }
    const ClientObject* ClientWorld::get(ObjectId id) const
    {
        if (!id)
            return nullptr;
        const auto found =
            std::lower_bound(frame_.objects.begin(), frame_.objects.end(), id.slot,
                             [](const ClientObject& object, auto slot) { return object.id.slot < slot; });
        return found != frame_.objects.end() && found->id == id ? &*found : nullptr;
    }
    const ClientObject* ClientWorld::get(ClientObjectId id) const
    {
        return id.world == frame_.world ? get(id.object) : nullptr;
    }
} // namespace OpenBlood2
