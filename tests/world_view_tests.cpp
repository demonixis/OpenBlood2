#include "engine/collision.h"
#include "engine/container.h"
#include "engine/world_view.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    World fixture()
    {
        World w;
        WorldModel m;
        m.name = "primary";
        m.points = {{-2, -2, 4}, {2, -2, 4}, {2, 2, 4}, {-2, 2, 4}};
        m.planes = {{{0, 0, -1}, -4}};
        m.surfaces.resize(1);
        m.polygons = {{0, {0, 1, 2, 3, 0, 2, 3}, 4}};
        w.models.push_back(m);
        return w;
    }
    bool mesh_test()
    {
        auto w = fixture();
        WorldMesh mesh;
        std::string error;
        CHECK(mesh.build(w, error));
        CHECK(mesh.vertices.size() == 6 && mesh.ranges.size() == 1);
        CHECK(mesh.vertices[0].position.x == -2 && mesh.vertices[0].position.z == 4);
        w.models[0].surfaces[0].flags = 4;
        CHECK(mesh.build(w, error));
        CHECK(mesh.vertices.empty() && mesh.invisible == 1);
        w.models[0].surfaces[0].flags = 8 | 16;
        CHECK(mesh.build(w, error));
        CHECK(mesh.transparent == 1 && mesh.sky == 1);
        w.models[0].points[1] = w.models[0].points[0];
        CHECK(mesh.build(w, error));
        CHECK(mesh.degenerate == 1 && mesh.vertices.size() == 3);
        return true;
    }
    bool rejects()
    {
        WorldMesh mesh;
        std::string error;
        for (int i = 0; i < 7; ++i)
        {
            auto w = fixture();
            if (i == 0)
                w.models.clear();
            if (i == 1)
                w.models[0].polygons[0].points[2] = 99;
            if (i == 2)
                w.models[0].polygons[0].base_points = 2;
            if (i == 3)
                w.models[0].polygons[0].surface = 1;
            if (i == 4)
                w.models[0].surfaces[0].plane = 1;
            if (i == 5)
                w.models[0].planes[0].normal.x = std::numeric_limits<float>::infinity();
            if (i == 6)
                w.models[0].points[0].x = std::numeric_limits<float>::quiet_NaN();
            CHECK(!mesh.build(w, error));
            CHECK(!error.empty() && mesh.vertices.empty() && mesh.ranges.empty());
        }
        return true;
    }
    bool camera()
    {
        WorldViewUniform u;
        WorldDraw d;
        CHECK(world_view_uniform({10, 20, 30}, {}, 2, d, u));
        CHECK(u.eye[0] == 10 && u.right[0] == 1 && u.up[1] == 1 && u.forward[2] == 1 && u.projection[1] == 2);
        CHECK(std::abs(u.projection[2] + u.projection[3]) < 1e-6);
        CHECK(std::abs((65536 * u.projection[2] + u.projection[3]) / 65536 - 1) < 1e-6);
        auto q = rotation_from_euler({0, 1.57079632679f, 0});
        CHECK(world_view_uniform({}, q, 1, d, u));
        CHECK(std::abs(u.forward[0] - 1) < 1e-5 && std::abs(u.right[2] + 1) < 1e-5);
        CHECK(!world_view_uniform({}, q, 0, d, u));
        q.m_Spin = 5;
        CHECK(!world_view_uniform({}, q, 1, d, u));
        return true;
    }
    bool lifetime()
    {
        GameSession g;
        CHECK(g.initialize());
        auto w = fixture();
        w.models.push_back(w.models[0]);
        w.models[1].name = "moving";
        w.models[1].translation = {10, 20, 30};
        WorldMesh mesh;
        std::string error;
        CHECK(mesh.build(w, error));
        CHECK(mesh.vertices[6].position.x == -12 && mesh.vertices[6].position.z == -26);
        auto h = g.create_example("CameraSpot");
        CHECK(h);
        auto* r = g.objects.get(h);
        r->state.m_ObjectType = OT_WORLDMODEL;
        SAFE_STRCPY(r->state.m_Filename, "moving");
        r->state.m_Pos = {30, 40, 50};
        r->state.m_Rotation = rotation_from_euler({0, 1, 0});
        r->world_model = std::make_shared<ContainerShape>();
        r->state.m_Flags |= FLAG_VISIBLE;
        r->activity = OBJSTATE_INACTIVE;
        std::vector<WorldDraw> draws;
        WorldViewStats stats;
        CHECK(world_draws(mesh, g.objects, draws, stats, error));
        CHECK(draws.size() == 2 && stats.unbound == 0);
        CHECK(draws[1].position.x == 30 && draws[1].rotation.m_Spin == r->state.m_Rotation.m_Spin);
        r->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(world_draws(mesh, g.objects, draws, stats, error));
        CHECK(draws.size() == 1 && stats.hidden == 1);
        r->state.m_Flags |= FLAG_VISIBLE;
        mesh.ranges.push_back(mesh.ranges[1]);
        CHECK(!world_draws(mesh, g.objects, draws, stats, error));
        CHECK(draws.empty());
        mesh.ranges.pop_back();
        CHECK(g.objects.remove(h));
        CHECK(world_draws(mesh, g.objects, draws, stats, error));
        CHECK(draws.size() == 1 && stats.unbound == 1);
        CHECK(g.objects.finish_frame());
        CHECK(!g.objects.get(h));
        CHECK(world_draws(mesh, g.objects, draws, stats, error));
        CHECK(draws.size() == 1);
        return true;
    }
    bool private_world(const char* directory)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = directory;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        WorldMesh mesh;
        std::vector<WorldDraw> draws;
        WorldViewStats stats;
        auto revision = g.world_revision;
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error));
            CHECK(g.world_revision > revision);
            revision = g.world_revision;
            CHECK(mesh.build(g.world, error));
            CHECK(mesh.ranges.size() == 116 && mesh.vertices.size() > 1000);
            CHECK(g.enter_player(error));
            CHECK(g.step(1. / 60));
            CHECK(world_draws(mesh, g.objects, draws, stats, error));
            CHECK(stats.draws > 50 && stats.triangles > 1000);
            std::size_t posed = 0;
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                if (!r || !r->world_model || !(r->state.m_Flags & FLAG_VISIBLE))
                    continue;
                auto it = std::find_if(draws.begin(), draws.end(),
                                       [&](const auto& d) { return mesh.ranges[d.range].name == r->state.m_Filename; });
                CHECK(it != draws.end());
                const auto& range = mesh.ranges[it->range];
                if (!range.count)
                    continue;
                auto point = vector_add(it->position, rotate_vector(mesh.vertices[range.first].position, it->rotation));
                bool found = false;
                const auto offset = vector_subtract(r->state.m_Pos, r->world_model->origin);
                for (const auto& t : r->world_model->triangles)
                    for (auto p : t)
                    {
                        auto delta = vector_subtract(point, vector_add(p, offset));
                        if (vector_dot(delta, delta) < .01f)
                            found = true;
                    }
                CHECK(found);
                ++posed;
            }
            CHECK(posed > 50);
            auto time = g.objects.time();
            auto before = g.objects.snapshot().size();
            CHECK(world_draws(mesh, g.objects, draws, stats, error));
            CHECK(g.objects.time() == time && g.objects.snapshot().size() == before);
            std::cout << "SubwayA: vertices=" << mesh.vertices.size() << " draws=" << stats.draws
                      << " triangles=" << stats.triangles << " unbound=" << stats.unbound
                      << " invisible=" << mesh.invisible << " posed=" << posed << '\n';
            CHECK(g.unload_world());
            CHECK(g.world_revision > revision);
            revision = g.world_revision;
            CHECK(world_draws(mesh, g.objects, draws, stats, error));
            CHECK(draws.size() == 1); // CPU cache owns no object identity.
            mesh = {};
            CHECK(world_draws(mesh, g.objects, draws, stats, error));
            CHECK(draws.empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string_view c = argv[1];
    bool ok = c == "lifetime"              ? lifetime()
              : c == "mesh"                ? mesh_test()
              : c == "rejects"             ? rejects()
              : c == "camera"              ? camera()
              : c == "subway" && argc == 3 ? private_world(argv[2])
                                           : false;
    return ok ? 0 : 1;
}
