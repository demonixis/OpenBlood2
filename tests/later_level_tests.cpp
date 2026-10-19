#include "engine/collision.h"
#include "engine/world_model.h"
#include "game/original/AI_Mgr.h"
#include "game/original/CampaignProjectiles.h"
#include "game/original/CampaignWeapons.h"
#include "game/original/LaterEnemies.h"
#include "game/original/ParticleSystem.h"
#include "game/original/Thief.h"
#include "game/original/Trigger.h"
#include "game/original/WeaponPickups.h"
#include "game/original/ZealotAI.h"
#include "game/original/spawn_object.h"
#include "game/server_adapter.h"
#include "game/world_validation.h"
#include <cmath>
#include <iostream>
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
    bool mount(GameSession& g, const char* root)
    {
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        return true;
    }
    bool fixture(GameSession& g, const char* root)
    {
        CHECK(mount(g, root));
        WorldModel floor;
        floor.points = {{-4000, 0, -4000}, {4000, 0, -4000}, {4000, 0, 4000}, {-4000, 0, 4000}};
        floor.surfaces.resize(1);
        floor.surfaces[0].flags = 1;
        floor.polygons = {{0, {0, 1, 2, 3}}};
        std::string error;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        g.world_path = "synthetic_campaign_probe";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Pos", DVector{0, 100, 0}}}));
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld());
        if (!g.enter_player(error) || !g.pump_player_messages(error))
        {
            std::cerr << error << '\n';
            for (const auto& e : g.objects.diagnostics())
                std::cerr << e << '\n';
            return false;
        }
        return true;
    }
    bool entry(const char* root, const char* world)
    {
        auto g = std::make_unique<GameSession>();
        CHECK(mount(*g, root));

        // Same transaction as the inspector, including replacement of an already running session.
        for (DBYTE difficulty : {1, 0, 2})
        {
            std::string error;
            if (!GameSession::start_new_game(g, world, difficulty, 0, error))
            {
                std::cerr << error << '\n';
                return false;
            }
            WorldValidation report;
            CHECK(check_world_initialization(*g, report, error));
            CHECK(report.unknown == 0 && report.failed == 0 && report.unavailable_services.empty());
            CHECK(g->local_client && g->client_shell.m_bInWorld && g->startup.checked_textures > 0);
            unsigned corrected = 0;
            for (const auto& e : g->entries)
                if (const auto* r = g->objects.get(e.object))
                {
                    if (const auto* ai = dynamic_cast<const AI_Mgr*>(r->game.get()))
                    {
                        CHECK(ai->RuntimeReady());
                        if (r->placement_nudges)
                        {
                            ++corrected;
                            CollisionHit hit;
                            CHECK(g->collision.sweep(r->state.m_Pos, r->state.m_Pos, r->dimensions, hit) !=
                                  CollisionQuery::Invalid);
                            CHECK(!hit.start_solid || hit.penetration < .01);
                            std::cout << world << " corrected " << r->state.m_Name << " by " << r->placement_nudge.x
                                      << ',' << r->placement_nudge.y << ',' << r->placement_nudge.z << '\n';
                        }
                    }
                }

            // The first rendered frame can be too short for float positions to move.
            for (double seconds : {0., 1e-8, 1e-7, 1e-5, 1e-4, 1. / 144})
                if (!g->step(seconds))
                {
                    std::cerr << "first dt=" << seconds << '\n';
                    for (const auto& why : g->step_errors())
                        std::cerr << why << '\n';
                    for (const auto& why : g->objects.diagnostics())
                        std::cerr << why << '\n';
                    return false;
                }
            // Exercise real physics, callbacks, model keys and player input after entry.
            g->client_shell.movement_input.forward = true;
            for (int i = 0; i < 180; ++i)
            {
                if (i == 30)
                    g->client_shell.movement_input.forward = false;
                if (!g->step(1. / 60))
                {
                    for (const auto& e : g->step_errors())
                        std::cerr << e << '\n';
                    return false;
                }
            }
            for (auto id : g->objects.snapshot())
                if (const auto* r = g->objects.get(id); r && r->life == Life::Alive)
                    if (const auto* ai = dynamic_cast<const AI_Mgr*>(r->game.get());
                        ai &&
                        (ai->npc_type == AI_Mgr::NPCType::Behemoth || ai->npc_type == AI_Mgr::NPCType::DeathShroud ||
                         ai->npc_type == AI_Mgr::NPCType::TheHand || ai->npc_type == AI_Mgr::NPCType::DrudgePriest))
                        if (!ai->RuntimeReady() || !ai->state_error.empty())
                        {
                            std::cerr << r->state.m_Name << ": " << ai->runtime_error << " / " << ai->state_error
                                      << '\n';
                            return false;
                        }
            std::cout << world << ": difficulty=" << unsigned(difficulty) << " started and stepped, "
                      << "omitted=" << report.omitted << " corrected=" << corrected << '\n';
        }
        CHECK(g->unload_world() && g->objects.snapshot().empty() && !g->server.string_count() &&
              !g->server.message_count() && !g->server.list_count());
        return true;
    }
    template <class T> T* actor(GameSession& g, const char* name, DVector pos)
    {
        auto h = g.objects.create(g.objects.find_class(name), {}, Creation::World,
                                  {{"Pos", pos}, {"AIState", std::string("IDLE")}, {"RandomHitPoints", 0.f}});
        auto* ai = h ? dynamic_cast<T*>(g.server.HandleToObject(h)) : nullptr;
        if (!ai || !ai->RuntimeReady())
            return nullptr;
        g.server.SetNextUpdate(h, 0);
        auto* p = g.server_shell.Player(g.local_client);
        if (!ai->SetNewTarget(p->m_hObject) || !ai->RefreshTargetPosition() || !ai->m_MoveObj.Update())
            return nullptr;
        return ai;
    }
    bool finish_clip(GameSession& g, AI_Mgr& ai)
    {
        const auto* r = g.objects.get(ai.m_hObject);
        CHECK(r && r->model && r->animation.animation < r->model->animations.size());
        CHECK(g.server.advance_models(r->model->animations[r->animation.animation].duration / 1000. + .001));
        return true;
    }
    bool behemoth(const char* root)
    {
        GameSession g;
        CHECK(fixture(g, root));
        auto* a = actor<Behemoth>(g, "Behemoth", {1000, 200, 0});
        CHECK(a && a->m_damage.m_fHitPoints >= 5000 && a->m_damage.m_fArmorPoints == 1000);
        CHECK(a->m_fRunSpeed == 320 && a->m_fWalkSpeed == 100);
        for (int state : {AI_Mgr::StateAttackClose, AI_Mgr::StateAttackFar})
        {
            CHECK(a->SetNewState(state) && a->MC_Fire_Stand() && a->Fire());
            const auto* gun = a->m_InventoryMgr.CurrentWeapon();
            CHECK(gun->data.m_nType ==
                  (state == AI_Mgr::StateAttackClose ? WEAP_BEHEMOTH_CLAW : WEAP_BEHEMOTH_SHOCKWAVE));
            if (state == AI_Mgr::StateAttackFar)
            {
                auto* projectile = dynamic_cast<CProjectile*>(g.server.HandleToObject(gun->projectile));
                CHECK(projectile && projectile->configured && !projectile->failed);
                CHECK(g.objects.engine_message(projectile->m_hObject, MID_UPDATE, nullptr));
                CHECK(!projectile->failed);
            }
            CHECK(finish_clip(g, *a) && a->MC_Fire_Stand());
            CHECK(a->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_BEHEMOTH_CLAW);
        }
        CHECK(g.unload_world() && g.objects.snapshot().empty());
        return true;
    }
    bool priest(const char* root)
    {
        GameSession g;
        CHECK(fixture(g, root));
        auto* a = actor<DrudgePriest>(g, "DrudgePriest", {1000, 200, 0});
        CHECK(a && !(g.server.GetObjectFlags(a->m_hObject) & FLAG_GRAVITY));
        CHECK(a->m_damage.m_fHitPoints >= 1000 && a->m_damage.m_fArmorPoints == 100);
        CHECK(!a->loop && !a->sound_error.empty()); // Authored loop is absent in the mounted retail archives.
        CHECK(a->MC_Taunt_Bold() && !a->spawned && !a->m_bAnimating);
        CHECK(g.objects.advance(30) && a->MC_Taunt_Bold() && a->m_bAnimating);
        CHECK(a->MC_Taunt_Bold() && a->spawned == 1);
        CHECK(a->MC_Taunt_Bold() && a->spawned == 1); // No same-frame catch-up spawn.
        CHECK(finish_clip(g, *a) && a->MC_Taunt_Bold() && !a->m_bAnimating);
        for (int state : {AI_Mgr::StateAttackClose, AI_Mgr::StateAttackFar})
        {
            CHECK(a->SetNewState(state) && a->MC_Fire_Stand() && a->Fire());
            auto* gun = a->m_InventoryMgr.CurrentWeapon();
            if (state == AI_Mgr::StateAttackClose)
            {
                auto* lightning = dynamic_cast<CDrudgeLightning*>(gun);
                CHECK(lightning && gun->last_shot.result == FireResult::Fired && !lightning->effect_error.empty());
            }
            else
                CHECK(gun->data.m_nType == WEAP_DRUDGE_FIREBALL && gun->projectile);
            CHECK(finish_clip(g, *a) && a->MC_Fire_Stand());
        }
        CHECK(g.unload_world() && g.objects.snapshot().empty());
        return true;
    }
    bool shroud(const char* root)
    {
        GameSession g;
        CHECK(fixture(g, root));
        auto& p = *g.server_shell.Player(g.local_client);
        auto* a = actor<DeathShroud>(g, "DeathShroud", {1000, 200, 0});
        CHECK(a && a->loop && a->sound_error.empty() && a->m_damage.ai_body_hits);
        CHECK(g.objects.get(a->m_hObject)->alpha == .9f);
        for (bool out : {true, false})
        {
            a->m_bAnimating = false;
            CHECK(a->Fade(out) && finish_clip(g, *a));
            for (unsigned i = 0; i < 9 && a->m_bAnimating; ++i)
                CHECK(a->Fade(out));
            CHECK(!a->m_bAnimating && bool(g.server.GetObjectFlags(a->m_hObject) & FLAG_SOLID) == !out);
        }
        CHECK(a->SetNewState(AI_Mgr::StateAttackFar) && a->MC_Fire_Stand() && a->Fire());
        const auto h = a->m_InventoryMgr.CurrentWeapon()->projectile;
        auto* skull = dynamic_cast<CSkullProjectile*>(g.server.HandleToObject(h));
        CHECK(skull && skull->configured && !skull->failed && g.objects.get(h)->model);
        DVector head;
        DRotation rot;
        CHECK(g.server.GetModelNodeTransform(p.m_hObject, "head", &head, &rot));
        auto pos = vector_add(head, {100, 0, 0});
        CHECK(g.server.TeleportObject(h, &pos) == DE_OK);
        DVector velocity{0, 0, 10};
        CHECK(g.server.SetVelocity(p.m_hObject, &velocity) == DE_OK);
        CHECK(g.objects.engine_message(h, MID_UPDATE, nullptr) && skull->tracked == p.m_hObject && skull->loop);
        velocity = {};
        CHECK(g.server.SetVelocity(p.m_hObject, &velocity) == DE_OK);
        CHECK(g.objects.engine_message(h, MID_UPDATE, nullptr) && !skull->tracked);
        SegmentHit hit;
        hit.object = p.m_hObject;
        hit.point = head;
        hit.normal = {1, 0, 0};
        skull->HandleContact(hit);
        CHECK(skull->contact_damage == 1 && skull->m_bExplode);
        CHECK(g.objects.engine_message(h, MID_UPDATE, nullptr) && skull->exploded && !skull->loop && !skull->failed);
        CHECK(!g.server.IsObjectAlive(h) && g.objects.finish_frame());
        a->m_bAnimating = false;
        CHECK(a->MC_Dead() && !a->loop);
        CHECK(finish_clip(g, *a) && a->MC_Dead() && a->m_bRemoveMe);
        CHECK(g.unload_world() && g.objects.snapshot().empty());
        return true;
    }
    bool hand(const char* root)
    {
        GameSession g;
        CHECK(fixture(g, root));
        auto& p = *g.server_shell.Player(g.local_client);
        auto* a = actor<TheHandAI>(g, "TheHandAI", {1000, 100, 1000});
        CHECK(a && a->m_damage.ai_body_hits && a->m_fAIMass == 125);
        CHECK(a->RequestAttach(p.m_hObject) && a->AI_STATE_EnemyAttach());
        std::string error;
        CHECK(g.pump_player_messages(error) && a->proceed && g.client_shell.creature == a->m_hObject);
        CHECK(a->AI_STATE_EnemyAttach());
        CHECK(g.objects.advance(1.01) && a->AI_STATE_EnemyAttach());
        CHECK(a->damage_ticks == 1 && p.m_damage.last_damage.amount >= 5 && p.m_damage.last_damage.amount <= 10);
        CHECK(a->Detach() && a->AI_STATE_EnemyAttach() && g.pump_player_messages(error));
        CHECK(!g.client_shell.creature && a->state_error.empty());
        CHECK(g.unload_world() && g.objects.snapshot().empty());
        return true;
    }
    bool minigun(const char* root)
    {
        GameSession g;
        CHECK(fixture(g, root));
        auto& p = *g.server_shell.Player(g.local_client);
        auto& inv = p.m_InventoryMgr;
        auto h =
            g.objects.create(g.objects.find_class("MiniGunPU"), {}, Creation::World, {{"Pos", DVector{200, 100, 200}}});
        CHECK(h && g.objects.get(h)->model && g.server.ValidateSpawnObject("MiniGunPU"));
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, p.m_hObject) && !g.server.IsObjectAlive(h));
        CHECK(inv.ChangeWeapon(WEAP_MINIGUN) == InventoryResult::Ok);
        std::string error;
        CHECK(g.pump_player_messages(error));
        auto* gun = inv.CurrentWeapon();
        CHECK(dynamic_cast<CMiniGun*>(gun));
        for (bool alternate : {false, true})
        {
            CHECK(g.objects.advance(.1));
            inv.SetAmmoCount(AMMO_BULLET, 100);
            WeaponFire shot{{0, 100, 0}, {0, -1, 0}, 17, WEAP_MINIGUN};
            shot.alternate = alternate;
            CHECK(p.HandleWeaponFireMessage(shot) == FireResult::Fired && gun->last_pellets == 4);
            CHECK(inv.GetAmmoCount(AMMO_BULLET) == 96);
            CHECK(!gun->impacts.empty());
            for (const auto& impact : gun->impacts)
                CHECK(!impact.effect_requested);
        }
        CHECK(g.unload_world() && g.objects.snapshot().empty());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    const bool ok = mode == "behemoth"  ? behemoth(argv[2])
                    : mode == "priest"  ? priest(argv[2])
                    : mode == "shroud"  ? shroud(argv[2])
                    : mode == "hand"    ? hand(argv[2])
                    : mode == "minigun" ? minigun(argv[2])
                                        : entry(argv[2], argv[1]);
    return ok ? 0 : 1;
}
