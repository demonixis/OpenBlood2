#include "audio_fixture.h"
#include "engine/config.h"
#include "engine/save_fingerprint.h"
#include "engine/save_objects.h"
#include "game/game_settings.h"
#include "game/original/AmmoPickups.h"
#include "game/original/BugSprayProjectile.h"
#include "game/original/CampaignProjectiles.h"
#include "game/original/FlareProjectile.h"
#include "game/original/Grenade.h"
#include "game/original/ItemPickups.h"
#include "game/original/KeyPickup.h"
#include "game/original/LeechProjectile.h"
#include "game/original/NapalmProjectile.h"
#include "game/original/OrbProjectile.h"
#include "game/original/PowerupPickups.h"
#include "game/original/ProximityBomb.h"
#include "game/original/RemoteBomb.h"
#include "game/original/SeeingEye.h"
#include "game/original/SingularityProjectile.h"
#include "game/original/TeslaProjectile.h"
#include "game/original/WeaponPickups.h"
#include "save_rewrites.h"
#include "save_world_fixture.h"
#include "scratch_dir.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <vector>

// Pickups and projectiles (save_load_contracts lot 9): the original PickupObject, KeyPickup,
// CProjectile and SeeingEye byte bodies through GameSession::save_game/load_game, the re-save byte
// identity, the restored player's links to its remote charges and thrown eye, and continuation of
// the restored world as the saved world continued.
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
    std::filesystem::path root;
    using save_world::object;
    using save_world::text;
    using save_world::vector;

    // The world, the player assets and every model, sprite and sound the lot-9 classes use.
    bool write_fixture()
    {
        if (!save_world::write_archive(
                root / "player.rez",
                {{"player",
                  save_world::dat({object("WorldProperties", {text("Name", "WorldProperties0")}),
                                   object("GameStartPoint", {text("Name", "start"), vector("Pos", {0, 10, 0})})})}}))
            return false;
        test_player::Directory dir;
        auto& models = dir.children["models"];
        auto& skins = dir.children["skins"];
        for (auto name : {"c4", "grenade", "orb", "lightining", "nagaspike"})
            models.children["ammo"].files[std::string(name) + ".abc"] = test_model::abc(true).b;
        for (auto name : {"c4_prox", "c4_remote", "c4_time", "grenade", "pulsebeam"})
            skins.children["ammo"].files[std::string(name) + ".dtx"] = save_world::dtx();
        for (auto name : {"bulletammo_pu", "medkit_pu", "m16_pu", "stealth_pu", "theeye_pu", "skull", "orb_pu",
                          "nagaspike_pu", "key", "proximities_pu"})
        {
            models.children["powerups"].files[std::string(name) + ".abc"] = test_model::abc(false, true).b;
            skins.children["powerups"].files[std::string(name) + ".dtx"] = save_world::dtx();
        }
        test_model::Writer sprite;
        for (auto n : {1u, 15u, 1u, 1u, 0u})
            sprite.u32(n);
        const std::string frame = "t.dtx";
        sprite.u16(static_cast<std::uint16_t>(frame.size()));
        sprite.b.insert(sprite.b.end(), frame.begin(), frame.end());
        for (auto name : {"lensflare32", "napalmprime", "teslaprime", "lleechproj", "yellowflare2", "howitzerball"})
            dir.children["sprites"].files[std::string(name) + ".spr"] = sprite.b;
        for (auto name : {"firepart64", "lensflare32_1", "lensflare_1", "drop32_1", "flames1_09", "lightn32",
                          "singlemus02", "smoke64_2", "smoke64_3", "teslaprime_3", "waterblue", "particle1"})
            dir.children["spritetextures"].files[std::string(name) + ".dtx"] = save_world::dtx();
        auto& weapons = dir.children["sounds"].children["weapons"];
        const std::map<std::string, std::vector<std::string>> sounds{
            {"assault", {"altbounce", "projectile"}},
            {"bugbuster", {"impact", "altimpact"}},
            {"napalm", {"impact", "altimpact", "projectile"}},
            {"singularity", {"impact"}},
            {"tesla", {"althit1", "althit2", "althit3", "altimpact", "impact", "projectile"}},
            {"c4", {"explosion_1"}},
            {"flare", {"impact", "projectile"}},
            {"howitzer", {"impact"}},
            {"laser", {"impact"}},
            {"lifeleech", {"projectile"}},
            {"orb", {"altloop", "explode", "loop", "loopdrilling"}},
            {"proximities", {"arm", "detect", "explode", "stick"}},
            {"time", {"explode"}}};
        for (const auto& [folder, names] : sounds)
            for (const auto& name : names)
                weapons.children[folder].files[name + ".wav"] = test_audio::wave();
        dir.children["sounds"].children["powerups"].files["inventory1.wav"] = test_audio::wave();
        for (auto name : {"ammo1", "powerup4", "weapon1"})
            dir.children["sounds"].children["powerups"].files[std::string(name) + ".wav"] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        const auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        std::ofstream out(root / "lot9.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), static_cast<std::streamsize>(w.b.size()));
        return out.good();
    }
    bool setup(std::unique_ptr<GameSession>& game)
    {
        game = std::make_unique<GameSession>();
        std::string error;
        CHECK(game->initialize() && game->resources.mount(root / "player.rez", error) &&
              game->resources.mount(root / "lot9.rez", error));
        CHECK(game->request_world("player", error));
        if (!game->run_world(error))
        {
            std::cerr << "run_world: " << error << '\n';
            return false;
        }
        return true;
    }
    CPlayerObj* player(GameSession& g)
    {
        return g.server_shell.Player(g.local_client);
    }
    bool load(std::unique_ptr<GameSession>& game, const GameSave& save)
    {
        std::string error;
        if (GameSession::load_game(game, save, error))
            return true;
        std::cerr << "load refused at " << game->game_load.stage << ": " << error << '\n';
        return false;
    }
    bool save(GameSession& g, GameSave& out)
    {
        std::string error;
        if (g.save_game(out, error))
            return true;
        std::cerr << "save refused: " << error << '\n';
        return false;
    }
    bool records(const GameSave& save, std::vector<SaveEngineRecord>& out)
    {
        SaveError error;
        return decode_save_records(save.image.objects, out, error);
    }
    bool linked(GameSession& g, HOBJECT owner, HOBJECT target)
    {
        const auto& links = g.objects.links();
        return std::find(links.begin(), links.end(), std::pair{owner, target}) != links.end();
    }
    bool contains(const std::string& text, std::string_view part)
    {
        if (text.find(part) != std::string::npos)
            return true;
        std::cerr << "expected \"" << part << "\" in: " << text << '\n';
        return false;
    }
    template <class T> T* get(GameSession& g, HOBJECT h)
    {
        auto* r = g.objects.get(h);
        return r && r->life == Life::Alive ? dynamic_cast<T*>(r->game.get()) : nullptr;
    }
    // The live object of class T named `name` (projectiles and pickups carry their test names).
    template <class T> T* named(GameSession& g, std::string_view name)
    {
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && r->life == Life::Alive && name == r->state.m_Name)
                return dynamic_cast<T*>(r->game.get());
        return nullptr;
    }
    bool steps(GameSession& g, unsigned n, double dt = .02)
    {
        for (unsigned i = 0; i < n; ++i)
            if (!g.step(dt))
            {
                for (const auto& d : g.objects.diagnostics())
                    std::cerr << d << '\n';
                return false;
            }
        return true;
    }
    template <class T>
    T* fire(GameSession& g, const char* type, const char* name, DVector at, DVector dir = {0, 0, 1}, float speed = 0,
            int radius = 100, HOBJECT owner = nullptr)
    {
        ObjectCreateStruct state;
        state.m_Pos = at;
        legacy_copy(state.m_Name, name);
        auto* p = dynamic_cast<T*>(g.server.CreateObject(g.server.GetClass(type), &state));
        if (!p || !p->Setup(dir, 0, 40, speed, radius, owner ? owner : g.client_shell.player))
        {
            std::cerr << type << " setup failed: " << (p ? p->error + " / " + p->trail_error : "creation") << '\n';
            return nullptr;
        }
        return p;
    }
    // The records of the lot-9 classes (the player's own listed rewrites are lot 5's).
    bool lot9(const SaveEngineRecord& r)
    {
        return r.class_name != "CPlayerObj";
    }
    // Compares the lot-9 records of two saves; `rewrite` normalizes listed original rewrites.
    bool same_records(const GameSave& a, const GameSave& b, const char* label,
                      const std::function<void(SaveEngineRecord&)>& rewrite = {})
    {
        std::vector<SaveEngineRecord> x, y;
        CHECK(records(a, x) && records(b, y));
        std::erase_if(x, [](const auto& r) { return !lot9(r); });
        std::erase_if(y, [](const auto& r) { return !lot9(r); });
        if (x.size() != y.size())
        {
            std::cerr << label << ": " << x.size() << " vs " << y.size() << " records\n";
            return false;
        }
        for (std::size_t i = 0; i < x.size(); ++i)
        {
            if (rewrite)
            {
                rewrite(x[i]);
                rewrite(y[i]);
            }
            if (!(x[i] == y[i]))
            {
                std::cerr << label << ": record " << i << " (" << x[i].class_name << ") differs";
                for (std::size_t k = 0; k < std::min(x[i].payload.size(), y[i].payload.size()); ++k)
                    if (x[i].payload[k] != y[i].payload[k])
                    {
                        std::cerr << " at payload byte " << k;
                        break;
                    }
                if (x[i].payload.size() != y[i].payload.size())
                    std::cerr << " (payload " << x[i].payload.size() << " vs " << y[i].payload.size() << ')';
                if (x[i].flags != y[i].flags)
                    std::cerr << " flags " << std::hex << x[i].flags << " vs " << y[i].flags << std::dec;
                if (x[i].update_remaining != y[i].update_remaining)
                    std::cerr << " delay";
                std::cerr << '\n';
                return false;
            }
        }
        return true;
    }

    // Pickups: an authored ammo box on the ground, a string-spawned bouncing medkit, a weapon with
    // a trigger and no ammo, a rotating powerup and an authored key.
    bool pickups()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        auto& g = *game;
        auto box = g.objects.create(g.objects.find_class("BulletAmmoPU"), {}, Creation::World,
                                    {{"Name", std::string("box")}, {"Pos", DVector{300, 40, 0}}});
        auto kit = g.objects.create(g.objects.find_class("MedKitPU"), {}, Creation::String,
                                    {{"Name", std::string("kit")}, {"Pos", DVector{-300, 40, 0}}});
        auto rifle = g.objects.create(g.objects.find_class("AssaultRiflePU"), {}, Creation::World,
                                      {{"Name", std::string("rifle")},
                                       {"Pos", DVector{0, 40, 300}},
                                       {"PickupTriggerTarget", std::string("door")},
                                       {"PickupTriggerMessage", std::string("open")}});
        auto stealth = g.objects.create(g.objects.find_class("StealthPU"), {}, Creation::World,
                                        {{"Name", std::string("stealth")}, {"Pos", DVector{0, 40, -300}}});
        auto key = g.objects.create(g.objects.find_class("KeyPickup"), {}, Creation::World,
                                    {{"Name", std::string("keyobj")},
                                     {"Pos", DVector{200, 40, 200}},
                                     {"ObjectName", std::string("Red Key")},
                                     {"IconFile", std::string("red.pcx")},
                                     {"HiIconFile", std::string("red_h.pcx")},
                                     {"ModelFile", std::string("models\\powerups\\key.abc")},
                                     {"ModelSkin", std::string("skins\\powerups\\key.dtx")},
                                     {"UseCount", std::int32_t(3)},
                                     {"PickupTriggerMessage", std::string("unlock")}});
        CHECK(box && kit && rifle && stealth && key);
        get<AssaultRiflePU>(g, rifle)->value.amount = 0; // A drop without ammunition.
        auto* medkit = get<MedKitPU>(g, kit);
        CHECK(medkit && medkit->bouncing && medkit->bounce_acceleration == 500);
        CHECK(steps(g, 3));
        const auto kit_rest = medkit->rest_position;
        const auto key_flags = get<KeyPickup>(g, key)->m_dwFlags;
        GameSave saved;
        CHECK(save(g, saved));
        CHECK(load(game, saved));
        auto& h = *game;
        auto* b = named<BulletAmmoPU>(h, "box");
        auto* m = named<MedKitPU>(h, "kit");
        auto* r = named<AssaultRiflePU>(h, "rifle");
        auto* s = named<StealthPU>(h, "stealth");
        auto* k = named<KeyPickup>(h, "keyobj");
        CHECK(b && m && r && s && k);
        CHECK(b->value.amount == 100 && b->m_Link.m_pData == b && b->error.empty()); // Rejoined the AI ammo list.
        CHECK(m->bouncing && m->bounce_acceleration == 500 && m->rest_position.y == kit_rest.y);
        CHECK(r->value.amount == 0 && r->pickup_target == "door" && r->pickup_command == "open");
        CHECK(s->rotate_fx && (h.server.GetObjectUserFlags(s->m_hObject) & (1u << 4)));
        CHECK(k->item.name == "Red Key" && k->item.display_name == "Red Key" && k->item.icon == "red.pcx" &&
              k->item.highlighted_icon == "red_h.pcx" && k->item.uses == 3 && k->pickup_command == "unlock" &&
              k->pickup_target.empty() && k->model_file == "models\\powerups\\key.abc" && k->m_dwFlags == key_flags);
        GameSave again;
        CHECK(save(h, again));
        CHECK(same_records(saved, again, "pickups re-save"));
        // The restored pickups still give themselves away: the player collects the ammunition.
        auto* p = player(h);
        const float ammo = p->m_InventoryMgr.GetAmmoCount(1);
        DVector there;
        CHECK(h.server.GetObjectPos(b->m_hObject, &there) == DE_OK);
        CHECK(h.server.TeleportObject(p->m_hObject, &there) == DE_OK);
        CHECK(steps(h, 5) && !named<BulletAmmoPU>(h, "box") && p->m_InventoryMgr.GetAmmoCount(1) > ammo);
        std::cout << "pickups: " << saved.image.records << " records\n";
        return true;
    }

    // Original rewrites listed for pickups: PostPropRead re-adds TOUCH_NOTIFY, VISIBLE and GRAVITY.
    bool pickup_rewrite()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        auto& g = *game;
        auto box = g.objects.create(g.objects.find_class("BulletAmmoPU"), {}, Creation::World,
                                    {{"Name", std::string("hidden")}, {"Pos", DVector{300, 40, 0}}});
        CHECK(box && steps(g, 1));
        g.server.SetObjectFlags(box, FLAG_VISIBLE); // As a pickup without model dimensions has.
        GameSave saved, again;
        CHECK(save(g, saved) && load(game, saved));
        auto* b = named<BulletAmmoPU>(*game, "hidden");
        CHECK(b && (game->server.GetObjectFlags(b->m_hObject) & (FLAG_TOUCH_NOTIFY | FLAG_GRAVITY)) ==
                       (FLAG_TOUCH_NOTIFY | FLAG_GRAVITY));
        CHECK(save(*game, again));
        CHECK(same_records(saved, again, "pickup flag rewrite",
                           [](SaveEngineRecord& r) { r.flags |= FLAG_TOUCH_NOTIFY | FLAG_GRAVITY; }));
        return true;
    }

    // Bombs, a grenade in flight and the remote charges the player still owns.
    bool bombs()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        auto& g = *game;
        auto* p = player(g);
        CHECK(p);
        const SegmentHit floor{{}, {0, 1, 0}, nullptr, 0};
        auto* prox = fire<CProximityBomb>(g, "CProximityBomb", "prox", {600, 2, 0});
        auto* timer = fire<CTimeBomb>(g, "CTimeBomb", "timer", {-600, 2, 0});
        auto* remote = fire<CRemoteBomb>(g, "CRemoteBomb", "remote", {0, 2, 600});
        auto* second = fire<CRemoteBomb>(g, "CRemoteBomb", "second", {0, 900, -600});
        auto* grenade = fire<CGrenade>(g, "CGrenade", "grenade", {-400, 300, -400}, {0, 1, 0}, 100, 200);
        CHECK(prox && timer && remote && second && grenade);
        CHECK(p->AddRemoteBomb(remote->m_hObject) && p->AddRemoteBomb(second->m_hObject));
        prox->HandleContact(floor);
        timer->HandleContact(floor);
        remote->HandleContact(floor);
        CHECK(prox->armed && timer->armed && remote->armed && !second->armed);
        CHECK(steps(g, 10)); // The prox bomb arms its detector after 0.75 s; the timer counts down.
        const float timer_start = timer->m_fStartTime, pitch = second->pitch;
        CHECK(!prox->m_bExplode && !timer->m_bExplode && prox->play_arm);
        GameSave saved;
        CHECK(save(g, saved));
        // The saved world continues: the time bomb explodes 5 s after it stuck.
        CHECK(steps(g, 260) && !named<CTimeBomb>(g, "timer"));
        CHECK(load(game, saved));
        auto& h = *game;
        p = player(h);
        auto* x = named<CProximityBomb>(h, "prox");
        auto* t = named<CTimeBomb>(h, "timer");
        auto* r = named<CRemoteBomb>(h, "remote");
        auto* s = named<CRemoteBomb>(h, "second");
        auto* n = named<CGrenade>(h, "grenade");
        CHECK(p && x && t && r && s && n);
        CHECK(x->armed && x->configured && x->m_hFiredFrom == p->m_hObject && x->play_arm && x->get_away == .75f);
        CHECK(t->armed && t->m_fStartTime == timer_start && !t->m_bExplode);
        CHECK(r->armed && !s->armed && s->pitch == pitch && r->m_hFiredFrom == p->m_hObject);
        CHECK(n->bounces_left == 5 && n->m_fLifeTime == 3 && n->m_hFiredFrom == p->m_hObject);
        // The restored player lists both charges again, in the saved world's order.
        CHECK(p->remote_bombs && p->remote_bombs->m_nInList == 2 &&
              p->remote_bombs->m_pFirstLink->m_hObject == s->m_hObject);
        CHECK(linked(h, p->m_hObject, r->m_hObject) && linked(h, r->m_hObject, p->m_hObject));
        GameSave again;
        CHECK(save(h, again));
        CHECK(same_records(saved, again, "bombs re-save"));
        CHECK(saved.image.links == again.image.links);
        // The time bomb keeps its countdown from the restored clock, then explodes.
        const auto remote_handle = r->m_hObject, timer_handle = t->m_hObject;
        unsigned frames = 0;
        while (h.objects.get(timer_handle) && frames < 400)
        {
            CHECK(steps(h, 1));
            ++frames;
        }
        CHECK(!h.objects.get(timer_handle) && h.server.GetTime() >= timer_start + 5);
        // The player still detonates its restored charges.
        p->command_run = true;
        CHECK(p->OnCommandDetonate() && p->remote_signals == 2 && !p->remote_bombs);
        CHECK(steps(h, 2) && !h.objects.get(remote_handle));
        // The proximity bomb detects the player and explodes.
        DVector near{560, 10, 0};
        CHECK(h.server.TeleportObject(p->m_hObject, &near) == DE_OK);
        CHECK(steps(h, 60) && !named<CProximityBomb>(h, "prox"));
        std::cout << "bombs: " << saved.image.records << " records, timer explodes after " << frames << " frames\n";
        return true;
    }

    // The thrown eye: stuck on the floor, then picked up again after a load.
    bool eye()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        auto& g = *game;
        auto* p = player(g);
        p->m_InventoryMgr.eye = std::make_unique<CInvTheEye>(p->m_hObject);
        CHECK(p->m_InventoryMgr.eye->ActivateItem());
        const auto thrown = p->m_InventoryMgr.eye->eye;
        auto* e = get<SeeingEye>(g, thrown);
        CHECK(e);
        for (int i = 0; i < 200 && !e->dropped; ++i)
            CHECK(steps(g, 1));
        CHECK(e->dropped);
        DVector away{400, 10, 400};
        CHECK(g.server.TeleportObject(p->m_hObject, &away) == DE_OK && steps(g, 2));
        CHECK(p->m_InventoryMgr.eye->SetActive(true)); // Looking through the eye.
        GameSave saved;
        CHECK(save(g, saved));
        CHECK(load(game, saved));
        auto& h = *game;
        p = player(h);
        auto* item = p->m_InventoryMgr.eye.get();
        CHECK(item && item->available && item->eye && !item->active); // Every item loads inactive.
        e = get<SeeingEye>(h, item->eye);
        CHECK(e && e->dropped && e->configured && e->m_hFiredFrom == p->m_hObject && !e->client && !e->stuck);
        CHECK(linked(h, e->m_hObject, p->m_hObject));
        GameSave again;
        CHECK(save(h, again));
        CHECK(same_records(saved, again, "eye re-save") && saved.image.links == again.image.links);
        // Looking through it again, then destroying it, turns the view off on the new client.
        CHECK(item->SetActive(true));
        const auto eye_handle = item->eye;
        CHECK(h.objects.remove(eye_handle) && h.objects.finish_frame());
        CHECK(!item->eye && !item->active && !item->available);
        std::cout << "eye: " << saved.image.records << " records\n";
        return true;
    }

    // Every projectile class in flight: the restored world behaves as the saved world did.
    bool family()
    {
        struct Kind
        {
            const char* type;
            DVector at, dir;
            float speed;
        };
        const std::vector<Kind> kinds{
            {"CGrenade", {-500, 300, 0}, {1, 0, 0}, 200},
            {"CFlareProjectile", {-500, 300, 100}, {1, 0, 0}, 200},
            {"CFlareAltProjectile", {-500, 300, 200}, {1, 0, 0}, 200},
            {"CTeslaProjectile", {-500, 300, 300}, {1, 0, 0}, 200},
            {"CTeslaBallProjectile", {850, 300, -850}, {0, -1, 0}, 0},
            {"CBugSprayProjectile", {-500, 300, -100}, {1, 0, 0}, 200},
            {"CBugSprayAltProjectile", {-500, 300, -200}, {1, 0, 0}, 200},
            {"CNapalmProjectile", {-500, 300, -300}, {1, 0, 0}, 200},
            {"CShikariLoogieProjectile", {-500, 300, -400}, {1, 0, 0}, 200},
            {"COrbProjectile", {500, 300, 0}, {1, 0, 0}, 200},
            {"CLeechPrimeProjectile", {500, 300, 100}, {1, 0, 0}, 200},
            {"CSingularityProjectile", {-850, 300, -850}, {0, -1, 0}, 0},
            {"CEnergyBlastProjectile", {500, 300, 300}, {1, 0, 0}, 200},
            {"CNagaSpikeProjectile", {500, 300, 400}, {1, 0, 0}, 200},
            {"CGroundStrikeProjectile", {500, 300, -100}, {1, 0, 0}, 200},
            {"CHowitzerAltFrag", {500, 300, -200}, {1, 0, 0}, 200},
            {"CNapalmFireball", {500, 300, -300}, {1, 0, 0}, 200},
            {"CVomitProjectile", {500, 300, -400}, {1, 0, 0}, 200},
            {"CSkullProjectile", {300, 300, 500}, {1, 0, 0}, 200},
            {"CDeathRayProjectile", {300, 300, -500}, {0, -1, 0}, 0},
        };
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        auto& g = *game;
        CHECK(steps(g, 300)); // Past both attract times, so the lost attract clocks show.
        for (const auto& k : kinds)
            CHECK(fire<CProjectile>(g, k.type, k.type, k.at, k.dir, k.speed));
        auto* burst = fire<CFlareBurstProjectile>(g, "CFlareBurstProjectile", "burst", {300, 300, 300}, {1, 0, 0}, 200);
        CHECK(burst && burst->SetColor({1, .25f, 0}));
        CHECK(steps(g, 2));
        // One-update projectiles exist only between their creation and their first update: fire
        // them right before the save.
        const std::vector<Kind> instant{
            {"CTeslaBoltProjectile", {-300, 300, 500}, {0, -1, 0}, 0},
            {"CLeechAltProjectile", {-300, 300, -500}, {0, -1, 0}, 0},
            {"CHowitzerShell", {-200, 300, 500}, {0, -1, 0}, 0},
            {"CHowitzerAltShell", {-200, 300, -500}, {0, -1, 0}, 0},
            {"CNapalmAltProjectile", {-100, 300, 500}, {1, 0, 0}, 200},
            {"CBehemothShockwaveProjectile", {-100, 300, -500}, {0, -1, 0}, 0},
            {"CShockwaveProjectile", {100, 300, 500}, {0, -1, 0}, 0},
            {"CNagaDebrisProjectile", {100, 300, -500}, {0, -1, 0}, 0},
            {"COrbAltProjectile", {200, 300, 500}, {1, 0, 0}, 200},
        };
        for (const auto& k : instant)
            CHECK(fire<CProjectile>(g, k.type, k.type, k.at, k.dir, k.speed));
        auto all = kinds;
        all.insert(all.end(), instant.begin(), instant.end());
        std::set<std::string> in_save;
        for (const auto& k : all)
            if (named<CProjectile>(g, k.type))
                in_save.insert(k.type);
        for (const auto& k : all)
            if (!in_save.contains(k.type))
                std::cout << k.type << " ended before the save\n";
        GameSave saved;
        CHECK(save(g, saved));
        // The saved world's outcome after 20 frames: which projectiles still fly.
        CHECK(steps(g, 20));
        std::set<std::string> live, failed;
        for (const auto& k : all)
            if (named<CProjectile>(g, k.type))
                live.insert(k.type);
        for (const auto& report : g.server.projectile_reports)
            if (report.outcome == "failed")
                failed.insert(report.source);
        CHECK(load(game, saved));
        auto& h = *game;
        for (const auto& name : in_save)
        {
            auto* p = named<CProjectile>(h, name);
            CHECK(p && p->configured && p->m_hFiredFrom == player(h)->m_hObject);
        }
        // A flare fragment keeps the colour its last update set (the original reads it unset).
        auto* fragment = named<CFlareBurstProjectile>(h, "burst");
        CHECK(fragment && fragment->color_ready && fragment->color.y == .25f && fragment->trail &&
              fragment->trail_error.empty());
        auto* flare = named<CFlareProjectile>(h, "CFlareProjectile");
        CHECK(flare && flare->trail && flare->trail_error.empty());
        for (auto* p : {static_cast<CProjectile*>(fragment), static_cast<CProjectile*>(flare)})
            CHECK(std::count_if(h.server.object_particles.active.begin(), h.server.object_particles.active.end(),
                                [&](const auto& fx) { return fx.owner == p->m_hObject; }) == 1);
        for (const char* name : {"CGrenade", "CNapalmProjectile", "CHowitzerAltFrag", "CEnergyBlastProjectile",
                                 "CGroundStrikeProjectile", "CShikariLoogieProjectile"})
        {
            auto* p = named<CProjectile>(h, name);
            CHECK(p && std::count_if(h.server.object_particles.active.begin(), h.server.object_particles.active.end(),
                                     [&](const auto& fx) { return fx.owner == p->m_hObject; }) == 1);
        }
        GameSave again;
        CHECK(save(h, again));
        CHECK(same_records(saved, again, "family re-save"));
        CHECK(steps(h, 20));
        std::set<std::string> live_after, failed_after;
        for (const auto& k : all)
            if (named<CProjectile>(h, k.type))
                live_after.insert(k.type);
        for (const auto& report : h.server.projectile_reports)
            if (report.outcome == "failed")
                failed_after.insert(report.source);
        // Original loss: the Tesla ball's and Singularity's attract clocks are their unsaved shadow
        // m_fStartTime (constructor 0), so an armed one ends on its first update after a load.
        for (auto name : {"CTeslaBallProjectile", "CSingularityProjectile"})
        {
            CHECK(in_save.contains(name) && live.contains(name) && !live_after.contains(name));
            live.erase(name);
        }
        if (live != live_after || failed_after != failed)
        {
            for (const auto& name : live)
                if (!live_after.contains(name))
                    std::cerr << name << " ended early after the load\n";
            for (const auto& name : live_after)
                if (!live.contains(name))
                    std::cerr << name << " outlived the saved world\n";
            for (const auto& name : failed_after)
                if (!failed.contains(name))
                    std::cerr << name << " failed after the load\n";
            return false;
        }
        std::cout << "family: " << in_save.size() << " of " << all.size() << " classes saved in flight, " << live.size()
                  << " still flying after 20 more frames\n";
        return true;
    }

    // Corrupt payloads are refused by class name before any member changes.
    bool refusals()
    {
        std::unique_ptr<GameSession> game;
        CHECK(setup(game));
        auto& g = *game;
        auto box = g.objects.create(g.objects.find_class("BulletAmmoPU"), {}, Creation::World,
                                    {{"Name", std::string("box")}, {"Pos", DVector{300, 40, 0}}});
        CHECK(box && fire<CGrenade>(g, "CGrenade", "grenade", {-400, 300, -400}, {0, 1, 0}, 100));
        GameSave saved;
        CHECK(save(g, saved));
        auto edit = [&](const char* type, const std::function<void(std::vector<std::uint8_t>&)>& change)
        {
            std::vector<SaveEngineRecord> list;
            records(saved, list);
            for (auto& r : list)
                if (r.class_name == type)
                    change(r.payload);
            GameSave out = saved;
            SaveError error;
            encode_save_records(list, out.image.objects, error);
            return out;
        };
        auto refused = [&](const GameSave& bad, std::string_view expected)
        {
            std::string error;
            const auto before = game.get();
            if (GameSession::load_game(game, bad, error))
                return false;
            return game.get() == before && contains(error, expected);
        };
        // PickupObject: m_bFirstUpdate is byte 0; m_nType byte 1; m_fRespawnTime bytes 6..9.
        CHECK(refused(edit("BulletAmmoPU", [](auto& p) { p[0] = 2; }), "BulletAmmoPU"));
        CHECK(refused(edit("BulletAmmoPU", [](auto& p) { p[1] = 3; }), "pickup type"));
        CHECK(refused(edit("BulletAmmoPU", [](auto& p) { p[9] = 0x3f; }), "multiplayer"));
        // CProjectile: the damage type is byte 20 and m_bRemoveFromWorld byte 29.
        CHECK(refused(edit("CGrenade", [](auto& p) { p[20] = 2; }), "damage type"));
        CHECK(refused(edit("CGrenade", [](auto& p) { p[29] = 1; }), "presentation"));
        CHECK(refused(edit("CGrenade", [](auto& p) { p.pop_back(); }), "CGrenade"));
        // The untouched save still loads.
        CHECK(load(game, saved));
        return true;
    }

    // Relative float clocks restore to the same saved difference but may land one ulp away
    // (lot 8); later saves may then differ by one ulp in that field. Accept exactly that.
    bool within_clock_ulp(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b)
    {
        if (a.size() != b.size())
            return false;
        auto value = [](const std::vector<std::uint8_t>& p, std::size_t at)
        {
            std::uint32_t bits = 0;
            for (unsigned i = 0; i < 4; ++i)
                bits |= std::uint32_t(p[at + i]) << (8 * i);
            return std::bit_cast<float>(bits);
        };
        for (std::size_t k = 0; k < a.size(); ++k)
        {
            if (a[k] == b[k])
                continue;
            bool explained = false;
            for (std::size_t start = k >= 3 ? k - 3 : 0; start <= k && start + 4 <= a.size() && !explained; ++start)
            {
                const float x = value(a, start), y = value(b, start);
                if (std::isfinite(x) && std::isfinite(y) && std::nextafter(x, y) == y)
                {
                    explained = true;
                    k = start + 3;
                }
            }
            if (!explained)
                return false;
        }
        return true;
    }

    // Real worlds (private data). For every mounted world, after entry and 30 frames: the classes
    // still blocking a whole save, then an image of every Original object (the player, lots 8 and
    // 9) through the full restore-mode world and candidate transaction, saved again byte for byte
    // apart from the player's listed rewrites and continued for 30 frames. A world with no blocking
    // class restores whole.
    bool private_worlds(const std::filesystem::path& data)
    {
        std::vector<std::string> worlds;
        {
            GameSession g;
            CHECK(g.initialize());
            Config config;
            config.data_dir = data;
            std::string error;
            CHECK(g.resources.mount_game(config, error));
            for (const auto& [path, resource] : g.resources.index())
            {
                (void)resource;
                if (path.starts_with("worlds/") && path.ends_with(".dat"))
                    worlds.push_back(path.substr(7, path.size() - 11));
            }
        }
        unsigned entered = 0, restored = 0, whole = 0;
        std::size_t lot9_records = 0;
        std::map<std::string, unsigned> blocking_worlds;
        std::vector<std::string> whole_names;
        for (const auto& name : worlds)
        {
            auto game = std::make_unique<GameSession>();
            CHECK(game->initialize());
            Config config;
            config.data_dir = data;
            std::string error;
            CHECK(game->resources.mount_game(config, error));
            if (!game->request_world(name, error))
            {
                std::cout << name << ": not entered (" << error.substr(0, 120) << ")\n";
                continue;
            }
            game->server_shell.inspection_start = true;
            if (!game->enter_player(error))
            {
                std::cout << name << ": no player (" << error.substr(0, 120) << ")\n";
                continue;
            }
            if (auto* p = player(*game); p && p->ctf_team)
                p->ctf_team = 0; // CTF match state is never saved (D12).
            ++entered;
            CHECK(steps(*game, 30, .05));
            std::map<std::string, unsigned> blocking;
            std::vector<HOBJECT> covered;
            std::size_t pickups = 0, projectiles = 0;
            for (auto id : game->objects.snapshot())
            {
                const auto* r = game->objects.get(id);
                if (!r || r->life != Life::Alive || !(r->user_flags & USRFLG_SAVEABLE))
                    continue;
                const auto support = save_support(r->type);
                if (support == SaveSupport::Unsupported)
                    ++blocking[r->type->name];
                if (support != SaveSupport::Original)
                    continue;
                covered.push_back(r->handle);
                pickups +=
                    dynamic_cast<const PickupObject*>(r->game.get()) || dynamic_cast<const KeyPickup*>(r->game.get());
                projectiles += dynamic_cast<const CProjectile*>(r->game.get()) != nullptr;
            }
            std::string names;
            for (const auto& [type, count] : blocking)
            {
                names += (names.empty() ? "" : ", ") + type + " x" + std::to_string(count);
                ++blocking_worlds[type];
            }
            std::cout << name << ": " << pickups << " pickups, " << projectiles << " projectiles; "
                      << (blocking.empty() ? "no blocking class" : "blocked by " + names) << '\n';
            GameSave save;
            if (blocking.empty() && !game->save_game(save, error))
            {
                // Lot 13 refuses a CTF world by name: its team flags are multiplayer state.
                CHECK(error.find("CTF games cannot be saved") != std::string::npos);
                blocking["CTF refusal"] = 1;
                ++blocking_worlds["CTF refusal"];
                std::cout << "  whole save refused: CTF\n";
            }
            if (!blocking.empty())
            {
                SaveObjectsReport report;
                CHECK(game->server.save_objects("Save\\Current\\Current.sav", covered, 0,
                                                SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS, report));
                save = {game->world_path, {}, *game->server.save_store.find(SaveStoreSlot::Current)};
                SaveError fingerprint;
                CHECK(build_fingerprint(game->resources, save.world_path, save.image.classes, save.fingerprint,
                                        fingerprint));
            }
            if (!GameSession::load_game(game, save, error))
            {
                std::cerr << name << " load refused at " << game->game_load.stage << ": " << error << '\n';
                return false;
            }
            GameSave again;
            if (blocking.empty())
                CHECK(game->save_game(again, error));
            else
            {
                std::vector<HOBJECT> live;
                for (auto h : game->server.last_restore.objects)
                    if (game->server.IsObjectAlive(h))
                        live.push_back(h);
                SaveObjectsReport report;
                CHECK(game->server.save_objects("Save\\Current\\Current.sav", live, 0,
                                                SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS, report));
                again.image = *game->server.save_store.find(SaveStoreSlot::Current);
            }
            std::vector<SaveEngineRecord> x, y;
            CHECK(records(save, x) && records(again, y) && x.size() == y.size());
            save_rewrites::apply(x);
            save_rewrites::apply(y);
            for (std::size_t i = 0; i < x.size(); ++i)
            {
                if (x[i].payload != y[i].payload && within_clock_ulp(x[i].payload, y[i].payload))
                    y[i].payload = x[i].payload; // Relative float clocks (lot 8).
                if (!(x[i] == y[i]))
                {
                    std::cerr << name << ": record " << i << " (" << x[i].class_name << ") saves differently";
                    for (std::size_t k = 0; k < std::min(x[i].payload.size(), y[i].payload.size()); ++k)
                        if (x[i].payload[k] != y[i].payload[k])
                        {
                            std::cerr << " at payload byte " << k;
                            break;
                        }
                    std::cerr << (x[i].flags != y[i].flags ? " flags" : "")
                              << (x[i].update_remaining != y[i].update_remaining ? " delay" : "")
                              << (x[i].scheduled != y[i].scheduled ? " scheduled" : "")
                              << (x[i].payload.size() != y[i].payload.size() ? " size" : "") << '\n';
                    return false;
                }
                lot9_records += x[i].class_name != "CPlayerObj";
            }
            CHECK(steps(*game, 30, .05) && game->server.unavailable_services.empty());
            ++restored;
            if (blocking.empty())
            {
                ++whole;
                whole_names.push_back(name);
            }
        }
        std::cout << entered << " of " << worlds.size() << " worlds entered, " << restored
                  << " restored their Original objects (" << lot9_records << " non-player records), " << whole
                  << " whole:";
        for (const auto& n : whole_names)
            std::cout << ' ' << n;
        std::cout << "\nBlocking classes (worlds):";
        for (const auto& [type, count] : blocking_worlds)
            std::cout << ' ' << type << '(' << count << ')';
        std::cout << '\n';
        CHECK(entered > 0 && restored == entered);
        return true;
    }

    bool run(const std::string& name)
    {
        if (name == "pickups")
            return pickups();
        if (name == "rewrites")
            return pickup_rewrite();
        if (name == "bombs")
            return bombs();
        if (name == "eye")
            return eye();
        if (name == "family")
            return family();
        if (name == "refusals")
            return refusals();
        std::cerr << "unknown case " << name << '\n';
        return false;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    if (std::string_view(argv[1]) == "private")
        return private_worlds(argv[2]) ? 0 : 1;
    root = argv[2];
    if (!OpenBlood2::Testing::reset_scratch_dir(root))
        return 2;
    if (!write_fixture())
    {
        std::cerr << "fixture failed\n";
        return 1;
    }
    return run(argv[1]) ? 0 : 1;
}
