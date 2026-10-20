#include "audio_fixture.h"
#include "brush_fixture.h"
#include "engine/config.h"
#include "engine/save_objects.h"
#include "game/original/CameraObj.h"
#include "game/original/CinematicTrigger.h"
#include "game/original/ConversationTrigger.h"
#include "game/original/Door.h"
#include "game/original/ExitTrigger.h"
#include "game/original/KeyFramer.h"
#include "game/original/ObjectivesTrigger.h"
#include "game/original/Spawner.h"
#include "game/original/Trigger.h"
#include "player_fixture.h"
#include "save_ai_fixture.h"
#include <bit>
#include <functional>
#include <iostream>
#include <map>
#include <set>

// Original Save/Load bodies of the world logic (save_load_contracts lot 7): byte round trips of
// each class in the states a player can save, the re-save byte identity, and identical
// continuation of the saved and the restored worlds from the restored clock.
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
    constexpr const char* current = "Save\\Current\\Current.sav";
    constexpr DDWORD save_flags = SAVEOBJECTS_SAVEGAMECONSOLE | SAVEOBJECTS_SAVEPORTALS;

    // The player fixture with pickup models (spawner children) and the audio fixture's WAVs.
    bool setup(GameSession& g, std::vector<WorldModel> models = {})
    {
        CHECK(test_player::setup(g, root, false, true, true));
        test_player::Directory d;
        d.files["cue.wav"] = test_audio::wave();
        d.files["long.wav"] = test_audio::wave(1, 16, 8000, 8000);
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = test_player::write_dir(w, d);
        w.patch(131, offset);
        w.patch(135, size);
        {
            std::ofstream out(root / "sound.rez", std::ios::binary);
            out.write(reinterpret_cast<const char*>(w.b.data()), static_cast<std::streamsize>(w.b.size()));
            CHECK(out.good());
        }
        std::string error;
        CHECK(g.resources.mount(root / "sound.rez", error));
        g.world.models = std::move(models);
        return true;
    }
    HOBJECT create(GameSession& g, const char* type, Properties props)
    {
        return g.objects.create(g.objects.find_class(type), {}, Creation::World, props);
    }
    template <class T> T* get(GameSession& g, HOBJECT h)
    {
        auto* r = g.objects.get(h);
        return r ? dynamic_cast<T*>(r->game.get()) : nullptr;
    }
    HOBJECT named(GameSession& g, std::string_view name)
    {
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && r->life == Life::Alive && name == r->state.m_Name)
                return r->handle;
        return nullptr;
    }
    template <class T> T* named(GameSession& g, std::string_view name)
    {
        return get<T>(g, named(g, name));
    }
    bool linked(GameSession& g, HOBJECT owner, HOBJECT target)
    {
        const auto& links = g.objects.links();
        return std::find(links.begin(), links.end(), std::pair{owner, target}) != links.end();
    }
    // A trigger message; the original Trigger validates its sender, so `from` names one.
    bool command(GameSession& g, HOBJECT h, const char* text, HOBJECT from = nullptr)
    {
        auto str = g.server.CreateString(text);
        auto msg = g.server.StartMessageToObject(from ? g.server.HandleToObject(from) : nullptr, h, 1000);
        CHECK(str && msg && g.server.WriteToMessageHString(msg, str) == DE_OK);
        CHECK(g.server.EndMessage(msg) == DE_OK);
        g.server.FreeString(str);
        return true;
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
    // The original shell's list: every live object with USRFLG_SAVEABLE. `supported` drops the
    // classes of other lots (a spawner's pickups), which the shell would refuse by name.
    std::vector<HOBJECT> saveable(GameSession& g, bool supported = false)
    {
        std::vector<HOBJECT> list;
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && r->life == Life::Alive && (r->user_flags & USRFLG_SAVEABLE) &&
                                             (!supported || save_support(r->type) == SaveSupport::Original))
                list.push_back(r->handle);
        return list;
    }
    bool save(GameSession& g, SaveImage& image, bool supported = false, std::string* error = nullptr)
    {
        SaveObjectsReport report;
        const bool ok = g.server.save_objects(current, saveable(g, supported), 0, save_flags, report);
        if (error)
            *error = report.error;
        if (!ok)
        {
            if (!error)
                std::cerr << "save: " << report.error << '\n';
            return false;
        }
        image = *g.server.save_store.find(SaveStoreSlot::Current);
        return true;
    }
    bool restore(GameSession& g, const SaveImage& image, RestoreObjectsReport& report)
    {
        g.server.save_store.put(SaveStoreSlot::Current, image);
        return g.server.restore_objects(current, 0, RESTOREOBJECTS_RESTORETIME, report);
    }
    bool decode(const SaveImage& image, std::vector<SaveEngineRecord>& records)
    {
        SaveError error;
        return decode_save_records(image.objects, records, error);
    }
    // The original rewrites listed by save_load_contracts lot 7: CameraObj's PostPropRead forces
    // its flags and Load's SetActive its schedule (0.01 s active, none inactive) and a timed
    // camera's full duration; lot 5's player rewrites; lot 11's NPC m_fLastUpdate and CCorpse flags.
    void listed_rewrites(std::vector<SaveEngineRecord>& records)
    {
        for (auto& r : records)
            if (save_ai::is_ai(r.class_name))
            {
                if (const auto at = save_ai::last_update_offset(r.class_name, r.payload); at != std::size_t(-1))
                    std::fill_n(r.payload.begin() + std::ptrdiff_t(at), 4, std::uint8_t(0));
            }
            else if (r.class_name == "CCorpse")
                r.flags = FLAG_VISIBLE | FLAG_RAYHIT | FLAG_GRAVITY | FLAG_MODELKEYS | FLAG_REMOVEIFOUTSIDE;
            else if (r.class_name == "CameraObj")
            {
                r.update_remaining = 0;
                r.flags &= FLAG_FORCECLIENTUPDATE | FLAG_FULLPOSITIONRES | FLAG_GOTHRUWORLD;
                if (r.payload.size() >= 13)
                    std::fill_n(r.payload.begin() + 9, 4, 0); // m_fDeactivateTime - GetTime().
            }
            else if (r.class_name == "CPlayerObj")
            {
                r.flags &= ~FLAG_SHADOW;
                r.velocity = {};
                r.hidden_nodes.clear();
                r.payload.clear();
            }
            else if (r.class_name == "WorldModelDebris")
                r.flags &= ~(FLAG_FULLPOSITIONRES | FLAG_MODELGOURAUDSHADE);
    }
    // Relative float clocks may restore one ulp away (lot 8): accept exactly that.
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
    bool same_image(const SaveImage& a, const SaveImage& b, bool clock = true, bool clock_ulp = false)
    {
        std::vector<SaveEngineRecord> x, y;
        CHECK(decode(a, x) && decode(b, y));
        listed_rewrites(x);
        listed_rewrites(y);
        if (x.size() != y.size())
        {
            std::cerr << "records: " << x.size() << " vs " << y.size() << '\n';
            return false;
        }
        for (std::size_t i = 0; i < x.size(); ++i)
        {
            if (clock_ulp && x[i].payload != y[i].payload && within_clock_ulp(x[i].payload, y[i].payload))
                y[i].payload = x[i].payload;
            if (!(x[i] == y[i]))
            {
                std::cerr << "record " << i << " (" << x[i].class_name << ") differs";
                if (x[i].payload != y[i].payload)
                    for (std::size_t k = 0; k < std::min(x[i].payload.size(), y[i].payload.size()); ++k)
                        if (x[i].payload[k] != y[i].payload[k])
                        {
                            std::cerr << " at payload byte " << k;
                            break;
                        }
                if (x[i].update_remaining != y[i].update_remaining)
                    std::cerr << " delay " << x[i].update_remaining << " vs " << y[i].update_remaining;
                if (x[i].flags != y[i].flags || x[i].user_flags != y[i].user_flags)
                    std::cerr << " flags " << x[i].flags << " vs " << y[i].flags;
                std::cerr << '\n';
                return false;
            }
        }
        CHECK(a.links == b.links && a.attachments == b.attachments);
        CHECK(a.classes == b.classes && a.records == b.records);
        if (clock)
            CHECK(a.clock == b.clock && a.console == b.console);
        return true;
    }

    // One round trip, as lot 8's: `build` drives the saved world to the state under test; the
    // restored world must save the same bytes again, then match the saved world for `after`
    // steps unless `diverges` (an original restore quirk under test).
    struct Trip
    {
        std::vector<WorldModel> models;
        std::function<bool(GameSession&)> build;
        std::function<bool(GameSession&, const RestoreObjectsReport&)> check;
        std::function<bool(GameSession&)> before_restore; // Always-load objects of a restore-mode world.
        std::function<bool(GameSession&)> after_steps;    // Checks on the restored world afterwards.
        unsigned after = 60;
        bool supported = false, diverges = false;
    };
    bool round_trip(const char* label, const Trip& trip)
    {
        SaveImage saved;
        std::vector<SaveImage> expected;
        {
            GameSession a;
            CHECK(setup(a, trip.models));
            CHECK(trip.build(a));
            CHECK(save(a, saved, trip.supported));
            for (unsigned i = 0; i < trip.after && !trip.diverges; ++i)
            {
                CHECK(steps(a, 1));
                CHECK(save(a, expected.emplace_back(), trip.supported));
            }
        }
        GameSession b;
        CHECK(setup(b, trip.models));
        if (trip.before_restore)
        {
            std::string error;
            b.server.save_store.put(SaveStoreSlot::Current, saved);
            CHECK(b.server.restore_image_state(SaveStoreSlot::Current, true, error));
            CHECK(trip.before_restore(b));
        }
        RestoreObjectsReport report;
        if (!restore(b, saved, report))
        {
            std::cerr << label << " restore: " << report.error << '\n';
            return false;
        }
        CHECK(report.records == saved.records);
        if (trip.check)
            CHECK(trip.check(b, report));
        SaveImage again;
        CHECK(save(b, again, trip.supported));
        if (!same_image(saved, again, false))
        {
            std::cerr << label << ": the restored world saves different bytes\n";
            return false;
        }
        for (unsigned i = 0; i < trip.after; ++i)
        {
            SaveImage y;
            CHECK(steps(b, 1) && save(b, y, trip.supported));
            if (!trip.diverges && !same_image(expected[i], y, true, true))
            {
                std::cerr << label << ": the worlds diverge after " << i + 1 << " steps\n";
                return false;
            }
        }
        if (trip.after_steps)
            CHECK(trip.after_steps(b));
        std::cout << label << ": " << saved.records << " records, " << saved.objects.size() << " OBJS bytes, "
                  << trip.after << (trip.diverges ? " steps\n" : " identical steps\n");
        return true;
    }

    // Triggers: every class of the family, armed with nondefault properties, then waiting on
    // a delayed send, locked, toggled and resetting, and a conversation mid-sequence.
    HOBJECT relay(GameSession& g, const char* name, Properties props = {})
    {
        props.try_emplace("Name", std::string(name));
        props.try_emplace("ResetTime", .3f);
        return create(g, "Trigger", props);
    }
    bool triggers()
    {
        auto family = [](GameSession& g)
        {
            CHECK(relay(g, "source")); // The sender the original sender test accepts (relay).
            CHECK(relay(g, "sink", {{"ActivationCount", std::int32_t(2)}, {"ResetTime", .2f}}));
            CHECK(create(g, "Trigger",
                         {{"Name", std::string("delayed")},
                          {"Dims", DVector{8, 16, 24}},
                          {"ActivationSound", std::string("cue.wav")},
                          {"SoundRadius", 300.0f},
                          {"TargetName1", std::string("sink")},
                          {"MessageName1", std::string("TRIGGER")},
                          {"MessageDelay", .5f},
                          {"TargetName2", std::string("sink")},
                          {"MessageName2", std::string("TRIGGER")},
                          {"MessageDelay2", 1.25f},
                          {"TargetName3", std::string("nobody")},
                          {"MessageName3", std::string("hello")},
                          {"ResetTime", 2.0f},
                          {"NamedObjectActivate", true},
                          {"ActivationObjectName", std::string("source")},
                          {"UnlockKeyName", std::string("Gold Key")},
                          {"LockedSound", std::string("long.wav")}}));
            CHECK(create(g, "Trigger", {{"Name", std::string("locked")}, {"Locked", true}}));
            CHECK(create(g, "BigTrigger", {{"Name", std::string("big")}, {"Dims", DVector{100, 50, 100}}}));
            CHECK(create(g, "ToggleTrigger",
                         {{"Name", std::string("toggle")},
                          {"TargetName1", std::string("sink")},
                          {"MessageName1", std::string("TRIGGER")},
                          {"OffMessageName1", std::string("RESET")},
                          {"ResetTime", .4f}}));
            CHECK(create(g, "ExitTrigger",
                         {{"Name", std::string("exit")},
                          {"NextWorld", std::string("worlds\\next")},
                          {"StartPointName", std::string("north")},
                          {"SubWorld", true}}));
            CHECK(create(g, "ObjectivesTrigger",
                         {{"Name", std::string("objectives")},
                          {"ResourceNum", std::int32_t(12)},
                          {"TitleToDisplay", std::string("Find the key")},
                          {"TextToDisplay", std::string("It is|somewhere")}}));
            CHECK(create(g, "ConversationTrigger",
                         {{"Name", std::string("talk")},
                          {"WaveFile", std::string("sounds\\4Prophet1.wav")},
                          {"WaveFile2", std::string("sounds\\1ACaleb1.wav")},
                          {"CharacterType", std::int32_t(3)},
                          {"LoopToFirstMessage", true},
                          {"TriggerRelayActivate", true}}));
            return true;
        };
        struct Case
        {
            const char* label;
            std::function<bool(GameSession&)> drive;
        };
        const std::vector<Case> cases{
            {"triggers armed", [](GameSession& g) { return steps(g, 2); }},
            {"trigger waiting on a delay",
             [](GameSession& g)
             {
                 CHECK(command(g, named(g, "delayed"), "TRIGGER", named(g, "source")) && steps(g, 30));
                 auto* t = named<Trigger>(g, "delayed");
                 CHECK(t && !t->active && !t->slots[0].pending && t->slots[1].pending); // 0.6 s of 1.25 s.
                 return true;
             }},
            {"trigger locked",
             [](GameSession& g)
             {
                 CHECK(command(g, named(g, "delayed"), "LOCK") && steps(g, 1));
                 CHECK(named<Trigger>(g, "delayed")->locked);
                 return true;
             }},
            {"toggle on and resetting",
             [](GameSession& g)
             {
                 CHECK(command(g, named(g, "toggle"), "TRIGGER", named(g, "source")) && steps(g, 5));
                 auto* t = named<ToggleTrigger>(g, "toggle");
                 CHECK(t && t->on && !t->active && named<Trigger>(g, "sink")->current_activation == 1);
                 return true;
             }},
            {"conversation mid-sequence",
             [](GameSession& g)
             {
                 CHECK(command(g, named(g, "talk"), "TRIGGER", named(g, "source")) && steps(g, 1));
                 CHECK(named<ConversationTrigger>(g, "talk")->current == 1);
                 return true;
             }},
        };
        for (const auto& c : cases)
        {
            Trip trip;
            trip.build = [&](GameSession& g) { return family(g) && c.drive(g); };
            trip.check = [](GameSession& b, const RestoreObjectsReport&)
            {
                auto* d = named<Trigger>(b, "delayed");
                CHECK(d && d->dims.y == 16 && d->activation_sound == "cue.wav" && d->sound_radius == 300 &&
                      d->reset_time == 2 && d->max_delay == 1.25f && d->named_activate &&
                      d->activation_name == "source" && d->key_name == "Gold Key" && d->locked_sound == "long.wav");
                CHECK(d->slots[0].target == "sink" && d->slots[1].delay == 1.25f && d->slots[2].message == "hello");
                CHECK(named<Trigger>(b, "locked")->locked && named<Trigger>(b, "sink")->activation_count == 2);
                CHECK(named<BigTrigger>(b, "big")->dims.x == 100);
                CHECK(named<ToggleTrigger>(b, "toggle")->slots[0].off_message == "RESET");
                auto* e = named<ExitTrigger>(b, "exit");
                CHECK(e && e->exit.next_world == "worlds\\next" && e->start_point == "north" &&
                      e->exit.type == WorldExitType::Subworld && !e->touch_activate);
                auto* o = named<ObjectivesTrigger>(b, "objectives");
                CHECK(o && o->objectives.resource == 12 && o->objectives.title == "Find the key");
                CHECK(o->sound.empty()); // Original loss: m_hstrSound is not saved.
                auto* t = named<ConversationTrigger>(b, "talk");
                CHECK(t && t->count == 2 && t->waves[1] == "sounds\\1ACaleb1.wav" && t->character == 3 && t->loop);
                // Restored triggers keep their saved touch flags and dims, not a new InitialUpdate.
                CHECK(b.server.GetObjectFlags(d->m_hObject) & FLAG_TOUCH_NOTIFY);
                return true;
            };
            trip.after_steps = [](GameSession& b)
            {
                // The delayed sends and resets resume from the restored clock.
                CHECK(named<Trigger>(b, "delayed")->slots[1].pending == false);
                return true;
            };
            CHECK(round_trip(c.label, trip));
        }
        return true;
    }

    // A self-triggered door saves its spawned Trigger (lot 8 waited for this lot); the restored
    // trigger keeps opening the restored door, and the door owns it again.
    bool doors()
    {
        Trip trip;
        trip.models = {test_brush::box("door", {-10, 10, -10}, {10, 30, 10})};
        trip.models[0].translation = {5, 20, 0};
        trip.build = [](GameSession& g)
        {
            CHECK(relay(g, "source"));
            auto d = create(g, "Door",
                            {{"Name", std::string("door")},
                             {"Speed", 100.0f},
                             {"MoveDist", 40.0f},
                             {"MoveDir", DVector{1, 0, 0}},
                             {"SelfTrigger", true},
                             {"TriggerDims", DVector{20, 20, 20}},
                             {"Locked", true},
                             {"UnlockKeyName", std::string("Red Key")}});
            CHECK(d && get<Door>(g, d)->trigger && steps(g, 2));
            return true;
        };
        trip.check = [](GameSession& b, const RestoreObjectsReport&)
        {
            auto* d = named<Door>(b, "door");
            CHECK(d && d->self_trigger && d->trigger);
            auto* t = get<Trigger>(b, d->trigger);
            CHECK(t && t->owner_door == d->m_hObject && t->slots[0].target == "door" &&
                  t->slots[0].message == "TRIGGER" && t->locked && t->key_name == "Red Key" && t->dims.x == 20);
            CHECK(linked(b, d->m_hObject, d->trigger));
            return true;
        };
        trip.after_steps = [](GameSession& b)
        {
            auto* d = named<Door>(b, "door");
            auto* t = get<Trigger>(b, d->trigger);
            // A relay is accepted while locked (the lock gates players); the door opens.
            CHECK(command(b, t->m_hObject, "TRIGGER", named(b, "source")) && steps(b, 3));
            CHECK(d->state == Door::Opening || d->state == Door::Open);
            // Removing the door still removes its trigger.
            const auto trigger = d->trigger;
            CHECK(b.objects.remove(d->m_hObject) && b.objects.finish_frame() && !b.server.IsObjectAlive(trigger));
            return true;
        };
        CHECK(round_trip("self-triggered door", trip));
        return true;
    }

    // Spawners: a running spawner keeps spawning after a load (decision-4 departure; the
    // original reset its schedule from the constructor defaults), a stopped one stays stopped.
    bool spawners()
    {
        for (bool running : {true, false})
        {
            Trip trip;
            trip.supported = true; // Pickups are lot 9's.
            trip.build = [running](GameSession& g)
            {
                auto h = create(g, "Spawner",
                                {{"Name", std::string("spawner")},
                                 {"Pos", DVector{2000, 2000, 2000}}, // Clear of the fixture geometry.
                                 {"SpawnObject", std::string("HealthPU")},
                                 {"StartActive", running},
                                 {"RespawnRate", .35f},
                                 {"RespawnCount", std::int32_t(20)},
                                 {"MinVelocity", DVector{-10, 50, -10}},
                                 {"MaxVelocity", DVector{10, 80, 10}},
                                 {"Sound", std::string("cue.wav")},
                                 {"SoundRadius", 250.0f}});
                CHECK(h && steps(g, 20));
                auto* s = get<Spawner>(g, h);
                CHECK(s && (running ? s->spawned == 1 && s->m_dwRespawnCount == 19 : s->spawned == 0));
                return true;
            };
            trip.check = [running](GameSession& b, const RestoreObjectsReport&)
            {
                auto* s = named<Spawner>(b, "spawner");
                CHECK(s && s->m_hstrSpawnObject == "HealthPU" && s->m_fRespawnRate == .35f &&
                      s->m_bStartActive == running && s->m_vMaxVelocity.y == 80 && s->m_hstrSound == "cue.wav");
                CHECK(b.objects.get(s->m_hObject)->scheduled == running);
                return true;
            };
            trip.after_steps = [running](GameSession& b)
            {
                auto* s = named<Spawner>(b, "spawner");
                CHECK(running ? s->spawned == 3 && s->m_dwRespawnCount == 16 : s->spawned == 0);
                return true;
            };
            CHECK(round_trip(running ? "running spawner" : "stopped spawner", trip));
        }
        return true;
    }

    // KeyFramer: consumed keys come back with the restore-mode world but stay inert; a path saved
    // mid-segment and a looping path continue exactly; one saved before its first update collects them.
    bool keyframers()
    {
        auto keys = [](GameSession& g)
        {
            const DVector points[]{{0, 100, 0}, {100, 100, 0}, {100, 100, 200}};
            for (unsigned i = 0; i < 3; ++i)
            {
                Properties props{{"Name", "k" + std::to_string(i)},
                                 {"Pos", points[i]},
                                 {"TimeStamp", i ? 1.0f : 0.0f},
                                 {"SoundName", std::string(i == 1 ? "long.wav" : "")},
                                 {"SoundRadius", 400.0f},
                                 {"LoopSound", true}};
                if (i == 2)
                {
                    props.emplace("MessageTarget", std::string("sink"));
                    props.emplace("MessageName", std::string("TRIGGER"));
                    props.emplace("BPrintMessage", std::string("arrived"));
                }
                CHECK(create(g, "Key", props));
            }
            return true;
        };
        struct Case
        {
            const char* label;
            bool looping;
            unsigned before, riders = 2;
        };
        // Decision-4 departure: offsets past the key count travel in a port tail (SubwayA's
        // twelve tunnel lights on a two-key path lost theirs in the original).
        for (const auto& c : {Case{"keyframer mid-path", false, 30}, Case{"keyframer looping", true, 130},
                              Case{"keyframer before its first update", false, 0},
                              Case{"keyframer with more targets than keys", true, 60, 5}})
        {
            Trip trip;
            trip.build = [&](GameSession& g)
            {
                CHECK(keys(g) && relay(g, "sink"));
                // Saved targets: cameras moved by the path, each at its own offset.
                for (unsigned i = 0; i < c.riders; ++i)
                    CHECK(create(g, "CameraObj",
                                 {{"Name", std::string("rider")}, {"Pos", DVector{10, 100 + 20.0f * float(i), 0}}}));
                CHECK(create(g, "KeyFramer",
                             {{"Name", std::string("path")},
                              {"ObjectName", std::string("rider")},
                              {"BaseKeyName", std::string("k")},
                              {"StartActive", true},
                              {"Looping", c.looping}}));
                if (c.before)
                    CHECK(steps(g, c.before));
                auto* f = named<KeyFramer>(g, "path");
                CHECK(f && f->initialized == (c.before > 0) && (c.before == 0 || f->targets.size() == c.riders));
                CHECK(c.before == 0 || (f->active && f->time > 0 && !named(g, "k0")));
                return true;
            };
            trip.before_restore = keys; // The always-load Keys of the restore-mode world.
            trip.check = [&](GameSession& b, const RestoreObjectsReport&)
            {
                auto* f = named<KeyFramer>(b, "path");
                CHECK(f && f->object_name == "rider" && f->base_key_name == "k" && f->looping == c.looping);
                CHECK(f->keys.size() == (c.before ? 3u : 0u) && !f->sound_voice);
                if (c.before)
                {
                    CHECK(f->keys[1].sound == "long.wav" && f->keys[1].loop_sound && f->keys[2].message == "TRIGGER");
                    CHECK(f->keys[2].print == "arrived" && f->keys[2].name == "k2" && f->keys[2].time == 2);
                    CHECK(f->targets.empty()); // Rebuilt by name at the next update.
                }
                return true;
            };
            trip.after_steps = [&](GameSession& b)
            {
                auto* f = named<KeyFramer>(b, "path");
                CHECK(f->targets.size() == c.riders && f->initialized);
                // Every target keeps its own offset (the list is in the engine's lookup order).
                std::set<float> offsets;
                for (const auto& t : f->targets)
                    offsets.insert(t.offset.y);
                for (unsigned i = 0; i < c.riders && c.before; ++i)
                    CHECK(offsets.contains(20.0f * float(i)));
                // A restored controller never collects its keys again: they stay in the world.
                // One that had not run its first update collects them now.
                CHECK(bool(named(b, "k0")) == (c.before > 0));
                return true;
            };
            trip.after = 80;
            CHECK(round_trip(c.label, trip));
        }
        return true;
    }

    // Cameras: an active listener camera restores as the client's camera and the active camera;
    // a tracked target reference is restored; a timed camera restarts its duration (original
    // Load quirk, listed rewrite).
    bool cameras()
    {
        Trip trip;
        trip.build = [](GameSession& g)
        {
            CHECK(relay(g, "look", {{"Pos", DVector{0, 0, 300}}}));
            auto idle = create(g, "CameraObj", {{"Name", std::string("idle")}, {"Pos", DVector{0, 0, 0}}});
            auto active = create(g, "CameraObj",
                                 {{"Name", std::string("shot")},
                                  {"Pos", DVector{50, 20, 0}},
                                  {"Type", std::int32_t(1)},
                                  {"AllowPlayerMovement", true},
                                  {"HidePlayer", true}});
            CHECK(idle && active && command(g, active, "LINK look") && command(g, active, "ON") && steps(g, 3));
            CHECK(g_hActiveCamera == active && g.server.GetCameraView().object == active);
            return true;
        };
        trip.check = [](GameSession& b, const RestoreObjectsReport&)
        {
            auto* c = named<CameraObj>(b, "shot");
            CHECK(c && c->IsActive() && c->AllowPlayerMovement() && c->m_bHidePlayer && c->m_nType == 1);
            CHECK(c->GetLinkObject() == named(b, "look") && linked(b, c->m_hObject, named(b, "look")));
            CHECK(g_hActiveCamera == c->m_hObject && b.server.camera_effects.size() == 2);
            const auto view = b.server.GetCameraView();
            CHECK(view.object == c->m_hObject && view.settings.type == 1 && view.settings.hide_player);
            CHECK(!named<CameraObj>(b, "idle")->IsActive() && CameraObj::m_dwNumCameras == 2);
            return true;
        };
        CHECK(round_trip("active camera", trip));
        // A timed camera: the original Load restarts the full ActiveTime.
        SaveImage image;
        float remaining = 0;
        {
            GameSession a;
            CHECK(setup(a));
            auto h = create(a, "CameraObj", {{"Name", std::string("timed")}, {"ActiveTime", 2.0f}});
            CHECK(h && command(a, h, "ON") && steps(a, 50) && get<CameraObj>(a, h)->IsActive());
            remaining = get<CameraObj>(a, h)->m_fDeactivateTime - a.server.GetTime();
            CHECK(remaining < 1.01f && save(a, image));
        }
        GameSession b;
        CHECK(setup(b));
        RestoreObjectsReport report;
        CHECK(restore(b, image, report));
        auto* c = named<CameraObj>(b, "timed");
        CHECK(c && c->IsActive() && c->m_fDeactivateTime - b.server.GetTime() == 2.0f && remaining < 2);
        CHECK(steps(b, 60) && c->IsActive() && steps(b, 45) && !c->IsActive() && !g_hActiveCamera);
        std::cout << "timed camera: " << remaining << " s left at the save, 2 s after the load\n";
        return true;
    }

    // Cinematics: a trigger saved between cues resumes its delay exactly; one saved while a cue
    // plays restarts that cue and sends its message again (original restore behavior).
    bool cinematics()
    {
        auto build = [](GameSession& g, unsigned before)
        {
            // The cinematic is not a Trigger: the sink accepts it as an ordinary object.
            CHECK(relay(g, "sink", {{"ActivationCount", std::int32_t(5)}, {"ObjectActivate", true}}));
            auto h = create(g, "CinematicTrigger",
                            {{"Name", std::string("scene")},
                             {"Delay1", .5f},
                             {"Sound1", std::string("cue.wav")},
                             {"StartSoundTriggerTarget1", std::string("sink")},
                             {"StartSoundTriggerMsg1", std::string("TRIGGER")},
                             {"Delay2", .3f},
                             {"Sound2", std::string("long.wav")},
                             {"WhoPlaysSound2", std::string("sink")},
                             {"StartSoundTriggerTarget2", std::string("sink")},
                             {"StartSoundTriggerMsg2", std::string("TRIGGER")}});
            CHECK(h && command(g, h, "TRIGGER") && steps(g, before));
            return true;
        };
        {
            Trip trip;
            trip.build = [&](GameSession& g)
            {
                CHECK(build(g, 35)); // Cue 1 (0.1 s) played; 0.2 s into cue 2's delay.
                auto* c = named<CinematicTrigger>(g, "scene");
                CHECK(c && c->current == 1 && !c->sound && named<Trigger>(g, "sink")->current_activation == 1);
                return true;
            };
            trip.check = [](GameSession& b, const RestoreObjectsReport&)
            {
                auto* c = named<CinematicTrigger>(b, "scene");
                CHECK(c && c->current == 1 && c->cues[1].speaker == "sink" && c->cues[1].sound == "long.wav");
                CHECK(b.objects.get(c->m_hObject)->scheduled);
                return true;
            };
            trip.after = 40;
            CHECK(round_trip("cinematic between cues", trip));
        }
        Trip trip;
        trip.diverges = true;
        trip.build = [&](GameSession& g)
        {
            CHECK(build(g, 52)); // Cue 2 (1 s) playing since about 0.94 s.
            auto* c = named<CinematicTrigger>(g, "scene");
            CHECK(c && c->current == 1 && c->sound && named<Trigger>(g, "sink")->current_activation == 2);
            return true;
        };
        trip.check = [](GameSession& b, const RestoreObjectsReport&)
        {
            CHECK(!named<CinematicTrigger>(b, "scene")->sound); // The voice is never saved.
            return true;
        };
        trip.after = 2;
        trip.after_steps = [](GameSession& b)
        {
            auto* c = named<CinematicTrigger>(b, "scene");
            CHECK(c->current == 1 && c->sound && c->messages == 1); // Cue 2 restarted, message sent again.
            CHECK(named<Trigger>(b, "sink")->current_activation == 3);
            return true;
        };
        CHECK(round_trip("cinematic mid-cue", trip));
        return true;
    }

    // Corrupt payloads fail the restore by class name before any member changes, and roll back.
    bool refusals()
    {
        SaveImage image;
        {
            GameSession a;
            CHECK(setup(a));
            CHECK(relay(a, "source") && create(a, "Spawner", {{"Name", std::string("s")}}));
            CHECK(create(a, "ConversationTrigger", {{"Name", std::string("talk")}}));
            CHECK(create(a, "CameraObj", {{"Name", std::string("cam")}}));
            CHECK(steps(a, 1) && save(a, image));
        }
        std::vector<SaveEngineRecord> records;
        CHECK(decode(image, records) && records.size() == 4);
        struct Corruption
        {
            std::size_t record, offset;
            std::uint8_t value;
            const char* expected;
        };
        const std::vector<Corruption> corruptions{
            {0, 0, 2, "Trigger: saved boolean"},                           // m_bActive.
            {1, records[1].payload.size() - 6, 9, "Spawner: saved boolean"}, // m_bCreateRiftEffect.
            // m_nNumMessages, after "default.wav" and seven NULL waves and the character byte.
            {2, 4 + 11 + 7 * 4 + 1, 9, "ConversationTrigger: saved conversation"},
            {3, 4, 3, "CameraObj: camera block"},                          // m_nType.
        };
        auto attempt = [&](const std::vector<SaveEngineRecord>& bad, const char* expected)
        {
            SaveImage corrupt = image;
            SaveError encode_error;
            CHECK(encode_save_records(bad, corrupt.objects, encode_error));
            GameSession b;
            CHECK(setup(b));
            RestoreObjectsReport report;
            CHECK(!restore(b, corrupt, report));
            if (report.error.find(expected) == std::string::npos)
            {
                std::cerr << "corruption error: " << report.error << '\n';
                return false;
            }
            std::cout << "refused: " << report.error << '\n';
            CHECK(b.objects.finish_frame() && b.objects.snapshot().empty() && !b.server.string_count() &&
                  !b.server.message_count() && b.server.camera_effects.empty() && !g_hActiveCamera);
            return true;
        };
        for (const auto& c : corruptions)
        {
            auto bad = records;
            bad[c.record].payload[c.offset] = c.value;
            CHECK(attempt(bad, c.expected));
        }
        // More than 255 keys refuses the save: the original byte count desynchronized its reader.
        {
            GameSession g;
            CHECK(setup(g));
            for (unsigned i = 0; i < 256; ++i)
                CHECK(create(g, "Key", {{"Name", "k" + std::to_string(i)}, {"TimeStamp", 1.0f}}));
            CHECK(create(g, "KeyFramer", {{"ObjectName", std::string("none")}, {"BaseKeyName", std::string("k")}}));
            SaveImage refused;
            std::string error;
            CHECK(steps(g, 1) && !save(g, refused, false, &error) && error.find("255 keys") != std::string::npos);
            std::cout << "refused: " << error << '\n';
        }
        auto bad = records;
        bad[0].payload.pop_back(); // A truncated Trigger block is an over-read.
        CHECK(attempt(bad, "Trigger"));
        bad = records;
        bad[1].payload.push_back(0); // A longer Spawner block is an under-read (D5).
        CHECK(attempt(bad, "under-read"));
        return true;
    }

    // Real worlds (private data). For every mounted world: the lot-7 objects present after entry
    // and what still blocks a whole save. Every world restores an image of all its Original
    // objects (lots 5, 7 and 8) through the full restore-mode world and candidate transaction,
    // saves the same bytes again apart from the listed rewrites and runs on; a world whose
    // remaining classes are all covered restores whole.
    bool private_worlds(const std::filesystem::path& data, std::string_view only)
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
        const std::set<std::string> lot7{"Trigger",          "BigTrigger", "ToggleTrigger",       "ExitTrigger",
                                         "ObjectivesTrigger", "Spawner",    "ConversationTrigger", "CinematicTrigger",
                                         "KeyFramer",         "CameraObj"};
        unsigned entered = 0, restored = 0, whole = 0;
        std::map<std::string, unsigned> totals, blockers;
        std::vector<std::string> whole_names;
        for (const auto& name : worlds)
        {
            if (!only.empty() && name.find(only) == std::string::npos)
                continue;
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
            ++entered;
            game->server_shell.inspection_start = true;
            if (!game->enter_player(error))
            {
                std::cout << name << ": no player (" << error.substr(0, 120) << ")\n";
                continue;
            }
            if (auto* player = game->server_shell.Player(game->local_client); player && player->ctf_team)
                player->ctf_team = 0; // CTF match state is never saved (D12).
            for (int i = 0; i < 30; ++i)
                CHECK(game->step(.05));
            std::map<std::string, unsigned> blocking, present;
            std::vector<HOBJECT> covered;
            unsigned overfull = 0;
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
                if (lot7.contains(r->type->name))
                    ++present[r->type->name];
                if (const auto* f = dynamic_cast<const KeyFramer*>(r->game.get()); f && f->targets.size() > f->keys.size())
                {
                    ++overfull; // The original layout loses these targets' offsets.
                    std::cout << "  keyframer " << r->state.m_Name << " (" << f->object_name << "): "
                              << f->targets.size() << " targets, " << f->keys.size() << " keys, first target "
                              << game->objects.get(f->targets.front().object)->type->name << ", active "
                              << f->active << '\n';
                }
            }
            std::string names, mine;
            for (const auto& [type, count] : blocking)
            {
                names += (names.empty() ? "" : ", ") + type + " x" + std::to_string(count);
                ++blockers[type];
            }
            for (const auto& [type, count] : present)
            {
                mine += (mine.empty() ? "" : ", ") + type + " x" + std::to_string(count);
                totals[type] += count;
            }
            std::cout << name << ": " << (mine.empty() ? "no lot-7 object" : mine)
                      << (overfull ? "; " + std::to_string(overfull) + " keyframers with more targets than keys" : "")
                      << "; " << (blocking.empty() ? "no blocking class" : "blocked by " + names) << '\n';
            if (blocking.empty())
            {
                GameSave save;
                CHECK(game->save_game(save, error));
                SaveImage first = save.image;
                if (!GameSession::load_game(game, save, error))
                {
                    std::cerr << name << " whole load refused at " << game->game_load.stage << ": " << error << '\n';
                    return false;
                }
                CHECK(game->save_game(save, error) && same_image(first, save.image, false));
                CHECK(steps(*game, 30, .05) && game->server.unavailable_services.empty());
                ++whole;
                whole_names.push_back(name);
                continue;
            }
            SaveObjectsReport saved;
            CHECK(game->server.save_objects(current, covered, 0, save_flags, saved));
            GameSave save{game->world_path, {}, *game->server.save_store.find(SaveStoreSlot::Current)};
            SaveError fingerprint;
            CHECK(
                build_fingerprint(game->resources, save.world_path, save.image.classes, save.fingerprint, fingerprint));
            if (!GameSession::load_game(game, save, error))
            {
                std::cerr << name << " Original-object load refused at " << game->game_load.stage << ": " << error
                          << '\n';
                return false;
            }
            const auto restored_objects = game->server.last_restore.objects;
            SaveObjectsReport again;
            CHECK(game->server.save_objects(current, restored_objects, 0, save_flags, again));
            if (!same_image(save.image, *game->server.save_store.find(SaveStoreSlot::Current), false))
            {
                std::cerr << name << ": the restored Original objects save different bytes\n";
                return false;
            }
            CHECK(steps(*game, 30, .05) && game->server.unavailable_services.empty());
            std::cout << "  restored " << restored_objects.size() << " Original records, saved them again and ran 30 "
                      << "steps\n";
            ++restored;
        }
        std::cout << entered << " of " << worlds.size() << " worlds entered; " << restored
                  << " restored their Original objects; " << whole << " restored whole";
        for (const auto& n : whole_names)
            std::cout << ' ' << n;
        std::cout << "\nlot-7 objects:";
        for (const auto& [type, count] : totals)
            std::cout << ' ' << type << ' ' << count;
        std::cout << "\nblocking classes (worlds):";
        for (const auto& [type, count] : blockers)
            std::cout << ' ' << type << ' ' << count;
        std::cout << '\n';
        CHECK(entered > 0 && restored + whole == entered);
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    const std::string_view mode = argv[1];
    root = argv[2];
    std::error_code ec;
    if (mode != "private")
        std::filesystem::create_directories(root, ec);
    const std::map<std::string_view, std::function<bool()>> cases{
        {"triggers", triggers},     {"doors", doors},           {"spawners", spawners}, {"keyframers", keyframers},
        {"cameras", cameras},       {"cinematics", cinematics}, {"refusals", refusals}};
    if (mode == "private")
        return private_worlds(argv[2], argc > 3 ? argv[3] : "") ? 0 : 1; // Optional world filter.
    const auto it = cases.find(mode);
    return it != cases.end() && it->second() ? 0 : 1;
}
