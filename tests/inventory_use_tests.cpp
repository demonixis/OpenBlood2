#include "app/movement_controls.h"
#include "engine/collision.h"
#include "game/original/BoneLeech.h"
#include "game/original/BugSprayProjectile.h"
#include "game/original/PickupObject.h"
#include "game/original/SeeingEye.h"
#include "game/server_adapter.h"
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
        g.world_path = "inventory-use-fixture.dat";
        g.state = SessionState::Inspection;
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 0}}}));
        g.server_shell.PreStartWorld();
        CHECK(g.server_shell.PostStartWorld() && g.enter_player(error) && g.pump_player_messages(error));
        return true;
    }
    bool step(GameSession& g, double dt = .05)
    {
        if (g.step(dt))
            return true;
        for (const auto& e : g.trace)
            std::cerr << e << '\n';
        for (auto id : g.objects.snapshot())
            if (auto* ai = dynamic_cast<AI_Mgr*>(g.objects.get(id)->game.get()); ai && ai->runtime_failed)
                std::cerr << ai->runtime_error << '\n';
        return false;
    }
    CPlayerObj& player(GameSession& g)
    {
        return *g.server_shell.Player(g.local_client);
    }
    bool pump(GameSession& g)
    {
        std::string error;
        CHECK(g.pump_player_messages(error));
        return true;
    }
    bool item(GameSession& g, DBYTE type)
    {
        CHECK(g.server_shell.OnMessage(g.local_client, PlayerItemCommand{type}));
        return pump(g);
    }
    bool clean(GameSession& g)
    {
        CHECK(g.unload_world());
        CHECK(g.objects.snapshot().empty() && !g.server.string_count() && !g.server.list_count() &&
              !g.server.message_count());
        CHECK(!g.client_shell.seeing_eye && !g.client_shell.creature && !g.client_shell.eye_loop &&
              !g.client_shell.goggles_loop);
        return true;
    }
    bool goggles(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto& c = g.client_shell;
        CHECK(!p.OnCommandItem(3));
        CHECK(p.m_InventoryMgr.AddItem(3, 1) == InventoryResult::Ok);
        CHECK(p.m_InventoryMgr.AddItem(3, 1) == InventoryResult::AlreadyHave);
        c.goggles_down = true;
        CHECK(c.UpdateUse(g.server_shell) && pump(g));
        auto& item = *p.m_InventoryMgr.goggles;
        CHECK(item.active && c.night_goggles && c.goggles_loop && c.item_audio_error.empty());
        CHECK(c.SceneLightScale().x == .1f && c.SceneLightScale().y == 1);
        CHECK(c.UpdateUse(g.server_shell) && pump(g) && item.active); // Hold is not a second action.
        CHECK(step(g, .1) && item.count == 100); // Rounded charge refuses an immediate replacement.
        CHECK(p.m_InventoryMgr.AddItem(3, 1) == InventoryResult::AlreadyHave);
        CHECK(step(g, .1) && item.count == 99);
        CHECK(p.m_InventoryMgr.AddItem(3, 1) == InventoryResult::Ok && item.remaining == 30);
        for (int i = 0; i < 301; ++i)
            CHECK(step(g, .1));
        CHECK(item.count == 0 && !item.active && !c.night_goggles && !c.goggles_loop);
        CHECK(!p.OnCommandItem(3));
        CHECK(p.m_InventoryMgr.AddItem(3, 1) == InventoryResult::Ok);
        CHECK(item.SetActive(true) && pump(g));
        InventoryState saved;
        std::string error;
        CHECK(p.m_InventoryMgr.Save(saved, error) && saved.goggles && saved.goggles->remaining == 30);
        {
            CInventoryMgr restored;
            CHECK(restored.Init(p.m_hObject, nullptr) && restored.Load(saved, InventoryLoadType::KeepAlive, error));
            CHECK(restored.goggles && restored.goggles->remaining == 30 && !restored.goggles->active);
            saved.goggles->remaining = 31;
            CHECK(!restored.Load(saved, InventoryLoadType::Restore, error));
        }
        p.m_InventoryMgr.RemoveDeathItems();
        CHECK(pump(g) && !c.night_goggles && !c.goggles_loop);
        return clean(g);
    }
    bool eye(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto& c = g.client_shell;
        CHECK(p.m_InventoryMgr.AddItem(4, 1) == InventoryResult::Ok);
        CHECK(item(g, 4));
        auto h = p.m_InventoryMgr.eye->eye;
        auto* eye = dynamic_cast<SeeingEye*>(g.server.HandleToObject(h));
        CHECK(eye && !p.m_InventoryMgr.eye->active && !c.seeing_eye);
        DVector velocity;
        CHECK(g.server.GetVelocity(h, &velocity) == DE_OK && velocity.z == 300 && velocity.y == 0);
        CHECK(p.OnCommandItem(4));
        // The server blocks a committed FIRE_KEY before its camera message is pumped.
        CHECK(p.HandleWeaponFireMessage({c.camera_position, {0, 0, 1}, 2, WEAP_BERETTA}) == FireResult::Blocked);
        CHECK(pump(g) && c.seeing_eye == h && c.eye_loop && c.view.object == h);
        CHECK(c.player_look_allowed());
        CHECK(!c.weapons_visible && c.player_visible && p.camera_movement_blocked);
        const auto ammo = p.m_InventoryMgr.GetAmmoCount(AMMO_BULLET);
        c.weapon_input.primary = true;
        c.movement_input.forward = true;
        c.rotation = {.2f, .6f};
        CHECK(step(g));
        CHECK(p.m_InventoryMgr.GetAmmoCount(AMMO_BULLET) == ammo && c.view.object == h);
        DRotation rotation;
        CHECK(g.server.GetObjectRotation(h, &rotation) == DE_OK && std::abs(rotation.m_Vec.y) > .1f);
        CHECK(item(g, 4) && !c.seeing_eye && !c.eye_loop && !p.camera_movement_blocked);
        c.weapon_input = {};
        c.movement_input = {};
        for (int i = 0; i < 40 && !eye->dropped; ++i)
            CHECK(step(g));
        CHECK(eye->dropped);
        CHECK(item(g, 4));
        DVector position;
        g.server.GetObjectPos(h, &position);
        const auto message = g.server.StartMessageToObject(&p, h, damage_message);
        CHECK(message &&
              g.server.WriteDamage(message, {{0, 0, 1}, position, 100, DamageType::Normal, p.m_hObject}) == DE_OK);
        CHECK(g.server.EndMessage(message) == DE_OK && pump(g));
        CHECK(!c.seeing_eye && !c.eye_loop && c.blind_until > g.server.GetTime() && c.SceneLightScale().y == 0);
        CHECK(!p.m_InventoryMgr.eye->available && !p.OnCommandItem(4));
        CHECK(g.objects.finish_frame());
        CHECK(p.m_InventoryMgr.AddItem(4, 1) == InventoryResult::Ok && item(g, 4));
        h = p.m_InventoryMgr.eye->eye;
        eye = dynamic_cast<SeeingEye*>(g.server.HandleToObject(h));
        CHECK(eye);
        // A landed eye touched by its owner returns to the inventory, without destruction blindness.
        eye->dropped = true;
        CHECK(item(g, 4));
        eye->HandleContact({{}, {}, p.m_hObject});
        CHECK(pump(g) && !c.seeing_eye && p.m_InventoryMgr.eye->available && !p.m_InventoryMgr.eye->eye);
        CHECK(g.objects.finish_frame());
        CHECK(item(g, 4));
        const auto old = p.m_InventoryMgr.eye->eye;
        InventoryState saved;
        std::string error;
        CHECK(p.m_InventoryMgr.Save(saved, error) && saved.eye);
        {
            CInventoryMgr restored;
            CHECK(restored.Init(p.m_hObject, nullptr) && restored.Load(saved, InventoryLoadType::KeepAlive, error));
            CHECK(restored.eye && restored.eye->available && !restored.eye->eye && !restored.eye->active);
        }
        CHECK(g.leave_player() && !g.server.IsObjectAlive(old));
        return clean(g);
    }
    bool leech(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto& c = g.client_shell;
        auto create = [&]() -> BoneLeech*
        {
            auto h = g.objects.create(g.objects.find_class("BoneLeech"), {}, Creation::World,
                                      {{"Pos", DVector{500, 100, 0}}});
            return h ? dynamic_cast<BoneLeech*>(g.server.HandleToObject(h)) : nullptr;
        };
        auto* cancelled = create();
        CHECK(cancelled);
        const auto flags = g.server.GetObjectFlags(cancelled->m_hObject);
        CHECK(cancelled->RequestAttach(p.m_hObject) && cancelled->Respond(p.m_hObject, false));
        CHECK(cancelled->AI_STATE_EnemyAttach() && g.server.GetObjectFlags(cancelled->m_hObject) == flags);
        g.server.RemoveObject(cancelled->m_hObject);
        CHECK(g.objects.finish_frame() && pump(g));
        auto* b = create();
        CHECK(b && b->RuntimeReady() && b->SetNewTarget(p.m_hObject));
        b->m_nCurMetacmd = 10;
        CHECK(g.objects.engine_message(b->m_hObject, MID_TOUCHNOTIFY, p.m_hObject));
        CHECK(b->attached == p.m_hObject && b->m_nState == AI_Mgr::StateEnemyAttach);
        for (int i = 0; i < 4; ++i)
            CHECK(step(g));
        CHECK(c.creature == b->m_hObject && b->proceed && b->Metacmd == 3);
        CHECK(g.server.GetObjectFlags(b->m_hObject) & FLAG_REALLYCLOSE);
        CHECK(!g.server_shell.OnMessage(g.local_client, CreatureResponse{p.m_hObject, true}));
        const float health = p.m_damage.m_fHitPoints, armor = p.m_damage.m_fArmorPoints;
        for (int i = 0; i < 24; ++i)
            CHECK(step(g));
        CHECK(b->damage_ticks && (p.m_damage.m_fHitPoints < health || p.m_damage.m_fArmorPoints < armor));
        CHECK(c.creature_light < 1);
        const auto bh = b->m_damage.m_fHitPoints;
        auto m = g.server.StartMessageToObject(&p, b->m_hObject, damage_message);
        CHECK(g.server.WriteDamage(m, {{0, 0, 1}, {}, 1000, DamageType::Normal, p.m_hObject}) == DE_OK &&
              g.server.EndMessage(m) == DE_OK);
        CHECK(b->m_damage.m_fHitPoints == bh);
        auto* second = create();
        CHECK(second && second->RequestAttach(p.m_hObject));
        for (int i = 0; i < 3; ++i)
            CHECK(step(g));
        CHECK(!second->attached && c.creature == b->m_hObject);
        g.server.RemoveObject(second->m_hObject);
        CHECK(g.objects.finish_frame());
        for (int i = 0; i < 6; ++i)
        {
            c.use_down = false;
            CHECK(c.UpdateUse(g.server_shell));
            c.use_down = true;
            CHECK(c.UpdateUse(g.server_shell));
        }
        CHECK(pump(g) && !c.creature && !b->attached);
        CHECK(step(g) && !(g.server.GetObjectFlags(b->m_hObject) & FLAG_REALLYCLOSE));
        // Reattach after the source four-second cooldown, then remove the owner.
        for (int i = 0; i < 120 && b->m_nState == AI_Mgr::StateEnemyAttach && !b->attached; ++i)
            CHECK(step(g));
        CHECK(b->attached || b->RequestAttach(p.m_hObject));
        for (int i = 0; i < 3; ++i)
            CHECK(step(g));
        CHECK(c.creature);
        CHECK(g.leave_player() && !c.creature && !b->attached);
        return clean(g);
    }
    struct Target : BaseClass
    {
        CDestructable damage;
        Target()
        {
            AddAggregate(&damage);
        }
        DDWORD EngineMessageFn(DDWORD message, void* data, DFLOAT reason) override
        {
            if (message == MID_INITIALUPDATE)
            {
                damage.Init(m_hObject, nullptr);
                damage.SetHitPoints(1000);
                damage.m_fMaxHitPoints = 1000;
                damage.apply_damage_physics = false;
            }
            return BaseClass::EngineMessageFn(message, data, reason);
        }
    };
    Target* target(GameSession& g, DVector position)
    {
        static ClassInfo info{"InventoryUseTarget",
                              nullptr,
                              []() -> std::unique_ptr<BaseClass> { return std::make_unique<Target>(); },
                              {}};
        info.parent = g.objects.find_class("BaseClass");
        if (!g.objects.register_class(info))
            return nullptr;
        ObjectCreateStruct create;
        create.m_Pos = position;
        create.m_Flags = FLAG_SOLID;
        auto h = g.objects.create(&info, create);
        DVector dims{20, 20, 20};
        if (!h || g.server.SetObjectDims(h, &dims) != DE_OK)
            return nullptr;
        return dynamic_cast<Target*>(g.server.HandleToObject(h));
    }
    bool bugspray(const char* root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& p = player(g);
        auto& c = g.client_shell;
        CHECK(p.m_InventoryMgr.ObtainWeapon(WEAP_BUGSPRAY) == InventoryResult::Ok);
        CHECK(p.m_InventoryMgr.AddAmmo(AMMO_DIEBUGDIE, 20) == InventoryResult::Ok);
        CHECK(p.m_InventoryMgr.ChangeWeapon(WEAP_BUGSPRAY) == InventoryResult::Ok);
        for (int i = 0; i < 30; ++i)
            CHECK(step(g)); // Draw animation.
        c.weapon_input.primary = true;
        for (int i = 0; i < 15; ++i)
            CHECK(step(g));
        c.weapon_input = {};
        CHECK(p.m_InventoryMgr.GetAmmoCount(AMMO_DIEBUGDIE) < 20);
        CHECK(g.server.object_particles.started > 0);
        CHECK(p.m_InventoryMgr.CurrentWeapon()->projectile_error.empty());
        for (int i = 0; i < 25; ++i)
            CHECK(step(g));
        const auto ammo = p.m_InventoryMgr.GetAmmoCount(AMMO_DIEBUGDIE);
        c.weapon_input.alternate = true;
        for (int i = 0; i < 25; ++i)
            CHECK(step(g));
        c.weapon_input = {};
        CHECK(p.m_InventoryMgr.GetAmmoCount(AMMO_DIEBUGDIE) < ammo);
        for (int i = 0; i < 50; ++i)
            CHECK(step(g));
        bool primary = false, alternate = false;
        for (const auto& r : g.server.scene_explosions)
        {
            if (r.type == 26)
            {
                primary = true;
                CHECK(r.flame_event && r.flame_error.empty() && r.voice);
            }
            if (r.type == 27)
            {
                alternate = true;
                CHECK(r.ring_event && r.light_event && r.ring_error.empty() && r.light_error.empty() && r.voice);
            }
        }
        CHECK(primary && alternate);
        auto* victim = target(g, {0, 150, 500});
        CHECK(victim);
        for (bool alternate : {false, true})
        {
            ObjectCreateStruct create;
            create.m_Pos = {0, 150, 300};
            const char* type = alternate ? "CBugSprayAltProjectile" : "CBugSprayProjectile";
            auto* projectile =
                dynamic_cast<CBugSprayProjectile*>(g.server.CreateObject(g.server.GetClass(type), &create));
            CHECK(projectile && projectile->Setup({0, 0, 1}, WEAP_BUGSPRAY, 50, 1500, 0, p.m_hObject));
            const auto h = projectile->m_hObject;
            const auto before = victim->damage.m_fHitPoints;
            for (int i = 0; i < 20 && g.server.IsObjectAlive(h); ++i)
                CHECK(step(g, .02));
            CHECK(!g.server.IsObjectAlive(h) && victim->damage.m_fHitPoints == before - 50);
        }
        return clean(g);
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view mode = argv[1];
    return (mode == "goggles"    ? goggles(argv[2])
            : mode == "eye"      ? eye(argv[2])
            : mode == "leech"    ? leech(argv[2])
            : mode == "bugspray" ? bugspray(argv[2])
                                 : false)
               ? 0
               : 1;
}
