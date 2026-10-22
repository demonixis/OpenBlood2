// Original SoulDrudge: constructor values, random weapon/skin, melee attack via fire keys.
#include "engine/collision.h"
#include "game/original/SoulDrudge.h"
#include "game/original/Weapon.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << "\n";                                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool setup(GameSession& g, const char* root)
    {
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        WorldModel floor;
        floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    bool step(GameSession& g, double dt)
    {
        CHECK(g.server.advance_models(dt) && g.server.advance_physics(dt) && g.objects.advance(dt) &&
              g.server.advance_projectiles(dt));
        return true;
    }
    SoulDrudge* drudge(GameSession& g, DVector at)
    {
        auto h = g.objects.create(g.objects.find_class("SoulDrudge"), {}, Creation::World,
                                  {{"Name", std::string("drudge")}, {"Pos", at}});
        auto* a = h ? dynamic_cast<SoulDrudge*>(g.server.HandleToObject(h)) : nullptr;
        return a && a->RuntimeReady() ? a : nullptr;
    }
    bool configuration(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        bool weapons[3]{}, skins[3]{};
        for (int i = 0; i < 24; ++i)
        {
            auto* a = drudge(g, {float(i * 100), 40, 0});
            CHECK(a && a->npc_type == AI_Mgr::NPCType::SoulDrudge && !a->m_bCabal && a->m_dwFlags == 0x200);
            CHECK(a->m_fHearingDist == 1000 && a->m_fSeeingDist == 1000 && a->m_fSensingDist == 100 &&
                  a->m_fWalkSpeed == 60 && a->m_fRunSpeed == 180);
            const auto* weapon = a->m_InventoryMgr.CurrentWeapon();
            CHECK(weapon && dynamic_cast<const CSoulWeapon*>(weapon));
            weapons[weapon->data.m_nType - WEAP_SOUL_CROWBAR] = true;
            std::string model, skin;
            CHECK(g.server.GetModelFilenames(a->m_hObject, model, skin));
            skins[skin.find("SoulDrudge3") != std::string::npos   ? 2
                  : skin.find("SoulDrudge2") != std::string::npos ? 1
                                                                  : 0] = true;
            // 120 + Random(1, 50) at medium difficulty.
            CHECK(a->m_damage.m_fHitPoints >= 120 && a->m_damage.m_fHitPoints <= 171);
        }
        CHECK(weapons[0] && weapons[1] && weapons[2] && skins[0] && skins[1] && skins[2]);
        CHECK(g.unload_world());
        return true;
    }
    bool attack(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = drudge(g, {0, 40, 0});
        CHECK(a);
        auto victim = g.objects.create(g.objects.find_class("CultistAI"), {}, Creation::World,
                                       {{"Name", std::string("victim")},
                                        {"Pos", DVector{0, 40, 300}},
                                        {"AIState", std::string("IDLE")},
                                        {"AIWeapon1", std::string("BERETTA")}});
        auto* target = victim ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(victim)) : nullptr;
        CHECK(target && target->RuntimeReady());
        const float before = target->m_damage.m_fHitPoints;
        CHECK(a->SetTarget("victim") && a->SetNewState(AI_Mgr::StateAttackClose));
        // A player would ping the NPCs awake every 0.5 s; an observer stands in for it.
        ObjectCreateStruct eye;
        eye.m_Pos = {0, 60, -200};
        auto observer = g.objects.create(g.objects.find_class("BaseClass"), eye);
        CHECK(observer);
        // A stationary target: pings now wake it (objects never block activation), but this test
        // exercises the attacker.
        g.server.SetObjectState(victim, OBJSTATE_INACTIVE);
        // It runs in from 300 units, then swings within 75; damage comes from the clip's fire_key.
        for (int i = 0; i < 900 && g.server.IsObjectAlive(victim) && target->m_damage.m_fHitPoints == before; ++i)
        {
            if (i % 30 == 0)
                CHECK(g.server.PingObjects(observer));
            CHECK(step(g, 1.0 / 60) && !a->runtime_failed);
        }
        if (a->runtime_failed)
            std::cerr << a->runtime_error << "\n";
        // Original soul-weapon aim: body position along the cached forward vector.
        const auto* w = a->m_InventoryMgr.CurrentWeapon();
        CHECK(w && w->shots >= 1);
        std::cout << "clips=" << a->fire_stand_clips << " clip=" << a->last_fire_clip
                  << " hp=" << (g.server.IsObjectAlive(victim) ? target->m_damage.m_fHitPoints : 0) << "\n";
        CHECK(a->fire_stand_clips >= 1 && a->last_fire_clip >= 4 && a->last_fire_clip <= 6 && a->attack_distance <= 75);
        CHECK(!g.server.IsObjectAlive(victim) || target->m_damage.m_fHitPoints < before);
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    if (mode == "configuration")
        return configuration(argv[2]) ? 0 : 1;
    if (mode == "attack")
        return attack(argv[2]) ? 0 : 1;
    return 2;
}
