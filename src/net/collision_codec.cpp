#include "net/collision_codec.h"

namespace OpenBlood2::Net
{
    namespace
    {
        bool bounded(double v)
        {
            return std::isfinite(v) && std::abs(v) <= 1e7;
        }
        bool vector(DVector v)
        {
            return bounded(v.x) && bounded(v.y) && bounded(v.z);
        }
        bool same(DVector a, DVector b)
        {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }
        void write_vector(Writer& w, DVector v)
        {
            w.f32(v.x);
            w.f32(v.y);
            w.f32(v.z);
        }
        DVector read_vector(Reader& r)
        {
            return {r.f32(), r.f32(), r.f32()};
        }
        bool read_bool(Reader& r, bool& b)
        {
            const auto v = r.u8();
            b = v != 0;
            return r.good() && v <= 1;
        }
    } // namespace
    bool valid_collision_motion(const CollisionMotion& m)
    {
        return vector(m.dimensions) && m.dimensions.x >= 0 && m.dimensions.y >= 0 && m.dimensions.z >= 0 &&
               vector(m.velocity) && vector(m.acceleration) && vector(m.shape_origin);
    }
    bool valid_collision_values(const ClientCollisionValues& v)
    {
        if (!v.enabled)
            return true;
        const auto q = v.basis;
        const double norm = double(q.m_Spin) * q.m_Spin + double(q.m_Vec.x) * q.m_Vec.x +
                            double(q.m_Vec.y) * q.m_Vec.y + double(q.m_Vec.z) * q.m_Vec.z;
        if (!valid_collision_motion(v.motion) || !std::isfinite(norm) || norm < .999 || norm > 1.001 ||
            (v.ignored_by ? v.ignored_by.slot >= 65536 || !v.ignored_by.generation : v.ignored_by.generation != 0) ||
            (v.world_model && v.container) || (v.shape_transformed && !v.world_model) ||
            (v.volume_brush && !v.container))
            return false;
        if (v.environment)
        {
            const auto& e = *v.environment;
            if (!v.container || !vector(e.current) || !bounded(e.gravity) || !bounded(e.viscosity) || e.viscosity < 0 ||
                e.viscosity > 1)
                return false;
        }
        return true;
    }
    bool same_collision_metadata(const ClientCollisionValues& a, const ClientCollisionValues& b)
    {
        if (a.enabled != b.enabled)
            return false;
        if (!a.enabled)
            return true;
        if (a.ignored_by != b.ignored_by || !same(a.basis.m_Vec, b.basis.m_Vec) || a.basis.m_Spin != b.basis.m_Spin ||
            a.volume_brush != b.volume_brush || a.world_model != b.world_model || a.container != b.container ||
            a.dynamic != b.dynamic || a.pushable != b.pushable || a.shape_transformed != b.shape_transformed ||
            a.priority != b.priority || bool(a.environment) != bool(b.environment))
            return false;
        if (a.environment)
        {
            const auto& x = *a.environment;
            const auto& y = *b.environment;
            return x.code == y.code && same(x.current, y.current) && x.gravity == y.gravity &&
                   x.viscosity == y.viscosity && x.hidden == y.hidden;
        }
        return true;
    }
    void write_collision_motion(Writer& w, const CollisionMotion& m)
    {
        write_vector(w, m.dimensions);
        write_vector(w, m.velocity);
        write_vector(w, m.acceleration);
        write_vector(w, m.shape_origin);
    }
    CollisionMotion read_collision_motion(Reader& r)
    {
        return {read_vector(r), read_vector(r), read_vector(r), read_vector(r)};
    }
    void write_collision_values(Writer& w, const ClientCollisionValues& v)
    {
        w.u8(v.enabled);
        if (!v.enabled)
            return;
        write_collision_motion(w, v.motion);
        w.u32(v.ignored_by.slot);
        w.u32(v.ignored_by.generation);
        write_vector(w, v.basis.m_Vec);
        w.f32(v.basis.m_Spin);
        w.u8(std::uint8_t(v.volume_brush | v.world_model << 1 | v.container << 2 | v.dynamic << 3 | v.pushable << 4 |
                          v.shape_transformed << 5));
        w.u8(v.priority);
        w.u8(bool(v.environment));
        if (v.environment)
        {
            const auto& e = *v.environment;
            w.u16(std::uint16_t(e.code));
            write_vector(w, e.current);
            w.f32(e.gravity);
            w.f32(e.viscosity);
            w.u8(e.hidden);
        }
    }
    bool read_collision_values(Reader& r, ClientCollisionValues& out)
    {
        ClientCollisionValues v;
        if (!read_bool(r, v.enabled))
            return false;
        if (v.enabled)
        {
            v.motion = read_collision_motion(r);
            v.ignored_by = {r.u32(), r.u32()};
            v.basis.m_Vec = read_vector(r);
            v.basis.m_Spin = r.f32();
            const auto flags = r.u8();
            if (flags > 63)
                return false;
            v.volume_brush = flags & 1;
            v.world_model = flags & 2;
            v.container = flags & 4;
            v.dynamic = flags & 8;
            v.pushable = flags & 16;
            v.shape_transformed = flags & 32;
            v.priority = r.u8();
            bool environment = false;
            if (!read_bool(r, environment))
                return false;
            if (environment)
            {
                ContainerEnvironment e;
                e.code = ContainerCode(r.u16());
                e.current = read_vector(r);
                e.gravity = r.f32();
                e.viscosity = r.f32();
                if (!read_bool(r, e.hidden))
                    return false;
                v.environment = e;
            }
        }
        if (!r.good() || !valid_collision_values(v))
            return false;
        out = std::move(v);
        return true;
    }
} // namespace OpenBlood2::Net
