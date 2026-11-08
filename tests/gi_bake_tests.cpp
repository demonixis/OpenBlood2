// GI lot G5: the bake scene, the BVH tracer, translucent layers, the job pool, settings, survey
// determinism and the bake driver (gi_contracts.md). The private case bakes 01_SubwayA, compares hits
// with WorldShade and prints the throughput.
#include "engine/gi_bake.h"
#include "engine/gi_jobs.h"
#include "engine/config.h"
#include "engine/resources.h"
#include "engine/world_shade.h"
#include "scratch_dir.h"
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <iostream>
#include <random>

using namespace OpenBlood2;

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
    // A hand-made presentation mesh: slot 0 is white, one range per add_range.
    struct MeshBuilder
    {
        WorldMesh mesh;
        MeshBuilder()
        {
            mesh.materials.resize(1);
            mesh.materials[0].image = solid(255, 255, 255, 255);
        }
        static Texture solid(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
        {
            Texture t;
            t.width = t.height = 1;
            t.mipmaps = 1;
            t.rgba = {r, g, b, a};
            return t;
        }
        std::uint32_t material(Texture image)
        {
            WorldMaterial m;
            m.path = "textures/test" + std::to_string(mesh.materials.size()) + ".dtx";
            m.image = std::move(image);
            mesh.materials.push_back(std::move(m));
            return static_cast<std::uint32_t>(mesh.materials.size() - 1);
        }
        void add_range(DVector pivot = {})
        {
            WorldRange r;
            r.first = static_cast<std::uint32_t>(mesh.vertices.size());
            r.pivot = pivot;
            mesh.ranges.push_back(r);
        }
        // One triangle batch; positions local to the range pivot.
        void triangle(std::array<DVector, 3> p, std::uint32_t material, bool translucent = false,
                      bool sky_portal = false, std::array<DVector, 3> colors = {})
        {
            WorldBatch batch;
            batch.first = static_cast<std::uint32_t>(mesh.vertices.size());
            batch.count = 3;
            batch.material = material;
            batch.translucent = translucent;
            batch.sky_portal = sky_portal;
            const auto e1 = vector_subtract(p[1], p[0]), e2 = vector_subtract(p[2], p[0]);
            DVector n{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
            const float length = std::sqrt(vector_dot(n, n));
            n = vector_scale(n, 1 / length);
            for (int k = 0; k < 3; ++k)
            {
                WorldVertex v{p[k], colors[k]};
                v.normal = n;
                v.uv = {p[k].y / 100, p[k].z / 100};
                mesh.vertices.push_back(v);
            }
            mesh.ranges.back().batches.push_back(batch);
            mesh.ranges.back().count = static_cast<std::uint32_t>(mesh.vertices.size()) - mesh.ranges.back().first;
        }
        // A square in the plane x = at, |y|, |z| <= 50, normal +x (two triangles).
        void wall(float at, std::uint32_t material, bool translucent = false, bool sky_portal = false)
        {
            triangle({DVector{at, -50, -50}, DVector{at, 50, -50}, DVector{at, 50, 50}}, material, translucent,
                     sky_portal);
            triangle({DVector{at, -50, -50}, DVector{at, 50, 50}, DVector{at, -50, 50}}, material, translucent,
                     sky_portal);
        }
    };
    bool build(MeshBuilder& b, GiBakeScene& scene, std::vector<GiRangeUse> use = {})
    {
        if (use.empty())
            use.assign(b.mesh.ranges.size(), GiRangeUse::Main);
        GiSceneReport report;
        std::string error;
        if (!scene.build(b.mesh, use, report, error))
        {
            std::cerr << error << '\n';
            return false;
        }
        return true;
    }
    // The brute-force reference: every triangle, the tracer's arithmetic, lower face on ties.
    bool brute(const GiBvh& bvh, DVector o, DVector d, float tmax, std::uint32_t& face, float& best)
    {
        face = UINT32_MAX;
        best = tmax;
        for (const auto& t : bvh.triangles())
        {
            const float px = d.y * t.e2[2] - d.z * t.e2[1], py = d.z * t.e2[0] - d.x * t.e2[2],
                        pz = d.x * t.e2[1] - d.y * t.e2[0];
            const float det = t.e1[0] * px + t.e1[1] * py + t.e1[2] * pz;
            if (std::abs(det) < 1e-12f)
                continue;
            const float sx = o.x - t.v0[0], sy = o.y - t.v0[1], sz = o.z - t.v0[2];
            const float u = (sx * px + sy * py + sz * pz) / det;
            if (u < 0 || u > 1)
                continue;
            const float qx = sy * t.e1[2] - sz * t.e1[1], qy = sz * t.e1[0] - sx * t.e1[2],
                        qz = sx * t.e1[1] - sy * t.e1[0];
            const float v = (d.x * qx + d.y * qy + d.z * qz) / det;
            if (v < 0 || u + v > 1)
                continue;
            const float f = (t.e2[0] * qx + t.e2[1] * qy + t.e2[2] * qz) / det;
            if (f < 0 || f > best)
                continue;
            if (f < best || t.face < face)
            {
                best = f;
                face = t.face;
            }
        }
        return face != UINT32_MAX;
    }
    DVector random_unit(std::mt19937& rng)
    {
        std::normal_distribution<float> n;
        for (;;)
        {
            DVector d{n(rng), n(rng), n(rng)};
            const float l = std::sqrt(vector_dot(d, d));
            if (l > 1e-3f)
                return vector_scale(d, 1 / l);
        }
    }

    // Random triangle soup (two ranges, one offset by its pivot): every hit equals brute force.
    bool bvh()
    {
        MeshBuilder b;
        std::mt19937 rng(7);
        std::uniform_real_distribution<float> pos(-500, 500), size(-40, 40);
        for (int r = 0; r < 2; ++r)
        {
            b.add_range(r ? DVector{100, -50, 25} : DVector{});
            for (int i = 0; i < 3000; ++i)
            {
                const DVector c{pos(rng), pos(rng), pos(rng)};
                b.triangle({c, vector_add(c, {size(rng), size(rng), size(rng)}),
                            vector_add(c, {size(rng), size(rng), size(rng)})},
                           0);
            }
        }
        b.add_range(); // Duplicate faces, away from the soup: a tie.
        b.triangle({DVector{2000, 0, 0}, DVector{2010, 0, 0}, DVector{2000, 10, 0}}, 0);
        b.triangle({DVector{2000, 0, 0}, DVector{2010, 0, 0}, DVector{2000, 10, 0}}, 0);
        GiBakeScene scene;
        CHECK(build(b, scene));
        CHECK(scene.faces().size() == 6002 && scene.main().triangles().size() == 6002);
        CHECK(scene.main().depth() <= gi_bake_limits::max_depth && !scene.main().nodes().empty());
        std::size_t leaves = 0, leaf_triangles = 0;
        for (const auto& node : scene.main().nodes())
            if (node.count)
            {
                ++leaves;
                leaf_triangles += node.count;
            }
        CHECK(leaf_triangles == 6002 && leaves > 6002 / 16);
        GiTraceStats stats;
        GiHit hit;
        std::size_t hits = 0;
        for (int i = 0; i < 20000; ++i)
        {
            const DVector o{pos(rng), pos(rng), pos(rng)};
            auto d = random_unit(rng);
            if (i % 7 == 0)
                d = {0, 0, i % 2 ? 1.f : -1.f}; // Axis rays: zero direction components.
            const float tmax = i % 3 ? FLT_MAX : 300;
            std::uint32_t face;
            float t;
            const bool expected = brute(scene.main(), o, d, tmax, face, t);
            scene.trace(o, d, tmax, hit, stats);
            CHECK(expected == (hit.kind == GiHit::Kind::Surface));
            if (expected)
            {
                CHECK(hit.face == face && hit.t == t && hit.layer_count == 0);
                CHECK(hit.back == (vector_dot(d, scene.faces()[face].normal) > 0));
                ++hits;
            }
        }
        CHECK(hits > 2000 && stats.rays == 20000 && stats.triangles < 20000ull * 6002 / 20);
        // The tie: the ray hits two identical faces at the same t; the lower index wins.
        scene.trace({2002, 2, 5}, {0, 0, -1}, FLT_MAX, hit, stats);
        CHECK(hit.kind == GiHit::Kind::Surface && hit.face == 6000 && hit.t == 5);
        // An empty scene and a negative tmax miss.
        scene.trace({2, 2, 5}, {0, 0, -1}, -1, hit, stats);
        CHECK(hit.kind == GiHit::Kind::Miss);
        GiBakeScene empty;
        MeshBuilder none;
        none.add_range();
        CHECK(build(none, empty));
        empty.trace({}, {1, 0, 0}, FLT_MAX, hit, stats);
        CHECK(hit.kind == GiHit::Kind::Miss && hit.face == UINT32_MAX);
        return true;
    }

    // Cutouts, partial-alpha layers, the eight-layer limit, sky portals and unresolved faces.
    bool alpha()
    {
        MeshBuilder b;
        const auto cut = b.material(MeshBuilder::solid(10, 20, 30, 0));
        const auto half = b.material(MeshBuilder::solid(10, 20, 30, 128));
        const auto full = b.material(MeshBuilder::solid(200, 100, 50, 255));
        const auto wall = b.material(MeshBuilder::solid(64, 128, 255, 77)); // Opaque face: alpha ignored.
        GiTraceStats stats;
        GiHit hit;
        {
            b.add_range();
            b.wall(10, cut, true);
            b.wall(20, half, true);
            b.wall(30, full, true);
            b.wall(40, wall);
            GiBakeScene scene;
            CHECK(build(b, scene));
            scene.trace({0, 1, 2}, {1, 0, 0}, FLT_MAX, hit, stats);
            CHECK(hit.kind == GiHit::Kind::Surface && hit.t == 30 && hit.layer_count == 1 && hit.back);
            CHECK(hit.layers[0].t == 20 && std::abs(hit.layers[0].alpha - 128 / 255.f) < 1e-6f);
            CHECK(scene.faces()[hit.layers[0].face].material == half && scene.faces()[hit.face].material == full);
            const auto a = scene.albedo(hit.face, hit.u, hit.v);
            CHECK(a[0] == 200 / 255.f && a[1] == 100 / 255.f && a[3] == 1);
            // From the other side: the opaque wall first, front facing; the opaque face ignores alpha.
            scene.trace({50, 1, 2}, {-1, 0, 0}, FLT_MAX, hit, stats);
            CHECK(hit.kind == GiHit::Kind::Surface && hit.t == 10 && !hit.back && hit.layer_count == 0);
            CHECK(scene.albedo(hit.face, hit.u, hit.v)[3] == 1);
            // A short segment ends before the layer: a miss.
            scene.trace({0, 1, 2}, {1, 0, 0}, 15, hit, stats);
            CHECK(hit.kind == GiHit::Kind::Miss && hit.layer_count == 0);
            // Within tmax: the layer is kept even without a surface.
            scene.trace({0, 1, 2}, {1, 0, 0}, 25, hit, stats);
            CHECK(hit.kind == GiHit::Kind::Miss && hit.layer_count == 1);
        }
        for (const int layers : {8, 9, 12})
        {
            MeshBuilder m;
            const auto glass = m.material(MeshBuilder::solid(1, 2, 3, 100));
            const auto stop = m.material(MeshBuilder::solid(9, 9, 9, 255));
            m.add_range();
            for (int i = layers; i-- > 0;) // Authored far to near: order comes from t.
                m.wall(10.f + 10 * i, glass, true);
            m.wall(500, stop);
            GiBakeScene scene;
            CHECK(build(m, scene));
            scene.trace({0, 0, 1}, {1, 0, 0}, FLT_MAX, hit, stats);
            CHECK(hit.layer_count == std::uint32_t(std::min(layers, 8)));
            for (std::uint32_t i = 0; i < hit.layer_count; ++i)
                CHECK(hit.layers[i].t == 10.f + 10 * i);
            if (layers <= 8)
                CHECK(hit.kind == GiHit::Kind::Surface && hit.t == 500);
            else
                CHECK(hit.kind == GiHit::Kind::LayerLimit && hit.t == 90 &&
                      scene.faces()[hit.face].material == glass);
        }
        {
            MeshBuilder m;
            m.add_range();
            m.wall(10, 0, false, true); // Sky portal.
            m.wall(-10, 0);             // Unresolved.
            GiBakeScene scene;
            CHECK(build(m, scene));
            scene.trace({0, 0, 1}, {1, 0, 0}, FLT_MAX, hit, stats);
            CHECK(hit.kind == GiHit::Kind::Sky && hit.t == 10);
            CHECK(scene.faces()[hit.face].flags & GiBakeFace::SkyPortal);
            scene.trace({0, 0, 1}, {-1, 0, 0}, FLT_MAX, hit, stats);
            CHECK(hit.kind == GiHit::Kind::Surface && (scene.faces()[hit.face].flags & GiBakeFace::Unresolved));
            CHECK((scene.albedo(hit.face, hit.u, hit.v) == std::array<float, 4>{.5f, .5f, .5f, 1}));
        }
        {
            // Sky ranges stay out of the main tree and go to the sky tree; skipped ranges nowhere.
            MeshBuilder m;
            m.add_range();
            m.wall(10, 0);
            m.add_range();
            m.wall(20, 0);
            m.add_range({100, 0, 0});
            m.wall(-70, 0); // At x = 30.
            GiBakeScene scene;
            CHECK(build(m, scene, {GiRangeUse::Skip, GiRangeUse::Sky, GiRangeUse::Main}));
            scene.trace({0, 0, 1}, {1, 0, 0}, FLT_MAX, hit, stats);
            CHECK(hit.kind == GiHit::Kind::Surface && hit.t == 30 && scene.faces()[hit.face].range == 2);
            scene.trace_sky({0, 0, 1}, {1, 0, 0}, FLT_MAX, hit, stats);
            CHECK(hit.kind == GiHit::Kind::Surface && hit.t == 20 && scene.faces()[hit.face].range == 1);
            GiSceneReport report;
            std::string error;
            CHECK(!scene.build(m.mesh, std::vector<GiRangeUse>{GiRangeUse::Main}, report, error));
        }
        return true;
    }

    // Mip choice, mip sampling, baked vertex and lightmap light.
    bool albedo()
    {
        const std::array<DVector, 3> p{DVector{0, 0, 0}, DVector{0, 20, 0}, DVector{0, 0, 20}};
        const std::array<std::array<float, 2>, 3> uv{{{0, 0}, {1, 0}, {0, 1}}};
        CHECK(gi_albedo_mip(p, uv, 64, 64, 4) == 3);  // 64 texels / 20 units: level 6, clamped.
        CHECK(gi_albedo_mip(p, uv, 64, 64, 8) == 6);
        CHECK(gi_albedo_mip(p, uv, 1, 1, 8) == 0);    // One texel over 20 units.
        CHECK(gi_albedo_mip(p, uv, 64, 64, 1) == 0);
        const std::array<std::array<float, 2>, 3> flat{{{0, 0}, {0, 0}, {0, 0}}};
        CHECK(gi_albedo_mip(p, flat, 64, 64, 8) == 0);

        // An 8x8 texture with two levels: level 0 red, level 1 (4x4) green, level 2 (2x2) blue.
        Texture t;
        t.width = t.height = 8;
        t.mipmaps = 3;
        for (int i = 0; i < 64; ++i)
            t.rgba.insert(t.rgba.end(), {255, 0, 0, 255});
        for (int i = 0; i < 16; ++i)
            t.mip_rgba.insert(t.mip_rgba.end(), {0, 255, 0, 255});
        for (int i = 0; i < 4; ++i)
            t.mip_rgba.insert(t.mip_rgba.end(), {0, 0, 255, 255});
        CHECK(t.mip_rgba.size() == texture_mip_bytes(t));
        MeshBuilder b;
        const auto m = b.material(t);
        b.add_range();
        // uv spans 1 per 100 units: 0.08 texels per unit -> log2(1.6) -> level 1.
        b.triangle({DVector{0, 0, 0}, DVector{0, 100, 0}, DVector{0, 0, 100}}, m, false, false,
                   {DVector{1, 0, 0}, DVector{0, 1, 0}, DVector{0, 0, 1}});
        GiBakeScene scene;
        CHECK(build(b, scene));
        CHECK(scene.faces()[0].mip == 1);
        CHECK((scene.albedo(0, .2f, .2f) == std::array<float, 4>{0, 1, 0, 1}));
        const auto light = scene.baked_light(0, .25f, .5f);
        CHECK(std::abs(light.x - .25f) < 1e-6f && std::abs(light.y - .25f) < 1e-6f && std::abs(light.z - .5f) < 1e-6f);

        // A lightmapped face: bilinear page sample times world_saturate_scale (WorldShade's rule).
        MeshBuilder lm;
        Texture page;
        page.width = page.height = 2;
        page.rgba = {100, 0, 0, 255, 200, 0, 0, 255, 100, 0, 0, 255, 200, 0, 0, 255};
        WorldMaterial pm;
        pm.path = "lightmap page 0";
        pm.image = page;
        lm.mesh.materials.push_back(pm);
        const auto page_id = static_cast<std::uint32_t>(lm.mesh.materials.size() - 1);
        const auto white = lm.material(MeshBuilder::solid(255, 255, 255, 255));
        lm.add_range();
        lm.triangle({DVector{0, 0, 0}, DVector{0, 100, 0}, DVector{0, 0, 100}}, white);
        lm.mesh.ranges.back().batches.back().lightmap = page_id;
        for (int k = 0; k < 3; ++k)
            lm.mesh.vertices[k].lightmap = {k == 1 ? 1.f : 0.f, k == 2 ? 1.f : 0.f};
        GiBakeScene lit;
        CHECK(build(lm, lit));
        CHECK(lit.pages().size() == 1 && (lit.faces()[0].flags & GiBakeFace::Lightmapped));
        // Page (0.5, 0.25): x = 0.5 -> halfway between the two columns.
        const auto sample = lit.baked_light(0, .5f, .25f);
        CHECK(std::abs(sample.x - world_saturate_scale * 150 / 255.f) < 1e-5f && sample.y == 0);
        return true;
    }

    bool jobs()
    {
        std::vector<std::uint64_t> slots(1000);
        auto run = run_gi_jobs(slots.size(), 4, nullptr, [&](std::uint64_t i, unsigned) { slots[i] = i * 2 + 1; });
        CHECK(!run.cancelled && run.done == 1000 && run.threads >= 1 && run.threads <= 4);
        for (std::size_t i = 0; i < slots.size(); ++i)
            CHECK(slots[i] == i * 2 + 1);
        run = run_gi_jobs(0, 8, nullptr, [](std::uint64_t, unsigned) {});
        CHECK(!run.cancelled && run.done == 0 && run.threads == 1);
        std::atomic<bool> cancel{false};
        std::atomic<std::uint64_t> started{0};
        run = run_gi_jobs(100000, 4, &cancel,
                          [&](std::uint64_t i, unsigned)
                          {
                              started.fetch_add(1);
                              if (i == 50)
                                  cancel.store(true);
                          });
        CHECK(run.cancelled && run.done < 100000 && run.done == started.load());
        cancel.store(true);
        run = run_gi_jobs(10, 2, &cancel, [](std::uint64_t, unsigned) {});
        CHECK(run.cancelled && run.done == 0);

        GiRandom a(3, 4), b(3, 4), c(4, 3);
        bool differs = false;
        for (int i = 0; i < 1000; ++i)
        {
            const float x = a.uniform();
            CHECK(x == b.uniform() && x >= 0 && x < 1);
            differs |= x != c.uniform();
        }
        CHECK(differs);
        CHECK(gi_default_jobs() >= 1 && gi_default_jobs() <= 256);
        // Directions: unit length, reproducible, centred.
        DVector sum{};
        for (std::uint64_t i = 0; i < 20000; ++i)
        {
            const auto d = gi_uniform_direction(9, i), e = gi_uniform_direction(9, i);
            CHECK(d.x == e.x && d.y == e.y && d.z == e.z && std::abs(vector_dot(d, d) - 1) < 1e-6f);
            sum = vector_add(sum, d);
        }
        CHECK(std::abs(sum.x) < 400 && std::abs(sum.y) < 400 && std::abs(sum.z) < 400);
        return true;
    }

    bool settings()
    {
        GiBakeSettings s;
        std::string error;
        CHECK(make_gi_bake_settings("preview", 0, 0, -1, s, error) && s.quality == GiQuality::Preview);
        CHECK(s.bounces == 1 && s.rays == 64 && s.sky == 100);
        CHECK(make_gi_bake_settings("", 0, 0, -1, s, error) && s.quality == GiQuality::Final && s.rays == 256);
        // Re-baked is the default direct source since Gate G; its settings line comes last.
        CHECK(s.direct == GiDirectSource::Rebaked);
        CHECK(gi_settings_text(s) ==
              "openblood2-gi-settings 2\nquality final\nbounces 2\nrays 256\nsky 1.00\ndirect rebaked\n");
        CHECK(make_gi_bake_settings("custom", 3, 0, -1, s, error) && s.bounces == 3 && s.rays == 256 && s.sky == 100);
        CHECK(make_gi_bake_settings("custom", 0, 4096, 25, s, error) && s.bounces == 2 && s.rays == 4096);
        CHECK(s.direct == GiDirectSource::Rebaked);
        CHECK(gi_settings_text(s) ==
              "openblood2-gi-settings 2\nquality custom\nbounces 2\nrays 4096\nsky 0.25\ndirect rebaked\n");
        // Original bakes keep the text (and the bytes) of the lots before GD2.
        s.direct = GiDirectSource::Original;
        CHECK(gi_settings_text(s) == "openblood2-gi-settings 2\nquality custom\nbounces 2\nrays 4096\nsky 0.25\n");
        CHECK(make_gi_bake_settings("custom", 0, 0, 0, s, error) && s.sky == 0);
        CHECK(gi_settings_text(s).ends_with("\nsky 0.00\ndirect rebaked\n"));
        CHECK(make_gi_bake_settings("custom", 0, 0, 400, s, error) && s.sky == 400);
        CHECK(gi_settings_text(s).ends_with("\nsky 4.00\ndirect rebaked\n"));
        CHECK(std::string(gi_direct_source_name(GiDirectSource::Original)) == "original" &&
              std::string(gi_direct_source_name(GiDirectSource::Rebaked)) == "rebaked");
        GiDirectSource source = GiDirectSource::Original;
        CHECK(gi_direct_source("rebaked", source) && source == GiDirectSource::Rebaked);
        CHECK(!gi_direct_source("Rebaked", source) && !gi_direct_source("", source) &&
              source == GiDirectSource::Rebaked);
        CHECK(gi_direct_source("original", source) && source == GiDirectSource::Original);
        // The presets always give the default source, whatever the settings held before.
        s.direct = GiDirectSource::Original;
        CHECK(make_gi_bake_settings("preview", 0, 0, -1, s, error) && s.direct == GiDirectSource::Rebaked);
        CHECK(make_gi_bake_settings("custom", 0, 0, 400, s, error) && s.direct == GiDirectSource::Rebaked);
        const auto before = s.rays;
        CHECK(!make_gi_bake_settings("custom", 5, 0, -1, s, error) && s.rays == before);
        CHECK(!make_gi_bake_settings("custom", 0, 15, -1, s, error));
        CHECK(!make_gi_bake_settings("custom", 0, 0, 401, s, error) && error == "--gi-sky must be 0 to 4");
        CHECK(!make_gi_bake_settings("custom", 0, 0, -2, s, error) && s.sky == 400);
        CHECK(!make_gi_bake_settings("final", 2, 0, -1, s, error));
        CHECK(!make_gi_bake_settings("final", 0, 0, 100, s, error));
        CHECK(error == "--gi-bounces, --gi-rays, --gi-sky, --gi-texel-samples, --gi-denoise, --gi-shadow-radius, "
                       "--gi-shadow-rays, --gi-reflection-strata and --gi-reflection-samples need --gi-quality custom");
        CHECK(!make_gi_bake_settings("best", 0, 0, -1, s, error));

        // Lots GQ1 and GE1 (decision D21): explicit defaults keep the text; each other value adds a line
        // before "direct rebaked".
        const std::string final_text = "openblood2-gi-settings 2\nquality final\nbounces 2\nrays 256\nsky 1.00\n";
        GiBakeArguments arguments;
        CHECK(make_gi_bake_settings(arguments, s, error) && gi_settings_text(s) == final_text + "direct rebaked\n");
        CHECK(s.texel_grid == 1 && s.denoise == 0 && s.shadow_radius == 0 && s.shadow_rays == 0);
        CHECK(s.reflection_strata == 2 && s.reflection_samples == 0 && s.emissive == 0);
        arguments.emissive = 0; // Given as zero: still the defaults.
        CHECK(make_gi_bake_settings(arguments, s, error) && gi_settings_text(s) == final_text + "direct rebaked\n");
        arguments = {"ultra"};
        CHECK(make_gi_bake_settings(arguments, s, error) && s.quality == GiQuality::Ultra);
        CHECK(s.bounces == 3 && s.rays == 512 && s.sky == 100 && s.texel_grid == 2 && s.denoise == 1);
        CHECK(s.shadow_radius == 8 && s.shadow_rays == 16 && s.reflection_strata == 4 && s.reflection_samples == 256);
        CHECK(gi_settings_text(s) == "openblood2-gi-settings 2\nquality ultra\nbounces 3\nrays 512\nsky 1.00\n"
                                     "texel-samples 4\ndenoise 1\nshadows 8 16\nreflection-strata 4\n"
                                     "reflection-samples 256\ndirect rebaked\n");
        CHECK(std::string(gi_quality_name(GiQuality::Ultra)) == "ultra");
        const auto digest = gi_manifest_digest_prefix();
        CHECK(digest.size() == 16 && digest.find_first_not_of("0123456789abcdef") == std::string::npos);
        arguments.emissive = 150;
        CHECK(make_gi_bake_settings(arguments, s, error) && s.emissive == 150);
        CHECK(gi_settings_text(s).ends_with("reflection-samples 256\nemissive 1.50 " + digest + "\ndirect rebaked\n"));
        arguments = {"preview"};
        arguments.emissive = 5;
        CHECK(make_gi_bake_settings(arguments, s, error) && s.emissive == 5 && s.quality == GiQuality::Preview);
        CHECK(gi_settings_text(s) == "openblood2-gi-settings 2\nquality preview\nbounces 1\nrays 64\nsky 1.00\n"
                                     "emissive 0.05 " + digest + "\ndirect rebaked\n");
        arguments = {"custom", 4, 0, -1, 16, 4, 64, 0, 8, 1024, 400};
        CHECK(make_gi_bake_settings(arguments, s, error) && s.bounces == 4 && s.texel_grid == 4);
        CHECK(s.denoise == 4 && s.shadow_radius == 64 && s.shadow_rays == 16 && s.reflection_strata == 8);
        CHECK(s.reflection_samples == 1024 && s.emissive == 400);
        CHECK(gi_settings_text(s) == "openblood2-gi-settings 2\nquality custom\nbounces 4\nrays 256\nsky 1.00\n"
                                     "texel-samples 16\ndenoise 4\nshadows 64 16\nreflection-strata 8\n"
                                     "reflection-samples 1024\nemissive 4.00 " + digest + "\ndirect rebaked\n");
        s.direct = GiDirectSource::Original; // The original source drops only its own line.
        CHECK(gi_settings_text(s).ends_with("emissive 4.00 " + digest + "\n"));
        arguments = {"custom", 0, 0, -1, 1, 0, 4, 9, 2, 32};
        CHECK(make_gi_bake_settings(arguments, s, error) && gi_settings_text(s).ends_with(
                  "sky 1.00\nshadows 4 9\nreflection-samples 32\ndirect rebaked\n"));
        const auto kept = s;
        for (const auto& [wrong, message] : std::initializer_list<std::pair<GiBakeArguments, std::string_view>>{
                 {{"custom", 0, 0, -1, 3}, "--gi-texel-samples must be 1, 4, 9 or 16"},
                 {{"custom", 0, 0, -1, 0, 5}, "--gi-denoise must be 0 to 4"},
                 {{"custom", 0, 0, -1, 0, -1, 65}, "--gi-shadow-radius and --gi-shadow-rays must be 1 to 64"},
                 {{"custom", 0, 0, -1, 0, -1, 4, 65}, "--gi-shadow-radius and --gi-shadow-rays must be 1 to 64"},
                 {{"custom", 0, 0, -1, 0, -1, 0, 4}, "--gi-shadow-rays needs --gi-shadow-radius"},
                 {{"custom", 0, 0, -1, 0, -1, 0, 0, 9}, "--gi-reflection-strata must be 2 to 8"},
                 {{"custom", 0, 0, -1, 0, -1, 0, 0, 1}, "--gi-reflection-strata must be 2 to 8"},
                 {{"custom", 0, 0, -1, 0, -1, 0, 0, 0, 31}, "--gi-reflection-samples must be 32 to 1024"},
                 {{"custom", 0, 0, -1, 0, -1, 0, 0, 0, 1025}, "--gi-reflection-samples must be 32 to 1024"},
                 {{"final", 0, 0, -1, 0, -1, 0, 0, 0, 0, 401}, "--gi-emissive must be 0 to 4"},
                 {{"ultra", 0, 0, -1, 0, 1},
                  "--gi-bounces, --gi-rays, --gi-sky, --gi-texel-samples, --gi-denoise, --gi-shadow-radius, "
                  "--gi-shadow-rays, --gi-reflection-strata and --gi-reflection-samples need --gi-quality custom"}})
        {
            CHECK(!make_gi_bake_settings(wrong, s, error) && error == message);
            CHECK(gi_settings_text(s) == gi_settings_text(kept)); // Failures leave the settings.
        }
        CHECK(std::string(gi_bake_status_name(GiBakeResult::Status::Current)) == "current");
        return true;
    }

    // A closed room with glass inside: the survey digest and counts are identical for 1, 2 and 7 jobs.
    bool determinism()
    {
        MeshBuilder b;
        const auto stone = b.material(MeshBuilder::solid(90, 90, 90, 255));
        const auto glass = b.material(MeshBuilder::solid(0, 90, 200, 120));
        b.add_range();
        const float s = 200;
        const DVector c[8]{{-s, -s, -s}, {s, -s, -s}, {-s, s, -s}, {s, s, -s},
                           {-s, -s, s},  {s, -s, s},  {-s, s, s},  {s, s, s}};
        const int quads[6][4]{{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
        for (const auto& q : quads)
        {
            b.triangle({c[q[0]], c[q[1]], c[q[2]]}, stone);
            b.triangle({c[q[0]], c[q[2]], c[q[3]]}, stone);
        }
        b.wall(0, glass, true);
        b.wall(100, 0, false, true);
        GiBakeScene scene;
        CHECK(build(b, scene));
        std::vector<GiLightProbe> probes;
        std::mt19937 rng(11);
        std::uniform_real_distribution<float> pos(-190, 190);
        for (std::uint32_t i = 0; i < 97; ++i)
            probes.push_back({i, {pos(rng), pos(rng), pos(rng)}, false});
        probes.push_back({97, {300, 0, 0}, false}); // Outside: half the rays miss.
        std::uint64_t progress_calls = 0, last = 0;
        bool ordered = true;
        const auto reference = survey_gi_probes(scene, probes, 64, 1, nullptr,
                                                [&](std::string_view stage, std::uint64_t done, std::uint64_t total)
                                                {
                                                    ordered &= stage == "survey" && done > last && total == 98;
                                                    last = done;
                                                    ++progress_calls;
                                                });
        CHECK(ordered && last == 98 && progress_calls >= 5 && progress_calls <= 21);
        CHECK(reference.rays == 98 * 64 && reference.miss > 0 && reference.miss < 64 && !reference.cancelled);
        CHECK(reference.front + reference.back + reference.sky + reference.layer_limit + reference.miss ==
              reference.rays);
        CHECK(reference.layers > 0 && reference.sky > 0 && reference.back > 0 && reference.inside >= 1);
        for (const unsigned jobs : {2u, 7u})
        {
            const auto again = survey_gi_probes(scene, probes, 64, jobs, nullptr, {});
            CHECK(again.digest == reference.digest && again.front == reference.front &&
                  again.back == reference.back && again.layers == reference.layers &&
                  again.inside == reference.inside && again.trace.triangles == reference.trace.triangles);
        }
        const auto other = survey_gi_probes(scene, probes, 65, 3, nullptr, {});
        CHECK(other.digest != reference.digest);
        std::atomic<bool> cancel{true};
        const auto stopped = survey_gi_probes(scene, probes, 64, 3, &cancel, {});
        CHECK(stopped.cancelled);
        return true;
    }

    // Private: 01_SubwayA. Hits against WorldShade, identical files for 1 and 4 jobs, --gi-if-stale,
    // cancellation and throughput.
    bool retail(const std::filesystem::path& data, const std::filesystem::path& placement_dir,
                const std::filesystem::path& scratch)
    {
        CHECK(Testing::reset_scratch_dir(scratch));
        Config config;
        config.data_dir = data;
        Resources resources;
        std::string error;
        CHECK(resources.mount_game(config, error));
        const std::string path = "worlds/01_subwaya.dat";
        std::vector<std::uint8_t> bytes;
        World world;
        CHECK(resources.read(path, bytes, error) && read_world(bytes, world, error));

        // Exclusions: a real non-primary model by any case; an unknown name fails.
        std::vector<GiRangeUse> use;
        GiSceneReport report;
        std::string mover;
        const auto roles = world_model_roles(world);
        for (std::size_t m = 1; m < roles.size() && mover.empty(); ++m)
            if (roles[m].role == WorldModelRole::Mover)
                mover = world.models[m].name;
        CHECK(!mover.empty());
        std::string upper = mover;
        for (auto& ch : upper)
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        CHECK(gi_range_uses(world, std::vector<std::string>{upper}, use, report, error));
        CHECK(report.excluded_ranges >= 1);
        CHECK(!gi_range_uses(world, std::vector<std::string>{"NoSuchModel_G5"}, use, report, error));
        CHECK(error.find("NoSuchModel_G5") != std::string::npos);
        report = {};
        CHECK(gi_range_uses(world, {}, use, report, error));

        WorldMesh mesh, shade_mesh;
        CHECK(mesh.build(world, error, &resources, true) && shade_mesh.build(world, error, &resources, true));
        GiBakeScene scene;
        CHECK(scene.build(std::move(mesh), use, report, error));
        WorldShade shade;
        CHECK(shade.build(shade_mesh, error));
        std::vector<WorldDraw> draws;
        for (std::uint32_t r = 0; r < use.size(); ++r)
            if (use[r] == GiRangeUse::Main)
                draws.push_back({r, scene.mesh().ranges[r].pivot, DRotation{}});
        std::cout << "SCENE faces=" << report.main_faces << " sky_faces=" << report.sky_faces
                  << " translucent=" << report.translucent << " sky_portals=" << report.sky_portals
                  << " unresolved=" << report.unresolved << " lightmapped=" << report.lightmapped
                  << " nodes=" << report.nodes << " depth=" << report.depth << " ranges=" << report.main_ranges << '/'
                  << report.sky_ranges << '/' << report.skipped_ranges << " build_ms=" << report.build_ms << '\n';

        // Segments from the lattice probes: the bake's opaque, unlayered hits equal WorldShade's.
        CollisionWorld collision;
        GiPlacementScene placement_scene;
        CHECK(collision.build(world.models.at(0), error) && placement_scene.build(world, collision, error));
        GiLightLattice lattice;
        GiPlacementStats stats;
        const auto placement = default_gi_placement(path, sha256(bytes));
        CHECK(resolve_light_probes(placement_scene, placement, lattice, stats, nullptr, error));
        std::mt19937 rng(5);
        std::size_t compared = 0, misses = 0, skipped = 0, worst_light_index = 0;
        float worst_t = 0, worst_light = 0;
        GiTraceStats trace;
        GiHit hit;
        for (std::size_t i = 0; i < 20000; ++i)
        {
            const auto& from = lattice.probes[i % lattice.probes.size()].position;
            const auto delta = vector_scale(random_unit(rng), 3000);
            scene.trace(from, delta, 1, hit, trace);
            DVector light{};
            float fraction = 0;
            const bool found = shade.trace(from, vector_add(from, delta), draws, light, fraction);
            if (hit.kind == GiHit::Kind::Miss && hit.layer_count == 0)
            {
                CHECK(!found);
                ++misses;
                continue;
            }
            const auto& face = scene.faces()[hit.face];
            if (hit.kind != GiHit::Kind::Surface || hit.layer_count ||
                (face.flags & (GiBakeFace::Translucent | GiBakeFace::Unresolved)))
            {
                ++skipped;
                continue;
            }
            CHECK(found);
            const auto baked = scene.baked_light(hit.face, hit.u, hit.v);
            const float dl = std::max({std::abs(baked.x - light.x), std::abs(baked.y - light.y),
                                       std::abs(baked.z - light.z)});
            const float dt = std::abs(hit.t - fraction);
            if (dt > worst_t)
                worst_t = dt;
            if (dl > worst_light)
            {
                worst_light = dl;
                worst_light_index = i;
            }
            ++compared;
        }
        std::cout << "SHADE compared=" << compared << " misses=" << misses << " skipped=" << skipped
                  << " worst_fraction=" << worst_t << " worst_light=" << worst_light << " (segment "
                  << worst_light_index << ")\n";
        CHECK(compared > 10000 && worst_t < 1e-5f && worst_light < 1e-3f);

        // Bakes: identical bytes and digests for 1 and 4 jobs; --gi-if-stale; cancellation.
        GiBakeRequest request;
        request.world = path;
        CHECK(make_gi_bake_settings("preview", 0, 0, -1, request.settings, error));
        request.placement_dir = placement_dir;
        request.jobs = 1;
        request.out_dir = scratch / "one";
        const auto one = bake_gi_world(resources, request);
        CHECK(one.status == GiBakeResult::Status::Baked && one.exit_code == 0 && one.error.empty());
        CHECK(one.placement == GiPlacementStatus::Ready && one.probes == 1031);
        request.jobs = 4;
        request.out_dir = scratch / "four";
        const auto four = bake_gi_world(resources, request);
        CHECK(four.status == GiBakeResult::Status::Baked && four.survey.threads == 4);
        CHECK(four.survey.digest == one.survey.digest && four.survey.rays == one.survey.rays);
        std::vector<std::uint8_t> a, b;
        CHECK(read_gi_bytes(path_text(one.file), a, error) && read_gi_bytes(path_text(four.file), b, error));
        CHECK(!a.empty() && a == b && a.size() == one.bytes);
        GiFile file;
        CHECK(read_gi_file(a, file, error) && file.settings == gi_settings_text(request.settings));
        // The default bake re-bakes the direct light (Gate G): DIR after LMAP; REFL last (lot G11).
        CHECK(file.find(gi_format::placement) && file.find(gi_format::lightmaps) && file.find(gi_format::direct) &&
              file.find(gi_format::vertices) && file.find(gi_format::probes) && file.sections.size() == 6 &&
              file.sections[5].tag == gi_format::reflections && file.key.algorithm == 4 &&
              file.settings.ends_with("\ndirect rebaked\n"));
        // Lot G11: one REFL chain per PLAC reflection probe, at their resolution.
        {
            GiLightLattice baked_lattice;
            std::vector<GiReflectionProbe> reflections;
            GiReflections refl;
            CHECK(decode_gi_placement_section(file.find(gi_format::placement)->payload, baked_lattice, reflections,
                                              error) &&
                  read_gi_reflections(file.find(gi_format::reflections)->payload, refl, error) &&
                  check_gi_reflections(reflections, refl, error));
            CHECK(refl.size() == reflections.size() && refl.size() == one.reflection_probes.probes &&
                  !reflections.empty() && one.reflection_probes.valid == refl.size() && refl.resolution == 64 &&
                  refl.levels == 5);
            std::cout << "REFLECTIONS probes=" << refl.size() << " valid=" << one.reflection_probes.valid
                      << " rays=" << one.reflection_probes.rays << " capture_ms=" << one.reflection_probes.capture_ms
                      << " prefilter_ms=" << one.reflection_probes.prefilter_ms << " ms=" << one.reflection_probes.ms
                      << " bytes=" << file.find(gi_format::reflections)->payload.size() << '\n';
        }
        // Lot G9: one PROB entry per PLAC probe, every SubwayA probe valid.
        {
            GiLightLattice baked_lattice;
            std::vector<GiReflectionProbe> reflections;
            GiProbes probes;
            CHECK(decode_gi_placement_section(file.find(gi_format::placement)->payload, baked_lattice, reflections,
                                              error) &&
                  read_gi_probes(file.find(gi_format::probes)->payload, probes, error) &&
                  check_gi_probes(baked_lattice, probes, error));
            CHECK(probes.size() == 1031 && one.light_probes.probes == 1031 && one.light_probes.valid == 1031);
            CHECK(one.light_probes.rays == 1031ull * request.settings.rays && four.light_probes.valid == 1031);
            std::size_t lit = 0;
            for (std::size_t i = 0; i < probes.size(); ++i)
                lit += gi_sh_irradiance(std::span<const float, 27>(probes.sh.data() + i * 27, 27), {0, 0, 1}).y > 0;
            CHECK(lit * 10 > probes.size() * 9);
            std::cout << "PROBES valid=" << one.light_probes.valid << " rays=" << one.light_probes.rays
                      << " back=" << one.light_probes.back << " sky=" << one.light_probes.sky
                      << " ms=" << one.light_probes.ms << " lit_up=" << lit << '\n';
        }
        GiExpectation expected;
        CHECK(gi_expectation(resources, path, expected, error));
        unsigned stale = 0;
        CHECK(gi_status(file.key, expected, stale) == GiStatus::Ready);
        const double rate1 = one.survey.rays / (one.survey.ms / 1000);
        const double rate4 = four.survey.rays / (four.survey.ms / 1000);
        std::cout << "BAKE probes=" << one.probes << " rays=" << one.survey.rays << " front=" << one.survey.front
                  << " back=" << one.survey.back << " sky=" << one.survey.sky << " miss=" << one.survey.miss
                  << " layers=" << one.survey.layers << " layer_limit=" << one.survey.layer_limit
                  << " inside=" << one.survey.inside << " digest=" << digest_text(one.survey.digest)
                  << " bytes=" << one.bytes << "\nTHROUGHPUT jobs=1 rays_per_second=" << std::llround(rate1)
                  << " jobs=4 rays_per_second=" << std::llround(rate4) << " per_thread=" << std::llround(rate4 / 4)
                  << " nodes_per_ray=" << double(one.survey.trace.nodes) / one.survey.rays
                  << " triangles_per_ray=" << double(one.survey.trace.triangles) / one.survey.rays << '\n';

        request.if_stale = true;
        const auto current = bake_gi_world(resources, request);
        CHECK(current.status == GiBakeResult::Status::Current && current.exit_code == 0);
        auto final_request = request;
        CHECK(make_gi_bake_settings("final", 0, 0, -1, final_request.settings, error));
        final_request.cancel = nullptr;
        // Other settings: stale, so it bakes (cancelled at once here, keeping the Preview file).
        std::atomic<bool> cancel{false};
        final_request.cancel = &cancel;
        final_request.progress = [&](std::string_view, std::uint64_t, std::uint64_t) { cancel.store(true); };
        const auto stopped = bake_gi_world(resources, final_request);
        CHECK(stopped.status == GiBakeResult::Status::Cancelled && stopped.exit_code == 12);
        std::vector<std::uint8_t> kept;
        CHECK(read_gi_bytes(path_text(four.file), kept, error) && kept == b);
        // Cancelled into an empty folder: nothing is written.
        final_request.out_dir = scratch / "cancelled";
        cancel.store(false);
        const auto none = bake_gi_world(resources, final_request);
        CHECK(none.exit_code == 12 && !std::filesystem::exists(scratch / "cancelled"));
        // Errors: an unmounted world (2), a foreign placement (4).
        request.world = "worlds/no_such_world.dat";
        CHECK(bake_gi_world(resources, request).exit_code == 2);
        request.world = path;
        request.if_stale = false;
        auto foreign = placement;
        foreign.world[0] ^= 1;
        CHECK(save_gi_placement(scratch / "placement", foreign, error));
        request.placement_dir = scratch / "placement";
        const auto refused = bake_gi_world(resources, request);
        CHECK(refused.exit_code == 4 && refused.placement == GiPlacementStatus::Foreign);
        return Testing::remove_scratch_dir(scratch);
    }

    // Private, lot GD2: re-baked 01_SubwayA Preview bakes against a scratch placement. Identical bytes
    // for 1, 2 and 7 jobs; the DIR section after LMAP; an original bake without it or a direct line;
    // staleness across sources and after a placement change; cancellation during the direct pass.
    bool retail_direct(const std::filesystem::path& data, const std::filesystem::path& scratch)
    {
        CHECK(Testing::reset_scratch_dir(scratch));
        Config config;
        config.data_dir = data;
        Resources resources;
        std::string error;
        CHECK(resources.mount_game(config, error));
        const std::string path = "worlds/01_subwaya.dat";
        std::vector<std::uint8_t> bytes;
        CHECK(resources.read(path, bytes, error));
        auto placement = default_gi_placement(path, sha256(bytes));
        CHECK(save_gi_placement(scratch / "placement", placement, error));

        GiBakeRequest request;
        request.world = path;
        CHECK(make_gi_bake_settings("preview", 0, 0, -1, request.settings, error));
        request.settings.direct = GiDirectSource::Original;
        request.placement_dir = scratch / "placement";
        request.jobs = 2;
        request.out_dir = scratch / "original";
        const auto original = bake_gi_world(resources, request);
        CHECK(original.status == GiBakeResult::Status::Baked && original.direct_lights.lights == 0 &&
              original.direct_tests == 0);
        std::vector<std::uint8_t> original_bytes;
        GiFile original_file;
        CHECK(read_gi_bytes(path_text(original.file), original_bytes, error) &&
              read_gi_file(original_bytes, original_file, error));
        CHECK(!original_file.find(gi_format::direct) && original_file.settings.find("direct") == std::string::npos);

        request.settings.direct = GiDirectSource::Rebaked;
        std::vector<std::uint8_t> reference;
        GiBakeResult first;
        std::vector<std::string> stages;
        for (const unsigned jobs : {1u, 2u, 7u})
        {
            request.jobs = jobs;
            request.out_dir = scratch / ("rebaked" + std::to_string(jobs));
            if (jobs == 1)
                request.progress = [&](std::string_view stage, std::uint64_t, std::uint64_t)
                {
                    if (stages.empty() || stages.back() != stage)
                        stages.emplace_back(stage);
                };
            const auto result = bake_gi_world(resources, request);
            request.progress = {};
            CHECK(result.status == GiBakeResult::Status::Baked && result.exit_code == 0);
            CHECK(result.direct_lights.lights > 0 && result.direct_tests > 0 && result.direct_triangles > 0);
            std::vector<std::uint8_t> got;
            CHECK(read_gi_bytes(path_text(result.file), got, error));
            if (jobs == 1)
            {
                reference = got;
                first = result;
            }
            CHECK(got == reference);
        }
        // The direct pass runs before the bounce passes.
        const auto direct_stage = std::find(stages.begin(), stages.end(), "direct");
        const auto bounce_stage = std::find(stages.begin(), stages.end(), "bounce 1/1");
        CHECK(direct_stage != stages.end() && bounce_stage != stages.end() && direct_stage < bounce_stage);
        GiFile file;
        CHECK(read_gi_file(reference, file, error) && file.settings == gi_settings_text(request.settings));
        CHECK(file.settings.ends_with("\ndirect rebaked\n") && file.sections.size() == 6);
        CHECK(file.sections[1].tag == gi_format::lightmaps && file.sections[2].tag == gi_format::direct &&
              file.sections[3].tag == gi_format::vertices && file.sections[5].tag == gi_format::reflections &&
              file.key.algorithm == 4);
        GiLightmaps maps;
        GiDirectSection direct;
        CHECK(read_gi_lightmaps(file.find(gi_format::lightmaps)->payload, maps, error) &&
              read_gi_direct(file.find(gi_format::direct)->payload, direct, error) &&
              direct.texels.size() == maps.texels.size());
        // The re-bake changes the light the passes start from, so E differs; the placement does not.
        CHECK(file.find(gi_format::placement)->payload == original_file.find(gi_format::placement)->payload);
        CHECK(file.find(gi_format::lightmaps)->payload != original_file.find(gi_format::lightmaps)->payload);
        GiExpectation expected;
        CHECK(gi_expectation(resources, path, expected, error));
        unsigned stale = 0;
        CHECK(gi_status(file.key, expected, stale) == GiStatus::Ready);
        std::cout << "REBAKED bytes=" << reference.size() << " original=" << original_bytes.size()
                  << " lights=" << first.direct_lights.lights << " triangles=" << first.direct_triangles
                  << " tests=" << first.direct_tests << " kept=" << first.direct_kept
                  << " saturated=" << first.direct_saturated << " direct_ms=" << first.direct_ms << '\n';

        // --gi-if-stale: the original file is stale for a re-baked request, then current.
        request.if_stale = true;
        request.jobs = 2;
        request.out_dir = scratch / "original";
        CHECK(bake_gi_world(resources, request).status == GiBakeResult::Status::Baked);
        std::vector<std::uint8_t> replaced;
        CHECK(read_gi_bytes(path_text(original.file), replaced, error) && replaced == reference);
        CHECK(bake_gi_world(resources, request).status == GiBakeResult::Status::Current);
        // And the re-baked file is stale for an original request.
        auto original_request = request;
        original_request.settings.direct = GiDirectSource::Original;
        original_request.out_dir = scratch / "rebaked2";
        const auto restored = bake_gi_world(resources, original_request);
        CHECK(restored.status == GiBakeResult::Status::Baked);
        std::vector<std::uint8_t> back;
        CHECK(read_gi_bytes(path_text(restored.file), back, error) && back == original_bytes);

        // Cancelled during the direct pass: exit 12, the file kept.
        std::atomic<bool> cancel{false};
        auto cancelled_request = request;
        cancelled_request.if_stale = false;
        cancelled_request.out_dir = scratch / "rebaked1";
        cancelled_request.cancel = &cancel;
        std::string cancelled_at;
        cancelled_request.progress = [&](std::string_view stage, std::uint64_t, std::uint64_t)
        {
            if (stage == "direct" && !cancel.load())
            {
                cancelled_at = stage;
                cancel.store(true);
            }
        };
        const auto stopped = bake_gi_world(resources, cancelled_request);
        CHECK(stopped.status == GiBakeResult::Status::Cancelled && stopped.exit_code == 12 && cancelled_at == "direct");
        std::vector<std::uint8_t> kept;
        const auto relative = original.file.lexically_relative(scratch / "original");
        CHECK(read_gi_bytes(path_text(scratch / "rebaked1" / relative), kept, error) && kept == reference);

        // A placement change makes the re-baked file stale.
        placement.spacing = 160;
        CHECK(save_gi_placement(scratch / "placement", placement, error));
        request.out_dir = scratch / "rebaked7";
        const auto moved = bake_gi_world(resources, request);
        CHECK(moved.status == GiBakeResult::Status::Baked && moved.direct_lights.lights == first.direct_lights.lights);
        std::vector<std::uint8_t> moved_bytes;
        CHECK(read_gi_bytes(path_text(moved.file), moved_bytes, error) && moved_bytes != reference);
        CHECK(bake_gi_world(resources, request).status == GiBakeResult::Status::Current);
        return Testing::remove_scratch_dir(scratch);
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view name(argv[1]);
    bool ok = false;
    if (name == "bvh")
        ok = bvh();
    else if (name == "alpha")
        ok = alpha();
    else if (name == "albedo")
        ok = albedo();
    else if (name == "jobs")
        ok = jobs();
    else if (name == "settings")
        ok = settings();
    else if (name == "determinism")
        ok = determinism();
    else if (name == "retail" && argc == 5)
        ok = retail(path_from_utf8(argv[2]), path_from_utf8(argv[3]), path_from_utf8(argv[4]));
    else if (name == "retail-direct" && argc == 4)
        ok = retail_direct(path_from_utf8(argv[2]), path_from_utf8(argv[3]));
    else
        return 2;
    return ok ? 0 : 1;
}
