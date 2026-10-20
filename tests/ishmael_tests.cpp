#include "app/model_scene.h"
#include "app/sprite_scene.h"
#include "brush_fixture.h"
#include "game/original/CampaignProjectiles.h"
#include "game/original/CampaignWeapons.h"
#include "game/original/ChosenAI.h"
#include "game/original/IshmaelAI.h"
#include "game/server_adapter.h"
#include "game/world_validation.h"
#include <cmath>
#include <iostream>
#include <set>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ":" << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool setup(GameSession& g, const char* root)
    {
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(config, error));
        auto floor = test_brush::box("floor", {-2000, -20, -2000}, {2000, -10, 2000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    IshmaelAI* create(GameSession& g, const char* name, DVector pos, Properties props = {})
    {
        props["Pos"] = pos;
        auto h = g.objects.create(g.objects.find_class(name), {}, Creation::World, std::move(props));
        if (!h)
            for (const auto& d : g.objects.diagnostics())
                std::cerr << d << '\n';
        auto* a = dynamic_cast<IshmaelAI*>(g.server.HandleToObject(h));
        if (a && !a->RuntimeReady())
            std::cerr << name << ": " << a->runtime_error << '\n';
        if (a && !a->m_MoveObj.Update())
            return nullptr;
        return a;
    }
    bool profiles(const char* root)
    {
        for (int difficulty : {0, 1, 2})
        {
            GameSession g;
            CHECK(setup(g, root));
            g.server.set_game_settings(difficulty, 0);
            const float factor = difficulty == 0 ? .25f : difficulty == 2 ? 1.5f : 1;
            auto* a = create(g, "IshmaelAI", {-300, 100, 0});
            auto* r = create(g, "IshmaelREV", {300, 100, 0});
            CHECK(a && r && a->AnimSound() != r->AnimSound());
            for (auto* npc : {a, r})
            {
                CHECK(npc->RuntimeReady() && npc->m_nState == AI_Mgr::StatePassive && npc->m_bCabal &&
                      npc->m_dwFlags == 0x20);
                CHECK(npc->m_fHearingDist == 0 && npc->m_fSeeingDist == 800 && npc->m_fSensingDist == 1000 &&
                      npc->m_fSmellingDist == 0);
                CHECK(npc->m_fWalkSpeed == 75 && npc->m_fRunSpeed == 180 && npc->m_fRollSpeed == 105 &&
                      npc->m_fJumpSpeed == 400);
                CHECK(npc->m_damage.m_fHitPoints >= 500 * factor && npc->m_damage.m_fHitPoints <= 501 * factor &&
                      npc->m_damage.m_fArmorPoints == 500 * factor);
                CHECK(npc->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_ORB);
                bool heal = false, wave = false;
                for (unsigned slot = 0; slot < inventory_weapon_slots; ++slot)
                    if (const auto* w = npc->m_InventoryMgr.GetWeapon(slot))
                    {
                        heal |= w->data.m_nType == WEAP_ZEALOT_HEAL;
                        wave |= w->data.m_nType == WEAP_ZEALOT_SHOCKWAVE;
                    }
                CHECK(heal && wave);
                CHECK(npc->AnimSound()->GetSoundRoot() == "sounds/chosen/ishmael");
                CHECK(std::string(g.objects.get(npc->m_hObject)->state.m_SkinName) ==
                      (npc->Revenant() ? "Skins/Characters/ishrev.dtx" : "Skins/Characters/ishmael.dtx"));
            }
            CHECK(g.objects.advance(.01) && a->RuntimeReady() && r->RuntimeReady());
            CHECK(g.unload_world());
        }
        return true;
    }
    bool actions(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g, "IshmaelAI", {-400, 100, 0});
        auto* r = create(g, "IshmaelREV", {400, 100, 0});
        CHECK(a && r);
        for (auto* npc : {a, r})
        {
            npc->m_vTargetPos = npc->m_MoveObj.GetPos();
            npc->m_fStimuli[5] = .09f;
            npc->m_fStimuli[3] = .8f;
            CHECK(npc->ComputeState(1) && npc->m_nState == AI_Mgr::StateEscapeHide);
            npc->m_fStimuli[5] = .1f;
            CHECK(npc->ComputeState(1) && npc->m_nState == AI_Mgr::StateAttackClose);
            npc->m_fStimuli[3] = .64f;
            CHECK(npc->ComputeState(1) && npc->m_nState == AI_Mgr::StateAttackFar);
            CHECK(npc->ComputeState(0) && npc->m_nState == AI_Mgr::StateSearchVisual);
            CHECK(npc->ComputeState(0) && npc->m_nState == AI_Mgr::StateIdle);
            CHECK(npc->SetNewState(AI_Mgr::StateAttackFar));
            npc->Metacmd = 3;
            npc->m_nCurMetacmd = 7;
            npc->m_bAnimating = true;
            CHECK(npc->AI_STATE_AttackFar());
            CHECK(npc->m_nState == (npc->Revenant() ? AI_Mgr::StateSearchVisual : AI_Mgr::StateAttackFar));
        }
        CHECK(a->SetNewTarget(r->m_hObject));
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose));
        a->Metacmd = 2;
        a->m_vTargetPos = vector_add(a->m_MoveObj.GetPos(), {0, 0, 200});
        CHECK(a->AI_STATE_AttackClose() && a->m_nState == AI_Mgr::StateTeleport);
        // Healing completes the authored clip, increases current HP by ten percent, with no max clamp.
        CHECK(a->SetNewState(AI_Mgr::StateEscapeHide));
        a->m_damage.SetHitPoints(100);
        CHECK(a->AI_STATE_Escape_Hide() && a->m_nCurMetacmd == 102);
        CHECK(a->Fire(false));
        auto* spell = dynamic_cast<CZealotSpell*>(a->m_InventoryMgr.CurrentWeapon());
        CHECK(spell && spell->effect_error.empty() && g.server.object_particles.active.back().effect.recipe == 18);
        g.objects.get(a->m_hObject)->animation.done = true;
        CHECK(a->AI_STATE_Escape_Hide() && a->heals == 1 && std::abs(a->m_damage.m_fHitPoints - 110) < .001f);
        CHECK(a->SetNewState(AI_Mgr::StateAttackFar));
        a->m_InventoryMgr.SetAmmoCount(AMMO_FOCUS, 0);
        CHECK(a->MC_Fire_Stand() && a->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) == 20);
        CHECK(a->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_ORB && a->Fire(false));
        CHECK(a->m_InventoryMgr.GetAmmoCount(AMMO_FOCUS) == 0 && a->m_InventoryMgr.CurrentWeapon()->projectile);
        CHECK(a->SetNewState(AI_Mgr::StateAttackClose) && a->MC_Fire_Stand());
        CHECK(a->m_InventoryMgr.CurrentWeapon()->data.m_nType == WEAP_ZEALOT_SHOCKWAVE && a->Fire(false));
        auto wave = a->m_InventoryMgr.CurrentWeapon()->projectile;
        // Suppress actor decisions while the real projectile executes once.
        g.server.SetObjectState(a->m_hObject, OBJSTATE_INACTIVE);
        g.server.SetObjectState(r->m_hObject, OBJSTATE_INACTIVE);
        CHECK(g.objects.advance(.002) && !g.server.IsObjectAlive(wave));
        CHECK(!g.server.scene_explosions.empty() && g.server.scene_explosions.back().type == 22);
        CHECK(g.server.scene_explosions.back().model_error.empty() &&
              g.server.scene_explosions.back().ring_error.empty());
        return g.unload_world();
    }
    bool keys(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g, "IshmaelAI", {-300, 100, 0});
        auto* target = create(g, "IshmaelREV", {300, 100, 0});
        CHECK(a && target);
        g.server.SetObjectState(target->m_hObject, OBJSTATE_INACTIVE);
        target->m_damage.can_damage = false;
        a->m_bSetShutdown = true;
        g.server.SetObjectState(a->m_hObject, OBJSTATE_ACTIVE);
        CHECK(a->SetNewTarget(target->m_hObject) && a->SetNewState(AI_Mgr::StateAttackFar));
        a->Metacmd = 3;
        a->m_vTargetPos = target->m_MoveObj.GetPos();
        for (int i = 0; i < 240 && !a->fire_shots; ++i)
            CHECK(g.step(1. / 60));
        if (a->runtime_failed)
            std::cerr << a->runtime_error << "\n";
        std::cerr << "key probe state=" << a->m_nState << " command=" << a->Metacmd << " updates=" << a->runtime_updates
                  << " keys=" << a->model_keys << " error=" << a->ai_key_error << "\n";
        CHECK(a->RuntimeReady() && a->fire_shots > 0);
        auto* w = a->m_InventoryMgr.CurrentWeapon();
        CHECK(w && w->data.m_nType == WEAP_ORB && w->last_shot.result == FireResult::Fired);
        std::cout << "Actual Ishmael animation-key Orb shots=" << a->fire_shots << "\n";
        return g.unload_world();
    }
    bool teleport(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g, "IshmaelAI", {-500, 100, 0});
        auto* r = create(g, "IshmaelREV", {500, 100, 0});
        CHECK(a && r);
        for (int i = 1; i <= 3; ++i)
            std::cout << "Ishmael laugh" << i << " indexed="
                      << bool(g.resources.find("sounds/chosen/ishmael/laugh" + std::to_string(i) + ".wav")) << "\n";
        for (auto* npc : {a, r})
        {
            CHECK(npc->SetNewState(AI_Mgr::StateTeleport) && npc->AI_STATE_Teleport());
            CHECK(!(g.server.GetObjectFlags(npc->m_hObject) & FLAG_SOLID));
            CHECK(bool(g.server.GetObjectFlags(npc->m_hObject) & FLAG_GRAVITY) == !npc->Revenant());
            for (int i = 0; i < 16; ++i)
                CHECK(npc->AI_STATE_Teleport());
            DVector color;
            float alpha;
            CHECK(g.server.GetObjectColor(npc->m_hObject, color, alpha) && alpha == 0 && npc->Metacmd == 2);
            CHECK(npc->AI_STATE_Teleport());
            if (!npc->AnimSound()->sound_error.empty())
                std::cerr << npc->AnimSound()->sound_resource << ": " << npc->AnimSound()->sound_error << "\n";
            if (!npc->AnimSound()->sound_error.empty())
            {
                CHECK(npc->AnimSound()->sound_error == "Missing or oversized sound resource" &&
                      !g.resources.find(npc->AnimSound()->sound_resource));
                bool authored = false;
                for (int i = 1; i <= 3; ++i)
                    authored |=
                        npc->AnimSound()->sound_resource == "sounds/chosen/ishmael\\laugh" + std::to_string(i) + ".wav";
                CHECK(authored && npc->AnimSound()->sound_failures > 0);
            }
            CHECK(npc->AI_STATE_Teleport() && npc->teleports == 1 && npc->Metacmd == 4);
            CHECK(npc->AI_STATE_Teleport() && g.server.GetObjectColor(npc->m_hObject, color, alpha) && alpha == 1);
            CHECK((g.server.GetObjectFlags(npc->m_hObject) & (FLAG_SOLID | FLAG_GRAVITY)) ==
                  (FLAG_SOLID | FLAG_GRAVITY));
        }
        CHECK(a->SetNewState(AI_Mgr::StateRecoil) && a->AI_STATE_Recoil() && a->m_nCurMetacmd == 101);
        return g.unload_world();
    }
    bool death(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g, "IshmaelAI", {0, 100, 0});
        CHECK(a);
        std::vector<AI_Mgr*> victims;
        for (float x : {100.f, 480.f, 600.f})
        {
            auto h = g.objects.create(g.objects.find_class("GabriellaAI"), {}, Creation::World,
                                      {{"Pos", DVector{x, 100, 0}}});
            auto* v = dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h));
            CHECK(v && v->RuntimeReady());
            DVector pos = vector_add(a->m_MoveObj.GetPos(), {x, 0, 0}), dims{40, 40, 40};
            CHECK(g.server.TeleportObject(h, &pos) == DE_OK && g.server.SetObjectDims(h, &dims) == DE_OK);
            v->m_damage.SetHitPoints(1000);
            v->m_damage.m_fArmorPoints = 0;
            v->m_damage.ai_body_hits = true;
            v->m_damage.apply_damage_physics = false;
            g.server.SetObjectState(h, OBJSTATE_INACTIVE);
            victims.push_back(v);
        }
        CHECK(a->MC_Dead());
        DVector velocity;
        CHECK(g.server.GetVelocity(a->m_hObject, &velocity) == DE_OK && velocity.y == 30);
        CHECK(!(g.server.GetObjectFlags(a->m_hObject) & FLAG_GRAVITY));
        g.objects.get(a->m_hObject)->animation.done = true;
        CHECK(a->MC_Dead() && a->m_bRemoveMe && a->death_deliveries >= 2);
        for (unsigned i = 0; i < 2; ++i)
        {
            const auto& d = victims[i]->m_damage.last_damage;
            CHECK(d.amount >= 25 && d.amount <= 50 && d.attacker == a->m_hObject && d.type == DamageType::Explode);
            CHECK(victims[i]->m_damage.m_fHitPoints < 1000);
            CHECK(g.server.GetVelocity(victims[i]->m_hObject, &velocity) == DE_OK &&
                  vector_dot(velocity, velocity) == 0);
        }
        CHECK(victims[2]->m_damage.m_fHitPoints == 1000);
        CHECK(a->death_effect.model_event && a->death_effect.ring_event && a->death_effect.light_event &&
              a->death_effect.voice);
        auto h = a->m_hObject;
        CHECK(g.objects.advance(.02) && !g.server.IsObjectAlive(h));
        CHECK(!g.server.explosion_models.active.empty());
        return g.unload_world();
    }
    bool effects(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create(g, "IshmaelAI", {0, 100, 0});
        CHECK(a);
        std::string error;
        CObjectParticleFX fx;
        std::minstd_rand rng(1);
        CHECK(fx.Init(18, 1, rng, error) && fx.radius == 500 && fx.gravity == 150 && fx.position_radius == 20 &&
              fx.max_life == .9f && fx.requested_alpha == 1);
        NodePose pose{{0, 100, 0}, {}};
        CHECK(fx.CreateObject(g.resources, pose, 0, error) && fx.Update(.01, &pose, rng, 2048) &&
              fx.Update(.11, &pose, rng, 2048));
        CHECK(fx.particles.size() == 18);
        for (const auto& p : fx.particles)
            CHECK(std::abs(std::sqrt(vector_dot(p.position, p.position)) - 20) < .001f && p.color.z == 1 &&
                  p.lifetime >= .75f && p.lifetime <= .9f);
        CHECK(fx.Update(2.4, &pose, rng, 2048) && fx.alpha < 1 && !fx.Update(2.6, &pose, rng, 2048));
        SceneExplosionReport r;
        CHECK(PublishDivineShockball(a->m_hObject, {0, 100, 0}, {0, 2, 0}, r));
        CHECK(g.server.explosion_models.active.back().settings.scale2.x == 1000);
        CHECK(g.server.explosion_rings.active.back().settings.position.y == 160 &&
              g.server.explosion_rings.active.back().particles.size() == 32);
        CHECK(g.server.explosion_lights.active.back().settings.position.y == 130);
        ModelView models;
        SpriteView sprites;
        CHECK(update_explosion_model_scene(g, models, error) && update_sprite_scene(g, sprites, error));
        CHECK(!models.entries.empty() && models.entries.back().error.empty() && !sprites.entries.empty());
        return g.unload_world();
    }
    bool sphere(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::vector<HOBJECT> found;
        ObjectCreateStruct state;
        state.m_Pos = {480, 0, 0};
        auto h = g.objects.create(g.objects.find_class("BaseClass"), state);
        DVector dims{40, 40, 40};
        CHECK(h && g.server.SetObjectDims(h, &dims) == DE_OK);
        g.server.SetObjectState(h, OBJSTATE_INACTIVE);
        CHECK(g.server.CollectObjectsTouchingSphere({}, 450, found) && found.size() == 1 && found[0] == h);
        // Bounding sphere alone would admit this, but the original box broad phase excludes it.
        DVector p{491, 0, 0};
        CHECK(g.server.TeleportObject(h, &p) == DE_OK);
        CHECK(g.server.CollectObjectsTouchingSphere({}, 450, found) && found.empty());
        CHECK(!g.server.CollectObjectsTouchingSphere({}, -1, found) && found.empty());
        CHECK(g.objects.remove(h) && g.server.CollectObjectsTouchingSphere({}, 1000, found) && found.empty());
        CHECK(g.objects.finish_frame());
        return g.unload_world();
    }
    bool worlds(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string error;
        unsigned count = 0;
        for (const char* world : {"24b_arena", "25_ancientone", "end"})
        {
            CHECK(g.request_world(world, error));
            for (std::size_t i = 0; i < g.entries.size(); ++i)
            {
                const auto& name = g.world.objects[i].class_name;
                if (name != "IshmaelAI" && name != "IshmaelREV")
                    continue;
                if (g.entries[i].state != EntryState::Created)
                    std::cerr << name << ": " << g.entries[i].detail << '\n';
                CHECK(g.entries[i].state == EntryState::Created);
                auto* a = dynamic_cast<IshmaelAI*>(g.objects.get(g.entries[i].object)->game.get());
                CHECK(a && a->RuntimeReady());
                ++count;
            }
            CHECK(g.unload_world());
        }
        CHECK(count == 3);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    return (mode == "keys"       ? keys(argv[2])
            : mode == "profiles" ? profiles(argv[2])
            : mode == "actions"  ? actions(argv[2])
            : mode == "teleport" ? teleport(argv[2])
            : mode == "death"    ? death(argv[2])
            : mode == "effects"  ? effects(argv[2])
            : mode == "sphere"   ? sphere(argv[2])
                                 : worlds(argv[2]))
               ? 0
               : 1;
}
