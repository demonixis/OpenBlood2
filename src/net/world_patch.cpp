#include "net/admission.h"
#include "net/codec.h"
#include "net/collision_codec.h"
#include "net/world_snapshot.h"
#include <algorithm>

namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t patch_magic = 0x504d3242;
        bool same(DVector a, DVector b)
        {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }
        bool same(const std::vector<ClientOwnedEffect>& a, const std::vector<ClientOwnedEffect>& b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const auto& x, const auto& y)
                                                      { return x.id == y.id && x.owner == y.owner; });
        }
        bool same_headers(const WorldSnapshot& a, const WorldSnapshot& b)
        {
            return same(a.sky.min, b.sky.min) && same(a.sky.max, b.sky.max) && same(a.sky.view_min, b.sky.view_min) &&
                   same(a.sky.view_max, b.sky.view_max) && a.sky.objects == b.sky.objects &&
                   same(a.model_add, b.model_add) && same(a.model_dir_add, b.model_dir_add) && a.player == b.player &&
                   a.seeing_eye == b.seeing_eye && a.orb == b.orb && a.player_alive == b.player_alive &&
                   same(a.polygrids, b.polygrids) && same(a.particle_streams, b.particle_streams);
        }
        bool same_metadata(const ObjectState& a, const ObjectState& b)
        {
            // Frequent pose/scale/color/animation values belong to State. Initial
            // values still accompany a reliable creation or resource change.
            return a.id == b.id && a.pose_revision == b.pose_revision && a.type == b.type && a.flags == b.flags &&
                   a.user_flags == b.user_flags && a.filename == b.filename && a.skin == b.skin &&
                   a.has_model == b.has_model && a.has_sprite == b.has_sprite && a.hidden_nodes == b.hidden_nodes &&
                   a.parent == b.parent && a.attached == b.attached && a.attachment_resolved == b.attachment_resolved &&
                   a.world_model_index == b.world_model_index && a.world_model_bound == b.world_model_bound &&
                   same_collision_metadata(a.collision, b.collision) &&
                   a.particle_stream.has_value() == b.particle_stream.has_value() &&
                   (!a.particle_stream || same_particle_stream(*a.particle_stream, *b.particle_stream)) &&
                   same_object_particles(a.object_particles, b.object_particles);
        }
        bool valid_patch(const WorldPatch& p)
        {
            if (!p.base || p.base >= p.values.sequence || !valid_snapshot(p.values) ||
                p.removed.size() > ClientWorld::object_limit)
                return false;
            ObjectId previous;
            for (auto id : p.removed)
            {
                if (!id || id.slot >= ClientWorld::object_limit || !id.generation ||
                    (previous && previous.slot >= id.slot))
                    return false;
                previous = id;
            }
            return true;
        }
    } // namespace
    bool make_world_patch(const WorldSnapshot& before, const WorldSnapshot& after, WorldPatch& out, bool& changed)
    {
        if (!valid_snapshot(before) || !valid_snapshot(after) || before.world != after.world ||
            before.sequence >= after.sequence || before.time > after.time)
            return false;
        WorldPatch p;
        p.base = before.sequence;
        p.values.world = after.world;
        p.values.sequence = after.sequence;
        p.values.time = after.time;
        p.values.sky = after.sky;
        p.values.model_add = after.model_add;
        p.values.model_dir_add = after.model_dir_add;
        p.values.player = after.player;
        p.values.seeing_eye = after.seeing_eye;
        p.values.orb = after.orb;
        p.values.player_alive = after.player_alive;
        p.values.polygrids = after.polygrids;
        p.values.particle_streams = after.particle_streams;
        std::size_t left = 0, right = 0;
        while (left < before.objects.size() || right < after.objects.size())
        {
            if (right == after.objects.size() ||
                (left < before.objects.size() && before.objects[left].id.slot < after.objects[right].id.slot))
                p.removed.push_back(before.objects[left++].id);
            else if (left == before.objects.size() || after.objects[right].id.slot < before.objects[left].id.slot)
                p.values.objects.push_back(after.objects[right++]);
            else
            {
                const auto& a = before.objects[left++];
                const auto& b = after.objects[right++];
                if (b.id.generation < a.id.generation)
                    return false;
                if (a.id != b.id)
                    p.removed.push_back(a.id);
                if (!same_metadata(a, b))
                    p.values.objects.push_back(b);
            }
        }
        changed = !p.removed.empty() || !p.values.objects.empty() || !same_headers(before, after);
        out = std::move(p);
        return true;
    }
    bool encode_world_patch(const WorldPatch& p, std::vector<std::uint8_t>& out)
    {
        if (!valid_patch(p))
            return false;
        std::vector<std::uint8_t> values;
        if (!encode_world_snapshot(p.values, values))
            return false;
        Writer w(world_snapshot_limit);
        w.u32(patch_magic);
        w.u16(protocol_version);
        w.u64(p.base);
        w.u32(std::uint32_t(values.size()));
        w.raw(values);
        w.u32(std::uint32_t(p.removed.size()));
        for (auto id : p.removed)
        {
            w.u32(id.slot);
            w.u32(id.generation);
        }
        if (!w.good())
            return false;
        out = w.bytes();
        return true;
    }
    bool decode_world_patch(std::span<const std::uint8_t> bytes, WorldPatch& out)
    {
        if (bytes.size() > world_snapshot_limit)
            return false;
        Reader r(bytes);
        if (r.u32() != patch_magic || r.u16() != protocol_version)
            return false;
        WorldPatch p;
        p.base = r.u64();
        const auto size = r.u32();
        if (!decode_world_snapshot(r.raw(size), p.values))
            return false;
        const auto count = r.u32();
        if (count > ClientWorld::object_limit || count > r.remaining() / 8)
            return false;
        p.removed.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
            p.removed.push_back({r.u32(), r.u32()});
        if (!r.done() || !valid_patch(p))
            return false;
        out = std::move(p);
        return true;
    }
    bool encode_patch_chunks(const WorldPatch& p, std::vector<std::vector<std::uint8_t>>& out)
    {
        std::vector<std::uint8_t> bytes;
        return encode_world_patch(p, bytes) && encode_world_chunks(p.values.world, p.values.sequence, bytes, out);
    }
} // namespace OpenBlood2::Net
