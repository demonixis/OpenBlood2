#include "engine/world.h"
#include "engine/formats/reader.h"
#include <limits>
#include <set>

namespace OpenBlood2
{
    const PropertyValue* WorldObject::find(std::string_view name) const
    {
        for (const auto& p : properties)
            if (!CaseLess{}(p.name, name) && !CaseLess{}(name, p.name))
                return &p.value;
        return nullptr;
    }
    namespace
    {
        bool property(Reader& r, WorldProperty& out)
        {
            out.name = r.string(100);
            out.type = r.u8();
            out.flags = r.u32();
            const auto size = r.u16();
            Reader p(r.take(size));
            switch (out.type)
            {
                case 0:
                    out.value = p.string();
                    break;
                case 1:
                case 2:
                    out.value = p.vec();
                    break;
                case 3:
                    out.value = p.real();
                    break;
                case 4:
                case 6:
                { // Both editor integer types are serialized as a float.
                    const double f = p.real();
                    if (!p.require(f >= INT32_MIN && f <= INT32_MAX && std::trunc(f) == f))
                        return false;
                    out.value = static_cast<std::int32_t>(f);
                    break;
                }
                case 5:
                {
                    const auto b = p.u8();
                    p.require(b <= 1);
                    out.value = bool(b);
                    break;
                }
                case 7:
                {
                    out.value = EulerAngles{p.vec()};
                    p.real();
                    break;
                } // Fourth float is reserved, not quaternion W.
                default:
                    return false;
            }
            return r.good() && p.good() && !p.remaining() && !out.name.empty();
        }
        bool bsp(Reader& file, WorldModel& out, std::size_t& budget, std::size_t& effect_bytes)
        {
            const auto start = file.position();
            const auto next = file.u32();
            if (!file.require(next > start + 36 && next - start - 4 <= file.remaining()))
                return false;
            Reader r(file.take(next - start - 4));
            r.skip(32);
            out.flags = r.u32();
            out.name = r.string(100);
            r.u32();
            std::uint32_t counts[10];
            for (auto& count : counts)
            {
                count = r.u32();
                if (!r.require(count <= 2000000))
                    return false;
            }
            const auto [points, planes, surfaces, portals, polygons, leaves, vertices, vis, leaflists, nodes] = counts;
            (void)vertices;
            (void)vis;
            (void)leaflists;
            const std::size_t items = std::size_t(points) + planes + surfaces + polygons + leaves + nodes + portals;
            if (items > 2000000 - budget || points > 65536 || polygons > 65536 || leaves > 65536 || portals > 65536)
                return false;
            budget += items;
            r.u32();
            out.min = r.vec();
            out.max = r.vec();
            out.translation = r.vec();
            if (!r.require(out.min.x <= out.max.x && out.min.y <= out.max.y && out.min.z <= out.max.z))
                return false;
            const auto text_bytes = r.u32(), textures = r.u32();
            if (textures > 65536 || textures > text_bytes)
                return false;
            Reader names(r.take(text_bytes));
            for (std::uint32_t i = 0; i < textures && names.good(); ++i)
                out.textures.push_back(names.cstring(1024));
            if (!names.good() || names.remaining())
                return false;
            std::vector<std::pair<unsigned, unsigned>> sizes;
            if (polygons > r.remaining() / 2)
                return false;
            for (std::uint32_t i = 0; i < polygons; ++i)
            {
                const unsigned base = r.u8(), extra = r.u8(), count = base + extra;
                if (count < 3 || count > 510)
                    return false;
                sizes.emplace_back(base, count);
            }
            for (std::uint32_t i = 0; i < leaves && r.good(); ++i)
            {
                auto count = r.u16();
                if (count == 65535)
                {
                    const auto shared = r.u16();
                    r.require(shared < leaves);
                }
                else
                    for (unsigned j = 0; j < count && r.good(); ++j)
                    {
                        r.u16();
                        const auto bytes = r.u16();
                        r.skip(bytes);
                    }
                count = r.u16();
                r.skip(std::size_t(count) * 4 + 4);
            }
            if (!r.good() || planes > r.remaining() / 16)
                return false;
            for (std::uint32_t i = 0; i < planes; ++i)
            {
                const auto normal = r.vec();
                out.planes.push_back({normal, r.real()});
            }
            for (std::uint32_t i = 0; i < surfaces && r.good(); ++i)
            {
                Surface s;
                s.origin = r.vec();
                s.u = r.vec();
                s.v = r.vec();
                r.vec();
                r.vec();
                r.skip(12); // Unspecified packed surface payload, not a float color.
                s.texture = r.u16();
                s.plane = r.u32();
                s.flags = r.u32();
                r.u32();
                const auto effect = r.u8();
                if (effect == 1)
                {
                    s.has_effect = true;
                    s.effect_name = r.string();
                    s.effect_parameters = r.string();
                    const auto bytes = s.effect_name.size() + s.effect_parameters.size();
                    if (!r.require(bytes <= 8 * 1024 * 1024 - effect_bytes))
                        return false;
                    effect_bytes += bytes;
                }
                s.texture_flags = r.u16();
                if (!r.require(effect <= 1 && s.texture < textures && s.plane < planes))
                    return false;
                out.surfaces.push_back(std::move(s));
            }
            std::size_t total_vertices = 0;
            for (auto [base, count] : sizes)
            {
                if (!r.good())
                    return false;
                total_vertices += count;
                if (total_vertices > 2000000)
                    return false;
                r.skip(12);
                Polygon p;
                p.base_points = static_cast<std::uint16_t>(base);
                p.surface = r.u32();
                if (!r.require(p.surface < surfaces))
                    return false;
                for (unsigned i = 0; i < count; ++i)
                {
                    const auto index = r.u16();
                    if (!r.require(index < points))
                        return false;
                    p.points.push_back(index);
                    const auto red = r.u8(), green = r.u8(), blue = r.u8();
                    p.colors.push_back({red, green, blue});
                }
                out.polygons.push_back(std::move(p));
            }
            // Node/visibility payloads are bounded, never traversed. Their graph and
            // collision semantics are not claimed by this reader.
            out.nodes = nodes;
            out.leaves = leaves;
            out.portals = portals;
            r.skip(std::size_t(nodes) * 34);
            for (std::uint32_t i = 0; i < portals && r.good(); ++i)
            {
                r.string();
                r.u32();
                r.u16();
                r.vec();
                r.vec();
            }
            if (!r.good() || points > r.remaining() / 12)
                return false;
            for (std::uint32_t i = 0; i < points; ++i)
                out.points.push_back(r.vec());
            out.opaque_bytes = r.remaining();
            return r.good() && file.good();
        }
    } // namespace
    bool read_world(std::span<const std::uint8_t> bytes, World& world, std::string& error)
    {
        world = {};
        error.clear();
        World next;
        Reader r(bytes);
        auto fail = [&](const char* section)
        {
            error =
                "DAT v56: invalid/unsupported " + std::string(section) + " near byte " + std::to_string(r.position());
            return false;
        };
        if (bytes.size() > 128 * 1024 * 1024)
            return fail("file size");
        next.version = r.u32();
        if (next.version != 56)
            return fail("version (only 56)");
        const auto objects = r.u32(), geometry = r.u32(), length = r.u32();
        next.info = r.text(length, 65536);
        if (!r.good() || objects < r.position() || objects > bytes.size() || geometry < objects ||
            geometry > bytes.size())
            return fail("section offsets");
        // Header remainder is preserved as an explicitly uninterpreted section.
        next.opaque_bytes = objects - r.position();
        r.skip(next.opaque_bytes);
        Reader list(r.take(geometry - objects));
        const auto count = list.u32();
        if (count > 65536 || count > list.remaining() / 8)
            return fail("object count");
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto size = list.u16();
            Reader o(list.take(size));
            WorldObject object;
            object.class_name = o.string(100);
            const auto properties = o.u32();
            if (!o.good() || object.class_name.empty() || properties > 1024 || properties > o.remaining() / 10)
                return fail("object record");
            std::set<std::string, CaseLess> names;
            for (std::uint32_t j = 0; j < properties; ++j)
            {
                WorldProperty p;
                if (!property(o, p) || !names.insert(p.name).second)
                    return fail("typed property");
                object.properties.push_back(std::move(p));
            }
            if (!o.good() || o.remaining())
                return fail("object length");
            next.objects.push_back(std::move(object));
        }
        if (!list.good() || list.remaining())
            return fail("object section");
        std::size_t budget = 0, effect_bytes = 0;
        WorldModel root;
        if (!bsp(r, root, budget, effect_bytes))
            return fail("root BSP");
        if (root.name.empty())
            root.name = "Root";
        next.models.push_back(std::move(root));
        const auto models = r.u32();
        if (models > 4096 || models > r.remaining() / 140)
            return fail("model count");
        for (std::uint32_t i = 0; i < models; ++i)
        {
            WorldModel model;
            if (!bsp(r, model, budget, effect_bytes))
                return fail("world-model BSP");
            next.models.push_back(std::move(model));
        }
        if (r.remaining() != 24)
            return fail("world bounds/footer size");
        next.min = r.vec();
        next.max = r.vec();
        if (!r.good() || next.min.x > next.max.x || next.min.y > next.max.y || next.min.z > next.max.z)
            return fail("world bounds");
        world = std::move(next);
        return true;
    }
} // namespace OpenBlood2
