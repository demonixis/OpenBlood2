#include "brush_fixture.h"
#include "game/original/CultistAI.h"
#include "game/server_adapter.h"
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
        return std::abs(a - b) < .003f;
    }
    class Probe : public AI_Mgr
    {
      public:
        unsigned computations = 0;
        bool ComputeState(int = -1) override
        {
            ++computations;
            return SetNewState(StateAttackClose);
        }
    };
    bool setup(GameSession& g, const char* root = "")
    {
        CHECK(g.initialize());
        auto floor = test_brush::box("floor", {-2000, -20, -2000}, {2000, -10, 2000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        std::string e;
        CHECK(g.collision.build(floor, e));
        g.server.collision_ready = true;
        if (*root)
        {
            Config c;
            c.data_dir = root;
            CHECK(g.resources.mount_game(c, e));
        }
        return true;
    }
    Probe* actor(GameSession& g, std::shared_ptr<Model>& model)
    {
        static const ClassInfo type{"HitProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = {0, 100, 0};
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* a = r ? dynamic_cast<Probe*>(r->game.get()) : nullptr;
        if (!a)
            return nullptr;
        model = std::make_shared<Model>();
        ModelAnimation animation;
        animation.name = "hit";
        animation.dimensions = {20, 20, 20};
        animation.duration = 100;
        animation.keys.resize(2);
        animation.keys[1].time = 100;
        for (unsigned i = 0; i < 18; ++i)
        {
            ModelNode n;
            n.name = szNodes[i];
            n.index = i;
            model->nodes.push_back(n);
            NodeAnimation node;
            node.frames = {{{float(i) * 4, 0, 0}, {}}, {{float(i) * 4, 0, 0}, {}}};
            animation.nodes.push_back(node);
        }
        for (int i = 0; i < 12; ++i)
            model->animations.push_back(animation);
        r->model = model;
        r->dimensions = {20, 20, 20};
        auto table = std::make_shared<CAnim_Sound>();
        table->hit_spheres_valid = true;
        table->m_fHitSpheres.fill(1);
        for (int i = 0; i < 12; ++i)
            table->m_nAnim_RECOIL[i] = i;
        if (!a->InitStatics(table) || !a->m_InventoryMgr.Init(h, nullptr) || !a->m_damage.Init(h, &a->m_InventoryMgr))
            return nullptr;
        a->m_damage.SetHitPoints(100);
        a->m_damage.m_fMaxHitPoints = 100;
        a->m_damage.ai_hit_localization = true;
        a->m_vDims = {20, 20, 20};
        a->m_nState = AI_Mgr::StateAttackClose;
        return a;
    }
    HOBJECT sender(GameSession& g)
    {
        ObjectCreateStruct s;
        s.m_Pos = {0, 100, 200};
        return g.objects.create(g.objects.find_class("BaseClass"), s);
    }
    Damage shot(HOBJECT from, int node, float amount = 10)
    {
        return {{0, 0, 1}, {float(node) * 4, 100, -1}, amount, DamageType::Normal, from};
    }
    bool send(GameSession& g, HOBJECT h, const Damage& d)
    {
        auto m = g.server.StartMessageToObject(g.server.HandleToObject(d.attacker), h, damage_message);
        CHECK(m && g.server.WriteDamage(m, d) == DE_OK && g.server.EndMessage(m) == DE_OK);
        return true;
    }
    bool geometry(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        std::shared_ptr<Model> m;
        auto* a = actor(g, m);
        CHECK(a);
        auto from = sender(g);
        for (int i = 0; i < 18; ++i)
        {
            CHECK(a->m_damage.LocalizeHit(shot(from, i), *a->AnimSound()));
            auto& h = a->m_damage.hit;
            CHECK(h.valid && !h.random_fallback && h.raw_node == i && near(h.distance, 0) && h.missing_nodes == 0);
            const int expected[] = {1, 1, 2, 2, 4, 4, 4, 7, 7, 7, 10, 10, 10, 10, 14, 14, 14, 14};
            CHECK(h.node == expected[i]);
        }
        CHECK(CDestructable::SetProperNode(18) == -1 && CDestructable::SetProperNode(-1) == -1);
        // Same distance retains the first node. Segment endpoints included; beyond endpoint misses.
        m->animations[0].nodes[3].frames = m->animations[0].nodes[2].frames;
        CHECK(a->m_damage.LocalizeHit(shot(from, 2), *a->AnimSound()) && a->m_damage.hit.raw_node == 2);
        auto d = shot(from, 2);
        d.position.z = -20;
        CHECK(a->m_damage.LocalizeHit(d, *a->AnimSound()) && !a->m_damage.hit.random_fallback);
        d.position.z = -21;
        CHECK(a->m_damage.LocalizeHit(d, *a->AnimSound()) && a->m_damage.hit.random_fallback);
        // Source tied axes form no segment; normal=0 still takes random fallback.
        d.direction = {1, 1, 0};
        CHECK(a->m_damage.LocalizeHit(d, *a->AnimSound()) && a->m_damage.hit.random_fallback);
        CHECK(vector_dot(a->m_damage.hit.segment, a->m_damage.hit.segment) == 0);
        d.direction = {};
        CHECK(a->m_damage.LocalizeHit(d, *a->AnimSound()) && a->m_damage.hit.random_fallback);
        // Query the current animation clock, not the sphere-generation pose.
        m->animations[0].nodes[2].frames[1].position.y = 8;
        g.objects.get(a->m_hObject)->animation.time = 50;
        d = shot(from, 2);
        d.position.y = 104;
        CHECK(a->m_damage.LocalizeHit(d, *a->AnimSound()) && a->m_damage.hit.raw_node == 2 &&
              !a->m_damage.hit.random_fallback);
        g.objects.get(a->m_hObject)->animation.time = 0;
        // Missing later nodes keep preceding pose, without masking invalid/ambiguous present poses.
        m->nodes[12].name = "missing";
        CHECK(a->m_damage.LocalizeHit(shot(from, 11), *a->AnimSound()) && a->m_damage.hit.missing_nodes == 1);
        m->nodes[0].name = "absent_head";
        CHECK(!a->m_damage.LocalizeHit(d, *a->AnimSound()) && !a->m_damage.hit.valid);
        return true;
    }
    bool hidden(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        std::shared_ptr<Model> m;
        auto* a = actor(g, m);
        CHECK(a);
        auto h = a->m_hObject;
        auto packet = shot(sender(g), 0);
        CHECK(a->m_damage.LocalizeHit(packet, *a->AnimSound()) && !a->m_damage.hit.random_fallback);
        CHECK(g.server.SetModelNodeHideStatus(h, "head", true) == DE_OK);
        CHECK(a->m_damage.LocalizeHit(packet, *a->AnimSound()) && a->m_damage.hit.random_fallback);
        CHECK(g.server.SetModelNodeHideStatus(h, "head", false) == DE_OK);
        CHECK(a->m_damage.LocalizeHit(packet, *a->AnimSound()) && a->m_damage.hit.raw_node == 0 &&
              !a->m_damage.hit.random_fallback);
        // Missing later names retain preceding hide status, just as they retain pose.
        CHECK(g.server.SetModelNodeHideStatus(h, "ll_leg", true) == DE_OK);
        m->nodes[12].name = "absent_ankle";
        CHECK(a->m_damage.LocalizeHit(shot(packet.attacker, 11), *a->AnimSound()) &&
              a->m_damage.hit.missing_nodes == 0);
        CHECK(g.server.SetModelNodeHideStatus(h, "ll_leg", false) == DE_OK);
        CHECK(a->m_damage.LocalizeHit(shot(packet.attacker, 11), *a->AnimSound()) &&
              a->m_damage.hit.missing_nodes == 1);
        // All hidden still reaches original random fallback, not artificial immunity.
        for (const auto& n : m->nodes)
            CHECK(g.server.SetModelNodeHideStatus(h, n.name.c_str(), true) == DE_OK);
        CHECK(a->m_damage.LocalizeHit(packet, *a->AnimSound()) && a->m_damage.hit.random_fallback);
        for (const auto& n : m->nodes)
            CHECK(g.server.SetModelNodeHideStatus(h, n.name.c_str(), false) == DE_OK);
        // A duplicate name resolves to the first node; an invalid pose still fails before health.
        m->nodes[1].name = "HEAD";
        CHECK(a->m_damage.LocalizeHit(packet, *a->AnimSound()) && a->m_damage.hit.valid);
        m->nodes[1].name = "neck";
        g.objects.get(h)->animation.animation = 99;
        CHECK(!a->m_damage.LocalizeHit(packet, *a->AnimSound()) && !a->m_damage.hit.valid);
        const auto hp = a->m_damage.m_fHitPoints;
        CHECK(send(g, h, packet) && a->m_damage.m_fHitPoints == hp && a->m_damage.accepted_damage == 0);
        return true;
    }
    bool query(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        std::shared_ptr<Model> m;
        auto* a = actor(g, m);
        CHECK(a);
        DVector pos{9, 8, 7};
        DRotation rot;
        auto h = a->m_hObject;
        CHECK(g.server.QueryModelNodeTransform(h, "missing", &pos, &rot) == ServerDE::NodeQuery::Missing && pos.x == 9);
        CHECK(g.server.QueryModelNodeTransform(h, "HEAD", &pos, &rot) == ServerDE::NodeQuery::Found && pos.y == 100);
        m->nodes[1].name = "HEAD"; // Duplicate: the first node in model order (LithTech FindNode).
        pos = {9, 8, 7};
        CHECK(g.server.QueryModelNodeTransform(h, "head", &pos, &rot) == ServerDE::NodeQuery::Found && pos.x == 0 &&
              pos.y == 100);
        m->nodes[1].name = "neck";
        g.objects.get(h)->animation.animation = 99;
        CHECK(!a->m_damage.LocalizeHit(shot(sender(g), 2), *a->AnimSound()));
        CHECK(g.objects.remove(h));
        CHECK(g.server.QueryModelNodeTransform(h, "head", &pos, &rot) == ServerDE::NodeQuery::Unavailable);
        return true;
    }
    bool modifiers(const char*)
    {
        for (int node : {2, 4, 10, 0})
        {
            GameSession g;
            CHECK(setup(g));
            std::shared_ptr<Model> m;
            auto* a = actor(g, m);
            CHECK(a);
            auto from = sender(g);
            a->m_damage.m_fArmorPoints = 20;
            auto packet = shot(from, node);
            if (node == 4)
                packet.type = DamageType::Fire;
            CHECK(send(g, a->m_hObject, packet));
            CHECK(a->m_damage.reaction_damage_type == packet.type);
            const float factor = node == 0 ? 1.5f : node == 2 ? 1.f : .5f;
            CHECK(near(a->m_damage.m_fArmorPoints, 20 - 10 * factor * .3f));
            CHECK(near(a->m_damage.m_fHitPoints, node == 0 ? 0 : 100 - 10 * factor * .7f));
            CHECK(a->m_damage.GetLastDamager() == from && a->m_damage.accepted_damage == 1 && a->damage_reactions == 1);
            CHECK(!a->runtime_failed);
            if (node == 0)
                CHECK(a->m_nState == AI_Mgr::StateRecoil && a->kill_voice_roll >= 0);
        }
        // Ordinary miss may draw 18 -> mapped -1, half damage, no reaction/state change.
        GameSession g;
        CHECK(setup(g));
        std::shared_ptr<Model> m;
        auto* a = actor(g, m);
        CHECK(a);
        auto from = sender(g);
        auto d = shot(from, 2, 2);
        d.direction = {};
        a->m_bCabal = false;
        bool found = false;
        for (int i = 0; i < 300 && !found; ++i)
        {
            a->m_damage.SetHitPoints(100);
            a->m_damage.m_fLastDamageAmount = 0;
            a->m_damage.m_fLastDamagePercent = 0;
            CHECK(send(g, a->m_hObject, d));
            found = a->m_damage.node_hit == -1;
        }
        CHECK(found && a->m_damage.hit.random_fallback && a->m_damage.hit.raw_node == 18 &&
              a->m_damage.m_fHitPoints == 99);
        a->m_bCabal = true;
        CHECK(send(g, a->m_hObject, shot(from, 0, 0)) && a->m_damage.IsDead() && !a->runtime_failed &&
              a->m_nState == AI_Mgr::StateRecoil);
        return true;
    }
    bool reactions(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        std::shared_ptr<Model> m;
        auto* a = actor(g, m);
        CHECK(a);
        auto from = sender(g);
        a->m_dwFlags = 4;
        a->m_damage.SetHitPoints(51);
        CHECK(send(g, a->m_hObject, shot(from, 10, 2)) && a->m_damage.m_fHitPoints == 50 && a->m_nInjuredLeg == 0);
        CHECK(send(g, a->m_hObject, shot(from, 10, 2)) && a->m_damage.m_fHitPoints == 49 && a->m_nInjuredLeg == 10);
        CHECK(send(g, a->m_hObject, shot(from, 14, 2)) && a->m_nInjuredLeg == 10);
        a->m_damage.SetHitPoints(100);
        a->m_damage.m_fLastDamageAmount = 0;
        a->m_damage.m_fLastDamagePercent = 0;
        CHECK(send(g, a->m_hObject, shot(from, 2, 19)) && a->m_nState == AI_Mgr::StateAttackClose);
        // Accumulator (20), compared against remaining HP (80), exact boundary recoils.
        CHECK(send(g, a->m_hObject, shot(from, 2, 1)) && a->m_nState == AI_Mgr::StateRecoil);
        CHECK(g.objects.engine_message(a->m_hObject, MID_UPDATE, nullptr) && a->m_damage.m_fLastDamageAmount == 0);
        a->m_nState = AI_Mgr::StateScript;
        a->m_dwScriptFlags = 0;
        a->m_dwFlags = 32;
        CHECK(send(g, a->m_hObject, shot(from, 2, 1)) && a->m_nState == AI_Mgr::StateScript);
        a->m_dwScriptFlags = 2;
        CHECK(send(g, a->m_hObject, shot(from, 2, 1)) && a->m_nState == AI_Mgr::StateRecoil);
        for (int state : {AI_Mgr::StateTeleport, AI_Mgr::StateDodge})
        {
            a->m_nState = state;
            a->m_dwFlags = 0;
            a->m_damage.m_fLastDamageAmount = 30;
            CHECK(send(g, a->m_hObject, shot(from, 2, 1)) && a->m_nState == AI_Mgr::StateAttackClose);
        }
        CHECK(a->computations == 2);
        return true;
    }
    bool recoil(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        std::shared_ptr<Model> m;
        auto* a = actor(g, m);
        CHECK(a);
        constexpr int nodes[]{1, 2, 4, 7, 10, 14};
        for (int side : {0, 6})
            for (int i = 0; i < 6; ++i)
            {
                a->m_damage.node_hit = nodes[i];
                a->m_damage.side_hit = side;
                CHECK(a->SetNewState(AI_Mgr::StateRecoil) && a->DispatchState());
                CHECK(a->recoil_slot == i + side && g.server.GetModelAnimation(a->m_hObject) == unsigned(i + side));
                CHECK(a->m_bAnimating && a->Metacmd == 1 && !g.server.GetModelLooping(a->m_hObject));
                CHECK(g.server.advance_models(.11) && a->DispatchState() && a->Metacmd == 2 && !a->m_bAnimating);
                CHECK(a->DispatchState() && a->m_nState == AI_Mgr::StateAttackClose);
            }
        a->m_damage.reaction_damage_type = DamageType::Fire;
        bool unmatched = false;
        for (int i = 0; i < 100 && !unmatched; ++i)
        {
            CHECK(a->SetNewState(AI_Mgr::StateRecoil));
            auto before = g.server.GetModelAnimation(a->m_hObject);
            CHECK(a->DispatchState());
            unmatched = a->recoil_slot == -1;
            if (unmatched)
                CHECK(g.server.GetModelAnimation(a->m_hObject) == before && a->m_nCurMetacmd == 19 && a->m_bAnimating);
        }
        CHECK(unmatched);
        a->Metacmd = std::numeric_limits<int>::max();
        g.objects.get(a->m_hObject)->animation.done = true;
        CHECK(!a->MC_Recoil());
        return true;
    }
    bool guards(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        std::shared_ptr<Model> m;
        auto* a = actor(g, m);
        CHECK(a);
        auto from = sender(g);
        a->m_damage.m_bGodMode = true;
        CHECK(send(g, a->m_hObject, shot(from, 2)) && a->damage_reactions == 0 && a->m_damage.hit_queries == 0);
        a->m_damage.m_bGodMode = false;
        a->m_damage.handling_damage = true;
        CHECK(send(g, a->m_hObject, shot(from, 2)) && a->damage_reactions == 0);
        a->m_damage.handling_damage = false;
        m->nodes[0].name = "missing";
        CHECK(send(g, a->m_hObject, shot(from, 2)) && a->runtime_failed && a->m_damage.m_fHitPoints == 100);
        CHECK(a->m_damage.accepted_damage == 0 && a->m_damage.hit_failures == 1);
        CHECK(g.server.message_count() == 0);
        return true;
    }
    class HitCallback : public B2BaseClass
    {
      public:
        std::function<void()> action;

      protected:
        DDWORD ObjectMessageFn(HOBJECT from, DDWORD message, HMESSAGEREAD read) override
        {
            if (message == 1000 && action)
                action();
            return B2BaseClass::ObjectMessageFn(from, message, read);
        }
    };
    bool lifetime(const char*)
    {
        for (bool remove : {false, true})
        {
            GameSession g;
            CHECK(setup(g));
            std::shared_ptr<Model> m;
            auto* a = actor(g, m);
            CHECK(a);
            static const ClassInfo type{"HitCallback",
                                        g.objects.find_class("BaseClass"),
                                        []() -> std::unique_ptr<BaseClass> { return std::make_unique<HitCallback>(); },
                                        {}};
            CHECK(g.objects.register_class(type));
            ObjectCreateStruct s;
            SAFE_STRCPY(s.m_Name, "hit_callback");
            auto callback = g.objects.create(&type, s);
            auto* c = dynamic_cast<HitCallback*>(g.server.HandleToObject(callback));
            CHECK(c);
            auto from = sender(g);
            auto h = a->m_hObject;
            bool nested = false;
            c->action = [&]
            {
                nested = send(g, h, shot(from, 2));
                if (remove)
                    g.server.RemoveObject(h);
            };
            a->m_damage.damage_target = "hit_callback";
            a->m_damage.damage_message = "TRIGGER";
            CHECK(send(g, h, shot(from, 2)) && nested);
            CHECK(a->m_damage.accepted_damage == 1 && a->m_damage.m_fHitPoints == 90 &&
                  a->damage_reactions == (remove ? 0 : 1));
            CHECK(g.server.IsObjectAlive(h) != remove && g.server.message_count() == 0 && g.server.string_count() == 0);
            CHECK(g.unload_world() && g.objects.snapshot().empty() && CDestructable::m_dwNumDestructables == 0);
        }
        return true;
    }
    bool sides(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        std::shared_ptr<Model> m;
        auto* a = actor(g, m);
        CHECK(a);
        auto from = sender(g);
        auto d = shot(from, 2); // facing +Z, incoming -Z, difference -pi is deliberately front.
        CHECK(a->m_damage.LocalizeHit(d, *a->AnimSound()) && a->m_damage.hit.side == 0);
        auto& rotation = g.objects.get(a->m_hObject)->state.m_Rotation;
        rotation = {{0, 1, 0}, 0}; // facing -Z
        d.direction = {0, 0, -1};
        CHECK(a->m_damage.LocalizeHit(d, *a->AnimSound()) && a->m_damage.hit.side == 6);
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        for (auto type : {"CultistAI", "CivilianAI"})
            for (bool male : {true, false})
            {
                auto h = g.objects.create(g.objects.find_class(type), {}, Creation::World,
                                          {{"Pos", DVector{0, 100, 0}},
                                           {"Male", male},
                                           {"AIState", std::string("GUARD")},
                                           {"RandomHitPoints", 1.f}});
                auto* a = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
                if (!a)
                    for (const auto& e : g.objects.diagnostics())
                        std::cerr << e << "\n";
                CHECK(a && a->RuntimeReady());
                auto from = sender(g);
                DVector torso;
                DRotation r;
                CHECK(g.server.GetModelNodeTransform(h, "torso", &torso, &r));
                Damage d{{0, 0, 1}, {torso.x, torso.y, torso.z - 1}, 1, DamageType::Normal, from};
                CHECK(a->m_damage.LocalizeHit(d, *a->AnimSound()));
                CHECK(a->m_damage.hit.node == 2 && !a->m_damage.hit.random_fallback &&
                      a->m_damage.hit.missing_nodes == 2);
                a->m_dwFlags |= 32; // Test stimulus forces source ALWAYSRECOIL; class/state logic remains original.
                CHECK(send(g, h, d) && a->RuntimeReady());
                if (type == std::string_view("CultistAI"))
                {
                    CHECK(a->m_nState == AI_Mgr::StateRecoil && a->AI_Update() && a->m_bAnimating &&
                          a->recoil_slot == 1);
                    CHECK(g.server.PingObjects(from));
                    for (int i = 0; i < 180 && a->Metacmd == 1; ++i)
                        CHECK(g.step(1.0 / 60));
                    std::cout << "recoil private error=" << a->runtime_error << " phase=" << a->Metacmd
                              << " finishes=" << a->animation_finishes << " updates=" << a->runtime_updates
                              << " clip=" << g.server.GetModelAnimation(h)
                              << " time=" << g.objects.get(h)->animation.time << "\n";
                    CHECK(a->RuntimeReady() && a->animation_finishes > 0);
                }
                else
                    CHECK(a->m_nState == AI_Mgr::StateEscapeRun); // source subclass override follows base recoil.
                std::cout << type << " male=" << male << " hit=" << a->m_damage.hit.node
                          << " missing=" << a->m_damage.hit.missing_nodes << " reactions=" << a->damage_reactions
                          << " recoil=" << a->recoil_starts << " hp=" << a->m_damage.m_fHitPoints << '\n';
                CHECK(g.objects.remove(h) && g.objects.remove(from) && g.objects.finish_frame());
                CHECK(g.objects.snapshot().empty() && g.server.message_count() == 0 && g.objects.attachments().empty());
            }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::map<std::string, std::function<bool(const char*)>> tests{
        {"hidden", hidden},       {"geometry", geometry}, {"query", query},   {"modifiers", modifiers},
        {"reactions", reactions}, {"recoil", recoil},     {"guards", guards}, {"lifetime", lifetime},
        {"sides", sides},         {"catalog", catalog}};
    if (argc < 2)
        return 2;
    auto t = tests.find(argv[1]);
    return t != tests.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
