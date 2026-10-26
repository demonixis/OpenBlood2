#include "app/session_playback.h"
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
        // Wider than 4:3 keeps the 4:3 vertical field (Hor+); 4:3 and narrower keep 90 degrees across.
        CHECK(u.eye[0] == 10 && u.right[0] == 1 && u.up[1] == 1 && u.forward[2] == 1 &&
              std::abs(u.projection[0] - 2.f / 3) < 1e-6f && std::abs(u.projection[1] - 4.f / 3) < 1e-6f);
        CHECK(world_view_uniform({}, {}, 4.f / 3, d, u) && u.projection[0] == 1 &&
              std::abs(u.projection[1] - 4.f / 3) < 1e-6f);
        CHECK(world_view_uniform({}, {}, 1, d, u) && u.projection[0] == 1 && u.projection[1] == 1);
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
        w.models[0].surfaces[0].has_effect = true;
        w.models[0].surfaces[0].effect_name = "Pan";
        w.models[0].surfaces[0].effect_parameters = "0 32";
        w.models.push_back(w.models[0]);
        w.models[1].name = "moving";
        w.models[1].translation = {10, 20, 30};
        WorldMesh mesh;
        std::string error;
        CHECK(mesh.build(w, error));
        CHECK(mesh.vertices[6].position.x == -12 && mesh.vertices[6].position.z == -26);
        mesh.ranges[1].batches[0].translucent = true; // Synthetic alpha surface for identity/lifetime checks.
        std::vector<WorldAlphaBatch> order;
        auto h = g.create_example("CameraSpot");
        CHECK(h);
        auto* r = g.objects.get(h);
        r->state.m_ObjectType = OT_WORLDMODEL;
        SAFE_STRCPY(r->state.m_Filename, "moving");
        r->state.m_Pos = {30, 40, 50};
        r->state.m_Rotation = rotation_from_euler({0, 1, 0});
        r->world_model = std::make_shared<ContainerShape>();
        r->world_model_index = 1; // Bound by creation in file order; drawing reuses it.
        r->state.m_Flags |= FLAG_VISIBLE;
        r->activity = OBJSTATE_INACTIVE;
        std::vector<WorldDraw> draws;
        WorldViewStats stats;
        CHECK(world_draws(mesh, g.objects, draws, stats, error));
        CHECK(draws.size() == 2 && stats.unbound == 0);
        CHECK(world_alpha_order(mesh, draws, {}, {}, order, error) && order.size() == 1);
        CHECK(draws[1].position.x == 30 && draws[1].rotation.m_Spin == r->state.m_Rotation.m_Spin);
        r->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(world_draws(mesh, g.objects, draws, stats, error));
        CHECK(draws.size() == 1 && stats.hidden == 1 && mesh.effects.size() == 2);
        CHECK(world_alpha_order(mesh, draws, {}, {}, order, error) && order.empty());
        r->state.m_Flags |= FLAG_VISIBLE;
        // A later same-named model stays unbound: drawing reuses the creation binding.
        mesh.ranges.push_back(mesh.ranges[1]);
        CHECK(world_draws(mesh, g.objects, draws, stats, error) && draws.size() == 2 && stats.unbound == 1);
        r->world_model_index = 0; // A lost binding is never repaired by a name search.
        CHECK(!world_draws(mesh, g.objects, draws, stats, error) && draws.empty());
        r->world_model_index = 1;
        mesh.ranges.pop_back();
        CHECK(g.objects.remove(h));
        CHECK(world_draws(mesh, g.objects, draws, stats, error));
        CHECK(draws.size() == 1 && stats.unbound == 1 && mesh.effects.size() == 2);
        CHECK(world_alpha_order(mesh, draws, {}, {}, order, error) && order.empty());
        CHECK(g.objects.finish_frame());
        CHECK(!g.objects.get(h));
        CHECK(world_draws(mesh, g.objects, draws, stats, error));
        CHECK(draws.size() == 1);
        return true;
    }
    bool alpha_order()
    {
        WorldMesh mesh;
        mesh.vertices.resize(18);
        mesh.ranges = {{0, 12, {}, "two faces", {{0, 6, 0, true, {0, 0, 2}}, {6, 6, 0, true, {0, 0, 4}}}},
                       {12, 6, {}, "moving", {{12, 6, 0, true, {0, 0, 3}}}}};
        std::vector<WorldDraw> draws = {{0, {}, {}}, {1, {}, {}}};
        std::vector<WorldAlphaBatch> order;
        std::string error;
        CHECK(world_alpha_order(mesh, draws, {}, {}, order, error));
        CHECK(order.size() == 3 && order[0].batch == 1 && order[1].draw == 1 && order[2].batch == 0);
        draws[1].position.z = 3;
        CHECK(world_alpha_order(mesh, draws, {}, {}, order, error));
        CHECK(order[0].draw == 1 && order[0].depth == 6);
        draws[1].position = {};
        draws[1].rotation = rotation_from_euler({0, 3.14159265f, 0});
        CHECK(world_alpha_order(mesh, draws, {}, {}, order, error));
        CHECK(order.back().draw == 1 && order.back().depth < -2.99);
        CHECK(world_alpha_order(mesh, draws, {}, draws[1].rotation, order, error));
        CHECK(order[0].draw == 1);
        draws[1].rotation = {};
        draws[1].position.z = 1; // Equal depths retain deterministic source order.
        CHECK(world_alpha_order(mesh, draws, {}, {}, order, error));
        CHECK(order[0].draw == 0 && order[0].batch == 1 && order[1].draw == 1);
        mesh.ranges[1].batches[0].count = 7;
        CHECK(!world_alpha_order(mesh, draws, {}, {}, order, error) && order.empty());
        mesh.ranges[1].batches[0].count = 6;
        draws[1].range = 2;
        CHECK(!world_alpha_order(mesh, draws, {}, {}, order, error) && order.empty());
        draws.resize(1);
        CHECK(!world_alpha_order(mesh, draws, {std::numeric_limits<float>::quiet_NaN(), 0, 0}, {}, order, error));
        CHECK(order.empty());
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
            CHECK(mesh.build(g.world, error, &g.resources));
            CHECK(mesh.ranges.size() == 116 && mesh.vertices.size() > 1000);
            CHECK(mesh.transparent == 31 && mesh.blended == 31);
            CHECK(mesh.effects.size() == 60);
            std::size_t positive_q = 0, negative_q = 0, pan_batches = 0;
            for (const auto& effect : mesh.effects)
            {
                CHECK(effect.model == 0 && effect.error.empty() && effect.speed[0] == 0);
                CHECK(std::abs(effect.origin_velocity.x) < .001 && std::abs(effect.origin_velocity.y) < .001 &&
                      std::abs(effect.origin_velocity.z + 1024) < .001);
                if (effect.speed[1] == 1024)
                    ++positive_q;
                else if (effect.speed[1] == -1024)
                    ++negative_q;
            }
            for (const auto& range : mesh.ranges)
                for (const auto& batch : range.batches)
                    pan_batches += batch.uv_rate[0] != 0 || batch.uv_rate[1] != 0;
            CHECK(positive_q == 18 && negative_q == 42 && pan_batches > 0);
            CHECK(g.server.world_time() == 0);
            CHECK(step_app_session(g, 1.) && g.server.world_time() == 0); // Waiting for entry never advances Pan.
            std::cout << "SubwayA Pan: 60 effects, 42 negative/18 positive Q speeds; all origins move -Z at 1024/s; "
                      << pan_batches << " prepared batches\n";
            std::size_t glass_faces = 0;
            for (const auto& range : mesh.ranges)
                for (const auto& batch : range.batches)
                    if (batch.translucent)
                    {
                        ++glass_faces;
                        const auto& material = mesh.materials[batch.material];
                        CHECK(material.path == "textures/greys/dirtyglass.dtx" && material.error.empty());
                        CHECK(material.image.flags == 10 && material.image.width == 64 && material.image.height == 64);
                        for (std::size_t i = 3; i < material.image.rgba.size(); i += 4)
                            CHECK(material.image.rgba[i] >= 51 && material.image.rgba[i] <= 102);
                        // Authored flat-shaded glass carries baked light 128 on every corner.
                        CHECK(batch.count == 6 && mesh.vertices[batch.first].color.x == 128 / 255.f &&
                              mesh.vertices[batch.first + 5].color.z == 128 / 255.f);
                    }
            CHECK(glass_faces == 31);
            std::cout << "SubwayA glass: 31 faces bind textures/greys/dirtyglass.dtx; 64x64, flags=10, alpha=51..102\n";
            std::size_t decoded = 0, failed = 0;
            for (std::size_t i = 1; i < mesh.materials.size(); ++i)
                mesh.materials[i].error.empty() ? ++decoded : ++failed;
            std::cout << "Textures: decoded=" << decoded << " failed=" << failed << " bytes=" << mesh.texture_bytes
                      << '\n';
            CHECK(decoded > 50);
            for (const auto& material : mesh.materials)
                if (!material.error.empty())
                    std::cout << material.path << ": " << material.error << '\n';
            const auto source_origin = g.world.models[0].surfaces[mesh.effects[0].surface].origin;
            CHECK(g.enter_player(error));
            CHECK(g.step(1. / 60));
            std::array<float, 4> pan_offset{}, repeated{};
            CHECK(world_texture_offset({0, 8}, g.server.world_time(), pan_offset) && pan_offset[1] > .1f);
            CHECK(world_texture_offset({0, 8}, g.server.world_time(), repeated) && repeated == pan_offset);
            CHECK(g.world.models[0].surfaces[mesh.effects[0].surface].origin.z == source_origin.z);
            CHECK(world_draws(mesh, g.objects, draws, stats, error));
            CHECK(stats.draws > 50 && stats.triangles > 1000);
            std::vector<WorldAlphaBatch> order;
            CHECK(world_alpha_order(mesh, draws, {}, {}, order, error));
            CHECK(order.size() == 31);
            for (std::size_t i = 1; i < order.size(); ++i)
                CHECK(order[i - 1].depth >= order[i].depth);

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
    bool ok = c == "alpha_order"           ? alpha_order()
              : c == "lifetime"            ? lifetime()
              : c == "mesh"                ? mesh_test()
              : c == "rejects"             ? rejects()
              : c == "camera"              ? camera()
              : c == "subway" && argc == 3 ? private_world(argv[2])
                                           : false;
    return ok ? 0 : 1;
}
