#include "client/effects.h"
#include <cmath>
#include <set>
namespace OpenBlood2
{
    namespace
    {
        bool pose(NodePose p)
        {
            const auto& q = p.rotation;
            const float norm = vector_dot(q.m_Vec, q.m_Vec) + q.m_Spin * q.m_Spin;
            return physics_vector(p.position) && physics_vector(q.m_Vec) && std::isfinite(q.m_Spin) &&
                   std::isfinite(norm) && std::abs(norm - 1) < .001f;
        }
        bool text(const std::string& s, std::size_t limit = 1024)
        {
            return s.size() <= limit && s.find('\0') == std::string::npos;
        }
        bool object(ObjectId id)
        {
            return id && id.generation && id.slot < ClientWorld::object_limit;
        }
    } // namespace
    bool ClientEffects::begin(std::uint64_t world)
    {
        if (!world || world <= frame_.world)
            return false;
        frame_ = {};
        frame_.world = world;
        return true;
    }
    bool ClientEffects::receive(ClientEffectsFrame&& next, std::string& error)
    {
        error.clear();
        auto fail = [&](const char* why)
        {
            error = why;
            return false;
        };
        if (!next.world || next.world != frame_.world || !next.sequence || next.sequence <= frame_.sequence)
            return fail("Stale client effects frame");
        if (!std::isfinite(next.time) || next.time < frame_.time || next.time < 0)
            return fail("Invalid client effects clock");
        if (next.billboards.size() > record_limit || next.models.size() > record_limit ||
            next.polygrids.size() > record_limit || next.tracers.size() > record_limit ||
            next.lights.size() > record_limit || next.volumes.size() > record_limit)
            return fail("Client effects record limit exceeded");
        std::set<EffectId> ids;
        auto id = [&](EffectId i)
        { return i.value && i.value != UINT64_MAX && i.kind <= EffectKind::Tracer && ids.insert(i).second; };
        std::size_t particles = 0, heights = 0;
        for (const auto& item : next.billboards)
        {
            if (!id(item.id) || item.id.kind > EffectKind::ParticleStream)
                return fail("Invalid/duplicate billboard identity");
            if (const auto* s = std::get_if<EffectSprite>(&item.value))
            {
                if (!pose(s->pose) || !physics_vector(s->scale) || !physics_vector(s->color) ||
                    !std::isfinite(s->alpha) || !std::isfinite(s->track.milliseconds) || s->track.milliseconds < 0 ||
                    s->clip_polygon.size() > 4096)
                    return fail("Invalid effect sprite");
                for (auto p : s->clip_polygon)
                    if (!physics_vector(p))
                        return fail("Invalid effect mark polygon");
            }
            else
            {
                const auto& p = std::get<EffectParticles>(item.value);
                if (p.particles.size() > particle_limit - particles || !text(p.texture) || !physics_vector(p.origin) ||
                    !std::isfinite(p.half_extent) || p.half_extent < 0 || !std::isfinite(p.alpha))
                    return fail("Invalid effect particle system");
                particles += p.particles.size();
                // Existing renderer skips bad cosmetic particles with per-entry diagnostics.
                // Preserve that policy; this local frame is not an untrusted wire decoder.
            }
        }
        for (const auto& m : next.models)
            if (!id(m.id) || m.id.kind < EffectKind::ExplosionModel || m.id.kind > EffectKind::Casing ||
                !pose(m.pose) || !physics_vector(m.scale) || !std::isfinite(m.alpha) || !text(m.skin) || !text(m.label))
                return fail("Invalid effect model");
        for (const auto& p : next.polygrids)
        {
            if (!id(p.id) || p.id.kind != EffectKind::PolyGrid || !object(p.owner) || !pose(p.pose) ||
                !physics_vector(p.dimensions) || p.width > 1024 || p.heights.size() > height_limit - heights ||
                !std::isfinite(p.alpha) || !text(p.texture) || !text(p.error))
                return fail("Invalid effect PolyGrid");
            heights += p.heights.size();
        }
        for (const auto& t : next.tracers)
            if (!id(t.id) || t.id.kind != EffectKind::Tracer || !physics_vector(t.center) ||
                !physics_vector(t.forward) || !physics_vector(t.right))
                return fail("Invalid effect tracer");
        ObjectId previous;
        for (const auto& v : next.volumes)
        {
            if (!object(v.owner) || (previous && previous.slot >= v.owner.slot) || !v.shape)
                return fail("Invalid effect fog volume");
            previous = v.owner;
        }
        frame_ = std::move(next);
        return true;
    }
} // namespace OpenBlood2
