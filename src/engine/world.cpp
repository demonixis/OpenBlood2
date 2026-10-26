#include "engine/world.h"
#include "engine/formats/reader.h"
#include <cctype>
#include <limits>
#include <set>
#include <utility>

namespace OpenBlood2
{
    const PropertyValue* WorldObject::find(std::string_view name) const
    {
        if (invalid_editor_position && !CaseLess{}(name, "Pos") && !CaseLess{}("Pos", name))
            return nullptr;
        for (const auto& p : properties)
            if (!CaseLess{}(p.name, name) && !CaseLess{}(name, p.name))
                return &p.value;
        return nullptr;
    }
    namespace
    {
        bool property(Reader& r, WorldProperty& out, WorldObject& object)
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
                    if ((object.class_name == "Light" || object.class_name == "DirLight") && out.name == "Pos")
                    {
                        // 16_bridge contains NaN positions on two preprocessor lights. These are
                        // already baked into the BSP; never turn their invalid metadata into a light/pose.
                        const DVector v{std::bit_cast<float>(p.u32()), std::bit_cast<float>(p.u32()),
                                        std::bit_cast<float>(p.u32())};
                        out.value = v;
                        object.invalid_editor_position =
                            !std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z);
                        break;
                    }
                    [[fallthrough]];
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
        // The model tail ends with u32 byte count, then per SURF_LIGHTMAP polygon in order:
        // u8 width, u8 height, width * height RGB565 texels. Entries are 0x0 or match the
        // grid-rounded extents (sky portals store unrelated sizes). The preceding bytes are
        // not decoded, so the block is located by its exact length and accepted only when a
        // single offset validates; otherwise the model keeps baked vertex light only.
        void read_lightmaps(std::span<const std::uint8_t> tail, WorldModel& out)
        {
            constexpr std::size_t texel_limit = 16 * 1024 * 1024;
            std::vector<std::uint32_t> lightmapped;
            for (std::uint32_t i = 0; i < out.polygons.size(); ++i)
                if (out.surfaces[out.polygons[i].surface].flags & (1u << 7))
                    lightmapped.push_back(i);
            auto u32 = [&](std::size_t at)
            {
                return std::uint32_t(tail[at]) | std::uint32_t(tail[at + 1]) << 8 | std::uint32_t(tail[at + 2]) << 16 |
                       std::uint32_t(tail[at + 3]) << 24;
            };
            auto validate = [&](std::size_t at)
            {
                std::size_t q = at + 4, texels = 0;
                for (auto index : lightmapped)
                {
                    const auto& polygon = out.polygons[index];
                    if (q + 2 > tail.size())
                        return false;
                    const unsigned w = tail[q], h = tail[q + 1];
                    const bool sky = out.surfaces[polygon.surface].flags & (1u << 4);
                    if (!sky && (w || h) &&
                        (polygon.lightmap_extent[0] % 20 || polygon.lightmap_extent[1] % 20 ||
                         w != polygon.lightmap_extent[0] / 20u + 1 || h != polygon.lightmap_extent[1] / 20u + 1))
                        return false;
                    texels += std::size_t(w) * h;
                    if (texels > texel_limit || 2 * std::size_t(w) * h > tail.size() - q - 2)
                        return false;
                    q += 2 + 2 * std::size_t(w) * h;
                }
                return q == tail.size();
            };
            std::size_t found = SIZE_MAX;
            for (std::size_t at = 0; at + 4 <= tail.size(); ++at)
                if (u32(at) == tail.size() - at - 4 && validate(at))
                {
                    if (found != SIZE_MAX)
                        return; // Ambiguous: keep vertex light only.
                    found = at;
                }
            if (found == SIZE_MAX)
                return;
            std::size_t q = found + 4;
            for (auto index : lightmapped)
            {
                auto& polygon = out.polygons[index];
                const unsigned w = tail[q], h = tail[q + 1];
                q += 2;
                polygon.lightmap_width = static_cast<std::uint8_t>(w);
                polygon.lightmap_height = static_cast<std::uint8_t>(h);
                polygon.lightmap_offset = static_cast<std::uint32_t>(out.lightmap_texels.size());
                for (std::size_t i = 0; i < std::size_t(w) * h; ++i, q += 2)
                    out.lightmap_texels.push_back(static_cast<std::uint16_t>(tail[q] | tail[q + 1] << 8));
            }
            out.lightmaps = true;
            out.opaque_bytes = found;
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
                BspLeaf leaf;
                auto count = r.u16();
                if (count == 65535)
                {
                    leaf.shared = r.u16();
                    r.require(leaf.shared < leaves);
                    // Retail copies the referenced leaf's lists as loaded so far (zeroed when later).
                    if (leaf.shared < i)
                    {
                        leaf.first_list = out.bsp_leaves[leaf.shared].first_list;
                        leaf.lists = out.bsp_leaves[leaf.shared].lists;
                    }
                }
                else
                {
                    leaf.first_list = static_cast<std::uint32_t>(out.visibility_lists.size());
                    leaf.lists = count;
                    for (unsigned j = 0; j < count && r.good(); ++j)
                    {
                        LeafVisibilityList list;
                        list.portal = r.u16();
                        list.size = r.u16();
                        list.offset = static_cast<std::uint32_t>(out.visibility.size());
                        const auto bytes = r.take(list.size);
                        out.visibility.insert(out.visibility.end(), bytes.begin(), bytes.end());
                        out.visibility_lists.push_back(list);
                    }
                }
                out.bsp_leaves.push_back(leaf);
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
                s.lightmap_p = r.vec();
                s.lightmap_q = r.vec();
                r.skip(12); // Unspecified packed surface payload, not a float color.
                s.texture = r.u16();
                s.plane = r.u32();
                s.flags = r.u32();
                s.visibility_skip = r.u8();
                r.skip(3);
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
                Polygon p;
                p.lightmap_extent = {r.u16(), r.u16()};
                r.skip(8); // Unspecified runtime values, not lightmap data.
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
            // Nodes (retail Server.dll order): plane, polygon, leaf, two sides (-1 inside, -2 outside),
            // then a bounding centre and radius that are not used here. Collision does not use them.
            out.nodes = nodes;
            out.leaves = leaves;
            out.portals = portals;
            if (nodes > r.remaining() / 34)
                return false;
            auto side = [&](std::uint32_t raw)
            {
                const auto value = static_cast<std::int32_t>(raw);
                r.require(value == BspNode::inside || value == BspNode::outside || (value >= 0 && raw < nodes));
                return value;
            };
            for (std::uint32_t i = 0; i < nodes && r.good(); ++i)
            {
                BspNode node;
                node.plane = r.u32();
                node.polygon = r.u32();
                node.leaf = r.u16();
                node.sides = {side(r.u32()), side(r.u32())};
                r.skip(16);
                if (!r.require(node.plane < planes && node.polygon < polygons &&
                               (node.leaf == UINT16_MAX || node.leaf < leaves)))
                    return false;
                out.bsp_nodes.push_back(node);
            }
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
            if (!r.good())
                return false;
            // Retail Server.dll then reads a cell grid (three u32 dimensions, two vectors, then per
            // cell u16 count, u16 and count * 6 bytes), not used here, and the root node index.
            // A BSP without nodes (synthetic geometry) has no tree to root; its tail stays opaque.
            if (nodes)
            {
                const std::uint64_t gx = r.u32(), gy = r.u32(), gz = r.u32();
                if (!r.good() || (gx && gy && gz && gx * gy * gz > r.remaining() / 4) || !r.skip(24))
                    return false;
                for (std::uint64_t i = 0, cells = gx * gy * gz; i < cells && r.good(); ++i)
                {
                    const auto count = r.u16();
                    r.u16();
                    r.skip(std::size_t(count) * 6);
                }
                out.root_node = side(r.u32());
                if (!r.good())
                    return false;
            }
            const auto tail = r.take(r.remaining());
            out.opaque_bytes = tail.size();
            read_lightmaps(tail, out);
            return r.good() && file.good();
        }
    } // namespace
    bool read_world_info(std::span<const std::uint8_t> prefix, std::string& info, std::string& error)
    {
        info.clear();
        Reader r(prefix);
        const auto version = r.u32();
        r.u32();
        r.u32();
        const auto length = r.u32();
        auto text = r.text(length, 65536);
        if (!r.good() || version != 56)
        {
            error = "DAT v56: invalid/unsupported world-info header";
            return false;
        }
        info = std::move(text);
        error.clear();
        return true;
    }
    std::vector<std::vector<std::string>> world_info_commands(std::string_view info)
    {
        std::vector<std::vector<std::string>> commands(1);
        std::string token;
        bool quoted = false, open = false;
        for (const char c : info)
        {
            if (quoted)
            {
                if (c == '"')
                    quoted = false;
                else
                    token += c;
                continue;
            }
            if (c == '"')
            {
                quoted = open = true;
                continue;
            }
            if (c == ';' || std::isspace(static_cast<unsigned char>(c)))
            {
                if (open)
                    commands.back().push_back(std::exchange(token, {}));
                open = false;
                if (c == ';')
                    commands.emplace_back();
                continue;
            }
            token += c;
            open = true;
        }
        if (open)
            commands.back().push_back(token);
        return commands;
    }
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
                const auto at = objects + list.position() - size + o.position();
                if (!property(o, p, object) || !names.insert(p.name).second)
                {
                    error = "DAT v56: invalid/unsupported typed property " + object.class_name + "." + p.name +
                            " at byte " + std::to_string(at);
                    return false;
                }
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
