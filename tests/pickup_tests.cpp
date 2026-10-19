#include "brush_fixture.h"
#include "game/original/AmmoPickups.h"
#include "game/original/DestructableBrush.h"
#include "game/original/PowerupPickups.h"
#include "game/original/spawn_object.h"
#include "player_fixture.h"
#include <functional>
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
    constexpr const char* names[] = {"BulletAmmoPU",    "ShellAmmoPU",    "BMGAmmoPU",  "FlareAmmoPU",
                                     "DieBugDieAmmoPU", "HowitzerAmmoPU", "FuelAmmoPU", "BatteryAmmoPU",
                                     "HealthPU",        "MegaHealthPU",   "WardPU",     "NecroWardPU"};
    bool near(float a, float b)
    {
        return std::abs(a - b) < .01f;
    }
    bool setup(GameSession& g, const std::filesystem::path& root, bool assets = true)
    {
        CHECK(test_player::setup(g, root, true, true, assets));
        std::string error;
        CHECK(g.enter_player(error));
        return true;
    }
    CPlayerObj& player(GameSession& g)
    {
        return *g.server_shell.Player(g.local_client);
    }
    PickupObject* item(GameSession& g, HOBJECT h)
    {
        auto* r = g.objects.get(h);
        return r ? dynamic_cast<PickupObject*>(r->game.get()) : nullptr;
    }
    HOBJECT create(GameSession& g, const char* name = "BulletAmmoPU", Properties props = {})
    {
        props.try_emplace("Pos", DVector{300, 100, 300});
        return g.objects.create(g.objects.find_class(name), {}, Creation::World, props);
    }
    bool touch(GameSession& g, HOBJECT h)
    {
        return g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player);
    }
    bool defaults(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        const float amounts[] = {100, 20, 20, 20, 20, 5, 10, 40, 25, 100, 25, 100};
        for (unsigned n = 0; n < 12; ++n)
        {
            auto h = create(g, names[n]);
            CHECK(h);
            auto* p = item(g, h);
            auto* r = g.objects.get(h);
            CHECK(p && p->value.type == (n < 8 ? n + 1 : n - 7) && p->value.amount == amounts[n]);
            CHECK(p->value.kind == (n < 8 ? PickupKind::Ammo : PickupKind::Powerup));
            CHECK(p->resource_id == int(n < 8 ? 317 + n : 309 + n - 8) && p->error.empty());
            CHECK(r->model && r->model_error.empty() && r->dimensions.x >= 10 && r->dimensions.z >= r->dimensions.x);
            CHECK(near(r->state.m_Pos.y, r->dimensions.y + .001f));
            CHECK(r->state.m_Flags & FLAG_TOUCH_NOTIFY);
            CHECK(p->rotate_fx == (n >= 8) && bool(r->user_flags & (1u << 4)) == (n >= 8));
            CHECK(Objects::is_kind_of(r->type, g.objects.find_class("PickupObject")));
        }
        CHECK(AmmoPickup::m_dwNumPickups == 8);
        CHECK(!create(g, "AmmoPickup") && !create(g, "HealthBase"));
        CHECK(!g.objects.find_class("ManaAmmoPU") && !g.objects.find_class("RocketAmmoPU"));
        CHECK(g.unload_world() && AmmoPickup::m_dwNumPickups == 0 && g.server.message_count() == 0);
        return true;
    }
    bool ammunition(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& inv = player(g).m_InventoryMgr;
        for (unsigned t = 1; t <= 8; ++t)
        {
            auto h = create(g, names[t - 1]);
            CHECK(h);
            inv.SetAmmoCount(t, inv.MaxAmmo(t));
            CHECK(touch(g, h) && !item(g, h)->picked_up);
            inv.SetAmmoCount(t, inv.MaxAmmo(t) - 1);
            CHECK(touch(g, h) && item(g, h)->picked_up);
            CHECK(inv.GetAmmoCount(t) == inv.MaxAmmo(t) && !g.server.IsObjectAlive(h));
            CHECK(g.objects.finish_frame() && !g.objects.get(h));
        }
        CHECK(g.server.pickup_reports.size() == 8 && AmmoPickup::m_dwNumPickups == 0);
        CHECK(g.server.message_count() == 0);
        CHECK(g.unload_world() && g.server.string_count() == 0);
        return true;
    }
    bool statistics(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto& d = player(g).m_damage;
        d.m_fMaxHitPoints = 100;
        d.m_fMaxMegaHitPoints = 200;
        d.m_fMaxArmorPoints = 100;
        d.m_fMaxNecroArmorPoints = 200;
        for (unsigned i = 0; i < 4; ++i)
        {
            auto& stat = i < 2 ? d.m_fHitPoints : d.m_fArmorPoints;
            const float cap = i % 2 ? 200 : 100;
            stat = cap;
            auto h = create(g, names[8 + i]);
            CHECK(h && touch(g, h) && !item(g, h)->picked_up);
            stat = cap - 10;
            CHECK(touch(g, h) && item(g, h)->picked_up && stat == cap);
            CHECK(g.objects.finish_frame());
        }
        d.SetHitPoints(0);
        d.m_bDead = true;
        auto h = create(g, "MegaHealthPU");
        CHECK(h && touch(g, h) && d.IsDead() && !item(g, h)->picked_up);
        d.m_bDead = false;
        player(g).m_bDead = true;
        CHECK(touch(g, h) && !item(g, h)->picked_up);
        d.SetHitPoints(10);
        CHECK(!d.Heal(-1) && !d.MegaHeal(std::numeric_limits<float>::infinity()));
        CHECK(!d.AddWard(std::numeric_limits<float>::quiet_NaN()) && !d.AddNecroWard(1e7f));
        return true;
    }
    bool protocol(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = create(g);
        CHECK(h);
        auto* p = item(g, h);
        auto ph = g.client_shell.player;
        auto send = [&](DDWORD id, PickupMessage value)
        {
            auto m = g.server.StartMessageToObject(p, ph, id);
            auto result = g.server.WritePickupMessage(m, value);
            g.server.EndMessage(m);
            return result;
        };
        const auto before = player(g).m_InventoryMgr.GetAmmoCount(1);
        CHECK(send(add_ammo_message, {PickupKind::Ammo, 0, 10}) == DE_INVALIDPARAMS);
        CHECK(send(add_ammo_message, {PickupKind::Ammo, 9, 10}) == DE_INVALIDPARAMS);
        CHECK(send(add_ammo_message, {PickupKind::Powerup, 1, 10}) == DE_INVALIDPARAMS);
        CHECK(send(add_ammo_message, {PickupKind::Ammo, 1, -1}) == DE_INVALIDPARAMS);
        CHECK(send(add_ammo_message, {PickupKind::Ammo, 1, std::numeric_limits<float>::quiet_NaN()}) ==
              DE_INVALIDPARAMS);
        auto m = g.server.StartMessageToObject(p, ph, add_ammo_message);
        CHECK(g.server.WritePickupMessage(m, p->value) == DE_OK);
        CHECK(g.server.WriteToMessageDWord(m, 1) == DE_INVALIDPARAMS && g.server.EndMessage(m) != DE_OK);
        m = g.server.StartMessageToObject(&player(g), h, picked_up_message);
        CHECK(g.server.WritePickupMessage(m, p->value) == DE_OK && g.server.EndMessage(m) == DE_OK);
        CHECK(!p->picked_up && player(g).m_InventoryMgr.GetAmmoCount(1) == before);
        PickupMessage value;
        CHECK(!g.server.ReadPickupMessage(m, value));
        auto other = g.objects.create(g.objects.find_class("BaseClass"));
        CHECK(g.objects.engine_message(h, MID_TOUCHNOTIFY, other) && !p->picked_up);
        CHECK(g.server.message_count() == 0);
        return true;
    }
    bool contact(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = create(g);
        CHECK(h);
        player(g).m_InventoryMgr.SetAmmoCount(1, 0);
        auto pos = g.objects.get(h)->state.m_Pos;
        CHECK(g.server.TeleportObject(g.client_shell.player, &pos) == DE_OK);
        CHECK(g.step(.02)); // Actual MoveMgr integration dispatches sensor contact.
        CHECK(player(g).m_InventoryMgr.GetAmmoCount(1) == 100 && !g.objects.get(h));
        CHECK(g.server.pickup_reports.size() == 1 && g.server.pickup_reports[0].sound == "sounds\\powerups\\ammo1.wav");
        return true;
    }
    HOBJECT brush(GameSession& g, std::string spawn = "BulletAmmoPU", Properties props = {})
    {
        const auto name = "crate" + std::to_string(g.world.models.size());
        auto model = test_brush::box(name, {200, 40, 200}, {220, 60, 220});
        model.translation = {210, 50, 210};
        g.world.models.push_back(std::move(model));
        props["Name"] = name;
        props["SpawnObject"] = spawn;
        props.try_emplace("HitPoints", 10.0f);
        props.try_emplace("SpawnObjectVel", DVector{10, 100, 20});
        return g.objects.create(g.objects.find_class("CDestructableBrush"), {}, Creation::World, props);
    }
    bool kill(GameSession& g, HOBJECT h)
    {
        auto msg = g.server.StartMessageToObject(g.server.HandleToObject(h), h, damage_message);
        CHECK(msg && g.server.WriteDamage(msg, {{}, {}, 1, DamageType::Death, h}) == DE_OK);
        CHECK(g.server.EndMessage(msg) == DE_OK);
        return true;
    }
    bool spawning(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = brush(g, "BulletAmmoPU Name dropped; PickupTriggerTarget target; PickupTriggerMessage GET");
        CHECK(h);
        CHECK(kill(g, h) && g.server.spawn_reports.size() == 1);
        auto child = g.server.spawn_reports.back().child;
        CHECK(child && item(g, child));
        auto* r = g.objects.get(child);
        CHECK(std::string(r->state.m_Name) == "dropped");
        CHECK(near(r->state.m_Pos.x, 210) && near(r->state.m_Pos.y, 50) && near(r->state.m_Pos.z, 210));
        DVector velocity;
        CHECK(g.server.GetVelocity(child, &velocity) == DE_OK && velocity.y == 100 && velocity.x == 10);
        CHECK(kill(g, h) && g.server.spawn_reports.size() == 1);
        CHECK(g.step(.1) && !g.objects.get(h) && g.server.IsObjectAlive(child));
        CHECK(g.unload_world() && !g.objects.get(child) && g.server.spawn_reports.empty() &&
              AmmoPickup::m_dwNumPickups == 0);
        return true;
    }
    bool rejection(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        for (auto text : {"Spawner", "WeaponPickup", "AmmoPickup", "  ", "BulletAmmoPU Unknown 1",
                          "BulletAmmoPU Name \"bad", "BulletAmmoPU Name a; Name b"})
            CHECK(!g.server.ValidateSpawnObject(text) && !brush(g, text));
        CHECK(!brush(g, "BulletAmmoPU", {{"SpawnObjectVel", DVector{1e8f, 0, 0}}}));
        CHECK(g.server.ValidateSpawnObject("  bulletammopu Name okay"));
        auto h = SpawnObject("HealthPU Pos 12 100 14", {200, 300, 400}, {}, {1, 2, 3});
        CHECK(h);
        CHECK(near(g.objects.get(h)->state.m_Pos.x, 12) && near(g.objects.get(h)->state.m_Pos.y, 100));
        const auto ammo_count = AmmoPickup::m_dwNumPickups;
        CHECK(!SpawnObject("BulletAmmoPU", {100, 100, 100}, {}, {1e8f, 0, 0}));
        CHECK(g.objects.finish_frame() && AmmoPickup::m_dwNumPickups == ammo_count);
        DRotation q{{0, .5f, 0}, .8660254f};
        h = SpawnObject("WardPU", {123, 321, 456}, q, {});
        CHECK(h);
        CHECK(near(g.objects.get(h)->state.m_Rotation.m_Vec.y, .5f));
        CHECK(!dynamic_cast<PickupObject*>(g.objects.get(h)->game.get())->bouncing); // Ammo/health: no bounce.
        // Authored container contents: original SpawnItem raises bouncing item/weapon leaves by 10
        // units and keeps them flagged, with no update scheduled for the source spring.
        for (auto text : {"MedKitPU", "FlashLightPU", "ProximitiesPU", "RemotesPU", "BerettaPU", "SubMachineGunPU",
                          "FlareGunPU", "AssaultRiflePU", "SniperRiflePU"})
        {
            CHECK(g.server.ValidateSpawnObject(text));
            h = SpawnObject(text, {100, 100, 100}, {}, {});
            auto* r = h ? g.objects.get(h) : nullptr;
            auto* p = r ? dynamic_cast<PickupObject*>(r->game.get()) : nullptr;
            // The fixture lacks some pickup models (dimension errors); the bounce itself must succeed.
            CHECK(p && p->bouncing && p->error.find("String-spawn") == std::string::npos &&
                  p->bounce_acceleration == 500);
            CHECK(near(p->rest_position.y, 100) && near(r->state.m_Pos.y, 110) && near(r->state.m_Pos.x, 100));
        }
        // Classes no original ObjectDLL defines: GetClass returns null, so the destroyed
        // container spawns nothing (03_Museum RocketAmmoPU, authoring typos).
        for (auto text : {"RocketAmmoPU", "rocketammopu Name x", "DieBugDieAmmo", "FareAmmoPU", "LifeEssencePU"})
        {
            CHECK(!g.server.ValidateSpawnObject(text));
            auto crate = brush(g, text);
            CHECK(crate && kill(g, crate) && !g.server.spawn_reports.empty());
            CHECK(!g.server.spawn_reports.back().child &&
                  g.server.spawn_reports.back().outcome.starts_with("No original class"));
        }
        return true;
    }
    struct Probe : BaseClass
    {
        std::function<void()> callback;
        unsigned hits = 0;
        DDWORD ObjectMessageFn(HOBJECT, DDWORD id, HMESSAGEREAD) override
        {
            if (id == 1000)
            {
                ++hits;
                if (callback)
                    callback();
            }
            return 1;
        }
    };
    Probe* probe(GameSession& g)
    {
        static ClassInfo type{"PickupProbe",
                              nullptr,
                              []() -> std::unique_ptr<BaseClass> { return std::make_unique<Probe>(); },
                              {{"Name", std::string("target")}}};
        if (!g.objects.find_class(type.name))
            g.objects.register_class(type);
        auto h = g.objects.create(&type);
        return dynamic_cast<Probe*>(g.objects.get(h)->game.get());
    }
    bool callbacks(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto* p = probe(g);
        CHECK(p);
        // This test class does not read BaseClass properties; give it the target name in the creation state.
        auto* pr = g.objects.get(p->m_hObject);
        SAFE_STRCPY(pr->state.m_Name, "target");
        bool death_before_spawn = false, damage_after_spawn = false;
        auto ordered = brush(g, "BulletAmmoPU",
                             {{"DeathTriggerTarget", std::string("target")},
                              {"DeathTriggerMessage", std::string("DEATH")},
                              {"DamageTriggerTarget", std::string("target")},
                              {"DamageTriggerMessage", std::string("HIT")}});
        CHECK(ordered);
        p->callback = [&]
        {
            if (p->hits == 1)
                death_before_spawn = g.server.spawn_reports.empty();
            if (p->hits == 2)
                damage_after_spawn =
                    g.server.spawn_reports.size() == 1 && g.server.IsObjectAlive(g.server.spawn_reports.back().child);
        };
        CHECK(kill(g, ordered) && death_before_spawn && damage_after_spawn);
        CHECK(g.objects.remove(g.server.spawn_reports.back().child) && g.objects.remove(ordered) &&
              g.objects.finish_frame());
        g.server.spawn_reports.clear();
        p->hits = 0;
        auto h = brush(g, "BulletAmmoPU",
                       {{"DeathTriggerTarget", std::string("target")}, {"DeathTriggerMessage", std::string("DEATH")}});
        CHECK(h);
        bool before = false;
        p->callback = [&]
        {
            before = g.server.spawn_reports.empty();
            g.objects.remove(h);
        };
        CHECK(kill(g, h) && before && p->hits == 1 && g.server.spawn_reports.size() == 1);
        CHECK(!g.server.spawn_reports.back().child && g.server.spawn_reports.back().outcome.starts_with("Cancelled"));
        CHECK(g.objects.finish_frame());
        h = create(g, "BulletAmmoPU",
                   {{"PickupTriggerTarget", std::string("target")}, {"PickupTriggerMessage", std::string("GET")}});
        CHECK(h);
        player(g).m_InventoryMgr.SetAmmoCount(1, 0);
        p->callback = [&]
        {
            g.objects.engine_message(h, MID_TOUCHNOTIFY, g.client_shell.player);
            g.objects.remove(h);
        };
        CHECK(touch(g, h) && p->hits == 2 && player(g).m_InventoryMgr.GetAmmoCount(1) == 100);
        CHECK(g.objects.finish_frame() && !g.objects.get(h));
        CHECK(g.server.message_count() == 0);
        CHECK(g.unload_world() && g.server.string_count() == 0);
        return true;
    }
    bool resources(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root, false));
        auto h = create(g);
        CHECK(h);
        CHECK(!item(g, h)->error.empty() && !(g.objects.get(h)->state.m_Flags & FLAG_TOUCH_NOTIFY));
        CHECK(touch(g, h) && !item(g, h)->picked_up);
        for (unsigned i = 0; i < 35; ++i)
        {
            auto b = brush(g);
            CHECK(b && kill(g, b));
        }
        CHECK(g.server.spawn_reports.size() == 32 && AmmoPickup::m_dwNumPickups == 36);
        for (auto& report : g.server.spawn_reports)
            CHECK(report.child && !g.objects.get(report.child)->model);
        CHECK(g.unload_world() && AmmoPickup::m_dwNumPickups == 0);
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        std::string error;
        CHECK(g.resources.mount_game(Config{root, "private"}, error));
        std::map<std::string, unsigned> counts, failures;
        unsigned worlds = 0, missing = 0, spawns = 0, created = 0;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            CHECK(g.resources.read(path, bytes, error));
            if (!read_world(bytes, g.world, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                continue;
            }
            ++worlds;
            for (const auto& source : g.world.objects)
            {
                auto type = g.objects.find_class(source.class_name);
                if (!type || !Objects::is_kind_of(type, g.objects.find_class("PickupObject")))
                    continue;
                ++counts[source.class_name];
                Properties props;
                for (const auto& p : source.properties)
                    props.emplace(p.name, p.value);
                auto h = g.objects.create(type, {}, Creation::World, props);
                if (!h)
                    ++failures[g.objects.diagnostics().back()];
                else
                {
                    ++created;
                    if (source.class_name == "BerettaPU")
                    {
                        auto* gun = dynamic_cast<PickupObject*>(g.objects.get(h)->game.get());
                        CHECK(gun && gun->value.kind == PickupKind::Weapon && gun->value.type == WEAP_BERETTA &&
                              gun->value.amount == 12);
                        std::cout << "Authored BerettaPU: " << path << '\n';
                    }
                    if (source.class_name == "SubMachineGunPU")
                    {
                        auto* gun = dynamic_cast<PickupObject*>(g.objects.get(h)->game.get());
                        CHECK(gun && gun->value.kind == PickupKind::Weapon && gun->value.type == WEAP_SUBMACHINEGUN &&
                              gun->value.amount == 20 && gun->resource_id == IDS_WEAPON_MAC10);
                        std::cout << "Authored SubMachineGunPU: " << path << '\n';
                    }
                    if (source.class_name == "FlareGunPU")
                    {
                        auto* gun = dynamic_cast<PickupObject*>(g.objects.get(h)->game.get());
                        CHECK(gun && gun->value.kind == PickupKind::Weapon && gun->value.type == WEAP_FLAREGUN &&
                              gun->value.amount == 8 && gun->resource_id == IDS_WEAPON_FLAREGUN);
                        std::cout << "Authored FlareGunPU: " << path << '\n';
                    }
                    if (source.class_name == "AssaultRiflePU")
                    {
                        auto* gun = dynamic_cast<PickupObject*>(g.objects.get(h)->game.get());
                        CHECK(gun && gun->value.kind == PickupKind::Weapon && gun->value.type == WEAP_ASSAULTRIFLE &&
                              gun->value.amount == 50 && gun->resource_id == IDS_WEAPON_M16);
                        std::cout << "Authored AssaultRiflePU: " << path << '\n';
                    }
                    if (source.class_name == "VoodooDollPU")
                    {
                        const auto* doll = dynamic_cast<PickupObject*>(g.objects.get(h)->game.get());
                        CHECK(doll && doll->value.kind == PickupKind::Weapon && doll->value.type == WEAP_VOODOO &&
                              doll->value.amount == 0 && doll->resource_id == IDS_WEAPON_VOODOO);
                    }
                    if (source.class_name == "ProximitiesPU")
                    {
                        auto* bomb = dynamic_cast<PickupObject*>(g.objects.get(h)->game.get());
                        CHECK(bomb && bomb->value.kind == PickupKind::Item && bomb->value.type == 7 &&
                              bomb->value.amount == 1 && bomb->resource_id == 306);
                    }
                    if (source.class_name == "RemotesPU")
                    {
                        auto* bomb = dynamic_cast<PickupObject*>(g.objects.get(h)->game.get());
                        CHECK(bomb && bomb->value.kind == PickupKind::Item && bomb->value.type == 8 &&
                              bomb->value.amount == 1 && bomb->resource_id == 307);
                    }
                    if (source.class_name == "FlashlightPU")
                    {
                        auto* item = dynamic_cast<PickupObject*>(g.objects.get(h)->game.get());
                        CHECK(item && item->value.kind == PickupKind::Item && item->value.type == 2 &&
                              item->value.amount == 1 && item->resource_id == 301);
                    }
                    missing += !g.objects.get(h)->model;
                }
                CHECK(g.objects.clear() && AmmoPickup::m_dwNumPickups == 0);
            }
            for (const auto& source : g.world.objects)
                if (source.class_name == "CDestructableBrush" || source.class_name == "RotatingBrush")
                    for (const auto& prop : source.properties)
                        if (prop.name == "SpawnObject" && std::holds_alternative<std::string>(prop.value))
                        {
                            const auto& request = std::get<std::string>(prop.value);
                            if (request.empty() || !g.server.ValidateSpawnObject(request))
                                continue;
                            auto h = SpawnObject(request, {100, 100, 100}, {}, {});
                            CHECK(h && g.objects.get(h)->model);
                            ++spawns;
                            CHECK(g.objects.clear() && AmmoPickup::m_dwNumPickups == 0);
                        }
        }
        for (auto& [n, c] : counts)
            std::cout << n << "=" << c << '\n';
        for (auto& [n, c] : failures)
            std::cout << "rejected " << n << "=" << c << '\n';
        std::cout << "worlds=" << worlds << " created=" << created << " missing=" << missing
                  << " supported_spawns=" << spawns << '\n';
        unsigned rejected = 0;
        for (auto& [reason, n] : failures)
        {
            CHECK(reason.ends_with("invalid property TypeAction"));
            rejected += n;
        }
        CHECK(counts["BerettaPU"] == 1 && counts["SubMachineGunPU"] == 4 && counts["FlareGunPU"] == 2 &&
              counts["AssaultRiflePU"] == 5 && counts["MedKitPU"] == 22 && counts["ProximitiesPU"] == 161 &&
              counts["RemotesPU"] == 107 && counts["FlashlightPU"] == 10 && counts["SniperRiflePU"] == 4 &&
              counts["BinocularsPU"] == 4 && counts["ShotgunPU"] == 3 && counts["SingularityPU"] == 1);
        CHECK(failures.empty()); // Retired TypeAction rows are created.
        CHECK(counts["HowitzerPU"] == 1 && counts["NapalmCannonPU"] == 1 && counts["DeathRayPU"] == 1 &&
              counts["TimeBombPU"] == 34);
        // 1785 includes the four SniperRiflePU pickups, 14 StealthPU,
        // 13 InvulnerabilityPU, 4 BinocularsPU, 3 ShotgunPU and 1 SingularityPU. 70 container spawns include the
        // bouncing item/weapon leaves.
        CHECK(counts["StealthPU"] == 14 && counts["InvulnerabilityPU"] == 13);
        // BugSpray, NightGoggles and TheEye add 25 authored pickups; Voodoo adds four.
        CHECK(counts["BugSprayPU"] == 3 && counts["NightGogglesPU"] == 10 && counts["TheEyePU"] == 12);
        CHECK(worlds == 57 && created == 1876 && counts["AngerPU"] == 25 && counts["VoodooDollPU"] == 4 &&
              missing == 0 && rejected == 0 && spawns == 70);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> tests = {
        {"defaults", defaults},   {"ammunition", ammunition}, {"statistics", statistics}, {"protocol", protocol},
        {"contact", contact},     {"spawning", spawning},     {"rejection", rejection},   {"callbacks", callbacks},
        {"resources", resources}, {"catalog", catalog}};
    auto it = tests.find(argv[1]);
    return it != tests.end() && it->second(argv[2]) ? 0 : 1;
}
