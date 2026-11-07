#include "engine/gi_lightmap.h"
#include "engine/collision.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <mutex>
#include <tuple>

namespace OpenBlood2
{
    namespace L = gi_lightmap_limits;
    namespace
    {
        double elapsed_ms(std::chrono::steady_clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
        std::array<std::uint8_t, 3> expand_565(std::uint16_t texel)
        {
            const unsigned r = texel >> 11 & 31, g = texel >> 5 & 63, b = texel & 31;
            return {std::uint8_t(r << 3 | r >> 2), std::uint8_t(g << 2 | g >> 4), std::uint8_t(b << 3 | b >> 2)};
        }
        void put16(std::vector<std::uint8_t>& out, std::uint16_t value)
        {
            out.push_back(static_cast<std::uint8_t>(value));
            out.push_back(static_cast<std::uint8_t>(value >> 8));
        }
        void put32(std::vector<std::uint8_t>& out, std::uint32_t value)
        {
            for (int i = 0; i < 4; ++i)
                out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
        }
        struct Reader
        {
            std::span<const std::uint8_t> bytes;
            std::size_t at = 0;
            bool good = true;
            std::uint32_t get(int size)
            {
                if (bytes.size() - at < std::size_t(size) || at > bytes.size())
                {
                    good = false;
                    return 0;
                }
                std::uint32_t value = 0;
                for (int i = 0; i < size; ++i)
                    value |= std::uint32_t(bytes[at + i]) << (8 * i);
                at += size;
                return value;
            }
        };
        // Validity bits, LSB first, zero padding.
        void put_bits(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> valid)
        {
            const auto start = out.size();
            out.resize(start + (valid.size() + 7) / 8, 0);
            for (std::size_t i = 0; i < valid.size(); ++i)
                if (valid[i])
                    out[start + i / 8] |= std::uint8_t(1u << (i % 8));
        }
        bool get_bits(Reader& reader, std::size_t count, std::vector<std::uint8_t>& valid)
        {
            const auto size = (count + 7) / 8;
            if (reader.bytes.size() - reader.at < size)
                return false;
            valid.assign(count, 0);
            for (std::size_t i = 0; i < count; ++i)
                valid[i] = reader.bytes[reader.at + i / 8] >> (i % 8) & 1;
            if (count % 8 && reader.bytes[reader.at + size - 1] >> (count % 8))
                return false; // Nonzero padding.
            reader.at += size;
            return true;
        }
        DVector unit(DVector v)
        {
            const double length = std::sqrt(double(v.x) * v.x + double(v.y) * v.y + double(v.z) * v.z);
            return length > 0 ? DVector{float(v.x / length), float(v.y / length), float(v.z / length)} : DVector{};
        }
        std::size_t ring_size(const Polygon& polygon)
        {
            return polygon.base_points == UINT16_MAX
                       ? polygon.points.size()
                       : std::min<std::size_t>(polygon.base_points, polygon.points.size());
        }
        // Double-precision geometry for sample placement.
        struct D3
        {
            double x = 0, y = 0, z = 0;
        };
        D3 d3(DVector v)
        {
            return {v.x, v.y, v.z};
        }
        D3 sub(D3 a, D3 b)
        {
            return {a.x - b.x, a.y - b.y, a.z - b.z};
        }
        D3 add(D3 a, D3 b)
        {
            return {a.x + b.x, a.y + b.y, a.z + b.z};
        }
        D3 mul(D3 a, double s)
        {
            return {a.x * s, a.y * s, a.z * s};
        }
        double dot(D3 a, D3 b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }
        D3 cross(D3 a, D3 b)
        {
            return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        }
        // Moves p toward the centroid by min(edge_inset, half the distance).
        D3 inset(D3 p, D3 centroid)
        {
            const auto to = sub(centroid, p);
            const double distance = std::sqrt(dot(to, to));
            if (!(distance > 0))
                return p;
            return add(p, mul(to, std::min(L::edge_inset, distance / 2) / distance));
        }
        // The texel centre clamped into the convex base ring: unchanged when inside and at least
        // edge_inset from every edge, else the closest boundary point moved inward (a centre on an
        // edge would sit on the adjoining wall's plane).
        D3 clamp_into(D3 x, std::span<const D3> ring, D3 normal, D3 centroid)
        {
            D3 area{};
            for (std::size_t k = 1; k + 1 < ring.size(); ++k)
                area = add(area, cross(sub(ring[k], ring[0]), sub(ring[k + 1], ring[0])));
            const double sign = dot(area, normal) >= 0 ? 1 : -1;
            bool inside = true;
            for (std::size_t k = 0; k < ring.size() && inside; ++k)
            {
                const auto& a = ring[k];
                const auto& b = ring[(k + 1) % ring.size()];
                inside = dot(cross(sub(b, a), sub(x, a)), normal) * sign >= 0;
            }
            D3 best{};
            double best_distance = DBL_MAX;
            for (std::size_t k = 0; k < ring.size(); ++k)
            {
                const auto& a = ring[k];
                const auto& b = ring[(k + 1) % ring.size()];
                const auto edge = sub(b, a);
                const double length = dot(edge, edge);
                const double t = length > 0 ? std::clamp(dot(sub(x, a), edge) / length, 0.0, 1.0) : 0.0;
                const auto q = add(a, mul(edge, t));
                const auto d = sub(x, q);
                if (const double distance = dot(d, d); distance < best_distance)
                    best_distance = distance, best = q;
            }
            if (inside && best_distance >= L::edge_inset * L::edge_inset)
                return x;
            return inset(best, centroid);
        }
        // Bilinear over a block (clamped, texel centres at i + 0.5) or interpolation over a set.
        void sample_link(const GiReceivers& r, const float* values, const GiReceivers::Link& link, float u, float v,
                         double out[3])
        {
            const double w = 1.0 - u - v;
            if (link.kind == GiReceivers::Link::Lightmap)
            {
                const auto& block = r.blocks[link.receiver];
                const int width = block.id.width, height = block.id.height;
                const double s = w * link.st[0][0] + u * link.st[1][0] + v * link.st[2][0] - .5;
                const double t = w * link.st[0][1] + u * link.st[1][1] + v * link.st[2][1] - .5;
                const double x = std::clamp(s, 0.0, double(width - 1)), y = std::clamp(t, 0.0, double(height - 1));
                const int x0 = int(x), y0 = int(y);
                const int x1 = std::min(x0 + 1, width - 1), y1 = std::min(y0 + 1, height - 1);
                const double fx = x - x0, fy = y - y0;
                const float* row0 = values + (std::size_t(block.first) + std::size_t(y0) * width) * 3;
                const float* row1 = values + (std::size_t(block.first) + std::size_t(y1) * width) * 3;
                for (int c = 0; c < 3; ++c)
                    out[c] = (1 - fy) * ((1 - fx) * row0[x0 * 3 + c] + fx * row0[x1 * 3 + c]) +
                             fy * ((1 - fx) * row1[x0 * 3 + c] + fx * row1[x1 * 3 + c]);
                return;
            }
            const auto& set = r.sets[link.receiver];
            const float* a = values + (std::size_t(set.first) + link.corners[0]) * 3;
            const float* b = values + (std::size_t(set.first) + link.corners[1]) * 3;
            const float* c = values + (std::size_t(set.first) + link.corners[2]) * 3;
            for (int k = 0; k < 3; ++k)
                out[k] = w * a[k] + u * b[k] + v * c[k];
        }
        // Receiver kinds for neighbour fill: blocks, vertex sets, DETAIL LOW sets.
        int receiver_kind(const GiReceivers& r, std::size_t receiver)
        {
            return receiver < r.blocks.size() ? 0 : r.sets[receiver - r.blocks.size()].low ? 2 : 1;
        }
    } // namespace

