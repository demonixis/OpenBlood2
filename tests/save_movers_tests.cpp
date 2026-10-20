#include "brush_fixture.h"
#include "engine/config.h"
#include "engine/container.h"
#include "engine/save_objects.h"
#include "game/game_settings.h"
#include "game/original/DestructableBrush.h"
#include "game/original/DestructableModel.h"
#include "game/original/Door.h"
#include "game/original/RotatingBrush.h"
#include "game/original/RotatingDoor.h"
#include "game/original/RotatingModel.h"
#include "game/original/Tchernotronic.h"
#include "game/original/TripLaser.h"
#include "game/original/VolumeBrush.h"
#include "game/original/WorldModelDebris.h"
#include "game/original/trigger_helpers.h"
#include "player_fixture.h"
#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <map>
#include <set>

// Original Save/Load bodies of the movers and brushes (save_load_contracts lot 8): byte-level
// round trips of each class in the states a player can save, the re-save byte identity, and
// identical continuation of the saved and the restored worlds from the restored clock.
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

    // TripLaser forces Models\default.abc and Skins\ammo\beamred.dtx; mount them beside the
    // player fixture's synthetic archive.
    bool mount_extra(GameSession& g)
    {
        test_player::Directory dir;
        dir.children["models"].files["default.abc"] = test_model::abc(false, true).b;
        dir.children["skins"].children["ammo"].files["beamred.dtx"] = {1, 2, 3};
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = test_player::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        {
            std::ofstream file(root / "extra.rez", std::ios::binary);
            file.write(reinterpret_cast<const char*>(w.b.data()), static_cast<std::streamsize>(w.b.size()));
            CHECK(file.good());
        }
        std::string error;
        CHECK(g.resources.mount(root / "extra.rez", error));
        return true;
    }
    bool same(DVector a, DVector b)
    {
        return std::bit_cast<std::array<std::uint32_t, 3>>(a) == std::bit_cast<std::array<std::uint32_t, 3>>(b);
    }
    using Models = std::vector<WorldModel>;
    bool setup(GameSession& g, const Models& models)
    {
        CHECK(test_player::setup(g, root, false, true));
        CHECK(mount_extra(g));
        g.world.models = models;
        return true;
    }
    WorldModel box(std::string name, DVector lo, DVector hi, DVector translation)
    {
        auto model = test_brush::box(std::move(name), lo, hi);
        model.translation = translation;
        return model;
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
    bool command(GameSession& g, HOBJECT h, const char* text)
    {
        auto str = g.server.CreateString(text);
        auto msg = g.server.StartMessageToObject(nullptr, h, 1000);
        CHECK(str && msg && g.server.WriteToMessageHString(msg, str) == DE_OK);
        CHECK(g.server.EndMessage(msg) == DE_OK);
        g.server.FreeString(str);
        return true;
    }
    bool damage(GameSession& g, HOBJECT h, float amount, DamageType type = DamageType::Normal)
    {
        auto msg = g.server.StartMessageToObject(g.server.HandleToObject(h), h, damage_message);
        CHECK(msg && g.server.WriteDamage(msg, {{0, 0, 1}, {}, amount, type, h}) == DE_OK);
        CHECK(g.server.EndMessage(msg) == DE_OK);
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
    // The original shell's list: every live object with USRFLG_SAVEABLE.
    std::vector<HOBJECT> saveable(GameSession& g)
    {
        std::vector<HOBJECT> list;
        for (auto id : g.objects.snapshot())
            if (auto* r = g.objects.get(id); r && r->life == Life::Alive && (r->user_flags & USRFLG_SAVEABLE))
                list.push_back(r->handle);
        return list;
    }
    bool save(GameSession& g, SaveImage& image, std::string* error = nullptr)
    {
        SaveObjectsReport report;
        const bool ok = g.server.save_objects(current, saveable(g), 0, save_flags, report);
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
    // The original rewrites listed by save_load_contracts: CDestructableBrush's PostPropRead
    // adds FULLPOSITIONRES | MODELGOURAUDSHADE back to a started WorldModelDebris, whose Start
    // replaced every flag.
    void listed_rewrites(std::vector<SaveEngineRecord>& records)
    {
        for (auto& r : records)
            if (r.class_name == "WorldModelDebris")
                r.flags &= ~(FLAG_FULLPOSITIONRES | FLAG_MODELGOURAUDSHADE);
            else if (r.class_name == "CPlayerObj")
            {
                // Lot 5's listed player rewrites (Load's flags, velocity, node reset, zoom and the
                // blob's load type); save_player checks the player's bytes field by field.
                r.flags &= ~FLAG_SHADOW;
                r.velocity = {};
                r.hidden_nodes.clear();
                r.payload.clear();
            }
    }
    // Relative float clocks (Save writes t - GetTime()) restore to the same saved difference, but
    // when the original subtraction rounded, the absolute value may land one ulp away; later
    // saves can then differ by one ulp in that field. Accept exactly that and nothing else.
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
                    k = start + 3; // Later differing bytes of this float are part of the same ulp.
                }
            }
            if (!explained)
                return false;
        }
        return true;
    }
    // Compares two images record by record, naming the first difference.
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
                if (!same(x[i].position, y[i].position))
                    std::cerr << " position";
                if (x[i].update_remaining != y[i].update_remaining)
                    std::cerr << " delay " << x[i].update_remaining << " vs " << y[i].update_remaining;
                if (x[i].flags != y[i].flags || x[i].user_flags != y[i].user_flags)
                    std::cerr << " flags";
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

    // One round trip. `build` creates the objects in the saved world and drives them to the
    // state under test, recording what `check` expects of the restored world. The saved world
    // records its images for `after` more steps (only one session can be bound at a time); the
    // restored world must save the same bytes again, then match every recorded image.
    struct Trip
    {
        Models models;
        std::function<bool(GameSession&)> build;
        std::function<bool(GameSession&, const RestoreObjectsReport&)> check;
        std::function<bool(GameSession&)> before_restore; // Always-load objects of a restore-mode world.
        unsigned after = 60;
    };
    bool round_trip(const char* label, const Trip& trip)
    {
        SaveImage saved;
        std::vector<SaveImage> expected;
        {
            GameSession a;
            CHECK(setup(a, trip.models));
            CHECK(trip.build(a));
            CHECK(save(a, saved));
            for (unsigned i = 0; i < trip.after; ++i)
            {
                CHECK(steps(a, 1));
                CHECK(save(a, expected.emplace_back()));
            }
        }
        GameSession b;
        CHECK(setup(b, trip.models));
        if (trip.before_restore)
        {
            // A restore-mode world: CONS and CLCK first, then the always-load objects.
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
        CHECK(save(b, again));
        if (!same_image(saved, again, false))
        {
            std::cerr << label << ": the restored world saves different bytes\n";
            return false;
        }
        for (unsigned i = 0; i < trip.after; ++i)
        {
            SaveImage y;
            CHECK(steps(b, 1) && save(b, y));
            if (!same_image(expected[i], y, true, true))
            {
                std::cerr << label << ": the worlds diverge after " << i + 1 << " steps\n";
                return false;
            }
        }
        std::cout << label << ": " << saved.records << " records, " << saved.objects.size() << " OBJS bytes, "
                  << trip.after << " identical steps\n";
        return true;
    }

    // Doors: a translating door and a rotating door.
    Models door_models()
    {
        return {box("door", {-10, 10, -10}, {10, 30, 10}, {5, 20, 0}),
                box("swing", {200, 0, -2}, {240, 40, 2}, {220, 20, 0})};
    }
    HOBJECT door(GameSession& g, Properties props = {})
    {
        props.try_emplace("Name", std::string("door"));
        props.try_emplace("Speed", 200.0f);
        props.try_emplace("MoveDist", 50.0f);
        props.try_emplace("MoveDir", DVector{2, 0, 0});
        props.try_emplace("TriggerClosed", true);
        props.try_emplace("WaitTime", .5f);
        props.try_emplace("OpenDelay", .1f);
        props.try_emplace("Waveform", std::int32_t(2));
        props.try_emplace("OpenBusySound", std::string("sounds\\doors\\busy.wav"));
        props.try_emplace("PortalName", std::string("portal1"));
        return create(g, "Door", props);
    }
    // World-space bounds of a world model's posed shell (queries offset it by the position).
    std::pair<DVector, DVector> posed_bounds(GameSession& g, HOBJECT h)
    {
        const auto* r = g.objects.get(h);
        const auto offset = vector_subtract(r->state.m_Pos, r->world_model->origin);
        return {vector_add(r->world_model->min, offset), vector_add(r->world_model->max, offset)};
    }
    HOBJECT swing(GameSession& g)
    {
        return create(g, "RotatingDoor",
                      {{"Name", std::string("swing")},
                       {"Speed", 90.0f},
                       {"RotationAngles", DVector{0, 90, 0}},
                       {"RotationPoint", DVector{200, 20, 0}},
                       {"TriggerClosed", false},
                       {"WaitTime", .3f}});
    }
    bool doors()
    {
        struct Case
        {
            const char* label;
            std::function<bool(GameSession&, HOBJECT, HOBJECT)> drive;
            Door::State state;
        };
        const std::vector<Case> cases{
            {"door closed", [](GameSession& g, HOBJECT, HOBJECT) { return steps(g, 3); }, Door::Closed},
            {"door opening (delay)",
             [](GameSession& g, HOBJECT d, HOBJECT) { return command(g, d, "TRIGGER") && steps(g, 2); }, Door::Opening},
            {"door opening", [](GameSession& g, HOBJECT d, HOBJECT) { return command(g, d, "TRIGGER") && steps(g, 9); },
             Door::Opening},
            {"door open", [](GameSession& g, HOBJECT d, HOBJECT) { return command(g, d, "TRIGGER") && steps(g, 50); },
             Door::Open},
            {"door closing", [](GameSession& g, HOBJECT d, HOBJECT)
             { return command(g, d, "TRIGGER") && steps(g, 50) && command(g, d, "TRIGGER") && steps(g, 5); },
             Door::Closing},
            {"rotating door opening",
             [](GameSession& g, HOBJECT, HOBJECT s) { return command(g, s, "TRIGGER") && steps(g, 15); }, Door::Closed},
        };
        for (const auto& c : cases)
        {
            struct
            {
                float open_time = 0, angle = 0;
                bool started = false;
                DVector min{}, max{};
                Door::State swing = Door::Closed;
            } want;
            Trip trip;
            trip.models = door_models();
            trip.build = [&](GameSession& g)
            {
                auto d = door(g), s = swing(g);
                CHECK(d && s && c.drive(g, d, s));
                auto* x = get<Door>(g, d);
                auto* p = get<RotatingDoor>(g, s);
                if (x->state != c.state)
                    std::cerr << c.label << ": state " << x->state << '\n';
                CHECK(x->state == c.state);
                const auto [min, max] = posed_bounds(g, s);
                want = {x->open_time, p->angles.y, x->started_moving, min, max, p->state};
                return true;
            };
            trip.check = [&](GameSession& b, const RestoreObjectsReport&)
            {
                auto* y = get<Door>(b, named(b, "door"));
                CHECK(y && y->state == c.state && y->open_time == want.open_time && y->started_moving == want.started);
                CHECK(y->move_dir.x == 1 && y->closed_pos.x == 5 && y->open_pos.x == 55);
                CHECK(y->open_busy == "sounds\\doors\\busy.wav" && y->portal == "portal1" && y->waveform == 2);
                CHECK(y->trigger_closed && !y->self_trigger && !y->trigger);
                CHECK(!y->loop_voice && y->last_sound.empty()); // Original loss: the busy loop is not restarted.
                auto* q = get<RotatingDoor>(b, named(b, "swing"));
                CHECK(q && q->angles.y == want.angle && q->state == want.swing && q->open_direction.y == 1);
                // The restored shell is posed about the BSP origin and offset by the saved position.
                const auto [min, max] = posed_bounds(b, q->m_hObject);
                CHECK(std::abs(min.x - want.min.x) < .001f && std::abs(min.z - want.min.z) < .001f);
                CHECK(std::abs(max.x - want.max.x) < .001f && std::abs(max.z - want.max.z) < .001f);
                return true;
            };
            CHECK(round_trip(c.label, trip));
        }
        // A blocked door retries at its deadline in both worlds.
        Trip blocked;
        blocked.models = door_models();
        blocked.build = [](GameSession& g)
        {
            // The door descends onto a crate resting on the floor: the crate cannot be pushed into
            // the primary world, so the whole move is refused.
            auto d = door(g, {{"OpenDelay", .01f}, {"MoveDir", DVector{0, -1, 0}}});
            auto blocker = create(g, "CDestructableModel",
                                  {{"Name", std::string("crate")},
                                   {"InitFilename", std::string("models/characters/caleb.abc")},
                                   {"InitSkin", std::string("skins/characters/caleb1.dtx")},
                                   {"Pos", DVector{5, 4.01f, 0}},
                                   {"InitDims", DVector{4, 4, 4}},
                                   {"Gravity", false}});
            CHECK(d && blocker && command(g, d, "TRIGGER") && steps(g, 30));
            CHECK(get<Door>(g, d)->last_move == MoverResult::Blocked && get<Door>(g, d)->state == Door::Opening);
            return true;
        };
        blocked.check = [](GameSession& b, const RestoreObjectsReport&)
        {
            CHECK(get<Door>(b, named(b, "door"))->state == Door::Opening);
            return true;
        };
        CHECK(round_trip("door blocked", blocked));
        return true;
    }

    // Rotators: a spinning and a spinning-up RotatingBrush, one that never moved (its pose
    // still the authored rotation), and a RotatingModel.
    bool rotators()
    {
        std::map<std::string, Rotating> want;
        Trip trip;
        trip.models = {box("spin", {-20, 40, -2}, {20, 60, 2}, {0, 50, 0}),
                       box("ramp", {100, 40, -2}, {140, 60, 2}, {120, 50, 0}),
                       box("idle", {300, 40, -2}, {340, 60, 2}, {320, 50, 0})};
        trip.build = [&](GameSession& g)
        {
            auto spin = create(g, "RotatingBrush",
                               {{"Name", std::string("spin")}, {"YAxisRevTime", 4.0f}, {"HitPoints", 10.0f}});
            auto ramp = create(g, "RotatingBrush",
                               {{"Name", std::string("ramp")},
                                {"YAxisRevTime", 2.0f},
                                {"YAxisSpinUpTime", 3.0f},
                                {"StartOn", false},
                                {"BusySound", std::string("sounds\\busy.wav")}});
            auto idle = create(g, "RotatingBrush",
                               {{"Name", std::string("idle")},
                                {"YAxisRevTime", 2.0f},
                                {"StartOn", false},
                                {"Rotation", EulerAngles{{0, .5f, 0}}}});
            auto model = create(g, "RotatingModel",
                                {{"Name", std::string("fan")},
                                 {"InitFilename", std::string("models/characters/caleb.abc")},
                                 {"InitSkin", std::string("skins/characters/caleb1.dtx")},
                                 {"Pos", DVector{0, 100, 200}},
                                 {"Gravity", false},
                                 {"XAxisRevTime", 3.0f}});
            CHECK(spin && ramp && idle && model && steps(g, 10) && command(g, ramp, "ON") && steps(g, 20));
            CHECK(get<RotatingBrush>(g, ramp)->rotating.state == Rotating::SpinUp);
            CHECK(get<RotatingBrush>(g, idle)->rotating.pose_angles.y == .5f &&
                  get<RotatingBrush>(g, idle)->rotating.angles.y == 0);
            for (auto name : {"spin", "ramp", "idle"})
                want[name] = get<RotatingBrush>(g, named(g, name))->rotating;
            want["fan"] = get<RotatingModel>(g, model)->rotating;
            return true;
        };
        trip.check = [&](GameSession& b, const RestoreObjectsReport&)
        {
            for (auto name : {"spin", "ramp", "idle"})
            {
                const auto &x = want[name], &y = get<RotatingBrush>(b, named(b, name))->rotating;
                CHECK(x.state == y.state && x.angles.y == y.angles.y && x.pose_angles.y == y.pose_angles.y);
                // Relative float clocks restore to the same saved difference: the original encoding can
                // be coarser than the absolute value, so start_time may land one ulp away.
                const float now = b.server.GetTime();
                CHECK(x.last_time - now == y.last_time - now && x.start_time - now == y.start_time - now);
                CHECK(x.time_left.y == y.time_left.y);
                CHECK(y.object == named(b, name) && y.velocity.y == x.velocity.y && y.sign.y == x.sign.y);
            }
            CHECK(get<RotatingBrush>(b, named(b, "ramp"))->rotating.busy_sound == "sounds\\busy.wav");
            const auto& y = get<RotatingModel>(b, named(b, "fan"))->rotating;
            CHECK(want["fan"].angles.x == y.angles.x && y.state == Rotating::Normal);
            return true;
        };
        trip.after = 150; // The spin-up completes in both worlds.
        CHECK(round_trip("rotators", trip));
        return true;
    }

    // Brushes: a damaged brush, a dead brush whose removal is pending, destroyed brushes that
    // are gone, and world-model debris in flight.
    bool brushes()
    {
        struct
        {
            unsigned wall_surface = 0;
            float piece_health = 0;
            Rotating::State piece_state = Rotating::Off;
        } want;
        Trip trip;
        trip.models = {box("wall", {-20, 0, -2}, {20, 40, 2}, {0, 20, 0}),
                       box("glass", {100, 0, -2}, {140, 40, 2}, {120, 20, 0}),
                       box("crate", {200, 0, -10}, {220, 20, 10}, {210, 10, 0}),
                       box("cratedebris0", {200, 30, -4}, {208, 34, 4}, {204, 32, 0}),
                       box("cratedebris1", {212, 30, -4}, {220, 34, 4}, {216, 32, 0}),
                       box("dying", {300, 0, -2}, {340, 40, 2}, {320, 20, 0})};
        trip.build = [&](GameSession& g)
        {
            auto wall = create(g, "CDestructableBrush",
                               {{"Name", std::string("wall")},
                                {"HitPoints", 100.0f},
                                {"Stone", true},
                                {"DeathTriggerTarget", std::string("nobody")},
                                {"DeathTriggerMessage", std::string("hello")}});
            auto glass = create(g, "CDestructableBrush", {{"Name", std::string("glass")}, {"HitPoints", 5.0f}});
            for (auto name : {"cratedebris0", "cratedebris1"})
                CHECK(create(g, "WorldModelDebris",
                             {{"Name", std::string(name)},
                              {"HitPoints", 20.0f},
                              {"BoxPhysics", false},
                              {"InitialVelocity", DVector{0, 200, 50}},
                              {"YAxisRevTime", 1.0f},
                              {"YAxisSpinDownTime", .5f}}));
            auto crate = create(g, "CDestructableBrush", {{"Name", std::string("crate")}, {"HitPoints", 5.0f}});
            auto dying = create(g, "CDestructableBrush", {{"Name", std::string("dying")}, {"HitPoints", 5.0f}});
            CHECK(wall && glass && crate && dying && steps(g, 2));
            CHECK(damage(g, wall, 40) && damage(g, glass, 50) && steps(g, 2) && !g.server.IsObjectAlive(glass));
            CHECK(damage(g, crate, 50) && steps(g, 2) && !g.server.IsObjectAlive(crate));
            CHECK(damage(g, dying, 50));
            CHECK(get<CDestructableBrush>(g, dying)->m_damage.IsDead() && g.server.IsObjectAlive(dying));
            auto* piece = get<WorldModelDebris>(g, named(g, "cratedebris0"));
            if (!piece || !piece->on || piece->starts != 1)
                std::cerr << "piece: on " << (piece && piece->on) << " starts " << (piece ? piece->starts : 0)
                          << " state " << (piece ? piece->rotating.state : -1) << '\n';
            CHECK(piece && piece->on && piece->starts == 1);
            want = {get<CDestructableBrush>(g, wall)->debris.surface, piece->m_damage.m_fHitPoints,
                    piece->rotating.state};
            return true;
        };
        trip.check = [&](GameSession& b, const RestoreObjectsReport&)
        {
            CHECK(!named(b, "glass") && !named(b, "crate")); // Destroyed brushes stay gone.
            auto* y = get<CDestructableBrush>(b, named(b, "wall"));
            CHECK(y && y->m_damage.m_fHitPoints == 60 && y->m_damage.death_target == "nobody");
            CHECK(y->m_damage.m_hObject == y->m_hObject && y->debris.object == y->m_hObject &&
                  y->debris.surface == want.wall_surface && y->debris.materials[0] && y->hit_points == 100);
            auto* d = get<CDestructableBrush>(b, named(b, "dying"));
            CHECK(d && d->m_damage.IsDead() && b.objects.get(d->m_hObject)->scheduled);
            auto* q = get<WorldModelDebris>(b, named(b, "cratedebris0"));
            CHECK(q && q->on && q->ready && q->rotating.state == want.piece_state &&
                  b.objects.get(q->m_hObject)->world_body);
            CHECK(q->initial_velocity.y == 200 && q->m_damage.m_fHitPoints == want.piece_health);
            CHECK(CDestructable::m_dwNumDestructables == 4); // Two debris pieces, the wall, the dying brush.
            return true;
        };
        trip.after = 120; // The dying brush goes, and the debris lands and spins down.
        CHECK(round_trip("brushes", trip));
        return true;
    }

    // Destructible models: a damaged model (replacement model and dimensions), one fading.
    bool models()
    {
        struct
        {
            DVector dims{};
            float alpha = 0, rate = 0, last = 0;
        } want;
        Trip trip;
        trip.build = [&](GameSession& g)
        {
            auto damaged = create(g, "CDestructableModel",
                                  {{"Name", std::string("barrel")},
                                   {"InitFilename", std::string("models/characters/caleb.abc")},
                                   {"InitSkin", std::string("skins/characters/caleb1.dtx")},
                                   {"DamageFilename", std::string("models/weapons/beretta_pv.abc")},
                                   {"DamageSkin", std::string("skins/weapons/c_beretta_pv_t.dtx")},
                                   {"DamageDims", DVector{3, 4, 5}},
                                   {"Pos", DVector{0, 100, 0}},
                                   {"Gravity", false},
                                   {"Metal", true}});
            auto fading = create(g, "CDestructableModel",
                                 {{"Name", std::string("ghost")},
                                  {"InitFilename", std::string("models/characters/caleb.abc")},
                                  {"InitSkin", std::string("skins/characters/caleb1.dtx")},
                                  {"Pos", DVector{100, 100, 0}},
                                  {"Gravity", false},
                                  {"Rotation", EulerAngles{{0, .25f, 0}}}});
            CHECK(damaged && fading && damage(g, damaged, 60) && command(g, fading, "Alpha 0 2") && steps(g, 10));
            auto *x = get<CDestructableModel>(g, damaged), *p = get<CDestructableModel>(g, fading);
            CHECK(x->damage_hit_points == 0 && p->alpha < 1 && p->yaw == .25f);
            want = {g.objects.get(damaged)->dimensions, p->alpha, p->fade_rate, p->last_time};
            return true;
        };
        trip.check = [&](GameSession& b, const RestoreObjectsReport&)
        {
            auto* y = get<CDestructableModel>(b, named(b, "barrel"));
            CHECK(y && y->initialized && y->damage_hit_points == 0 && y->m_damage.m_fHitPoints == 40);
            CHECK(std::string(b.objects.get(y->m_hObject)->state.m_Filename) == "models/weapons/beretta_pv.abc");
            CHECK(same(b.objects.get(y->m_hObject)->dimensions, want.dims));
            auto* q = get<CDestructableModel>(b, named(b, "ghost"));
            CHECK(q && q->alpha == want.alpha && q->fade_rate == want.rate && q->last_time == want.last);
            CHECK(q->yaw == .25f && b.objects.get(q->m_hObject)->model_body);
            return true;
        };
        trip.after = 120; // The fade completes from the absolute saved clock.
        CHECK(round_trip("destructible models", trip));
        return true;
    }

    // Volumes: water holding a pushable model body, a hidden damage volume, and a generic
    // VolumeBrush authored as water (its saved container code is kept).
    bool volumes()
    {
        float viscosity = 0;
        Trip trip;
        trip.models = {box("pool", {-50, 0, -50}, {50, 40, 50}, {0, 20, 0}),
                       box("lava", {200, 0, -20}, {240, 20, 20}, {220, 10, 0}),
                       box("tank", {400, 0, -20}, {440, 20, 20}, {420, 10, 0})};
        trip.build = [&](GameSession& g)
        {
            auto pool = create(g, "Water", {{"Name", std::string("pool")}, {"Current", DVector{10, 0, 0}}});
            auto lava = create(g, "Damage", {{"Name", std::string("lava")}, {"Damage", 50.0f}});
            auto tank = create(g, "VolumeBrush",
                               {{"Name", std::string("tank")},
                                {"Code", std::int32_t(CC_WATER)},
                                {"Viscosity", .3f},
                                {"ShowSurface", false},
                                {"FogEnable", true}});
            auto body = create(g, "CDestructableModel",
                               {{"Name", std::string("float")},
                                {"InitFilename", std::string("models/characters/caleb.abc")},
                                {"InitSkin", std::string("skins/characters/caleb1.dtx")},
                                {"Pos", DVector{0, 60, 0}},
                                {"InitDims", DVector{4, 4, 4}},
                                {"Pushable", true}});
            CHECK(pool && lava && tank && body && steps(g, 30));
            CHECK(command(g, lava, "OFF") && get<VolumeBrush>(g, pool)->surface_object);
            viscosity = get<VolumeBrush>(g, pool)->info.viscosity;
            ContainerState state;
            CHECK(g.server.GetContainers(body, g.objects.get(body)->state.m_Pos, state) && state.count == 1);
            return true;
        };
        trip.check = [&](GameSession& b, const RestoreObjectsReport&)
        {
            auto* y = get<VolumeBrush>(b, named(b, "pool"));
            CHECK(y && y->info.code == CC_WATER && y->info.current.x == 10 && y->info.viscosity == viscosity);
            CHECK(y->info.object == y->m_hObject && b.server.IsObjectAlive(y->surface_object));
            CHECK(b.objects.get(y->surface_object)->type->name == std::string("PolyGrid")); // Recreated by Load.
            CHECK(get<VolumeBrush>(b, named(b, "lava"))->info.hidden);
            auto* tank = get<VolumeBrush>(b, named(b, "tank"));
            CHECK(tank->info.code == CC_WATER && b.objects.get(tank->m_hObject)->state.m_ContainerCode == CC_WATER);
            CHECK(tank->info.viscosity == .3f && tank->fog && !tank->surface_object);
            // The pushable body is in the water again (the container index is rebuilt).
            auto body = named(b, "float");
            ContainerState state;
            CHECK(b.server.GetContainers(body, b.objects.get(body)->state.m_Pos, state));
            CHECK(state.count == 1 && state.body[0].code == CC_WATER && state.body[0].object == y->m_hObject);
            // Departure (decision 4): a restored volume still recognizes the player.
            ContainerPhysics player{nullptr, {100, 0, 0}, {}, .1f, true, true, false};
            ContainerPhysics other = player;
            other.player = false;
            tank->UpdatePhysics(player);
            tank->UpdatePhysics(other);
            CHECK(player.velocity.x == 100 && player.gravity && other.velocity.x == 70 && !other.gravity);
            return true;
        };
        CHECK(round_trip("volumes", trip));
        return true;
    }

    // TripLaser on (its loop restarts in Load) and off (flickering), and a Tchernotronic
    // counting down to its removal.
    bool lasers()
    {
        struct
        {
            float damage_time = 0, remove_time = 0;
            DDWORD flags = 0;
            std::string sound_error;
        } want;
        Trip trip;
        trip.build = [&](GameSession& g)
        {
            auto on = create(g, "TripLaser",
                             {{"Name", std::string("beam")},
                              {"Pos", DVector{0, 40, 0}},
                              {"Dims", DVector{1, 1, 52}},
                              {"StartOn", true},
                              {"Sound", std::string("sounds\\hum.wav")},
                              {"TouchTriggerTarget", std::string("alarm")}});
            auto off = create(g, "TripLaser",
                              {{"Name", std::string("flicker")}, {"Pos", DVector{200, 40, 0}}, {"StartOn", true}});
            auto tech = create(g, "Tchernotronic",
                               {{"Name", std::string("tech")},
                                {"Pos", DVector{0, 60, 300}},
                                {"Filename", std::string("models/characters/caleb.abc")},
                                {"Skin", std::string("skins/characters/caleb1.dtx")},
                                {"DestroyTriggerTarget", std::string("door")},
                                {"DestroyTriggerMessage", std::string("trigger")}});
            CHECK(on && off && tech && command(g, off, "TRIGGEROFF") && steps(g, 3));
            auto* t = get<Tchernotronic>(g, tech);
            CHECK(command(g, tech, "TRIGGER") && steps(g, 6) && t);
            if (!t->m_bTimer)
                std::cerr << "tchernotronic: triggers " << t->triggers << " explosions " << t->explosions << '\n';
            CHECK(t->m_bTimer);
            auto* x = get<TripLaser>(g, on);
            want = {x->m_fDamageTime, t->m_fRemoveTime, x->m_dwFlags, x->sound_error};
            return true;
        };
        trip.check = [&](GameSession& b, const RestoreObjectsReport&)
        {
            auto* y = get<TripLaser>(b, named(b, "beam"));
            CHECK(y && y->m_bTriggered && y->m_hstrSound == "sounds\\hum.wav" && y->m_fDamageTime == want.damage_time);
            CHECK(y->m_vEndPoints[1].z > 25 && y->m_dwFlags == want.flags && y->m_hstrTriggerTarget == "alarm");
            CHECK(y->sound_error == want.sound_error); // Load requested the loop again (missing file here).
            auto* f = get<TripLaser>(b, named(b, "flicker"));
            CHECK(f && !f->m_bTriggered && b.objects.get(f->m_hObject)->scheduled);
            auto* q = get<Tchernotronic>(b, named(b, "tech"));
            CHECK(q && q->m_bTimer && q->m_fRemoveTime == want.remove_time && q->m_hstrTriggerTarget == "door");
            return true;
        };
        trip.after = 40; // The Tchernotronic is removed and the flicker ends in both worlds.
        CHECK(round_trip("lasers", trip));
        return true;
    }

    // Sky membership: a door and a brush holding sky slots restore INSKY and their slots; the
    // restore-mode world's always-load SkyPointers then bind the same objects without a failure.
    bool sky()
    {
        Trip trip;
        trip.models = {box("skydoor", {-10, 500, -10}, {10, 520, 10}, {0, 510, 0}),
                       box("skybrush", {100, 500, -10}, {120, 520, 10}, {110, 510, 0})};
        auto pointers = [](GameSession& g)
        {
            CHECK(create(g, "SkyPointer", {{"SkyObjectName", std::string("skydoor")}, {"Index", std::int32_t(3)}}));
            CHECK(create(g, "SkyPointer", {{"SkyObjectName", std::string("skybrush")}, {"Index", std::int32_t(4)}}));
            return true;
        };
        trip.build = [&](GameSession& g)
        {
            CHECK(create(g, "Door", {{"Name", std::string("skydoor")}, {"Speed", 1.0f}}));
            CHECK(create(g, "CDestructableBrush", {{"Name", std::string("skybrush")}, {"HitPoints", 1.0f}}));
            CHECK(pointers(g) && steps(g, 2));
            CHECK(g.server.sky.objects[3] == g.objects.get(named(g, "skydoor"))->id);
            return true;
        };
        trip.before_restore = pointers;
        trip.check = [](GameSession& b, const RestoreObjectsReport&)
        {
            CHECK(b.server.sky.objects[3] == b.objects.get(named(b, "skydoor"))->id);
            CHECK(b.server.sky.objects[4] == b.objects.get(named(b, "skybrush"))->id);
            CHECK(b.server.GetObjectFlags(named(b, "skydoor")) & FLAG_SKYOBJECT);
            return true;
        };
        trip.after = 5; // The pointers bind the restored objects again, then remove themselves.
        CHECK(round_trip("sky", trip));
        return true;
    }

    // Refusals: a corrupt payload fails the restore by class name and rolls back every created
    // record. A self-triggered door saves with its Trigger since lot 7 (save_world_logic.doors).
    bool refusals()
    {
        SaveImage image;
        {
            GameSession a;
            CHECK(setup(a, door_models()));
            CHECK(door(a) && swing(a) && steps(a, 2) && save(a, image));
        }
        std::vector<SaveEngineRecord> records;
        CHECK(decode(image, records) && records.size() == 2 && records[0].class_name == "Door");
        struct Corruption
        {
            std::size_t record, offset;
            std::uint8_t value;
            const char* expected;
        };
        const std::vector<Corruption> corruptions{
            {0, records[0].payload.size() - 1, 7, "saved boolean"}, // Door FireThrough.
            {1, records[1].payload.size() - 1, 2, "saved boolean"}, // RotatingDoor's Door FireThrough.
            {1, 60, 0x7f, "rotating door block"},                   // RotatingDoor open direction X.
        };
        auto attempt = [&](const std::vector<SaveEngineRecord>& bad, const char* expected)
        {
            SaveImage corrupt = image;
            SaveError encode_error;
            CHECK(encode_save_records(bad, corrupt.objects, encode_error));
            GameSession b;
            CHECK(setup(b, door_models()));
            RestoreObjectsReport report;
            CHECK(!restore(b, corrupt, report));
            if (report.error.find(expected) == std::string::npos)
            {
                std::cerr << "corruption error: " << report.error << '\n';
                return false;
            }
            std::cout << "refused: " << report.error << '\n';
            CHECK(b.objects.finish_frame() && b.objects.snapshot().empty() && !b.server.string_count() &&
                  !b.server.message_count());
            return true;
        };
        for (const auto& c : corruptions)
        {
            auto bad = records;
            bad[c.record].payload[c.offset] = c.value;
            CHECK(attempt(bad, c.expected));
        }
        auto bad = records;
        bad[0].payload.pop_back(); // A truncated Door block is an over-read.
        CHECK(attempt(bad, "Door"));
        bad = records;
        bad[0].payload.push_back(0); // A longer one is an under-read (D5).
        CHECK(attempt(bad, "under-read"));
        return true;
    }

    // Real worlds (private data). For every mounted world: the classes still blocking a whole
    // save after entry. Sky worlds bind SkyPointers to Door/CDestructableBrush objects that only
    // a save supplies: each one restores from an image of its sky objects through the full
    // restore-mode world and candidate transaction, and a sky world whose remaining classes are
    // all covered restores whole.
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
        unsigned sky_worlds = 0, sky_restored = 0, whole = 0, entered = 0;
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
            ++entered;
            // A load requires exactly one saved local player (lot 5, D6). Multiplayer maps use the
            // explicit local-inspection marker.
            game->server_shell.inspection_start = true;
            if (!game->enter_player(error))
                std::cout << name << ": no player (" << error.substr(0, 120) << ")\n";
            else if (auto* player = game->server_shell.Player(game->local_client); player && player->ctf_team)
                player->ctf_team = 0; // CTF match state is never saved (D12); this image tests lot-8 objects.
            std::size_t pointers = 0;
            for (const auto& row : game->world.objects)
                pointers += row.class_name == "SkyPointer";
            for (int i = 0; i < 30; ++i)
                CHECK(game->step(.05));
            // What the shell would list, and what blocks it.
            std::map<std::string, unsigned> blocking;
            std::vector<HOBJECT> sky_objects, covered;
            for (auto id : game->objects.snapshot())
            {
                const auto* r = game->objects.get(id);
                if (!r || r->life != Life::Alive)
                    continue;
                const auto support = save_support(r->type);
                if (support == SaveSupport::Unsupported && (r->user_flags & USRFLG_SAVEABLE))
                    ++blocking[r->type->name];
                if (support == SaveSupport::Original && (r->user_flags & USRFLG_SAVEABLE))
                    covered.push_back(r->handle);
                if (std::find(game->server.sky.registered.begin(), game->server.sky.registered.end(), r->id) !=
                    game->server.sky.registered.end())
                    sky_objects.push_back(r->handle);
            }
            std::string names;
            for (const auto& [type, count] : blocking)
                names += (names.empty() ? "" : ", ") + type + " x" + std::to_string(count);
            std::cout << name << ": " << pointers << " sky pointers, " << sky_objects.size() << " sky objects, "
                      << covered.size() << " lot-8 objects; "
                      << (blocking.empty() ? "no blocking class" : "blocked by " + names) << '\n';
            if (blocking.empty())
            {
                GameSave save;
                CHECK(game->save_game(save, error));
                SaveImage first = save.image;
                for (unsigned cycle = 0; cycle < 2; ++cycle)
                {
                    if (!GameSession::load_game(game, save, error))
                    {
                        std::cerr << name << " whole load refused at " << game->game_load.stage << ": " << error
                                  << '\n';
                        return false;
                    }
                    CHECK(game->save_game(save, error) && same_image(first, save.image, false));
                    CHECK(steps(*game, 30, .05) && game->server.unavailable_services.empty());
                    CHECK(game->save_game(save, error));
                    first = save.image;
                }
                ++whole;
                whole_names.push_back(name);
            }
            if (sky_objects.empty())
                continue;
            ++sky_worlds;
            // An image of every lot-8 object through the real restore-mode world: the always-load
            // SkyPointers must bind the restored sky objects again with no sky failure. References
            // to unsaved classes (a door's self-spawned Trigger) read back as NULL.
            SaveObjectsReport saved;
            CHECK(game->server.save_objects(current, covered, 0, save_flags, saved));
            GameSave save{game->world_path, {}, *game->server.save_store.find(SaveStoreSlot::Current)};
            SaveError fingerprint;
            CHECK(
                build_fingerprint(game->resources, save.world_path, save.image.classes, save.fingerprint, fingerprint));
            std::vector<SaveEngineRecord> records;
            CHECK(decode(save.image, records));
            if (!GameSession::load_game(game, save, error))
            {
                std::cerr << name << " lot-8 load refused at " << game->game_load.stage << ": " << error << '\n';
                return false;
            }
            const auto restored = game->server.last_restore.objects; // Record order.
            CHECK(restored.size() == records.size());
            std::size_t in_sky = 0;
            for (std::size_t i = 0; i < records.size(); ++i)
                if (records[i].in_sky)
                {
                    ++in_sky;
                    const auto* r = game->objects.get(restored[i]);
                    CHECK(r && std::find(game->server.sky.registered.begin(), game->server.sky.registered.end(),
                                         r->id) != game->server.sky.registered.end());
                    CHECK(records[i].sky_slot == save_records::no_index ||
                          game->server.sky.objects[records[i].sky_slot] == r->id);
                }
            // DemoSkyWorldModel sky objects are always-load: the restore-mode world recreates them.
            CHECK(in_sky == std::count_if(sky_objects.begin(), sky_objects.end(), [&](HOBJECT h)
                                          { return std::find(covered.begin(), covered.end(), h) != covered.end(); }));
            CHECK(steps(*game, 10, .05) && game->server.unavailable_services.empty());
            CHECK(game->server.sky.registered.size() == sky_objects.size());
            SaveObjectsReport again;
            std::vector<HOBJECT> live;
            for (auto h : restored)
                if (game->server.IsObjectAlive(h))
                    live.push_back(h);
            CHECK(game->server.save_objects(current, live, 0, save_flags, again));
            std::cout << "  restored " << records.size() << " lot-8 records (" << in_sky << " in the sky) and saved "
                      << again.saved << " after 10 steps\n";
            ++sky_restored;
        }
        std::cout << entered << " of " << worlds.size() << " worlds entered; " << sky_restored << " of " << sky_worlds
                  << " sky worlds restored their sky objects; " << whole << " restored whole";
        for (const auto& n : whole_names)
            std::cout << ' ' << n;
        std::cout << '\n';
        CHECK(sky_worlds > 0 && sky_restored == sky_worlds);
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
        {"doors", doors},     {"rotators", rotators}, {"brushes", brushes}, {"models", models},
        {"volumes", volumes}, {"lasers", lasers},     {"sky", sky},         {"refusals", refusals}};
    if (mode == "private")
        return private_worlds(argv[2]) ? 0 : 1;
    const auto it = cases.find(mode);
    return it != cases.end() && it->second() ? 0 : 1;
}
