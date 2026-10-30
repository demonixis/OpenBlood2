#include "net/world_snapshot.h"
#include "engine/resources.h"
#include "net/admission.h"
#include "net/codec.h"
#include "net/transport.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t magic = 0x574d3242, chunk_magic = 0x424d3242;
        constexpr std::size_t chunk_header = 30;
        constexpr std::size_t chunk_data = packet_limit(Channel::Baseline) - chunk_header;
        constexpr std::size_t minimum_object_bytes = 133;
        bool bounded(double v, double limit = 1e7)
        {
            return std::isfinite(v) && std::abs(v) <= limit;
        }
        bool vector(DVector v)
        {
            return bounded(v.x) && bounded(v.y) && bounded(v.z);
        }
        bool identity(ObjectId id, bool optional = true)
        {
            return id ? id.slot < ClientWorld::object_limit && id.generation : optional && id.generation == 0;
        }
        bool text(std::string_view value)
        {
            return value.size() <= 255 &&
                   std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= 32 && c != 127; });
        }
        bool resource(std::string_view value)
        {
            std::string normalized;
            return value.empty() || (text(value) && resource_path(value, normalized));
        }
        template <class To, class From> void copy_object(To& to, const From& from)
        {
            to.id = from.id;
            to.type = from.type;
            to.flags = from.flags;
            to.user_flags = from.user_flags;
            to.pose = from.pose;
            to.scale = from.scale;
            to.color = from.color;
            to.alpha = from.alpha;
            to.filename = from.filename;
            to.skin = from.skin;
            to.animation = from.animation;
            to.sprite_track = from.sprite_track;
            to.hidden_nodes = from.hidden_nodes;
            to.parent = from.parent;
            to.attached = from.attached;
            to.attachment_resolved = from.attachment_resolved;
            to.world_model_index = from.world_model_index;
            to.world_model_bound = from.world_model_bound;
        }
        template <class To, class From> void copy_frame(To& to, const From& from)
        {
            to.world = from.world;
            to.sequence = from.sequence;
            to.time = from.time;
            to.sky = from.sky;
            to.model_add = from.model_add;
            to.model_dir_add = from.model_dir_add;
            to.player = from.player;
            to.seeing_eye = from.seeing_eye;
            to.orb = from.orb;
            to.player_alive = from.player_alive;
            to.polygrids = from.polygrids;
            to.particle_streams = from.particle_streams;
        }
        void write_id(Writer& w, ObjectId id)
        {
            w.u32(id.slot);
            w.u32(id.generation);
        }
        ObjectId read_id(Reader& r)
        {
            return {r.u32(), r.u32()};
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
        bool valid_effects(const std::vector<ClientOwnedEffect>& effects)
        {
            if (effects.size() > ClientWorld::object_limit)
                return false;
            std::vector<std::uint64_t> ids;
            ids.reserve(effects.size());
            for (const auto& e : effects)
            {
                if (!e.id || !identity(e.owner))
                    return false;
                ids.push_back(e.id);
            }
            std::sort(ids.begin(), ids.end());
            return std::adjacent_find(ids.begin(), ids.end()) == ids.end();
        }
        void write_effects(Writer& w, const std::vector<ClientOwnedEffect>& effects)
        {
            w.u32(std::uint32_t(effects.size()));
            for (const auto& e : effects)
            {
                w.u64(e.id);
                write_id(w, e.owner);
            }
        }
        bool read_effects(Reader& r, std::vector<ClientOwnedEffect>& effects)
        {
            const auto count = r.u32();
            if (count > ClientWorld::object_limit || count > r.remaining() / 16)
                return false;
            effects.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i)
                effects.push_back({r.u64(), read_id(r)});
            return r.good();
        }
    } // namespace
    WorldSnapshot snapshot_values(const ClientWorldFrame& frame)
    {
        WorldSnapshot result;
        copy_frame(result, frame);
        result.objects.resize(frame.objects.size());
        for (std::size_t i = 0; i < frame.objects.size(); ++i)
        {
            copy_object(result.objects[i], frame.objects[i]);
            result.objects[i].has_model = bool(frame.objects[i].model);
            result.objects[i].has_sprite = bool(frame.objects[i].sprite);
        }
        return result;
    }
    ClientWorldFrame snapshot_frame_values(const WorldSnapshot& snapshot)
    {
        ClientWorldFrame result;
        copy_frame(result, snapshot);
        result.objects.resize(snapshot.objects.size());
        for (std::size_t i = 0; i < snapshot.objects.size(); ++i)
            copy_object(result.objects[i], snapshot.objects[i]);
        return result;
    }
    bool valid_snapshot(const WorldSnapshot& s)
    {
        if (!s.world || !s.sequence || !bounded(s.time, 1e12) || s.time < 0 ||
            s.objects.size() > ClientWorld::object_limit || !vector(s.model_add) || !vector(s.model_dir_add) ||
            !vector(s.sky.min) || !vector(s.sky.max) || !vector(s.sky.view_min) || !vector(s.sky.view_max) ||
            !identity(s.player) || !identity(s.seeing_eye) || !identity(s.orb) || !valid_effects(s.polygrids) ||
            !valid_effects(s.particle_streams))
            return false;
        for (auto id : s.sky.objects)
            if (!identity(id))
                return false;
        ObjectId previous;
        for (const auto& o : s.objects)
        {
            const auto q = o.pose.rotation;
            const double norm = double(q.m_Spin) * q.m_Spin + double(q.m_Vec.x) * q.m_Vec.x +
                                double(q.m_Vec.y) * q.m_Vec.y + double(q.m_Vec.z) * q.m_Vec.z;
            if (!identity(o.id, false) || (previous && previous.slot >= o.id.slot) ||
                (o.type != OT_NORMAL && o.type != OT_MODEL && o.type != OT_SPRITE && o.type != OT_LIGHT &&
                 o.type != OT_WORLDMODEL && o.type != OT_CONTAINER) ||
                !vector(o.pose.position) || !bounded(norm, 1.001) || norm < .999 || !vector(o.scale) ||
                !vector(o.color) || !bounded(o.alpha, 1) || o.alpha < 0 || !text(o.filename) || !resource(o.skin) ||
                !identity(o.parent) || (o.attachment_resolved && !o.attached) || (!o.attached && o.parent) ||
                (o.has_model && (o.type != OT_MODEL || o.filename.empty() || !resource(o.filename))) ||
                (o.has_sprite && (o.type != OT_SPRITE || o.filename.empty() || !resource(o.filename))) ||
                (o.world_model_bound && ((o.type != OT_WORLDMODEL && o.type != OT_CONTAINER) || !o.world_model_index ||
                                         o.world_model_index >= 65536)) ||
                !bounded(o.animation.time, 1e12) || o.animation.time < 0 ||
                !bounded(o.sprite_track.milliseconds, 1e12) || o.sprite_track.milliseconds < 0 ||
                o.hidden_nodes.size() > 256 ||
                !std::all_of(o.hidden_nodes.begin(), o.hidden_nodes.end(), [](auto n) { return n <= 1; }))
                return false;
            previous = o.id;
        }
        return true;
    }
    bool encode_world_snapshot(const WorldSnapshot& s, std::vector<std::uint8_t>& bytes)
    {
        if (!valid_snapshot(s))
            return false;
        Writer w(world_snapshot_limit);
        w.u32(magic);
        w.u16(protocol_version);
        w.u64(s.world);
        w.u64(s.sequence);
        w.f64(s.time);
        write_vector(w, s.sky.min);
        write_vector(w, s.sky.max);
        write_vector(w, s.sky.view_min);
        write_vector(w, s.sky.view_max);
        for (auto id : s.sky.objects)
            write_id(w, id);
        write_vector(w, s.model_add);
        write_vector(w, s.model_dir_add);
        write_id(w, s.player);
        write_id(w, s.seeing_eye);
        write_id(w, s.orb);
        w.u8(s.player_alive);
        w.u32(std::uint32_t(s.objects.size()));
        for (const auto& o : s.objects)
        {
            write_id(w, o.id);
            w.u16(o.type);
            w.u32(o.flags);
            w.u32(o.user_flags);
            write_vector(w, o.pose.position);
            write_vector(w, o.pose.rotation.m_Vec);
            w.f32(o.pose.rotation.m_Spin);
            write_vector(w, o.scale);
            write_vector(w, o.color);
            w.f32(o.alpha);
            w.text(o.filename, 255);
            w.text(o.skin, 255);
            w.u8(o.has_model);
            w.u8(o.has_sprite);
            w.u32(o.animation.animation);
            w.f64(o.animation.time);
            w.u8(o.animation.looping);
            w.u8(o.animation.done);
            w.u8(o.animation.first);
            w.u64(o.animation.revision);
            w.f64(o.sprite_track.milliseconds);
            w.u32(o.sprite_track.frame);
            w.u8(o.sprite_track.has_frame);
            w.u16(std::uint16_t(o.hidden_nodes.size()));
            w.raw(o.hidden_nodes);
            write_id(w, o.parent);
            w.u8(o.attached);
            w.u8(o.attachment_resolved);
            w.u32(o.world_model_index);
            w.u8(o.world_model_bound);
        }
        write_effects(w, s.polygrids);
        write_effects(w, s.particle_streams);
        if (!w.good())
            return false;
        bytes = w.bytes();
        return true;
    }
    bool decode_world_snapshot(std::span<const std::uint8_t> bytes, WorldSnapshot& out)
    {
        if (bytes.size() > world_snapshot_limit)
            return false;
        Reader r(bytes);
        if (r.u32() != magic || r.u16() != protocol_version)
            return false;
        WorldSnapshot s;
        s.world = r.u64();
        s.sequence = r.u64();
        s.time = r.f64();
        s.sky.min = read_vector(r);
        s.sky.max = read_vector(r);
        s.sky.view_min = read_vector(r);
        s.sky.view_max = read_vector(r);
        for (auto& id : s.sky.objects)
            id = read_id(r);
        s.model_add = read_vector(r);
        s.model_dir_add = read_vector(r);
        s.player = read_id(r);
        s.seeing_eye = read_id(r);
        s.orb = read_id(r);
        if (!read_bool(r, s.player_alive))
            return false;
        const auto count = r.u32();
        if (count > ClientWorld::object_limit || count > r.remaining() / minimum_object_bytes)
            return false;
        s.objects.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            ObjectState o;
            o.id = read_id(r);
            o.type = r.u16();
            o.flags = r.u32();
            o.user_flags = r.u32();
            o.pose.position = read_vector(r);
            o.pose.rotation.m_Vec = read_vector(r);
            o.pose.rotation.m_Spin = r.f32();
            o.scale = read_vector(r);
            o.color = read_vector(r);
            o.alpha = r.f32();
            o.filename = r.text(255);
            o.skin = r.text(255);
            if (!read_bool(r, o.has_model) || !read_bool(r, o.has_sprite))
                return false;
            o.animation.animation = r.u32();
            o.animation.time = r.f64();
            if (!read_bool(r, o.animation.looping) || !read_bool(r, o.animation.done) ||
                !read_bool(r, o.animation.first))
                return false;
            o.animation.revision = r.u64();
            o.sprite_track.milliseconds = r.f64();
            o.sprite_track.frame = r.u32();
            if (!read_bool(r, o.sprite_track.has_frame))
                return false;
            const auto hidden = r.u16();
            if (hidden > 256)
                return false;
            const auto nodes = r.raw(hidden);
            o.hidden_nodes.assign(nodes.begin(), nodes.end());
            o.parent = read_id(r);
            if (!read_bool(r, o.attached) || !read_bool(r, o.attachment_resolved))
                return false;
            o.world_model_index = r.u32();
            if (!read_bool(r, o.world_model_bound))
                return false;
            s.objects.push_back(std::move(o));
        }
        if (!read_effects(r, s.polygrids) || !read_effects(r, s.particle_streams) || !r.done() || !valid_snapshot(s))
            return false;
        out = std::move(s);
        return true;
    }
    bool encode_baseline_chunks(const WorldSnapshot& s, std::vector<std::vector<std::uint8_t>>& out)
    {
        std::vector<std::uint8_t> bytes;
        if (!encode_world_snapshot(s, bytes))
            return false;
        return encode_world_chunks(s.world, s.sequence, bytes, out);
    }
    bool encode_world_chunks(std::uint64_t world, std::uint64_t sequence, std::span<const std::uint8_t> bytes,
                             std::vector<std::vector<std::uint8_t>>& out)
    {
        if (!world || !sequence || bytes.empty() || bytes.size() > world_snapshot_limit)
            return false;
        std::vector<std::vector<std::uint8_t>> chunks;
        for (std::size_t offset = 0; offset < bytes.size(); offset += chunk_data)
        {
            Writer w(packet_limit(Channel::Baseline));
            w.u32(chunk_magic);
            w.u16(protocol_version);
            w.u64(world);
            w.u64(sequence);
            w.u32(std::uint32_t(bytes.size()));
            w.u32(std::uint32_t(offset));
            w.raw(std::span(bytes).subspan(offset, std::min(chunk_data, bytes.size() - offset)));
            if (!w.good())
                return false;
            chunks.push_back(w.bytes());
        }
        out = std::move(chunks);
        return true;
    }
    BaselineResult BaselineReceiver::receive(std::span<const std::uint8_t> bytes, WorldSnapshot& out)
    {
        WorldUpdate update;
        const auto status = receive(bytes, update);
        if (status != BaselineResult::Complete)
            return status;
        auto* snapshot = std::get_if<WorldSnapshot>(&update);
        if (!snapshot)
            return BaselineResult::Refused;
        out = std::move(*snapshot);
        return BaselineResult::Complete;
    }
    BaselineResult BaselineReceiver::receive(std::span<const std::uint8_t> bytes, WorldUpdate& out)
    {
        if (bytes.size() <= chunk_header || bytes.size() > packet_limit(Channel::Baseline))
            return BaselineResult::Refused;
        Reader r(bytes);
        if (r.u32() != chunk_magic || r.u16() != protocol_version)
            return BaselineResult::Refused;
        const auto world = r.u64(), sequence = r.u64();
        const auto total = r.u32(), offset = r.u32();
        const auto data = r.raw(r.remaining());
        if (!r.done() || !world_ || world != world_ || !sequence || sequence <= completed_ || !total ||
            total > world_snapshot_limit || offset >= total ||
            data.size() != std::min(chunk_data, std::size_t(total - offset)) || offset != bytes_.size() ||
            (!bytes_.empty() && (sequence != sequence_ || total != total_)))
            return BaselineResult::Refused;
        // The reliable channel has no missing/reordered chunk recovery. Refusals
        // leave the pending prefix intact; the endpoint can terminate the session.
        if (bytes_.empty())
        {
            sequence_ = sequence;
            total_ = total;
            bytes_.reserve(total);
        }
        bytes_.insert(bytes_.end(), data.begin(), data.end());
        if (bytes_.size() < total)
            return BaselineResult::Pending;
        WorldUpdate result;
        WorldSnapshot snapshot;
        WorldPatch patch;
        bool valid = false;
        if (decode_world_snapshot(bytes_, snapshot))
        {
            valid = snapshot.world == world && snapshot.sequence == sequence;
            result = std::move(snapshot);
        }
        else if (decode_world_patch(bytes_, patch))
        {
            valid = patch.values.world == world && patch.values.sequence == sequence;
            result = std::move(patch);
        }
        if (!valid)
        {
            bytes_.clear();
            sequence_ = 0;
            total_ = 0;
            return BaselineResult::Refused;
        }
        out = std::move(result);
        completed_ = sequence;
        sequence_ = 0;
        total_ = 0;
        bytes_.clear();
        return BaselineResult::Complete;
    }
} // namespace OpenBlood2::Net
