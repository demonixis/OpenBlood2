#include "brush_fixture.h"
#include "engine/sky.h"
#include "game/original/SkyObjects.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool close(float a, float b)
    {
        return std::abs(a - b) < .001f;
    }
    HOBJECT model(GameSession& g, const std::string& name, int index, DVector dims = {})
    {
        auto m = test_brush::box(name, {-10, -10, 100}, {10, 10, 120});
        m.translation = {5, 0, 110};
        g.world.models.push_back(m);
        return g.objects.create(g.objects.find_class("DemoSkyWorldModel"), {}, Creation::World,
                                {{"Name", name}, {"Index", std::int32_t(index)}, {"SkyDims", dims}});
    }
    HOBJECT pointer(GameSession& g, const std::string& name, int index)
    {
        return g.objects.create(g.objects.find_class("SkyPointer"), {}, Creation::World,
                                {{"SkyObjectName", name}, {"Index", std::int32_t(index)}});
    }
    bool rules()
    {
        GameSession g;
        CHECK(g.initialize());
        g.world.models.emplace_back();
        auto h = model(g, "sky", 3, {100, 200, 300});
        CHECK(h);
        const auto* r = g.objects.get(h);
        CHECK(r && r->life == Life::Alive);
        CHECK(g.server.sky.objects[3] == r->id && g.server.sky.additions == 1 && g.server.sky.definitions == 1);
        CHECK(r->state.m_ObjectType == OT_WORLDMODEL && (r->state.m_Flags & FLAG_SKYOBJECT) &&
              (r->state.m_Flags & FLAG_FORCEOPTIMIZEOBJECT) && !r->scheduled);
        auto& d = g.server.sky.definition;
        CHECK(close(d.min.x, -95) && close(d.max.z, 410) && close(d.view_min.x, -5) && close(d.view_max.z, 140));
        auto second = model(g, "second", 0, {100, 0, 100});
        CHECK(second && g.server.sky.definitions == 1);
        CHECK(g.server.AddSkyObject(h, 9) && !g.server.sky.objects[9] && g.server.sky.additions == 2);
        CHECK(!g.server.AddSkyObject(second, 30));
        CHECK(!model(g, "bad", -1));
        CHECK(!model(g, "bad30", 30));
        const auto saved = d;
        auto invalid = d;
        invalid.min.x = std::numeric_limits<float>::quiet_NaN();
        CHECK(!g.server.SetSkyDefinition(invalid) && d.min.x == saved.min.x);
        CHECK(!g.objects.create(g.objects.find_class("SkyPointer"), {}, Creation::Restore));
        CHECK(model(g, "last", 29) && g.server.sky.objects[29]);
        return true;
    }
    bool pointers()
    {
        GameSession g;
        CHECK(g.initialize());
        g.world.models.emplace_back();
        // Target is created after the pointer; original lookup is deferred to first update.
        auto p = pointer(g, "target", 2);
        CHECK(p && !g.server.sky.objects[2]);
        auto h = model(g, "target", 1);
        CHECK(h);
        CHECK(g.server.sky.remove(g.objects, h));
        g.server.SetObjectFlags(h, FLAG_VISIBLE);
        CHECK(g.objects.advance(.0005) && g.objects.get(p));
        CHECK(g.objects.advance(.0006));
        g.objects.finish_frame();
        CHECK(!g.objects.get(p) && g.server.sky.objects[2] == g.objects.get(h)->id);
        CHECK(g.server.list_count() == 0 && (g.server.GetObjectFlags(h) & FLAG_SKYOBJECT));
        auto missing = pointer(g, "TARGET", 4);
        CHECK(missing);
        CHECK(g.objects.advance(.002));
        g.objects.finish_frame();
        CHECK(!g.objects.get(missing) && !g.server.sky.error.empty() && !g.server.unavailable_services.empty());
        CHECK(g.server.list_count() == 0 && !g.server.sky.objects[4]);
        auto other = model(g, "other", 5);
        CHECK(other);
        CHECK(g.server.AddSkyObject(other, 2) &&
              g.server.sky.objects[5] == g.objects.get(other)->id); // already registered: source no-op
        CHECK(g.server.sky.remove(g.objects, other));
        CHECK(!g.server.AddSkyObject(other, 2)); // new registration into occupied slot rejects
        CHECK(g.server.sky.objects[2] == g.objects.get(h)->id);
        auto plain = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(plain);
        CHECK(!g.server.AddSkyObject(plain, 6));
        return true;
    }
    bool projection()
    {
        SkyDefinition d{{-100, -100, -100}, {100, 100, 100}, {1, 2, 3}, {11, 22, 33}};
        DVector out;
        CHECK(sky_view_position(d, {-10, -20, -30}, {10, 20, 30}, {}, out));
        CHECK(out.x == 6 && out.y == 12 && out.z == 18);
        CHECK(sky_view_position(d, {-10, -20, -30}, {10, 20, 30}, {30, -60, 90}, out));
        CHECK(out.x == 21 && out.y == -18 && out.z == 63); // no clamp
        World world;
        world.models.push_back(test_brush::box("main", {-2, -2, -2}, {2, 2, 2}));
        world.models[0].surfaces[1].flags = 1u << 4;
        WorldMesh mesh;
        std::string error;
        CHECK(mesh.build(world, error) && mesh.sky == 1 && mesh.ranges[0].batches.size() == 3);
        CHECK(!mesh.ranges[0].batches[0].sky_portal && mesh.ranges[0].batches[1].sky_portal &&
              !mesh.ranges[0].batches[2].sky_portal);
        CHECK(!sky_view_position(d, {}, {0, 1, 1}, {}, out) && out.x == 0);
        CHECK(!sky_view_position(d, {}, {1, 1, 1}, {std::numeric_limits<float>::infinity(), 0, 0}, out));
        return true;
    }
    bool lifetime()
    {
        GameSession g;
        CHECK(g.initialize());
        g.world.min = {-500, -500, -500};
        g.world.max = {500, 500, 500};
        g.world.models.emplace_back();
        auto a = model(g, "a", 4), b = model(g, "b", 1);
        CHECK(a && b);
        WorldMesh mesh;
        std::string e;
        CHECK(mesh.build(g.world, e));
        std::vector<WorldDraw> draws;
        DVector eye;
        WorldViewStats stats;
        CHECK(world_draws(mesh, g.objects, draws, stats, e) && draws.size() == 1 && stats.unbound == 0);
        CHECK(g.server.sky.draws(g.world, mesh, g.objects, {}, draws, eye, e) && draws.size() == 2);
        CHECK(draws[0].range == 2 && draws[1].range == 1);
        g.server.SetObjectState(a, OBJSTATE_INACTIVE);
        CHECK(g.server.sky.draws(g.world, mesh, g.objects, {}, draws, eye, e) && draws.size() == 2);
        g.server.SetObjectFlags(a, g.server.GetObjectFlags(a) & ~FLAG_VISIBLE);
        CHECK(g.server.sky.draws(g.world, mesh, g.objects, {}, draws, eye, e) && draws.size() == 1);
        CHECK(g.objects.remove(b));
        CHECK(g.server.sky.draws(g.world, mesh, g.objects, {}, draws, eye, e) && draws.empty());
        g.objects.finish_frame();
        auto new_object = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(new_object);
        CHECK(g.server.sky.draws(g.world, mesh, g.objects, {}, draws, eye, e) && draws.empty());
        CHECK(g.unload_world() && !g.server.sky.defined && !g.server.sky.objects[1]);
        return true;
    }
    bool private_world(const char* path)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = path;
        std::string e;
        CHECK(g.resources.mount_game(c, e));
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_cityhub", e));
            unsigned created = 0, failed = 0, unknown = 0;
            for (const auto& entry : g.entries)
            {
                created += entry.state == EntryState::Created;
                failed += entry.state == EntryState::Failed;
                unknown += entry.state == EntryState::Unsupported;
            }
            CHECK(created == 452 && failed == 1 && unknown == 1);
            for (const auto& use : g.resource_uses)
                CHECK(use.path != "Clouds1Pointer" && use.path != "Moon1Pointer" && use.path != "Star1Pointer" &&
                      use.path != "BuildingsPointer");
            CHECK(g.server.sky.additions == 1 && g.server.sky.definitions == 1);
            CHECK(g.step(.02) && g.server.sky.additions == 5 && g.server.sky.error.empty());
            WorldMesh mesh;
            CHECK(mesh.build(g.world, e, &g.resources));
            std::vector<WorldDraw> draws;
            DVector eye;
            CHECK(g.server.sky.draws(g.world, mesh, g.objects, {}, draws, eye, e) && draws.size() == 5);
            const char* expected[]{"Stars1", "Clouds1", "Moon1", "SkyWorldModel", "Buildings"};
            for (unsigned i = 0; i < 5; ++i)
                CHECK(mesh.ranges[draws[i].range].name == expected[i]);
            CHECK(g.server.sky.draws(g.world, mesh, g.objects, vector_scale(vector_add(g.world.min, g.world.max), .5f),
                                     draws, eye, e));
            CHECK(close(eye.x, -1664) && close(eye.y, -2032) && close(eye.z, 2048));
            CHECK(g.server.list_count() == 0);
            std::size_t sky_triangles = 0, alpha = 0, portals = 0;
            for (const auto& d : draws)
                for (const auto& b : mesh.ranges[d.range].batches)
                {
                    sky_triangles += b.count / 3;
                    alpha += b.translucent;
                    portals += b.sky_portal;
                }
            std::cout << "CityHub sky: " << draws.size() << " objects, " << sky_triangles << " triangles, " << alpha
                      << " alpha batches, " << portals << " sky markers, eye " << eye.x << ',' << eye.y << ',' << eye.z
                      << '\n';
            CHECK(sky_triangles > 0 && alpha > 0 && portals == 0);
            for (const auto& d : draws)
                for (const auto& b : mesh.ranges[d.range].batches)
                    CHECK(mesh.materials[b.material].error.empty());
            CHECK(g.unload_world() && g.server.sky.additions == 0 && g.server.string_count() == 0 &&
                  g.server.list_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string mode = argv[1];
    return (mode == "rules"        ? rules()
            : mode == "pointers"   ? pointers()
            : mode == "projection" ? projection()
            : mode == "lifetime"   ? lifetime()
                                   : mode == "private" && argc == 3 && private_world(argv[2]))
               ? 0
               : 1;
}
