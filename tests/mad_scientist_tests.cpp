// Original MadScientistAI: constructor values, idle until a stimulus, then the facing beg taunt.
#include "engine/collision.h"
#include "game/original/MadScientistAI.h"
#include "game/server_adapter.h"
#include "game/weapon_messages.h"
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
    MadScientistAI* scientist(GameSession& g, DVector at, Properties extra = {})
    {
        extra["Name"] = std::string("scientist");
        extra["Pos"] = at;
        auto h = g.objects.create(g.objects.find_class("MadScientistAI"), {}, Creation::World, extra);
        auto* a = h ? dynamic_cast<MadScientistAI*>(g.server.HandleToObject(h)) : nullptr;
        return a && a->RuntimeReady() ? a : nullptr;
    }
    bool configuration(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        // Authored HitPoints/ArmorPoints/AIWeapon are declared but never read by AI_Mgr::ReadProp.
        auto* a = scientist(g, {0, 60, 0},
                            {{"HitPoints", 500.0f}, {"ArmorPoints", 50.0f}, {"AIWeapon", std::string("BERETTA")}});
        CHECK(a && a->npc_type == AI_Mgr::NPCType::MadScientist && a->m_bCabal && a->m_dwFlags == 0);
        CHECK(a->m_fHearingDist == 0 && a->m_fSeeingDist == 1000 && a->m_fSensingDist == 200 &&
              a->m_fSmellingDist == 0 && a->m_fWalkSpeed == 75 && a->m_fRunSpeed == 180 && a->m_fRollSpeed == 2 &&
              a->m_fJumpSpeed == 500);
        CHECK(!a->m_InventoryMgr.CurrentWeapon() && a->m_damage.m_fArmorPoints == 0);

        std::string model, skin;
        CHECK(g.server.GetModelFilenames(a->m_hObject, model, skin));
        CHECK(model.ends_with("madscientist.abc") && skin.ends_with("madscientist.dtx"));
        // 30 + Random(1, 0) (reversed endpoints) at medium difficulty.
        CHECK(a->m_damage.m_fHitPoints >= 30 && a->m_damage.m_fHitPoints <= 31);
        CHECK(g.unload_world());
        return true;
    }
    bool beg(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = scientist(g, {0, 60, 0}); // Half-height 45: start above the floor.
        CHECK(a);
        // A monster must exist: without one, the original Enemies list falls back to the
        // Cabal list, which contains the scientist itself (the player is in both lists in game).
        auto threat = g.objects.create(g.objects.find_class("ShikariAI"), {}, Creation::World,
                                       {{"Name", std::string("threat")}, {"Pos", DVector{3000, 40, 0}}});
        auto* monster = threat ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(threat)) : nullptr;
        CHECK(monster && monster->RuntimeReady());
        ObjectCreateStruct eye;
        eye.m_Pos = {0, 60, -200};
        auto observer = g.objects.create(g.objects.find_class("BaseClass"), eye);
        CHECK(observer);
        // Nothing to perceive: the scientist idles at its authored state.
        for (int i = 0; i < 60; ++i)
        {
            if (i % 30 == 0)
                CHECK(g.server.PingObjects(observer));
            const bool stepped = step(g, 1.0 / 60);
            if (a->runtime_failed)
                std::cerr << a->runtime_error << " / " << a->state_error << "\n";
            CHECK(stepped && !a->runtime_failed);
        }
        CHECK(a->m_nState == AI_Mgr::StateIdle && !a->hide_updates);
        // The monster in sight (the scientist is Cabal) makes it face and beg. It stands
        // beyond its own 500-unit sensing range, facing away, so it does not attack.
        DVector near{800, 40, 300}; // In front of the scientist (+Z), 854 units away.
        const auto away = rotation_from_euler({0, 1.5707963f, 0});
        CHECK(g.server.TeleportObject(threat, &near) == DE_OK && g.server.SetObjectRotation(threat, &away) == DE_OK);
        const auto self = a->m_hObject;
        bool begged = false;
        for (int i = 0; i < 300 && !begged; ++i)
        {
            if (i % 30 == 0)
                CHECK(g.server.PingObjects(observer));
            CHECK(step(g, 1.0 / 60) && g.server.IsObjectAlive(self));
            if (a->runtime_failed)
                std::cerr << a->runtime_error << "\n";
            CHECK(!a->runtime_failed);
            begged = a->m_nState == AI_Mgr::StateEscapeHide && a->m_nCurMetacmd == 17 && a->m_bAnimating;
        }
        CHECK(begged && a->hide_updates > 0 && a->GetTarget() == threat);
        // Facing: the scientist turned toward the target (+X).
        DRotation rotation;
        CHECK(g.server.GetObjectRotation(a->m_hObject, &rotation) == DE_OK);
        const auto forward = rotate_vector({0, 0, 1}, rotation);
        CHECK(forward.x > .9f);
        std::cout << "hide updates=" << a->hide_updates << " forward=" << forward.x << "\n";
        CHECK(g.unload_world());
        return true;
    }
    bool wounds(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = scientist(g, {0, 60, 0});
        CHECK(a);
        const auto self = a->m_hObject;
        // A character attacker (non-character damagers are rejected by ComputeStimuli).
        auto attacker = g.objects.create(g.objects.find_class("ShikariAI"), {}, Creation::World,
                                         {{"Name", std::string("threat")}, {"Pos", DVector{3000, 40, 0}}});
        CHECK(attacker);
        ObjectCreateStruct eye;
        eye.m_Pos = {0, 60, -200};
        auto observer = g.objects.create(g.objects.find_class("BaseClass"), eye);
        CHECK(observer);
        // Hits localize on the model despite its two "torso" nodes. madscientist.abc has no
        // recoil clip: the recoil that follows enough damage is an explicit, visible failure.
        bool localized = false;
        for (int shot = 0; shot < 8 && !a->runtime_failed && !a->m_damage.IsDead(); ++shot)
        {
            const float before = a->m_damage.m_fHitPoints;
            Damage d{{0, 0, 1}, {0, 80, -30}, 6, DamageType::Normal, attacker};
            auto m = g.server.StartMessageToObject(g.server.HandleToObject(attacker), self, damage_message);
            CHECK(m && g.server.WriteDamage(m, d) == DE_OK && g.server.EndMessage(m) == DE_OK);
            localized |= a->m_damage.hit.valid;
            CHECK(a->m_damage.m_fHitPoints < before);
            for (int i = 0; i < 60 && !a->runtime_failed; ++i)
            {
                if (i % 30 == 0)
                    CHECK(g.server.PingObjects(observer));
                CHECK(step(g, 1.0 / 60) && g.server.IsObjectAlive(self));
            }
        }
        CHECK(localized && a->runtime_failed &&
              a->runtime_error.find("animation model/clip unavailable") != std::string::npos);
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
    if (mode == "wounds")
        return wounds(argv[2]) ? 0 : 1;
    if (mode == "beg")
        return beg(argv[2]) ? 0 : 1;
    return 2;
}
