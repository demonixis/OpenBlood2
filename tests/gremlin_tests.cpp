// _ADD_ON GremlinAI (lot N6, nightmares_edition_contracts.md): constructor values, claw (close),
// thrown rocks (far) with CGremlinRock's speed/direction and CRockProjectile's bounce, damage,
// fade and expiry, and the retail refusal. Needs the installed retail data plus BLOOD2NM.REZ.
#include "engine/collision.h"
#include "game/original/AddonProjectiles.h"
#include "game/original/GremlinAI.h"
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
    bool setup(GameSession& g, const char* root, bool nightmares = true)
    {
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        config.nightmares = nightmares;
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
        const bool ok = g.server.advance_models(dt) && g.server.advance_physics(dt) && g.objects.advance(dt) &&
                        g.server.advance_projectiles(dt);
        if (!ok)
            for (auto id : g.objects.snapshot())
                if (auto* r = dynamic_cast<CRockProjectile*>(g.objects.get(id)->game.get()))
                    std::cerr << "rock error: " << r->error << "\n";
        CHECK(ok);
        return true;
    }
    GremlinAI* gremlin(GameSession& g, DVector at)
    {
        auto h = g.objects.create(g.objects.find_class("GremlinAI"), {}, Creation::World,
                                  {{"Name", std::string("gremlin")}, {"Pos", at}});
        auto* a = h ? dynamic_cast<GremlinAI*>(g.server.HandleToObject(h)) : nullptr;
        return a && a->RuntimeReady() ? a : nullptr;
    }
    AI_Mgr* victim(GameSession& g, DVector at, const char* type = "CultistAI")
    {
        Properties props{{"Name", std::string("victim")}, {"Pos", at}, {"AIState", std::string("IDLE")}};
        if (std::string_view(type) == "CultistAI")
            props.emplace("AIWeapon1", std::string("BERETTA"));
        auto h = g.objects.create(g.objects.find_class(type), {}, Creation::World, props);
        auto* a = h ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h)) : nullptr;
        return a && a->RuntimeReady() ? a : nullptr;
    }
    HOBJECT observer(GameSession& g)
    {
        ObjectCreateStruct eye;
        eye.m_Pos = {0, 60, -200};
        return g.objects.create(g.objects.find_class("BaseClass"), eye);
    }
    bool configuration(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = gremlin(g, {0, 40, 0});
        CHECK(a && a->npc_type == AI_Mgr::NPCType::Gremlin && !a->m_bCabal && a->m_dwFlags == 0x202);
        CHECK(a->m_fHearingDist == 2000 && a->m_fSmellingDist == 1200 && a->m_fSeeingDist == 700 &&
              a->m_fSensingDist == 300 && a->m_fWalkSpeed == 320 && a->m_fRollSpeed == 320 && a->m_fRunSpeed == 640 &&
              a->m_fJumpSpeed == 640);
        CHECK(dynamic_cast<CShikariClaw*>(a->m_InventoryMgr.CurrentWeapon()));
        CHECK(a->m_damage.m_fArmorPoints == 0 && a->m_damage.m_fHitPoints >= 65 && a->m_damage.m_fHitPoints <= 76);
        CHECK(a->AnimSound() && a->AnimSound()->GetSoundRoot() == "sounds_ao/enemies/Gremlin");
        std::string model, skin;
        CHECK(g.server.GetModelFilenames(a->m_hObject, model, skin) && model.find("Gremlin.abc") != std::string::npos &&
              skin.find("Gremlin.dtx") != std::string::npos);
        DBYTE id = 0;
        CHECK(AI_Mgr::WeaponStrToInt("ROCK", id) && id == WEAP_GREMLIN_ROCK);
        // ComputeState: SIGHT <= .75 attacks from afar, else close; no height test.
        a->m_fStimuli[0] = .5f;
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateAttackFar);
        a->m_fStimuli[0] = .9f;
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateAttackClose);
        CHECK(a->ComputeState(3) && a->m_nState == AI_Mgr::StateSearchSmell); // The argument, not the stimulus.
        CHECK(g.unload_world());
        return true;
    }
    bool claw(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        // A target at the Gremlin's own height: a taller Cultist counts as above the body and draws
        // rocks even at close range (the source rule, exercised by the rock case).
        auto* a = gremlin(g, {0, 40, 0});
        auto* t = victim(g, {0, 40, 60}, "GremlinAI");
        auto eye = observer(g);
        CHECK(a && t && eye);
        g.server.SetObjectState(t->m_hObject, OBJSTATE_INACTIVE);
        CHECK(a->SetTarget("victim") && a->SetNewState(AI_Mgr::StateAttackClose));
        const float before = t->m_damage.m_fHitPoints;
        for (int i = 0; i < 900 && t->m_damage.m_fHitPoints == before; ++i)
        {
            if (i % 30 == 0)
                CHECK(g.server.PingObjects(eye));
            CHECK(step(g, 1.0 / 60));
            if (a->runtime_failed)
            {
                std::cerr << a->runtime_error << "\n";
                return false;
            }
        }
        std::cout << "Gremlin claw: " << a->claw_clips << " claw / " << a->rock_clips << " rock clip(s), state "
                  << a->m_nState << ", fire " << a->fire_shots << " shot(s) / " << a->fire_blocked
                  << " blocked, error '" << a->fire_error << "', victim " << before << " -> "
                  << t->m_damage.m_fHitPoints << "\n";
        CHECK(a->claw_clips >= 1 && t->m_damage.m_fHitPoints < before);
        CHECK(dynamic_cast<CShikariClaw*>(a->m_InventoryMgr.CurrentWeapon()));
        CHECK(g.unload_world());
        return true;
    }
    // AttackFar throws CRockProjectile: a random 500..1000 speed and an upward bias, five damaging
    // bounces scaled by the bounces left, a fade over the last 1.5 s and removal at 6.5 s.
    bool rock(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = gremlin(g, {0, 40, 0});
        auto* t = victim(g, {0, 40, 400});
        auto eye = observer(g);
        CHECK(a && t && eye);
        g.server.SetObjectState(t->m_hObject, OBJSTATE_INACTIVE);
        CHECK(a->SetTarget("victim") && a->SetNewState(AI_Mgr::StateAttackFar));
        CRockProjectile* thrown = nullptr;
        HOBJECT thrown_handle = nullptr;
        float speed = 0;
        for (int i = 0; i < 1800 && !thrown; ++i)
        {
            if (i % 30 == 0)
                CHECK(g.server.PingObjects(eye));
            CHECK(step(g, 1.0 / 60));
            if (a->runtime_failed)
            {
                std::cerr << a->runtime_error << "\n";
                return false;
            }
            for (auto id : g.objects.snapshot())
                if (auto* r = dynamic_cast<CRockProjectile*>(g.objects.get(id)->game.get()))
                {
                    thrown = r;
                    thrown_handle = r->m_hObject;
                    auto* weapon = dynamic_cast<CGremlinRock*>(a->m_InventoryMgr.CurrentWeapon());
                    CHECK(weapon && weapon->projectile_speed >= 500 && weapon->projectile_speed <= 1000);
                    speed = weapon->projectile_speed;
                    const auto dir = weapon->last_shot.direction;
                    CHECK(std::abs(std::sqrt(vector_dot(dir, dir)) - 1) < 1e-4f && dir.y > 0);
                }
        }
        if (!thrown)
        {
            auto* weapon = a->m_InventoryMgr.CurrentWeapon();
            std::cerr << "fire_result " << int(a->fire_result) << " fire_error '" << a->fire_error << "' weapon "
                      << (weapon ? int(weapon->data.m_nType) : -1) << " shot "
                      << (weapon ? int(weapon->last_shot.result) : -1) << " projectile_error '"
                      << (weapon ? weapon->projectile_error : "") << "' state " << a->m_nState << " state_error '"
                      << a->state_error << "'\n";
        }
        std::cout << "Gremlin: " << a->rock_clips << " rock clip(s), " << a->taunts << " taunt(s); rock speed " << speed
                  << "\n";
        CHECK(thrown && a->rock_clips >= 1 && thrown->m_hFiredFrom == a->m_hObject && thrown->m_fLifeTime == 6.5f &&
              thrown->m_fVelocity == speed);
        const float start = thrown->m_fStartTime;
        // Snapshot while alive: the rock is destroyed at its expiry.
        bool faded = false;
        unsigned sounds = 0, hits = 0;
        std::uint64_t contacts = 0;
        int bounces_left = 5;
        std::string sound_error;
        while (g.server.IsObjectAlive(thrown_handle))
        {
            CHECK(!thrown->failed);
            sounds = thrown->bounce_sounds;
            hits = thrown->damage_deliveries;
            contacts = thrown->contacts;
            bounces_left = thrown->bounces_left;
            sound_error = thrown->sound_error;
            DVector color;
            float alpha = 1;
            if (g.server.GetObjectColor(thrown_handle, color, alpha) && alpha < 1)
                faded = true;
            CHECK(g.server.GetTime() < start + 7);
            CHECK(step(g, 1.0 / 60));
        }
        std::cout << "Rock: " << contacts << " flight contact(s), " << 5 - bounces_left << " counted bounce(s), "
                  << sounds << " bounce sound(s), " << hits << " hit(s), removed after " << g.server.GetTime() - start
                  << " s" << (sound_error.empty() ? "" : "; sound error: " + sound_error) << "\n";
        CHECK(faded && g.server.GetTime() >= start + 6.5f && contacts >= 1 && bounces_left < 5 &&
              sounds == unsigned(5 - bounces_left));
        // A direct hit on a character: m_fDamage * bounces_left / 5 (5/5 before any bounce), from
        // the shooter, then the rock stops (an object contact zeroes its velocity).
        DVector target_position;
        CHECK(g.server.GetObjectPos(t->m_hObject, &target_position) == DE_OK);
        ObjectCreateStruct state;
        state.m_Pos = {target_position.x, target_position.y + 10, target_position.z - 40};
        auto* direct =
            dynamic_cast<CRockProjectile*>(g.server.CreateObject(g.server.GetClass("CRockProjectile"), &state));
        CHECK(direct && direct->Setup({0, 0, 1}, WEAP_GREMLIN_ROCK, 15, 600, 0, a->m_hObject));
        const auto direct_handle = direct->m_hObject;
        const float before = t->m_damage.m_fHitPoints;
        unsigned direct_hits = 0;
        for (int i = 0; i < 60 && g.server.IsObjectAlive(direct_handle) && !direct_hits; ++i)
        {
            CHECK(step(g, 1.0 / 60));
            if (g.server.IsObjectAlive(direct_handle))
                direct_hits = direct->damage_deliveries;
        }
        std::cout << "Direct rock: " << direct_hits << " hit(s), victim " << before << " -> "
                  << t->m_damage.m_fHitPoints << " HP\n";
        CHECK(direct_hits == 1 && t->m_damage.m_fHitPoints < before && direct->bounces_left == 4);
        CHECK(g.unload_world());
        return true;
    }
    // The retail build has no GremlinAI and maps ROCK to the source default Beretta.
    bool retail(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root, false));
        CHECK(!gremlin(g, {0, 40, 0}));
        DBYTE id = 0;
        CHECK(AI_Mgr::WeaponStrToInt("ROCK", id) && id == WEAP_BERETTA);
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
    if (mode == "rock")
        return rock(argv[2]) ? 0 : 1;
    if (mode == "retail")
        return retail(argv[2]) ? 0 : 1;
    return 2;
}
