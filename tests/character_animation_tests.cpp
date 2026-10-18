#include "camera_fixture.h"
#include "game/original/Anim_Sound.h"
#include "game/server_adapter.h"
#include "model_fixture.h"
#include <algorithm>
#include <cmath>
#include <functional>
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
    bool near(float a, float b, float tolerance = .001f)
    {
        return std::abs(a - b) < tolerance;
    }
    HOBJECT bind(GameSession& game, std::shared_ptr<Model> model)
    {
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        auto object = game.objects.create(game.objects.find_class("BaseClass"), state);
        if (auto* r = game.objects.get(object))
            r->model = std::move(model);
        return object;
    }
    std::shared_ptr<Model> fixture()
    {
        auto model = std::make_shared<Model>();
        std::string error;
        if (!read_model(test_model::abc().b, *model, error))
            return {};
        return model;
    }
    void clips(Model& model, std::initializer_list<const char*> names)
    {
        const auto base = model.animations.front();
        model.animations.clear();
        for (auto name : names)
        {
            model.animations.push_back(base);
            model.animations.back().name = name;
        }
    }
    bool mappings(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto m = fixture();
        CHECK(m);
        clips(*m, {"idle1", "walk_rifle", "fire_stand_autorifle", "fire_stand_2pistol", "strafe_right_1pistol", "talk2",
                   "special4", "death_head1"});
        auto h = bind(g, m);
        CAnim_Sound a;
        CHECK(a.SetAnimationIndexes(h));
        CHECK(a.m_nAnim_IDLE[3] == 0 && a.m_nAnim_WALK[8] == 1);
        CHECK(a.m_nAnim_FIRE_STAND[0] == UINT32_MAX && a.m_nAnim_FIRE_STAND[1] == 2 && a.m_nAnim_FIRE_STAND[2] == 2);
        CHECK(a.m_nAnim_FIRE_STAND[4] == 3 && a.m_nAnim_FIRE_STAND[8] == 3);
        CHECK(a.m_nAnim_TALK[0] == UINT32_MAX && a.m_nAnim_TALK[1] == 5 && a.m_nAnim_TALK[2] == UINT32_MAX);
        CHECK(a.m_nAnim_SPECIAL[3] == 6 && a.m_nAnim_SPECIAL[4] == UINT32_MAX);
        CHECK(a.m_nAnim_DEATH[11] == 7 && a.m_nAnim_STRAFELEFT[2] == 4 && a.m_nAnim_STRAFERIGHT[4] == 4);
        CHECK(a.m_nAnim_STRAFELEFT[5] == 0 && a.m_nAnim_HUMILIATION[9] == 0); // Untouched source constructor slots.
        CHECK(a.m_nAnim_RUN[8] == UINT32_MAX);                                // No invented clip-zero fallback.
        DDWORD values[]{UINT32_MAX, 1000, 999, 1001, 2, UINT32_MAX};
        CheckAnims(values);
        CHECK(values[0] == UINT32_MAX && values[1] == UINT32_MAX && values[2] == 999 && values[3] == 999 &&
              values[5] == 2);
        CHECK(!a.SetAnimationIndexes(nullptr));
        std::cout << "animation bindings=" << a.InspectAnimations().size() << "\n";
        return true;
    }
    bool humiliation(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto m = fixture();
        CHECK(m);
        auto h = bind(g, m);
        CAnim_Sound a;
        clips(*m, {"humilation_01", "humilation_02", "humilation_03", "humilation_05", "humiliation_05"});
        CHECK(a.SetAnimationIndexes(h));
        CHECK(a.m_nAnim_HUMILIATION[0] == 0 && a.m_nAnim_HUMILIATION[1] == 1 && a.m_nAnim_HUMILIATION[2] == 2 &&
              a.m_nAnim_HUMILIATION[3] == 4);
        clips(*m, {"humiliation_01", "humiliation_03", "humiliation_05"});
        CHECK(a.SetAnimationIndexes(h));
        CHECK(a.m_nAnim_HUMILIATION[1] == 0 && a.m_nAnim_HUMILIATION[3] == 2);
        clips(*m, {"humiliation_01", "humiliation_04", "humiliation_05"});
        CHECK(a.SetAnimationIndexes(h));
        CHECK(a.m_nAnim_HUMILIATION[3] == 1);
        clips(*m, {"humilation_05"});
        CHECK(a.SetAnimationIndexes(h));
        CHECK(a.m_nAnim_HUMILIATION[3] == 0);
        return true;
    }
    bool force(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto m = fixture();
        CHECK(m);
        auto h = bind(g, m);
        auto* r = g.objects.get(h);
        CAnim_Sound a;
        r->animation.time = 40;
        const auto first = r->animation.revision;
        CHECK(a.ForceAnimation(h, 0) && r->animation.time == 40 && r->animation.revision == first);
        CHECK(a.ForceAnimation(h, 1));
        r->animation.time = 20;
        const auto before = r->animation.revision;
        CHECK(a.ForceAnimation(h, 1) && r->animation.time == 0 && r->animation.revision == before + 2);
        CHECK(!a.ForceAnimation(h, UINT32_MAX) && r->animation.revision == before + 2);
        CHECK(g.objects.remove(h) && !a.ForceAnimation(h, 0) && !a.SetAnimationIndexes(h));
        return true;
    }
    bool transforms(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto m = fixture();
        CHECK(m);
        auto h = bind(g, m);
        auto* r = g.objects.get(h);
        // Root rotates +90 around Z, child translates along its rotated local Y.
        for (auto& frame : m->animations[0].nodes[0].frames)
            frame.rotation = {{0, 0, std::sqrt(.5f)}, std::sqrt(.5f)};
        r->animation.time = 25;
        r->state.m_Pos = {100, 200, 300};
        r->state.m_Scale = {2, 3, 4};
        r->state.m_Rotation = {{0, std::sqrt(.5f), 0}, std::sqrt(.5f)};
        DVector p{9, 9, 9};
        DRotation q;
        CHECK(g.server.GetModelNodeTransform(h, "HAND", &p, &q));
        CHECK(near(p.x, 100) && near(p.y, 200) && near(p.z, 294)); // local root 5 minus child 2, scaled X=6, yaw -> -Z.
        auto axis = rotate_vector(q, {1, 0, 0});
        CHECK(near(axis.x, 0) && near(axis.y, 1) && near(axis.z, 0));
        std::vector<NodePose> all;
        NodePose one;
        for (double ms : {0., 25., 50., 99., 100., 1000.})
        {
            CHECK(sample_pose(*m, 0, ms, all));
            for (size_t i = 0; i < m->nodes.size(); ++i)
            {
                CHECK(sample_node_pose(*m, 0, ms, i, one));
                CHECK(near(one.position.x, all[i].position.x) && near(one.position.y, all[i].position.y));
            }
        }
        const auto revision = r->animation.revision;
        r->activity = OBJSTATE_INACTIVE;
        CHECK(g.server.GetModelNodeTransform(h, "hand", &p, &q) && r->animation.revision == revision &&
              r->animation.time == 25);
        CHECK(g.server.ResetModelAnimation(h) == DE_OK && g.server.GetModelNodeTransform(h, "hand", &p, &q) &&
              near(p.z, 304));
        return true;
    }
    bool rejection(const std::filesystem::path&)
    {
        for (float magnitude :
             {1e-5f, std::numeric_limits<float>::denorm_min(), 101.f, 1e20f, std::numeric_limits<float>::max()})
        {
            Model model;
            std::string error;
            NodePose pose;
            CHECK(read_model(test_model::abc(false, false, magnitude).b, model, error));
            CHECK(sample_node_pose(model, 0, 25, 1, pose) && near(pose.rotation.m_Spin, 1));
        }
        for (float magnitude : {0.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            Model model;
            std::string error;
            CHECK(!read_model(test_model::abc(false, false, magnitude).b, model, error) && model.nodes.empty());
        }

        GameSession g;
        CHECK(g.initialize());
        auto m = fixture();
        CHECK(m);
        auto h = bind(g, m);
        auto* r = g.objects.get(h);
        DVector p{1, 2, 3};
        DRotation q{{.1f, .2f, .3f}, .4f};
        CHECK(!g.server.GetModelNodeTransform(h, "absent", &p, &q) && p.x == 1 && q.m_Spin == .4f);
        CHECK(!g.server.GetModelNodeTransform(h, nullptr, &p, &q));
        CHECK(!g.server.GetModelNodeTransform(h, "hand", nullptr, &q));
        CHECK(!g.server.GetModelNodeTransform(h, "hand", &p, nullptr));
        DVector root_position, hand_position;
        DRotation root_rotation, hand_rotation;
        CHECK(g.server.GetModelNodeTransform(h, "root", &root_position, &root_rotation));
        CHECK(g.server.GetModelNodeTransform(h, "hand", &hand_position, &hand_rotation));
        m->nodes[0].name = "HAND"; // Duplicate: the first node in model order (LithTech FindNode).
        CHECK(g.server.GetModelNodeTransform(h, "hand", &p, &q) && p.x == root_position.x && p.y == root_position.y &&
              p.z == root_position.z);
        m->nodes[0].name = "root";
        r->animation.time = std::numeric_limits<double>::infinity();
        CHECK(!g.server.GetModelNodeTransform(h, "hand", &p, &q));
        r->animation.time = 0;
        r->state.m_Scale = {-1, 1, 1};
        CHECK(!g.server.GetModelNodeTransform(h, "hand", &p, &q));
        r->state.m_Scale = {1, 1, 1};
        r->state.m_Pos = {0, 1e7f, 0};
        CHECK(!g.server.GetModelNodeTransform(h, "hand", &p, &q));
        r->state.m_Pos = {};
        NodePose pose;
        m->nodes[1].parent = 1;
        CHECK(!sample_node_pose(*m, 0, 0, 1, pose));
        m->nodes[1].parent = 0;
        const auto old_id = r->id;
        CHECK(g.objects.remove(h) && !g.server.GetModelNodeTransform(h, "hand", &p, &q));
        CHECK(g.objects.finish_frame() && !g.objects.get(old_id));
        auto newer = bind(g, fixture());
        CHECK(newer != h);
        CHECK(!g.server.GetModelNodeTransform(h, "hand", &p, &q));
        CHECK(g.unload_world() && !g.server.GetModelNodeTransform(newer, "hand", &p, &q));
        return true;
    }
    bool spheres(const std::filesystem::path&)
    {
        GameSession g;
        CHECK(g.initialize());
        auto m = fixture();
        CHECK(m);
        const auto node = m->nodes.front();
        const auto track = m->animations[0].nodes[0];
        m->nodes.clear();
        for (auto& a : m->animations)
            a.nodes.clear();
        for (unsigned i = 0; i < NUM_STD_NODES; ++i)
        {
            m->nodes.push_back(node);
            auto& n = m->nodes.back();
            n.name = szNodes[i];
            n.parent = -1;
            n.index = i;
            for (auto& a : m->animations)
            {
                a.nodes.push_back(track);
                for (auto& f : a.nodes.back().frames)
                    f.position = {float(i * i), 0, 0};
            }
        }
        auto h = bind(g, m);
        CAnim_Sound a;
        CHECK(a.GenerateHitSpheres(h));
        const float expected[]{1, 1, 5, 5, 9, 11, 11, 15, 17, 17, 21, 23, 25, 25, 29, 31, 33, 33};
        for (unsigned i = 0; i < NUM_STD_NODES; ++i)
            CHECK(a.hit_nodes_found[i] && near(a.m_fHitSpheres[i], expected[i] * .66f));
        m->nodes[5].name = "missing";
        CHECK(a.GenerateHitSpheres(h) && !a.hit_nodes_found[5]);
        CHECK(a.m_fHitSpheres[4] == 0 && near(a.m_fHitSpheres[5], 20 * .66f) && !a.hit_sphere_error.empty());
        m->nodes[0].name = "missing-head";
        CHECK(!a.GenerateHitSpheres(h) && !a.hit_spheres_valid);
        for (float radius : a.m_fHitSpheres)
            CHECK(radius == 0);
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        CHECK(g.resources.mount_game(config, error));
        const char* paths[]{"models/characters/caleb.abc",    "models/enemies/m_civilian1.abc",
                            "models/enemies/f_civilian1.abc", "models/enemies/labtech.abc",
                            "models/enemies/m_cultist1.abc",  "models/enemies/f_cultist1.abc",
                            "models/enemies/gideon.abc"};
        for (const auto* path : paths)
        {
            ObjectCreateStruct state;
            state.m_ObjectType = OT_MODEL;
            SAFE_STRCPY(state.m_Filename, path);
            const auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
            auto* r = g.objects.get(h);
            if (r && !r->model)
                std::cerr << r->state.m_Filename << ": " << r->model_error << "\n";
            CHECK(r && r->model);
            CAnim_Sound a;
            CHECK(a.SetAnimationIndexes(h) && a.GenerateHitSpheres(h));
            const auto bindings = a.InspectAnimations();
            size_t missing = 0, queries = 0;
            for (auto b : bindings)
                missing += b.index == UINT32_MAX;
            for (std::uint32_t clip = 0; clip < r->model->animations.size(); ++clip)
            {
                g.server.SetModelAnimation(h, clip);
                r->animation.time = r->model->animations[clip].duration * .5;
                std::vector<NodePose> pose;
                CHECK(sample_pose(*r->model, clip, r->animation.time, pose));
                for (size_t i = 0; i < r->model->nodes.size(); ++i)
                {
                    DVector pos;
                    DRotation rotation;
                    CHECK(g.server.GetModelNodeTransform(h, r->model->nodes[i].name.c_str(), &pos, &rotation));
                    CHECK(near(pos.x, pose[i].position.x, .01f) && near(pos.y, pose[i].position.y, .01f) &&
                          near(pos.z, pose[i].position.z, .01f));
                    ++queries;
                }
            }
            std::cout << path << " clips=" << r->model->animations.size() << " nodes=" << r->model->nodes.size()
                      << " queries=" << queries << " missing_bindings=" << missing << " found_standard_nodes="
                      << std::count(a.hit_nodes_found.begin(), a.hit_nodes_found.end(), true)
                      << " head_radius=" << a.m_fHitSpheres[0] << " missing_nodes=";
            for (unsigned i = 0; i < NUM_STD_NODES; ++i)
                if (!a.hit_nodes_found[i])
                    std::cout << szNodes[i] << ',';
            std::cout << "\n";
            CHECK(g.objects.remove(h) && g.objects.finish_frame());
        }
        return true;
    }
    bool player(const std::filesystem::path& root)
    {
        GameSession g;
        std::string error;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        CHECK(g.resources.mount_game(config, error));
        HOBJECT previous = nullptr;
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            CHECK(test_camera::complete_subway_intro(g));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p && p->m_hObject != previous && p->m_Anim_Sound.hit_spheres_valid);
            CHECK(p->m_Anim_Sound.m_nAnim_WALK[2] == g.server.GetAnimIndex(p->m_hObject, "walk_rifle"));
            auto* r = g.objects.get(p->m_hObject);
            if (r && !r->model)
                std::cerr << r->state.m_Filename << ": " << r->model_error << "\n";
            CHECK(r && r->model);
            DVector initial, after;
            DRotation q;
            CHECK(g.server.GetModelNodeTransform(p->m_hObject, "head", &initial, &q));
            g.client_shell.movement_input.forward = true;
            for (unsigned i = 0; i < 12; ++i)
                CHECK(g.step(1. / 60));
            CHECK(p->locomotion_clip == "walk_rifle" &&
                  g.server.GetModelNodeTransform(p->m_hObject, "head", &after, &q));
            CHECK(std::hypot(after.x - initial.x, after.y - initial.y, after.z - initial.z) > 1);
            previous = p->m_hObject;
            CHECK(g.leave_player() && !g.server.GetModelNodeTransform(previous, "head", &after, &q));
            CHECK(g.unload_world() && g.server.string_count() == 0 && g.server.message_count() == 0);
        }
        std::cout << "Two real shell/player cycles: original tables, current world node transforms, cleanup verified\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"mappings", mappings},   {"humiliation", humiliation}, {"force", force},     {"transforms", transforms},
        {"rejection", rejection}, {"spheres", spheres},         {"catalog", catalog}, {"player", player}};
    auto i = cases.find(argv[1]);
    return i != cases.end() && i->second(argv[2]) ? 0 : 1;
}
