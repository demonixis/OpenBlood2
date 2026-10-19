#include "brush_fixture.h"
#include "game/original/AmmoPickups.h"
#include "game/original/CameraObj.h"
#include "game/original/CivilianAI.h"
#include "game/original/CultistAI.h"
#include "game/original/DestructableBrush.h"
#include "game/original/SurfaceTypes.h"
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
        return std::abs(a - b) < .003f;
    }
    bool setup(GameSession& g, const char* root = "")
    {
        CHECK(g.objects.find_class("BaseClass") || g.initialize());
        auto floor = test_brush::box("floor", {-2000, -20, -2000}, {2000, -10, 2000});
        for (auto& s : floor.surfaces)
            s.flags = 1;
        std::string e;
        CHECK(g.collision.build(floor, e));
        g.server.collision_ready = true;
        if (*root)
        {
            Config cfg;
            cfg.data_dir = root;
            CHECK(g.resources.mount_game(cfg, e));
        }
        return true;
    }
    HOBJECT object(GameSession& g, DVector pos = {0, 10, 40}, const char* name = "object",
                   const char* type = "BaseClass")
    {
        ObjectCreateStruct s;
        s.m_Pos = pos;
        s.m_Flags = FLAG_SOLID;
        SAFE_STRCPY(s.m_Name, name);
        auto h = g.objects.create(g.objects.find_class(type), s);
        if (h)
            g.objects.get(h)->dimensions = {1, 2, 1};
        return h;
    }
    class Probe : public CivilianAI
    {
      public:
        bool remove = false, recurse = false, nested = true;
        bool ComputeState(int = -1) override
        {
            if (remove)
                GetServerDE()->RemoveObject(m_hObject);
            if (recurse)
                nested = AI_Update();
            return true;
        }
    };
    // A deliberately synthetic scheduler fixture: real AI_Update/state/engine services,
    // but injected animation data/initialized flag. Catalog tests cover actual initialization.
    Probe* actor(GameSession& g)
    {
        static const ClassInfo type{"RuntimeProbe",
                                    g.objects.find_class("CBaseCharacter"),
                                    []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); },
                                    {}};
        if (!g.objects.find_class(type.name) && !g.objects.register_class(type))
            return nullptr;
        ObjectCreateStruct s;
        s.m_ObjectType = OT_MODEL;
        s.m_Pos = {0, 10, 0};
        s.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(&type, s);
        auto* r = g.objects.get(h);
        auto* a = r ? dynamic_cast<Probe*>(r->game.get()) : nullptr;
        if (!a)
            return nullptr;
        auto model = std::make_shared<Model>();
        std::string e;
        if (!read_model(test_model::abc().b, *model, e))
            return nullptr;
        model->animations[0].keys.clear();
        model->animations[0].dimensions = {1, 2, 1};
        r->model = model;
        r->dimensions = {1, 2, 1};
        auto table = std::make_shared<CAnim_Sound>();
        for (auto& clip : table->m_nAnim_IDLE)
            clip = 0;
        if (!a->InitStatics(table) || !a->m_MoveObj.Init(h) || !a->m_InventoryMgr.Init(h, nullptr))
            return nullptr;
        a->m_damage.Init(h);
        a->m_damage.SetHitPoints(100);
        a->m_vDims = r->dimensions;
        a->npc_type = AI_Mgr::NPCType::Civilian;
        a->runtime_initialized = true;
        a->m_nState = AI_Mgr::StateInactive;
        g.server.SetNextUpdate(h, .001f);
        return a;
    }
    HOBJECT ammo_fixture(GameSession& g, DVector pos = {0, 10, 80}, const char* type = "BulletAmmoPU")
    {
        ObjectCreateStruct state;
        state.m_Pos = pos;
        auto h = g.objects.create(g.objects.find_class(type), state);
        if (auto* r = g.objects.get(h))
        {
            // Labelled synthetic pickup bounds; private case uses actual retail ABCs.
            r->state.m_Flags = FLAG_TOUCH_NOTIFY | FLAG_VISIBLE;
            r->dimensions = {2, 2, 2};
        }
        return h;
    }
    bool ammo_receive(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        auto h = ammo_fixture(g);
        CHECK(a && h);
        auto* pickup = dynamic_cast<AmmoPickup*>(g.objects.get(h)->game.get());
        CHECK(pickup);
        // Forged/replayed offers outside the pickup's active contact cannot grant ammo.
        auto packet = g.server.StartMessageToObject(pickup, a->m_hObject, add_ammo_message);
        CHECK(packet && g.server.WritePickupMessage(packet, pickup->value) == DE_OK);
        g.server.EndMessage(packet);
        CHECK(a->ammo_refusals == 1 && a->m_InventoryMgr.GetAmmoCount(1) == 0);
        a->m_dwFlags |= 0x200;
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, a->m_hObject));
        CHECK(a->ammo_refusals == 2 && !pickup->picked_up);
        a->m_dwFlags &= ~0x200u;
        a->m_damage.m_bDead = true;
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, a->m_hObject));
        CHECK(a->ammo_refusals == 3 && !pickup->picked_up);
        a->m_damage.m_bDead = false;
        a->m_InventoryMgr.SetAmmoCount(1, a->m_InventoryMgr.MaxAmmo(1));
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, a->m_hObject));
        CHECK(a->ammo_refusals == 4 && !pickup->picked_up);
        a->m_InventoryMgr.SetAmmoCount(1, a->m_InventoryMgr.MaxAmmo(1) - 7);
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, a->m_hObject));
        CHECK(a->ammo_collections == 1 && pickup->picked_up && a->last_ammo_source == h);
        CHECK(a->m_InventoryMgr.GetAmmoCount(1) == a->m_InventoryMgr.MaxAmmo(1));
        CHECK(!g.server.IsObjectAlive(h) && AmmoPickup::m_dwNumPickups == 1);
        CHECK(g.objects.finish_frame() && !g.objects.get(h) && AmmoPickup::m_dwNumPickups == 0);
        CHECK(g.server.message_count() == 0 && g.server.string_count() == 0);
        auto health = ammo_fixture(g, {100, 10, 0}, "HealthPU");
        CHECK(health && g.objects.engine_message(health, MID_TOUCHNOTIFY, a->m_hObject));
        CHECK(g.server.IsObjectAlive(health)); // Original AI does not receive powerups.
        return true;
    }
    bool ammo_search(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a && a->SetNewState(AI_Mgr::StateFindAmmo) && a->AI_Update());
        CHECK(a->m_nState == AI_Mgr::StateIdle && a->ammo_searches == 1);
        CHECK(a->m_InventoryMgr.ObtainWeapon(WEAP_BERETTA, 1) == InventoryResult::Ok);
        CHECK(a->m_InventoryMgr.ChangeWeaponSlot(1) == InventoryResult::Ok);
        a->m_fSeeingDist = 500;
        a->m_vTargetPos = {0, 10, -100};
        CHECK(a->MC_BestWeapon() && a->m_nState == AI_Mgr::StateFindAmmo);
        CHECK(a->AI_Update() && !a->runtime_failed && a->m_nState == AI_Mgr::StateEscapeRun);
        CHECK(a->searched_ammo == 1 && a->searched_ammo_class == "BulletAmmoPU");
        auto h = ammo_fixture(g);
        CHECK(h && a->SetNewState(AI_Mgr::StateFindAmmo) && a->AI_Update());
        // Original CanSee ignores nonsolid pickups. Do not invent pickup vision.
        CHECK(a->m_nState == AI_Mgr::StateEscapeRun && a->find.valid && !a->find.selected);
        g.objects.get(h)->state.m_Flags |= FLAG_RAYHIT; // Explicit branch fixture, not retail pickup flags.
        CHECK(a->SetNewState(AI_Mgr::StateFindAmmo) && a->AI_Update());
        CHECK(a->m_nState == AI_Mgr::StateFindAmmo && a->Metacmd == 2 && a->GetTrackedObject() == h);
        CHECK(a->m_MoveObj.path().size() == 1 && a->AI_Update() && a->Metacmd == 3);
        g.objects.get(a->m_hObject)->state.m_Flags &= ~FLAG_VISIBLE;
        CHECK(a->AI_Update() && a->velocity_commands > 0 && !a->runtime_failed);
        // A removed pickup no longer freezes the actor; its value-owned route remains.
        CHECK(g.objects.remove(h) && a->AI_Update() && !a->runtime_failed && !a->GetTrackedObject());
        CHECK(a->ammo_target_losses == 1 && a->m_MoveObj.path().size() == 1);
        // Reacquire through the real state, then interrupt it before another collector removes the pickup.
        auto next = ammo_fixture(g, {0, 10, 80});
        CHECK(next && g.objects.finish_frame());
        g.objects.get(next)->state.m_Flags |= FLAG_RAYHIT;
        CHECK(a->SetNewState(AI_Mgr::StateFindAmmo) && a->AI_Update() && a->searched_ammo_target == next);
        CHECK(a->SetNewState(AI_Mgr::StateInactive) && g.objects.remove(next) && g.objects.finish_frame());
        CHECK(a->AI_Update() && !a->runtime_failed && a->ammo_target_losses == 2 && !a->GetTrackedObject());
        a->m_nState = AI_Mgr::StateFindAmmo;
        a->Metacmd = 3; // Continue the retained synthetic route, without reacquiring a dead object.
        g.objects.get(a->m_hObject)->state.m_Pos = {0, 10, 80}; // Labelled arrival, not navigation proof.
        CHECK(a->m_MoveObj.Update() && a->AI_Update() && a->Metacmd == 4);
        CHECK(a->m_MoveObj.path().empty() && a->navigation_arrivals == 1 && a->AI_Update());
        CHECK(g.objects.finish_frame() && g.unload_world() && AmmoPickup::m_dwNumPickups == 0);
        return true;
    }
    bool animation_clearance(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        const auto h = a->m_hObject;
        auto* r = g.objects.get(h);
        auto model = std::make_shared<Model>(*r->model);
        model->animations.push_back(model->animations.front());
        model->animations[1].name = "tall_idle";
        model->animations[1].dimensions = {1, 8, 1};
        r->model = model;
        auto table = std::make_shared<CAnim_Sound>();
        for (auto& clip : table->m_nAnim_IDLE)
            clip = 1;
        CHECK(a->InitStatics(table));
        const auto ceiling = object(g, {0, 14, 0}, "ceiling");
        CHECK(ceiling);
        g.objects.get(ceiling)->dimensions = {10, .5f, 10};
        a->m_bSetShutdown = true;
        a->m_bCabal = false;
        CHECK(a->SetNewState(AI_Mgr::StatePassive));
        const auto position = r->state.m_Pos;
        r->state.m_Flags &= ~FLAG_VISIBLE; // Animation/lifecycle does not depend on rendering.
        for (int i = 0; i < 40; ++i)
            CHECK(g.step(.1));
        CHECK(a->RuntimeReady() && a->runtime_updates > 10 && a->animation_finishes > 0);
        CHECK(a->animation_resize_blocks == 1 && r->animation.animation == 1 && a->state_failures == 0);
        CHECK(near(r->dimensions.y, 2) && near(r->state.m_Pos.y, position.y) && near(a->m_vDims.y, 2) &&
              near(a->animation_requested_dims.y, 8));
        CHECK(g.objects.remove(ceiling) && g.objects.finish_frame());
        CHECK(a->SetAnimation(0) && a->SetAnimation(1) && near(r->dimensions.y, 8));
        CHECK(near(r->state.m_Pos.y, position.y + 6));
        CHECK(g.objects.remove(h) && !a->SetAnimation(0));
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        return true;
    }
    bool clearance_private(const char* root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        ObjectId previous{};
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error) && g.enter_player(error) && !g.objects.get(previous));
            auto* player = g.server_shell.Player(g.local_client);
            CHECK(player);
            player->m_damage.can_damage = false; // Diagnostic relocation, not a retail playthrough.
            for (int frame = 0; frame < 780; ++frame)
                CHECK(g.step(1. / 60)); // Authored intro completes normally.
            unsigned encounters = 0;
            const auto ids = g.objects.snapshot();
            for (auto id : ids)
            {
                auto* r = g.objects.get(id);
                auto* ai = r ? dynamic_cast<AI_Mgr*>(r->game.get()) : nullptr;
                if (!ai || ai->npc_type != AI_Mgr::NPCType::Cultist)
                    continue;
                auto position = r->state.m_Pos;
                position.z += 150;
                position.y = 170; // Existing train-floor encounter height, not the NPC's scaled center.
                CHECK(g.server.TeleportObject(player->m_hObject, &position) == DE_OK &&
                      player->TeleportClientToServerPos());
                for (int frame = 0; frame < 600; ++frame)
                {
                    const bool stepped = g.step(1. / 60);
                    if (!stepped)
                        std::cerr << "Encounter " << encounters << ": " << g.trace.back() << '\n';
                    CHECK(stepped);
                }
                if (++encounters == 2)
                    break;
            }
            CHECK(encounters == 2);
            std::uint64_t blocks = 0, shots = 0;
            for (auto id : ids)
            {
                auto* r = g.objects.get(id);
                auto* ai = r ? dynamic_cast<AI_Mgr*>(r->game.get()) : nullptr;
                if (!ai || ai->npc_type != AI_Mgr::NPCType::Cultist)
                    continue;
                if (ai->runtime_failed)
                    std::cerr << ai->runtime_error << '\n';
                CHECK(ai->RuntimeReady());
                blocks += ai->animation_resize_blocks;
                shots += ai->fire_shots;
                if (ai->animation_resize_blocks)
                    previous = id;
            }
            std::cout << "SubwayA relocation cycle " << cycle << ": " << blocks << " blocked resizes, " << shots
                      << " original AI shots; no suspended Cultist\n";
            CHECK(blocks > 0 && shots > 0);
            auto* body = g.objects.get(previous);
            auto* victim = body ? dynamic_cast<AI_Mgr*>(body->game.get()) : nullptr;
            CHECK(victim);
            const auto handle = body->handle;
            // Explicit ordinary-death probe after blocked clip changes. Avoid
            // random gib selection so requested/physical bounds reach CreateCorpse.
            victim->m_dwFlags |= 0x800u; // Source NEVERGIB.
            victim->m_damage.node_hit = 2;
            auto message = g.server.StartMessageToObject(player, handle, damage_message);
            CHECK(message &&
                  g.server.WriteDamage(
                      message, {{0, 0, 1}, body->state.m_Pos, 1, DamageType::Death, player->m_hObject}) == DE_OK &&
                  g.server.EndMessage(message) == DE_OK);
            HOBJECT corpse = nullptr;
            for (int frame = 0; frame < 180 && g.objects.get(previous); ++frame)
            {
                CHECK(g.step(1. / 60));
                if (const auto* remaining = g.objects.get(previous))
                    corpse = dynamic_cast<AI_Mgr*>(remaining->game.get())->last_corpse;
            }
            if (auto* remaining = g.objects.get(previous))
                std::cerr << dynamic_cast<AI_Mgr*>(remaining->game.get())->runtime_error << '\n';
            CHECK(!g.objects.get(previous)); // Dead actor cannot remain a frozen solid blocker.
            CHECK(g.server.IsObjectAlive(corpse) &&
                  g.server.IsKindOf(g.server.GetObjectClass(corpse), g.server.GetClass("CCorpse")) &&
                  !(g.server.GetObjectFlags(corpse) & FLAG_SOLID));
        }
        CHECK(g.unload_world() && !g.objects.get(previous) && g.objects.snapshot().empty());
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0 && g.server.client_count() == 0);
        return true;
    }
    bool activation(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = a->m_hObject;
        auto observer = object(g, {0, 10, 50});
        CHECK(observer);
        CHECK(g.step(.01) && a->runtime_updates == 1 && a->m_bSetShutdown);
        auto* r = g.objects.get(h);
        CHECK(r->activity == OBJSTATE_AUTODEACTIVATE_NOW && r->scheduled);
        CHECK(g.step(.5) && a->runtime_updates == 1 && g.server.PingObjects(observer));
        CHECK(r->activity == OBJSTATE_ACTIVE && r->activation_pings == 1);
        CHECK(g.step(.01) && a->runtime_updates == 2 && a->runtime_desyncs == 0);
        CHECK(g.step(3) && r->activity == OBJSTATE_AUTODEACTIVATE_NOW && a->runtime_updates == 2);
        CHECK(g.server.PingObjects(observer) && g.step(.01) && a->runtime_desyncs == 1);
        CHECK(r->due > g.objects.time() && r->due <= g.objects.time() + .5);
        CHECK(g.step(.5) && a->runtime_updates == 4);
        g.server.SetObjectState(h, OBJSTATE_INACTIVE);
        CHECK(g.server.PingObjects(observer) && r->activity == OBJSTATE_INACTIVE);
        CHECK(!g.objects.engine_message(h, MID_TOUCHNOTIFY, observer));
        g.server.SetObjectState(h, OBJSTATE_INACTIVE_TOUCH);
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, observer));
        CHECK(g.server.SetDeactivationTime(h, .25f));
        g.server.SetObjectState(h, OBJSTATE_ACTIVE);
        CHECK(r->auto_deactivate && g.step(.25) && r->activity == OBJSTATE_AUTODEACTIVATE_NOW);
        CHECK(!g.objects.engine_message(h, MID_TOUCHNOTIFY, observer));
        CHECK(!g.server.SetDeactivationTime(h, -1) && !g.server.SetDeactivationTime(h, NAN));
        CHECK(g.server.SetDeactivationTime(h, 0) && !r->auto_deactivate);
        return true;
    }
    bool visibility(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto observer = object(g, {0, 10, 50});
        auto h = object(g, {0, 10, 0});
        auto blocker = object(g, {0, 10, 25});
        CHECK(observer && h && blocker);
        CHECK(g.server.SetDeactivationTime(h, 3));
        g.server.SetObjectState(h, OBJSTATE_AUTODEACTIVATE_NOW);
        CHECK(g.server.PingObjects(observer) && g.objects.get(h)->activation_pings == 0);
        CHECK(g.objects.remove(blocker) && g.server.PingObjects(observer) && g.objects.get(h)->activation_pings == 1);
        auto pings = g.objects.get(h)->activation_pings;
        g.server.collision_ready = false;
        CHECK(!g.server.PingObjects(observer) && g.objects.get(h)->activation_pings == pings);
        CHECK(g.objects.remove(observer) && !g.server.PingObjects(observer) && g.objects.finish_frame());
        return true;
    }
    HOBJECT activation_brush(GameSession& g, const char* type, Properties props = {})
    {
        const auto name = "activation-brush" + std::to_string(g.world.models.size());
        g.world.models.push_back(test_brush::box(name, {-20, 0, 22}, {20, 40, 28}));
        props["Name"] = name;
        props.try_emplace("Flags", std::int32_t(FLAG_SOLID));
        if (std::string_view(type) == "CDestructableBrush")
            props.try_emplace("HitPoints", 10.f);
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, props);
    }
    bool visibility_filter(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto observer = object(g, {0, 10, 50}, "observer");
        auto target = object(g, {0, 10, 0}, "activation target");
        CHECK(observer && target && g.server.SetDeactivationTime(target, 3));
        auto* r = g.objects.get(target);
        auto h = activation_brush(g, "CDestructableBrush",
                                  {{"SurfaceType", std::int32_t(SURFTYPE_GLASS)}, {"CanDamage", false}});
        auto* glass = h ? dynamic_cast<CDestructableBrush*>(g.objects.get(h)->game.get()) : nullptr;
        CHECK(glass);
        auto check = [&](bool visible)
        {
            g.server.SetObjectState(target, OBJSTATE_AUTODEACTIVATE_NOW);
            const auto before = r->activation_pings;
            if (!g.server.PingObjects(observer))
                return false;
            return r->activation_probe.observer == g.objects.get(observer)->id &&
                   r->activation_probe.visibility ==
                       (visible ? ActivationVisibility::Visible : ActivationVisibility::Blocked) &&
                   r->activation_probe.refreshed == visible && r->activation_pings == before + unsigned(visible) &&
                   r->activity == (visible ? OBJSTATE_ACTIVE : OBJSTATE_AUTODEACTIVATE_NOW);
        };
        CHECK(check(true)); // Source CanDamage does not define glass visibility.
        SegmentHit hit;
        CHECK(g.server.IntersectSegment({0, 10, 50}, {0, 10, 0}, observer, hit) == SegmentResult::Hit &&
              hit.object == h);
        glass->destructable = false;
        CHECK(check(false) && r->activation_probe.hit_object == g.objects.get(h)->id);
        glass->destructable = true;
        glass->surface = SURFTYPE_WOOD;
        glass->fire_through = true;
        CHECK(check(false)); // FireThrough is a separate weapon rule.
        glass->surface = SURFTYPE_GLASS;
        CHECK(check(true));
        const auto checks = r->activation_probe.checks;
        g.server.SetObjectState(target, OBJSTATE_INACTIVE);
        CHECK(g.server.PingObjects(observer) && r->activity == OBJSTATE_INACTIVE &&
              r->activation_probe.checks == checks);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        for (const auto* type : {"Water", "Ladder"})
        {
            h = activation_brush(g, type);
            CHECK(h);
            g.server.SetObjectFlags(h, g.server.GetObjectFlags(h) | FLAG_SOLID);
            CHECK(check(true)); // Excludes every VolumeBrush subclass, even a solid diagnostic volume.
            CHECK(g.objects.remove(h) && g.objects.finish_frame());
        }
        h = activation_brush(g, "Door", {{"FireThrough", true}});
        CHECK(h && check(false) && r->activation_probe.hit_object == g.objects.get(h)->id);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        auto wall = test_brush::box("world", {-20, 0, 22}, {20, 40, 28});
        for (auto& surface : wall.surfaces)
        {
            surface.flags = 1;
            surface.texture_flags = SURFTYPE_GLASS;
        }
        std::string error;
        CHECK(g.collision.build(wall, error) && check(false));
        CHECK(!r->activation_probe.hit_object && r->activation_probe.polygon != UINT32_MAX);
        return true; // Object filtering never invents transparent baked world geometry.
    }
    bool visibility_atomic(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto observer = object(g, {0, 10, 50});
        auto target = object(g, {0, 10, 0});
        auto invalid = object(g, {0, 10, 50}); // Invalid zero-length ray after a valid candidate.
        CHECK(observer && target && invalid);
        g.server.SetObjectFlags(invalid, 0); // Do not obstruct the first candidate's ray.
        CHECK(g.server.SetDeactivationTime(target, 3) && g.server.SetDeactivationTime(invalid, 3));
        g.server.SetObjectState(target, OBJSTATE_AUTODEACTIVATE_NOW);
        g.server.SetObjectState(invalid, OBJSTATE_AUTODEACTIVATE_NOW);
        auto* r = g.objects.get(target);
        CHECK(!g.server.PingObjects(observer));
        CHECK(r->activation_probe.visibility == ActivationVisibility::Visible && !r->activation_probe.refreshed &&
              r->activation_pings == 0 && r->activity == OBJSTATE_AUTODEACTIVATE_NOW);
        CHECK(g.objects.get(invalid)->activation_probe.visibility == ActivationVisibility::Invalid &&
              g.objects.get(invalid)->activation_pings == 0);
        CHECK(g.objects.remove(invalid) && g.server.PingObjects(observer));
        CHECK(r->activation_pings == 1 && r->activation_probe.refreshed && r->activity == OBJSTATE_ACTIVE);
        const auto checks = r->activation_probe.checks;
        g.server.collision_ready = false;
        CHECK(!g.server.PingObjects(observer) && r->activation_probe.checks == checks);
        const auto old_observer = r->activation_probe.observer;
        CHECK(g.objects.remove(observer) && g.objects.finish_frame() && !g.objects.get(old_observer));
        CHECK(!g.server.PingObjects(observer) && r->activation_probe.observer == old_observer);
        const auto target_id = r->id;
        CHECK(g.unload_world() && !g.objects.get(target_id));
        return true;
    }
    bool lifetime(const char*)
    {
        for (bool remove : {false, true})
        {
            GameSession g;
            CHECK(setup(g));
            auto* a = actor(g);
            CHECK(a);
            a->m_nState = AI_Mgr::StateEscapeHide;
            a->remove = remove;
            a->recurse = !remove;
            auto h = a->m_hObject;
            CHECK(!a->AI_Update());
            if (remove)
                CHECK(!g.server.IsObjectAlive(h));
            else
                CHECK(a->runtime_failed && !a->nested && g.objects.get(h)->activity == OBJSTATE_INACTIVE &&
                      !g.objects.get(h)->scheduled);
            CHECK(g.unload_world() && g.objects.snapshot().empty() && g.server.string_count() == 0 &&
                  g.server.message_count() == 0);
        }
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = a->m_hObject;
        a->m_bRemoveMe = true;
        CHECK(g.step(.01) && !g.objects.get(h));
        return true;
    }
    bool targets(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = object(g, {0, 10, 50}, "target", "CBaseCharacter");
        CHECK(h);
        auto* c = dynamic_cast<CBaseCharacter*>(g.server.HandleToObject(h));
        CHECK(c);
        c->m_damage.Init(h);
        c->m_damage.SetHitPoints(100);
        a->m_fSensingDist = 20;
        a->m_fSeeingDist = 100;
        CHECK(a->SetNewTarget(h) && a->AI_Update() && a->GetTarget() == h && a->target_position_valid);
        CHECK(a->m_bHasTargeted && a->m_fSensingDist == 100 && near(a->m_vTargetPos.z, 50));
        auto blocker = object(g, {0, 10, 25});
        CHECK(blocker);
        // ALWAYSCLEAR bypasses sight, but never the checked current pose/character type.
        a->m_dwFlags |= 0x100;
        CHECK(a->AI_Update() && a->GetTarget() == h);
        a->m_dwFlags &= ~0x100u;
        CHECK(a->AI_Update() && !a->GetTarget() && a->runtime_target_losses == 1);
        CHECK(g.objects.remove(blocker) && g.objects.finish_frame());
        a->m_dwFlags |= 0x100;
        CHECK(a->SetNewTarget(h));
        c->m_damage.m_bDead = DTRUE;
        CHECK(a->AI_Update() && !a->GetTarget() && a->runtime_target_losses == 2);
        a->m_nState = AI_Mgr::StateInactive;
        c->m_damage.m_bDead = DFALSE;
        CHECK(a->SetNewTarget(h) && g.objects.remove(h) && a->AI_Update() && !a->GetTarget());
        CHECK(g.objects.finish_frame());
        auto wrong = object(g, {10, 10, 20});
        CHECK(wrong && a->SetNewTarget(wrong));
        CHECK(!a->AI_Update() && a->runtime_failed && a->runtime_error.find("not a character") != std::string::npos);
        return true;
    }
    bool tracking(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = object(g);
        CHECK(h && a->SetTrackedObject(h) && a->AI_Update() && a->tracked_position_valid);
        CHECK(near(a->m_vTrackObjPos.z, 40));
        g.objects.get(h)->state.m_Pos.z = 60;
        CHECK(a->AI_Update() && near(a->m_vTrackObjPos.z, 60));
        CHECK(g.objects.remove(h) && !a->AI_Update() && !a->GetTrackedObject() && a->runtime_failed);
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool camera(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = g.objects.create(g.objects.find_class("CameraObj"));
        CHECK(h);
        g_hActiveCamera = h;
        a->m_nState = AI_Mgr::StateAttackFar;
        auto* r = g.objects.get(a->m_hObject);
        r->velocity = {4, 5, 6};
        CHECK(a->AI_Update() && a->runtime_camera_pauses == 1 && !a->m_bSetShutdown);
        CHECK(near(r->velocity.x, 0) && near(r->velocity.y, 5) && near(r->velocity.z, 0));
        a->m_nState = AI_Mgr::StateIdle;
        CHECK(a->AI_Update() && a->runtime_camera_pauses == 1 && a->m_bSetShutdown);
        CHECK(g.objects.remove(h) && g.objects.finish_frame());
        g_hActiveCamera = nullptr;
        a->m_nState = AI_Mgr::StateDying;
        CHECK(a->AI_Update() && a->death_starts == 1 && !a->runtime_failed);
        // This injected model has no resource filename; transfer must retain a failed visible actor.
        CHECK(!a->AI_Update() && a->runtime_error.find("Corpse class/model filenames") != std::string::npos &&
              !a->m_bRemoveMe);
        return true;
    }
    bool spatial(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = a->m_hObject;
        CHECK(g.server.ConfigureCharacterBody(h, 250, 10) && g.objects.get(h)->character_body);
        CHECK(!g.server.ConfigureCharacterBody(h, 0, 10) && !g.server.ConfigureCharacterBody(h, 250, NAN));
        CHECK(g.server.ScaleObject(h, {2, 3, 4}) && g.objects.get(h)->state.m_Scale.y == 3);
        CHECK(!g.server.ScaleObject(h, {0, 1, 1}) && !g.server.ScaleObject(h, {1, INFINITY, 1}));
        auto normal = object(g);
        CHECK(normal && g.server.ScaleObject(normal, {2, 3, 4})); // Original OT_NORMAL projectile metadata.
        CHECK(g.objects.get(normal)->state.m_Scale.x == 2 && g.objects.get(normal)->state.m_Scale.y == 3 &&
              g.objects.get(normal)->state.m_Scale.z == 4 && !g.objects.get(normal)->model &&
              !g.objects.get(normal)->sprite);
        CHECK(!g.server.ScaleObject(normal, {-1, 1, 1}));
        g.world.min = {-100, -100, -100};
        g.world.max = {100, 100, 100};
        g.world.models.emplace_back();
        auto* r = g.objects.get(h);
        r->state.m_Pos = {100, 10, 0};
        r->state.m_Flags |= FLAG_REMOVEIFOUTSIDE;
        CHECK(g.step(.01) && g.objects.get(h));
        r->state.m_Pos.x = 100.1f;
        CHECK(g.step(.01) && !g.objects.get(h));
        return true;
    }
    bool body(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = a->m_hObject;
        auto* r = g.objects.get(h);
        r->state.m_Pos = {0, -7.999f, 0};
        r->state.m_Flags |= FLAG_GRAVITY;
        CHECK(g.server.ConfigureCharacterBody(h, 250, 10));
        r->velocity = {100, 0, 0};
        CHECK(g.server.advance_physics(.1));
        CHECK(near(r->velocity.x, 100 * std::exp(-1.f)) && near(r->state.m_Pos.x, 10 * (1 - std::exp(-1.f))));
        CHECK(r->body_steps == 12);
        // Standing on a character changes follow policy; world support preserves it.
        auto support = object(g, {0, 10, 0}, "support", "CBaseCharacter");
        CHECK(support);
        r->state.m_Pos = {0, 14.001f, 0};
        r->velocity = {};
        CHECK(a->AI_Update() && (r->state.m_Flags & FLAG_DONTFOLLOWSTANDING));
        CHECK(g.objects.remove(support) && g.objects.finish_frame());
        support = object(g, {0, 10, 0});
        CHECK(support);
        CHECK(a->AI_Update() && !(r->state.m_Flags & FLAG_DONTFOLLOWSTANDING));
        auto samples = a->m_MoveObj.samples;
        g.server.SetDeactivationTime(h, 0);
        g.server.SetObjectState(h, OBJSTATE_ACTIVE);
        CHECK(g.step(.01) && a->m_MoveObj.samples == samples + 1);
        return true;
    }
    bool send(GameSession& g, HOBJECT from, HOBJECT to, const char* text)
    {
        auto s = g.server.CreateString(text);
        auto m = g.server.StartMessageToObject(g.server.HandleToObject(from), to, 1000);
        CHECK(s && m && g.server.WriteToMessageHString(m, s) == DE_OK);
        g.server.FreeString(s);
        CHECK(g.server.EndMessage(m) == DE_OK);
        return true;
    }
    bool triggers(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        auto* a = actor(g);
        CHECK(a);
        auto h = object(g, {0, 10, 50}, "speaker", "CBaseCharacter");
        CHECK(h);
        a->m_nState = AI_Mgr::StateIdle;
        CHECK(send(g, h, a->m_hObject, "trigger") && a->m_nState == AI_Mgr::StateIdle);
        CHECK(send(g, h, a->m_hObject, "TRIGGER") && a->m_nState == AI_Mgr::StateSpecial1 && a->GetTarget() == h);
        a->npc_labtech = true;
        CHECK(send(g, h, a->m_hObject, "TRIGGER") && a->m_nState == AI_Mgr::StateSpecial1);
        a->m_nState = AI_Mgr::StateIdle;
        CHECK(send(g, h, a->m_hObject, "TRIGGER") && a->m_nState == AI_Mgr::StateEscapeHide);
        CHECK(g.server.string_count() == 0 && g.server.message_count() == 0);
        // Forced original SCRIPT update runs immediately, including shutdown selection.
        CHECK(send(g, h, a->m_hObject, "SCRIPT 0; WAIT 1;") && a->m_nState == AI_Mgr::StateScript);
        CHECK(a->runtime_updates == 1 && !a->runtime_failed && a->m_bSetShutdown);
        return true;
    }
    AI_Mgr* npc(GameSession& g, const char* type, Properties p = {}, Creation reason = Creation::World)
    {
        p.insert_or_assign("Pos", DVector{0, 100, 0});
        auto h = g.objects.create(g.objects.find_class(type), {}, reason, std::move(p));
        return g.server.IsObjectAlive(h) ? dynamic_cast<AI_Mgr*>(g.server.HandleToObject(h)) : nullptr;
    }
    bool ammo_private(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        ObjectId old{};
        for (int reload = 0; reload < 2; ++reload)
        {
            CHECK(!g.objects.get(old));
            g.world_path = "synthetic NPC ammunition arena (retail resources)";
            g.state = SessionState::Inspection;
            CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                                   {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 200}}}));
            g.server_shell.PreStartWorld();
            std::string error;
            CHECK(g.server_shell.PostStartWorld() && g.enter_player(error));
            auto* a = npc(g, "CultistAI", {{"AIState", std::string("IDLE")}});
            CHECK(a && a->RuntimeReady());
            old = g.objects.get(a->m_hObject)->id;
            auto& inventory = a->m_InventoryMgr;
            inventory.SetAmmoCount(AMMO_BULLET, 0);
            auto target = g.client_shell.player;
            CHECK(target && a->SetNewTarget(target) && a->RefreshTargetPosition());
            CHECK(a->MC_BestWeapon() && a->m_nState == AI_Mgr::StateFindAmmo);
            CHECK(a->AI_Update() && !a->runtime_failed && a->m_nState == AI_Mgr::StateEscapeRun);
            auto pos = g.objects.get(a->m_hObject)->state.m_Pos;
            pos.z += 5; // Deliberate overlapping placement for actual contact dispatch.
            auto initial = pos;
            initial.z += 100; // Ground placement must not collect it before tracking is established.
            auto ammo = g.objects.create(g.objects.find_class("BulletAmmoPU"), {}, Creation::World,
                                         {{"Pos", initial}, {"Name", std::string("NPC ammo contact fixture")}});
            auto* r = g.objects.get(ammo);
            CHECK(r && r->model && r->model_error.empty());
            CHECK(a->SetTrackedObject(ammo) && a->SetNewState(AI_Mgr::StateFindAmmo));
            CHECK(g.server.TeleportObject(ammo, &pos) == DE_OK);
            a->searched_ammo_target = ammo; // Labelled completed-route fixture, not autonomous retail pursuit.
            a->Metacmd = 4;                 // Preserve completed route semantics after a contact removes its pickup.
            CHECK(g.server.advance_physics(.02));
            CHECK(a->ammo_collections == 1 && inventory.GetAmmoCount(1) == 100);
            CHECK(!g.server.IsObjectAlive(ammo) && a->AI_Update() && !a->runtime_failed);
            CHECK(a->ammo_target_losses == 1 && !a->GetTrackedObject());
            CHECK(g.objects.finish_frame() && !g.objects.get(ammo));
            CHECK(!g.server.pickup_reports.empty() && g.server.pickup_reports.back().voice != 0);
            std::cout << "Retail NPC/ammo arena reload=" << reload << " searches=" << a->ammo_searches
                      << " collections=" << a->ammo_collections << " bullets=" << inventory.GetAmmoCount(1)
                      << " voice=" << g.server.pickup_reports.back().voice << '\n';
            CHECK(g.unload_world() && g.objects.snapshot().empty() && AmmoPickup::m_dwNumPickups == 0);
            CHECK(g.server.message_count() == 0 && g.server.string_count() == 0);
            CHECK(setup(g, root));
        }
        return true;
    }
    bool missing(const char*)
    {
        GameSession g;
        CHECK(setup(g));
        for (int i = 0; i < 2; ++i)
        {
            CHECK(!npc(g, "CultistAI") && !npc(g, "CivilianAI"));
            CHECK(g.objects.finish_frame() && g.objects.snapshot().empty());
            CHECK(AI_Mgr::m_dwNumCabal == 0 && CDestructable::m_dwNumDestructables == 0);
        }
        return true;
    }
    bool catalog(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        for (auto type : {"CultistAI", "CivilianAI"})
            for (bool male : {true, false})
                for (int difficulty : {0, 1, 2, 3})
                {
                    g.server.set_game_settings(difficulty, 0);
                    auto* a =
                        npc(g, type, {{"Male", male}, {"AIState", std::string("GUARD")}, {"RandomHitPoints", 1.f}});
                    if (!a)
                    {
                        for (const auto& d : g.objects.diagnostics())
                            std::cerr << d << '\n';
                        return false;
                    }
                    auto h = a->m_hObject;
                    auto* r = g.objects.get(h);
                    float factor = difficulty == 0 ? .25f : difficulty == 2 ? 1.5f : 1.f;
                    CHECK(a->RuntimeReady() && r->model && a->npc_male == male && a->m_nState == AI_Mgr::StateGuard);
                    CHECK(near(a->m_damage.m_fHitPoints, 31 * factor) &&
                          near(a->m_InventoryMgr.damage_multiplier,
                               (type == std::string_view("CultistAI") ? .5f : 1) * factor));
                    CHECK(a->m_vGuardPos.y == 100 && r->state.m_Pos.y < 100 && r->auto_deactivate == false);
                    CHECK(r->state.m_Scale.y >= .95f && r->state.m_Scale.y <= 1.1f &&
                          near(r->state.m_Scale.x, 2 - r->state.m_Scale.y));
                    CHECK((r->state.m_Flags & FLAG_REMOVEIFOUTSIDE) && r->character_body &&
                          r->body_properties.mass == 250 && r->body_properties.friction == 10);
                    CHECK(a->m_MoveObj.initialized && r->scheduled && a->AnimSound());
                    std::cout << type << " male=" << male << " difficulty=" << difficulty
                              << " hp=" << a->m_damage.m_fHitPoints << " model=" << r->state.m_Filename << '\n';
                    CHECK(g.objects.remove(h) && g.objects.finish_frame());
                }
        g.server.set_game_settings(1, 0);
        auto* a = npc(g, "CivilianAI", {{"Male", false}, {"LabTech", true}, {"Scared", true}});
        CHECK(a && a->npc_labtech && !a->npc_male && dynamic_cast<CivilianAI*>(a)->m_bScared);
        CHECK(a->AnimSound()->GetSoundRoot() == "sounds/enemies/civilian");
        CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame());
        a = npc(g, "CultistAI", {{"Male", false}}, Creation::String);
        CHECK(a && a->npc_male); // source world-only subclass properties
        auto* shared_table = a->AnimSound();
        DVector separated{300, 100, 0};
        CHECK(g.server.TeleportObject(a->m_hObject, &separated) == DE_OK);
        {
            GameSession rejected;
            CHECK(!rejected.initialize());
        }
        auto* other = npc(g, "CultistAI");
        CHECK(other && other->AnimSound() == shared_table);
        CHECK(g.unload_world() && AI_Mgr::m_dwNumCabal == 0 && AI_Mgr::m_dwNumMonster == 0);
        return true;
    }
    bool sparse_weapons(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        Properties p{{"AIWeapon1", std::string("GIDEON_SHIELD")}, {"AIWeapon2", std::string("ENERGY_BLAST")},
                     {"AIWeapon3", std::string("NAGA_BLAST")},    {"AIWeapon4", std::string("GIDEON_WIND")},
                     {"AIWeapon5", std::string("GIDEON_SHIELD")}, {"AIState", std::string("INACTIVE")}};
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            auto* a = npc(g, "CultistAI", p);
            CHECK(a && a->RuntimeReady());
            const DBYTE expected[]{41, 27, 38, 42};
            for (unsigned i = 0; i < 4; ++i)
                CHECK(a->m_InventoryMgr.GetWeapon(i) && a->m_InventoryMgr.GetWeapon(i)->data.m_nType == expected[i]);
            CHECK(!a->m_InventoryMgr.GetWeapon(4) && a->m_InventoryMgr.CurrentWeapon()->data.m_nType == 41);
            CHECK(g.objects.remove(a->m_hObject) && g.objects.finish_frame());
        }
        CHECK(g.unload_world());
        std::cout << "Sparse IDs 41/27/38/42 and duplicate fifth slot initialized three times through original NPC "
                     "lifecycle.\n";
        return true;
    }
    bool failures(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        for (auto props : {Properties{{"AIWeapon1", std::string("HOWITZER")}}, Properties{{"RobeSkin", true}},
                           Properties{{"RandomHitPoints", -1.f}}})
        {
            CHECK(!npc(g, "CultistAI", props));
            CHECK(g.objects.finish_frame() && g.objects.snapshot().empty());
        }
        // Retail AddInventoryWeapon(INV_PROXIMITY, n) stores an unused slot-10 inventory weapon.
        auto* bomber = npc(g, "CultistAI", {{"Proximity_Bombs", 3.f}});
        CHECK(bomber && bomber->RuntimeReady() && bomber->m_InventoryMgr.GetWeapon(10) &&
              bomber->m_InventoryMgr.GetAmmoCount(AMMO_PROXIMITYBOMB) == 3 &&
              bomber->m_InventoryMgr.CurrentWeapon() != bomber->m_InventoryMgr.GetWeapon(10));
        CHECK(g.objects.remove(bomber->m_hObject) && g.objects.finish_frame() && g.objects.snapshot().empty());
        CHECK(!npc(g, "CivilianAI", {}, Creation::Restore) && g.objects.finish_frame() && g.objects.snapshot().empty());
        CHECK(AI_Mgr::m_dwNumCabal == 0 && g.server.string_count() == 0 && g.server.message_count() == 0);
        return true;
    }
    bool arena(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string e;
        g.world_path = "synthetic NPC integration arena";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 200}}}));
        auto* a = dynamic_cast<CultistAI*>(npc(g, "CultistAI"));
        CHECK(a);
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld());
        if (!g.enter_player(e))
        {
            std::cerr << e << '\n';
            return false;
        }
        auto* player = g.server_shell.Player(g.local_client);
        CHECK(player);
        for (int tick = 0; tick < 600; ++tick)
            CHECK(g.step(1.0 / 60));
        std::cout << "Arena state=" << a->m_nState << " phase=" << a->Metacmd << " updates=" << a->runtime_updates
                  << " shots=" << a->fire_shots << " error=" << a->runtime_error
                  << " player HP=" << player->m_damage.m_fHitPoints << '\n';
        CHECK(a->RuntimeReady() && a->runtime_updates > 15 && a->attack_updates > 0 && a->fire_shots > 0);
        CHECK(player->m_damage.m_fHitPoints < 100);
        CHECK(a->m_InventoryMgr.CurrentWeapon()->m_hHandModel);
        return true;
    }
    bool subway(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        std::string e;
        ObjectId old{};
        for (int load = 0; load < 2; ++load)
        {
            CHECK(g.request_world("01_SubwayA", e) && !g.objects.get(old));
            unsigned cultists = 0, civilians = 0, bosses = 0, failed = 0, unsupported = 0, created = 0;
            for (auto& entry : g.entries)
            {
                failed += entry.state == EntryState::Failed;
                unsupported += entry.state == EntryState::Unsupported;
                created += entry.state == EntryState::Created;
            }
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                auto* a = dynamic_cast<AI_Mgr*>(r->game.get());
                if (!a)
                    continue;
                CHECK(a->RuntimeReady());
                old = id;
                cultists += a->npc_type == AI_Mgr::NPCType::Cultist;
                civilians += a->npc_type == AI_Mgr::NPCType::Civilian;
                bosses += a->npc_type == AI_Mgr::NPCType::Gideon;
            }
            CHECK(cultists == 10 && civilians == 5 && bosses == 1 && failed == 0 && unsupported == 0 && created == 204);
            CHECK(g.objects.snapshot().size() == 227 && g.enter_player(e) && g.objects.snapshot().size() == 230);
            for (int tick = 0; tick < 600; ++tick)
            {
                CHECK(g.step(1.0 / 60));
                if (!tick)
                    CHECK(g.objects.snapshot().size() == 232 && g.objects.attachments().size() == 11);
            }
            std::uint64_t updates = 0, pings = 0;
            for (auto id : g.objects.snapshot())
            {
                auto* r = g.objects.get(id);
                auto* a = dynamic_cast<AI_Mgr*>(r->game.get());
                if (!a)
                    continue;
                if (a->runtime_failed)
                    std::cerr << a->runtime_error << '\n';
                CHECK(a->RuntimeReady() && a->runtime_updates > 0);
                updates += a->runtime_updates;
                pings += r->activation_pings;
            }
            CHECK(updates > 15 && pings > 0);
            std::cout << "SubwayA load=" << load << " created=" << created << " unsupported=" << unsupported
                      << " AI_Update=" << updates << " pings=" << pings << '\n';
            CHECK(g.unload_world() && !g.objects.get(old) && g.objects.snapshot().empty());
            CHECK(AI_Mgr::m_dwNumCabal == 0 && AI_Mgr::m_dwNumMonster == 0 && g.server.string_count() == 0 &&
                  g.server.message_count() == 0);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    const std::map<std::string, std::function<bool(const char*)>> tests{{"body", body},
                                                                        {"ammo_receive", ammo_receive},
                                                                        {"ammo_search", ammo_search},
                                                                        {"ammo_private", ammo_private},
                                                                        {"missing", missing},
                                                                        {"activation", activation},
                                                                        {"animation_clearance", animation_clearance},
                                                                        {"clearance_private", clearance_private},
                                                                        {"visibility", visibility},
                                                                        {"visibility_filter", visibility_filter},
                                                                        {"visibility_atomic", visibility_atomic},
                                                                        {"lifetime", lifetime},
                                                                        {"targets", targets},
                                                                        {"tracking", tracking},
                                                                        {"camera", camera},
                                                                        {"spatial", spatial},
                                                                        {"triggers", triggers},
                                                                        {"arena", arena},
                                                                        {"catalog", catalog},
                                                                        {"failures", failures},
                                                                        {"subway", subway},
                                                                        {"sparse_weapons", sparse_weapons}};
    if (argc < 2)
        return 2;
    auto t = tests.find(argv[1]);
    return t != tests.end() && t->second(argc > 2 ? argv[2] : "") ? 0 : 1;
}
