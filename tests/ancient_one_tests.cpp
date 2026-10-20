#include "brush_fixture.h"
#include "engine/world_model.h"
#include "game/original/AncientOne.h"
#include "game/server_adapter.h"
#include <cmath>
#include <iostream>
#include <set>
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
    bool setup(GameSession& g, const char* root)
    {
        Config c;
        c.data_dir = root;
        std::string error;
        CHECK(g.initialize() && g.resources.mount_game(c, error));
        auto floor = test_brush::box("floor", {-10000, -20, -10000}, {10000, -10, 10000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        CHECK(g.collision.build(floor, error));
        g.server.collision_ready = true;
        return true;
    }
    template <class T> T* create(GameSession& g, const char* name, DVector pos)
    {
        auto h = g.objects.create(g.objects.find_class(name), {}, Creation::World, {{"Pos", pos}});
        auto* a = dynamic_cast<T*>(g.server.HandleToObject(h));
        if (a && !a->RuntimeReady())
            std::cerr << name << ": " << a->runtime_error << '\n';
        return a && a->m_MoveObj.Update() ? a : nullptr;
    }
    bool profiles(const char* root)
    {
        for (int d : {0, 1, 2})
        {
            GameSession g;
            CHECK(setup(g, root));
            g.server.set_game_settings(d, 0);
            auto* a = create<AncientOne>(g, "AncientOne", {0, 1000, 0});
            auto* t = create<AncientOneTentacle>(g, "AncientOneTentacle", {1500, 1000, 0});
            CHECK(a && t && a->RuntimeReady() && t->RuntimeReady());
            float factor = d == 0 ? .25f : d == 2 ? 1.5f : 1;
            CHECK(a->m_damage.m_fHitPoints >= 22000 * factor && a->m_damage.m_fHitPoints <= 22001 * factor);
            CHECK(a->m_damage.m_fArmorPoints == 2000 * factor && !a->m_damage.apply_damage_physics &&
                  a->m_dwFlags == 0x100);
            CHECK(a->m_fHearingDist == 10000 && a->m_fSeeingDist == 18000 && a->m_fWalkSpeed == 0 &&
                  a->m_vScale.y >= 4.95f);
            CHECK(t->m_fSensingDist == 1000 && t->m_fSeeingDist == 2000 && t->m_vScale.y >= 2.45f && !t->m_bCabal);
            CHECK(a->AnimSound() != t->AnimSound() && a->m_InventoryMgr.CurrentWeapon()->data.m_nType == 46);
            CHECK(t->m_InventoryMgr.CurrentWeapon()->data.m_nType == 47 &&
                  t->m_InventoryMgr.CurrentWeapon()->damage_type == DamageType::Melee);
            CHECK(g.objects.advance(.002) && a->health_requests == 1 && !a->m_bCreateHealth &&
                  a->m_fFullHealth == a->m_damage.m_fHitPoints);
            CHECK(g.unload_world() && g.objects.snapshot().empty());
        }
        return true;
    }
    bool decisions(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create<AncientOne>(g, "AncientOne", {0, 1000, 0});
        auto* t = create<AncientOneTentacle>(g, "AncientOneTentacle", {1500, 1000, 0});
        CHECK(a && t && a->RuntimeReady() && t->RuntimeReady());
        CHECK(a->ComputeState(0) && a->m_nState == AI_Mgr::StateSearchVisual);
        CHECK(a->ComputeState(0) && a->m_nState == AI_Mgr::StateIdle);
        CHECK(a->ComputeState(1) && a->m_nState == AI_Mgr::StateAttackFar);
        a->m_vTargetPos = vector_add(a->m_MoveObj.GetPos(), {0, 0, 299});
        CHECK(a->MC_Fire_Stand() && a->m_InventoryMgr.CurrentWeapon()->data.m_nType == 47 &&
              !g.server.GetModelLooping(a->m_hObject));
        a->m_bAnimating = false;
        a->m_vTargetPos.z += 1;
        CHECK(a->MC_Fire_Stand() && a->m_InventoryMgr.CurrentWeapon()->data.m_nType == 46);
        t->m_vTargetPos = vector_add(t->m_MoveObj.GetPos(), {0, 0, 500});
        CHECK(t->ComputeState(1) && t->m_nState == AI_Mgr::StateAttackClose);
        t->m_vTargetPos.z += 1;
        CHECK(t->ComputeState(1) && t->m_nState == AI_Mgr::StateIdle);
        CHECK(t->MC_Idle() && g.server.GetModelLooping(t->m_hObject));
        CHECK(g.unload_world());
        return true;
    }
    bool scale(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* t = create<AncientOneTentacle>(g, "AncientOneTentacle", {0, 1000, 0});
        CHECK(t && t->RuntimeReady());
        CHECK(t->MC_Dead() && t->m_damage.m_fHitPoints == 200 && t->m_nState == AI_Mgr::StateSpecial2);
        CHECK(t->DispatchState() && !(g.server.GetObjectFlags(t->m_hObject) & FLAG_SOLID));
        for (int i = 0; i < 20; i++)
        {
            if (!t->DispatchState())
            {
                std::cerr << t->state_error << '\n';
                return false;
            }
        }
        CHECK(t->Metacmd == 2 && t->m_fScaleBy == .01f && t->m_bAnimating);
        CHECK(t->DispatchState() && t->m_nState == AI_Mgr::StateSpecial3);
        CHECK(t->DispatchState() && t->Metacmd == 1);
        // Exact ten-second threshold, independent of the scheduled AI callback.
        t->m_fTimeOfDeath = g.server.GetTime() - 10;
        CHECK(t->DispatchState() && t->Metacmd == 2);
        CHECK(t->DispatchState() && t->m_nState == AI_Mgr::StateSpecial1);
        CHECK(t->DispatchState());
        for (int i = 0; i < 20; i++)
            CHECK(t->DispatchState());
        CHECK(t->m_fScaleBy == 2.5f && t->Metacmd == 2 && (g.server.GetObjectFlags(t->m_hObject) & FLAG_SOLID));
        CHECK(t->DispatchState() && t->m_nState == AI_Mgr::StateIdle && !t->m_bHiding);
        t->m_bAnimating = false;
        t->Metacmd = 1;
        CHECK(t->MC_FadeOut());
        for (int i = 0; i < 64; i++)
            CHECK(t->MC_FadeOut());
        DVector color;
        float alpha;
        CHECK(g.server.GetObjectColor(t->m_hObject, color, alpha) && alpha == 0 && t->Metacmd == 2);
        CHECK(t->MC_FadeIn());
        for (int i = 0; i < 64; i++)
            CHECK(t->MC_FadeIn());
        CHECK(g.server.GetObjectColor(t->m_hObject, color, alpha) && alpha == 1 && t->Metacmd == 3);
        CHECK(g.unload_world());
        return true;
    }
    bool damage(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create<AncientOne>(g, "AncientOne", {0, 1000, 0});
        auto* t = create<AncientOneTentacle>(g, "AncientOneTentacle", {1500, 1000, 0});
        CHECK(a && t && a->RuntimeReady() && t->RuntimeReady() && g.objects.advance(.002));
        const auto hp = t->m_damage.m_fHitPoints;
        auto send = [&](AI_Mgr* from, AI_Mgr* to, float amount)
        {
            auto m = g.server.StartMessageToObject(from, to->m_hObject, damage_message);
            return m &&
                   g.server.WriteDamage(
                       m, {{0, 0, 1}, to->m_MoveObj.GetPos(), amount, DamageType::Normal, from->m_hObject}) == DE_OK &&
                   g.server.EndMessage(m) == DE_OK;
        };
        CHECK(send(a, t, 100) && t->ignored_boss_damage == 1 && t->m_damage.m_fHitPoints == hp);
        CHECK(send(t, a, 100) && a->health_requests == 2 && a->RuntimeReady());
        auto* other = create<AncientOneTentacle>(g, "AncientOneTentacle", {3000, 1000, 0});
        CHECK(other && other->RuntimeReady());
        CHECK(send(other, t, 10) && t->m_damage.m_fHitPoints < hp);
        t->m_damage.SetHitPoints(4);
        CHECK(send(other, t, 5) && t->m_damage.m_fHitPoints <= 0 && !t->m_bRemoveMe);
        CHECK(g.objects.engine_message(t->m_hObject, MID_UPDATE, nullptr));
        CHECK(t->m_damage.m_fHitPoints == 200 && t->m_nState == AI_Mgr::StateSpecial2 && t->revivals == 1);
        CHECK(g.unload_world());
        return true;
    }
    bool beam(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create<AncientOne>(g, "AncientOne", {0, 1000, -2000});
        CHECK(a && a->RuntimeReady());
        auto* t = create<AncientOneTentacle>(g, "AncientOneTentacle", {0, 1000, 200});
        CHECK(t && t->RuntimeReady());
        auto* b = create<AncientOne>(g, "AncientOne", {0, 1000, 1500});
        CHECK(b && b->RuntimeReady());
        CHECK(g.objects.advance(.002));
        g.server.SetObjectState(t->m_hObject, OBJSTATE_INACTIVE);
        g.server.SetObjectState(b->m_hObject, OBJSTATE_INACTIVE);
        CHECK(g.objects.advance(.002));
        // A controlled ray crosses the tentacle bounds, then the boss body.
        DVector pos = {0, t->m_MoveObj.GetPos().y, 0};
        auto* w = dynamic_cast<CAncientOneBeam*>(a->m_InventoryMgr.CurrentWeapon());
        CHECK(w);
        WeaponFire shot;
        shot.weapon = 46;
        shot.position = pos;
        shot.direction = {0, 0, 1};
        shot.seed = 42;
        const auto hp = t->m_damage.m_fHitPoints;
        auto result = w->Fire(shot);
        if (result != FireResult::Fired)
            std::cerr << "beam " << int(result) << ' ' << w->beam_error << ' ' << w->explosion_error << '\n';
        CHECK(result == FireResult::Fired && w->beam && w->last_shot.hit.object == b->m_hObject &&
              w->last_shot.damage_delivered);
        CHECK(t->m_damage.m_fHitPoints == hp && g.server.laser_beams.active.back().type == 1);
        CHECK(!g.server.laser_beams.active.back().skin_path.empty());
        CHECK(g.server.explosion_models.active.size() == 3 &&
              g.server.explosion_models.reports.back().models.size() == 3);
        CHECK(g.server.explosion_sprites.active.size() == 2 && g.server.explosion_rings.active.size() == 1);
        CHECK(g.server.explosion_lights.active.size() == 2 &&
              g.server.explosion_lights.active.back().settings.radius2 == 0);
        CHECK(g.server.explosion_fragments.active.empty() && g.server.explosion_flames.active.empty());
        const auto& m = g.server.explosion_models.active;
        CHECK(m[0].settings.scale1.x == 20 && m[1].settings.waveform == 2 && m[2].settings.duration == 1);
        CHECK(g.server.explosion_rings.active[0].settings.gravity == 350);
        CHECK(g.server.TeleportObject(b->m_hObject, &(pos = DVector{3000, 1000, 1500})) == DE_OK);
        CHECK(w->Fire(shot) == FireResult::Fired && w->last_shot.collision == SegmentResult::Miss);
        // Real head-gun branch and no CheckClearShot, including malformed pose refusal.
        a->m_vTargetPos = vector_add(a->m_MoveObj.GetPos(), {0, 0, 5000});
        CHECK(a->Fire() && a->fire_gun_node && a->clear_shot.rays == 0);
        auto* record = g.objects.get(a->m_hObject);
        auto model = record->model;
        record->model.reset();
        CHECK(!a->Fire() && !a->fire_error.empty());
        record->model = model;
        auto* claw = dynamic_cast<CAncientOneTentacle*>(t->m_InventoryMgr.CurrentWeapon());
        CHECK(claw);
        CHECK(std::string_view(claw->FireSound(false)) == "Sounds\\Weapons\\Knife\\fire.wav");
        claw->BeginFire(false);
        CHECK(std::string_view(claw->FireSound(false)) == "Sounds\\Weapons\\Knife\\fire.wav");
        CHECK(std::string_view(claw->FireSound(true)).starts_with("Sounds\\Slash"));
        claw->BeginFire(true);
        CHECK(std::string_view(claw->FireSound(false)).starts_with("Sounds\\Slash"));
        CHECK(g.unload_world());
        return true;
    }
    bool keys(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create<AncientOne>(g, "AncientOne", {0, 1000, 0});
        auto* b = create<AncientOne>(g, "AncientOne", {0, 1000, 3000});
        CHECK(a && b && a->RuntimeReady() && b->RuntimeReady());
        g.server.SetObjectState(b->m_hObject, OBJSTATE_INACTIVE);
        b->m_damage.can_damage = false;
        a->m_bSetShutdown = true;
        g.server.SetObjectState(a->m_hObject, OBJSTATE_ACTIVE);
        CHECK(a->SetNewTarget(b->m_hObject) && a->RefreshTargetPosition());
        CHECK(a->SetNewState(AI_Mgr::StateAttackFar));
        a->Metacmd = 2;
        CHECK(a->MC_Fire_Stand());
        for (int i = 0; i < 240 && !a->fire_shots; i++)
        {
            CHECK(g.objects.advance(1. / 60));
            CHECK(g.server.advance_models(1. / 60));
        }
        if (a->runtime_failed)
            std::cerr << a->runtime_error << '\n';
        CHECK(a->RuntimeReady() && a->fire_shots > 0 && a->model_keys > 0);
        std::cout << "Ancient actual model-key shots=" << a->fire_shots << "\n";
        CHECK(g.unload_world());
        return true;
    }
    bool spawn(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* a = create<AncientOne>(g, "AncientOne", {5000, 1000, 5000});
        CHECK(a && a->RuntimeReady());
        if (!a->SpawnTentacle())
        {
            std::cerr << a->state_error << '\n';
            return false;
        }
        auto* t = dynamic_cast<AncientOneTentacle*>(g.server.HandleToObject(a->last_tentacle));
        CHECK(t && t->RuntimeReady() && a->tentacles_spawned == 1);
        auto h = t->m_hObject;
        CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame() && g.server.IsObjectAlive(h));
        CHECK(g.unload_world());
        return true;
    }
    bool world(const char* root)
    {
        for (int d : {0, 1, 2})
        {
            GameSession g;
            Config c;
            c.data_dir = root;
            std::string error;
            CHECK(g.initialize() && g.resources.mount_game(c, error));
            g.server.set_game_settings(d, 0);
            CHECK(g.request_world("25_ancientone", error));
            if (!g.run_world(error))
            {
                std::cerr << error << '\n';
                for (auto& e : g.entries)
                    if (e.state == EntryState::Failed)
                        std::cerr << (g.objects.get(e.object) ? g.objects.get(e.object)->state.m_Name : "missing")
                                  << " " << e.detail << '\n';
                return false;
            }
            if (d == 1)
                for (auto id : g.objects.snapshot())
                {
                    auto* r = g.objects.get(id);
                    if (!r || std::string_view(r->state.m_Name) != "AncientOne2")
                        continue;
                    auto* a = dynamic_cast<AncientOne*>(r->game.get());
                    CHECK(a && a->runtime_updates == 0 && r->placement_nudges > 0);
                    const auto placed = r->state.m_Pos;
                    auto below = placed;
                    below.y -= r->dimensions.y * 1.4f;
                    CHECK(g.server.TeleportObject(r->handle, &below) == DE_OK);
                    a->runtime_updates = 1;
                    CHECK(g.server.ResolveInitialActorOverlap(r->handle) != DE_OK && r->state.m_Pos.y == below.y);
                    a->runtime_updates = 0;
                    CHECK(g.server.ResolveInitialActorOverlap(r->handle) == DE_OK &&
                          std::abs(r->state.m_Pos.y - placed.y) < .01f);
                }
            for (int i = 0; i < 180; i++)
                if (!g.step(1. / 60))
                {
                    std::cerr << error << '\n';
                    for (auto& e : g.step_errors())
                        std::cerr << e << '\n';
                    return false;
                }
            CHECK(g.run_world(error) && g.unload_world());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    bool ok = mode == "keys"        ? keys(argv[2])
              : mode == "profiles"  ? profiles(argv[2])
              : mode == "decisions" ? decisions(argv[2])
              : mode == "scale"     ? scale(argv[2])
              : mode == "damage"    ? damage(argv[2])
              : mode == "beam"      ? beam(argv[2])
              : mode == "world"     ? world(argv[2])
              : mode == "spawn"     ? spawn(argv[2])
                                    : false;
    return ok ? 0 : 1;
}
