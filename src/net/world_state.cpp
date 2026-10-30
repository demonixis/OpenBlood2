#include "net/world_state.h"
#include "net/admission.h"
#include "net/codec.h"
#include "net/collision_codec.h"
#include "net/transport.h"
#include <algorithm>

namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t magic = 0x534d3242;
        constexpr std::size_t header_bytes = 40, motion_bytes = 156;
        static_assert(header_bytes + state_objects_per_packet * motion_bytes <= packet_limit(Channel::State));
        template <class T> ObjectMotion values(const T& object)
        {
            return {object.id,    object.pose_revision, object.pose,         object.scale,           object.color,
                    object.alpha, object.animation,     object.sprite_track, object.collision.motion};
        }
        bool bounded(double value, double limit)
        {
            return std::isfinite(value) && std::abs(value) <= limit;
        }
        bool vector(DVector v)
        {
            return bounded(v.x, 1e7) && bounded(v.y, 1e7) && bounded(v.z, 1e7);
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
        bool read_bool(Reader& r, bool& value)
        {
            const auto b = r.u8();
            value = b != 0;
            return r.good() && b <= 1;
        }
    } // namespace
    ObjectMotion motion_values(const ObjectState& object)
    {
        return values(object);
    }
    ObjectMotion motion_values(const ClientObject& object)
    {
        return values(object);
    }
    void apply_motion(const ObjectMotion& m, ClientObject& object)
    {
        object.pose = m.pose;
        object.scale = m.scale;
        object.color = m.color;
        object.alpha = m.alpha;
        object.animation = m.animation;
        object.sprite_track = m.sprite_track;
        if (object.collision.enabled)
            object.collision.motion = m.collision;
    }
    bool valid_state(const WorldState& state)
    {
        if (!state.world || !state.membership || state.sequence < state.membership || !bounded(state.time, 1e12) ||
            state.time < 0 || state.objects.empty() || state.objects.size() > state_objects_per_packet)
            return false;
        ObjectId previous;
        for (const auto& m : state.objects)
        {
            const auto q = m.pose.rotation;
            const double norm = double(q.m_Spin) * q.m_Spin + double(q.m_Vec.x) * q.m_Vec.x +
                                double(q.m_Vec.y) * q.m_Vec.y + double(q.m_Vec.z) * q.m_Vec.z;
            if (!m.id || !m.id.generation || m.id.slot >= ClientWorld::object_limit ||
                (previous && previous.slot >= m.id.slot) || !vector(m.pose.position) || !bounded(norm, 1.001) ||
                norm < .999 || !vector(m.scale) || !vector(m.color) || !bounded(m.alpha, 1) || m.alpha < 0 ||
                !bounded(m.animation.time, 1e12) || m.animation.time < 0 ||
                !bounded(m.sprite_track.milliseconds, 1e12) || m.sprite_track.milliseconds < 0 ||
                !valid_collision_motion(m.collision))
                return false;
            previous = m.id;
        }
        return true;
    }
    bool encode_world_state(const WorldState& state, std::vector<std::uint8_t>& out)
    {
        if (!valid_state(state))
            return false;
        Writer w(packet_limit(Channel::State));
        w.u32(magic);
        w.u16(protocol_version);
        w.u64(state.world);
        w.u64(state.membership);
        w.u64(state.sequence);
        w.f64(state.time);
        w.u16(std::uint16_t(state.objects.size()));
        for (const auto& m : state.objects)
        {
            w.u32(m.id.slot);
            w.u32(m.id.generation);
            w.u64(m.pose_revision);
            write_vector(w, m.pose.position);
            write_vector(w, m.pose.rotation.m_Vec);
            w.f32(m.pose.rotation.m_Spin);
            write_vector(w, m.scale);
            write_vector(w, m.color);
            w.f32(m.alpha);
            w.u32(m.animation.animation);
            w.f64(m.animation.time);
            w.u8(m.animation.looping);
            w.u8(m.animation.done);
            w.u8(m.animation.first);
            w.u64(m.animation.revision);
            w.f64(m.sprite_track.milliseconds);
            w.u32(m.sprite_track.frame);
            w.u8(m.sprite_track.has_frame);
            write_collision_motion(w, m.collision);
        }
        if (!w.good())
            return false;
        out = w.bytes();
        return true;
    }
    bool decode_world_state(std::span<const std::uint8_t> bytes, WorldState& out)
    {
        if (bytes.size() < header_bytes || bytes.size() > packet_limit(Channel::State))
            return false;
        Reader r(bytes);
        if (r.u32() != magic || r.u16() != protocol_version)
            return false;
        WorldState state;
        state.world = r.u64();
        state.membership = r.u64();
        state.sequence = r.u64();
        state.time = r.f64();
        const auto count = r.u16();
        if (!count || count > state_objects_per_packet || r.remaining() != count * motion_bytes)
            return false;
        state.objects.reserve(count);
        for (unsigned i = 0; i < count; ++i)
        {
            ObjectMotion m;
            m.id = {r.u32(), r.u32()};
            m.pose_revision = r.u64();
            m.pose.position = read_vector(r);
            m.pose.rotation.m_Vec = read_vector(r);
            m.pose.rotation.m_Spin = r.f32();
            m.scale = read_vector(r);
            m.color = read_vector(r);
            m.alpha = r.f32();
            m.animation.animation = r.u32();
            m.animation.time = r.f64();
            if (!read_bool(r, m.animation.looping) || !read_bool(r, m.animation.done) ||
                !read_bool(r, m.animation.first))
                return false;
            m.animation.revision = r.u64();
            m.sprite_track.milliseconds = r.f64();
            m.sprite_track.frame = r.u32();
            if (!read_bool(r, m.sprite_track.has_frame))
                return false;
            m.collision = read_collision_motion(r);
            state.objects.push_back(m);
        }
        if (!r.done() || !valid_state(state))
            return false;
        out = std::move(state);
        return true;
    }
    bool encode_state_packets(const WorldSnapshot& snapshot, std::uint64_t membership,
                              std::vector<std::vector<std::uint8_t>>& out)
    {
        if (!valid_snapshot(snapshot) || !membership || membership > snapshot.sequence)
            return false;
        std::vector<std::vector<std::uint8_t>> packets;
        WorldState state{snapshot.world, membership, snapshot.sequence, snapshot.time, {}};
        for (const auto& object : snapshot.objects)
        {
            state.objects.push_back(motion_values(object));
            if (state.objects.size() == state_objects_per_packet)
            {
                packets.emplace_back();
                if (!encode_world_state(state, packets.back()))
                    return false;
                state.objects.clear();
            }
        }
        if (!state.objects.empty())
        {
            packets.emplace_back();
            if (!encode_world_state(state, packets.back()))
                return false;
        }
        out = std::move(packets);
        return true;
    }
} // namespace OpenBlood2::Net
