#include "app/model_scene.h"
#include "gideon_fixture.h"
#include <cmath>
#include <functional>
#include <limits>
#include <map>
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
    bool near(DVector a, DVector b)
    {
        return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z) < .003f;
    }
    bool setup(GameSession& g, const char* root)
    {
        std::string e;
        std::filesystem::create_directories(root);
        CHECK(g.initialize() && test_gideon::floor(g) && test_shield::mount(g.resources, root, e));
        return true;
    }
    bool colors(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        const auto h = a->m_hObject;
        DVector color;
        float alpha = -1;
        CHECK(g.server.GetObjectColor(h, color, alpha) && near(color, {0, 0, 0}) && alpha == 1);
        CHECK(g.server.SetObjectColor(h, {.2f, .4f, .6f}, .5f));
        for (float v : {-1.f, 1.01f, NAN, INFINITY})
        {
            CHECK(!g.server.SetObjectColor(h, {v, 1, 1}, .8f));
            CHECK(!g.server.SetObjectColor(h, {1, v, 1}, .8f));
            CHECK(!g.server.SetObjectColor(h, {1, 1, v}, .8f));
            CHECK(!g.server.SetObjectColor(h, {1, 1, 1}, v));
        }
        CHECK(g.server.GetObjectColor(h, color, alpha) && near(color, {.2f, .4f, .6f}) && alpha == .5f);
        auto normal = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(normal && !g.server.SetObjectColor(normal, {1, 1, 1}, 1));
        CHECK(!g.server.GetObjectColor(normal, color, alpha) && alpha == .5f);
        CHECK(g.objects.remove(h) && !g.server.SetObjectColor(h, {1, 1, 1}, 1));
        CHECK(!g.server.GetObjectColor(h, color, alpha) && alpha == .5f && g.objects.finish_frame());
        auto* b = test_gideon::actor(g);
        CHECK(b && b->m_hObject != h && g.server.GetObjectColor(b->m_hObject, color, alpha) && alpha == 1);
        return true;
    }
    bool fade(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        r->state.m_Flags |= FLAG_SOLID;
        CHECK(g.server.SetObjectColor(a->m_hObject, {.2f, .4f, .6f}, 1));
        const auto animation = r->animation.animation;
        CHECK(a->MC_FadeOut() && a->m_bAnimating && a->m_nCurMetacmd == 101);
        CHECK(r->alpha == 1 && !(r->state.m_Flags & FLAG_SOLID) && (r->state.m_Flags & FLAG_VISIBLE));
        for (int i = 1; i <= 16; ++i)
        {
            CHECK(a->MC_FadeOut() && r->alpha == 1 - float(i) / 16);
            CHECK(near(r->color, {.2f, .4f, .6f}) && r->animation.animation == animation);
            CHECK(a->Metacmd == (i == 16 ? 2 : 1) && a->m_bAnimating == (i != 16));
        }
        CHECK(a->fade_calls == 17);
        a->m_bAnimating = true;
        a->Metacmd = std::numeric_limits<int>::max();
        CHECK(g.server.SetObjectColor(a->m_hObject, {1, 1, 1}, .01f));
        CHECK(!a->MC_FadeOut() && r->alpha == .01f); // Completion overflow is atomic.
        a->Metacmd = 1;
        CHECK(a->MC_FadeOut() && r->alpha == 0 && a->Metacmd == 2);
        return true;
    }
    bool teleport(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a && a->SetNewState(AI_Mgr::StateTeleport));
        auto* r = g.objects.get(a->m_hObject);
        for (int i = 0; i < 17; ++i)
            CHECK(a->DispatchState());
        CHECK(a->Metacmd == 2 && r->alpha == 0);
        CHECK(a->DispatchState() && a->Metacmd == 3 && !a->teleport_sound_started && !a->teleport_sound_error.empty());
        // Missing WAV does not stall the original state; cache remains the old position.
        const auto cached = a->m_MoveObj.GetPos();
        DVector moved{50, 10, 20};
        CHECK(g.server.TeleportObject(a->m_hObject, &moved) == DE_OK);
        CHECK(a->DispatchState() && a->Metacmd == 4 && a->teleport_ray == SegmentResult::Miss);
        CHECK(near(a->teleport_from, cached));
        CHECK(near(a->teleport_to, vector_add(cached, vector_scale(a->teleport_direction, 1500))));
        CHECK(near(a->teleport_requested, a->teleport_to) && near(r->state.m_Pos, a->teleport_to));
        CHECK(r->alpha == 0 && (r->state.m_Flags & FLAG_SOLID) && a->teleport_moves == 1 && a->teleport_position_valid);
        CHECK(a->DispatchState() && a->Metacmd == 5 && r->alpha == 1);
        CHECK(a->DispatchState() && a->Metacmd == 6);
        CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateSearchVisual);
        return true;
    }
    bool special(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        ObjectCreateStruct cs;
        cs.m_Pos = {100, 10, 100};
        cs.m_Flags = FLAG_SOLID;
        auto target = g.objects.create(g.objects.find_class("BaseClass"), cs);
        g.objects.get(target)->dimensions = {5, 5, 5};
        CHECK(a->SetNewTarget(target) && a->SetNewState(AI_Mgr::StateSpecial1));
        for (int i = 0; i < 17; ++i)
            CHECK(a->DispatchState());
        CHECK(a->DispatchState() && a->Metacmd == 3 && a->AnimSound()->sound_attempts == 0);
        CHECK(a->DispatchState() && a->teleport_ray == SegmentResult::Miss && a->teleport_direction.y == .2f);
        CHECK(near(a->teleport_from, cs.m_Pos));
        float distance = (a->teleport_to.y - cs.m_Pos.y) / .2f;
        CHECK(distance >= 400 && distance <= 600 && near(a->teleport_requested, a->teleport_to));
        CHECK(a->DispatchState() && a->Metacmd == 5);
        CHECK(g.objects.remove(target) && g.objects.finish_frame() && !a->GetTarget());
        // Missing target changes state and starts fading in this same call.
        CHECK(a->SetNewState(AI_Mgr::StateSpecial1) && a->DispatchState());
        CHECK(a->m_nState == AI_Mgr::StateTeleport && a->Metacmd == 1 && a->m_bAnimating && a->m_nCurMetacmd == 101);
        CHECK(g.objects.get(a->m_hObject)->alpha == 1);
        return true;
    }
    bool obstruction(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        // Enclose the origin with six nonsolid surfaces: All must hit them.
        for (auto pos : {DVector{-50, 10, 0}, DVector{50, 10, 0}, DVector{0, 10, -50}, DVector{0, 10, 50}})
        {
            ObjectCreateStruct s;
            s.m_Pos = pos;
            auto h = g.objects.create(g.objects.find_class("BaseClass"), s);
            CHECK(h);
            g.objects.get(h)->dimensions = pos.x ? DVector{1, 100, 100} : DVector{100, 100, 1};
        }
        CHECK(a->SetNewState(AI_Mgr::StateTeleport));
        a->Metacmd = 3;
        CHECK(a->DispatchState() && a->teleport_ray == SegmentResult::Hit);
        // Hit point reconstructed from source offset must lie on a nonsolid wall entry face.
        const auto point = vector_add(a->teleport_requested, vector_scale(a->teleport_direction, 100));
        CHECK(std::abs(std::max(std::abs(point.x), std::abs(point.z)) - 49) < .01f);
        CHECK(near(a->teleport_actual, a->teleport_requested));
        DVector origin{0, 10, 0};
        CHECK(g.server.TeleportObject(a->m_hObject, &origin) == DE_OK);
        ObjectCreateStruct target_state;
        target_state.m_Pos = origin;
        auto target = g.objects.create(g.objects.find_class("BaseClass"), target_state);
        CHECK(target && a->SetNewTarget(target) && a->SetNewState(AI_Mgr::StateSpecial1));
        a->Metacmd = 3;
        CHECK(a->DispatchState() && a->teleport_ray == SegmentResult::Hit);
        const auto special_hit = vector_add(a->teleport_requested, vector_scale(a->teleport_direction, 300));
        CHECK(std::abs(std::max(std::abs(special_hit.x), std::abs(special_hit.z)) - 49) < .01f);
        CHECK(a->teleport_requested.y < -9 && a->teleport_actual.y >= -9.01f);
        CHECK(!near(a->teleport_requested, a->teleport_actual)); // MoveObject clips; this is not TeleportObject.
        a->Metacmd = 3;
        g.server.collision_ready = false;
        CHECK(!a->DispatchState() && a->Metacmd == 3 && !a->teleport_position_valid && !a->state_error.empty());
        CHECK((g.objects.get(a->m_hObject)->state.m_Flags & FLAG_SOLID) != 0); // Source mutation before failed query.
        return true;
    }
    struct Remover : BaseClass
    {
        inline static bool recursive_refused = false;
        DDWORD EngineMessageFn(DDWORD id, void* data, float f) override
        {
            if (id == MID_TOUCHNOTIFY)
            {
                if (auto* boss = dynamic_cast<Gideon*>(GetServerDE()->HandleToObject(static_cast<HOBJECT>(data))))
                    recursive_refused = !boss->AI_STATE_Teleport() && !boss->MC_FadeOut();
                GetServerDE()->RemoveObject(static_cast<HOBJECT>(data));
            }
            return BaseClass::EngineMessageFn(id, data, f);
        }
    };
    bool lifetime(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a && a->SetNewState(AI_Mgr::StateTeleport));
        auto h = a->m_hObject;
        static ClassInfo type{
            "TeleportRemover", nullptr, []() -> std::unique_ptr<BaseClass> { return std::make_unique<Remover>(); }, {}};
        CHECK(g.objects.register_class(type));
        // Enclosing nonsolid sensor is skipped by the ray, but contacted by MoveObject.
        ObjectCreateStruct s;
        s.m_Pos = {0, 10, 0};
        s.m_Flags = FLAG_TOUCH_NOTIFY;
        auto sensor = g.objects.create(&type, s);
        CHECK(sensor);
        g.objects.get(sensor)->dimensions = {10000, 100, 10000};
        a->Metacmd = 3;
        CHECK(!a->DispatchState() && a->teleport_moves == 0 && a->Metacmd == 3);
        CHECK(Remover::recursive_refused);
        CHECK(!g.server.IsObjectAlive(h) && !a->MC_FadeOut() && !a->DispatchState());
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        return true;
    }
    bool presentation(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = test_gideon::actor(g);
        CHECK(a);
        auto* r = g.objects.get(a->m_hObject);
        SAFE_STRCPY(r->state.m_SkinName, "alpha.dtx");
        ModelView scene;
        std::string e;
        CHECK(g.server.SetObjectColor(a->m_hObject, {.25f, .5f, .75f}, .5f));
        CHECK(update_model_scene(g, scene, e) && scene.entries.size() == 1 && scene.mesh.blended == 1);
        CHECK(scene.entries[0].error.empty() && scene.entries[0].alpha == .5f &&
              near(scene.mesh.vertices[0].color, {1, 1, 1}));
        CHECK(scene.mesh.ranges[0].batches[0].translucent && scene.world_draws[0].alpha == .5f);
        auto clock = r->animation.time;
        CHECK(update_model_scene(g, scene, e) && scene.mesh.blended == 1 && r->animation.time == clock);
        r->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(update_model_scene(g, scene, e) && scene.entries.size() == 1 && scene.world_draws.empty() &&
              scene.mesh.blended == 0);
        r->state.m_Flags |= FLAG_VISIBLE;
        CHECK(g.server.SetObjectColor(a->m_hObject, {1, 1, 1}, 1) && update_model_scene(g, scene, e));
        CHECK(scene.mesh.blended == 0 && !scene.mesh.ranges[0].batches[0].translucent);
        CHECK(g.objects.remove(a->m_hObject) && update_model_scene(g, scene, e) && scene.entries.empty());
        return true;
    }
    bool private_assets(const char* root)
    {
        GameSession g;
        std::string e;
        Config c;
        c.data_dir = root;
        CHECK(g.initialize() && g.resources.mount_game(c, e));
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            CHECK(test_gideon::floor(g));
            auto* a = test_gideon::actor(g, true, {0, 150, 0});
            CHECK(a && a->SetNewState(AI_Mgr::StateTeleport));
            const auto h = a->m_hObject;
            for (int i = 0; i < 17; ++i)
                CHECK(a->DispatchState());
            ModelView view;
            CHECK(update_model_scene(g, view, e) && view.entries[0].error.empty() && view.entries[0].alpha == 0);
            CHECK(a->DispatchState() && a->teleport_sound_started && a->teleport_sound_error.empty());
            CHECK(g.server.audio.voices().size() == 1);
            const auto& voice = g.server.audio.voices().begin()->second;
            CHECK(voice.clip && voice.clip->frames() > 0 && voice.auto_remove &&
                  voice.request.resource.ends_with("teleport.wav"));
            CHECK(a->DispatchState() && a->teleport_moves == 1 && a->DispatchState());
            CHECK(update_model_scene(g, view, e) && view.entries[0].alpha == 1 && view.mesh.blended == 0);
            std::cout << "External Gideon cycle " << cycle << ": " << view.entries[0].triangles
                      << " triangles, real teleport WAV, fade/move/reappear\n";
            CHECK(g.unload_world() && !g.objects.get(h));
            view.clear();
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> cases{{"colors", colors},
                                                                        {"fade", fade},
                                                                        {"teleport", teleport},
                                                                        {"special", special},
                                                                        {"obstruction", obstruction},
                                                                        {"lifetime", lifetime},
                                                                        {"presentation", presentation},
                                                                        {"private", private_assets}};
    auto test = cases.find(argv[1]);
    return test != cases.end() && test->second(argv[2]) ? 0 : 1;
}
