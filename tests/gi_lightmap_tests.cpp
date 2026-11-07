// GI lot G7: the lightmap core (gi_contracts.md). Analytic bakes of synthetic worlds (furnace, box,
// two planes, leak), the exact E = 0 identity, RGB9E5, the LMAP and VTX sections, sample placement,
// dilation, back-face validity, directions, thread determinism, the sky setup and cube, and the
// preview helpers. The private case bakes 01_SubwayA through the driver and checks its sections.
// Lot G8 (gi_runtime.*): the world override composed by WorldMesh::build, every view, and the
// runtime lookup of 01_SubwayA (missing, stale, ready; E = 0 leaves the mesh byte-identical).
// Lot G9 (gi_probes.*): binary16, the PROB section, the SH basis, probe capture (furnace, back faces,
// determinism), the trilinear field, the model split, the Light probes world source and its cost.
#include "engine/config.h"
#include "engine/gi_bake.h"
#include "engine/gi_lightmap.h"
#include "engine/gi_placement.h"
#include "engine/gi_probes.h"
#include "engine/gi_preview.h"
#include "engine/gi_runtime.h"
#include "engine/gi_sky.h"
#include "engine/resources.h"
#include "engine/sha256.h"
#include "resource_fixture.h"
#include "scratch_dir.h"
#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <set>

using namespace OpenBlood2;
namespace fs = std::filesystem;

