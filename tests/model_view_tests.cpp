#include "app/model_scene.h"
#include "model_fixture.h"
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
    bool legacy_pose()
    {
        for (int flags : {-1, 0, 1})
        {
            Model model;
            std::string error;
            const auto bytes = test_model::abc(false, false, 2, 2, flags);
            if (flags == 1)
            {
                CHECK(!read_model(bytes.b, model, error) && model.nodes.empty());
                continue;
            }
            CHECK(read_model(bytes.b, model, error));
            std::vector<NodePose> poses;
            CHECK(sample_pose(model, 0, 0, poses));
            const auto right = rotate_vector(poses[0].rotation, {1, 0, 0});
            const float sign = flags < 0 ? -1.f : 1.f;
            CHECK(std::abs(right.x) < .0001f && std::abs(right.y - sign) < .0001f);
            CHECK(std::abs(poses[1].position.x + 2 * sign) < .0001f);
            NodePose node;
            CHECK(sample_node_pose(model, 0, 0, 1, node));
            CHECK(std::abs(node.position.x - poses[1].position.x) < .0001f);
            std::vector<DVector> vertices;
            CHECK(sample_vertices(model, 0, 0, vertices));
            CHECK(std::abs(vertices[2].x - (-2 * sign - 2)) < .0001f);
            CHECK(std::abs(vertices[2].y + 2) < .0001f && std::abs(vertices[2].z - 6) < .0001f);
            CHECK(read_model(test_model::abc(false, false, 2, 0, flags, 2).b, model, error));
            CHECK(sample_pose(model, 0, 0, poses));
            const auto forward = rotate_vector(poses[0].rotation, {0, 0, 1});
            CHECK(std::abs(forward.x + sign) < .0001f && std::abs(forward.z) < .0001f);
        }
        return true;
    }
    bool near(DVector a, DVector b)
    {
        return std::abs(a.x - b.x) < .001f && std::abs(a.y - b.y) < .001f && std::abs(a.z - b.z) < .001f;
    }
    bool sample()
    {
        Model m;
        std::string e;
        CHECK(read_model(test_model::abc().b, m, e));
        Resources r;
        ModelView scene;
        AnimationTrack t;
        t.time = 50;
        NodePose pose{{100, 20, 30}, rotation_from_euler({0, 1, 0})};
        CHECK(scene.add(&m, t, "missing.dtx", pose, {2, 3, 4}, true, false, {}, "sample", r, e));
        CHECK(scene.entries.size() == 1 && !scene.entries[0].error.empty() && scene.world_draws.size() == 1);
        std::vector<DVector> sampled;
        CHECK(sample_vertices(m, 0, 50, sampled));
        CHECK(scene.mesh.vertices.size() == 3 && scene.mesh.vertices[1].uv == m.faces[0].uv[1]);
        for (unsigned i = 0; i < 3; ++i)
        {
            auto p = sampled[m.faces[0].vertices[i]];
            CHECK(near(scene.mesh.vertices[i].position, {p.x * 2, p.y * 3, p.z * 4}));
        }
        CHECK(near(scene.world_draws[0].position, pose.position));
        CHECK(t.time == 50 && t.first && t.revision == 0);
        auto revision = scene.revision;
        scene.begin_frame();
        CHECK(scene.add(&m, t, "MISSING.DTX", pose, {1, 1, 1}, false, false, {}, "hidden", r, e));
        CHECK(scene.mesh.vertices.empty() && scene.world_draws.empty() && scene.entries.size() == 1 &&
              !scene.entries[0].visible);
        CHECK(scene.revision == revision && scene.mesh.materials.size() == 2);
        scene.begin_frame();
        CHECK(scene.add(nullptr, t, "missing.dtx", pose, {1, 1, 1}, true, false, {}, "absent", r, e));
        CHECK(scene.world_draws.empty() && !scene.entries[0].error.empty());
        scene.clear();
        CHECK(scene.mesh.materials.size() == 1 && scene.revision > revision && scene.entries.empty());
        return true;
    }
    bool rejects()
    {
        Model m;
        std::string e;
        CHECK(read_model(test_model::abc().b, m, e));
        Resources r;
        ModelView scene;
        AnimationTrack t;
        for (int i = 0; i < 4; ++i)
        {
            auto bad = m;
            DVector scale{1, 1, 1};
            NodePose pose;
            if (i == 0)
                bad.faces[0].vertices[1] = 65000;
            if (i == 1)
                bad.faces[0].uv[0][0] = std::numeric_limits<float>::infinity();
            if (i == 2)
                scale.x = 0;
            if (i == 3)
                pose.rotation.m_Spin = 5;
            CHECK(!scene.add(&bad, t, "missing.dtx", pose, scale, true, false, {}, "invalid", r, e));
            CHECK(!e.empty() && scene.mesh.vertices.empty() && scene.world_draws.empty() && scene.entries.empty());
        }
        return true;
    }
    bool weapon_pose()
    {
        CViewWeapon weapon;
        weapon.type = WEAP_BERETTA;
        weapon.model = std::make_shared<Model>();
        NodePose pose;
        CHECK(weapon.AdjustAngleAndSetPos(0, 0, {10, 20, 30}, pose));
        CHECK(near(pose.position, {10.6f, 19.4f, 31.9f}));
        weapon.left = true;
        CHECK(weapon.AdjustAngleAndSetPos(0, 0, {10, 20, 30}, pose));
        CHECK(near(pose.position, {9.4f, 19.4f, 31.9f}));
        CHECK(weapon.AdjustAngleAndSetPos(0, 1.57079632679f, {10, 20, 30}, pose));
        CHECK(near(pose.position, {11.9f, 19.4f, 30.6f}));
        CHECK(!weapon.AdjustAngleAndSetPos(std::numeric_limits<float>::quiet_NaN(), 0, {}, pose));
        weapon.model.reset();
        CHECK(!weapon.AdjustAngleAndSetPos(0, 0, {}, pose));
        return true;
    }
    bool private_world(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string e;
        CHECK(g.resources.mount_game(c, e));
        ModelView scene;
        std::uint64_t revision = scene.revision;
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", e) && g.enter_player(e) && g.step(1. / 60));
            // File-authored oracle: the old unconjugated Beretta extended beyond
            // Z=8 while its static key ends at 4.24. No sampled golden is used.
            CHECK(g.client_shell.weapons[0].model);
            const auto& weapon_model = *g.client_shell.weapons[0].model;
            const auto rest = animation_index(weapon_model, "static_model");
            CHECK(rest != no_animation);
            std::vector<DVector> weapon_vertices;
            CHECK(sample_vertices(weapon_model, rest, 0, weapon_vertices));
            const auto& key = weapon_model.animations[rest].keys[0];
            for (const auto& v : weapon_vertices)
                CHECK(v.x >= key.min.x - .001f && v.x <= key.max.x + .001f && v.y >= key.min.y - .001f &&
                      v.y <= key.max.y + .001f && v.z >= key.min.z - .001f && v.z <= key.max.z + .001f);
            NodePose muzzle, hand;
            auto node_index = [&](std::string_view name)
            {
                for (std::size_t i = 0; i < weapon_model.nodes.size(); ++i)
                    if (weapon_model.nodes[i].name == name)
                        return i;
                return weapon_model.nodes.size();
            };
            CHECK(sample_node_pose(weapon_model, rest, 0, node_index("muzzle"), muzzle));
            CHECK(sample_node_pose(weapon_model, rest, 0, node_index("hand"), hand));
            CHECK(muzzle.position.z > hand.position.z + 1); // Barrel points along original firing +Z.
            const auto clock = g.objects.time();
            const auto ids = g.objects.snapshot();
            // Exercise both local visibility policies without changing source object flags.
            g.client_shell.player_visible = true;
            g.client_shell.weapons_visible = false;
            CHECK(update_model_scene(g, scene, e));
            CHECK(g.objects.time() == clock && g.objects.snapshot().size() == ids.size());
            std::cout << "Prepared world=" << scene.world_draws.size() << " close=" << scene.close_draws.size()
                      << " entries=" << scene.entries.size() << '\n';
            for (const auto& entry : scene.entries)
                std::cout << entry.label << " visible=" << entry.visible << " triangles=" << entry.triangles
                          << " error=" << entry.error << '\n';
            CHECK(scene.world_draws.size() == 2 && scene.close_draws.empty());
            std::size_t failures = 0;
            for (const auto& entry : scene.entries)
            {
                if (!entry.error.empty())
                {
                    ++failures;
                    std::cout << entry.label << ": " << entry.error << '\n';
                }
                if (entry.object && entry.visible && entry.range != UINT32_MAX)
                {
                    auto* record = g.objects.get(entry.object);
                    CHECK(record);
                    const auto& draw = scene.world_draws[entry.range];
                    CHECK(near(draw.position, record->state.m_Pos));
                }
            }
            std::cout << "Models: world=" << scene.world_draws.size() << " entries=" << scene.entries.size()
                      << " vertices=" << scene.mesh.vertices.size() << " diagnostics=" << failures
                      << " skin bytes=" << scene.mesh.texture_bytes << '\n';
            CHECK(failures == 0);
            g.client_shell.player_visible = false;
            g.client_shell.weapons_visible = true;
            CHECK(update_model_scene(g, scene, e));
            CHECK(scene.close_draws.size() == 1);
            CHECK(g.select_weapon(0, e) && g.step(1. / 60));
            g.client_shell.weapons_visible = true;
            CHECK(update_model_scene(g, scene, e) && scene.close_draws.size() == 1);
            auto* owner = g.server_shell.Player(g.local_client);
            CHECK(owner);
            const auto h = owner->m_hObject;
            const auto id = g.objects.get(h)->id;
            CHECK(g.objects.remove(h));
            CHECK(update_model_scene(g, scene, e));
            for (const auto& entry : scene.entries)
                CHECK(!(entry.object.slot == id.slot && entry.object.generation == id.generation));
            CHECK(scene.world_draws.size() == 1 && scene.close_draws.empty()); // Independent child until destructor.
            CHECK(g.unload_world());
            scene.clear();
            CHECK(scene.revision > revision);
            revision = scene.revision;
            CHECK(update_model_scene(g, scene, e) && scene.entries.empty() && scene.mesh.vertices.empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string_view c = argv[1];
    return (c == "legacy_pose"           ? legacy_pose()
            : c == "sample"              ? sample()
            : c == "rejects"             ? rejects()
            : c == "weapon_pose"         ? weapon_pose()
            : c == "subway" && argc == 3 ? private_world(argv[2])
                                         : false)
               ? 0
               : 1;
}
