#include "brush_fixture.h"
#include "game/original/AI_Mgr.h"
#include "game/original/ExitHint.h"
#include "game/original/SmellHint.h"
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
    bool setup(GameSession& g)
    {
        CHECK(g.initialize());
        auto floor = test_brush::box("floor", {-1000, -20, -1000}, {1000, -10, 1000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        std::string error;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    AI_Mgr* actor(GameSession& g, DVector pos = {0, 10, 0}, bool cabal = true)
    {
        static const ClassInfo type{"AIPerceptionProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<AI_Mgr>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = pos;
        s.m_Flags = FLAG_SOLID;
        SAFE_STRCPY(s.m_Name, "perception probe (no AI states)");
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* ai = r ? dynamic_cast<AI_Mgr*>(r->game.get()) : nullptr;
        if (!ai)
            return nullptr;
        r->dimensions = {2, 5, 2};
        ai->m_bCabal = cabal;
        ai->m_damage.m_fHitPoints = ai->m_damage.m_fMaxHitPoints = 100;
        ai->m_damage.Init(h);
        return ai->InsertMe() ? ai : nullptr;
    }
    HOBJECT object(GameSession& g, const char* type, DVector pos, unsigned flags = 0)
    {
        ObjectCreateStruct s;
        s.m_Pos = pos;
        s.m_Flags = flags;
        return g.objects.create(g.objects.find_class(type), s);
    }
    // Explicit synthetic player fixture bypasses retail model/entry prerequisites.
    // Private tests below exercise the real registered player and original InitialUpdate.
    struct PlayerProbe : CPlayerObj
    {
        DDWORD EngineMessageFn(DDWORD m, void* d, DFLOAT f) override
        {
            return B2BaseClass::EngineMessageFn(m, d, f);
        }
    };
    CPlayerObj* player(GameSession& g, DVector pos)
    {
        static const ClassInfo type{"PerceptionPlayerFixture",
                                    g.objects.find_class("CPlayerObj"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<PlayerProbe>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        auto h = object(g, type.name.c_str(), pos, FLAG_SOLID);
        auto* r = g.objects.get(h);
        if (!r)
            return nullptr;
        r->dimensions = {2, 5, 2};
        auto* p = dynamic_cast<CPlayerObj*>(r->game.get());
        AI_Mgr::InsertPlayer(*p);
        return p;
    }
    bool factions(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto* ally = actor(g, {0, 10, 20});
        auto* enemy = actor(g, {20, 10, 50}, false);
        auto* p = player(g, {0, 10, 30});
        CHECK(a && ally && enemy && p && AI_Mgr::m_dwNumCabal == 3 && AI_Mgr::m_dwNumMonster == 2);
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 100) == p->m_hObject);
        CHECK(enemy->FindObjectInRadius(AIFindList::Enemies, 100) == p->m_hObject);
        CHECK(a->InsertMe() && AI_Mgr::m_dwNumCabal == 3);
        a->m_bCabal = false;
        CHECK(!a->InsertMe());
        a->m_bCabal = true;
        AI_Mgr::InsertPlayer(*p);
        CHECK(AI_Mgr::m_dwNumMonster == 2);
        CHECK(g.objects.remove(p->m_hObject));
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 100) == enemy->m_hObject);
        CHECK(g.objects.finish_frame() && AI_Mgr::m_dwNumMonster == 1);
        CHECK(g.unload_world() && AI_Mgr::m_dwNumCabal == 0 && AI_Mgr::m_dwNumMonster == 0);
        // Empty Monster list falls back to Cabal, including self: reject undefined normalize explicitly.
        a = actor(g);
        CHECK(a && !a->FindObjectInRadius(AIFindList::Enemies, 100) && !a->find.valid);
        return true;
    }
    bool selection(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto* far = actor(g, {0, 10, 100}, false);
        auto* first = actor(g, {-30, 10, 40}, false);
        auto* second = actor(g, {30, 10, 40}, false);
        CHECK(a && far && first && second);
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 100) == second->m_hObject); // prepend + tie
        g.objects.get(second->m_hObject)->activity = OBJSTATE_INACTIVE;
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 100) ==
              second->m_hObject); // list ignores activity/visible flag
        CHECK(!a->FindObjectInRadius(AIFindList::Enemies, 49) && a->find.valid);
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 50) == second->m_hObject); // inclusive query boundary
        CHECK(g.objects.remove(second->m_hObject));
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 100) == first->m_hObject);
        CHECK(g.objects.finish_frame());
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 100, FindSpecific,
                                    g.objects.find_class("AIPerceptionProbe")) == first->m_hObject);
        CHECK(!a->FindObjectInRadius(AIFindList::Enemies, 100, FindSpecific,
                                     g.objects.find_class("CBaseCharacter"))); // reversed source IsKindOf
        // Specific-object branch leaves fObjectDp at zero, so FaceObject doesn't reject a rear object.
        g.objects.get(first->m_hObject)->state.m_Pos = {0, 10, -30};
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 100, FindSpecific | FindFaceObject,
                                    g.objects.find_class("AIPerceptionProbe")) == first->m_hObject);
        CHECK(a->find.valid && !a->GetTarget() && g.objects.link_count(a->m_hObject) == 0);
        return true;
    }
    bool visibility(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto* p = player(g, {0, 10, 50});
        auto* behind = actor(g, {0, 10, -10}, false);
        CHECK(a && p && behind);
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 100, FindVisible | FindFaceObject) == p->m_hObject);
        p->m_bPowerupActivated[PU_INVISIBLE] = true;
        CHECK(!a->FindObjectInRadius(AIFindList::Enemies, 100, FindVisible | FindFaceObject) && a->find.valid &&
              a->find.rays == 0);
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 100, FindFaceObject) ==
              p->m_hObject); // sense bypasses invisibility
        p->m_bPowerupActivated[PU_INVISIBLE] = false;
        auto block = object(g, "BaseClass", {0, 10, 25}, FLAG_SOLID);
        CHECK(block);
        g.objects.get(block)->dimensions = {5, 20, 5};
        CHECK(!a->FindObjectInRadius(AIFindList::Enemies, 100, FindVisible | FindFaceObject) && a->find.valid &&
              a->find.rays == 1);
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 100) == behind->m_hObject); // no ray/FOV for sense
        CHECK(g.objects.remove(block) && g.objects.finish_frame());
        g.objects.get(p->m_hObject)->state.m_Pos = {50, 10, 0};
        CHECK(a->FindObjectInRadius(AIFindList::Enemies, 100, FindVisible | FindFaceObject) ==
              p->m_hObject); // exact side accepted
        return true;
    }
    bool hints(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto first = object(g, "ExitHint", {0, 10, 40}), second = object(g, "ExitHint", {0, 10, 40});
        auto smell1 = object(g, "SmellHint", {0, 10, 30}), smell2 = object(g, "SmellHint", {0, 10, 30});
        auto ammo = object(g, "BulletAmmoPU", {0, 10, 20});
        CHECK(a && first && second && smell1 && smell2 && ammo);
        CHECK(a->FindObjectInRadius(AIFindList::Exits, 100) == second);
        CHECK(a->FindObjectInRadius(AIFindList::Smells, 100) == smell1); // scents append oldest first
        CHECK(a->FindObjectInRadius(AIFindList::Ammo, 100, FindSpecific, g.objects.find_class("BulletAmmoPU")) == ammo);
        CHECK(!a->FindObjectInRadius(AIFindList::Ammo, 100, FindSpecific, g.objects.find_class("AmmoPickup")) &&
              a->find.valid);
        auto target = object(g, "BaseClass", {0, 10, -30});
        CHECK(a->SetNewTarget(target) && a->RefreshTargetPosition());
        CHECK(!a->FindObjectInRadius(AIFindList::Exits, 100, FindAvoidTarget) &&
              a->find.valid); // source uses observer-target, not reverse
        g.objects.get(target)->state.m_Pos = {0, 10, 30};
        CHECK(a->RefreshTargetPosition());
        CHECK(a->FindObjectInRadius(AIFindList::Exits, 100, FindAvoidTarget) == second);
        CHECK(g.objects.remove(second) && g.objects.remove(smell1));
        CHECK(a->FindObjectInRadius(AIFindList::Exits, 100) == first);
        CHECK(a->FindObjectInRadius(AIFindList::Smells, 100) == smell2);
        CHECK(g.step(20.1) && SmellHint::m_dwNumSmells == 0 && ExitHint::m_dwNumExits == 1);
        CHECK(g.unload_world() && ExitHint::m_dwNumExits == 0);
        return true;
    }
    bool stimuli(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto* p = player(g, {0, 10, 40});
        auto smell = object(g, "SmellHint", {10, 10, 0});
        CHECK(a && p && smell);
        a->m_fSeeingDist = a->m_fSmellingDist = a->m_fSensingDist = 100;
        a->m_damage.m_fHitPoints = 50;
        CHECK(a->ComputeStimuli() == 1 && a->GetTarget() == p->m_hObject);
        CHECK(near(a->m_fStimuli[0], .6f) && near(a->m_fStimuli[2], .9f) && near(a->m_fStimuli[3], .6f) &&
              near(a->m_fStimuli[5], .5f));
        CHECK(!a->target_position_valid); // acquisition does not invent AI_Update's target sample
        p->m_bPowerupActivated[PU_INVISIBLE] = true;
        CHECK(a->ComputeStimuli() == 4 && a->GetTarget() == p->m_hObject);
        a->m_fSensingDist = 0;
        CHECK(a->ComputeStimuli() == 3 && !a->GetTarget()); // smell isn't a combat target/link
        CHECK(a->stimulus_queries[1].selected == smell && g.objects.link_count(a->m_hObject) == 0);
        a->m_fSmellingDist = 0;
        CHECK(a->ComputeStimuli() == 0 && !a->GetTarget());
        // Range query uses head; strength uses body. A boundary sight overwrites a positive sense target.
        p->m_bPowerupActivated[PU_INVISIBLE] = false;
        a->m_fSeeingDist = 40;
        a->m_fSensingDist = 100;
        CHECK(a->ComputeStimuli() == 0 && !a->GetTarget() && near(a->m_fStimuli[3], .6f));
        a->m_fSeeingDist = 100;
        p->m_damage.m_bDead = true;
        auto* other = actor(g, {20, 10, 60}, false);
        CHECK(other);
        CHECK(a->ComputeStimuli() == 0 && !a->GetTarget()); // don't retry farther living enemy
        CHECK(a->perception_failures == 0);
        return true;
    }
    bool damage(GameSession& g, AI_Mgr* a, HOBJECT attacker)
    {
        auto msg = g.server.StartMessageToObject(g.server.HandleToObject(attacker), a->m_hObject, damage_message);
        CHECK(msg && g.server.WriteDamage(msg, {{}, {}, 1, DamageType::Normal, attacker}) == DE_OK);
        g.server.EndMessage(msg);
        return true;
    }
    bool retaliation(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto* p = player(g, {0, 10, -500});
        CHECK(a && p);
        a->m_fStimuli[0] = .7f;
        a->m_fSeeingDist = 1;
        CHECK(damage(g, a, p->m_hObject) && a->m_damage.GetLastDamager() == p->m_hObject);
        CHECK(a->ComputeStimuli() == 1 && a->from_damage && a->GetTarget() == p->m_hObject &&
              near(a->m_fStimuli[0], .7f));
        CHECK(!a->m_damage.GetLastDamager() && g.objects.link_count(a->m_hObject) == 1);
        CHECK(damage(g, a, p->m_hObject));
        p->m_damage.m_bDead = true;
        CHECK(a->ComputeStimuli() == 0 && a->GetTarget() == p->m_hObject &&
              a->m_damage.GetLastDamager() == p->m_hObject);
        auto noncharacter = object(g, "BaseClass", {0, 10, 20});
        CHECK(noncharacter && damage(g, a, noncharacter));
        CHECK(a->ComputeStimuli() == -1 && !a->m_damage.GetLastDamager() && a->GetTarget() == p->m_hObject);
        p->m_damage.m_bDead = false;
        CHECK(damage(g, a, p->m_hObject) && g.objects.remove(p->m_hObject));
        CHECK(!a->m_damage.GetLastDamager() && g.objects.finish_frame() && !a->GetTarget());
        return true;
    }
    bool opportunity(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto* p = player(g, {0, 10, 40});
        CHECK(a && p);
        a->m_fSeeingDist = 100;
        CHECK(a->SetNewTarget(p->m_hObject));
        CHECK(!a->ChooseOpportunityTarget() && a->GetTarget() == p->m_hObject); // no weapon, no clear
        CHECK(a->m_InventoryMgr.Init(a->m_hObject, nullptr));
        CHECK(a->m_InventoryMgr.ObtainWeapon(1, 0) == InventoryResult::Ok);
        CHECK(a->ChooseOpportunityTarget() && a->GetTarget() == p->m_hObject && near(a->m_fStimuli[0], .6f));
        p->m_bPowerupActivated[PU_INVISIBLE] = true;
        CHECK(!a->ChooseOpportunityTarget() && !a->GetTarget() && a->perception_failures == 0);
        return true;
    }
    bool failures(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto* p = player(g, {0, 10, 40});
        CHECK(a && p);
        for (auto f : {NAN, INFINITY})
            CHECK(!a->FindObjectInRadius(AIFindList::Enemies, f) && !a->find.valid);
        CHECK(!a->FindObjectInRadius(AIFindList::Enemies, 0) && a->find.valid);
        CHECK(!a->FindObjectInRadius(AIFindList::Enemies, 100, 16) && !a->find.valid);
        CHECK(!a->FindObjectInRadius(AIFindList::Enemies, 100, FindSpecific | FindAvoidTarget,
                                     g.objects.find_class("CPlayerObj")) &&
              !a->find.valid);
        auto getter = g.server.GetObjectPos;
        g.server.GetObjectPos = [](HOBJECT, DVector*) -> DRESULT { return DE_ERROR; };
        CHECK(!a->FindObjectInRadius(AIFindList::Enemies, 100) && !a->find.valid);
        g.server.GetObjectPos = getter;
        a->m_fSeeingDist = 100;
        g.server.collision_ready = false;
        CHECK(a->ComputeStimuli() == -1 && !a->GetTarget());
        g.server.collision_ready = true;
        auto link = g.server.CreateInterObjectLink;
        g.server.CreateInterObjectLink = [](HOBJECT, HOBJECT) -> DRESULT { return DE_ERROR; };
        CHECK(a->ComputeStimuli() == -1 && !a->GetTarget());
        g.server.CreateInterObjectLink = link;
        a->m_damage.m_fHitPoints = std::numeric_limits<float>::max();
        a->m_damage.m_fMaxHitPoints = std::numeric_limits<float>::min();
        CHECK(a->ComputeStimuli() == -1);
        a->m_damage.m_fMaxHitPoints = 0;
        CHECK(a->ComputeStimuli() == -1);
        CHECK(g.objects.remove(a->m_hObject));
        CHECK(a->ComputeStimuli() == -1);
        CHECK(g.objects.finish_frame() && g.server.list_count() == 0);
        return true;
    }
    bool lifetime(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        HOBJECT old = nullptr;
        for (int i = 0; i < 4; ++i)
        {
            auto* a = actor(g);
            auto* p = player(g, {0, 10, 40});
            CHECK(a && p && !g.objects.get(old));
            a->m_fSeeingDist = 100;
            CHECK(a->ComputeStimuli() == 1);
            old = p->m_hObject;
            CHECK(damage(g, a, old));
            CHECK(g.objects.remove(old) && !a->m_damage.GetLastDamager());
            CHECK(g.objects.finish_frame() && !a->GetTarget() && a->target_link_breaks == 1);
            CHECK(g.unload_world() && AI_Mgr::m_dwNumCabal == 0 && AI_Mgr::m_dwNumMonster == 0 &&
                  SmellHint::m_dwNumSmells == 0);
            CHECK(g.server.list_count() == 0 && g.server.message_count() == 0 && g.server.string_count() == 0);
            // unload clears the collision service; next cycle rebuilds it without rebinding a second session.
            auto floor = test_brush::box("floor", {-100, -20, -100}, {100, -10, 100});
            for (auto& s : floor.surfaces)
                s.flags = 1;
            std::string error;
            CHECK(g.collision.build(floor, error));
            g.server.collision_ready = true;
        }
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(setup(g));
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        auto* a = actor(g);
        auto* p = player(g, {0, 10, 100});
        CHECK(a && p);
        unsigned samples = 0;
        for (const char* name : {"m_civilian1", "f_civilian1", "labtech", "m_cultist1", "f_cultist1", "gideon"})
        {
            auto* r = g.objects.get(a->m_hObject);
            CHECK(g.models.load("models/enemies/" + std::string(name) + ".abc", r->model, error));
            for (unsigned clip = 0; clip < r->model->animations.size(); ++clip)
            {
                r->animation.animation = clip;
                r->animation.time = r->model->animations[clip].duration * .5;
                const auto before = r->animation;
                CHECK(a->FindObjectInRadius(AIFindList::Enemies, 500, FindFaceObject) == p->m_hObject);
                CHECK(a->find.origin_head && a->find.valid && r->animation.time == before.time &&
                      r->animation.revision == before.revision && a->model_keys == 0);
                ++samples;
            }
        }
        std::cout << "Private NPC head-origin acquisition samples=" << samples << "; no animation advancement\n";
        return true;
    }
    bool subway(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        HOBJECT old = nullptr;
        for (int i = 0; i < 2; ++i)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error));
            auto* p = g.server_shell.Player(g.local_client);
            CHECK(p && !g.objects.get(old));
            auto pos = g.objects.get(p->m_hObject)->state.m_Pos;
            pos.x -= 50;
            auto* a = actor(g, pos);
            CHECK(a);
            auto* r = g.objects.get(a->m_hObject);
            CHECK(g.models.load("models/enemies/m_cultist1.abc", r->model, error));
            CHECK(AI_Mgr::m_dwNumCabal == 17 && AI_Mgr::m_dwNumMonster == 1);
            a->m_fSensingDist = 200;
            CHECK(a->ComputeStimuli() == 4 && a->GetTarget() == p->m_hObject);
            CHECK(a->RefreshTargetPosition() && a->MC_FaceTarget());
            a->m_fSeeingDist = 1500;
            const auto stim = a->ComputeStimuli();
            CHECK((stim == 1 || stim == 4) && a->GetTarget() == p->m_hObject);
            p->m_bPowerupActivated[PU_INVISIBLE] = true;
            CHECK(a->ComputeStimuli() == 4 && a->stimulus_queries[0].rays == 0);
            old = p->m_hObject;
            CHECK(g.leave_player() && !a->GetTarget() && AI_Mgr::m_dwNumMonster == 0);
            CHECK(g.enter_player(error));
            p = g.server_shell.Player(g.local_client);
            CHECK(p && p->m_hObject != old && a->ComputeStimuli() > 0 && a->GetTarget() == p->m_hObject);
            CHECK(g.unload_world() && AI_Mgr::m_dwNumMonster == 0 && AI_Mgr::m_dwNumCabal == 0 &&
                  g.server.string_count() == 0);
            std::cout << "Subway cycle=" << i
                      << ": original player enters both factions; acquisition/invisibility/leave/reentry checked\n";
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::map<std::string, std::function<bool(const char*)>> tests{{"factions", factions},
                                                                        {"selection", selection},
                                                                        {"visibility", visibility},
                                                                        {"hints", hints},
                                                                        {"stimuli", stimuli},
                                                                        {"retaliation", retaliation},
                                                                        {"opportunity", opportunity},
                                                                        {"failures", failures},
                                                                        {"lifetime", lifetime},
                                                                        {"catalog", catalog},
                                                                        {"subway", subway}};
    const auto t = tests.find(argv[1]);
    return t != tests.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
