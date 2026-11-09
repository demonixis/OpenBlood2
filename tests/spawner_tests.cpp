#include "audio_fixture.h"
#include "game/original/AI_Mgr.h"
#include "game/original/Door.h"
#include "game/original/Spawner.h"
#include "game/original/Trigger.h"
#include <cmath>
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
    bool setup(GameSession& g, const std::filesystem::path& root)
    {
        if (!test_player::setup(g, root, false, true, true))
            return false;
        test_player::Directory d;
        d.children["sounds"].children["ambient"].files["warp1.wav"] = test_audio::wave();
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [off, size] = test_player::write_dir(w, d);
        w.patch(131, off);
        w.patch(135, size);
        std::ofstream out(root / "spawner-sound.rez", std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        std::string error;
        return g.resources.mount(root / "spawner-sound.rez", error);
    }
    HOBJECT make(GameSession& g, Properties props = {}, Creation reason = Creation::World)
    {
        props.try_emplace("Name", std::string("spawn1"));
        props.try_emplace("Pos", DVector{10, 100, 20});
        props.try_emplace("SpawnObject", std::string("HealthPU"));
        return g.objects.create(g.objects.find_class("Spawner"), {}, reason, props);
    }
    Spawner* get(GameSession& g, HOBJECT h)
    {
        auto* r = g.objects.get(h);
        return r ? dynamic_cast<Spawner*>(r->game.get()) : nullptr;
    }
    HOBJECT sender(GameSession& g, DVector pos)
    {
        ObjectCreateStruct s;
        s.m_Pos = pos;
        s.m_Rotation = rotation_from_euler({0, 1, 0});
        SAFE_STRCPY(s.m_Name, "relay");
        return g.objects.create(g.objects.find_class("BaseClass"), s);
    }
    bool send(GameSession& g, HOBJECT from, HOBJECT to, const char* text)
    {
        auto message = g.server.StartMessageToObject(from ? g.server.HandleToObject(from) : nullptr, to, 1000);
        auto string = g.server.CreateString(text);
        CHECK(message && string && g.server.WriteToMessageHString(message, string) == DE_OK);
        g.server.FreeString(string);
        CHECK(g.server.EndMessage(message) == DE_OK);
        return true;
    }
    bool same(DVector a, DVector b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    bool properties(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        // World files read the registration defaults (which differ from the constructor).
        auto h = make(g);
        auto* s = get(g, h);
        CHECK(s && same(s->m_vMinVelocity, {0, 500, 0}) && same(s->m_vMaxVelocity, {0, 500, 0}));
        CHECK(s->m_bUseTriggerObjPos && !s->m_bStartActive && !s->m_bCreateRiftEffect && s->m_hstrSound.empty());
        CHECK(s->m_dwRespawnCount == 0xffffffffu && s->m_fRespawnRate == 1 && s->m_fSoundRadius == 500);
        auto* r = g.objects.get(h);
        CHECK(r->state.m_ObjectType == OT_NORMAL && !r->scheduled && (r->user_flags & (1u << 3)));
        auto active = make(g, {{"StartActive", true}, {"RespawnRate", 2.5f}, {"RespawnCount", std::int32_t(3)}});
        CHECK(active && g.objects.get(active)->scheduled && g.objects.get(active)->due == 2.5);
        CHECK(get(g, active)->m_dwRespawnCount == 3);
        // String creation does not read properties: constructor defaults remain.
        auto text = make(g, {{"StartActive", true}}, Creation::String);
        s = get(g, text);
        CHECK(s && same(s->m_vMinVelocity, {-100, 400, -100}) && same(s->m_vMaxVelocity, {100, 600, 100}));
        CHECK(!s->m_bUseTriggerObjPos && !s->m_bStartActive && s->m_hstrSpawnObject.empty());
        CHECK(!g.objects.get(text)->scheduled);
        // Explicit bounds, restore and unknown properties reject.
        const float nan = std::numeric_limits<float>::quiet_NaN();
        CHECK(!make(g, {{"MinVelocity", DVector{nan, 0, 0}}}));
        CHECK(!make(g, {{"RespawnRate", -1.0f}}));
        CHECK(!make(g, {{"SoundRadius", -5.0f}}));
        CHECK(!make(g, {{"Bogus", true}}));
        // Save/load lot 7 (decision-4 departure): a savegame spawner keeps the restored schedule.
        const auto restored = make(g, {}, Creation::Restore);
        CHECK(restored && !g.objects.get(restored)->scheduled);
        // Original GenericProp/szSpawn hold MAX_GP_STRING_LEN (200) bytes; 24_AncientCity authors 141.
        const std::string padded =
            "HealthPU Name " + std::string(80, 'x') + ";PickupTriggerTarget " + std::string(40, 'y');
        CHECK(padded.size() > 140 && make(g, {{"SpawnObject", padded}}) && g.server.ValidateSpawnObject(padded));
        CHECK(!make(g, {{"SpawnObject", padded + ";PickupTriggerMessage " + std::string(40, 'z')}}));
        // Retired editor flag on older DAT rows: accepted on every class, never read or defaulted.
        auto legacy = make(g, {{"TypeAction", true}});
        CHECK(legacy && g.objects.get(legacy)->properties.contains("TypeAction"));
        CHECK(!g.objects.get(make(g))->properties.contains("TypeAction"));
        CHECK(!make(g, {{"TypeAction", 1.0f}})); // Known names keep their type.
        return true;
    }
    bool commands(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = make(g, {{"Rotation", EulerAngles{{0, 2, 0}}}});
        auto relay = sender(g, {-50, 100, 70});
        CHECK(h && relay);
        auto* s = get(g, h);
        auto* r = g.objects.get(h);
        // Whole-string, case-insensitive START/STOP schedule without spawning.
        CHECK(send(g, relay, h, "start") && r->scheduled && r->due == g.objects.time() + 1);
        CHECK(send(g, relay, h, "STOP") && !r->scheduled && g.server.spawn_reports.empty());
        // SINGLE uses the spawner's own pose even with UseTriggerObjPos, and decrements.
        CHECK(send(g, relay, h, "Single") && s->m_dwRespawnCount == 0xfffffffeu);
        CHECK(g.server.spawn_reports.size() == 1 && same(g.server.spawn_reports.back().position, {10, 100, 20}));
        auto child = g.server.spawn_reports.back().child;
        CHECK(child && g.objects.get(child)->type->name == "HealthPU" && g.server.spawn_reports[0].source == "spawn1");
        // Any other payload spawns at the triggering object's pose.
        CHECK(send(g, relay, h, "TRIGGER") && g.server.spawn_reports.size() == 2);
        child = g.server.spawn_reports.back().child;
        CHECK(child && same(g.objects.get(child)->state.m_Pos, {-50, 100, 70}));
        const auto q = g.objects.get(child)->state.m_Rotation, expected = g.objects.get(relay)->state.m_Rotation;
        CHECK(q.m_Spin == expected.m_Spin && q.m_Vec.y == expected.m_Vec.y);
        CHECK(send(g, relay, h, "STARTNOW") && g.server.spawn_reports.size() == 3);
        CHECK(s->triggers == 5 && s->spawned == 3 && s->spawn_failures == 0);
        // Without a sender the configured position is used.
        CHECK(send(g, nullptr, h, "go") && same(g.server.spawn_reports.back().position, {10, 100, 20}));
        // SINGLE is ignored when the count is exhausted.
        auto once = make(g, {{"RespawnCount", std::int32_t(0)}});
        const auto reports = g.server.spawn_reports.size();
        CHECK(send(g, relay, once, "SINGLE") && get(g, once)->m_dwRespawnCount == 0);
        CHECK(g.server.spawn_reports.size() == reports && get(g, once)->spawn_attempts == 0);
        // An empty SpawnObject makes no attempt and records nothing.
        auto empty = make(g, {{"SpawnObject", std::string()}, {"Sound", std::string("sounds\\ambient\\warp1.wav")}});
        CHECK(send(g, relay, empty, "TRIGGER") && g.server.spawn_reports.size() == reports &&
              !get(g, empty)->last_voice);
        return true;
    }
    bool respawn(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto twice =
            make(g, {{"StartActive", true}, {"RespawnRate", .5f}, {"RespawnCount", std::int32_t(2)}}, Creation::World);
        auto zero = make(g, {{"Name", std::string("zero")},
                             {"StartActive", true},
                             {"RespawnRate", .5f},
                             {"RespawnCount", std::int32_t(0)}});
        auto endless = make(g, {{"Name", std::string("endless")}, {"StartActive", true}, {"RespawnRate", .5f}});
        CHECK(twice && zero && endless);
        CHECK(g.step(.4) && g.server.spawn_reports.empty());
        CHECK(g.step(.11) && g.server.spawn_reports.size() == 3);
        CHECK(get(g, twice)->m_dwRespawnCount == 1 && g.objects.get(twice)->scheduled);
        // A zero count is not decremented, spawns once and stops.
        CHECK(get(g, zero)->m_dwRespawnCount == 0 && !g.objects.get(zero)->scheduled && get(g, zero)->spawned == 1);
        CHECK(get(g, endless)->m_dwRespawnCount == 0xfffffffeu && g.objects.get(endless)->scheduled);
        CHECK(g.step(.5) && get(g, twice)->m_dwRespawnCount == 0 && !g.objects.get(twice)->scheduled);
        CHECK(get(g, twice)->spawned == 2 && get(g, endless)->spawned == 2 && get(g, zero)->spawned == 1);
        // START rearms even after exhaustion; the next update spawns and stops again.
        CHECK(send(g, nullptr, twice, "START") && g.step(.3) && get(g, twice)->spawned == 2);
        CHECK(g.step(.21) && get(g, twice)->spawned == 3);
        CHECK(get(g, twice)->m_dwRespawnCount == 0 && !g.objects.get(twice)->scheduled);
        // Spawned children are independent of the spawner.
        auto child = get(g, twice)->last_child;
        CHECK(child && g.objects.remove(twice) && g.step(.01) && g.objects.get(child));
        return true;
    }
    bool spawn(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto fixed = make(g, {{"Sound", std::string("sounds\\ambient\\warp1.wav")}, {"SoundRadius", 2000.0f}});
        CHECK(send(g, nullptr, fixed, "TRIGGER"));
        const auto report = g.server.spawn_reports.back();
        CHECK(report.child && report.outcome == "Created" && same(report.velocity, {0, 500, 0}));
        CHECK(same(g.objects.get(report.child)->velocity, {0, 500, 0}));
        const auto voice = get(g, fixed)->last_voice;
        const auto* v = g.server.audio.get(voice);
        CHECK(v && v->request.inner_radius == 200 && v->request.outer_radius == 2000 && v->request.priority == 0);
        CHECK(same(v->position, {10, 100, 20}) && v->request.space == SoundSpace::Spatial);
        // Random ranges (including reversed endpoints) stay within the authored interval.
        auto ranged = make(g, {{"MinVelocity", DVector{-10, 100, 30}}, {"MaxVelocity", DVector{10, 200, -30}}});
        for (int i = 0; i < 20; ++i)
        {
            CHECK(send(g, nullptr, ranged, "TRIGGER"));
            const auto vel = g.server.spawn_reports.back().velocity;
            CHECK(vel.x >= -10 && vel.x <= 10 && vel.y >= 100 && vel.y <= 200 && vel.z >= -30 && vel.z <= 30);
        }
        // Unsupported spawn classes fail visibly; rift and sound still follow the source order.
        auto bad = make(g, {{"SpawnObject", std::string("BaseClass")},
                            {"CreateRiftEffect", true},
                            {"Sound", std::string("sounds\\ambient\\missing.wav")}});
        CHECK(send(g, nullptr, bad, "TRIGGER"));
        const auto& failed = g.server.spawn_reports.back();
        CHECK(!failed.child && failed.outcome.starts_with("Failed: unsupported spawn class"));
        CHECK(failed.outcome.find("rift FX:") != std::string::npos);
        CHECK(failed.outcome.find("sound failed") != std::string::npos);
        CHECK(get(g, bad)->rift_unavailable == 1 && get(g, bad)->spawn_failures == 1 && !get(g, bad)->error.empty());
        // Properties in the spawn string are validated before creation.
        auto props = make(g, {{"SpawnObject", std::string("HealthPU Bogus 1")}});
        CHECK(send(g, nullptr, props, "TRIGGER") && !g.server.spawn_reports.back().child);
        for (const char* type :
             {"Behemoth", "CalebAI", "DeathShroud", "DrudgeLord", "DrudgePriest", "EnhancedGideon", "GabriellaREV",
              "IshmaelAI", "IshmaelREV", "Naga", "OpheliaAI", "OpheliaREV", "ShikariAI", "SoulDrudge", "UndeadGideon",
              "ZealotAI"})
            CHECK(g.server.ValidateSpawnObject(type));
        CHECK(g.server.ValidateSpawnObject("GabriellaAI Name GabriellaAI;AIState PASSIVE;TriggerDestroyObject TRUE;"));
        CHECK(!g.server.ValidateSpawnObject("GabriellaAI TriggerDestroyObject invalid;"));
        CHECK(g.unload_world() && !g.server.string_count());
        return true;
    }
    bool private_world(const char* path)
    {
        GameSession g;
        CHECK(g.initialize());
        Config c;
        c.data_dir = path;
        std::string e;
        CHECK(g.resources.mount_game(c, e));
        CHECK(g.request_world("05_cdmb", e));
        unsigned count = 0;
        for (std::size_t i = 0; i < g.entries.size(); ++i)
        {
            if (g.world.objects[i].class_name != "Spawner")
                continue;
            CHECK(g.entries[i].state == EntryState::Created);
            const auto* r = g.objects.get(g.entries[i].object);
            const auto* s = r ? dynamic_cast<const Spawner*>(r->game.get()) : nullptr;
            CHECK(s && s->m_hstrSpawnObject == "Fanatic" && same(s->m_vMinVelocity, {0, 500, 0}));
            CHECK(!s->m_bUseTriggerObjPos && !s->m_bStartActive && !s->m_bCreateRiftEffect);
            CHECK(s->m_dwRespawnCount == 0xffffffffu && !r->scheduled);
            ++count;
        }
        std::cout << "05_CDMB spawners: " << count << " created\n";
        CHECK(count == 2 && g.unload_world());
        return true;
    }
    template <class T> T* named(GameSession& g, std::string_view name)
    {
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && r->life == Life::Alive && same_name(r->state.m_Name, name))
                if (auto* object = dynamic_cast<T*>(r->game.get()))
                    return object;
        return nullptr;
    }
    bool step(GameSession& g, unsigned ticks)
    {
        for (unsigned i = 0; i < ticks; ++i)
            if (!g.step(1. / 60))
            {
                for (const auto& error : g.step_errors())
                    std::cerr << error << '\n';
                return false;
            }
        return true;
    }
    bool spawned(GameSession& g, const Spawner* s, std::string_view type)
    {
        if (s && s->spawned && !s->spawn_failures)
            if (auto* r = g.objects.get(s->last_child); r && r->life == Life::Alive && r->type->name == type)
                if (const auto* actor = dynamic_cast<const AI_Mgr*>(r->game.get()); actor && actor->RuntimeReady())
                    return true;
        std::cerr << type << " spawn: " << (s ? s->error : "missing spawner") << '\n';
        for (const auto& error : g.objects.diagnostics())
            std::cerr << error << '\n';
        return false;
    }
    bool cathedral(GameSession& g, HOBJECT player)
    {
        auto* door_trigger = named<Trigger>(g, "maindoor1trigger1");
        auto* unlocker = named<Trigger>(g, "maindoor1trigger1unlocker");
        auto* wave_trigger = named<Trigger>(g, "Trigger1");
        CHECK(door_trigger && door_trigger->locked && unlocker && wave_trigger && wave_trigger->activations == 1);
        // The player touches Trigger1 at entry. Run its first three delayed spawns.
        for (const auto* name : {"spawn100", "spawn5", "spawn101"})
        {
            auto* spawner = named<Spawner>(g, name);
            CHECK(spawner);
            for (unsigned i = 0; i < 420 && !spawner->spawn_attempts; ++i)
                CHECK(step(g, 1));
            CHECK(spawned(g, spawner, "ShikariAI"));
            CHECK(step(g, 15) && send(g, player, spawner->last_child, "KILL"));
        }
        CHECK(unlocker->activations == 1 && !door_trigger->locked);
        CHECK(send(g, player, door_trigger->m_hObject, "TRIGGER"));
        auto* naga = named<Spawner>(g, "nagaspawn");
        auto* gabriella = named<Spawner>(g, "gabspawn");
        auto* endrelay = named<Trigger>(g, "endrelay");
        const auto* wall = named<BaseClass>(g, "gabbywall");
        CHECK(naga && gabriella && endrelay && !endrelay->activations && wall);
        const auto exit_wall = wall->m_hObject;
        std::vector<HOBJECT> guards;
        for (auto id : g.objects.snapshot())
            if (const auto* r = g.objects.get(id); r && r->life == Life::Alive && same_name(r->state.m_Name, "ShikariAI2"))
                guards.push_back(r->handle);
        CHECK(guards.size() == 2);
        for (auto guard : guards)
            CHECK(send(g, player, guard, "KILL"));
        CHECK(spawned(g, naga, "Naga"));
        CHECK(step(g, 60) && send(g, player, naga->last_child, "KILL"));
        CHECK(endrelay->activations == 1 && spawned(g, gabriella, "GabriellaAI"));
        const auto* actor = named<AI_Mgr>(g, "GabriellaAI");
        CHECK(actor && actor->m_hObject == gabriella->last_child && !actor->m_damage.trigger_only);
        CHECK(step(g, 300) && !g.server.IsObjectAlive(exit_wall));
        for (const auto* name : {"maindoor1left", "maindoor1right"})
        {
            const auto* door = named<Door>(g, name);
            CHECK(door && door->state != Door::Closed);
        }
        return true;
    }
    bool ancient_city(GameSession& g, HOBJECT player)
    {
        auto* enhanced = named<Spawner>(g, "Spawner-Gideon");
        auto* undead = named<Spawner>(g, "Spawner-UndeadGideon");
        auto* soul = named<Spawner>(g, "Spawner-SoulMan");
        auto* death_relay = named<Trigger>(g, "Trigger-Gideon'sDeath");
        auto* left = named<Door>(g, "Door-ShikariDenLeft");
        auto* right = named<Door>(g, "Door-ShikariDenRight");
        CHECK(enhanced && undead && soul && death_relay && left && right);
        CHECK(left->state == Door::Closed && right->state == Door::Closed);
        auto* enhanced_trigger = named<Trigger>(g, "Trigger-Gideon'sSpawner");
        auto* undead_trigger = named<Trigger>(g, "Trigger-UndeadGideon'sSpawner");
        auto* soul_trigger = named<Trigger>(g, "Trigger-SoulMan");
        auto* enhanced_keyframer = named<BaseClass>(g, "KeyFramer-Camera-GidA");
        auto* undead_keyframer = named<BaseClass>(g, "KeyFramer-Camera-GidC");
        CHECK(enhanced_trigger && undead_trigger && soul_trigger && enhanced_keyframer && undead_keyframer);
        // Replay the keyframe messages that enter the encounters.
        CHECK(send(g, enhanced_keyframer->m_hObject, enhanced_trigger->m_hObject, "TRIGGER") &&
              spawned(g, enhanced, "EnhancedGideon"));
        CHECK(send(g, player, soul_trigger->m_hObject, "TRIGGER") && spawned(g, soul, "SoulDrudge"));
        CHECK(send(g, undead_keyframer->m_hObject, undead_trigger->m_hObject, "TRIGGER") &&
              spawned(g, undead, "UndeadGideon"));
        CHECK(step(g, 60) && send(g, player, undead->last_child, "KILL"));
        CHECK(death_relay->activations == 1 && step(g, 990));
        CHECK(left->state != Door::Closed && right->state != Door::Closed);
        return true;
    }
    bool arena(GameSession& g, HOBJECT player)
    {
        auto* ishmael = named<Spawner>(g, "Spawner-Ishmael");
        auto* ophelia = named<Spawner>(g, "Spawner-Ophelia");
        auto* gabriella = named<Spawner>(g, "Spawner-Gabriella");
        auto* portal = named<Trigger>(g, "BloodPortalTrigger");
        auto* cinematic = named<Trigger>(g, "Trigger-ChosenCinematic3");
        CHECK(ishmael && ophelia && gabriella && portal && cinematic && !portal->activations);
        // Enter at the end of the opening cinematic. Only the first spawner is
        // triggered manually. The original death messages run the rest.
        CHECK(send(g, cinematic->m_hObject, ishmael->m_hObject, "TRIGGER") && spawned(g, ishmael, "IshmaelREV"));
        CHECK(step(g, 30) && send(g, player, ishmael->last_child, "KILL"));
        CHECK(spawned(g, ophelia, "OpheliaREV") && step(g, 30) && send(g, player, ophelia->last_child, "KILL"));
        CHECK(spawned(g, gabriella, "GabriellaREV") && step(g, 30) && send(g, player, gabriella->last_child, "KILL"));
        CHECK(portal->activations == 1 && step(g, 210));
        for (const auto* name : {"BloodPortalDoor-UpperRight", "BloodPortalDoor-UpperLeft",
                                 "BloodPortalDoor-LowerRight", "BloodPortalDoor-LowerLeft"})
        {
            const auto* door = named<Door>(g, name);
            CHECK(door && door->state != Door::Closed);
        }
        return true;
    }
    bool private_progression(const char* path, std::string_view mode)
    {
        const auto world = mode == "cathedral" ? "07_cathedral" : mode == "ancient_city" ? "24_ancientcity" : "24b_arena";
        Config config;
        config.data_dir = path;
        auto g = std::make_unique<GameSession>();
        std::string error;
        if (!g->initialize() || !g->resources.mount_game(config, error) ||
            !GameSession::start_new_game(g, world, 1, 0, error))
        {
            std::cerr << world << ": " << error << '\n';
            return false;
        }
        const auto* player = g->server_shell.Player(g->local_client);
        CHECK(player);
        const auto handle = player->m_hObject;
        return mode == "cathedral" ? cathedral(*g, handle)
               : mode == "ancient_city" ? ancient_city(*g, handle)
                                         : arena(*g, handle);
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string mode = argv[1];
    if (mode == "cathedral" || mode == "ancient_city" || mode == "arena")
        return private_progression(argv[2], mode) ? 0 : 1;
    return (mode == "properties" ? properties(argv[2])
            : mode == "commands" ? commands(argv[2])
            : mode == "respawn"  ? respawn(argv[2])
            : mode == "spawn"    ? spawn(argv[2])
                                 : mode == "private" && private_world(argv[2]))
               ? 0
               : 1;
}