    std::uint32_t gi_rgb9e5(DVector value)
    {
        constexpr int mantissa = 9, bias = 15;
        constexpr double largest = 511.0 / 512.0 * 65536.0;
        auto clamp = [&](float c) { return c > 0 ? std::min(double(c), largest) : 0.0; }; // NaN: 0.
        const double r = clamp(value.x), g = clamp(value.y), b = clamp(value.z);
        const double m = std::max({r, g, b});
        int exponent = -bias - 1;
        if (m > 0)
        {
            int e = 0;
            std::frexp(m, &e);
            exponent = std::max(exponent, e - 1);
        }
        int shared = exponent + 1 + bias;
        double scale = std::ldexp(1.0, shared - bias - mantissa);
        auto quantize = [&](double c) { return std::floor(c / scale + .5); };
        if (quantize(m) == double(1 << mantissa))
            scale = std::ldexp(1.0, ++shared - bias - mantissa);
        return std::uint32_t(quantize(r)) | std::uint32_t(quantize(g)) << 9 | std::uint32_t(quantize(b)) << 18 |
               std::uint32_t(shared) << 27;
    }
    DVector gi_rgb9e5_decode(std::uint32_t packed)
    {
        const int shift = int(packed >> 27) - 15 - 9;
        return {float(std::ldexp(double(packed & 511), shift)), float(std::ldexp(double(packed >> 9 & 511), shift)),
                float(std::ldexp(double(packed >> 18 & 511), shift))};
    }
    std::uint8_t gi_compose(std::uint8_t original, float ambient, float energy, float k, float f, bool texel)
    {
        const double o = original;
        const double a = ambient > 0 ? std::min(o, double(ambient)) : 0.0; // NaN and negative: no fill.
        const double d = o - a;
        double indirect = double(k) * (energy > 0 ? double(energy) : 0.0) * (texel ? 255.0 / 2 : 255.0);
        if (!(indirect >= 0))
            indirect = 0;
        const double fill = double(f) * a;
        const double out = std::floor(d + std::max(indirect, fill >= 0 ? fill : 0.0) + .5);
        return static_cast<std::uint8_t>(out >= 255 ? 255 : out > 0 ? out : 0);
    }
    std::uint8_t gi_compose_direct(float direct, std::uint8_t original, float ambient, float energy, float k, float f)
    {
        const double o = original;
        const double a = ambient > 0 ? std::min(o, double(ambient)) : 0.0;
        const double d = direct > 0 ? double(direct) * 255 : 0.0;
        double indirect = double(k) * (energy > 0 ? double(energy) : 0.0) * (255.0 / 2);
        if (!(indirect >= 0))
            indirect = 0;
        const double fill = double(f) * a;
        const double out = std::floor(d + std::max(indirect, fill >= 0 ? fill : 0.0) + .5);
        return static_cast<std::uint8_t>(out >= 255 ? 255 : out > 0 ? out : 0);
    }

