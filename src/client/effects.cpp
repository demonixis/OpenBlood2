#include "client/effects.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace OpenBlood2
{
    bool valid_line_effect(const LineEffectGeometry& line)
    {
        const auto unit = [](float n) { return std::isfinite(n) && n >= 0 && n <= 1; };
        const auto color = [&](DVector c) { return unit(c.x) && unit(c.y) && unit(c.z); };
        return physics_vector(line.from) && physics_vector(line.to) && color(line.start_color) &&
               color(line.end_color) && unit(line.start_alpha) && unit(line.end_alpha);
    }
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
    bool ClientEffects::receive(ClientEffectsFrame&& next, std::string& error, bool drop_invalid)
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
        if (!std::isfinite(next.far_z) || next.far_z < 0 ||
            (next.sky_fog && (!std::isfinite(next.sky_fog_near) || !std::isfinite(next.sky_fog_far) ||
                              next.sky_fog_far <= next.sky_fog_near)))
            return fail("Invalid client far plane or sky fog");
        if (next.billboards.size() > record_limit || next.models.size() > record_limit ||
            next.polygrids.size() > record_limit || next.tracers.size() > record_limit ||
            next.lines.size() > record_limit ||
            next.lights.size() > record_limit || next.volumes.size() > record_limit)
            return fail("Client effects record limit exceeded");
        std::set<EffectId> ids;
        auto id = [&](EffectId i)
        { return i.value && i.value != UINT64_MAX && i.kind <= EffectKind::LegacySmoke && ids.insert(i).second; };
        std::size_t particles = 0, heights = 0;
        // Returns the refusal reason for one item, or nullptr when it is valid.
        auto billboard = [&](const auto& item) -> const char*
        {
            if (!id(item.id) || (item.id.kind > EffectKind::ParticleStream && item.id.kind != EffectKind::LegacySmoke))
                return "Invalid/duplicate billboard identity";
            if (const auto* s = std::get_if<EffectSprite>(&item.value))
            {
                if (!pose(s->pose) || !physics_vector(s->scale) || !physics_vector(s->color) ||
                    !std::isfinite(s->alpha) || !std::isfinite(s->track.milliseconds) || s->track.milliseconds < 0 ||
                    s->clip_polygon.size() > 4096)
                    return "Invalid effect sprite";
                for (auto p : s->clip_polygon)
                    if (!physics_vector(p))
                        return "Invalid effect mark polygon";
                return nullptr;
            }
            const auto& p = std::get<EffectParticles>(item.value);
            if (p.particles.size() > particle_limit - particles || !text(p.texture) || !physics_vector(p.origin) ||
                !std::isfinite(p.half_extent) || p.half_extent < 0 || !std::isfinite(p.alpha))
                return "Invalid effect particle system";
            particles += p.particles.size();
            // Existing renderer skips bad cosmetic particles with per-entry diagnostics.
            // Preserve that policy; this local frame is not an untrusted wire decoder.
            return nullptr;
        };
        auto model = [&](const auto& m) -> const char*
        {
            return !id(m.id) || m.id.kind < EffectKind::ExplosionModel || m.id.kind > EffectKind::Casing ||
                           !pose(m.pose) || !physics_vector(m.scale) || !std::isfinite(m.alpha) || !text(m.skin) ||
                           !text(m.label)
                       ? "Invalid effect model"
                       : nullptr;
        };
        auto polygrid = [&](const auto& p) -> const char*
        {
            if (!id(p.id) || p.id.kind != EffectKind::PolyGrid || !object(p.owner) || !pose(p.pose) ||
                !physics_vector(p.dimensions) || p.width > 1024 || p.heights.size() > height_limit - heights ||
                !std::isfinite(p.alpha) || !text(p.texture) || !text(p.error))
                return "Invalid effect PolyGrid";
            heights += p.heights.size();
            return nullptr;
        };
        auto tracer = [&](const auto& t) -> const char*
        {
            return !id(t.id) || t.id.kind != EffectKind::Tracer || !physics_vector(t.center) ||
                           !physics_vector(t.forward) || !physics_vector(t.right)
                       ? "Invalid effect tracer"
                       : nullptr;
        };
        ObjectId previous;
        auto volume = [&](const auto& v) -> const char*
        {
            if (!object(v.owner) || (previous && previous.slot >= v.owner.slot) || !v.shape)
                return "Invalid effect fog volume";
            previous = v.owner;
            return nullptr;
        };
        std::uint64_t dropped = 0;
        const char* reason = nullptr;
        auto filter = [&](auto& items, auto&& check)
        {
            std::erase_if(items,
                          [&](const auto& item)
                          {
                              const char* why = reason ? nullptr : check(item);
                              if (why && drop_invalid)
                              {
                                  ++dropped;
                                  drop_error = why;
                                  return true;
                              }
                              reason = why ? why : reason;
                              return false;
                          });
        };
        filter(next.billboards, billboard);
        filter(next.models, model);
        filter(next.polygrids, polygrid);
        filter(next.tracers, tracer);
        filter(next.lines,
               [&](const auto& line) -> const char*
               {
                   return id(line.id) && (line.id.kind == EffectKind::CastLine || line.id.kind == EffectKind::DetachedLine) &&
                                  valid_line_effect(line.geometry)
                              ? nullptr
                              : "Invalid effect line";
               });
        filter(next.volumes, volume);
        if (reason)
            return fail(reason);
        dropped_items += dropped;
        frame_ = std::move(next);
        return true;
    }
} // namespace OpenBlood2
