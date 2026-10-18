#include "engine/collision.h"
#include "game/original/AI_Mgr.h"
#include "game/server_adapter.h"
#include "model_fixture.h"
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
    bool near(float a, float b)
    {
        return std::abs(a - b) < .001f;
    }
    bool near(DVector a, DVector b)
    {
        return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
    }
    AI_Mgr* actor(GameSession& g)
    {
        static const ClassInfo type{"AITargetProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<AI_Mgr>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct state;
        state.m_ObjectType = OT_MODEL;
        SAFE_STRCPY(state.m_Name, "explicit target probe (no AI states)");
        auto h = g.objects.create(&type, state);
        return h ? dynamic_cast<AI_Mgr*>(g.objects.get(h)->game.get()) : nullptr;
    }
    HOBJECT target(GameSession& g, const char* name = "target", DVector position = {100, 0, 0})
    {
        ObjectCreateStruct state;
        SAFE_STRCPY(state.m_Name, name);
        state.m_Pos = position;
        return g.objects.create(g.objects.find_class("BaseClass"), state);
    }
    DVector forward(GameSession& g, HOBJECT h)
    {
        return rotate_vector({0, 0, 1}, g.objects.get(h)->state.m_Rotation);
    }
    bool named(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* ai = actor(g);
        auto first = target(g), second = target(g, "target", {0, 50, -100});
        CHECK(ai && first && second);
        g.server.trace_enabled = true;
        g.objects.get(second)->activity = OBJSTATE_INACTIVE;
        // Existing named lists prepend matches: newest slot is first, even hidden/inactive.
        CHECK(ai->SetTarget("target") && ai->GetTarget() == second && ai->Metacmd == 1);
        CHECK(near(forward(g, ai->m_hObject), {0, 0, -1}) && ai->target_position_valid);
        CHECK(g.server.list_count() == 0 && g.objects.link_count(ai->m_hObject) == 1);
        CHECK(ai->SetTarget("target") && ai->target_changes == 1 && ai->face_commands == 2);
        ai->target_name = "prefix_target";
        CHECK(ai->SetTarget(std::string_view(ai->target_name).substr(7)) && ai->GetTarget() == second);
        // Source no-op: no match keeps the previous target and is not a failure.
        const auto failures = ai->target_failures;
        CHECK(ai->SetTarget("Target") && ai->GetTarget() == second && ai->Metacmd == 1);
        CHECK(g.server.list_count() == 0 && ai->target_misses == 1 && ai->target_failures == failures &&
              ai->target_error.empty() && ai->target_name == "Target");
        CHECK(g.objects.remove(second));
        CHECK(ai->SetTarget("target") && ai->GetTarget() == first && ai->target_changes == 2);
        CHECK(g.objects.finish_frame() && ai->GetTarget() == first && ai->target_link_breaks == 0);
        CHECK(near(forward(g, ai->m_hObject), {1, 0, 0}));
        auto coincident = target(g, "coincident", {});
        const auto commands = ai->face_commands;
        CHECK(coincident && !ai->SetTarget("coincident") && ai->GetTarget() == coincident);
        CHECK(ai->Metacmd == 1 && ai->face_commands == commands && g.server.list_count() == 0);
        for (const auto* service : {"FindNamedObjects", "RelinquishList", "CreateInterObjectLink",
                                    "BreakInterObjectLink", "GetObjectPos", "GetObjectRotation", "SetObjectRotation"})
        {
            CHECK(g.server.service_calls[service] > 0);
            std::cout << service << "=" << g.server.service_calls[service] << '\n';
        }
        return true;
    }
    bool cached(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* ai = actor(g);
        auto t = target(g);
        CHECK(ai && t);
        CHECK(ai->SetNewTarget(t) && !ai->target_position_valid);
        CHECK(!ai->MC_FaceTarget() && ai->Metacmd == 1);
        CHECK(ai->RefreshTargetPosition() && ai->MC_FaceTarget() && ai->Metacmd == 2);
        CHECK(near(forward(g, ai->m_hObject), {1, 0, 0}));
        g.objects.get(t)->state.m_Pos = {0, 0, -100};
        CHECK(ai->SetNewTarget(t) && ai->target_changes == 1);
        CHECK(ai->MC_FaceTarget() && near(forward(g, ai->m_hObject), {1, 0, 0}));
        CHECK(ai->RefreshTargetPosition() && ai->MC_FaceTarget() && near(forward(g, ai->m_hObject), {0, 0, -1}));
        CHECK(ai->SetNewTarget(nullptr) && !ai->GetTarget() && g.objects.link_count(ai->m_hObject) == 0);
        const auto commands = ai->face_commands;
        const auto rotation = g.objects.get(ai->m_hObject)->state.m_Rotation;
        CHECK(ai->MC_FaceTarget() && ai->Metacmd == 5 && ai->face_commands == commands);
        CHECK(near(rotate_vector({0, 0, 1}, rotation), forward(g, ai->m_hObject)));
        return true;
    }
    bool facing(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* ai = actor(g);
        CHECK(ai);
        auto h = ai->m_hObject;
        auto* r = g.objects.get(h);
        r->state.m_Pos = {50, 30, 70};
        r->activity = OBJSTATE_INACTIVE;
        const auto clock = r->animation;
        for (auto direction : {DVector{1, 0, 0}, DVector{0, 0, -1}, DVector{-1, 0, 0}, DVector{0, 0, 1}})
        {
            CHECK(ai->MC_FacePos(vector_add(r->state.m_Pos, direction)));
            CHECK(near(forward(g, h), direction) && near(r->state.m_Pos, {50, 30, 70}));
        }
        // A rolled observer rotates about local up; a world-Y LookAt is different.
        const float q = std::sqrt(.5f);
        DRotation roll{{0, 0, q}, q};
        CHECK(g.server.SetObjectRotation(h, &roll) == DE_OK);
        CHECK(ai->MC_FacePos({150, 30, 70}));
        CHECK(near(forward(g, h), {0, 1, 0}) && near(rotate_vector({0, 1, 0}, r->state.m_Rotation), {-1, 0, 0}));
        // Vertical displacement has atan2(0,0)==0, as in source, and remains finite.
        DRotation identity;
        CHECK(g.server.SetObjectRotation(h, &identity) == DE_OK);
        CHECK(ai->MC_FacePos({50, 130, 70}) && near(forward(g, h), {0, 0, 1}));
        CHECK(r->animation.time == clock.time && r->animation.revision == clock.revision && ai->model_keys == 0);
        CHECK(g.objects.link_count(h) == 0 && g.server.list_count() == 0);
        return true;
    }
    bool failures(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* ai = actor(g);
        auto t = target(g);
        auto other = target(g, "other");
        CHECK(ai && t && other && ai->SetTarget("target"));
        auto h = ai->m_hObject;
        const auto changes = ai->target_changes, faces = ai->face_commands;
        for (auto bad : {h, reinterpret_cast<HOBJECT>(std::uintptr_t(1))})
            CHECK(!ai->SetNewTarget(bad) && ai->GetTarget() == t);
        const auto link = g.server.CreateInterObjectLink;
        g.server.CreateInterObjectLink = [](HOBJECT, HOBJECT) -> DRESULT { return DE_ERROR; };
        CHECK(!ai->SetNewTarget(other) && ai->GetTarget() == t && g.objects.link_count(h) == 1);
        g.server.CreateInterObjectLink = link;
        for (auto name : {std::string(), std::string(101, 'x'), std::string("target\0ignored", 14)})
            CHECK(!ai->SetTarget(name) && ai->GetTarget() == t && g.server.list_count() == 0);
        auto list = g.server.FindNamedObjects;
        g.server.FindNamedObjects = [](const char*) -> ObjectList* { return nullptr; };
        CHECK(!ai->SetTarget("target") && ai->GetTarget() == t);
        g.server.FindNamedObjects = list;
        for (auto p : {DVector{}, DVector{std::numeric_limits<float>::infinity(), 0, 0}, DVector{0, NAN, 0}})
            CHECK(!ai->MC_FacePos(p) && ai->Metacmd == 1 && near(forward(g, h), {1, 0, 0}));
        ai->Metacmd = std::numeric_limits<int>::max();
        CHECK(!ai->MC_FaceTarget() && ai->Metacmd == std::numeric_limits<int>::max());
        CHECK(ai->target_changes == changes && ai->face_commands == faces);
        CHECK(g.objects.remove(t));
        CHECK(!ai->RefreshTargetPosition() && !ai->MC_FaceTarget() && !ai->SetNewTarget(t));
        CHECK(g.objects.finish_frame() && !ai->GetTarget() && ai->target_link_breaks == 1);
        CHECK(!ai->MC_FaceTarget()); // No target also rejects overflow.
        CHECK(!ai->SetNewTarget(t));
        CHECK(g.objects.remove(h) && !ai->SetNewTarget(other) && !ai->MC_FacePos({0, 0, 100}));
        return true;
    }
    bool services(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        auto* ai = actor(g);
        auto t = target(g);
        auto parent = target(g, "parent", {200, 0, 0});
        CHECK(ai && t && parent);
        DVector position{9, 9, 9}, offset{};
        DRotation identity;
        HATTACHMENT attachment = 0;
        CHECK(g.server.GetObjectPos(nullptr, &position) == DE_INVALIDPARAMS && near(position, {}));
        CHECK(g.server.GetObjectPos(t, nullptr) == DE_INVALIDPARAMS);
        auto& live_position = g.objects.get(t)->state.m_Pos;
        CHECK(g.server.GetObjectPos(t, &live_position) == DE_OK && near(live_position, {100, 0, 0}));
        auto model = std::make_shared<Model>();
        std::string error;
        CHECK(read_model(test_model::abc().b, *model, error));
        auto* record = g.objects.get(parent);
        record->state.m_ObjectType = OT_MODEL;
        record->model = model;
        CHECK(g.server.CreateAttachment(parent, t, "hand", &offset, &identity, &attachment) == DE_OK);
        CHECK(g.server.GetObjectPos(t, &position) == DE_OK && ai->SetNewTarget(t) && ai->RefreshTargetPosition());
        const auto snapshot = ai->m_vTargetPos;
        record->model.reset();
        CHECK(g.server.GetObjectPos(t, &position) == DE_ERROR && near(position, {}));
        CHECK(!ai->RefreshTargetPosition() && !ai->target_position_valid && near(ai->m_vTargetPos, snapshot));
        CHECK(!ai->CanSee(t) && ai->sight.outcome == AISightOutcome::Unavailable && ai->sight.rays == 0);
        record->model = model;
        CHECK(ai->RefreshTargetPosition() && ai->MC_FaceTarget());
        CHECK(g.server.RemoveAttachment(attachment) == DE_OK);
        // The rotation service rejects an attached observer. Do not increment Metacmd or claim a turn.
        CHECK(g.server.CreateAttachment(parent, ai->m_hObject, nullptr, &offset, &identity, &attachment) == DE_OK);
        const auto count = ai->face_commands;
        const auto meta = ai->Metacmd;
        CHECK(!ai->MC_FacePos({500, 0, 0}) && ai->face_commands == count && ai->Metacmd == meta);
        CHECK(ai->target_error.find("rotation service") != std::string::npos);
        CHECK(g.server.RemoveAttachment(attachment) == DE_OK);
        CHECK(ai->MC_FacePos({500, 0, 0}));
        return true;
    }
    bool lifetime(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        HOBJECT old = nullptr;
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            auto* ai = actor(g);
            auto t = target(g), extra = target(g, "extra");
            CHECK(ai && t && extra);
            CHECK(!g.objects.get(old) && ai->SetTarget("target"));
            old = ai->m_hObject;
            CHECK(g.objects.link(old, extra) && g.objects.remove(extra) && g.objects.finish_frame());
            CHECK(ai->GetTarget() == t && ai->target_link_breaks == 0);
            CHECK(g.objects.remove(t) && ai->GetTarget() == t); // Deferred callback, checked queries already fail.
            CHECK(!ai->MC_FaceTarget() && g.objects.finish_frame());
            CHECK(!ai->GetTarget() && !ai->target_position_valid && ai->target_link_breaks == 1);
            t = target(g);
            CHECK(t && ai->SetTarget("target"));
            CHECK(g.objects.remove(old) && g.objects.finish_frame());
            CHECK(g.objects.get(t) && g.objects.link_count(old) == 0); // Target never owned by observer.
            CHECK(g.unload_world() && g.server.list_count() == 0 && g.server.message_count() == 0);
        }
        return true;
    }
    struct CallbackAI : AI_Mgr
    {
        static inline HOBJECT replacement = nullptr;
        static inline bool called = false, removed = false, cleared = false;
        DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
        {
            auto result = AI_Mgr::EngineMessageFn(m, d, f);
            if (m == MID_LINKBROKEN && replacement)
            {
                called = true;
                cleared = !GetTarget();
                auto next = replacement;
                replacement = nullptr;
                if (SetNewTarget(next))
                {
                    GetServerDE()->RemoveObject(next);
                    removed = true;
                }
            }
            return result;
        }
    };
    bool callbacks(const char*)
    {
        GameSession g;
        CHECK(g.initialize());
        static const ClassInfo type{"AITargetCallbackProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<CallbackAI>(); },
                                    {}};
        CHECK(g.objects.register_class(type));
        auto h = g.objects.create(&type, {});
        auto t = target(g), next = target(g, "next");
        auto* ai = h ? dynamic_cast<CallbackAI*>(g.objects.get(h)->game.get()) : nullptr;
        CHECK(ai && t && next && ai->SetNewTarget(t));
        CallbackAI::replacement = next;
        CHECK(g.objects.remove(t) && g.objects.finish_frame());
        CHECK(CallbackAI::called && CallbackAI::cleared && CallbackAI::removed);
        CHECK(!ai->GetTarget() && ai->target_link_breaks == 2 && g.objects.link_count(h) == 0);
        CHECK(!g.objects.get(t) && !g.objects.get(next));
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        auto* ai = actor(g);
        auto t = target(g, "marker", {100, 25, 100});
        CHECK(ai && t);
        unsigned poses = 0;
        for (const char* name : {"m_civilian1", "f_civilian1", "labtech", "m_cultist1", "f_cultist1", "gideon"})
        {
            auto* r = g.objects.get(ai->m_hObject);
            CHECK(g.models.load("models/enemies/" + std::string(name) + ".abc", r->model, error));
            for (unsigned clip = 0; clip < r->model->animations.size(); ++clip)
            {
                r->animation.animation = clip;
                r->animation.time = r->model->animations[clip].duration * .5;
                auto before = r->animation;
                CHECK(ai->SetTarget("marker") && ai->GetTarget() == t && ai->Metacmd == 1);
                CHECK(near(forward(g, ai->m_hObject), {std::sqrt(.5f), 0, std::sqrt(.5f)}));
                CHECK(before.time == r->animation.time && before.revision == r->animation.revision &&
                      ai->model_keys == 0);
                DVector head;
                DRotation rot;
                CHECK(g.server.GetModelNodeTransform(ai->m_hObject, "head", &head, &rot));
                ++poses;
            }
        }
        CHECK(g.objects.link_count(ai->m_hObject) == 1 && g.server.list_count() == 0);
        std::cout << "Private character facing samples=" << poses << "; unchanged animation clocks/keys\n";
        return true;
    }
    bool subway(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(c, error));
        HOBJECT old = nullptr, old_player = nullptr;
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* player = g.server_shell.Player(g.local_client);
            auto* ai = actor(g);
            CHECK(player && ai);
            CHECK(!g.objects.get(old) && !g.objects.get(old_player));
            old = ai->m_hObject;
            old_player = player->m_hObject;
            auto* r = g.objects.get(old);
            CHECK(g.models.load("models/enemies/m_cultist1.abc", r->model, error));
            const auto pos = g.objects.get(old_player)->state.m_Pos;
            r->state.m_Pos = vector_add(pos, {-100, 0, 0});
            CHECK(ai->SetNewTarget(old_player) && ai->RefreshTargetPosition() && ai->MC_FaceTarget());
            CHECK(near(forward(g, old), {1, 0, 0}));
            CHECK(g.step(1. / 60));
            CHECK(g.leave_player() && !g.objects.get(old_player));
            CHECK(!ai->GetTarget() && ai->target_link_breaks == 1 && !g.objects.link_count(old));
            CHECK(g.enter_player(error));
            player = g.server_shell.Player(g.local_client);
            CHECK(player && player->m_hObject != old_player);
            CHECK(ai->SetNewTarget(player->m_hObject) && ai->RefreshTargetPosition() && ai->MC_FaceTarget());
            CHECK(g.unload_world() && !g.objects.get(old) && g.server.list_count() == 0 &&
                  g.server.string_count() == 0);
            std::cout << "Subway cycle=" << cycle << ": player leave/reentry clears/rebinds original target link\n";
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> tests{
        {"named", named},       {"cached", cached},     {"facing", facing},
        {"failures", failures}, {"lifetime", lifetime}, {"callbacks", callbacks},
        {"services", services}, {"catalog", catalog},   {"subway", subway}};
    auto t = tests.find(argv[1]);
    return t != tests.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
