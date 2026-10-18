#include "audio_fixture.h"
#include "engine/audio_stream.h"
#include "game/original/Sound.h"
#include <cmath>
#include <functional>
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
    HOBJECT make(GameSession& g, Properties p = {})
    {
        return g.objects.create(g.objects.find_class("Sound"), {}, Creation::World, p);
    }
    Sound* get(GameSession& g, HOBJECT h)
    {
        auto* r = g.objects.get(h);
        return r ? dynamic_cast<Sound*>(r->game.get()) : nullptr;
    }
    bool defaults(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = make(g, {{"Filename", std::string("cue.wav")}});
        auto* c = get(g, h);
        CHECK(c && c->sound && c->request.loop && !c->file_stream && !c->request.reverb_requested);
        CHECK(c->request.space == SoundSpace::Ambient && c->request.volume == 1 && c->request.inner_radius == 10 &&
              c->request.outer_radius == 100);
        CHECK(g.objects.get(h)->life == Life::Pending && g.objects.get(h)->type->parent->name == "BaseClass");
        auto id = c->sound;
        CHECK(g.objects.finish_frame() && !g.objects.get(h) && g.server.audio.get(id));
        for (auto volume : {-1, 100, 255, 256, 300})
        {
            h = make(g, {{"Filename", std::string("cue.wav")},
                         {"Volume", std::int32_t(volume)},
                         {"Priority", std::int32_t(300)},
                         {"Ambient", false}});
            c = get(g, h);
            CHECK(c && c->sound && c->request.volume == std::min<unsigned>(DBYTE(volume), 100) / 100.0f);
            CHECK(c->request.priority == 44 && c->request.space == SoundSpace::Spatial);
        }
        h = g.objects.create(g.objects.find_class("Sound"), {}, Creation::Runtime);
        c = get(g, h);
        CHECK(c && !c->sound && c->request.resource.empty() && !c->error.empty());
        CHECK(!g.objects.create(g.objects.find_class("Sound"), {}, Creation::Restore));
        CHECK(g.objects.finish_frame());
        return true;
    }
    bool loops(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = make(g, {{"Filename", std::string("cue.wav")}});
        CHECK(h);
        auto id = get(g, h)->sound;
        CHECK(id && g.objects.finish_frame());
        g.server.audio.inspect_pcm = true;
        CHECK(g.server.advance_audio(.25, {}, {}));
        auto* v = g.server.audio.get(id);
        CHECK(v && !v->done && v->world_owned && v->loops == 2 && std::abs(v->cursor - 400) < 1e-6);
        CHECK(g.server.audio.mixed.size() == 24000 && std::abs(g.server.audio.peak - .5f) < 1e-6);
        for (float f : g.server.audio.mixed)
            CHECK(std::abs(f - .5f) < 1e-6); // No silent frames at either wrap.
        CHECK(g.server.advance_audio(60, {}, {}));
        CHECK(v->loops == 602 && std::abs(v->cursor - 400) < 1e-6);
        CHECK(g.server.audio.mixed_frames == 24000 && g.server.audio.dropped_frames == 2868000);
        CHECK(g.server.advance_audio(.05, {}, {}));
        CHECK(v->loops == 603 && v->cursor == 0 && !v->done);
        bool done = true;
        CHECK(g.server.IsSoundDone(id, done) && !done);
        g.server.audio.inspect_pcm = false;
        CHECK(g.server.advance_audio(1, {}, {}));
        CHECK(v->loops == 613 && v->cursor == 0);
        CHECK(!g.server.advance_audio(61, {}, {}));
        CHECK(v->loops == 613 && v->cursor == 0);
        return true;
    }
    bool resampling(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto h = make(g, {{"Filename", std::string("cue.wav")}});
        auto id = get(g, h)->sound;
        CHECK(id && g.objects.finish_frame());
        // Mutate only a private synthetic clip to make the last-to-first interpolation observable.
        auto* v = g.server.audio.get(id);
        auto clip = std::make_shared<AudioClip>();
        clip->rate = 8000;
        clip->channels = 2;
        clip->samples = {0, 1, 1, 0};
        v->clip = clip;
        g.server.audio.inspect_pcm = true;
        CHECK(g.server.advance_audio(1.0 / 48000, {}, {}));
        CHECK(g.server.audio.mixed[0] == 0 && g.server.audio.mixed[1] == 1);
        CHECK(g.server.advance_audio(8.0 / 48000, {}, {}));
        // Last produced cursor 8/6=1.333: interpolate frame 1 toward frame 0 across the loop seam.
        CHECK(std::abs(g.server.audio.mixed[14] - 2.0f / 3) < 1e-6 &&
              std::abs(g.server.audio.mixed[15] - 1.0f / 3) < 1e-6);
        CHECK(v->loops == 0 && std::abs(v->cursor - 1.5) < 1e-6);
        CHECK(g.server.advance_audio(3.0 / 48000, {}, {}));
        CHECK(v->loops == 1 && v->cursor == 0);
        CHECK(g.unload_world());
        // A one-frame loop remains valid and never indexes outside its sole sample.
        h = make(g, {{"Filename", std::string("cue.wav")}});
        id = get(g, h)->sound;
        CHECK(id && g.objects.finish_frame());
        v = g.server.audio.get(id);
        clip = std::make_shared<AudioClip>();
        clip->rate = 96000;
        clip->channels = 1;
        clip->samples = {.25f};
        v->clip = clip;
        CHECK(g.server.advance_audio(.001, {}, {}));
        CHECK(v->loops == 96 && v->cursor == 0 && g.server.audio.mixed.size() == 96);
        for (float f : g.server.audio.mixed)
            CHECK(f == .25f);
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        SoundId last = 0;
        for (unsigned cycle = 0; cycle < 3; ++cycle)
        {
            auto h = make(g, {{"Filename", std::string("cue.wav")},
                              {"Pos", DVector{75, 0, 0}},
                              {"InnerRadius", 50.0f},
                              {"Volume", std::int32_t(50)}});
            CHECK(h);
            auto id = get(g, h)->sound;
            CHECK(id > last && !g.server.audio.get(last));
            last = id;
            CHECK(g.objects.finish_frame() && !g.objects.get(h) && g.server.advance_audio(.35, {}, {}));
            const auto* v = g.server.audio.get(id);
            CHECK(v && v->loops == 3 && v->position.x == 75 && v->left == .25f && v->right == .25f);
            CHECK(g.server.advance_audio(.1, {}, {{0, 1, 0}, 0}) && v->left == .25f && v->right == .25f);
            SoundRequest r;
            r.resource = "cue.wav";
            r.loop = true;
            std::string error;
            CHECK(!g.server.PlayWorldSound(h, r, false, error)); // Pending/stale source cannot issue another voice.
            auto owner = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::Runtime);
            CHECK(owner);
            auto owned = g.server.PlaySoundAt(owner, r, {}, error);
            CHECK(owned);
            CHECK(g.objects.remove(owner) && g.server.advance_audio(.1, {}, {}));
            CHECK(!g.server.audio.get(owned) && g.server.audio.get(id) && g.objects.finish_frame());
            CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.audio.cached_clips() &&
                  !g.server.KillSound(id));
            CHECK(g.objects.snapshot().empty() && !g.server.message_count() && !g.server.string_count() &&
                  !g.server.list_count());
        }
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        for (const auto& p : std::vector<Properties>{{{"Filename", std::string("absent.wav")}},
                                                     {{"Filename", std::string("cue.wav")}, {"InnerRadius", 150.0f}},
                                                     {{"Filename", std::string("cue.wav")}, {"OuterRadius", -1.0f}}})
        {
            auto h = make(g, p);
            auto* c = get(g, h);
            CHECK(c && !c->sound && !c->error.empty());
            CHECK(g.objects.finish_frame() && !g.objects.get(h) && g.server.audio.voices().empty());
        }
        CHECK(!make(g, {{"VoiceType", std::int32_t(1)}})); // Nonzero historical behavior is not guessed.
        CHECK(g.objects.finish_frame());
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::Runtime);
        CHECK(owner);
        SoundRequest r;
        r.resource = "cue.wav";
        std::string error;
        CHECK(!g.server.PlayWorldSound(owner, r, false, error));
        r.loop = true;
        r.space = SoundSpace::Local;
        CHECK(!g.server.PlayWorldSound(owner, r, false, error));
        r.space = SoundSpace::Ambient;
        r.speaker = owner;
        CHECK(!g.server.PlayWorldSound(owner, r, false, error));
        r.speaker = nullptr;
        for (unsigned i = 0; i < 128; ++i)
            CHECK(g.server.PlayWorldSound(owner, r, false, error));
        CHECK(!g.server.PlayWorldSound(owner, r, false, error) && g.server.audio.voices().size() == 128);
        CHECK(g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool metadata(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        for (const auto& tag : {std::string("cue "), std::string("smpl"), std::string("LIST")})
        {
            test_model::Writer w;
            w.b = test_audio::wave();
            for (char c : tag)
                w.u8(c);
            w.u32(4);
            for (char c : std::string("adtl"))
                w.u8(c);
            w.patch(4, w.b.size() - 8);
            AudioClip clip;
            std::string error;
            CHECK(read_wave(w.b, clip, error) && clip.loop_metadata);
            auto h = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::Runtime);
            CHECK(h);
            test_player::Directory dir;
            dir.files["region.wav"] = w.b;
            test_model::Writer rez;
            rez.b.resize(168);
            rez.b[0] = rez.b[62] = rez.b[124] = 13;
            rez.b[1] = rez.b[63] = rez.b[125] = 10;
            rez.b[126] = 26;
            rez.patch(127, 1);
            auto [offset, size] = test_player::write_dir(rez, dir);
            rez.patch(131, offset);
            rez.patch(135, size);
            const auto path = root / (tag + ".rez");
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(rez.b.data()), rez.b.size());
            out.close();
            CHECK(g.resources.mount(path, error));
            SoundRequest r;
            r.resource = "region.wav";
            r.loop = true;
            CHECK(!g.server.PlayWorldSound(h, r, false, error) && error.find("region-loop") != std::string::npos);
            CHECK(!g.server.PlayWorldSound(h, r, true, error) && error.find("region-loop") != std::string::npos);
            r.loop = false;
            CHECK(g.server.PlaySoundAt(h, r, {}, error));
            r.streaming = true;
            CHECK(g.server.PlaySoundAt(h, r, {}, error));
            w.b.pop_back();
            AudioClip untouched;
            untouched.rate = 123;
            CHECK(!read_wave(w.b, untouched, error) && untouched.rate == 123);
            CHECK(g.unload_world());
        }
        return true;
    }
    bool catalog(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        unsigned worlds = 0, sounds = 0, streams = 0, missing = 0, invalid = 0, metadata = 0, started = 0, rejected = 0,
                 failed = 0;
        for (const auto& [path, res] : g.resources.index())
        {
            (void)res;
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            std::vector<std::uint8_t> bytes;
            World w;
            CHECK(g.resources.read(path, bytes, error));
            if (!read_world(bytes, w, error))
            {
                CHECK(path == "worlds/16_bridge.dat");
                continue;
            }
            ++worlds;
            for (const auto& o : w.objects)
                if (o.class_name == "Sound")
                {
                    ++sounds;
                    Properties p;
                    for (const auto& prop : o.properties)
                        p.emplace(prop.name, prop.value);
                    auto file = std::get<std::string>(p.at("Filename"));
                    bool stream = std::get<bool>(p.at("FileStream"));
                    streams += stream;
                    std::shared_ptr<const AudioClip> clip;
                    if (!g.server.audio.load(g.resources, file, clip, error))
                    {
                        if (!g.resources.find(file))
                            ++missing;
                        else
                            ++invalid;
                        std::cout << path << ' ' << file << " decode=" << error << '\n';
                    }
                    else
                        metadata += clip->loop_metadata;
                    auto h = make(g, p);
                    auto* c = get(g, h);
                    if (!c)
                    {
                        ++rejected;
                        std::cout << path << ' ' << file << " schema rejection\n";
                    }
                    else if (!c->sound)
                    {
                        ++failed;
                        std::cout << path << ' ' << file << " playback=" << c->error << '\n';
                    }
                    else
                    {
                        ++started;
                        CHECK(g.server.audio.get(c->sound)->world_owned);
                        const auto* voice = g.server.audio.get(c->sound);
                        CHECK(voice->request.streaming == stream && voice->clip->frames() == clip->frames());
                        if (stream)
                        {
                            CHECK(voice->clip->stream && voice->clip->samples.empty());
                            for (auto frame : {std::size_t(0), clip->frames() / 2, clip->frames() - 1})
                                for (unsigned ch = 0; ch < clip->channels; ++ch)
                                {
                                    float sample = 0;
                                    CHECK(voice->clip->stream->sample(frame, ch, sample, error));
                                    CHECK(std::abs(sample - clip->samples[frame * clip->channels + ch]) < 1e-6f);
                                }
                        }
                        if (path == "worlds/01_subwaya.dat")
                            std::cout << path << ' ' << file << " world loop=" << c->sound
                                      << " frames=" << clip->frames() << " rate=" << clip->rate << '\n';
                    }
                    CHECK(g.objects.finish_frame());
                }
            CHECK(g.unload_world());
        }
        std::cout << "worlds=" << worlds << " sounds=" << sounds << " streams=" << streams << " missing=" << missing
                  << " invalid=" << invalid << " metadata=" << metadata << " started=" << started
                  << " rejected=" << rejected << " failed=" << failed << '\n';
        CHECK(worlds == 57 && sounds == 792 && streams == 302 && missing == 15 && !invalid && !metadata);
        CHECK(started == 769 && rejected == 0 && failed == 23); // Equal authored radii play.
        return true;
    }
    bool subway(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        SoundId previous = 0;
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("01_SubwayA", error));
            std::map<EntryState, unsigned> counts;
            std::vector<SoundId> ids;
            for (size_t i = 0; i < g.entries.size(); ++i)
            {
                const auto& e = g.entries[i];
                ++counts[e.state];
                if (g.world.objects[i].class_name == "Sound")
                {
                    CHECK(e.state == EntryState::WorldSound && !e.object && e.sound > previous);
                    ids.push_back(e.sound);
                    previous = e.sound;
                    const auto* v = g.server.audio.get(e.sound);
                    CHECK(v && v->world_owned && v->request.loop && !v->request.speaker && !g.objects.get(v->owner));
                    CHECK(v->request.space == SoundSpace::Ambient && v->request.volume == .8f &&
                          v->request.inner_radius == 700 && v->request.outer_radius == 1200);
                    CHECK(v->clip->rate == 22050 && v->clip->frames() == 198623 && v->clip->channels == 1);
                }
            }
            CHECK(ids.size() == 4 && counts[EntryState::Created] == 204 && counts[EntryState::WorldSound] == 4 &&
                  counts[EntryState::Unsupported] == 0 && counts[EntryState::EditorOnly] == 372 &&
                  !counts[EntryState::Failed]);
            CHECK(g.objects.snapshot().size() == 227 && g.server.audio.voices().size() == 4 &&
                  g.server.audio.cached_clips() == 1);
            CHECK(g.enter_player(error) && g.objects.snapshot().size() == 230 && g.step(1.0 / 60));
            // Advance actual sample clocks across a full retail clip without adding unrelated physics work.
            CHECK(g.server.advance_audio(20, {}, {}));
            for (auto id : ids)
            {
                const auto* v = g.server.audio.get(id);
                CHECK(v && !v->done && v->loops == 2 && v->cursor > 0);
            }
            g.server.audio.inspect_pcm = true;
            const auto pos = g.server.audio.get(ids.front())->position;
            CHECK(g.server.advance_audio(.02, pos, {}) && g.server.audio.peak > 0 &&
                  g.server.audio.mixed_frames == 960);
            CHECK(g.run_world(error) && error.empty() && g.state == SessionState::Ready);
            CHECK(g.unload_world() && g.objects.snapshot().empty() && g.server.audio.voices().empty() &&
                  !g.server.audio.cached_clips());
            for (auto id : ids)
                CHECK(!g.server.audio.get(id));
            g.server.audio.inspect_pcm = false;
            std::cout << "cycle=" << cycle
                      << " 4 world loops survived source removal, wrapped twice, produced PCM and unloaded\n";
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"defaults", defaults}, {"loops", loops},       {"resampling", resampling}, {"lifetime", lifetime},
        {"failures", failures}, {"metadata", metadata}, {"catalog", catalog},       {"subway", subway}};
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