    bool write_gi_lightmaps(const GiLightmaps& maps, std::vector<std::uint8_t>& out, std::string& error)
    {
        out.clear();
        std::size_t texels = 0;
        for (std::size_t i = 0; i < maps.blocks.size(); ++i)
        {
            const auto& b = maps.blocks[i];
            if (!b.width || !b.height ||
                (i && std::tie(maps.blocks[i - 1].model, maps.blocks[i - 1].polygon) >= std::tie(b.model, b.polygon)))
            {
                error = "LMAP blocks are empty or out of order";
                return false;
            }
            texels += std::size_t(b.width) * b.height;
        }
        if (texels != maps.texels.size() || texels != maps.valid.size() || texels > L::max_texels)
        {
            error = "LMAP texel count mismatch";
            return false;
        }
        put32(out, L::format);
        put32(out, static_cast<std::uint32_t>(maps.blocks.size()));
        put32(out, static_cast<std::uint32_t>(texels));
        for (const auto& b : maps.blocks)
        {
            put16(out, b.model);
            put16(out, b.polygon);
            out.push_back(b.width);
            out.push_back(b.height);
            put16(out, 0);
        }
        for (const auto value : maps.texels)
            put32(out, value);
        put_bits(out, maps.valid);
        return true;
    }
    bool read_gi_lightmaps(std::span<const std::uint8_t> bytes, GiLightmaps& maps, std::string& error)
    {
        maps = {};
        Reader reader{bytes};
        const auto format = reader.get(4), blocks = reader.get(4), texels = reader.get(4);
        if (!reader.good || format != L::format || texels > L::max_texels || blocks > texels ||
            bytes.size() - reader.at < std::size_t(blocks) * 8)
        {
            error = "Invalid LMAP header";
            return false;
        }
        GiLightmaps out;
        out.blocks.resize(blocks);
        std::size_t total = 0;
        for (std::uint32_t i = 0; i < blocks; ++i)
        {
            auto& b = out.blocks[i];
            b.model = static_cast<std::uint16_t>(reader.get(2));
            b.polygon = static_cast<std::uint16_t>(reader.get(2));
            b.width = static_cast<std::uint8_t>(reader.get(1));
            b.height = static_cast<std::uint8_t>(reader.get(1));
            const auto reserved = reader.get(2);
            if (reserved || !b.width || !b.height ||
                (i && std::tie(out.blocks[i - 1].model, out.blocks[i - 1].polygon) >= std::tie(b.model, b.polygon)))
            {
                error = "Invalid LMAP block";
                return false;
            }
            total += std::size_t(b.width) * b.height;
        }
        if (total != texels || bytes.size() - reader.at != std::size_t(texels) * 4 + (texels + 7) / 8)
        {
            error = "LMAP size mismatch";
            return false;
        }
        out.texels.resize(texels);
        for (auto& value : out.texels)
            value = reader.get(4);
        if (!get_bits(reader, texels, out.valid) || reader.at != bytes.size())
        {
            error = "Invalid LMAP validity bits";
            return false;
        }
        maps = std::move(out);
        return true;
    }
    bool write_gi_vertex_light(const GiVertexLight& light, std::vector<std::uint8_t>& out, std::string& error)
    {
        out.clear();
        std::size_t corners = 0;
        for (std::size_t i = 0; i < light.sets.size(); ++i)
        {
            const auto& s = light.sets[i];
            if (!s.corners ||
                (i && std::tie(light.sets[i - 1].model, light.sets[i - 1].polygon) >= std::tie(s.model, s.polygon)))
            {
                error = "VTX sets are empty or out of order";
                return false;
            }
            corners += s.corners;
        }
        if (corners != light.corners.size() || corners != light.valid.size() || corners > L::max_corners)
        {
            error = "VTX corner count mismatch";
            return false;
        }
        put32(out, L::format);
        put32(out, static_cast<std::uint32_t>(light.sets.size()));
        put32(out, static_cast<std::uint32_t>(corners));
        for (const auto& s : light.sets)
        {
            put16(out, s.model);
            put16(out, s.polygon);
            put16(out, s.corners);
            put16(out, 0);
        }
        for (const auto value : light.corners)
            put32(out, value);
        put_bits(out, light.valid);
        return true;
    }
    bool read_gi_vertex_light(std::span<const std::uint8_t> bytes, GiVertexLight& light, std::string& error)
    {
        light = {};
        Reader reader{bytes};
        const auto format = reader.get(4), sets = reader.get(4), corners = reader.get(4);
        if (!reader.good || format != L::format || corners > L::max_corners || sets > corners ||
            bytes.size() - reader.at < std::size_t(sets) * 8)
        {
            error = "Invalid VTX header";
            return false;
        }
        GiVertexLight out;
        out.sets.resize(sets);
        std::size_t total = 0;
        for (std::uint32_t i = 0; i < sets; ++i)
        {
            auto& s = out.sets[i];
            s.model = static_cast<std::uint16_t>(reader.get(2));
            s.polygon = static_cast<std::uint16_t>(reader.get(2));
            s.corners = static_cast<std::uint16_t>(reader.get(2));
            const auto reserved = reader.get(2);
            if (reserved || !s.corners ||
                (i && std::tie(out.sets[i - 1].model, out.sets[i - 1].polygon) >= std::tie(s.model, s.polygon)))
            {
                error = "Invalid VTX set";
                return false;
            }
            total += s.corners;
        }
        if (total != corners || bytes.size() - reader.at != std::size_t(corners) * 4 + (corners + 7) / 8)
        {
            error = "VTX size mismatch";
            return false;
        }
        out.corners.resize(corners);
        for (auto& value : out.corners)
            value = reader.get(4);
        if (!get_bits(reader, corners, out.valid) || reader.at != bytes.size())
        {
            error = "Invalid VTX validity bits";
            return false;
        }
        light = std::move(out);
        return true;
    }
    bool check_gi_lightmaps(const World& world, const GiLightmaps& maps, std::string& error)
    {
        for (const auto& b : maps.blocks)
        {
            const auto* model = b.model < world.models.size() ? &world.models[b.model] : nullptr;
            const auto* polygon = model && b.polygon < model->polygons.size() ? &model->polygons[b.polygon] : nullptr;
            if (!polygon || !model->lightmaps || polygon->lightmap_width != b.width ||
                polygon->lightmap_height != b.height)
            {
                error = "LMAP block " + std::to_string(b.model) + "/" + std::to_string(b.polygon) +
                        " does not match the world";
                return false;
            }
        }
        return true;
    }
    bool check_gi_vertex_light(const World& world, const GiVertexLight& light, std::string& error)
    {
        for (const auto& s : light.sets)
        {
            const auto* model = s.model < world.models.size() ? &world.models[s.model] : nullptr;
            const auto* polygon = model && s.polygon < model->polygons.size() ? &model->polygons[s.polygon] : nullptr;
            if (!polygon || polygon->colors.size() != polygon->points.size() || ring_size(*polygon) != s.corners)
            {
                error = "VTX set " + std::to_string(s.model) + "/" + std::to_string(s.polygon) +
                        " does not match the world";
                return false;
            }
        }
        return true;
    }

