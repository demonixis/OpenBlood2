// Original ShikariAI: constructor values, claw (close) and spit (far) attacks.
#include "engine/collision.h"
#include "game/original/ShikariAI.h"
#include "game/original/TeslaProjectile.h"
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
    ShikariAI* shikari(GameSession& g, DVector at)
    {
        auto h = g.objects.create(g.objects.find_class("ShikariAI"), {}, Creation::World,
                                  {{"Name", std::string("shikari")}, {"Pos", at}});
        auto* a = h ? dynamic_cast<ShikariAI*>(g.server.HandleToObject(h)) : nullptr;
        return a && a->RuntimeReady() ? a : nullptr;
    }
    AI_Mgr* victim(GameSession& g, DVector at)
    {
        auto h = g.objects.create(g.objects.find_class("CultistAI"), {}, Creation::World,
                                  {{"Name", std::string("victim")},
                                   {"Pos", at},
                                   {"AIState", std::string("IDLE")},
                                   {"AIWeapon1", std::string("BERETTA")}});
        auto* a = h ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h)) : nullptr;
        return a && a->RuntimeReady() ? a : nullptr;
    }
    bool run(GameSession& g, ShikariAI& a, AI_Mgr& target, int frames, HOBJECT observer)
    {
        const auto handle = target.m_hObject;
        const float before = target.m_damage.m_fHitPoints;
        for (int i = 0; i < frames && g.server.IsObjectAlive(handle) && target.m_damage.m_fHitPoints == before; ++i)
        {
            if (i % 30 == 0)
                CHECK(g.server.PingObjects(observer));
            CHECK(step(g, 1.0 / 60));
            if (a.runtime_failed)
            {
                std::cerr << a.runtime_error << "\n";
                return false;
            }
        }
        CHECK(!g.server.IsObjectAlive(handle) || target.m_damage.m_fHitPoints < before);
        return true;
    }
    bool configuration(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = shikari(g, {0, 40, 0});
        CHECK(a && a->npc_type == AI_Mgr::NPCType::Shikari && !a->m_bCabal && a->m_dwFlags == 0x0b);
        CHECK(a->m_fHearingDist == 2000 && a->m_fSmellingDist == 2000 && a->m_fSeeingDist == 1000 &&
              a->m_fSensingDist == 500 && a->m_fWalkSpeed == 180 && a->m_fRunSpeed == 320 && a->m_fJumpSpeed == 600);
        CHECK(dynamic_cast<CShikariClaw*>(a->m_InventoryMgr.CurrentWeapon()));
        CHECK(a->m_damage.m_fArmorPoints == 50 && a->m_damage.m_fHitPoints >= 175 && a->m_damage.m_fHitPoints <= 226);
        std::string model, skin;
        CHECK(g.server.GetModelFilenames(a->m_hObject, model, skin) && model.find("Shikari.abc") != std::string::npos);
        CHECK(g.unload_world());
        return true;
    }
    bool claw(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = shikari(g, {0, 40, 0});
        auto* t = victim(g, {0, 40, 150});
        CHECK(a && t);
        ObjectCreateStruct eye;
        eye.m_Pos = {0, 60, -200};
        auto observer = g.objects.create(g.objects.find_class("BaseClass"), eye);
        CHECK(observer && a->SetTarget("victim") && a->SetNewState(AI_Mgr::StateAttackClose));
        CHECK(run(g, *a, *t, 900, observer));
        CHECK(a->claw_clips >= 1 && a->last_fire_clip >= 4 && a->last_fire_clip <= 6);
        CHECK(g.unload_world());
        return true;
    }
    bool spit(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = shikari(g, {0, 40, 0});
        auto* t = victim(g, {0, 40, 500});
        CHECK(a && t);
        ObjectCreateStruct eye;
        eye.m_Pos = {0, 60, -200};
        auto observer = g.objects.create(g.objects.find_class("BaseClass"), eye);
        // AttackFar within 0.75 x seeing distance spits (FIRE_STAND[0]) an Acid loogie.
        CHECK(observer && a->SetTarget("victim") && a->SetNewState(AI_Mgr::StateAttackFar));
        CHECK(run(g, *a, *t, 900, observer));
        CHECK(a->spit_clips >= 1);
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
    if (mode == "claw")
        return claw(argv[2]) ? 0 : 1;
    if (mode == "spit")
        return spit(argv[2]) ? 0 : 1;
    return 2;
}
