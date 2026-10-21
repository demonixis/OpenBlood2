#include "audio_fixture.h"
#include "engine/config.h"
#include "engine/save_objects.h"
#include "game/game_settings.h"
#include "game/original/ClientExplosionSFX.h"
#include "game/original/ClientLightFX.h"
#include "game/original/ClientLightningSFX.h"
#include "game/original/ClientParticleStreamSFX.h"
#include "game/original/ClientWarpGateSFX.h"
#include "game/original/DetailSprite.h"
#include "game/original/RandomSoundFX.h"
#include "game/original/SoundFX.h"
#include "save_rewrites.h"
#include "save_world_fixture.h"
#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <map>
#include <set>

// Original Save/Load bodies of the effect publishers and audio emitters (save_load_contracts
// lot 10): byte round trips of each class in the states a player can save, the re-save byte
// identity, identical continuation of the saved and the restored worlds (including evaluated
// lights, sounds and published effects), and one owner-bound effect per publisher after a load.
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

    std::vector<std::uint8_t> spr(unsigned count, unsigned fps, std::string name)
    {
        test_model::Writer w;
        w.u32(count);
        w.u32(fps);
        w.u32(1);
        w.u32(0);
        w.u32(0xff00ff);
        for (unsigned i = 0; i < count; ++i)
        {
            w.u16(static_cast<unsigned>(name.size()));
            for (char c : name)
                w.u8(c);
        }
        return w.b;
    }
    // Sounds (a 0.1 s and a 1 s clip), an animated sprite and a particle texture.
    bool mount_extra(GameSession& g)
    {
        test_player::Directory dir;
        dir.files["cue.wav"] = test_audio::wave();
        dir.files["long.wav"] = test_audio::wave(1, 16, 8000, 8000);
        dir.files["frame.dtx"] = save_world::dtx();
        dir.files["anim.spr"] = spr(3, 10, "frame.dtx");
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
            std::ofstream file(root / "effects.rez", std::ios::binary);
            file.write(reinterpret_cast<const char*>(w.b.data()), static_cast<std::streamsize>(w.b.size()));
            CHECK(file.good());
        }
        std::string error;
        CHECK(g.resources.mount(root / "effects.rez", error));
        return true;
    }
    bool setup(GameSession& g)
    {
        CHECK(test_player::setup(g, root, false, true));
        CHECK(mount_extra(g));
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
    bool command(GameSession& g, HOBJECT h, const char* text)
    {
        auto str = g.server.CreateString(text);
        auto msg = g.server.StartMessageToObject(nullptr, h, 1000);
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
    // Every lot's listed re-save rewrites (save_rewrites.h).
    void listed_rewrites(std::vector<SaveEngineRecord>& records)
    {
        save_rewrites::apply(records);
    }
    bool same_image(const SaveImage& a, const SaveImage& b, bool clock = true)
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
                if (x[i].update_remaining != y[i].update_remaining || x[i].scheduled != y[i].scheduled)
                    std::cerr << " delay " << x[i].update_remaining << " vs " << y[i].update_remaining;
                if (x[i].flags != y[i].flags || x[i].user_flags != y[i].user_flags)
                    std::cerr << " flags";
                if (x[i].sprite_milliseconds != y[i].sprite_milliseconds || x[i].sprite_frame != y[i].sprite_frame)
                    std::cerr << " sprite track";
                std::cerr << '\n';
                return false;
            }
        CHECK(a.links == b.links && a.attachments == b.attachments);
        CHECK(a.classes == b.classes && a.records == b.records);
        if (clock)
            CHECK(a.clock == b.clock && a.console == b.console);
        return true;
    }
    // Every owner-bound publication counted per owner: a load must leave at most one each.
    bool no_duplicates(GameSession& g)
    {
        std::map<HOBJECT, unsigned> lights, streams, gates;
        for (const auto& l : g.server.light_effects)
            CHECK(++lights[l.source] == 1);
        for (const auto& s : g.server.particle_streams.active)
            CHECK(++streams[s.owner] == 1);
        for (const auto& w : g.server.warp_gates.active)
            CHECK(++gates[w.owner] == 1);
        std::map<HOBJECT, unsigned> voices;
        for (const auto& [id, voice] : g.server.audio.voices())
            if (voice.owner && !voice.world_owned && get<SoundFX>(g, voice.owner))
                CHECK(++voices[voice.owner] == 1);
        return true;
    }

    // One round trip, as in save_movers: `build` drives the saved world to the state under test;
    // the saved world records its images (and `probe` values) for `after` more steps; the restored
    // world must save the same bytes again and then match every recorded image and probe.
    using Probe = std::function<std::vector<float>(GameSession&)>;
    struct Trip
    {
        std::function<bool(GameSession&)> build;
        std::function<bool(GameSession&, const RestoreObjectsReport&)> check;
        Probe probe;
        unsigned after = 60;
    };
    bool round_trip(const char* label, const Trip& trip)
    {
        SaveImage saved;
        std::vector<SaveImage> expected;
        std::vector<std::vector<float>> probes;
        {
            GameSession a;
            CHECK(setup(a));
            CHECK(trip.build(a));
            CHECK(save(a, saved));
            for (unsigned i = 0; i < trip.after; ++i)
            {
                CHECK(steps(a, 1));
                CHECK(save(a, expected.emplace_back()));
                if (trip.probe)
                    probes.push_back(trip.probe(a));
            }
        }
        GameSession b;
        CHECK(setup(b));
        RestoreObjectsReport report;
        if (!restore(b, saved, report))
        {
            std::cerr << label << " restore: " << report.error << '\n';
            return false;
        }
        CHECK(report.records == saved.records);
        CHECK(no_duplicates(b));
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
            CHECK(steps(b, 1) && save(b, y) && no_duplicates(b));
            if (!same_image(expected[i], y))
            {
                std::cerr << label << ": the worlds diverge after " << i + 1 << " steps\n";
                return false;
            }
            if (trip.probe && trip.probe(b) != probes[i])
            {
                const auto got = trip.probe(b);
                std::cerr << label << ": evaluated effects diverge after " << i + 1 << " steps:";
                for (std::size_t k = 0; k < std::max(got.size(), probes[i].size()); ++k)
                    if (k >= got.size() || k >= probes[i].size() || got[k] != probes[i][k])
                        std::cerr << " [" << k << "] " << (k < probes[i].size() ? probes[i][k] : NAN) << " vs "
                                  << (k < got.size() ? got[k] : NAN);
                std::cerr << '\n';
                return false;
            }
        }
        std::cout << label << ": " << saved.records << " records, " << saved.objects.size() << " OBJS bytes, "
                  << trip.after << " identical steps\n";
        return true;
    }
    std::size_t voices_of(GameSession& g, HOBJECT owner)
    {
        std::size_t n = 0;
        for (const auto& [id, voice] : g.server.audio.voices())
            n += voice.owner == owner && !voice.done;
        return n;
    }
    std::string voice_of(GameSession& g, HOBJECT owner)
    {
        for (const auto& [id, voice] : g.server.audio.voices())
            if (voice.owner == owner && !voice.done)
                return voice.request.resource + (voice.request.loop ? " (loop)" : "");
        return {};
    }

    // SoundFX in every state: ambient ON (looping), ramping up and down, a non-ambient one-shot
    // ON, and OFF. Load restarts each state's sound; the decision-4 departure keeps the saved
    // deadline, so the ramps reach their next state at the same time as in the saved world.
    bool sounds()
    {
        Trip trip;
        auto sound = [](GameSession& g, const char* name, bool start, bool ambient)
        {
            return create(g, "SoundFX",
                          {{"Name", std::string(name)},
                           {"StartOn", start},
                           {"Ambient", ambient},
                           {"Sound", std::string("cue.wav")},
                           {"RampUpSound", std::string("long.wav")},
                           {"RampDownSound", std::string("long.wav")}});
        };
        trip.build = [&](GameSession& g)
        {
            auto loop = sound(g, "loop", true, true), up = sound(g, "up", false, true),
                 down = sound(g, "down", true, true), shot = sound(g, "shot", true, false),
                 off = sound(g, "off", false, true);
            CHECK(loop && up && down && shot && off && steps(g, 2));
            CHECK(command(g, up, "ON") && command(g, down, "OFF") && steps(g, 15));
            CHECK(get<SoundFX>(g, up)->state == SoundFX::RampUp && get<SoundFX>(g, down)->state == SoundFX::RampDown);
            CHECK(get<SoundFX>(g, loop)->state == SoundFX::On && get<SoundFX>(g, shot)->state == SoundFX::On);
            CHECK(get<SoundFX>(g, off)->state == SoundFX::Off && g.objects.get(up)->scheduled);
            return true;
        };
        trip.check = [](GameSession& b, const RestoreObjectsReport&)
        {
            auto loop = named(b, "loop"), up = named(b, "up"), down = named(b, "down"), off = named(b, "off");
            CHECK(get<SoundFX>(b, loop)->state == SoundFX::On && voice_of(b, loop) == "cue.wav (loop)");
            CHECK(get<SoundFX>(b, up)->state == SoundFX::RampUp && voice_of(b, up) == "long.wav");
            CHECK(get<SoundFX>(b, down)->state == SoundFX::RampDown && voice_of(b, down) == "long.wav");
            CHECK(get<SoundFX>(b, off)->state == SoundFX::Off && !voices_of(b, off));
            // The departure: the ramp's saved deadline survives (the original cleared it).
            CHECK(b.objects.get(up)->scheduled && b.objects.get(down)->scheduled);
            CHECK(get<SoundFX>(b, up)->ramp_up == "long.wav" && get<SoundFX>(b, up)->outer_radius == 500);
            return true;
        };
        // The state each emitter reaches: the ramps end within the window (up -> ON, down -> OFF).
        trip.probe = [](GameSession& g)
        {
            std::vector<float> out;
            for (auto name : {"loop", "up", "down", "shot", "off"})
            {
                const auto h = named(g, name);
                // Voices are not compared: Load restarts each state's sound from its start.
                out.push_back(h ? float(get<SoundFX>(g, h)->state) : -1);
            }
            return out;
        };
        trip.after = 50;
        CHECK(round_trip("sounds", trip));
        return true;
    }

    // RandomSoundFX mid-interval: random and sequential emitters, one switched off. The original
    // never marked them saveable (decision-4 departure); a restore keeps the saved wait, and the
    // shared generator (D10) draws the same next waits and choices in both worlds.
    bool random_sounds()
    {
        Trip trip;
        auto base = std::make_shared<std::map<std::string, std::uint64_t>>();
        trip.build = [](GameSession& g)
        {
            Properties p{{"Name", std::string("random")},
                         {"MinWaitTime", .3f},
                         {"MaxWaitTime", .6f},
                         {"PosRangeRadius", 20.0f},
                         {"SoundFile1", std::string("cue.wav")},
                         {"SoundFile2", std::string("long.wav")}};
            CHECK(create(g, "RandomSoundFX", p));
            p["Name"] = std::string("sequence");
            p["SequentialPlay"] = true;
            p["SoundFile3"] = std::string("cue.wav");
            CHECK(create(g, "RandomSoundFX", p));
            p["Name"] = std::string("quiet");
            auto quiet = create(g, "RandomSoundFX", p);
            CHECK(quiet && steps(g, 40) && command(g, quiet, "OFF"));
            const auto* s = get<RandomSoundFX>(g, named(g, "sequence"));
            CHECK(s->attempts >= 1 && s->current == s->attempts % 3 && g.objects.get(named(g, "random"))->scheduled);
            return true;
        };
        trip.check = [base](GameSession& b, const RestoreObjectsReport&)
        {
            for (auto& [name, count] : *base)
                count = 0; // The restored world's counters start empty.
            const auto* s = get<RandomSoundFX>(b, named(b, "sequence"));
            CHECK(s && s->on && s->sequential && s->count == 3 && s->sounds[1] == "long.wav" && s->sounds[3].empty());
            CHECK(s->min_wait == .3f && s->position_radius == 20 && b.objects.get(s->m_hObject)->scheduled);
            const auto* q = get<RandomSoundFX>(b, named(b, "quiet"));
            CHECK(q && !q->on && !b.objects.get(q->m_hObject)->scheduled);
            return true;
        };
        // Plays since the save (the attempt counters are inspection only) and the last choice and
        // randomized position once a world has played again.
        trip.build = [build = trip.build, base](GameSession& g)
        {
            CHECK(build(g));
            for (auto name : {"random", "sequence", "quiet"})
                (*base)[name] = get<RandomSoundFX>(g, named(g, name))->attempts;
            return true;
        };
        trip.probe = [base](GameSession& g)
        {
            std::vector<float> out;
            for (auto name : {"random", "sequence", "quiet"})
            {
                const auto* r = get<RandomSoundFX>(g, named(g, name));
                const auto played = r->attempts >= (*base)[name] ? r->attempts - (*base)[name] : r->attempts;
                out.push_back(float(played));
                out.push_back(played ? float(r->last_choice) : -1);
                out.push_back(played ? r->last_position.x : 0);
                out.push_back(played ? r->last_position.z : 0);
            }
            return out;
        };
        trip.after = 80; // Several more random waits and choices.
        CHECK(round_trip("random sounds", trip));
        return true;
    }

    // The ClientLightFX family at every waveform, mid-phase, plus an initially-off light (decision
    // 4) and one switched off. Each light publishes again from its saved effect message, once,
    // and its waveforms read the absolute restored clock: every evaluated colour and radius
    // continues exactly.
    bool lights()
    {
        static constexpr const char* types[] = {"SquareWaveLightFX",   "SawWaveLightFX",      "RampUpWaveLightFX",
                                                "RampDownWaveLightFX", "SineWaveLightFX",     "Flicker1WaveLightFX",
                                                "Flicker2WaveLightFX", "Flicker3WaveLightFX", "Flicker4WaveLightFX",
                                                "StrobeWaveLightFX",   "SearchWaveLightFX",   "FlickerLight",
                                                "GlowingLight",        "ClientLightFX"};
        Trip trip;
        trip.build = [](GameSession& g)
        {
            unsigned i = 0;
            for (auto type : types)
            {
                ++i;
                CHECK(create(g, type,
                             {{"Name", std::string(type)},
                              {"Pos", DVector{float(i) * 10, 20, 0}},
                              {"IntensityFreq", 1.5f + i * .25f},
                              {"IntensityPhase", 30.0f * i},
                              {"RadiusFreq", 2.0f + i * .5f},
                              {"RadiusPhase", 15.0f * i},
                              {"Color", DVector{255, 128, 64}},
                              {"LifeTime", 100.0f},
                              {"HitPoints", 5.0f},
                              {"RampUpSound", std::string("cue.wav")}}));
            }
            CHECK(create(g, "SineWaveLightFX", {{"Name", std::string("initially off")}, {"LightSwitch", false}}));
            auto off = create(g, "GlowingLight", {{"Name", std::string("switched off")}});
            CHECK(off && steps(g, 37) && command(g, off, "OFF"));
            CHECK(!(g.server.GetObjectUserFlags(off) & 1u) && g.server.light_effects.size() == std::size(types) + 2);
            return true;
        };
        trip.check = [](GameSession& b, const RestoreObjectsReport&)
        {
            CHECK(b.server.light_effects.size() == std::size(types) + 2);
            for (auto type : types)
            {
                const auto h = named(b, type);
                const auto* l = get<ClientLightFX>(b, h);
                const auto* fx = b.server.GetLightEffect(h);
                CHECK(l && fx && l->on && !l->dynamic && l->lifetime == 100 && l->damage.m_fHitPoints == 5);
                CHECK(fx->settings.ramp_up_sound == "cue.wav" && fx->settings.color.x == 255.0f / 511);
                CHECK(b.server.GetObjectFlags(h) == (FLAG_VISIBLE | FLAG_GOTHRUWORLD)); // Listed rewrite.
            }
            const auto* off = get<ClientLightFX>(b, named(b, "initially off"));
            CHECK(off && !off->on && b.server.GetLightEffect(off->m_hObject));
            CHECK(!get<ClientLightFX>(b, named(b, "switched off"))->on);
            return true;
        };
        trip.probe = [](GameSession& g)
        {
            std::vector<float> out;
            // An invisible light keeps the colour its client last evaluated (here, its publication),
            // which the republication evaluates again; only what is lit is compared.
            for (const auto& l : g.server.light_effects)
            {
                out.push_back(float(l.visible));
                out.push_back(l.radius);
                if (l.visible)
                    out.insert(out.end(), {l.color.x, l.color.y, l.color.z});
            }
            return out;
        };
        CHECK(round_trip("lights", trip));
        return true;
    }

    // A pending (untriggered) scene explosion keeps its cached position, damage and sound; once
    // triggered after the restore it explodes where the saved one would. A triggered emitter is
    // removed at its next update; its explosion was an instant effect and is not replayed.
    bool explosions()
    {
        Trip trip;
        trip.build = [](GameSession& g)
        {
            CHECK(create(g, "CClientExplosionSFX",
                         {{"Name", std::string("pending")},
                          {"Pos", DVector{40, 15, 20}},
                          {"ExplosionType", std::int32_t(3)},
                          {"Damage", 75.0f},
                          {"DamageRadius", 150.0f},
                          {"Direction", DVector{0, 0, 1}},
                          {"WaveFile", std::string("cue.wav")}}));
            return steps(g, 7);
        };
        trip.check = [](GameSession& b, const RestoreObjectsReport&)
        {
            const auto* e = get<CClientExplosionSFX>(b, named(b, "pending"));
            CHECK(e && !e->triggered && e->position.x == 40 && e->normal.z == 1 && e->type == 3 && e->damage == 75);
            CHECK(e->radius == 150 && e->sound == "cue.wav" && b.server.scene_explosions.empty());
            return true;
        };
        CHECK(round_trip("pending explosion", trip));
        SaveImage image;
        {
            GameSession a;
            CHECK(setup(a) && trip.build(a));
            CHECK(command(a, named(a, "pending"), "TRIGGER") && a.server.scene_explosions.size() == 1);
            CHECK(save(a, image));
        }
        GameSession b;
        CHECK(setup(b));
        RestoreObjectsReport report;
        CHECK(restore(b, image, report) && b.server.scene_explosions.empty());
        const auto e = named(b, "pending");
        CHECK(get<CClientExplosionSFX>(b, e)->triggered);
        CHECK(steps(b, 6) && !b.server.IsObjectAlive(e)); // Removed by its next update (0.1 s).
        std::cout << "triggered explosion: removed after the restore, not replayed\n";
        return true;
    }

    // Particle streams on and off, each published again from its saved effect message, once. The
    // client starts from the published ON, as the original engine's replay did; the saved ON and
    // USRFLG_VISIBLE then drive it.
    bool streams()
    {
        Trip trip;
        trip.build = [](GameSession& g)
        {
            Properties p{
                {"Name", std::string("on")}, {"InitiallyOn", true},   {"NumParticles", std::int32_t(3)},
                {"AddDelay", .05f},          {"RampTime", .2f},       {"MinLifetime", .3f},
                {"MaxLifetime", .5f},        {"MinColor", DVector{}}, {"ParticleFile", std::string("frame.dtx")},
                {"Pos", DVector{0, 30, 0}}};
            CHECK(create(g, "CClientParticleStreamSFX", p));
            p["Name"] = std::string("off");
            p["InitiallyOn"] = false;
            CHECK(create(g, "CClientParticleStreamSFX", p));
            p["Name"] = std::string("stopped");
            p["InitiallyOn"] = true;
            auto stopped = create(g, "CClientParticleStreamSFX", p);
            p["Name"] = std::string("started");
            p["InitiallyOn"] = false;
            auto started = create(g, "CClientParticleStreamSFX", p);
            CHECK(stopped && started && steps(g, 10) && command(g, stopped, "OFF") && command(g, started, "ON"));
            CHECK(steps(g, 3) && g.server.particle_streams.active.size() == 4);
            return true;
        };
        trip.check = [](GameSession& b, const RestoreObjectsReport&)
        {
            CHECK(b.server.particle_streams.active.size() == 4);
            for (auto [name, on, published] : {std::tuple{"on", true, true}, std::tuple{"off", false, false},
                                               std::tuple{"stopped", false, true}, std::tuple{"started", true, false}})
            {
                const auto h = named(b, name);
                const auto* s = get<CClientParticleStreamSFX>(b, h);
                CHECK(s && s->settings.on == on && s->published.on == published && s->effect);
                CHECK(bool(b.server.GetObjectUserFlags(h) & 1u) == on && s->published.texture == "frame.dtx");
                for (const auto& i : b.server.particle_streams.active)
                    if (i.owner == h)
                        CHECK(i.effect.state == (published ? 2u : 0u) && i.effect.settings.count == 3);
            }
            return true;
        };
        CHECK(round_trip("streams", trip));
        return true;
    }

    // Lightning mid-bolt: the bolt is an instant effect and is lost, as in the original; the
    // emitter keeps its last endpoints and settings and fires again normally.
    bool lightning()
    {
        Trip trip;
        trip.build = [](GameSession& g)
        {
            auto h = create(g, "CClientLightningSFX",
                            {{"Name", std::string("bolt")},
                             {"Pos", DVector{0, 200, 0}},
                             {"DamageRadius", 0.0f},
                             {"Sound", std::string("cue.wav")},
                             {"LightningShape", std::int32_t(1)}});
            CHECK(h && steps(g, 3) && command(g, h, "FIRE") && steps(g, 1));
            const auto* l = get<CClientLightningSFX>(g, h);
            if (l->fires != 1)
                std::cerr << "lightning: " << l->error << '\n';
            CHECK(l->fires == 1);
            return true;
        };
        DVector destination{};
        trip.check = [&](GameSession& b, const RestoreObjectsReport&)
        {
            const auto* l = get<CClientLightningSFX>(b, named(b, "bolt"));
            CHECK(l && l->bolt.shape == 1 && l->damage_radius == 0 && l->sound == "cue.wav" && !l->fires);
            CHECK(l->bolt.source.y == 200 && b.server.lightning.active.empty());
            destination = l->bolt.destination;
            return true;
        };
        CHECK(round_trip("lightning", trip));
        std::cout << "  last bolt endpoint " << destination.x << ' ' << destination.y << ' ' << destination.z << '\n';
        return true;
    }

    // A warp gate ramping up: the decision-4 departure publishes its visuals again, once (the
    // original lost them). A gate saved before its first update publishes at that update.
    bool warp_gates()
    {
        Properties gate{{"Name", std::string("gate")},
                        {"InitiallyOn", true},
                        {"RampUpTime", 3.0f},
                        {"SpriteFile", std::string("anim.spr")},
                        {"PS1ParticleFile", std::string("frame.dtx")},
                        {"PS2ParticleFile", std::string("frame.dtx")},
                        {"Pos", DVector{0, 40, 0}}};
        Trip trip;
        trip.build = [&](GameSession& g)
        {
            auto h = create(g, "CClientWarpGateSFX", gate);
            CHECK(h && steps(g, 70));
            const auto* w = get<CClientWarpGateSFX>(g, h);
            CHECK(w->event && !w->first_update && g.server.warp_gates.active.size() == 1);
            return true;
        };
        trip.check = [](GameSession& b, const RestoreObjectsReport&)
        {
            const auto h = named(b, "gate");
            const auto* w = get<CClientWarpGateSFX>(b, h);
            CHECK(w && w->on && !w->first_update && w->republished == 1 && w->event);
            CHECK(b.server.warp_gates.active.size() == 1 && b.server.warp_gates.active[0].owner == h);
            CHECK(w->settings.ramp_up == 3 && w->settings.sprite.resource == "anim.spr");
            return true;
        };
        CHECK(round_trip("warp gate ramping", trip));
        SaveImage image;
        {
            GameSession a;
            CHECK(setup(a));
            CHECK(create(a, "CClientWarpGateSFX", gate) && steps(a, 5) && save(a, image));
            CHECK(a.server.warp_gates.active.empty());
        }
        GameSession b;
        CHECK(setup(b));
        RestoreObjectsReport report;
        CHECK(restore(b, image, report) && b.server.warp_gates.active.empty());
        CHECK(get<CClientWarpGateSFX>(b, named(b, "gate"))->first_update);
        CHECK(steps(b, 51) && b.server.warp_gates.active.size() == 1 && no_duplicates(b));
        std::cout << "warp gate before its first update: published once at that update\n";
        return true;
    }

    // DetailSprites: an animated sprite saved mid-frame (the engine record's sprite track), a
    // static one and a repeat-like copy created without a filename member.
    bool sprites()
    {
        Trip trip;
        trip.build = [](GameSession& g)
        {
            CHECK(create(g, "DetailSprite",
                         {{"Name", std::string("anim")},
                          {"Filename", std::string("anim.spr")},
                          {"ScaleX", 2.0f},
                          {"Chromakey", true},
                          {"Pos", DVector{0, 50, 0}}}));
            ObjectCreateStruct copy;
            legacy_copy(copy.m_Filename, "anim.spr");
            legacy_copy(copy.m_Name, "copy");
            copy.m_Pos = {30, 50, 0};
            CHECK(g.objects.create(g.objects.find_class("DetailSprite"), copy, Creation::Runtime));
            CHECK(steps(g, 13));
            const auto* r = g.objects.get(named(g, "anim"));
            CHECK(r && r->sprite_track.frame == 2);
            return true;
        };
        trip.check = [](GameSession& b, const RestoreObjectsReport&)
        {
            const auto* s = get<DetailSprite>(b, named(b, "anim"));
            CHECK(s && s->filename == "anim.spr" && s->scale.x == 2 && s->chromakey && !s->flush);
            CHECK(b.objects.get(s->m_hObject)->sprite_track.frame == 2);
            CHECK(get<DetailSprite>(b, named(b, "copy"))->filename.empty());
            return true;
        };
        trip.probe = [](GameSession& g)
        {
            return std::vector<float>{float(g.objects.get(named(g, "anim"))->sprite_track.frame),
                                      float(g.objects.get(named(g, "copy"))->sprite_track.frame)};
        };
        CHECK(round_trip("sprites", trip));
        return true;
    }

    // Corrupt payloads fail the restore by class name and roll back every created record.
    bool refusals()
    {
        SaveImage image;
        {
            GameSession a;
            CHECK(setup(a));
            CHECK(create(a, "SoundFX", {{"Sound", std::string("cue.wav")}}));
            CHECK(create(a, "RandomSoundFX", {{"SoundFile1", std::string("cue.wav")}}));
            CHECK(create(a, "GlowingLight", {}));
            CHECK(create(a, "CClientParticleStreamSFX", {{"ParticleFile", std::string("frame.dtx")}}));
            CHECK(create(a, "CClientWarpGateSFX", {{"SpriteFile", std::string("anim.spr")}}));
            CHECK(create(a, "CClientLightningSFX", {}));
            CHECK(create(a, "CClientExplosionSFX", {}));
            CHECK(create(a, "DetailSprite", {{"Filename", std::string("anim.spr")}}));
            CHECK(steps(a, 2) && save(a, image));
        }
        std::vector<SaveEngineRecord> records;
        CHECK(decode(image, records) && records.size() == 8);
        struct Corruption
        {
            std::size_t record, offset;
            std::uint8_t value;
            const char* expected;
        };
        const std::vector<Corruption> corruptions{
            {0, 0, 9, "SoundFX: sound block"},                                             // State.
            {1, 21, 11, "RandomSoundFX: random sound block"},                              // Count past ten.
            {2, 0, 2, "GlowingLight: saved boolean"},                                      // ON.
            {2, 34, 12, "GlowingLight: light block"},                                      // Intensity waveform.
            {3, 0, 3, "CClientParticleStreamSFX: saved boolean"},                          // ON.
            {3, 20, 0x7f, "CClientParticleStreamSFX: particle stream message"},            // Particle count.
            {4, 8, 1, "CClientWarpGateSFX: warp gate block"},                              // bInitiallyOn.
            {5, records[5].payload.size() - 1, 1, "CClientLightningSFX: lightning block"}, // bTriggered.
            {6, 27, 0x3f, "CClientExplosionSFX: explosion block"},                         // Offset.
            {7, 24, 2, "DetailSprite: saved boolean"},                                     // Rotatable.
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
            CHECK(b.objects.finish_frame());
            if (!b.objects.snapshot().empty() || b.server.string_count() || b.server.message_count())
                std::cerr << "left: " << b.objects.snapshot().size() << " objects, " << b.server.string_count()
                          << " strings, " << b.server.message_count() << " messages\n";
            CHECK(b.objects.snapshot().empty() && !b.server.string_count() && !b.server.message_count() && steps(b, 1));
            CHECK(b.server.light_effects.empty() && b.server.particle_streams.active.empty() &&
                  b.server.warp_gates.active.empty());
            return true;
        };
        for (const auto& c : corruptions)
        {
            auto bad = records;
            bad[c.record].payload[c.offset] = c.value;
            CHECK(attempt(bad, c.expected));
        }
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            auto bad = records;
            bad[i].payload.pop_back(); // A truncated block is an over-read.
            CHECK(attempt(bad, records[i].class_name.c_str()));
            bad = records;
            bad[i].payload.push_back(0); // A longer one is an under-read (D5).
            CHECK(attempt(bad, "under-read"));
        }
        return true;
    }

    // Real worlds (private data). For every mounted world: the classes still blocking a whole save
    // after entry; a world with none restores whole twice. Every world also restores an image of
    // all its Original objects (lots 5, 8 and 10, the player included) through the real
    // restore-mode world and candidate transaction: every light, stream and warp gate publishes
    // once, SoundFX loops restart, and the world runs on with no unavailable service.
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
        std::map<std::string, unsigned> totals;
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
            game->server_shell.inspection_start = true;
            if (!game->enter_player(error))
                std::cout << name << ": no player (" << error.substr(0, 120) << ")\n";
            else if (auto* player = game->server_shell.Player(game->local_client); player && player->ctf_team)
                player->ctf_team = 0; // CTF match state is never saved (D12).
            for (int i = 0; i < 30; ++i)
                CHECK(game->step(.05));
            std::map<std::string, unsigned> blocking, lot10;
            std::vector<HOBJECT> covered;
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
                const auto& n = r->type->name;
                if (save_rewrites::light_class(n) || n == "SoundFX" || n == "RandomSoundFX" || n == "DetailSprite" ||
                    n.starts_with("CClient"))
                {
                    ++lot10[save_rewrites::light_class(n) ? std::string("lights") : n];
                    ++totals[save_rewrites::light_class(n) ? std::string("lights") : n];
                }
            }
            auto list = [](const std::map<std::string, unsigned>& m)
            {
                std::string out;
                for (const auto& [type, count] : m)
                    out += (out.empty() ? "" : ", ") + type + " x" + std::to_string(count);
                return out.empty() ? std::string("none") : out;
            };
            std::cout << name << ": lot-10 objects " << list(lot10) << "; "
                      << (blocking.empty() ? "no blocking class" : "blocked by " + list(blocking)) << '\n';
            GameSave save;
            if (blocking.empty() && !game->save_game(save, error))
            {
                // Lot 13 refuses a CTF world by name: its team flags are multiplayer state.
                CHECK(error.find("CTF games cannot be saved") != std::string::npos);
                std::cout << "  whole save refused: CTF\n";
                continue;
            }
            if (blocking.empty())
            {
                SaveImage first = save.image;
                for (unsigned cycle = 0; cycle < 2; ++cycle)
                {
                    if (!GameSession::load_game(game, save, error))
                    {
                        std::cerr << name << " whole load refused at " << game->game_load.stage << ": " << error
                                  << '\n';
                        return false;
                    }
                    CHECK(no_duplicates(*game));
                    CHECK(game->save_game(save, error) && same_image(first, save.image, false));
                    CHECK(steps(*game, 30, .05) && game->server.unavailable_services.empty() && no_duplicates(*game));
                    CHECK(game->save_game(save, error));
                    first = save.image;
                }
                ++whole;
                whole_names.push_back(name);
                continue;
            }
            if (!game->server_shell.Player(game->local_client))
                continue; // A load needs exactly one saved local player (D6).
            const auto lights = game->server.light_effects.size(),
                       streams = game->server.particle_streams.active.size();
            std::size_t gates = 0;
            for (const auto& w : game->server.warp_gates.active)
                gates += game->server.IsObjectAlive(w.owner);
            SaveObjectsReport saved;
            CHECK(game->server.save_objects(current, covered, 0, save_flags, saved));
            save = {game->world_path, {}, *game->server.save_store.find(SaveStoreSlot::Current)};
            SaveError fingerprint;
            CHECK(
                build_fingerprint(game->resources, save.world_path, save.image.classes, save.fingerprint, fingerprint));
            if (!GameSession::load_game(game, save, error))
            {
                std::cerr << name << " Original-object load refused at " << game->game_load.stage << ": " << error
                          << '\n';
                return false;
            }
            CHECK(no_duplicates(*game));
            // Every saved light and stream is published again; every gate past its first update too.
            CHECK(game->server.light_effects.size() == lights &&
                  game->server.particle_streams.active.size() == streams);
            std::size_t republished = 0;
            for (auto h : game->server.last_restore.objects)
                if (const auto* w = get<CClientWarpGateSFX>(*game, h); w && !w->first_update)
                    ++republished;
            CHECK(game->server.warp_gates.active.size() == republished && republished == gates);
            for (auto h : game->server.last_restore.objects)
                if (const auto* s = get<SoundFX>(*game, h); s && s->state == SoundFX::On && s->ambient && s->sound)
                    CHECK(voices_of(*game, h) == 1);
            CHECK(steps(*game, 10, .05) && game->server.unavailable_services.empty() && no_duplicates(*game));
            std::cout << "  restored " << game->server.last_restore.records << " Original records: " << lights
                      << " lights, " << streams << " streams, " << gates << " warp gates published once\n";
            ++restored;
        }
        std::cout << entered << " of " << worlds.size() << " worlds entered; " << restored
                  << " restored their Original objects; " << whole << " restored whole";
        for (const auto& n : whole_names)
            std::cout << ' ' << n;
        std::cout << "\nlot-10 objects over all worlds:";
        for (const auto& [type, count] : totals)
            std::cout << ' ' << type << " x" << count;
        std::cout << '\n';
        CHECK(entered > 0 && restored + whole > 0);
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
        {"sounds", sounds},    {"random_sounds", random_sounds}, {"lights", lights},         {"explosions", explosions},
        {"streams", streams},  {"lightning", lightning},         {"warp_gates", warp_gates}, {"sprites", sprites},
        {"refusals", refusals}};
    if (mode == "private")
        return private_worlds(argv[2]) ? 0 : 1;
    const auto it = cases.find(mode);
    return it != cases.end() && it->second() ? 0 : 1;
}