    bool gi_receivers(const World& world, const GiBakeScene& scene, std::span<const WorldTriangleSource> sources,
                      std::span<const GiRangeUse> use, unsigned jobs, GiReceivers& out, std::string& error)
    {
        out = {};
        const auto faces = scene.faces();
        if (sources.size() != scene.mesh().vertices.size() / 3 || use.size() != world.models.size() ||
            scene.mesh().ranges.size() != world.models.size())
        {
            error = "GI receivers: triangle sources do not match the world mesh";
            return false;
        }
        using Key = std::pair<std::uint32_t, std::uint32_t>;
        std::vector<Key> lightmapped, vertex;
        for (const auto& source : sources)
        {
            if (source.model >= use.size() || use[source.model] != GiRangeUse::Main)
                continue;
            if (source.light == WorldFaceLight::Lightmap)
                lightmapped.push_back({source.model, source.polygon});
            else if (source.light == WorldFaceLight::Vertex)
                vertex.push_back({source.model, source.polygon});
        }
        for (auto* list : {&lightmapped, &vertex})
        {
            std::sort(list->begin(), list->end());
            list->erase(std::unique(list->begin(), list->end()), list->end());
        }
        auto polygon_of = [&](Key key) -> const Polygon& { return world.models[key.first].polygons[key.second]; };
        auto plane_normal = [&](Key key)
        {
            const auto& model = world.models[key.first];
            const auto& surface = model.surfaces[polygon_of(key).surface];
            return unit(model.planes[surface.plane].normal);
        };
        for (const auto key : lightmapped)
        {
            const auto& model = world.models[key.first];
            const auto& polygon = polygon_of(key);
            GiReceivers::Block block;
            if (key.first > UINT16_MAX || key.second > UINT16_MAX ||
                !lightmap_frame(model, polygon, ring_size(polygon), block.frame) ||
                block.frame.width != polygon.lightmap_width || block.frame.height != polygon.lightmap_height ||
                std::size_t(polygon.lightmap_offset) + std::size_t(polygon.lightmap_width) * polygon.lightmap_height >
                    model.lightmap_texels.size())
            {
                error = "GI receivers: invalid lightmap on " + std::to_string(key.first) + "/" +
                        std::to_string(key.second);
                return false;
            }
            block.id = {std::uint16_t(key.first), std::uint16_t(key.second), polygon.lightmap_width,
                        polygon.lightmap_height};
            block.first = static_cast<std::uint32_t>(out.texels);
            block.normal = plane_normal(key);
            out.texels += std::size_t(block.id.width) * block.id.height;
            if (out.texels > L::max_texels)
            {
                error = "GI receivers exceed " + std::to_string(L::max_texels) + " texels";
                return false;
            }
            out.blocks.push_back(block);
        }
        // Sets: vertex-lit polygons and lightmapped polygons with colours, in (model, polygon) order.
        std::vector<std::pair<Key, bool>> sets;
        for (const auto key : vertex)
            sets.push_back({key, false});
        for (const auto key : lightmapped)
            if (polygon_of(key).colors.size() == polygon_of(key).points.size())
                sets.push_back({key, true});
        std::sort(sets.begin(), sets.end());
        for (const auto& [key, low] : sets)
        {
            const auto& polygon = polygon_of(key);
            const auto count = ring_size(polygon);
            if (key.first > UINT16_MAX || key.second > UINT16_MAX || count < 3 || count > UINT16_MAX ||
                polygon.colors.size() != polygon.points.size())
            {
                error = "GI receivers: invalid vertex light on " + std::to_string(key.first) + "/" +
                        std::to_string(key.second);
                return false;
            }
            GiReceivers::Set set;
            set.id = {std::uint16_t(key.first), std::uint16_t(key.second), std::uint16_t(count)};
            set.first = static_cast<std::uint32_t>(out.texels + out.corners);
            set.low = low;
            set.normal = plane_normal(key);
            out.corners += count;
            if (low)
                ++out.low_sets, out.low_corners += count;
            if (out.corners > L::max_corners)
            {
                error = "GI receivers exceed " + std::to_string(L::max_corners) + " corners";
                return false;
            }
            out.sets.push_back(set);
        }
        // Face links.
        auto find_block = [&](std::uint32_t model, std::uint32_t polygon)
        {
            const auto found = std::lower_bound(out.blocks.begin(), out.blocks.end(), Key{model, polygon},
                                                [](const GiReceivers::Block& b, Key k)
                                                { return Key{b.id.model, b.id.polygon} < k; });
            return found != out.blocks.end() && found->id.model == model && found->id.polygon == polygon
                       ? std::int64_t(found - out.blocks.begin())
                       : std::int64_t(-1);
        };
        auto find_set = [&](std::uint32_t model, std::uint32_t polygon, bool low)
        {
            for (auto found = std::lower_bound(out.sets.begin(), out.sets.end(), Key{model, polygon},
                                               [](const GiReceivers::Set& s, Key k)
                                               { return Key{s.id.model, s.id.polygon} < k; });
                 found != out.sets.end() && found->id.model == model && found->id.polygon == polygon; ++found)
                if (found->low == low)
                    return std::int64_t(found - out.sets.begin());
            return std::int64_t(-1);
        };
        out.links.resize(faces.size());
        for (std::size_t f = 0; f < faces.size(); ++f)
        {
            const auto& source = sources[faces[f].triangle];
            auto& link = out.links[f];
            const bool main = use[source.model] == GiRangeUse::Main;
            switch (source.light)
            {
            case WorldFaceLight::Lightmap:
                if (const auto b = main ? find_block(source.model, source.polygon) : -1; b >= 0)
                {
                    link.kind = GiReceivers::Link::Lightmap;
                    link.receiver = static_cast<std::uint32_t>(b);
                    const auto& model = world.models[source.model];
                    const auto& polygon = model.polygons[source.polygon];
                    for (int k = 0; k < 3; ++k)
                    {
                        const auto st = out.blocks[b].frame.coordinate(model.points[polygon.points[source.corners[k]]]);
                        link.st[k] = {float(st[0]), float(st[1])};
                    }
                }
                break;
            case WorldFaceLight::Vertex:
                if (const auto s = main ? find_set(source.model, source.polygon, false) : -1; s >= 0)
                {
                    link.kind = GiReceivers::Link::Vertex;
                    link.receiver = static_cast<std::uint32_t>(s);
                    link.corners = source.corners;
                }
                break;
            case WorldFaceLight::Bright:
                link.kind = GiReceivers::Link::Bright;
                break;
            case WorldFaceLight::Sky:
                link.kind = GiReceivers::Link::Sky;
                break;
            default:
                break;
            }
        }
        // Elements: owner, original light, sample position and root-BSP contents.
        const auto elements = out.elements();
        out.owner.resize(elements);
        out.samples.resize(elements);
        out.solid.assign(elements, 0);
        out.original.resize(elements);
        if (!world_info_ambient_any_case(world.info, out.ambient))
            out.ambient = {};
        const auto receivers = out.blocks.size() + out.sets.size();
        std::atomic<bool> failed{false};
        run_gi_jobs(receivers, jobs ? jobs : gi_default_jobs(), nullptr,
                    [&](std::uint64_t index, unsigned)
                    {
                        const bool is_block = index < out.blocks.size();
                        const Key key = is_block ? Key{out.blocks[index].id.model, out.blocks[index].id.polygon}
                                                 : Key{out.sets[index - out.blocks.size()].id.model,
                                                       out.sets[index - out.blocks.size()].id.polygon};
                        const auto& model = world.models[key.first];
                        const auto& polygon = model.polygons[key.second];
                        const auto count = ring_size(polygon);
                        std::vector<D3> ring(count);
                        D3 centroid{};
                        for (std::size_t k = 0; k < count; ++k)
                        {
                            ring[k] = d3(model.points[polygon.points[k]]);
                            centroid = add(centroid, ring[k]);
                        }
                        centroid = mul(centroid, 1.0 / double(count));
                        const auto normal =
                            d3(is_block ? out.blocks[index].normal : out.sets[index - out.blocks.size()].normal);
                        auto place = [&](std::size_t element, D3 p)
                        {
                            const auto sample = add(p, mul(normal, L::sample_offset));
                            out.samples[element] = {float(sample.x), float(sample.y), float(sample.z)};
                            out.solid[element] =
                                point_contents(world.models[0], out.samples[element]) == PointContents::Solid;
                            out.owner[element] = static_cast<std::uint32_t>(index);
                        };
                        if (is_block)
                        {
                            const auto& block = out.blocks[index];
                            for (unsigned j = 0; j < block.id.height; ++j)
                                for (unsigned i = 0; i < block.id.width; ++i)
                                {
                                    const auto element = block.first + std::size_t(j) * block.id.width + i;
                                    DVector centre;
                                    if (!block.frame.position(i, j, centre))
                                    {
                                        failed = true;
                                        return;
                                    }
                                    place(element, clamp_into(d3(centre), ring, normal, centroid));
                                    out.original[element] = expand_565(
                                        model.lightmap_texels[polygon.lightmap_offset + j * block.id.width + i]);
                                }
                        }
                        else
                        {
                            const auto& set = out.sets[index - out.blocks.size()];
                            for (std::size_t k = 0; k < count; ++k)
                            {
                                place(set.first + k, inset(ring[k], centroid));
                                out.original[set.first + k] = polygon.colors[k];
                            }
                        }
                    });
        if (failed)
        {
            out = {};
            error = "GI receivers: degenerate lightmap axes";
            return false;
        }
        // Neighbours across shared base-ring edges.
        struct Edge
        {
            std::uint32_t model, a, b, receiver;
        };
        std::vector<Edge> edges;
        for (std::size_t r = 0; r < receivers; ++r)
        {
            const bool is_block = r < out.blocks.size();
            const auto model_index = is_block ? out.blocks[r].id.model : out.sets[r - out.blocks.size()].id.model;
            const auto polygon_index =
                is_block ? out.blocks[r].id.polygon : out.sets[r - out.blocks.size()].id.polygon;
            const auto& polygon = world.models[model_index].polygons[polygon_index];
            const auto count = ring_size(polygon);
            for (std::size_t k = 0; k < count; ++k)
            {
                const std::uint32_t a = polygon.points[k], b = polygon.points[(k + 1) % count];
                edges.push_back({model_index, std::min(a, b), std::max(a, b), std::uint32_t(r)});
            }
        }
        std::sort(edges.begin(), edges.end(), [](const Edge& x, const Edge& y)
                  { return std::tie(x.model, x.a, x.b, x.receiver) < std::tie(y.model, y.a, y.b, y.receiver); });
        std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
        for (std::size_t i = 0; i < edges.size();)
        {
            std::size_t j = i;
            while (j < edges.size() && edges[j].model == edges[i].model && edges[j].a == edges[i].a &&
                   edges[j].b == edges[i].b)
                ++j;
            for (std::size_t x = i; x < j; ++x)
                for (std::size_t y = i; y < j; ++y)
                    if (edges[x].receiver != edges[y].receiver)
                        pairs.push_back({edges[x].receiver, edges[y].receiver});
            i = j;
        }
        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
        auto normal_of = [&](std::size_t r)
        { return d3(r < out.blocks.size() ? out.blocks[r].normal : out.sets[r - out.blocks.size()].normal); };
        out.neighbour_first.assign(receivers + 1, 0);
        for (const auto& [a, b] : pairs)
            if (receiver_kind(out, a) == receiver_kind(out, b) && dot(normal_of(a), normal_of(b)) >= L::fill_normal)
            {
                out.neighbours.push_back(b);
                ++out.neighbour_first[a + 1];
            }
        for (std::size_t r = 0; r < receivers; ++r)
            out.neighbour_first[r + 1] += out.neighbour_first[r];
        return true;
    }