#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace
{
    constexpr double pi = 3.14159265358979323846;
    namespace L = gi_lightmap_limits;

    bool near(double a, double b, double tolerance)
    {
        return std::abs(a - b) <= tolerance;
    }

    // Double-precision reference geometry.
    struct V3
    {
        double x = 0, y = 0, z = 0;
    };
    V3 v3(DVector v)
    {
        return {v.x, v.y, v.z};
    }
    V3 operator-(V3 a, V3 b)
    {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }
    V3 operator+(V3 a, V3 b)
    {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }
    V3 operator*(V3 a, double s)
    {
        return {a.x * s, a.y * s, a.z * s};
    }
    double dot(V3 a, V3 b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    V3 cross(V3 a, V3 b)
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    double norm(V3 a)
    {
        return std::sqrt(dot(a, a));
    }
    // Lambert's point-to-polygon configuration factor: the cosine-weighted fraction of the hemisphere
    // of `normal` at `at` that the convex polygon (wholly in front) covers.
    double form_factor(DVector at, DVector normal, const std::vector<DVector>& polygon)
    {
        double sum = 0;
        for (std::size_t i = 0; i < polygon.size(); ++i)
        {
            const V3 a = v3(polygon[i]) - v3(at), b = v3(polygon[(i + 1) % polygon.size()]) - v3(at);
            const V3 c = cross(a, b);
            if (const double length = norm(c); length > 0)
                sum += std::atan2(length, dot(a, b)) * dot(v3(normal), c) / length;
        }
        return std::abs(sum) / (2 * pi);
    }
    // The documented sample rule, restated: a point inside the convex ring and at least edge_inset
    // from its boundary stays; otherwise it moves to the closest boundary point, then toward the
    // centroid by min(edge_inset, half the distance).
    V3 inset(V3 p, V3 centroid)
    {
        const V3 to = centroid - p;
        const double distance = norm(to);
        return distance > 0 ? p + to * (std::min(L::edge_inset, distance / 2) / distance) : p;
    }
    V3 reference_sample(V3 x, const std::vector<V3>& ring, V3 normal)
    {
        V3 centroid, area;
        for (const auto& p : ring)
            centroid = centroid + p * (1.0 / double(ring.size()));
        for (std::size_t k = 1; k + 1 < ring.size(); ++k)
            area = area + cross(ring[k] - ring[0], ring[k + 1] - ring[0]);
        const double sign = dot(area, normal) >= 0 ? 1 : -1;
        bool inside = true;
        double best = DBL_MAX;
        V3 closest;
        for (std::size_t k = 0; k < ring.size(); ++k)
        {
            const V3 a = ring[k], b = ring[(k + 1) % ring.size()], edge = b - a;
            inside = inside && dot(cross(edge, x - a), normal) * sign >= 0;
            const double t = std::clamp(dot(x - a, edge) / dot(edge, edge), 0.0, 1.0);
            const V3 q = a + edge * t;
            if (norm(x - q) < best)
                best = norm(x - q), closest = q;
        }
        return inside && best >= L::edge_inset ? x : inset(closest, centroid);
    }
    std::array<std::uint8_t, 3> expand(std::uint16_t texel)
    {
        const unsigned r = texel >> 11 & 31, g = texel >> 5 & 63, b = texel & 31;
        return {std::uint8_t(r << 3 | r >> 2), std::uint8_t(g << 2 | g >> 4), std::uint8_t(b << 3 | b >> 2)};
    }

    // A version -2 DTX of one palette colour (2x2 texels, palette entry bytes a, r, g, b).
    std::vector<std::uint8_t> dtx(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
    {
        test_resource::Writer image;
        image.u32(0);
        image.u32(0xfffffffeu);
        image.u16(2);
        image.u16(2);
        image.u16(1);
        image.u16(0);
        image.u32(0);
        image.u32(0);
        image.b.resize(1068);
        image.b[44] = a, image.b[45] = r, image.b[46] = g, image.b[47] = b;
        image.b.insert(image.b.end(), {0, 0, 0, 0});
        return image.b;
    }
    // textures/<name>.dtx for every colour the cases use.
    bool write_rez(const fs::path& path)
    {
        test_resource::Directory dir;
        auto& files = dir.children["textures"].files;
        files["white.dtx"] = dtx(255, 255, 255);
        files["black.dtx"] = dtx(0, 0, 0);
        files["grey.dtx"] = dtx(153, 153, 153);
        files["red.dtx"] = dtx(255, 0, 0);
        files["green.dtx"] = dtx(0, 255, 0);
        files["blue.dtx"] = dtx(0, 0, 255);
        files["yellow.dtx"] = dtx(255, 255, 0);
        files["cyan.dtx"] = dtx(0, 255, 255);
        files["magenta.dtx"] = dtx(255, 0, 255);
        test_resource::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13; // REZ v1 header line breaks, EOF marker and version.
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        const auto [offset, size] = test_resource::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), static_cast<std::streamsize>(w.b.size()));
        return static_cast<bool>(out);
    }
    bool mount(const fs::path& root, Resources& resources)
    {
        std::string error;
        if (!Testing::reset_scratch_dir(root) || !write_rez(root / "textures.rez") ||
            !resources.mount(root / "textures.rez", error))
        {
            std::cerr << "mount " << root << ": " << error << '\n';
            return false;
        }
        return true;
    }

    // Synthetic worlds: convex polygons with one colour each, through the real WorldMesh path.
    enum class Kind
    {
        Lightmap,    // A stored lightmap of `texel`.
        LowLightmap, // The same with baked colours (DETAIL LOW).
        Vertex,      // Baked colours.
        Bright,      // SURF_BRIGHT.
        Untextured,  // A missing texture.
        Sky          // A sky portal.
    };
    struct Face
    {
        std::vector<DVector> ring;
        DVector normal{};
        Kind kind = Kind::Lightmap;
        std::string texture = "grey.dtx";
        std::uint16_t texel = 0;             // RGB565 of every stored texel.
        std::array<std::uint8_t, 3> color{}; // Every baked colour.
    };
    struct WorldBuilder
    {
        World world;
        bool extents = false;
        WorldBuilder()
        {
            world.version = 56;
            world.models.resize(1);
            world.models[0].lightmaps = true;
        }
        std::uint16_t point(WorldModel& model, DVector p)
        {
            for (std::size_t i = 0; i < model.points.size(); ++i)
                if (model.points[i].x == p.x && model.points[i].y == p.y && model.points[i].z == p.z)
                    return static_cast<std::uint16_t>(i);
            model.points.push_back(p);
            if (!extents)
                world.min = world.max = p, extents = true;
            world.min = {std::min(world.min.x, p.x), std::min(world.min.y, p.y), std::min(world.min.z, p.z)};
            world.max = {std::max(world.max.x, p.x), std::max(world.max.y, p.y), std::max(world.max.z, p.z)};
            return static_cast<std::uint16_t>(model.points.size() - 1);
        }
        std::uint32_t add(const Face& face, std::size_t m = 0)
        {
            auto& model = world.models[m];
            Polygon polygon;
            for (const auto& p : face.ring)
                polygon.points.push_back(point(model, p));
            Surface surface;
            surface.plane = static_cast<std::uint32_t>(model.planes.size());
            model.planes.push_back({face.normal, vector_dot(face.normal, face.ring[0])});
            const auto path = "textures/" + (face.kind == Kind::Untextured ? std::string("missing.dtx") : face.texture);
            const auto found = std::find(model.textures.begin(), model.textures.end(), path);
            surface.texture = static_cast<std::uint16_t>(found - model.textures.begin());
            if (found == model.textures.end())
                model.textures.push_back(path);
            surface.u = {1, 0, 0};
            surface.v = {0, 1, 0};
            const float ax = std::abs(face.normal.x), ay = std::abs(face.normal.y), az = std::abs(face.normal.z);
            if (ax >= ay && ax >= az)
                surface.lightmap_p = {0, 1, 0}, surface.lightmap_q = {0, 0, 1};
            else if (ay >= az)
                surface.lightmap_p = {1, 0, 0}, surface.lightmap_q = {0, 0, 1};
            else
                surface.lightmap_p = {1, 0, 0}, surface.lightmap_q = {0, 1, 0};
            const bool lightmapped = face.kind == Kind::Lightmap || face.kind == Kind::LowLightmap;
            surface.flags = lightmapped                 ? 1u << 7
                            : face.kind == Kind::Bright ? 1u << 5
                            : face.kind == Kind::Sky    ? 1u << 4
                                                        : 0u;
            polygon.surface = static_cast<std::uint32_t>(model.surfaces.size());
            model.surfaces.push_back(surface);
            if (face.kind == Kind::Vertex || face.kind == Kind::LowLightmap)
                polygon.colors.assign(polygon.points.size(), face.color);
            if (lightmapped)
            {
                // The smallest stored size whose origin the mesh rule fits.
                const DVector axes[2]{surface.lightmap_p, surface.lightmap_q};
                unsigned size[2]{};
                for (int k = 0; k < 2; ++k)
                {
                    double lo = DBL_MAX, hi = -DBL_MAX, origin = 0;
                    for (const auto& p : face.ring)
                    {
                        lo = std::min(lo, double(vector_dot(axes[k], p)));
                        hi = std::max(hi, double(vector_dot(axes[k], p)));
                    }
                    for (size[k] = 1; size[k] < 255 && !lightmap_origin(lo, hi, size[k], origin); ++size[k])
                    {
                    }
                }
                polygon.lightmap_width = static_cast<std::uint8_t>(size[0]);
                polygon.lightmap_height = static_cast<std::uint8_t>(size[1]);
                polygon.lightmap_offset = static_cast<std::uint32_t>(model.lightmap_texels.size());
                model.lightmap_texels.insert(model.lightmap_texels.end(), std::size_t(size[0]) * size[1], face.texel);
            }
            model.polygons.push_back(std::move(polygon));
            return static_cast<std::uint32_t>(model.polygons.size() - 1);
        }
        // The six inward faces of [lo, hi]: 0 floor (z = lo), 1 ceiling, 2 y = lo, 3 y = hi, 4 x = lo,
        // 5 x = hi; `edit` sets up each one.
        void box(DVector lo, DVector hi, const std::function<void(int, Face&)>& edit, std::size_t m = 0)
        {
            const DVector a{lo.x, lo.y, lo.z}, b{hi.x, lo.y, lo.z}, c{hi.x, hi.y, lo.z}, d{lo.x, hi.y, lo.z};
            const DVector e{lo.x, lo.y, hi.z}, f{hi.x, lo.y, hi.z}, g{hi.x, hi.y, hi.z}, h{lo.x, hi.y, hi.z};
            const Face sides[6]{{{a, b, c, d}, {0, 0, 1}},  {{e, h, g, f}, {0, 0, -1}}, {{a, e, f, b}, {0, 1, 0}},
                                {{d, c, g, h}, {0, -1, 0}}, {{a, d, h, e}, {1, 0, 0}},  {{b, f, g, c}, {-1, 0, 0}}};
            for (int s = 0; s < 6; ++s)
            {
                Face face = sides[s];
                edit(s, face);
                add(face, m);
            }
        }
    };
    std::vector<DVector> quad_z(float x0, float y0, float x1, float y1, float z)
    {
        return {{x0, y0, z}, {x1, y0, z}, {x1, y1, z}, {x0, y1, z}};
    }
    WorldObject object(std::string class_name, std::vector<WorldProperty> properties)
    {
        WorldObject o;
        o.class_name = std::move(class_name);
        o.properties = std::move(properties);
        return o;
    }
    WorldProperty property(std::string name, PropertyValue value)
    {
        WorldProperty p;
        p.name = std::move(name);
        p.value = std::move(value);
        return p;
    }

    // The bake pipeline up to the receivers, as bake_gi_world runs it.
    struct Prepared
    {
        GiBakeScene scene;
        GiReceivers receivers;
        std::vector<WorldTriangleSource> sources;
        std::vector<GiRangeUse> use;
    };
    bool prepare(const World& world, const Resources& resources, Prepared& out, unsigned jobs = 2)
    {
        std::string error;
        WorldMesh mesh;
        GiSceneReport report;
        if (!mesh.build(world, error, &resources, true, &out.sources) ||
            !gi_range_uses(world, {}, out.use, report, error) ||
            !out.scene.build(std::move(mesh), out.use, report, error) ||
            !gi_receivers(world, out.scene, out.sources, out.use, jobs, out.receivers, error))
        {
            std::cerr << "prepare: " << error << '\n';
            return false;
        }
        return true;
    }
    bool bake(const Prepared& p, const GiSky& sky, std::uint32_t bounces, std::uint32_t rays, GiIndirect& out,
              GiLightmapReport& report, unsigned jobs = 2, std::uint64_t seed = 0x5eed)
    {
        std::string error;
        if (!bake_gi_lightmaps(p.scene, sky, p.receivers, bounces, rays, seed, jobs, nullptr, {}, out, report, error))
        {
            std::cerr << "bake: " << error << '\n';
            return false;
        }
        return out.energy.size() == p.receivers.elements() * 3 && out.valid.size() == p.receivers.elements();
    }

    // D1: E = 0 with f = 1 gives back every stored value exactly, whatever the ambient, k or texel kind.
    bool identity()
    {
        const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
        const float ambients[]{0, .5f, 1, 7.5f, 20, 63.25f, 254.9f, 255, 300, -3, nan, inf, -inf};
        const float ks[]{0, .5f, 1, 2.5f, 4, inf, nan};
        const float zeros[]{0.f, -0.f, -1.f, -inf, nan};
        std::size_t checked = 0;
        for (unsigned o = 0; o < 256; ++o)
            for (const float a : ambients)
                for (const float k : ks)
                    for (const float e : zeros)
                        for (const bool texel : {true, false})
                        {
                            CHECK(gi_compose(std::uint8_t(o), a, e, k, 1, texel) == o);
                            ++checked;
                        }
        // The formula: out = round(D + max(k E s, f A_eff)), s = 255/2 for texels, 255 for corners.
        CHECK(gi_compose(200, 20, .1f, 1, 1, true) == 200);  // 180 + max(12.75, 20).
        CHECK(gi_compose(200, 20, .5f, 1, 1, true) == 244);  // 180 + 63.75.
        CHECK(gi_compose(200, 20, .1f, 1, 1, false) == 206); // 180 + 25.5.
        CHECK(gi_compose(200, 20, .1f, 1, 0, true) == 193);  // 180 + 12.75: no ambient fill.
        CHECK(gi_compose(200, 20, .1f, 2, 0, true) == 206);  // k doubles the indirect term.
        CHECK(gi_compose(10, 20, 0, 1, 1, true) == 10);      // A_eff = min(O, A).
        CHECK(gi_compose(10, 20, 0, 1, 0, true) == 0);
        CHECK(gi_compose(200, 20, 10, 1, 1, true) == 255); // Clamped.
        CHECK(gi_compose(200, 20, 0, 1, 2, true) == 220);  // f = 2 doubles the fill.
        std::cout << "IDENTITY checked=" << checked << '\n';
        return true;
    }

    bool rgb9e5()
    {
        auto round_trip = [](DVector v) { return gi_rgb9e5_decode(gi_rgb9e5(v)); };
        const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
        CHECK(gi_rgb9e5({0, 0, 0}) == 0);
        const auto one = round_trip({1, .5f, .25f});
        CHECK(one.x == 1 && one.y == .5f && one.z == .25f);
        CHECK(gi_rgb9e5({1, 0, 0}) == (256u | 16u << 27));
        const auto big = round_trip({1e9f, inf, 65408});
        CHECK(big.x == 65408 && big.y == 65408 && big.z == 65408); // Saturation.
        CHECK(gi_rgb9e5({1e9f, 1e9f, 1e9f}) == 0xffffffffu);
        const auto bad = round_trip({nan, -1, -inf});
        CHECK(bad.x == 0 && bad.y == 0 && bad.z == 0);
        const auto up = round_trip({511.75f, 0, 0}); // Rounds into the next exponent.
        CHECK(up.x == 512);
        const auto tiny = round_trip({std::ldexp(1.f, -24), 0, 0});
        CHECK(tiny.x == std::ldexp(1.f, -24));
        CHECK(round_trip({std::ldexp(1.f, -26), 0, 0}).x == 0);
        // Every channel within half a step of the shared exponent; nine significant bits round-trip.
        std::mt19937 rng(9);
        std::uniform_real_distribution<float> exponent(-20, 16), unit(0, 1);
        for (int i = 0; i < 100000; ++i)
        {
            const DVector v{std::exp2(exponent(rng)), unit(rng) * std::exp2(exponent(rng)), unit(rng)};
            const auto d = round_trip(v);
            const double m = std::max({v.x, v.y, v.z});
            int e = 0;
            std::frexp(m, &e);
            const double step = std::ldexp(1.0, std::max(e - 1, -16) - 8);
            CHECK(near(d.x, v.x, step) && near(d.y, v.y, step) && near(d.z, v.z, step));
            const DVector exact{float(std::floor(v.x / step) * step), 0, 0};
            if (exact.x >= std::ldexp(1.0, e - 1))
                CHECK(round_trip(exact).x == exact.x);
        }
        return true;
    }

    bool sections()
    {
        std::string error;
        GiLightmaps maps;
        maps.blocks = {{0, 1, 2, 1}, {0, 3, 1, 3}, {2, 0, 1, 1}};
        maps.texels = {1, 2, 3, 4, 5, 0x80000000u};
        maps.valid = {1, 0, 1, 1, 0, 1};
        std::vector<std::uint8_t> bytes;
        CHECK(write_gi_lightmaps(maps, bytes, error));
        CHECK(bytes.size() == 12 + 3 * 8 + 6 * 4 + 1);
        GiLightmaps read;
        CHECK(read_gi_lightmaps(bytes, read, error));
        CHECK(read.texels == maps.texels && read.valid == maps.valid && read.blocks.size() == 3);
        for (std::size_t i = 0; i < 3; ++i)
            CHECK(read.blocks[i].model == maps.blocks[i].model && read.blocks[i].polygon == maps.blocks[i].polygon &&
                  read.blocks[i].width == maps.blocks[i].width && read.blocks[i].height == maps.blocks[i].height);
        CHECK(bytes.back() == 0x2d); // 101101: LSB first.
        auto refused = [&](std::size_t at, std::uint8_t value)
        {
            auto bad = bytes;
            bad[at] = value;
            GiLightmaps out;
            return !read_gi_lightmaps(bad, out, error) && out.blocks.empty();
        };
        CHECK(refused(0, 2));                      // Format.
        CHECK(refused(8, 7));                      // Texel count.
        CHECK(refused(4, 7));                      // Record count.
        CHECK(refused(12 + 4, 0));                 // Empty block.
        CHECK(refused(12 + 4, 3));                 // Size mismatch.
        CHECK(refused(12 + 8 + 2, 1));             // Order: (0, 3) after (0, 1) becomes (0, 1) again.
        CHECK(refused(12 + 6, 1));                 // Reserved.
        CHECK(refused(bytes.size() - 1, 0x6d));    // Nonzero padding.
        auto trailing = bytes;
        trailing.push_back(0);
        GiLightmaps out;
        CHECK(!read_gi_lightmaps(trailing, out, error));
        for (std::size_t size = 0; size < bytes.size(); ++size)
            CHECK(!read_gi_lightmaps(std::span(bytes).first(size), out, error));
        auto unordered = maps;
        std::swap(unordered.blocks[0], unordered.blocks[1]);
        CHECK(!write_gi_lightmaps(unordered, bytes, error) && bytes.empty());
        auto short_valid = maps;
        short_valid.valid.pop_back();
        CHECK(!write_gi_lightmaps(short_valid, bytes, error));
        auto empty = maps;
        empty.blocks[1].height = 0;
        CHECK(!write_gi_lightmaps(empty, bytes, error));
        GiLightmaps none;
        CHECK(write_gi_lightmaps(none, bytes, error) && bytes.size() == 12 && read_gi_lightmaps(bytes, out, error));

        GiVertexLight light;
        light.sets = {{0, 0, 3}, {0, 5, 4}, {1, 2, 3}};
        light.corners.resize(10);
        for (std::size_t i = 0; i < 10; ++i)
            light.corners[i] = std::uint32_t(i * 77);
        light.valid = {1, 1, 1, 0, 0, 0, 0, 1, 1, 0};
        CHECK(write_gi_vertex_light(light, bytes, error) && bytes.size() == 12 + 3 * 8 + 10 * 4 + 2);
        GiVertexLight back;
        CHECK(read_gi_vertex_light(bytes, back, error) && back.corners == light.corners && back.valid == light.valid);
        CHECK(back.sets.size() == 3 && back.sets[1].polygon == 5 && back.sets[1].corners == 4);
        auto refused_vtx = [&](std::size_t at, std::uint8_t value)
        {
            auto bad = bytes;
            bad[at] = value;
            GiVertexLight o;
            return !read_gi_vertex_light(bad, o, error) && o.sets.empty();
        };
        CHECK(refused_vtx(0, 0));
        CHECK(refused_vtx(12 + 4, 0));       // Empty set.
        CHECK(refused_vtx(12 + 8 + 4, 5));   // Corner count.
        CHECK(refused_vtx(12 + 6, 1));       // Reserved.
        CHECK(refused_vtx(12 + 8 + 2, 0));   // Order: (0, 0) twice.
        CHECK(refused_vtx(bytes.size() - 1, 0x07)); // Padding past ten bits.
        auto bad_count = light;
        bad_count.corners.pop_back();
        CHECK(!write_gi_vertex_light(bad_count, bytes, error));

        // Sections against a world: sizes, polygons, colours and rings.
        World world;
        world.models.resize(2);
        world.models[0].lightmaps = true;
        world.models[0].polygons.resize(4);
        world.models[0].polygons[1].lightmap_width = 2, world.models[0].polygons[1].lightmap_height = 1;
        world.models[0].polygons[3].lightmap_width = 1, world.models[0].polygons[3].lightmap_height = 3;
        world.models[0].polygons[0].points = {0, 1, 2};
        world.models[0].polygons[0].colors.resize(3);
        GiLightmaps two;
        two.blocks = {{0, 1, 2, 1}, {0, 3, 1, 3}};
        CHECK(check_gi_lightmaps(world, two, error));
        two.blocks[1].height = 2;
        CHECK(!check_gi_lightmaps(world, two, error) && error.find("0/3") != std::string::npos);
        two.blocks = {{1, 0, 1, 1}};
        CHECK(!check_gi_lightmaps(world, two, error)); // No such polygon.
        world.models[0].lightmaps = false;
        two.blocks = {{0, 1, 2, 1}};
        CHECK(!check_gi_lightmaps(world, two, error)); // The model's lightmaps did not validate.
        GiVertexLight sets;
        sets.sets = {{0, 0, 3}};
        CHECK(check_gi_vertex_light(world, sets, error));
        world.models[0].polygons[0].base_points = 2; // Extras after the base ring.
        CHECK(!check_gi_vertex_light(world, sets, error));
        world.models[0].polygons[0].base_points = UINT16_MAX;
        sets.sets = {{0, 1, 3}};
        CHECK(!check_gi_vertex_light(world, sets, error)); // No colours.
        sets.sets = {{2, 0, 3}};
        CHECK(!check_gi_vertex_light(world, sets, error));
        return true;
    }

    bool directions()
    {
        double worst_sin = 0, worst_cos = 0;
        for (int i = -1000; i <= 1000; ++i)
        {
            const double x = pi / 4 * i / 1000;
            double s, c;
            gi_quarter_sin_cos(x, s, c);
            worst_sin = std::max(worst_sin, std::abs(s - std::sin(x)));
            worst_cos = std::max(worst_cos, std::abs(c - std::cos(x)));
        }
        CHECK(worst_sin < 1e-13 && worst_cos < 1e-14);
        const DVector normals[]{{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {0, -1, 0}, {.6f, 0, .8f}, {0, .6f, -.8f},
                                {.48f, .6f, .64f}, {0, 0, -0.f}};
        for (auto n : normals)
        {
            if (n.z == 0 && n.x == 0 && n.y == 0)
                n = {0, 0, -1};
            for (const std::uint32_t count : {16u, 64u, 256u, 4096u})
            {
                double mean_cos = 0, mean_cos2 = 0;
                V3 mean;
                for (std::uint32_t i = 0; i < count; ++i)
                {
                    const auto d = gi_cosine_direction(n, i, count, .3, .7);
                    CHECK(near(norm(v3(d)), 1, 1e-6));
                    const double c = dot(v3(d), v3(n));
                    CHECK(c > -1e-7);
                    mean_cos += c / count, mean_cos2 += c * c / count;
                    mean = mean + v3(d) * (1.0 / count);
                }
                // Cosine-weighted moments: E[cos] = 2/3, E[cos^2] = 1/2, mean direction 2/3 n (measured
                // worst errors over these normals: .0098/.051 at 16 rays, .0017/.0019 at 256).
                const double moment = count == 4096 ? 5e-4 : count == 16 ? 2e-2 : 5e-3;
                const double direction = count == 4096 ? 1e-3 : count == 16 ? 8e-2 : count == 64 ? 2e-2 : 5e-3;
                CHECK(near(mean_cos, 2.0 / 3, moment) && near(mean_cos2, .5, moment));
                CHECK(norm(mean - v3(n) * (2.0 / 3)) < direction);
            }
        }
        // The lattice shift wraps: a shift of one is no shift.
        const auto a = gi_cosine_direction({0, 0, 1}, 5, 64, .25, .5);
        const auto b = gi_cosine_direction({0, 0, 1}, 5, 64, 1.25, 1.5);
        CHECK(near(a.x, b.x, 1e-6) && near(a.y, b.y, 1e-6) && near(a.z, b.z, 1e-6));
        return true;
    }

    // Receivers of a small world: block/set layout, sample placement, originals, ambient, owners,
    // root-BSP contents, face links, neighbours and section checks.
    bool placement(const fs::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        WorldBuilder b;
        b.world.info = "AmbientLight 10 20 30";
        // 0: a lightmapped triangle with DETAIL LOW colours (hypotenuse clamping).
        b.add({{{5, 5, 5}, {95, 5, 5}, {5, 95, 5}}, {0, 0, 1}, Kind::LowLightmap, "grey.dtx", 0xf800, {10, 20, 30}});
        // 1, 2: small vertex-lit squares sharing an edge (centroid nearer than twice the inset).
        b.add({quad_z(200, 0, 201.5f, 1.5f, 5), {0, 0, 1}, Kind::Vertex, "grey.dtx", 0, {1, 2, 3}});
        b.add({quad_z(201.5f, 0, 203, 1.5f, 5), {0, 0, 1}, Kind::Vertex, "grey.dtx", 0, {4, 5, 6}});
        // 3: a wall on the second square's far edge (perpendicular: no neighbour).
        b.add({{{203, 0, 5}, {203, 1.5f, 5}, {203, 1.5f, 6.5f}, {203, 0, 6.5f}}, {-1, 0, 0}, Kind::Vertex});
        // 4: a bright face (no receiver).
        b.add({quad_z(300, 0, 310, 10, 5), {0, 0, 1}, Kind::Bright});
        // Root BSP: x < 150 is solid.
        auto& model = b.world.models[0];
        model.planes.push_back({{1, 0, 0}, 150});
        BspNode node;
        node.plane = static_cast<std::uint32_t>(model.planes.size() - 1);
        node.sides = {BspNode::outside, BspNode::inside};
        model.bsp_nodes = {node};
        model.root_node = 0;
        Prepared p;
        CHECK(prepare(b.world, resources, p, 1));
        const auto& r = p.receivers;
        CHECK(r.blocks.size() == 1 && r.blocks[0].id.polygon == 0 && r.blocks[0].id.width == 6 &&
              r.blocks[0].id.height == 6 && r.texels == 36);
        CHECK(r.sets.size() == 4 && r.corners == 3 + 4 + 4 + 4 && r.low_sets == 1 && r.low_corners == 3);
        CHECK(r.sets[0].low && r.sets[0].id.polygon == 0 && !r.sets[1].low && r.sets[3].id.polygon == 3);
        CHECK(r.sets[0].first == 36 && r.sets[1].first == 39 && r.sets[3].first == 47 && r.elements() == 51);
        CHECK(r.ambient.x == 10 && r.ambient.y == 20 && r.ambient.z == 30);
        // Texel samples: centres at (20 i, 20 j), clamped into the triangle, raised half a unit.
        const std::vector<V3> triangle{{5, 5, 5}, {95, 5, 5}, {5, 95, 5}};
        std::size_t moved = 0;
        for (unsigned j = 0; j < 6; ++j)
            for (unsigned i = 0; i < 6; ++i)
            {
                const auto e = j * 6 + i;
                const auto expected = reference_sample({20.0 * i, 20.0 * j, 5}, triangle, {0, 0, 1}) + V3{0, 0, .5};
                CHECK(norm(v3(r.samples[e]) - expected) < 1e-4);
                moved += expected.x != 20.0 * i || expected.y != 20.0 * j;
                CHECK(r.solid[e] && r.owner[e] == 0);
                CHECK(r.original[e][0] == 255 && r.original[e][1] == 0 && r.original[e][2] == 0);
            }
        CHECK(moved == 36 - 6); // (20, 20), (20, 40), (20, 60), (40, 20), (40, 40), (60, 20) stay.
        // Corner samples: toward the centroid by min(1, half the distance).
        const double s = 1 / std::sqrt(2.0);
        CHECK(norm(v3(r.samples[36]) - V3{5 + s, 5 + s, 5.5}) < 1e-4);
        CHECK(norm(v3(r.samples[39]) - V3{200.375, .375, 5.5}) < 1e-4); // Half of 1.06: the centroid is near.
        CHECK(norm(v3(r.samples[43]) - V3{201.875, .375, 5.5}) < 1e-4);
        CHECK(norm(v3(r.samples[47]) - V3{202.5, .375, 5.375}) < 1e-4); // The wall's normal is -x.
        CHECK(r.original[36][0] == 10 && r.original[36][2] == 30 && r.original[39][1] == 2 && r.original[43][2] == 6);
        CHECK(r.solid[36] && !r.solid[39] && !r.solid[47]);
        CHECK(r.owner[36] == 1 && r.owner[39] == 2 && r.owner[46] == 3 && r.owner[50] == 4);
        // Neighbours: the two squares only (the wall is perpendicular, the LOW set has no peer).
        CHECK(r.neighbour_first.size() == 6 && r.neighbours.size() == 2);
        CHECK(r.neighbour_first[2] == 0 && r.neighbour_first[3] == 1 && r.neighbours[0] == 3 && r.neighbours[1] == 2);
        // Links: the triangle draws with its block (not its LOW set), the squares with their sets.
        std::size_t lightmap_links = 0, bright_links = 0;
        for (std::size_t f = 0; f < r.links.size(); ++f)
        {
            const auto& link = r.links[f];
            const auto& source = p.sources[p.scene.faces()[f].triangle];
            if (source.polygon == 0)
                CHECK(link.kind == GiReceivers::Link::Lightmap && link.receiver == 0);
            if (source.polygon == 1)
                CHECK(link.kind == GiReceivers::Link::Vertex && link.receiver == 1 && link.corners == source.corners);
            lightmap_links += link.kind == GiReceivers::Link::Lightmap;
            bright_links += link.kind == GiReceivers::Link::Bright;
        }
        CHECK(lightmap_links == 1 && bright_links == 2);
        // Lookups: bilinear over the block at the triangle's first vertex, st (0.75, 0.75).
        std::vector<float> values(r.elements() * 3);
        for (std::size_t i = 0; i < values.size(); ++i)
            values[i] = float(i / 3);
        std::uint32_t face = 0;
        while (r.links[face].kind != GiReceivers::Link::Lightmap)
            ++face;
        DVector out;
        const auto& link = r.links[face];
        CHECK(near(link.st[0][0], .75, 1e-5) && near(link.st[0][1], .75, 1e-5));
        CHECK(gi_receiver_sample(r, values, face, 0, 0, out));
        CHECK(near(out.x, .75 * (.75 * 0 + .25 * 1) + .25 * (.75 * 6 + .25 * 7), 1e-5));
        CHECK(!gi_receiver_sample(r, std::span(values).first(3), face, 0, 0, out));
        // Sections from these receivers fit the world.
        GiIndirect indirect;
        indirect.energy.assign(r.elements() * 3, .5f);
        indirect.valid.assign(r.elements(), 1);
        GiLightmaps maps;
        GiVertexLight vertex;
        gi_lightmap_sections(r, indirect, maps, vertex);
        std::string error;
        CHECK(check_gi_lightmaps(b.world, maps, error) && check_gi_vertex_light(b.world, vertex, error));
        CHECK(maps.texels.size() == 36 && vertex.corners.size() == 15 && gi_rgb9e5_decode(vertex.corners[0]).y == .5f);
        // Threads do not change the receivers.
        Prepared q;
        CHECK(prepare(b.world, resources, q, 7));
        CHECK(q.receivers.samples.size() == r.samples.size() &&
              std::memcmp(q.receivers.samples.data(), r.samples.data(), r.samples.size() * sizeof(DVector)) == 0);
        return true;
    }

    // Closed boxes of one albedo: E_b = 2 a D (1 + a + ... + a^(b-1)) on lightmapped walls, a D (...) on
    // vertex-lit walls; DETAIL LOW corners take the last pass's value.
    bool furnace(const fs::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        const double albedo = 153 / 255.f;
        const std::uint16_t texel = 6 << 11 | 12 << 5 | 6; // (49, 48, 49).
        GiSky sky;
        sky.fill({1, 1, 1}, 1); // Never seen: any miss would show.
        for (const bool lightmapped : {true, false})
        {
            WorldBuilder b;
            b.box({5, 5, 5}, {95, 95, 95},
                  [&](int, Face& face)
                  {
                      face.kind = lightmapped ? Kind::LowLightmap : Kind::Vertex;
                      face.texel = texel;
                      face.color = {51, 51, 51};
                  });
            Prepared p;
            CHECK(prepare(b.world, resources, p));
            const auto& r = p.receivers;
            CHECK(r.sets.size() == 6 && r.corners == 24 && r.blocks.size() == (lightmapped ? 6u : 0u));
            CHECK(r.texels == (lightmapped ? 216u : 0u) && r.low_sets == (lightmapped ? 6u : 0u));
            const auto o = expand(texel);
            for (std::uint32_t bounces = 1; bounces <= 3; ++bounces)
            {
                GiIndirect out;
                GiLightmapReport report;
                CHECK(bake(p, sky, bounces, 64, out, report));
                CHECK(report.back == 0 && report.sky == 0 && report.untextured == 0 && report.layer_limit == 0);
                CHECK(report.back_invalid == 0 && report.solid == 0 && report.filled == 0 && report.unfilled == 0);
                CHECK(report.pass_ms.size() == bounces && report.back_fraction[0] == r.elements());
                CHECK(report.rays == 64ull * (lightmapped ? 216 * bounces + 24 : 24 * bounces));
                double series = 0, power = 1;
                for (std::uint32_t k = 0; k < bounces; ++k)
                    series += power, power *= albedo;
                for (std::size_t e = 0; e < r.elements(); ++e)
                {
                    CHECK(out.valid[e]);
                    for (int c = 0; c < 3; ++c)
                    {
                        const double expected =
                            lightmapped ? 2 * albedo * (o[c] / 255.0) * series : albedo * .2 * series;
                        CHECK(near(out.energy[e * 3 + c], expected, 2e-6 * expected));
                    }
                }
            }
        }
        return true;
    }

    // A black box with a bright grey ceiling: a floor element's E is the ceiling's albedo times its
    // configuration factor.
    bool box(const fs::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        const double ceiling = 153 / 255.f;
        GiSky sky;
        sky.fill({1, 1, 1}, 1);
        for (const auto floor : {Kind::Lightmap, Kind::Vertex})
        {
            WorldBuilder b;
            b.box({5, 5, 5}, {95, 95, 95},
                  [&](int side, Face& face)
                  {
                      face.kind = side == 0 ? floor : Kind::Bright;
                      face.texture = side == 1 ? "grey.dtx" : side == 0 ? "white.dtx" : "black.dtx";
                  });
            Prepared p;
            CHECK(prepare(b.world, resources, p));
            const auto& r = p.receivers;
            CHECK(r.elements() == (floor == Kind::Lightmap ? 36u : 4u));
            GiIndirect out;
            GiLightmapReport report;
            CHECK(bake(p, sky, 1, 4096, out, report));
            CHECK(report.sky == 0 && report.back == 0);
            const std::vector<DVector> lid = quad_z(5, 5, 95, 95, 95);
            double worst = 0;
            for (std::size_t e = 0; e < r.elements(); ++e)
            {
                const double expected = ceiling * form_factor(r.samples[e], {0, 0, 1}, lid);
                for (int c = 0; c < 3; ++c)
                    worst = std::max(worst, std::abs(out.energy[e * 3 + c] - expected));
                CHECK(out.valid[e] && out.energy[e * 3] == out.energy[e * 3 + 1]);
            }
            std::cout << "BOX " << (floor == Kind::Lightmap ? "lightmap" : "vertex") << " worst=" << worst << '\n';
            CHECK(worst < 2e-3);
        }
        return true;
    }

    // An open floor under a bright square, a uniform sky around: E = F + (1 - F) sky scale.
    bool planes(const fs::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        WorldBuilder b;
        b.add({quad_z(5, 5, 95, 95, 5), {0, 0, 1}, Kind::Lightmap});
        const auto emitter = quad_z(25, 25, 75, 75, 45);
        b.add({{emitter[0], emitter[3], emitter[2], emitter[1]}, {0, 0, -1}, Kind::Bright, "white.dtx"});
        Prepared p;
        CHECK(prepare(b.world, resources, p));
        GiSky sky;
        sky.fill({.25f, .5f, .75f}, 2);
        const double radiance[3]{.5, 1, 1.5};
        for (const std::uint32_t bounces : {1u, 2u})
        {
            GiIndirect out;
            GiLightmapReport report;
            CHECK(bake(p, sky, bounces, 4096, out, report));
            CHECK(report.back == 0 && report.sky > 0 && report.untextured == 0);
            double worst = 0;
            for (std::size_t e = 0; e < p.receivers.elements(); ++e)
            {
                const double f = form_factor(p.receivers.samples[e], {0, 0, 1}, emitter);
                for (int c = 0; c < 3; ++c)
                    worst = std::max(worst, std::abs(out.energy[e * 3 + c] - (f + (1 - f) * radiance[c])));
            }
            std::cout << "PLANES bounces=" << bounces << " worst=" << worst << '\n';
            CHECK(worst < 3e-3);
        }
        return true;
    }

    // Two rooms behind one zero-thickness wall, the second lit: no light reaches the first.
    void two_rooms(WorldBuilder& b)
    {
        b.box({5, 5, 5}, {95, 95, 95}, [](int, Face&) {}); // Room A: dark lightmapped walls.
        b.box({95, 5, 5}, {185, 95, 95},
              [](int side, Face& face)
              {
                  if (side == 1)
                      face.kind = Kind::Bright, face.texture = "white.dtx";
                  if (side == 2 || side == 3)
                      face.kind = Kind::Vertex;
              });
    }
    bool leak(const fs::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        WorldBuilder b;
        two_rooms(b);
        Prepared p;
        CHECK(prepare(b.world, resources, p));
        const auto& r = p.receivers;
        GiSky sky;
        sky.fill({1, 1, 1}, 1);
        GiIndirect out;
        GiLightmapReport report;
        CHECK(bake(p, sky, 3, 256, out, report));
        CHECK(report.sky == 0);
        std::size_t a = 0, beyond = 0, floor = 0, lit = 0;
        for (std::size_t e = 0; e < r.elements(); ++e)
        {
            const auto receiver = r.owner[e];
            const bool block = receiver < r.blocks.size();
            const auto polygon = block ? r.blocks[receiver].id.polygon : r.sets[receiver - r.blocks.size()].id.polygon;
            if (polygon < 6) // Room A: dark, whatever its samples' validity.
            {
                ++a;
                CHECK(out.energy[e * 3] == 0 && out.energy[e * 3 + 1] == 0 && out.energy[e * 3 + 2] == 0);
                CHECK(r.samples[e].x < 95);
                // Texel centres past the shared wall stand in room B until clamped.
                DVector centre;
                const auto i = e - r.blocks[receiver].first;
                CHECK(block && r.blocks[receiver].frame.position(i % r.blocks[receiver].id.width,
                                                                  i / r.blocks[receiver].id.width, centre));
                beyond += centre.x > 95;
            }
            if (polygon == 6) // Room B's floor.
                ++floor, lit += out.energy[e * 3] > 0;
        }
        std::cout << "LEAK room_a=" << a << " beyond=" << beyond << " floor_b=" << floor << " lit=" << lit
                  << " back_invalid=" << report.back_invalid << " unfilled=" << report.unfilled << '\n';
        CHECK(a == 6 * 36 && beyond >= 6 * 4 && floor == 42 && lit == floor);
        return true;
    }

    // In-block dilation, neighbour fill across a shared edge, and unfilled receivers.
    std::vector<float> dilate(int width, int height, std::vector<float> values, std::vector<std::uint8_t> known)
    {
        std::size_t missing = std::count(known.begin(), known.end(), 0);
        while (missing && missing < known.size())
        {
            const auto snapshot = values;
            auto now = known;
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
                            if ((dx || dy) && nx >= 0 && ny >= 0 && nx < width && ny < height &&
                                known[std::size_t(ny) * width + nx])
                            {
                                for (int c = 0; c < 3; ++c)
                                    sum[c] += snapshot[(std::size_t(ny) * width + nx) * 3 + c];
                                ++count;
                            }
                        }
                    if (!count)
                        continue;
                    for (int c = 0; c < 3; ++c)
                        values[at * 3 + c] = float(sum[c] / count);
                    now[at] = 1;
                    --missing;
                }
            known = std::move(now);
        }
        return values;
    }
    bool dilation(const fs::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        WorldBuilder b;
        b.add({quad_z(45, 5, 95, 95, 5), {0, 0, 1}, Kind::Lightmap});         // 0: half solid.
        b.add({quad_z(5, 5, 45, 95, 5), {0, 0, 1}, Kind::Lightmap});          // 1: solid, shares an edge with 0.
        b.add({quad_z(-95, 5, -55, 95, 5), {0, 0, 1}, Kind::Lightmap});       // 2: solid, alone.
        b.add({quad_z(5, 95, 45, 135, 5), {0, 0, 1}, Kind::Vertex});          // 3: solid, beside 1 (other kind).
        const auto lamp = quad_z(60, 5, 95, 95, 45);
        b.add({{lamp[0], lamp[3], lamp[2], lamp[1]}, {0, 0, -1}, Kind::Bright, "white.dtx"});
        auto& model = b.world.models[0];
        model.planes.push_back({{1, 0, 0}, 50});
        BspNode node;
        node.plane = static_cast<std::uint32_t>(model.planes.size() - 1);
        node.sides = {BspNode::outside, BspNode::inside};
        model.bsp_nodes = {node};
        model.root_node = 0;
        Prepared p;
        CHECK(prepare(b.world, resources, p));
        const auto& r = p.receivers;
        CHECK(r.blocks.size() == 3 && r.sets.size() == 1 && r.blocks[0].id.width == 4 && r.blocks[0].id.height == 6);
        CHECK(r.neighbour_first[1] - r.neighbour_first[0] == 1 && r.neighbours[r.neighbour_first[0]] == 1);
        GiSky sky;
        sky.fill({.25f, .25f, .25f}, 1);
        GiIndirect out;
        GiLightmapReport report;
        CHECK(bake(p, sky, 1, 256, out, report));
        const auto& first = r.blocks[0];
        std::vector<float> values(24 * 3);
        std::vector<std::uint8_t> known(24);
        std::size_t solid = 0;
        for (std::size_t i = 0; i < 24; ++i)
        {
            const auto e = first.first + i;
            known[i] = !r.solid[e] && out.valid[e];
            solid += r.solid[e];
            CHECK(r.solid[e] == (i % 4 == 0) && out.valid[e] == !r.solid[e]);
            for (int c = 0; c < 3; ++c)
                values[i * 3 + c] = out.energy[e * 3 + c];
            if (known[i])
                CHECK(out.energy[e * 3] >= .25f);
        }
        CHECK(solid == 6);
        const auto filled = dilate(4, 6, values, known);
        CHECK(std::memcmp(filled.data(), out.energy.data() + first.first * 3, filled.size() * sizeof(float)) == 0);
        double mean[3]{};
        for (std::size_t i = 0; i < 24; ++i)
            for (int c = 0; c < 3; ++c)
                mean[c] += filled[i * 3 + c];
        for (std::size_t e = r.blocks[1].first; e < r.blocks[1].first + 24u; ++e)
            for (int c = 0; c < 3; ++c)
                CHECK(out.energy[e * 3 + c] == float(mean[c] / 24) && !out.valid[e]);
        for (std::size_t e = r.blocks[2].first; e < r.elements(); ++e)
            CHECK(out.energy[e * 3] == 0 && out.energy[e * 3 + 1] == 0 && out.energy[e * 3 + 2] == 0);
        CHECK(report.solid == 6 + 24 + 24 + 4 && report.back_invalid == 0);
        CHECK(report.filled == 6 && report.neighbour_filled == 1 && report.unfilled == 2);
        return true;
    }

    // Back faces: a sample seeing more than a tenth of back faces is invalid; a back hit carries
    // nothing, even on a bright face.
    bool back(const fs::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        GiSky sky;
        sky.fill({.5f, .5f, .5f}, 1);
        for (const bool large : {true, false})
        {
            WorldBuilder b;
            b.add({quad_z(5, 5, 95, 95, 5), {0, 0, 1}, Kind::Lightmap});
            b.add({large ? quad_z(-500, -500, 600, 600, 50) : quad_z(45, 45, 55, 55, 50), {0, 0, 1}, Kind::Bright,
                   "white.dtx"});
            Prepared p;
            CHECK(prepare(b.world, resources, p));
            const auto& r = p.receivers;
            GiIndirect out;
            GiLightmapReport report;
            CHECK(bake(p, sky, 1, 256, out, report));
            std::uint64_t traced = 0, back_rays = 0;
            for (const auto count : report.back_fraction)
                traced += count;
            CHECK(traced == r.elements() && report.back > 0);
            for (std::size_t e = 0; e < r.elements(); ++e)
            {
                if (large)
                {
                    CHECK(!out.valid[e] && out.energy[e * 3] == 0);
                    continue;
                }
                CHECK(out.valid[e]);
                const double misses = out.energy[e * 3] * 256 / .5; // Each miss adds .5 / 256 exactly.
                CHECK(misses == std::floor(misses) && misses >= 231);
                back_rays += 256 - std::uint64_t(misses);
            }
            if (large)
                CHECK(report.back_invalid == r.elements() && report.unfilled == 1 && report.back_fraction[0] == 0);
            else
                CHECK(report.back_invalid == 0 && back_rays == report.back);
        }
        return true;
    }

    // Bitwise identical results for 1, 2 and 7 jobs; another seed changes them.
    bool determinism(const fs::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        WorldBuilder b;
        two_rooms(b);
        Prepared p;
        CHECK(prepare(b.world, resources, p));
        GiSky sky;
        sky.fill({.3f, .4f, .5f}, 1.5f);
        GiIndirect one;
        GiLightmapReport first;
        CHECK(bake(p, sky, 3, 64, one, first, 1));
        for (const unsigned jobs : {2u, 7u})
        {
            GiIndirect other;
            GiLightmapReport report;
            CHECK(bake(p, sky, 3, 64, other, report, jobs));
            CHECK(std::memcmp(other.energy.data(), one.energy.data(), one.energy.size() * sizeof(float)) == 0);
            CHECK(other.valid == one.valid && report.rays == first.rays && report.back == first.back);
            CHECK(report.back_fraction == first.back_fraction && report.filled == first.filled);
            CHECK(report.threads == jobs);
        }
        GiIndirect reseeded;
        GiLightmapReport report;
        CHECK(bake(p, sky, 3, 64, reseeded, report, 2, 0x5eee));
        CHECK(std::memcmp(reseeded.energy.data(), one.energy.data(), one.energy.size() * sizeof(float)) != 0);
        return true;
    }

    // The sky objects (D9) and the painted cube.
    bool sky(const fs::path& root)
    {
        {
            World world;
            world.models.resize(5);
            world.models[1].name = "SkyA";
            world.models[2].name = "SkyB";
            world.models[3].name = "DoorC";
            world.models[4].name = "Unused";
            const auto setup0 = gi_sky_setup(world);
            CHECK(!setup0.defined && setup0.models.empty() && setup0.slots[0] == -1 && !setup0.fog.enabled);
            using P = std::vector<WorldProperty>;
            world.objects = {
                object("DemoSkyWorldModel", P{property("Name", std::string("SkyA")), property("Index", 2.f),
                                               property("SkyDims", DVector{10, 10, 10})}),
                object("DemoSkyWorldModel", P{property("Name", std::string("SkyB")), property("Index", 30.f),
                                               property("SkyDims", DVector{5, 5, 5})}),
                object("DemoSkyWorldModel", P{property("Name", std::string("SkyB")), property("Index", 3.f),
                                               property("SkyDims", DVector{-1, 1, 1})}),
                object("DemoSkyWorldModel",
                       P{property("Name", std::string("SkyB")), property("Index", 4.f),
                         property("SkyDims", DVector{20, 20, 20}), property("InnerPercentX", .5f),
                         property("Pos", DVector{100, 0, 0}), property("Flags", 0.f)}),
                object("Door", P{property("Name", std::string("DoorC"))}),
                object("Prop", P{property("Name", std::string("PropD"))}),
                object("SkyPointer", P{property("Index", 2.f), property("SkyObjectName", std::string("DoorC"))}),
                object("SkyPointer", P{property("Index", 5.f), property("SkyObjectName", std::string("SkyA"))}),
                object("SkyPointer", P{property("Index", 6.f), property("SkyObjectName", std::string("doorc"))}),
                object("SkyPointer", P{property("Index", 7.f), property("SkyObjectName", std::string("Nothing"))}),
                object("SkyPointer", P{property("Index", 8.f), property("SkyObjectName", std::string("PropD"))}),
            };
            const auto setup = gi_sky_setup(world);
            CHECK(setup.defined && setup.definition.min.x == 80 && setup.definition.max.y == 20);
            CHECK(setup.definition.view_min.x == 90 && setup.definition.view_max.z == 2);
            CHECK(setup.slots[2] == 3 && setup.visible[2] && setup.slots[4] == 2 && !setup.visible[4]);
            CHECK(setup.slots[3] == -1 && setup.slots[5] == -1 && setup.slots[6] == -1);
            CHECK((setup.models == std::vector<std::uint32_t>{1, 2, 3})); // SkyA stays in the sky.
            CHECK(setup.pointers == 5 && setup.unresolved_pointers == 3);
            std::vector<GiRangeUse> use;
            GiSceneReport report;
            std::string error;
            CHECK(gi_range_uses(world, {}, use, report, error));
            CHECK(use[1] == GiRangeUse::Sky && use[3] == GiRangeUse::Sky && use[4] == GiRangeUse::Skip);
            // Fog: the last WorldProperties decides; truncated channels over 255.
            const auto fog = object("WorldProperties",
                                    P{property("FogEnable", 1.f), property("FogColor", DVector{10.7f, 300, -5}),
                                      property("SkyFog", 1.f), property("SkyFogNearZ", 100.9f),
                                      property("SkyFogFarZ", 1000.2f)});
            world.objects.push_back(fog);
            auto fogged = gi_sky_setup(world).fog;
            CHECK(fogged.enabled && fogged.color.x == 10 / 255.f && fogged.color.y == 1 && fogged.color.z == 0);
            CHECK(fogged.start == 100 && fogged.end == 1000);
            world.objects.push_back(object("WorldProperties", P{property("FogEnable", 0.f)}));
            CHECK(!gi_sky_setup(world).fog.enabled);
            world.objects.back() =
                object("WorldProperties", P{property("FogEnable", 1.f), property("SkyFog", 1.f),
                                            property("SkyFogNearZ", 50.f), property("SkyFogFarZ", 50.f)});
            CHECK(!gi_sky_setup(world).fog.enabled); // An empty fog range.
        }
        Resources resources;
        CHECK(mount(root, resources));
        const char* colours[6]{"red.dtx", "green.dtx", "blue.dtx", "yellow.dtx", "cyan.dtx", "magenta.dtx"};
        const DVector expected[6]{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}, {0, 1, 1}, {1, 0, 1}};
        const DVector axes[6]{{0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, 1, 0}, {-1, 0, 0}, {1, 0, 0}};
        auto make = [&](bool walls, std::vector<WorldObject> extra)
        {
            WorldBuilder b;
            b.add({quad_z(-100, -100, 100, 100, -100), {0, 0, 1}, Kind::Bright});
            b.world.min = {-100, -100, -100}, b.world.max = {100, 100, 100};
            b.world.models.resize(2);
            b.world.models[1].name = "SkyBox";
            if (walls)
                b.box({-50, -50, 950}, {50, 50, 1050},
                      [&](int side, Face& face)
                      {
                          face.kind = Kind::Bright;
                          face.texture = colours[side];
                      },
                      1);
            else
                b.add({{{-50, -50, 1050}, {-50, 50, 1050}, {50, 50, 1050}, {50, -50, 1050}}, {0, 0, -1}, Kind::Bright,
                       colours[1]},
                      1);
            b.world.objects = {object("DemoSkyWorldModel",
                                      std::vector<WorldProperty>{property("Name", std::string("SkyBox")),
                                                                 property("Index", 0.f),
                                                                 property("SkyDims", DVector{60, 60, 60}),
                                                                 property("InnerPercentX", 0.f),
                                                                 property("InnerPercentY", 0.f),
                                                                 property("InnerPercentZ", 0.f),
                                                                 property("Pos", DVector{0, 0, 1000})})};
            for (auto& o : extra)
                b.world.objects.push_back(std::move(o));
            return b.world;
        };
        auto build = [&](const World& world, GiSky& out, GiSkyReport& report, std::string& error,
                         GiSkySetup* edit = nullptr)
        {
            WorldMesh mesh;
            std::vector<GiRangeUse> use;
            GiSceneReport scene_report;
            GiBakeScene scene;
            if (!mesh.build(world, error, &resources, true) || !gi_range_uses(world, {}, use, scene_report, error) ||
                !scene.build(std::move(mesh), use, scene_report, error))
                return false;
            auto setup = gi_sky_setup(world);
            if (edit)
                setup = *edit;
            return out.build(world, scene, setup, 2, 3, report, error);
        };
        std::string error;
        {
            const auto world = make(true, {});
            GiSky cube;
            GiSkyReport report;
            CHECK(build(world, cube, report, error));
            CHECK(report.slots == 1 && report.triangles == 12 && report.covered > .9999);
            CHECK(report.eye.x == 0 && report.eye.y == 0 && report.eye.z == 1000);
            for (int s = 0; s < 6; ++s)
            {
                const auto d = cube.displayed(axes[s]), l = cube.radiance(axes[s]);
                CHECK(near(d.x, expected[s].x, 1e-6) && near(d.y, expected[s].y, 1e-6) &&
                      near(d.z, expected[s].z, 1e-6));
                CHECK(l.x == 2 * d.x && l.y == 2 * d.y && l.z == 2 * d.z && cube.scale() == 2);
            }
            // Cube texel centres map back to their own texel.
            for (unsigned face = 0; face < 6; ++face)
                for (unsigned y = 0; y < GiSky::size; y += 9)
                    for (unsigned x = 0; x < GiSky::size; x += 7)
                    {
                        const auto d = cube.displayed(gi_cube_direction(face, x + .5f, y + .5f));
                        const auto* t = &cube.texels()[((std::size_t(face) * GiSky::size + y) * GiSky::size + x) * 3];
                        CHECK(near(d.x, t[0], 1e-5) && near(d.y, t[1], 1e-5) && near(d.z, t[2], 1e-5));
                    }
            // Hidden holders draw nothing, so no definition is needed.
            auto setup = gi_sky_setup(world);
            setup.visible[0] = false;
            auto flat = world;
            flat.max = flat.min;
            CHECK(!build(flat, cube, report, error) && error.find("sky definition") != std::string::npos);
            CHECK(build(flat, cube, report, error, &setup) && report.slots == 0 && report.covered == 0);
            const auto none = cube.displayed({0, 0, 1});
            CHECK(none.x == 0 && none.y == 0 && none.z == 0);
        }
        {
            // One lid: a sixth of the cube, the rest takes its colour.
            const auto world = make(false, {});
            GiSky cube;
            GiSkyReport report;
            CHECK(build(world, cube, report, error));
            CHECK(report.triangles == 2 && near(report.covered, 1.0 / 6, 1e-6));
            CHECK(near(report.average.y, 1, 1e-6) && report.average.x == 0);
            for (const auto& axis : axes)
                CHECK(near(cube.displayed(axis).y, 1, 1e-6) && cube.displayed(axis).x == 0);
        }
        {
            // SkyFog over the radial distance: half-way to white at 50 units.
            const std::vector<WorldProperty> fog{property("FogEnable", 1.f), property("SkyFog", 1.f),
                                                 property("FogColor", DVector{255, 255, 255}),
                                                 property("SkyFogNearZ", 0.f), property("SkyFogFarZ", 100.f)};
            const auto world = make(true, {object("WorldProperties", fog)});
            GiSky cube;
            GiSkyReport report;
            CHECK(build(world, cube, report, error));
            const auto d = cube.displayed({1, 0, 0});
            CHECK(near(d.x, 1, 1e-6) && near(d.y, .5, 1e-3) && near(d.z, 1, 1e-6));
        }
        return true;
    }

    // The --gi-preview helpers: views, panel values, sections back to energies, and a render.
    bool preview(const fs::path& root)
    {
        {
            World world;
            using P = std::vector<WorldProperty>;
            world.objects = {
                object("GameStartPoint",
                       P{property("Pos", DVector{0, 0, 0}), property("Rotation", EulerAngles{{0, 0, 0}})}),
                object("GameStartPoint", P{property("Rotation", EulerAngles{{0, 1, 0}})}), // No Pos: skipped.
                object("gamestartpoint", P{property("Pos", DVector{10, 20, 30})}),
            };
            for (int i = 0; i < 4; ++i)
                world.objects.push_back(object("GameStartPoint", P{property("Pos", DVector{float(i), 0, 0})}));
            const auto views = gi_preview_views(world);
            CHECK(views.size() == 6 && views[0].label == "start0" && views[1].label == "start0+120");
            CHECK(views[3].label == "start1" && views[3].eye.y == 56 && views[3].eye.z == 30 && views[0].eye.y == 36);
            const double c = dot(v3(views[0].forward), v3(views[1].forward));
            CHECK(near(c, -.5, 1e-6) && near(dot(v3(views[0].forward), v3(views[2].forward)), -.5, 1e-6));
            CHECK(near(views[0].forward.z, 1, 1e-6) && views[1].forward.y == views[0].forward.y);
            CHECK(gi_preview_views(World{}).empty());
        }
        {
            GiReceivers r;
            r.texels = 1, r.corners = 1;
            r.original = {{100, 200, 30}, {50, 60, 70}};
            r.ambient = {20, 20, 20};
            GiIndirect indirect;
            indirect.energy = {.1f, .2f, .3f, .4f, .5f, .6f};
            const auto values = gi_preview_values(r, indirect, 2, .5f);
            for (std::size_t e = 0; e < 2; ++e)
                for (int c = 0; c < 3; ++c)
                {
                    const auto i = e * 3 + c;
                    CHECK(values.original[i] == r.original[e][c] / 255.f);
                    CHECK(values.composed[i] ==
                          gi_compose(r.original[e][c], 20, indirect.energy[i], 2, .5f, e == 0) / 255.f);
                    CHECK(values.indirect[i] == gi_preview_indirect_exposure * 2 * indirect.energy[i]);
                }
        }
        Resources resources;
        CHECK(mount(root, resources));
        WorldBuilder b;
        const std::uint16_t texel = 6 << 11 | 12 << 5 | 6;
        b.box({5, 5, 5}, {95, 95, 95}, [&](int, Face& face) { face.texel = texel; });
        Prepared p;
        CHECK(prepare(b.world, resources, p));
        GiSky sky;
        sky.fill({0, 0, 0}, 1);
        GiIndirect out;
        GiLightmapReport report;
        CHECK(bake(p, sky, 2, 64, out, report));
        GiLightmaps maps;
        GiVertexLight vertex;
        gi_lightmap_sections(p.receivers, out, maps, vertex);
        GiIndirect back;
        std::string error;
        CHECK(gi_indirect_from_sections(p.receivers, maps, vertex, back, error));
        CHECK(back.valid == out.valid);
        for (std::size_t i = 0; i < out.energy.size(); ++i)
            CHECK(near(back.energy[i], out.energy[i], out.energy[i] / 256));
        auto wrong = maps;
        wrong.blocks[2].width = 5;
        GiIndirect refused;
        CHECK(!gi_indirect_from_sections(p.receivers, wrong, vertex, refused, error) && refused.energy.empty());
        CHECK(error.find("do not match") != std::string::npos);
        // A render from the middle of the uniform box: three uniform panels.
        const auto values = gi_preview_values(p.receivers, back, 1, 1);
        std::vector<std::uint8_t> rgba;
        render_gi_preview(p.scene, sky, p.receivers, values, {{50, 50, 50}, {0, 0, 1}, "test"}, 8, 6, 2, rgba);
        CHECK(rgba.size() == 8 * 3 * 6 * 4);
        const double albedo = 153 / 255.f;
        const auto o = expand(texel);
        const double e0 = back.energy[0];
        for (int c = 0; c < 3; ++c)
        {
            const double composed = gi_compose(o[c], 0, back.energy[c], 1, 1, true) / 255.0;
            const double panel[3]{albedo * 2 * o[c] / 255.0, albedo * 2 * std::min(composed, 1.0),
                                  albedo * gi_preview_indirect_exposure * back.energy[c]};
            for (int y = 0; y < 6; ++y)
                for (int x = 0; x < 24; ++x)
                {
                    const int value = rgba[(std::size_t(y) * 24 + x) * 4 + c];
                    CHECK(std::abs(value - panel[x / 8] * 255) <= 1);
                }
        }
        CHECK(e0 > 0);
        return true;
    }

    // 01_SubwayA at Preview through the driver: readable sections that fit the world and the receivers,
    // and E = 0 composes every stored value back.
    bool retail(const fs::path& data, const fs::path& placement_dir, const fs::path& scratch)
    {
        CHECK(Testing::reset_scratch_dir(scratch));
        Config config;
        config.data_dir = data;
        Resources resources;
        std::string error;
        CHECK(resources.mount_game(config, error));
        const std::string path = "worlds/01_subwaya.dat";
        GiBakeRequest request;
        request.world = path;
        CHECK(make_gi_bake_settings("preview", 0, 0, -1, request.settings, error));
        request.placement_dir = placement_dir;
        request.out_dir = scratch / "bake";
        const auto baked = bake_gi_world(resources, request);
        CHECK(baked.status == GiBakeResult::Status::Baked && baked.exit_code == 0);
        std::vector<std::uint8_t> bytes;
        GiFile file;
        CHECK(read_gi_bytes(path_text(baked.file), bytes, error) && read_gi_file(bytes, file, error));
        const auto* lmap = file.find(gi_format::lightmaps);
        const auto* vtx = file.find(gi_format::vertices);
        GiLightmaps maps;
        GiVertexLight vertex;
        CHECK(lmap && vtx && read_gi_lightmaps(lmap->payload, maps, error) &&
              read_gi_vertex_light(vtx->payload, vertex, error));
        std::vector<std::uint8_t> world_bytes;
        World world;
        CHECK(resources.read(path, world_bytes, error) && read_world(world_bytes, world, error));
        CHECK(check_gi_lightmaps(world, maps, error) && check_gi_vertex_light(world, vertex, error));
        Prepared p;
        CHECK(prepare(world, resources, p, 0));
        const auto& r = p.receivers;
        GiIndirect indirect;
        CHECK(gi_indirect_from_sections(r, maps, vertex, indirect, error));
        const auto& report = baked.lightmaps;
        CHECK(report.blocks == maps.blocks.size() && report.texels == maps.texels.size());
        CHECK(report.sets == vertex.sets.size() && report.corners == vertex.corners.size());
        std::uint64_t traced = 0, valid = 0;
        for (const auto count : report.back_fraction)
            traced += count;
        for (const auto v : indirect.valid)
            valid += v;
        CHECK(traced == r.elements() - report.solid && valid == traced - report.back_invalid);
        const float ambient[3]{r.ambient.x, r.ambient.y, r.ambient.z};
        for (std::size_t e = 0; e < r.elements(); ++e)
            for (int c = 0; c < 3; ++c)
                CHECK(gi_compose(r.original[e][c], ambient[c], 0, 1, 1, e < r.texels) == r.original[e][c]);
        std::cout << "LIGHTMAPS blocks=" << report.blocks << " texels=" << report.texels << " sets=" << report.sets
                  << " corners=" << report.corners << " low=" << report.low_sets << '/' << report.low_corners
                  << " solid=" << report.solid << " back_invalid=" << report.back_invalid << " filled=" << report.filled
                  << " neighbour_filled=" << report.neighbour_filled << " unfilled=" << report.unfilled
                  << " rays=" << report.rays << " ms=" << report.ms << "\nSKY slots=" << baked.sky.slots
                  << " triangles=" << baked.sky.triangles << " covered=" << baked.sky.covered << " average="
                  << baked.sky.average.x << ',' << baked.sky.average.y << ',' << baked.sky.average.z << '\n';
        return Testing::remove_scratch_dir(scratch);
    }

    // ---- Lot G8: the world override ----

    // The mesh data a GI composition may change (pages and vertex colours) and everything else.
    bool same_vertices(const WorldMesh& a, const WorldMesh& b)
    {
        if (a.vertices.size() != b.vertices.size())
            return false;
        for (std::size_t i = 0; i < a.vertices.size(); ++i)
        {
            const auto &x = a.vertices[i], &y = b.vertices[i];
            if (std::memcmp(&x.position, &y.position, sizeof x.position) ||
                std::memcmp(&x.color, &y.color, sizeof x.color) || x.uv != y.uv || x.lightmap != y.lightmap ||
                std::memcmp(&x.normal, &y.normal, sizeof x.normal))
                return false;
        }
        return true;
    }
    bool same_materials(const WorldMesh& a, const WorldMesh& b)
    {
        if (a.materials.size() != b.materials.size())
            return false;
        for (std::size_t i = 0; i < a.materials.size(); ++i)
            if (a.materials[i].path != b.materials[i].path || a.materials[i].image.rgba != b.materials[i].image.rgba ||
                a.materials[i].image.mip_rgba != b.materials[i].image.mip_rgba)
                return false;
        return true;
    }
    bool same_mesh(const WorldMesh& a, const WorldMesh& b)
    {
        return same_vertices(a, b) && same_materials(a, b) && a.ranges.size() == b.ranges.size() &&
               a.lightmapped == b.lightmapped && a.lightmap_pages == b.lightmap_pages && a.lit == b.lit &&
               a.fullbright == b.fullbright;
    }
    bool is_page(const WorldMaterial& m)
    {
        return m.path.rfind("lightmap page", 0) == 0;
    }
    // Every opaque (alpha 255) page colour but the unused white page area.
    std::set<std::array<std::uint8_t, 3>> page_colors(const WorldMesh& mesh)
    {
        std::set<std::array<std::uint8_t, 3>> out;
        for (const auto& m : mesh.materials)
            if (is_page(m))
                for (std::size_t p = 0; p + 3 < m.image.rgba.size(); p += 4)
                    if (m.image.rgba[p + 3] == 255)
                        out.insert({m.image.rgba[p], m.image.rgba[p + 1], m.image.rgba[p + 2]});
        out.erase({255, 255, 255});
        return out;
    }
    GiFile section_file(const GiLightmaps& maps, const GiVertexLight& vertex, bool with_vertex = true)
    {
        GiFile file;
        std::string error;
        GiSection lmap{gi_format::lightmaps, {}}, vtx{gi_format::vertices, {}};
        write_gi_lightmaps(maps, lmap.payload, error);
        write_gi_vertex_light(vertex, vtx.payload, error);
        file.sections.push_back(std::move(lmap));
        if (with_vertex)
            file.sections.push_back(std::move(vtx));
        return file;
    }

    GiFile probe_file(const GiLightmaps&, const GiVertexLight&, const GiLightLattice&, const GiProbes&);

    // A box of lightmapped, vertex-lit and DETAIL LOW faces with an ambient fill. E = 0 at Final is
    // byte-identical to no GI; otherwise pages and corners follow gi_compose, view by view, and only
    // listed polygons change.
    bool runtime_override(const fs::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        WorldBuilder b;
        b.world.info = "AmbientLight 40 20 10";
        const std::uint16_t texel = 20 << 11 | 30 << 5 | 12, other = 6 << 11 | 12 << 5 | 6;
        std::map<std::uint32_t, std::uint16_t> face_texel;
        b.box({5, 5, 5}, {95, 95, 95},
              [&](int s, Face& face)
              {
                  face.texel = s == 5 ? other : texel;
                  face.kind = s == 2 ? Kind::Vertex : s == 3 ? Kind::LowLightmap : Kind::Lightmap;
                  face.color = s == 2 ? std::array<std::uint8_t, 3>{90, 80, 70}
                                      : std::array<std::uint8_t, 3>{60, 50, 30};
                  face.texture = s == 4 ? "red.dtx" : "grey.dtx";
                  face_texel[std::uint32_t(s)] = face.texel;
              });
        Prepared p;
        CHECK(prepare(b.world, resources, p));
        const auto& r = p.receivers;
        CHECK(r.blocks.size() == 5 && r.sets.size() == 2); // Faces 0, 1, 3, 4, 5; faces 2 and 3 (low).
        std::string error;
        WorldMesh off, off_low;
        std::vector<WorldTriangleSource> sources, sources_low;
        CHECK(off.build(b.world, error, &resources, true, &sources));
        CHECK(off_low.build(b.world, error, &resources, false, &sources_low));
        const DVector ambient{40, 20, 10};
        const float a[3]{40, 20, 10};

        // E = 0: the exact identity, whatever k.
        GiIndirect zero;
        zero.energy.assign(r.elements() * 3, 0.f);
        zero.valid.assign(r.elements(), 1);
        GiLightmaps maps;
        GiVertexLight vertex;
        gi_lightmap_sections(r, zero, maps, vertex);
        GiWorldLight light;
        CHECK(gi_world_light(b.world, section_file(maps, vertex), light, error));
        CHECK(light.ambient.x == ambient.x && light.ambient.y == ambient.y && light.ambient.z == ambient.z);
        CHECK(light.block_first.size() == 5 && light.set_first.size() == 2 && light.set_first[1] == 4);
        for (const float k : {0.f, 1.f, 2.f, 4.f})
        {
            light.composition = {k, 1, GiView::Final};
            WorldMesh on, on_low;
            CHECK(on.build(b.world, error, &resources, true, nullptr, &light));
            CHECK(on_low.build(b.world, error, &resources, false, nullptr, &light));
            CHECK(same_mesh(on, off) && same_mesh(on_low, off_low));
            CHECK(on.gi_blocks == 5 && on.gi_texels == maps.texels.size());
            CHECK(on.gi_sets == 1 && on.gi_corners == 4 && on_low.gi_blocks == 0 && on_low.gi_sets == 2 &&
                  on_low.gi_corners == 8 && on.gi_white_materials == 0);
        }
        // Block 2 is face 3, the DETAIL LOW face: lightmapped at high detail.
        CHECK(r.blocks[2].id.polygon == 3 && r.sets[1].low && r.sets[1].id.polygon == 3);

        // A uniform E per block and a varying one per corner, with some invalid elements.
        GiIndirect energy;
        energy.energy.resize(r.elements() * 3);
        energy.valid.resize(r.elements());
        for (std::size_t bi = 0; bi < r.blocks.size(); ++bi)
            for (std::uint32_t t = 0; t < std::uint32_t(r.blocks[bi].id.width) * r.blocks[bi].id.height; ++t)
            {
                const auto e = r.blocks[bi].first + t;
                for (int c = 0; c < 3; ++c)
                    energy.energy[e * 3 + c] = .02f * float(bi + 1) * float(c + 1);
                energy.valid[e] = bi != 1;
            }
        for (std::size_t e = r.texels; e < r.elements(); ++e)
        {
            for (int c = 0; c < 3; ++c)
                energy.energy[e * 3 + c] = .03f * float(e - r.texels + 1) + .01f * float(c);
            energy.valid[e] = e % 2;
        }
        gi_lightmap_sections(r, energy, maps, vertex);
        CHECK(gi_world_light(b.world, section_file(maps, vertex), light, error));
        auto block_energy = [&](std::size_t bi) { return gi_rgb9e5_decode(maps.texels[r.blocks[bi].first]); };
        auto corner_energy = [&](std::size_t set, std::uint16_t corner)
        { return gi_rgb9e5_decode(vertex.corners[r.sets[set].first - r.texels + corner]); };
        // The expected page colours (a set per block) and vertex colours, per view.
        auto expect_pages = [&](const std::function<std::uint8_t(std::uint8_t o, float a, float e, int c)>& f)
        {
            std::set<std::array<std::uint8_t, 3>> out;
            for (std::size_t bi = 0; bi < r.blocks.size(); ++bi)
            {
                const auto o = expand(face_texel[r.blocks[bi].id.polygon]);
                const auto e = block_energy(bi);
                const float ev[3]{e.x, e.y, e.z};
                std::array<std::uint8_t, 3> rgb{};
                for (int c = 0; c < 3; ++c)
                    rgb[c] = f(o[c], a[c], ev[c], c);
                out.insert(rgb);
            }
            return out;
        };
        // Vertex colours of `mesh` against `f` over the set's corners; unlisted triangles unchanged.
        auto check_corners = [&](const WorldMesh& mesh, const WorldMesh& reference,
                                 const std::vector<WorldTriangleSource>& src,
                                 const std::function<float(std::uint8_t o, float a, float e, int c, bool valid)>& f)
        {
            for (std::size_t t = 0; t < src.size(); ++t)
                for (int v = 0; v < 3; ++v)
                {
                    const auto& got = mesh.vertices[t * 3 + v].color;
                    const auto& was = reference.vertices[t * 3 + v].color;
                    if (src[t].light != WorldFaceLight::Vertex)
                    {
                        if (std::memcmp(&got, &was, sizeof got))
                            return false;
                        continue;
                    }
                    const auto polygon = src[t].polygon;
                    const std::size_t set = polygon == 2 ? 0 : 1;
                    const auto corner = src[t].corners[v];
                    const auto e = corner_energy(set, corner);
                    const float ev[3]{e.x, e.y, e.z}, gv[3]{got.x, got.y, got.z};
                    const auto& o = b.world.models[0].polygons[polygon].colors[corner];
                    const bool valid = vertex.valid[r.sets[set].first - r.texels + corner];
                    for (int c = 0; c < 3; ++c)
                        if (gv[c] != f(o[c], a[c], ev[c], c, valid))
                            return false;
                }
            return true;
        };
        auto build = [&](GiView view, float k, float f, WorldMesh& high, WorldMesh& low)
        {
            light.composition = {k, f, view};
            return high.build(b.world, error, &resources, true, nullptr, &light) &&
                   low.build(b.world, error, &resources, false, nullptr, &light);
        };
        WorldMesh final_high, final_low;
        CHECK(build(GiView::Final, 2, 1, final_high, final_low));
        CHECK(page_colors(final_high) ==
              expect_pages([](std::uint8_t o, float a, float e, int) { return gi_compose(o, a, e, 2, 1, true); }));
        CHECK(page_colors(final_high) != page_colors(off));
        CHECK(check_corners(final_high, off, sources, [](std::uint8_t o, float a, float e, int, bool)
                            { return gi_compose(o, a, e, 2, 1, false) / 255.f; }));
        CHECK(check_corners(final_low, off_low, sources_low, [](std::uint8_t o, float a, float e, int, bool)
                            { return gi_compose(o, a, e, 2, 1, false) / 255.f; }));
        CHECK(same_materials(final_low, off_low)); // DETAIL LOW draws no page.
        {
            WorldMesh high, low;
            CHECK(build(GiView::Final, .5f, .25f, high, low));
            CHECK(page_colors(high) == expect_pages([](std::uint8_t o, float a, float e, int)
                                                    { return gi_compose(o, a, e, .5f, .25f, true); }));
            CHECK(check_corners(low, off_low, sources_low, [](std::uint8_t o, float a, float e, int, bool)
                                { return gi_compose(o, a, e, .5f, .25f, false) / 255.f; }));
        }
        {
            WorldMesh high, low;
            CHECK(build(GiView::Original, 2, 1, high, low));
            CHECK(same_mesh(high, off) && same_mesh(low, off_low));
        }
        {
            WorldMesh high, low;
            CHECK(build(GiView::Direct, 2, 1, high, low));
            CHECK(page_colors(high) ==
                  expect_pages([](std::uint8_t o, float a, float, int) { return gi_compose(o, a, 0, 0, 0, true); }));
            CHECK(check_corners(low, off_low, sources_low, [](std::uint8_t o, float a, float, int, bool)
                                { return gi_compose(o, a, 0, 0, 0, false) / 255.f; }));
        }
        {
            WorldMesh high, low;
            CHECK(build(GiView::Indirect, 3, 1, high, low));
            CHECK(page_colors(high) ==
                  expect_pages([](std::uint8_t, float, float e, int) { return gi_compose(0, 0, e, 3, 0, true); }));
            CHECK(check_corners(low, off_low, sources_low, [](std::uint8_t, float, float e, int, bool)
                                { return gi_compose(0, 0, e, 3, 0, false) / 255.f; }));
        }
        {
            WorldMesh high, low;
            CHECK(build(GiView::Validity, 2, 1, high, low));
            const std::set<std::array<std::uint8_t, 3>> both{{0, 128, 0}, {128, 0, 0}};
            CHECK(page_colors(high) == both); // Block 1 is invalid, the others valid.
            CHECK(check_corners(low, off_low, sources_low, [](std::uint8_t, float, float, int c, bool valid)
                                { return (valid ? c == 1 : c == 0) ? 1.f : 0.f; }));
        }
        {
            WorldMesh high, low;
            CHECK(build(GiView::Lighting, 2, 1, high, low));
            CHECK(same_vertices(high, final_high) && same_vertices(low, final_low));
            std::size_t white = 0;
            for (std::size_t i = 1; i < high.materials.size(); ++i)
            {
                const auto &m = high.materials[i], &was = final_high.materials[i];
                if (is_page(m))
                {
                    CHECK(m.image.rgba == was.image.rgba);
                    continue;
                }
                if (m.image.rgba.empty())
                    continue;
                ++white;
                for (std::size_t q = 0; q + 3 < m.image.rgba.size(); q += 4)
                    CHECK(m.image.rgba[q] == 255 && m.image.rgba[q + 1] == 255 && m.image.rgba[q + 2] == 255 &&
                          m.image.rgba[q + 3] == was.image.rgba[q + 3]);
            }
            CHECK(white == 2 && high.gi_white_materials == 2 && low.gi_white_materials == 2); // grey, red.
        }
        // A polygon the file does not list keeps its stored light.
        {
            auto partial = maps;
            const auto first = r.blocks[4].first, size = std::uint32_t(r.blocks[4].id.width) * r.blocks[4].id.height;
            partial.blocks.erase(partial.blocks.begin() + 4);
            partial.texels.erase(partial.texels.begin() + first, partial.texels.begin() + first + size);
            partial.valid.erase(partial.valid.begin() + first, partial.valid.begin() + first + size);
            GiWorldLight some;
            CHECK(gi_world_light(b.world, section_file(partial, vertex), some, error));
            some.composition = {};
            WorldMesh high;
            CHECK(high.build(b.world, error, &resources, true, nullptr, &some));
            CHECK(high.gi_blocks == 4 && page_colors(high).count(expand(other)));
        }
        // Refusals leave the output untouched.
        {
            GiWorldLight refused;
            CHECK(!gi_world_light(b.world, section_file(maps, vertex, false), refused, error));
            CHECK(error.find("LMAP/VTX") != std::string::npos && refused.lightmaps.blocks.empty());
            auto wrong = maps;
            wrong.blocks[4].polygon = 999; // Still sorted; no such polygon.
            CHECK(!gi_world_light(b.world, section_file(wrong, vertex), refused, error));
            CHECK(error.find("does not match") != std::string::npos && refused.lightmaps.blocks.empty());
            auto wrong_ring = vertex;
            wrong_ring.sets[0].polygon = 0; // A lightmapped polygon without baked colours.
            CHECK(!gi_world_light(b.world, section_file(maps, wrong_ring), refused, error));
            CHECK(refused.vertex.sets.empty());
        }
        CHECK(valid_gi_composition({}) && valid_gi_composition({0, 0}) && valid_gi_composition({4, 4}));
        CHECK(!valid_gi_composition({4.5f, 1}) && !valid_gi_composition({2, -1}) &&
              !valid_gi_composition({std::numeric_limits<float>::quiet_NaN(), 1}));
        CHECK(light.block(0, 999) == -1 && light.set(1, 2) == -1 && light.block(0, 0) == 0 && light.set(0, 2) == 0);
        // Composition of an element outside the decoded data keeps the stored value.
        light.composition = {};
        const std::array<std::uint8_t, 3> stored{1, 2, 3};
        CHECK(light.compose(true, std::uint32_t(maps.texels.size()), stored) == stored);
        return true;
    }

    // Private: 01_SubwayA through the runtime lookup: no file, then a stale one, then a Ready zero bake
    // (byte-identical mesh at both detail levels), then a nonzero one (changed pages).
    bool runtime_retail(const fs::path& data, const fs::path& scratch)
    {
        CHECK(Testing::reset_scratch_dir(scratch));
        Config config;
        config.data_dir = data;
        Resources resources;
        std::string error;
        CHECK(resources.mount_game(config, error));
        const std::string path = "worlds/01_subwaya.dat";
        std::vector<std::uint8_t> world_bytes;
        World world;
        CHECK(resources.read(path, world_bytes, error) && read_world(world_bytes, world, error));
        const std::vector<GiRoot> roots{{"--gi-dir", path_text(scratch)}};
        std::optional<GiWorldLight> light;
        GiRuntimeStatus status;
        load_gi_world_light(resources, world, "", roots, light, status);
        CHECK(status.state == GiRuntimeState::NoWorld && !light && status.loads == 1);
        load_gi_world_light(resources, world, path, roots, light, status);
        CHECK(status.state == GiRuntimeState::Missing && !light && status.loads == 2);
        CHECK(status.detail == "No baked file in any search root" && status.expectation_ms > 0);
        CHECK(gi_status_line(status).find("not applied") != std::string::npos);

        GiExpectation expected;
        CHECK(gi_expectation(resources, path, expected, error));
        Prepared p;
        CHECK(prepare(world, resources, p, 0));
        GiIndirect indirect;
        indirect.energy.assign(p.receivers.elements() * 3, 0.f);
        indirect.valid.assign(p.receivers.elements(), 1);
        GiLightmaps maps;
        GiVertexLight vertex;
        gi_lightmap_sections(p.receivers, indirect, maps, vertex);
        auto file = section_file(maps, vertex);
        file.key.world = expected.world;
        file.key.albedo = expected.albedo;
        file.key.world_path = path;
        file.settings = "quality=test";
        file.key.settings = sha256(std::span(reinterpret_cast<const std::uint8_t*>(file.settings.data()),
                                             file.settings.size()));
        std::string name;
        CHECK(gi_file_name(path, expected.world, name));
        fs::create_directories(scratch / "worlds");
        const auto target = scratch / path_from_utf8(name);
        file.key.albedo[0] ^= 1;
        CHECK(save_gi_file(target, file, error));
        load_gi_world_light(resources, world, path, roots, light, status);
        CHECK(status.state == GiRuntimeState::Missing && !light &&
              status.detail.find("stale (albedo)") != std::string::npos);
        file.key.albedo[0] ^= 1;
        CHECK(save_gi_file(target, file, error));
        load_gi_world_light(resources, world, path, roots, light, status);
        CHECK(status.state == GiRuntimeState::Applied && light && status.settings == "quality=test");
        CHECK(status.lookup.chosen == 0 && !status.lookup.file && status.file.find(name) != std::string::npos);
        CHECK(status.blocks == maps.blocks.size() && status.texels == maps.texels.size() &&
              status.sets == vertex.sets.size() && status.corners == vertex.corners.size());
        CHECK(gi_status_line(status).rfind("GI applied: " + path, 0) == 0);
        for (const bool lightmaps : {true, false})
        {
            WorldMesh off, on;
            CHECK(off.build(world, error, &resources, lightmaps));
            CHECK(on.build(world, error, &resources, lightmaps, nullptr, &*light));
            CHECK(same_mesh(on, off));
            CHECK(lightmaps ? on.gi_blocks == maps.blocks.size() && on.gi_texels == maps.texels.size()
                            : on.gi_blocks == 0 && on.gi_sets == vertex.sets.size());
            std::cout << "GI_RUNTIME detail=" << (lightmaps ? "high" : "low") << " blocks=" << on.gi_blocks
                      << " texels=" << on.gi_texels << " sets=" << on.gi_sets << " corners=" << on.gi_corners << '\n';
        }
        // Lot G9: zero probes over a coarse lattice: models and the Light probes source leave every
        // page and corner unchanged.
        {
            GiLightLattice lattice;
            lattice.spacing = 256;
            lattice.origin = world.min;
            for (int a = 0; a < 3; ++a)
            {
                const float extent = a == 0 ? world.max.x - world.min.x : a == 1 ? world.max.y - world.min.y
                                                                                 : world.max.z - world.min.z;
                lattice.dims[a] = std::uint32_t(std::ceil(extent / 256)) + 1;
            }
            const auto nodes = lattice.dims[0] * lattice.dims[1] * lattice.dims[2];
            for (std::uint32_t node = 0; node < nodes; node += 3)
                lattice.probes.push_back({node, lattice.node_position(node)});
            GiProbes probes;
            probes.sh.assign(lattice.probes.size() * 27, 0.f);
            probes.valid.assign(lattice.probes.size(), 1);
            auto with_probes = probe_file(maps, vertex, lattice, probes);
            with_probes.key = file.key;
            with_probes.settings = file.settings;
            CHECK(save_gi_file(target, with_probes, error));
            load_gi_world_light(resources, world, path, roots, light, status);
            CHECK(status.state == GiRuntimeState::Applied && light && light->has_probes);
            CHECK(status.probes == lattice.probes.size() && status.valid_probes == status.probes &&
                  status.probe_spacing == 256 && gi_status_line(status).find(" light probes") != std::string::npos);
            light->composition.source = GiWorldSource::Probes;
            CHECK(light->prepare_probe_source(world, error));
            WorldMesh off, on;
            CHECK(off.build(world, error, &resources) && on.build(world, error, &resources, true, nullptr, &*light));
            CHECK(same_mesh(on, off));
            std::cout << "GI_RUNTIME probes=" << status.probes << " probe_source_ms=" << light->probe_source_ms
                      << " texels=" << light->probe_texels.size() << " corners=" << light->probe_corners.size()
                      << '\n';
            CHECK(save_gi_file(target, file, error));
        }
        std::fill(indirect.energy.begin(), indirect.energy.end(), .05f);
        gi_lightmap_sections(p.receivers, indirect, maps, vertex);
        auto lit = section_file(maps, vertex);
        lit.key = file.key;
        lit.settings = file.settings;
        CHECK(save_gi_file(target, lit, error));
        load_gi_world_light(resources, world, path, roots, light, status);
        CHECK(status.state == GiRuntimeState::Applied && light && status.loads == 6);
        WorldMesh off, on;
        CHECK(off.build(world, error, &resources) && on.build(world, error, &resources, true, nullptr, &*light));
        CHECK(same_vertices(on, off) == vertex.sets.empty() && !same_materials(on, off));
        std::cout << "GI_RUNTIME keys_ms=" << status.expectation_ms << " lookup_ms=" << status.lookup_ms
                  << " sections_ms=" << status.decode_ms << '\n';
        return Testing::remove_scratch_dir(scratch);
    }

    // ---- Lot G9: light probes (gi_probes.*) ----

    // binary16: rounding to nearest even, subnormals, saturation, NaN, and every finite half round trips.
    bool probe_half()
    {
        CHECK(gi_half(0.f) == 0 && gi_half(-0.f) == 0x8000 && gi_half(1.f) == 0x3C00 && gi_half(-2.f) == 0xC000);
        CHECK(gi_half(65504.f) == 0x7BFF && gi_half(65520.f) == 0x7BFF && gi_half(1e30f) == 0x7BFF);
        CHECK(gi_half(std::numeric_limits<float>::infinity()) == 0x7BFF);
        CHECK(gi_half(-std::numeric_limits<float>::infinity()) == 0xFBFF);
        CHECK(gi_half(std::numeric_limits<float>::quiet_NaN()) == 0);
        CHECK(gi_half(std::ldexp(1.f, -14)) == 0x0400 && gi_half(std::ldexp(1.f, -24)) == 1);
        CHECK(gi_half(std::ldexp(1.f, -25)) == 0 && gi_half(std::ldexp(3.f, -25)) == 2); // Ties to even.
        CHECK(gi_half(1 + std::ldexp(1.f, -11)) == 0x3C00 && gi_half(1 + std::ldexp(3.f, -11)) == 0x3C02);
        CHECK(gi_half(std::ldexp(4095.f, -12)) == 0x3C00); // A tie carried into the next binade.
        CHECK(gi_half_decode(0x7C00) == 0 && gi_half_decode(0x7E00) == 0 && gi_half_decode(0x0001) == std::ldexp(1.f, -24));
        for (std::uint32_t h = 0; h < 0x10000; ++h)
        {
            if ((h >> 10 & 31) == 31)
                continue;
            const float value = gi_half_decode(std::uint16_t(h));
            CHECK(gi_half(value) == h);
            CHECK(std::isfinite(value) && std::abs(value) <= 65504);
        }
        return true;
    }

    std::vector<std::uint8_t> probe_payload(const GiProbes& probes)
    {
        std::vector<std::uint8_t> out;
        std::string error;
        write_gi_probes(probes, out, error);
        return out;
    }
    // The PROB section: layout, strict reader and lattice check.
    bool probe_sections()
    {
        std::string error;
        GiProbes probes;
        for (std::size_t i = 0; i < 9 * 27; ++i)
            probes.sh.push_back(float(int(i % 61) - 30) / 8); // Exact halves.
        probes.valid = {1, 0, 1, 1, 0, 0, 1, 0, 1};
        auto bytes = probe_payload(probes);
        CHECK(bytes.size() == 12 + 9 * 27 * 2 + 2 && bytes[0] == 1 && bytes[4] == 9 && bytes[8] == 0);
        CHECK(bytes[12 + 9 * 54] == 0x4D && bytes[12 + 9 * 54 + 1] == 0x01);
        GiProbes back;
        CHECK(read_gi_probes(bytes, back, error));
        CHECK(back.sh == probes.sh && back.valid == probes.valid && back.size() == 9);
        GiProbes empty;
        CHECK(read_gi_probes(probe_payload(empty), back, error) && back.size() == 0 && back.sh.empty());
        auto refused = [&](std::vector<std::uint8_t> b, const char* expected)
        {
            GiProbes untouched = probes;
            std::string message;
            return !read_gi_probes(b, untouched, message) && message.find(expected) != std::string::npos &&
                   untouched.sh == probes.sh;
        };
        auto edit = [&](std::size_t at, std::uint8_t value)
        {
            auto b = bytes;
            b[at] = value;
            return b;
        };
        CHECK(refused({bytes.begin(), bytes.begin() + 11}, "truncated"));
        CHECK(refused(edit(0, 2), "format"));
        CHECK(refused(edit(6, 4), "too many"));
        CHECK(refused(edit(8, 1), "reserved"));
        CHECK(refused(edit(4, 8), "size"));
        CHECK(refused({bytes.begin(), bytes.end() - 1}, "size"));
        CHECK(refused(edit(13, 0x7C), "finite"));
        CHECK(refused(edit(bytes.size() - 1, 0x03), "padding"));
        GiProbes mismatched = probes;
        mismatched.sh.pop_back();
        std::vector<std::uint8_t> out;
        CHECK(!write_gi_probes(mismatched, out, error));
        GiLightLattice lattice;
        lattice.probes.resize(8);
        CHECK(!check_gi_probes(lattice, probes, error));
        lattice.probes.resize(9);
        CHECK(check_gi_probes(lattice, probes, error));
        return true;
    }

    // The coefficients of E(n) = color max(0, n.l) (a lone directional light): A_l Y_lm(l).
    std::array<float, 27> directional_sh(DVector l, DVector color)
    {
        double basis[9];
        gi_sh_basis(l, basis);
        constexpr double a[9]{pi, 2 * pi / 3, 2 * pi / 3, 2 * pi / 3, pi / 4, pi / 4, pi / 4, pi / 4, pi / 4};
        std::array<float, 27> sh{};
        const double c[3]{color.x, color.y, color.z};
        for (int k = 0; k < 9; ++k)
            for (int ch = 0; ch < 3; ++ch)
                sh[k * 3 + ch] = float(a[k] * basis[k] * c[ch]);
        return sh;
    }
    DVector unit_vector(DVector v)
    {
        return vector_scale(v, 1 / std::sqrt(vector_dot(v, v)));
    }
    // The SH basis is orthonormal; Fibonacci directions and probe rotations are unit, reproducible and
    // rigid; the irradiance of known coefficient sets.
    bool probe_basis()
    {
        const std::uint32_t n = 40000;
        double gram[9][9]{};
        for (std::uint32_t i = 0; i < n; ++i)
        {
            const auto d = gi_fibonacci_direction(i, n);
            CHECK(near(vector_dot(d, d), 1, 1e-6) && d.z == float(1 - (2.0 * i + 1) / n));
            double y[9];
            gi_sh_basis(d, y);
            for (int a = 0; a < 9; ++a)
                for (int b = 0; b < 9; ++b)
                    gram[a][b] += y[a] * y[b] * 4 * pi / n;
        }
        for (int a = 0; a < 9; ++a)
            for (int b = 0; b < 9; ++b)
                CHECK(near(gram[a][b], a == b, 2e-4));
        const auto q = gi_probe_rotation(0x5eed, 7);
        CHECK(q == gi_probe_rotation(0x5eed, 7) && q != gi_probe_rotation(0x5eed, 8) &&
              q != gi_probe_rotation(0x5eee, 7));
        CHECK(near(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3], 1, 1e-12));
        for (std::uint32_t i = 0; i < 64; ++i)
            for (std::uint32_t j = i + 1; j < 64; j += 7)
            {
                const auto a = gi_probe_direction(q, i, 64), b = gi_probe_direction(q, j, 64);
                CHECK(near(vector_dot(a, a), 1, 1e-6));
                CHECK(near(vector_dot(a, b), vector_dot(gi_fibonacci_direction(i, 64), gi_fibonacci_direction(j, 64)),
                           1e-6));
            }
        // Uniform E = 1: only the first coefficient (1 / Y00).
        std::array<float, 27> uniform{};
        uniform[0] = uniform[1] = uniform[2] = float(1 / 0.28209479177387814);
        for (const DVector normal : {DVector{0, 0, 1}, DVector{1, 0, 0}, unit_vector({-1, 2, -3})})
            CHECK(near(gi_sh_irradiance(uniform, normal).y, 1, 1e-6));
        // A directional light: the L2 clamped cosine is 1/4 + 1/2 + 5/16 facing it and 1/16 away.
        const auto l = unit_vector({1, 2, 3});
        const auto sh = directional_sh(l, {1, .5f, .25f});
        CHECK(near(gi_sh_irradiance(sh, l).x, 1.0625, 1e-6) && near(gi_sh_irradiance(sh, l).z, 1.0625 / 4, 1e-6));
        CHECK(near(gi_sh_irradiance(sh, vector_scale(l, -1)).x, .0625, 1e-6));
        return true;
    }

    std::vector<GiLightProbe> probes_at(std::initializer_list<DVector> points)
    {
        std::vector<GiLightProbe> out;
        for (const auto& p : points)
            out.push_back({std::uint32_t(out.size()), p});
        return out;
    }
    bool capture(const Prepared& p, const GiSky& sky, std::span<const float> light, std::span<const GiLightProbe> lattice,
                 std::uint32_t rays, GiProbes& out, GiProbeReport& report, unsigned jobs = 2,
                 std::uint64_t seed = 0x5eed)
    {
        std::string error;
        if (!bake_gi_probes(p.scene, sky, p.receivers, light, lattice, rays, seed, jobs, nullptr, {}, out, report,
                            error))
        {
            std::cerr << "probes: " << error << '\n';
            return false;
        }
        return out.size() == lattice.size() && out.sh.size() == lattice.size() * 27;
    }
    // A probe in a closed, uniformly lit box sees uniform radiance: E = albedo x light everywhere.
    // A probe inside outward-facing walls sees only back faces: invalid and black. Bitwise identical
    // for 1, 2 and 7 jobs; another seed turns the rays.
    bool probe_capture(const fs::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        const double albedo = 153 / 255.f;
        GiSky sky;
        sky.fill({1, 1, 1}, 1); // Never seen in the closed box.
        {
            WorldBuilder b;
            b.box({5, 5, 5}, {95, 95, 95},
                  [&](int, Face& face)
                  {
                      face.kind = Kind::Vertex;
                      face.color = {51, 51, 51};
                  });
            Prepared p;
            CHECK(prepare(b.world, resources, p));
            GiIndirect indirect;
            GiLightmapReport lightmap;
            CHECK(bake(p, sky, 2, 64, indirect, lightmap));
            std::vector<float> light;
            gi_light_state(p.receivers, indirect, light);
            CHECK(light.size() == p.receivers.elements() * 3);
            for (const auto v : light)
                CHECK(v == light[0] && near(v, .2 + albedo * .2 * 1.6, 1e-6));
            const double radiance = albedo * light[0];
            const auto lattice = probes_at({{50, 50, 50}, {20, 30, 70}, {80, 85, 15}});
            GiProbes probes;
            GiProbeReport report;
            CHECK(capture(p, sky, light, lattice, 64, probes, report));
            CHECK(report.probes == 3 && report.valid == 3 && report.rays == 192 && report.back == 0);
            CHECK(report.sky == 0 && report.untextured == 0 && report.back_fraction[0] == 3);
            for (std::size_t i = 0; i < 3; ++i)
            {
                CHECK(probes.valid[i]);
                const std::span<const float, 27> sh(probes.sh.data() + i * 27, 27);
                for (int c = 0; c < 3; ++c)
                    CHECK(near(sh[c], radiance / 0.28209479177387814, 1e-5));
                for (const DVector n : {DVector{0, 0, 1}, DVector{0, 0, -1}, DVector{1, 0, 0}, DVector{0, -1, 0},
                                        unit_vector({1, 1, 1})})
                    CHECK(near(gi_sh_irradiance(sh, n).x, radiance, .02 * radiance));
            }
        }
        {
            WorldBuilder b;
            b.box({5, 5, 5}, {95, 95, 95},
                  [](int, Face& face)
                  {
                      std::reverse(face.ring.begin(), face.ring.end());
                      face.normal = vector_scale(face.normal, -1);
                  });
            Prepared p;
            CHECK(prepare(b.world, resources, p));
            GiIndirect indirect;
            GiLightmapReport lightmap;
            CHECK(bake(p, sky, 1, 16, indirect, lightmap));
            std::vector<float> light;
            gi_light_state(p.receivers, indirect, light);
            const auto lattice = probes_at({{50, 50, 50}, {50, 50, 400}});
            GiProbes probes;
            GiProbeReport report;
            CHECK(capture(p, sky, light, lattice, 64, probes, report));
            CHECK(!probes.valid[0] && probes.valid[1] && report.valid == 1 && report.back_fraction[9] == 1);
            for (std::size_t k = 0; k < 27; ++k)
                CHECK(probes.sh[k] == 0);
            // Outside, the box hides a small part of the sky: the rest arrives at radiance 1.
            CHECK(report.back >= 64 && report.back < 64 + 32 && report.sky == 128 - report.back);
        }
        WorldBuilder b;
        two_rooms(b);
        Prepared p;
        CHECK(prepare(b.world, resources, p));
        GiSky colored;
        colored.fill({.3f, .4f, .5f}, 1.5f);
        GiIndirect indirect;
        GiLightmapReport lightmap;
        CHECK(bake(p, colored, 2, 32, indirect, lightmap));
        std::vector<float> light;
        gi_light_state(p.receivers, indirect, light);
        const auto lattice = probes_at({{50, 50, 50}, {140, 50, 50}, {94, 50, 50}, {30, 80, 20}, {170, 20, 90},
                                        {100, 50, 50}, {60, 60, 60}});
        GiProbes one;
        GiProbeReport first;
        CHECK(capture(p, colored, light, lattice, 48, one, first, 1));
        CHECK(first.valid == lattice.size());
        for (const unsigned jobs : {2u, 7u})
        {
            GiProbes other;
            GiProbeReport report;
            CHECK(capture(p, colored, light, lattice, 48, other, report, jobs));
            CHECK(std::memcmp(other.sh.data(), one.sh.data(), one.sh.size() * sizeof(float)) == 0);
            CHECK(other.valid == one.valid && report.rays == first.rays && report.back == first.back);
            CHECK(report.back_fraction == first.back_fraction && report.sky == first.sky);
        }
        GiProbes reseeded;
        GiProbeReport report;
        CHECK(capture(p, colored, light, lattice, 48, reseeded, report, 2, 0x5eee));
        CHECK(std::memcmp(reseeded.sh.data(), one.sh.data(), one.sh.size() * sizeof(float)) != 0);
        // The bright ceiling of room B: its probe is brighter than room A's.
        const auto e_a = gi_sh_irradiance(std::span<const float, 27>(one.sh.data(), 27), {0, 0, 1});
        const auto e_b = gi_sh_irradiance(std::span<const float, 27>(one.sh.data() + 27, 27), {0, 0, 1});
        CHECK(e_b.x > 2 * e_a.x);
        // Cancelled: nothing out.
        std::atomic<bool> cancel{true};
        GiProbes cancelled;
        std::string error;
        CHECK(!bake_gi_probes(p.scene, colored, p.receivers, light, lattice, 48, 1, 2, &cancel, {}, cancelled, report,
                              error) &&
              report.cancelled && cancelled.size() == 0);
        CHECK(!bake_gi_probes(p.scene, colored, p.receivers, std::span<const float>(light).first(3), lattice, 48, 1, 2,
                              nullptr, {}, cancelled, report, error));
        return true;
    }

    // A 3 x 2 x 2 lattice (spacing 10 at the origin): probe i at node i holds E = i + 1, node 5 has no
    // probe and node 2 is invalid.
    GiProbeField test_field(bool all_invalid = false)
    {
        GiLightLattice lattice;
        lattice.spacing = 10;
        lattice.dims = {3, 2, 2};
        GiProbes probes;
        for (std::uint32_t node = 0; node < 12; ++node)
        {
            if (node == 5)
                continue;
            lattice.probes.push_back({node, lattice.node_position(node)});
            for (int k = 0; k < 27; ++k)
                probes.sh.push_back(k < 3 ? float((node + 1) / 0.28209479177387814) : 0.f);
            probes.valid.push_back(node != 2 && !all_invalid);
        }
        GiProbeField field;
        std::string error;
        field.build(std::move(lattice), std::move(probes), error);
        return field;
    }
    double field_e(const GiProbeSample& s)
    {
        return gi_sh_irradiance(s.sh, {0, 0, 1}).x;
    }
    // Validity-weighted trilinear: weights, missing/invalid corners renormalized, clamping and refusals.
    bool probe_field()
    {
        std::string error;
        auto field = test_field();
        CHECK(field.probes().size() == 11 && field.valid() == 10 && !field.empty());
        CHECK(field.probe_at(0) == 0 && field.probe_at(4) == 4 && field.probe_at(5) == -1 && field.probe_at(6) == 5 &&
              field.probe_at(11) == 10 && field.probe_at(12) == -1);
        GiProbeSample s;
        // Cell (0,0,0) at fractions (.25, .5, .75): nodes 0 1 3 4 6 7 9 10, all valid.
        CHECK(field.sample({2.5f, 5, 7.5f}, s) && s.valid && s.cell == (std::array<std::uint32_t, 3>{0, 0, 0}));
        double expected = 0, total = 0;
        const std::uint32_t nodes[8]{0, 1, 3, 4, 6, 7, 9, 10};
        for (int c = 0; c < 8; ++c)
        {
            const double w = (c & 1 ? .25 : .75) * .5 * (c & 4 ? .75 : .25);
            expected += w * (nodes[c] + 1);
            total += s.weights[c];
            CHECK(near(s.weights[c], w, 1e-7) && s.probes[c] == field.probe_at(nodes[c]));
        }
        CHECK(near(total, 1, 1e-6) && near(field_e(s), expected, 1e-5));
        // Cell (1,0,0): node 2 invalid and node 5 missing drop out; the rest renormalize.
        CHECK(field.sample({15, 5, 5}, s) && s.cell == (std::array<std::uint32_t, 3>{1, 0, 0}));
        CHECK(s.probes[1] == 2 && s.weights[1] == 0 && s.probes[3] == -1 && s.weights[3] == 0);
        expected = 0;
        const std::uint32_t corners[8]{1, 2, 4, 5, 7, 8, 10, 11};
        for (int c = 0; c < 8; ++c)
            if (corners[c] != 2 && corners[c] != 5)
                expected += (corners[c] + 1) / 6.0;
        CHECK(near(field_e(s), expected, 1e-5));
        // Clamped to the lattice: far below is node 0 alone, far above node 11, NaN as the low edge.
        CHECK(field.sample({-1000, -1000, -1000}, s) && near(field_e(s), 1, 1e-6) && s.weights[0] == 1);
        CHECK(field.sample({1000, 1000, 1000}, s) && near(field_e(s), 12, 1e-5) && s.weights[7] == 1);
        CHECK(s.cell == (std::array<std::uint32_t, 3>{1, 0, 0}));
        const float nan = std::numeric_limits<float>::quiet_NaN();
        CHECK(field.sample({nan, nan, nan}, s) && near(field_e(s), 1, 1e-6));
        // Exactly on invalid node 2 with the missing node beside it: no valid weight.
        CHECK(!field.sample({20, 0, 0}, s) && !s.valid && s.probes[1] == 2);
        std::array<std::uint32_t, 3> cell{};
        field.cell({25, 15, 15}, cell);
        CHECK(cell == (std::array<std::uint32_t, 3>{1, 0, 0}));
        CHECK(!test_field(true).sample({5, 5, 5}, s) && !s.valid);
        CHECK(!GiProbeField().sample({0, 0, 0}, s) && GiProbeField().empty());
        // Refusals: counts, order, range.
        GiLightLattice lattice;
        lattice.spacing = 10;
        lattice.dims = {2, 1, 1};
        GiProbes probes;
        probes.sh.assign(54, 0.f);
        probes.valid = {1, 1};
        lattice.probes = {{1, {}}, {0, {}}};
        CHECK(!GiProbeField().build(lattice, probes, error));
        lattice.probes = {{0, {}}, {2, {}}};
        CHECK(!GiProbeField().build(lattice, probes, error));
        lattice.probes = {{0, {}}};
        CHECK(!GiProbeField().build(lattice, probes, error));
        lattice.probes = {{0, {}}, {1, {}}};
        GiProbeField one_axis;
        CHECK(one_axis.build(lattice, probes, error) && one_axis.sample({4, 50, -50}, s) && s.weights[0] == .6f);
        return true;
    }

    // Decision 3's model split: the zero identity, a lone directional light recovered, ambient plus a
    // light, and the floor.
    bool probe_light()
    {
        const std::array<float, 27> zero{};
        const DVector a{.2f, .3f, .4f};
        auto light = gi_probe_model_light(zero, 1, 1, a);
        CHECK(light.ambient.x == a.x && light.ambient.y == a.y && light.ambient.z == a.z);
        CHECK(light.color.x == 0 && light.color.y == 0 && light.color.z == 0);
        light = gi_probe_model_light(zero, 1, 0, a);
        CHECK(light.ambient.x == 0 && light.color.x == 0);
        const auto l = unit_vector({1, 2, 3});
        const DVector c{.5f, .25f, .125f};
        for (const float scale : {1.f, 2.f})
        {
            light = gi_probe_model_light(directional_sh(l, c), scale, 0, a);
            CHECK(near(light.direction.x, l.x, 1e-6) && near(light.direction.y, l.y, 1e-6) &&
                  near(light.direction.z, l.z, 1e-6));
            CHECK(near(light.color.x, scale * c.x, 1e-6) && near(light.color.y, scale * c.y, 1e-6) &&
                  near(light.color.z, scale * c.z, 1e-6));
            CHECK(near(light.ambient.x, 0, 1e-6) && near(light.ambient.z, 0, 1e-6));
        }
        // Plus a uniform U: the ambient is U and the light keeps its colour.
        auto sh = directional_sh(l, c);
        const DVector u{.1f, .05f, .2f};
        sh[0] += float(u.x / 0.28209479177387814), sh[1] += float(u.y / 0.28209479177387814),
            sh[2] += float(u.z / 0.28209479177387814);
        light = gi_probe_model_light(sh, 1, 0, a);
        CHECK(near(light.ambient.x, u.x, 1e-6) && near(light.ambient.y, u.y, 1e-6) && near(light.ambient.z, u.z, 1e-6));
        CHECK(near(light.color.x, c.x, 1e-6) && near(light.color.z, c.z, 1e-6));
        // The floor f A: facing the light a + c and away a, each at least f A.
        light = gi_probe_model_light(directional_sh(l, c), 1, 1, {.1f, .1f, .1f});
        CHECK(near(light.ambient.x, .1, 1e-6) && near(light.color.x, .4, 1e-6) && near(light.color.y, .15, 1e-6));
        CHECK(near(light.ambient.z, .1, 1e-6) && near(light.color.z, .025, 1e-6));
        light = gi_probe_model_light(directional_sh(l, c), 1, 1, {.9f, .9f, .9f});
        CHECK(light.ambient.x == .9f && light.color.x == 0 && light.color.z == 0);
        // Negative or non-finite coefficients never make light.
        std::array<float, 27> negative{};
        negative[0] = negative[1] = negative[2] = -1;
        light = gi_probe_model_light(negative, 1, 0, a);
        CHECK(light.ambient.x == 0 && light.color.x == 0);
        negative[9] = std::numeric_limits<float>::infinity();
        light = gi_probe_model_light(negative, 1, 0, a);
        CHECK(std::isfinite(light.ambient.x) && std::isfinite(light.color.x) && std::isfinite(light.direction.x));
        return true;
    }

    GiFile probe_file(const GiLightmaps& maps, const GiVertexLight& vertex, const GiLightLattice& lattice,
                      const GiProbes& probes)
    {
        auto file = section_file(maps, vertex);
        std::string error;
        GiSection plac{gi_format::placement, {}}, prob{gi_format::probes, {}};
        encode_gi_placement_section(lattice, {}, plac.payload, error);
        write_gi_probes(probes, prob.payload, error);
        file.sections.insert(file.sections.begin(), std::move(plac)); // PLAC precedes LMAP.
        file.sections.push_back(std::move(prob));
        return file;
    }
    // The Light probes world source: zero or invalid probes leave the mesh byte-identical; uniform probes
    // give every texel and corner their E; a file without PLAC, or a PROB of the wrong size, fails.
    bool probe_source(const fs::path& root)
    {
        Resources resources;
        CHECK(mount(root, resources));
        WorldBuilder b;
        b.world.info = "AmbientLight 40 20 10";
        b.box({5, 5, 5}, {95, 95, 95},
              [&](int s, Face& face)
              {
                  face.texel = 20 << 11 | 30 << 5 | 12;
                  face.kind = s == 2 ? Kind::Vertex : Kind::Lightmap;
                  face.color = {90, 80, 70};
              });
        Prepared p;
        CHECK(prepare(b.world, resources, p));
        const auto& r = p.receivers;
        std::string error;
        GiIndirect zero;
        zero.energy.assign(r.elements() * 3, 0.f);
        zero.valid.assign(r.elements(), 1);
        GiLightmaps maps;
        GiVertexLight vertex;
        gi_lightmap_sections(r, zero, maps, vertex);
        GiLightLattice lattice;
        lattice.spacing = 64;
        lattice.dims = {3, 3, 3};
        for (std::uint32_t node = 0; node < 27; ++node)
            lattice.probes.push_back({node, lattice.node_position(node)});
        GiProbes probes;
        probes.sh.assign(27 * 27, 0.f);
        probes.valid.assign(27, 1);
        WorldMesh off;
        CHECK(off.build(b.world, error, &resources));
        GiWorldLight light;
        CHECK(gi_world_light(b.world, probe_file(maps, vertex, lattice, probes), light, error));
        CHECK(light.has_probes && light.probes.probes().size() == 27 && !light.probe_source_ready);
        light.composition.source = GiWorldSource::Probes;
        WorldMesh on;
        CHECK(on.build(b.world, error, &resources, true, nullptr, &light) && same_mesh(on, off)); // Not prepared yet.
        CHECK(light.prepare_probe_source(b.world, error) && light.probe_source_ready);
        CHECK(light.probe_texels.size() == maps.texels.size() && light.probe_corners.size() == vertex.corners.size());
        CHECK(on.build(b.world, error, &resources, true, nullptr, &light) && same_mesh(on, off));
        // All invalid: E = 0 and red validity.
        GiProbes invalid = probes;
        invalid.valid.assign(27, 0);
        GiWorldLight none;
        CHECK(gi_world_light(b.world, probe_file(maps, vertex, lattice, invalid), none, error));
        none.composition.source = GiWorldSource::Probes;
        CHECK(none.prepare_probe_source(b.world, error));
        CHECK(std::all_of(none.probe_texel_valid.begin(), none.probe_texel_valid.end(), [](auto v) { return !v; }));
        CHECK(on.build(b.world, error, &resources, true, nullptr, &none) && same_mesh(on, off));
        // Uniform E = .25: every element gets it, and the mesh changes.
        GiProbes uniform = probes;
        for (std::size_t i = 0; i < 27; ++i)
            for (int c = 0; c < 3; ++c)
                uniform.sh[i * 27 + c] = float(.25 / 0.28209479177387814);
        GiWorldLight lit;
        CHECK(gi_world_light(b.world, probe_file(maps, vertex, lattice, uniform), lit, error));
        CHECK(lit.prepare_probe_source(b.world, error));
        const auto quarter = gi_rgb9e5({.25f, .25f, .25f});
        for (const auto v : lit.probe_texels)
            CHECK(std::abs(gi_rgb9e5_decode(v).x - .25f) <= .25f * 1e-3f);
        for (const auto v : lit.probe_corners)
            CHECK(std::abs(gi_rgb9e5_decode(v).y - .25f) <= .25f * 1e-3f);
        CHECK(lit.probe_texels.front() == quarter || std::abs(gi_rgb9e5_decode(lit.probe_texels.front()).x - .25f) < 1e-3f);
        CHECK(on.build(b.world, error, &resources, true, nullptr, &lit) && same_mesh(on, off)); // Texels: E = 0.
        lit.composition.source = GiWorldSource::Probes;
        CHECK(on.build(b.world, error, &resources, true, nullptr, &lit) && !same_materials(on, off) &&
              !same_vertices(on, off));
        CHECK(lit.compose(true, 0, {10, 10, 10}) == (std::array<std::uint8_t, 3>{gi_compose(10, 40, .25f, 2, 1, true),
                                                                                 gi_compose(10, 20, .25f, 2, 1, true),
                                                                                 gi_compose(10, 10, .25f, 2, 1, true)}));
        // Refusals: PROB without PLAC, a PROB that does not match its lattice.
        auto file = section_file(maps, vertex);
        GiSection prob{gi_format::probes, probe_payload(probes)};
        file.sections.push_back(prob);
        GiWorldLight refused;
        CHECK(!gi_world_light(b.world, file, refused, error) && error.find("PLAC") != std::string::npos);
        GiProbes short_probes = probes;
        short_probes.valid.pop_back();
        short_probes.sh.resize(26 * 27);
        CHECK(!gi_world_light(b.world, probe_file(maps, vertex, lattice, short_probes), refused, error));
        GiWorldLight plain;
        CHECK(gi_world_light(b.world, section_file(maps, vertex), plain, error) && !plain.has_probes);
        CHECK(!plain.prepare_probe_source(b.world, error));
        return true;
    }

    // Cost: 1000 samples of a 64^3 lattice stay far below a frame.
    bool probe_cost()
    {
        GiLightLattice lattice;
        lattice.spacing = 64;
        lattice.dims = {64, 64, 64};
        GiProbes probes;
        std::mt19937 random(7);
        for (std::uint32_t node = 0; node < 64 * 64 * 64; node += 2)
        {
            lattice.probes.push_back({node, lattice.node_position(node)});
            for (int k = 0; k < 27; ++k)
                probes.sh.push_back(float(random() % 1000) / 1000);
            probes.valid.push_back(1);
        }
        GiProbeField field;
        std::string error;
        CHECK(field.build(std::move(lattice), std::move(probes), error));
        GiProbeSample s;
        std::size_t valid = 0;
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; ++i)
        {
            const DVector point{float(random() % 4000) + .5f, float(random() % 4000) + .5f, float(random() % 4000) + .5f};
            valid += field.sample(point, s);
            const auto split = gi_probe_model_light(s.sh, 1, 1, {.1f, .1f, .1f});
            valid += split.ambient.x >= 0 ? 0 : 1;
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::cout << "GI_PROBE_COST samples=1000 ms=" << ms << '\n';
        CHECK(valid == 1000 && ms < 2);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view name(argv[1]);
    bool ok = false;
    if (name == "identity")
        ok = identity();
    else if (name == "rgb9e5")
        ok = rgb9e5();
    else if (name == "sections")
        ok = sections();
    else if (name == "directions")
        ok = directions();
    else if (name == "probe-half")
        ok = probe_half();
    else if (name == "probe-sections")
        ok = probe_sections();
    else if (name == "probe-basis")
        ok = probe_basis();
    else if (name == "probe-field")
        ok = probe_field();
    else if (name == "probe-light")
        ok = probe_light();
    else if (name == "probe-cost")
        ok = probe_cost();
    else if (name == "retail" && argc == 5)
        return retail(path_from_utf8(argv[2]), path_from_utf8(argv[3]), path_from_utf8(argv[4])) ? 0 : 1;
    else if (name == "runtime-retail" && argc == 4)
        return runtime_retail(path_from_utf8(argv[2]), path_from_utf8(argv[3])) ? 0 : 1;
    else if (argc == 3)
    {
        const auto root = path_from_utf8(argv[2]);
        static const std::map<std::string_view, bool (*)(const fs::path&)> cases{
            {"placement", placement}, {"furnace", furnace},         {"box", box},   {"planes", planes},
            {"leak", leak},           {"dilation", dilation},       {"back", back}, {"determinism", determinism},
            {"sky", sky},             {"preview", preview},         {"runtime", runtime_override},
            {"probe-capture", probe_capture}, {"probe-source", probe_source}};
        const auto found = cases.find(name);
        if (found == cases.end())
            return 2;
        ok = found->second(root) && Testing::remove_scratch_dir(root);
    }
    else
        return 2;
    return ok ? 0 : 1;
}
