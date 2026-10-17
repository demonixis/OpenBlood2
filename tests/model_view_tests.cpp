#include "app/model_scene.h"
#include "model_fixture.h"
#include "player_fixture.h"
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
    bool same_geometry(const ModelView& a, const ModelView& b)
    {
        CHECK(a.mesh.vertices.size() == b.mesh.vertices.size());
        CHECK(a.world_draws.size() == b.world_draws.size() && a.close_draws.size() == b.close_draws.size());
        CHECK(a.entries.size() == b.entries.size());
        for (std::size_t i = 0; i < a.mesh.vertices.size(); ++i)
        {
            const auto& x = a.mesh.vertices[i];
            const auto& y = b.mesh.vertices[i];
            CHECK(x.position.x == y.position.x && x.position.y == y.position.y && x.position.z == y.position.z);
            CHECK(x.color.x == y.color.x && x.color.y == y.color.y && x.color.z == y.color.z && x.uv == y.uv);
        }
        for (std::size_t i = 0; i < a.entries.size(); ++i)
        {
            CHECK(a.entries[i].error == b.entries[i].error && a.entries[i].range == b.entries[i].range);
            CHECK(a.entries[i].triangles == b.entries[i].triangles &&
                  a.entries[i].hidden_triangles == b.entries[i].hidden_triangles);
        }
        return true;
    }
    bool prepared_cache()
    {
        Model m;
        std::string e;
        CHECK(read_model(test_model::abc(false, false, 2, 2, -1).b, m, e));
        std::shared_ptr<const Model> model = std::make_shared<Model>(m);
        Resources resources;
        ModelView cached, reference;
        AnimationTrack track;
        for (const double time : {0., 50., 100., 1000.})
            for (const DVector scale : {DVector{1, 1, 1}, DVector{2, 3, 4}})
                for (const auto& mask : {std::vector<std::uint8_t>{}, std::vector<std::uint8_t>{0, 1}})
                    for (int repeat = 0; repeat < 2; ++repeat)
                    {
                        cached.begin_frame();
                        reference.begin_frame();
                        track.time = time;
                        const NodePose pose{{float(repeat * 100), 20, 30}, rotation_from_euler({0, float(repeat), 0})};
                        const float alpha = repeat ? .25f : 1;
                        CHECK(cached.add(model, track, "missing.dtx", pose, scale, true, repeat, {}, "cached",
                                         resources, e, mask, alpha));
                        CHECK(reference.add(model.get(), track, "missing.dtx", pose, scale, true, repeat, {},
                                            "reference", resources, e, mask, alpha));
                        CHECK(same_geometry(cached, reference));
                        CHECK(cached.preparation.reused == std::size_t(repeat));
                        CHECK(track.time == time && track.first && track.revision == 0);
                        if (!cached.close_draws.empty())
                            CHECK(cached.close_draws[0].alpha == alpha &&
                                  cached.close_draws[0].position.x == pose.position.x);
                    }
        for (const auto animation : {1u, 0u})
        {
            cached.begin_frame();
            reference.begin_frame();
            track.animation = animation;
            track.time = 0;
            CHECK(cached.add(model, track, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "clip", resources, e));
            CHECK(
                reference.add(model.get(), track, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "clip", resources, e));
            CHECK(!cached.entries[0].reused_geometry && same_geometry(cached, reference));
        }
        // A cache hit cannot bypass invalid current transforms/alpha or failed pose selection.
        cached.begin_frame();
        CHECK(!cached.add(model, track, "missing.dtx", {}, {0, 1, 1}, true, false, {}, "bad", resources, e));
        CHECK(cached.entries.empty() && cached.mesh.vertices.empty());
        track.animation = UINT32_MAX;
        CHECK(cached.add(model, track, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "bad", resources, e));
        CHECK(cached.entries.back().error == "Model animation pose is unavailable" && cached.mesh.vertices.empty());
        track.animation = 0;
        track.time = std::numeric_limits<double>::quiet_NaN();
        CHECK(cached.add(model, track, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "bad", resources, e));
        CHECK(cached.mesh.vertices.empty());
        track.time = 50;
        CHECK(cached.add(model, track, "missing.dtx", {}, {1, 1, 1}, false, false, {}, "hidden", resources, e));
        CHECK(cached.add(model, track, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "skin required", resources, e, {},
                         1, true));
        CHECK(cached.mesh.vertices.empty());
        // Releasing the last asset owner must work even while the inspector retains prepared geometry.
        std::weak_ptr<const Model> weak = model;
        model.reset();
        CHECK(weak.expired());
        cached.begin_frame();
        CHECK(cached.preparation.cached_assets == 0 && cached.preparation.cached_bytes == 0);
        // Simulate address reuse with a different control block, without a begin_frame prune.
        auto no_delete = [](const Model*) {};
        model = std::shared_ptr<const Model>(&m, no_delete);
        CHECK(cached.add(model, track, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "first", resources, e));
        model.reset();
        m.faces[0].uv[0][0] += .125f;
        model = std::shared_ptr<const Model>(&m, no_delete);
        CHECK(cached.add(model, track, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "replacement", resources, e));
        CHECK(!cached.entries.back().reused_geometry && cached.mesh.vertices.back().uv == m.faces[0].uv[2]);
        CHECK(cached.mesh.vertices[3].uv == m.faces[0].uv[0]);
        cached.clear();
        CHECK(cached.preparation.cached_assets == 0 && cached.preparation.cached_bytes == 0);
        return true;
    }
    bool cache_budget()
    {
        Model m;
        std::string e;
        CHECK(read_model(test_model::abc().b, m, e));
        Resources resources;
        ModelView scene;
        std::vector<std::shared_ptr<const Model>> models;
        for (std::size_t i = 0; i <= ModelView::cache_asset_limit; ++i)
        {
            models.push_back(std::make_shared<Model>(m));
            scene.begin_frame();
            CHECK(scene.add(models.back(), {}, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "small", resources, e));
            CHECK(scene.mesh.vertices.size() == 3);
        }
        CHECK(scene.preparation.cached_assets == ModelView::cache_asset_limit && scene.preparation.uncached == 1);
        models.clear();
        scene.begin_frame();
        CHECK(scene.preparation.cached_assets == 0);
        m.faces.resize(200000, m.faces.front());
        for (int i = 0; i < 2; ++i)
        {
            models.push_back(std::make_shared<Model>(m));
            scene.begin_frame();
            CHECK(scene.add(models.back(), {}, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "large", resources, e));
            CHECK(scene.mesh.vertices.size() == 600000);
        }
        CHECK(scene.preparation.cached_assets == 1 && scene.preparation.uncached == 1);
        CHECK(scene.preparation.cached_bytes <= ModelView::cache_vertex_limit * sizeof(WorldVertex));
        CHECK(!scene.add(models.back(), {}, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "frame overflow", resources,
                         e));
        CHECK(scene.mesh.vertices.empty());
        CHECK(scene.add(models.front(), {}, "missing.dtx", {}, {1, 1, 1}, true, false, {}, "all hidden", resources, e,
                        std::array<std::uint8_t, 2>{1, 1}));
        CHECK(scene.mesh.vertices.empty() && scene.preparation.cached_bytes == 600000 * sizeof(WorldVertex));
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
    bool builtin_resources(const char* root)
    {
        GameSession g;
        CHECK(test_player::setup(g, root, false));
        std::string error;
        ModelView scene;
        auto h = g.objects.create_props(g.objects.find_class("Model"), {},
                                        "Filename models/characters/caleb.abc; Skin skins/characters/caleb1.dtx");
        auto* record = g.objects.get(h);
        CHECK(record && record->model && record->model_error.empty());
        std::weak_ptr<const Model> retained = record->model;
        CHECK(update_model_scene(g, scene, error));
        CHECK(scene.entries.size() == 1 && !scene.entries[0].error.empty()); // Malformed synthetic DTX.
        CHECK(scene.world_draws.size() == 1);                                // Existing explicit magenta fallback.
        record->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(g.server.advance_models(.025));
        const auto time = record->animation.time;
        CHECK(time == 25 && update_model_scene(g, scene, error));
        CHECK(scene.entries.size() == 1 && !scene.entries[0].visible && scene.world_draws.empty());
        CHECK(record->animation.time == time); // Rendering is read-only even when hidden.
        auto bad = g.objects.create_props(g.objects.find_class("Model"), {},
                                          "Filename skins/characters/caleb1.dtx; Skin missing.dtx");
        auto* missing = g.objects.get(bad);
        CHECK(missing && !missing->model && !missing->model_error.empty());
        CHECK(update_model_scene(g, scene, error) && scene.entries.size() == 2 && !scene.entries.back().error.empty() &&
              scene.world_draws.empty());
        const auto id = record->id;
        CHECK(g.objects.remove(h) && update_model_scene(g, scene, error) && scene.entries.size() == 1);
        CHECK(!retained.expired() && g.objects.finish_frame() && retained.expired() && !g.objects.get(id));
        CHECK(g.objects.remove(bad) && g.objects.finish_frame());
        CHECK(update_model_scene(g, scene, error) && scene.entries.empty());
        return true;
    }
    bool builtin_city(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        ModelView scene;
        ObjectId old;
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_CityHub", error) && !g.objects.get(old));
            std::map<std::string, std::weak_ptr<const Model>> models;
            std::vector<ObjectId> ids;
            for (std::size_t i = 0; i < g.world.objects.size(); ++i)
            {
                const auto& source = g.world.objects[i];
                if (source.class_name != "Model")
                    continue;
                const auto& entry = g.entries[i];
                auto* r = g.objects.get(entry.object);
                CHECK(entry.state == EntryState::Created && r && r->model && r->model_error.empty());
                CHECK(r->state.m_ObjectType == OT_MODEL && r->state.m_Flags == FLAG_VISIBLE && !r->scheduled);
                CHECK(near(r->state.m_Pos, std::get<DVector>(*source.find("Pos"))));
                CHECK(std::string(r->state.m_Filename) == std::get<std::string>(*source.find("Filename")));
                CHECK(std::string(r->state.m_SkinName) == std::get<std::string>(*source.find("Skin")));
                CHECK(near(r->dimensions, {}) && !r->world_body && !r->character_body);
                auto [it, inserted] = models.emplace(r->state.m_Filename, r->model);
                CHECK(inserted || it->second.lock() == r->model);
                ids.push_back(r->id);
            }
            CHECK(ids.size() == 47 && models.size() == 2);
            CHECK(g.server.advance_models(.025)); // No NPC tick, exercise engine clocks without UI.
            CHECK(update_model_scene(g, scene, error));
            CHECK(scene.preparation.reused > 0);
            // A changed skin's failure color is geometry state; its material index is draw state.
            const auto* probe = g.objects.get(ids.front());
            ModelView skins, uncached;
            for (const auto* skin : {probe->state.m_SkinName, probe->state.m_SkinName, "missing.dtx", "missing.dtx",
                                     probe->state.m_SkinName})
            {
                skins.begin_frame();
                uncached.begin_frame();
                CHECK(skins.add(probe->model, probe->animation, skin, {}, {1, 1, 1}, true, false, {}, "skin",
                                g.resources, error));
                CHECK(uncached.add(probe->model.get(), probe->animation, skin, {}, {1, 1, 1}, true, false, {}, "skin",
                                   g.resources, error));
                CHECK(same_geometry(skins, uncached));
            }
            std::size_t triangles = 0;
            for (auto id : ids)
            {
                const auto* r = g.objects.get(id);
                const auto found = std::find_if(scene.entries.begin(), scene.entries.end(),
                                                [&](const auto& e) { return e.object == id; });
                CHECK(found != scene.entries.end() && found->visible && found->error.empty());
                CHECK(found->triangles == r->model->faces.size() && found->triangles > 0);
                CHECK(found->range < scene.world_draws.size());
                const auto& draw = scene.world_draws[found->range];
                CHECK(near(draw.position, r->state.m_Pos));
                CHECK(near(rotate_vector(draw.rotation, {0, 0, 1}), rotate_vector(r->state.m_Rotation, {0, 0, 1})));
                triangles += found->triangles;
            }
            std::cout << "CityHub cycle " << cycle << ": " << ids.size() << " built-in Models / " << models.size()
                      << " shared ABC assets / " << triangles << " triangles / zero model or skin failures\n";
            old = ids.front();
            auto* hidden = g.objects.get(old);
            const auto time = hidden->animation.time;
            hidden->state.m_Flags &= ~FLAG_VISIBLE;
            CHECK(update_model_scene(g, scene, error) && hidden->animation.time == time);
            const auto found = std::find_if(scene.entries.begin(), scene.entries.end(),
                                            [&](const auto& e) { return e.object == old; });
            CHECK(found != scene.entries.end() && !found->visible && found->error.empty());
            CHECK(g.unload_world() && !g.objects.get(old));
            for (const auto& [path, model] : models)
                CHECK(model.expired());
            scene.clear();
            CHECK(update_model_scene(g, scene, error) && scene.entries.empty() && scene.world_draws.empty());
        }
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
            CHECK(scene.world_draws.size() == 28 && scene.close_draws.empty());
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
            CHECK(scene.world_draws.size() == 27 &&
                  scene.close_draws.empty()); // 26 NPC draws (including Gideon) + independent player child.
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
    return (c == "legacy_pose"                      ? legacy_pose()
            : c == "builtin_resources" && argc == 3 ? builtin_resources(argv[2])
            : c == "builtin_city" && argc == 3      ? builtin_city(argv[2])
            : c == "prepared_cache"                 ? prepared_cache()
            : c == "cache_budget"                   ? cache_budget()
            : c == "sample"                         ? sample()
            : c == "rejects"                        ? rejects()
            : c == "weapon_pose"                    ? weapon_pose()
            : c == "subway" && argc == 3            ? private_world(argv[2])
                                                    : false)
               ? 0
               : 1;
}