    bool gi_receiver_sample(const GiReceivers& r, std::span<const float> values, std::uint32_t face, float u, float v,
                            DVector& out)
    {
        if (face >= r.links.size() || values.size() != r.elements() * 3)
            return false;
        const auto& link = r.links[face];
        if (link.kind != GiReceivers::Link::Lightmap && link.kind != GiReceivers::Link::Vertex)
            return false;
        double value[3];
        sample_link(r, values.data(), link, u, v, value);
        out = {float(value[0]), float(value[1]), float(value[2])};
        return true;
    }

    void gi_quarter_sin_cos(double x, double& sine, double& cosine)
    {
        const double x2 = x * x;
        sine = x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110 * (1 - x2 / 156))))));
        const double tail = 1 - x2 / 132 * (1 - x2 / 182);
        cosine = 1 - x2 / 2 * (1 - x2 / 12 * (1 - x2 / 30 * (1 - x2 / 56 * (1 - x2 / 90 * tail))));
    }
    DVector gi_cosine_direction(DVector normal, std::uint32_t i, std::uint32_t n, double s1, double s2)
    {
        double u1 = (i + .5) / n + s1, u2 = i * 0.6180339887498949 + s2;
        u1 -= std::floor(u1);
        u2 -= std::floor(u2);
        const double a = 2 * u1 - 1, b = 2 * u2 - 1;
        double x = 0, y = 0;
        if (a != 0 || b != 0)
        {
            constexpr double quarter_pi = 0.78539816339744830962;
            double sine, cosine;
            if (std::fabs(a) > std::fabs(b))
            {
                gi_quarter_sin_cos(quarter_pi * (b / a), sine, cosine);
                x = a * cosine, y = a * sine;
            }
            else
            {
                gi_quarter_sin_cos(quarter_pi * (a / b), sine, cosine);
                x = b * sine, y = b * cosine;
            }
        }
        const double z = std::sqrt(std::max(0.0, 1 - x * x - y * y));
        const D3 nn = d3(normal);
        const double sign = std::copysign(1.0, nn.z);
        const double c = -1 / (sign + nn.z), d = nn.x * nn.y * c;
        const D3 t{1 + sign * nn.x * nn.x * c, sign * d, -sign * nn.x}, b2{d, sign + nn.y * nn.y * c, -nn.y};
        const D3 out = add(add(mul(t, x), mul(b2, y)), mul(nn, z));
        return {float(out.x), float(out.y), float(out.z)};
    }

    namespace
    {
        // Radiance leaving a face point toward the ray, from the light state.
        void surface_radiance(const GiBakeScene& scene, const GiSky& sky, const GiReceivers& r, const float* light,
                              std::uint32_t face, float u, float v, DVector direction, GiRayCounters& count,
                              double out[3])
        {
            const auto& link = r.links[face];
            switch (link.kind)
            {
            case GiReceivers::Link::Lightmap:
            case GiReceivers::Link::Vertex:
            {
                const auto albedo = scene.albedo(face, u, v);
                double value[3];
                sample_link(r, light, link, u, v, value);
                const double scale = link.kind == GiReceivers::Link::Lightmap ? world_saturate_scale : 1;
                for (int c = 0; c < 3; ++c)
                    out[c] = albedo[c] * scale * value[c];
                return;
            }
            case GiReceivers::Link::Bright:
            {
                const auto albedo = scene.albedo(face, u, v);
                out[0] = albedo[0], out[1] = albedo[1], out[2] = albedo[2];
                return;
            }
            case GiReceivers::Link::Sky:
            {
                const auto s = sky.radiance(direction);
                out[0] = s.x, out[1] = s.y, out[2] = s.z;
                ++count.sky;
                return;
            }
            default:
                out[0] = out[1] = out[2] = 0;
                ++count.untextured;
            }
        }
    } // namespace
    void gi_hit_radiance(const GiBakeScene& scene, const GiSky& sky, const GiReceivers& r, const float* light,
                         const GiHit& hit, DVector direction, GiRayCounters& count, double out[3], bool& back)
    {
        out[0] = out[1] = out[2] = 0;
        back = false;
        double transmission = 1, value[3];
        for (std::uint32_t i = 0; i < hit.layer_count; ++i)
        {
            const auto& layer = hit.layers[i];
            surface_radiance(scene, sky, r, light, layer.face, layer.u, layer.v, direction, count, value);
            for (int c = 0; c < 3; ++c)
                out[c] += layer.alpha * transmission * value[c];
            transmission *= 1 - layer.alpha;
        }
        switch (hit.kind)
        {
        case GiHit::Kind::Miss:
        {
            const auto s = sky.radiance(direction);
            value[0] = s.x, value[1] = s.y, value[2] = s.z;
            ++count.sky;
            break;
        }
        case GiHit::Kind::LayerLimit:
        {
            // The ninth layer: its own share only.
            ++count.layer_limit;
            surface_radiance(scene, sky, r, light, hit.face, hit.u, hit.v, direction, count, value);
            const double alpha = scene.albedo(hit.face, hit.u, hit.v)[3];
            for (int c = 0; c < 3; ++c)
                value[c] *= alpha;
            break;
        }
        default:
            if (hit.back)
            {
                back = true;
                ++count.back;
                return;
            }
            if (hit.kind == GiHit::Kind::Sky)
            {
                const auto s = sky.radiance(direction);
                value[0] = s.x, value[1] = s.y, value[2] = s.z;
                ++count.sky;
            }
            else
                surface_radiance(scene, sky, r, light, hit.face, hit.u, hit.v, direction, count, value);
        }
        for (int c = 0; c < 3; ++c)
            out[c] += transmission * value[c];
    }
    void gi_light_state(const GiReceivers& r, const GiIndirect& indirect, std::vector<float>& light,
                        std::span<const float> rebaked)
    {
        const auto n = r.elements();
        light.assign(n * 3, 0.f);
        if (indirect.energy.size() != n * 3 || r.original.size() != n || r.owner.size() != n ||
            (!rebaked.empty() && rebaked.size() != r.texels * 3))
            return;
        const float ambient[3]{r.ambient.x, r.ambient.y, r.ambient.z};
        for (std::size_t e = 0; e < n; ++e)
        {
            const bool low = e >= r.texels && r.sets[r.owner[e] - r.blocks.size()].low;
            const double scale = e < r.texels ? .5 : 1;
            for (int c = 0; c < 3; ++c)
            {
                // The passes' arithmetic: D as a float, then min(1, D + scale E) as a float.
                const double o = r.original[e][c];
                const double a = ambient[c] > 0 ? std::min(o, double(ambient[c])) : 0.0;
                const float direct = e < r.texels && !rebaked.empty() ? rebaked[e * 3 + c] : float((o - a) / 255);
                light[e * 3 + c] =
                    low ? direct : float(std::min(1.0, direct + scale * indirect.energy[e * 3 + c]));
            }
        }
    }

    bool bake_gi_lightmaps(const GiBakeScene& scene, const GiSky& sky, const GiReceivers& r, std::uint32_t bounces,
                           std::uint32_t rays, std::uint64_t seed, unsigned jobs, const std::atomic<bool>* cancel,
                           const GiProgress& progress, GiIndirect& result, GiLightmapReport& report,
                           std::string& error, std::span<const float> rebaked)
    {
        const auto start = std::chrono::steady_clock::now();
        result = {};
        report = {};
        report.blocks = r.blocks.size();
        report.texels = r.texels;
        report.sets = r.sets.size();
        report.corners = r.corners;
        report.low_sets = r.low_sets;
        report.low_corners = r.low_corners;
        const auto n = r.elements();
        const auto receivers = r.blocks.size() + r.sets.size();
        if (!bounces || !rays || r.links.size() != scene.faces().size() || r.owner.size() != n ||
            r.samples.size() != n || r.solid.size() != n || r.original.size() != n ||
            r.neighbour_first.size() != receivers + 1 || (!rebaked.empty() && rebaked.size() != r.texels * 3))
        {
            error = "GI lightmaps: invalid receivers or settings";
            return false;
        }
        const unsigned threads = std::max(1u, jobs ? jobs : gi_default_jobs());
        auto is_low = [&](std::size_t e) { return e >= r.texels && r.sets[r.owner[e] - r.blocks.size()].low; };
        auto normal_of = [&](std::size_t e)
        { return e < r.texels ? r.blocks[r.owner[e]].normal : r.sets[r.owner[e] - r.blocks.size()].normal; };
        // D: the original light without the ambient fill (0-1), or the re-baked D' on texels (lot
        // GD2), and B: what the next pass sees.
        const float ambient[3]{r.ambient.x, r.ambient.y, r.ambient.z};
        std::vector<float> direct(n * 3), light(n * 3), energy(n * 3, 0.f), next(n * 3);
        for (std::size_t e = 0; e < n; ++e)
            for (int c = 0; c < 3; ++c)
            {
                const double o = r.original[e][c];
                const double a = ambient[c] > 0 ? std::min(o, double(ambient[c])) : 0.0;
                direct[e * 3 + c] = light[e * 3 + c] =
                    e < r.texels && !rebaked.empty() ? rebaked[e * 3 + c] : float((o - a) / 255);
            }
        std::vector<std::uint8_t> valid(n, 0);
        struct Counters
        {
            GiRayCounters rays;
            std::array<std::uint64_t, 10> fraction{};
        };
        std::vector<Counters> counters(threads);
        std::vector<std::uint32_t> traced;
        std::vector<std::uint8_t> seeds(n, 0);
        std::vector<std::uint8_t> state(receivers, 0); // 0: no value, 1: own samples, 2: neighbour fill.
        std::vector<double> means(receivers * 3, 0);
        std::mutex report_lock;
        for (std::uint32_t pass = 1; pass <= bounces; ++pass)
        {
            const auto pass_start = std::chrono::steady_clock::now();
            const bool last = pass == bounces;
            // Pass 1 traces every sample outside solid space; later passes trace the valid ones. DETAIL
            // LOW sets feed no lookup: they are traced once, in the last pass.
            traced.clear();
            for (std::size_t e = 0; e < n; ++e)
            {
                if (r.solid[e])
                    continue;
                const bool low = is_low(e);
                if ((low && last) || (!low && (pass == 1 || valid[e])))
                    traced.push_back(static_cast<std::uint32_t>(e));
            }
            std::fill(next.begin(), next.end(), 0.f);
            std::atomic<std::uint64_t> done{0};
            std::uint64_t reported = 0;
            const std::string stage = "bounce " + std::to_string(pass) + "/" + std::to_string(bounces);
            const std::uint64_t total = traced.size();
            const auto chunks = (traced.size() + L::chunk - 1) / L::chunk;
            const auto run = run_gi_jobs(
                chunks, threads, cancel,
                [&](std::uint64_t chunk, unsigned thread)
                {
                    auto& count = counters[thread];
                    GiHit hit;
                    const auto first = chunk * L::chunk, end = std::min<std::uint64_t>(first + L::chunk, total);
                    for (auto t = first; t < end; ++t)
                    {
                        const auto e = traced[t];
                        const auto normal = normal_of(e);
                        const auto origin = r.samples[e];
                        GiRandom random(seed, e, pass);
                        const double s1 = random.uniform(), s2 = random.uniform();
                        double sum[3]{};
                        std::uint32_t back_rays = 0;
                        for (std::uint32_t i = 0; i < rays; ++i)
                        {
                            const auto direction = gi_cosine_direction(normal, i, rays, s1, s2);
                            scene.trace(origin, direction, FLT_MAX, hit, count.rays.trace);
                            double value[3];
                            bool back = false;
                            gi_hit_radiance(scene, sky, r, light.data(), hit, direction, count.rays, value, back);
                            back_rays += back;
                            for (int c = 0; c < 3; ++c)
                                sum[c] += value[c];
                        }
                        for (int c = 0; c < 3; ++c)
                            next[std::size_t(e) * 3 + c] = float(sum[c] / rays);
                        // A sample decides its validity on its first trace.
                        if (pass == 1 || is_low(e))
                        {
                            valid[e] = back_rays <= L::back_limit * rays;
                            ++count.fraction[std::min<std::uint32_t>(9, back_rays * 10 / rays)];
                        }
                    }
                    const auto finished = done.fetch_add(end - first) + (end - first);
                    if (progress && total && (finished * 20 / total != (finished - (end - first)) * 20 / total))
                    {
                        std::lock_guard lock(report_lock);
                        if (finished > reported)
                        {
                            reported = finished;
                            progress(stage, finished, total);
                        }
                    }
                });
            report.threads = std::max(report.threads, run.threads);
            if (run.cancelled)
            {
                report.cancelled = true;
                report.ms = elapsed_ms(start);
                error = "cancelled";
                return false;
            }
            // Dilation: inside each receiver first, then from receivers to neighbours that have none.
            for (std::size_t e = 0; e < n; ++e)
                seeds[e] = !r.solid[e] && valid[e] && (!is_low(e) || last);
            std::fill(state.begin(), state.end(), 0);
            std::size_t filled = 0;
            std::vector<std::uint8_t> known;
            std::vector<float> scratch;
            for (std::size_t b = 0; b < r.blocks.size(); ++b)
            {
                const auto& block = r.blocks[b];
                const int width = block.id.width, height = block.id.height;
                const std::size_t first = block.first, size = std::size_t(width) * height;
                known.assign(seeds.begin() + first, seeds.begin() + first + size);
                std::size_t missing = 0;
                for (const auto k : known)
                    missing += !k;
                if (missing == size)
                    continue;
                state[b] = 1;
                filled += missing;
                while (missing)
                {
                    // One round over a snapshot: each missing texel takes the mean of known 8-neighbours.
                    scratch.assign(next.begin() + first * 3, next.begin() + (first + size) * 3);
                    std::vector<std::uint8_t> now = known;
                    for (int y = 0; y < height; ++y)
                        for (int x = 0; x < width; ++x)
                        {
                            const auto at = std::size_t(y) * width + x;
                            if (known[at])
                                continue;
                            double sum[3]{};
                            int count = 0;
                            for (int dy = -1; dy <= 1; ++dy)
                                for (int dx = -1; dx <= 1; ++dx)
                                {
                                    const int nx = x + dx, ny = y + dy;
                                    if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= width || ny >= height ||
                                        !known[std::size_t(ny) * width + nx])
                                        continue;
                                    const auto other = std::size_t(ny) * width + nx;
                                    for (int c = 0; c < 3; ++c)
                                        sum[c] += scratch[other * 3 + c];
                                    ++count;
                                }
                            if (!count)
                                continue;
                            for (int c = 0; c < 3; ++c)
                                next[(first + at) * 3 + c] = float(sum[c] / count);
                            now[at] = 1;
                            --missing;
                        }
                    known = std::move(now);
                }
            }
            for (std::size_t s = 0; s < r.sets.size(); ++s)
            {
                const auto& set = r.sets[s];
                if (set.low && !last)
                    continue;
                double sum[3]{};
                std::size_t count = 0;
                for (std::size_t k = 0; k < set.id.corners; ++k)
                    if (seeds[set.first + k])
                    {
                        for (int c = 0; c < 3; ++c)
                            sum[c] += next[(set.first + k) * 3 + c];
                        ++count;
                    }
                if (!count)
                    continue;
                state[r.blocks.size() + s] = 1;
                for (std::size_t k = 0; k < set.id.corners; ++k)
                    if (!seeds[set.first + k])
                    {
                        for (int c = 0; c < 3; ++c)
                            next[(set.first + k) * 3 + c] = float(sum[c] / count);
                        ++filled;
                    }
            }
            auto elements_of = [&](std::size_t receiver)
            {
                if (receiver < r.blocks.size())
                    return std::pair<std::size_t, std::size_t>{
                        r.blocks[receiver].first,
                        std::size_t(r.blocks[receiver].id.width) * r.blocks[receiver].id.height};
                const auto& set = r.sets[receiver - r.blocks.size()];
                return std::pair<std::size_t, std::size_t>{set.first, set.id.corners};
            };
            for (std::size_t q = 0; q < receivers; ++q)
            {
                if (!state[q])
                    continue;
                const auto [first, size] = elements_of(q);
                double sum[3]{};
                for (std::size_t e = first; e < first + size; ++e)
                    for (int c = 0; c < 3; ++c)
                        sum[c] += next[e * 3 + c];
                for (int c = 0; c < 3; ++c)
                    means[q * 3 + c] = sum[c] / size;
            }
            auto active = [&](std::size_t q)
            { return q < r.blocks.size() || !r.sets[q - r.blocks.size()].low || last; };
            std::size_t neighbour_filled = 0, unfilled = 0;
            for (std::uint32_t round = 0; round < L::fill_rounds; ++round)
            {
                std::vector<std::pair<std::size_t, std::array<double, 3>>> fills;
                for (std::size_t q = 0; q < receivers; ++q)
                {
                    if (state[q] || !active(q))
                        continue;
                    double sum[3]{};
                    std::size_t count = 0;
                    for (auto at = r.neighbour_first[q]; at < r.neighbour_first[q + 1]; ++at)
                        if (const auto other = r.neighbours[at]; state[other])
                        {
                            for (int c = 0; c < 3; ++c)
                                sum[c] += means[other * 3 + c];
                            ++count;
                        }
                    if (count)
                        fills.push_back({q, {sum[0] / count, sum[1] / count, sum[2] / count}});
                }
                if (fills.empty())
                    break;
                for (const auto& [q, mean] : fills)
                {
                    const auto [first, size] = elements_of(q);
                    for (std::size_t e = first; e < first + size; ++e)
                        for (int c = 0; c < 3; ++c)
                            next[e * 3 + c] = float(mean[c]);
                    for (int c = 0; c < 3; ++c)
                        means[q * 3 + c] = mean[c];
                    state[q] = 2;
                    ++neighbour_filled;
                }
            }
            for (std::size_t q = 0; q < receivers; ++q)
                if (!state[q] && active(q))
                {
                    const auto [first, size] = elements_of(q);
                    std::fill(next.begin() + first * 3, next.begin() + (first + size) * 3, 0.f);
                    ++unfilled;
                }
            energy.swap(next);
            for (std::size_t e = 0; e < n; ++e)
            {
                if (is_low(e))
                    continue;
                const double scale = e < r.texels ? .5 : 1;
                for (int c = 0; c < 3; ++c)
                    light[e * 3 + c] = float(std::min(1.0, direct[e * 3 + c] + scale * energy[e * 3 + c]));
            }
            if (last)
            {
                report.filled = filled;
                report.neighbour_filled = neighbour_filled;
                report.unfilled = unfilled;
            }
            report.pass_ms.push_back(elapsed_ms(pass_start));
        }
        for (std::size_t e = 0; e < n; ++e)
        {
            report.solid += r.solid[e] != 0;
            report.back_invalid += !r.solid[e] && !valid[e];
        }
        for (const auto& count : counters)
        {
            report.trace.rays += count.rays.trace.rays;
            report.trace.nodes += count.rays.trace.nodes;
            report.trace.triangles += count.rays.trace.triangles;
            report.trace.texels += count.rays.trace.texels;
            report.back += count.rays.back;
            report.sky += count.rays.sky;
            report.untextured += count.rays.untextured;
            report.layer_limit += count.rays.layer_limit;
            for (std::size_t i = 0; i < count.fraction.size(); ++i)
                report.back_fraction[i] += count.fraction[i];
        }
        report.rays = report.trace.rays;
        result.energy = std::move(energy);
        result.valid = std::move(valid);
        report.ms = elapsed_ms(start);
        return true;
    }

    void gi_lightmap_sections(const GiReceivers& r, const GiIndirect& indirect, GiLightmaps& maps,
                              GiVertexLight& vertex)
    {
        maps = {};
        vertex = {};
        auto value = [&](std::size_t e)
        { return gi_rgb9e5({indirect.energy[e * 3], indirect.energy[e * 3 + 1], indirect.energy[e * 3 + 2]}); };
        for (const auto& block : r.blocks)
            maps.blocks.push_back(block.id);
        for (std::size_t e = 0; e < r.texels; ++e)
        {
            maps.texels.push_back(value(e));
            maps.valid.push_back(indirect.valid[e]);
        }
        for (const auto& set : r.sets)
            vertex.sets.push_back(set.id);
        for (std::size_t e = r.texels; e < r.elements(); ++e)
        {
            vertex.corners.push_back(value(e));
            vertex.valid.push_back(indirect.valid[e]);
        }
    }
} // namespace OpenBlood2
