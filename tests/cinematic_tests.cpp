#include "audio_fixture.h"
#include "game/game_settings.h"
#include "game/original/CinematicTrigger.h"
#include <SDL3/SDL.h>
#include <algorithm>
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
    using test_audio::setup;
    using test_audio::wave;
    HOBJECT cine(GameSession& g, Properties p = {})
    {
        return g.objects.create(g.objects.find_class("CinematicTrigger"), {}, Creation::World, p);
    }
    CinematicTrigger* get(GameSession& g, HOBJECT h)
    {
        return dynamic_cast<CinematicTrigger*>(g.server.HandleToObject(h));
    }
    bool trigger(GameSession& g, HOBJECT h, const char* cmd = "TRIGGER")
    {
        auto str = g.server.CreateString(cmd);
        auto msg = g.server.StartMessageToObject(nullptr, h, 1000);
        bool ok = msg && g.server.WriteToMessageHString(msg, str) == DE_OK && g.server.EndMessage(msg) == DE_OK;
        g.server.FreeString(str);
        return ok;
    }
    bool decode(const std::filesystem::path&)
    {
        std::string error;
        AudioClip clip;
        for (unsigned channels : {1u, 2u})
            for (unsigned bits : {8u, 16u})
            {
                auto bytes = wave(channels, bits);
                CHECK(read_wave(bytes, clip, error));
                CHECK(clip.rate == 8000 && clip.channels == channels && clip.frames() == 800 && clip.duration() == .1);
                CHECK(std::abs(clip.samples[100] - .5f) < .005f);
            }
        // Observed retail PCM8 terminal padding variants; arbitrary trailing bytes still reject.
        auto odd = wave(1, 8, 8000, 801);
        CHECK(read_wave(odd, clip, error) && clip.frames() == 801);
        odd.push_back(0);
        CHECK(read_wave(odd, clip, error) && clip.frames() == 801);
        ++odd[4];
        CHECK(read_wave(odd, clip, error) && clip.frames() == 801);
        odd.push_back(0);
        CHECK(!read_wave(odd, clip, error));
        // A missing required inter-chunk pad never becomes an accepted terminal-pad variant.
        odd = wave(1, 8, 8000, 801);
        odd.insert(odd.end(), {'J', 'U', 'N', 'K', 0, 0, 0, 0});
        unsigned riff = unsigned(odd.size() - 8);
        for (unsigned i = 0; i < 4; ++i)
            odd[4 + i] = riff >> (8 * i);
        CHECK(!read_wave(odd, clip, error));
        CHECK(read_wave(wave(), clip, error));
        auto valid = wave();
        for (std::size_t size = 0; size < valid.size(); ++size)
            CHECK(!read_wave(std::span(valid).first(size), clip, error));
        CHECK(clip.frames() == 800); // Failure leaves the previous decoded resource intact.
        for (auto [pos, value] : std::vector<std::pair<unsigned, unsigned>>{
                 {20, 3}, {22, 0}, {22, 3}, {24, 1}, {32, 0}, {34, 24}, {16, 15}, {40, 255}})
        {
            auto bytes = valid;
            bytes[pos] = value;
            CHECK(!read_wave(bytes, clip, error));
        }
        // Real IMA ADPCM: one mono eight-byte block, predictor 0/index 0 and zero nibbles => nine zero samples.
        test_model::Writer w;
        for (char c : std::string("RIFF"))
            w.u8(c);
        w.u32(48);
        for (char c : std::string("WAVEfmt "))
            w.u8(c);
        w.u32(20);
        w.u16(17);
        w.u16(1);
        w.u32(8000);
        w.u32(7111);
        w.u16(8);
        w.u16(4);
        w.u16(2);
        w.u16(9);
        for (char c : std::string("data"))
            w.u8(c);
        w.u32(8);
        for (unsigned i = 0; i < 8; ++i)
            w.u8(0);
        CHECK(read_wave(w.b, clip, error) && clip.frames() == 9 && clip.encoding == 17);
        CHECK(std::all_of(clip.samples.begin(), clip.samples.end(), [](float x) { return x == 0; }));
        w.b[38] = 0;
        CHECK(!read_wave(w.b, clip, error));
        // MS ADPCM: first standard predictor, delta 16, zero seed samples and zero nibbles.
        w.b.clear();
        for (char c : std::string("RIFF"))
            w.u8(c);
        w.u32(78);
        for (char c : std::string("WAVEfmt "))
            w.u8(c);
        w.u32(50);
        w.u16(2);
        w.u16(1);
        w.u32(8000);
        w.u32(16000);
        w.u16(8);
        w.u16(4);
        w.u16(32);
        w.u16(4);
        w.u16(7);
        for (auto [a, b] : std::vector<std::pair<int, int>>{
                 {256, 0}, {512, -256}, {0, 0}, {192, 64}, {240, 0}, {460, -208}, {392, -232}})
        {
            w.u16(a);
            w.u16(b);
        }
        for (char c : std::string("data"))
            w.u8(c);
        w.u32(8);
        w.u8(0);
        w.u16(16);
        w.u16(0);
        w.u16(0);
        w.u8(0);
        CHECK(read_wave(w.b, clip, error) && clip.frames() == 4 && clip.encoding == 2);
        CHECK(std::all_of(clip.samples.begin(), clip.samples.end(), [](float x) { return x == 0; }));
        w.b[36] = 2;
        CHECK(!read_wave(w.b, clip, error));
        w.b[36] = 32;
        w.b[40] = 0;
        CHECK(!read_wave(w.b, clip, error));

        return true;
    }
    bool mixer(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = cine(g);
        CHECK(h);
        SoundRequest r;
        r.resource = "cue.wav";
        r.space = SoundSpace::Local;
        std::string error;
        g.server.audio.inspect_pcm = true;
        auto id = g.server.PlayCinematicSound(h, r, error);
        CHECK(id && g.server.audio.cached_clips() == 1);
        bool done = true;
        CHECK(g.server.IsSoundDone(id, done) && !done);
        CHECK(g.server.audio.advance(.05, {}, {}));
        CHECK(g.server.audio.mixed.size() == 4800 && std::abs(g.server.audio.peak - .5f) < .005f);
        CHECK(g.server.IsSoundDone(id, done) && !done);
        CHECK(g.server.audio.advance(.05, {}, {}));
        CHECK(g.server.IsSoundDone(id, done) && done);
        CHECK(g.server.KillSound(id) && !g.server.KillSound(id) && !g.server.IsSoundDone(id, done));
        auto next = g.server.PlayCinematicSound(h, r, error);
        CHECK(next > id);
        CHECK(g.unload_world() && !g.server.audio.cached_clips() && g.server.audio.voices().empty() &&
              !g.server.audio.get(next));
        CHECK(cine(g));
        AudioMixer other;
        auto fresh = other.play(g.resources, nullptr, r, {}, error);
        CHECK(fresh > next && !other.get(next));
        return true;
    }
    bool spatial(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = cine(g);
        CHECK(h);
        auto speaker =
            g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World, {{"Pos", DVector{500, 0, 0}}});
        CHECK(speaker);
        SoundRequest r;
        r.resource = "long.wav";
        r.speaker = speaker;
        std::string error;
        auto id = g.server.PlayCinematicSound(h, r, error);
        CHECK(id);
        g.server.audio.inspect_pcm = true;
        CHECK(g.server.advance_audio(.01, {}, {}));
        auto* v = g.server.audio.get(id);
        CHECK(v && v->left == 0 && v->right > .66f && v->right < .67f);
        DVector pos{-500, 0, 0};
        CHECK(g.server.TeleportObject(speaker, &pos) == DE_OK);
        CHECK(g.server.advance_audio(.01, {}, {}));
        CHECK(v->right == 0 && v->left > .66f && v->position.x == -500);
        CHECK(g.server.advance_audio(.01, {}, rotation_from_euler({0, 3.14159265f, 0})));
        CHECK(v->right > .66f && v->left < .001f);
        CHECK(g.server.advance_audio(.01, {-500, 0, 0}, {}));
        CHECK(std::abs(v->left - .7071067f) < .001f);
        CHECK(g.objects.remove(speaker));
        CHECK(g.server.advance_audio(.01, {}, {}));
        CHECK(v->done && v->speaker_lost);
        CHECK(g.objects.finish_frame() && g.unload_world() && g.server.audio.voices().empty());
        return true;
    }
    bool sequence(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto target =
            g.objects.create(g.objects.find_class("CameraObj"), {}, Creation::World, {{"Name", std::string("camera")}});
        CHECK(target);
        auto h = cine(g, {{"Sound1", std::string("cue.wav")},
                          {"Delay1", .05f},
                          {"WhoPlaysSound1", std::string("PlAyEr")},
                          {"StartSoundTriggerTarget1", std::string("camera")},
                          {"StartSoundTriggerMsg1", std::string("ON")},
                          {"Sound2", std::string("cue.wav")},
                          {"Delay2", .2f},
                          {"WhoPlaysSound2", std::string("missing")}});
        auto* c = get(g, h);
        CHECK(c && !g.objects.get(h)->scheduled && c->current == 0);
        CHECK(trigger(g, h, "OFF")); // Payload is intentionally ignored in original code.
        CHECK(g.step(.05) && !c->sound && !c->starts);
        CHECK(g.step(.001) && c->sound && c->current == 0 && c->starts == 1 && c->messages == 1);
        CHECK(g.server.audio.get(c->sound)->request.space == SoundSpace::Local);
        const auto voice = c->sound;
        CHECK(trigger(g, h, "ON") && c->sound == voice && c->current == 0);
        CHECK(g.step(.1) && !c->sound && c->current == 1);
        CHECK(g.step(.2) && !c->sound);
        CHECK(g.step(.001) && c->sound && c->starts == 2 && c->last_speaker == h);
        CHECK(!g.server.list_count() && !g.server.string_count());
        CHECK(g.step(.1) && c->current == 2 && !c->sound);
        CHECK(g.step(.001) && !g.objects.get(h) && g.server.audio.voices().empty());
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = cine(g, {{"Name", std::string("self")},
                          {"Sound1", std::string("cue.wav")},
                          {"StartSoundTriggerTarget1", std::string("self")},
                          {"StartSoundTriggerMsg1", std::string("REMOVE")}});
        CHECK(h && trigger(g, h) && g.step(.01) && !g.objects.get(h) && g.server.audio.voices().empty());
        auto speaker = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::World,
                                        {{"Name", std::string("speaker")}});
        CHECK(speaker);
        h = cine(g, {{"Sound1", std::string("long.wav")}, {"WhoPlaysSound1", std::string("speaker")}});
        CHECK(h && trigger(g, h) && g.step(.01));
        auto* c = get(g, h);
        auto id = c->sound;
        CHECK(id && c->last_speaker == speaker && !g.server.list_count());
        CHECK(g.objects.remove(speaker) && g.step(.01) && !c->sound && c->current == 1);
        CHECK(g.step(.01) && !g.objects.get(h) && !g.server.audio.get(id));
        h = cine(g, {{"Sound1", std::string("long.wav")}});
        CHECK(h && trigger(g, h) && g.step(.01));
        id = get(g, h)->sound;
        CHECK(id);
        CHECK(g.objects.remove(h) && g.objects.finish_frame() && !g.server.audio.get(id));
        CHECK(g.unload_world() && !g.server.message_count() && !g.server.list_count() && !g.server.string_count());
        return true;
    }
    bool failures(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        auto h = cine(g, {{"Sound1", std::string("missing.wav")}});
        CHECK(h && trigger(g, h));
        auto* c = get(g, h);
        CHECK(g.step(.01) && c->starts == 1 && c->failures == 1 && !c->sound && c->current == 0 && !c->error.empty());
        CHECK(g.step(.01) && c->starts == 2 && c->failures == 2 &&
              c->current == 0); // No fabricated duration/advancement.
        CHECK(!cine(g, {{"Delay1", -1.0f}}));
        CHECK(!g.objects.create(g.objects.find_class("CinematicTrigger"), {}, Creation::Restore));
        g.server.set_game_settings(1, GAMETYPE_ACTION);
        auto filtered = cine(g);
        CHECK(filtered && g.objects.get(filtered)->life == Life::Pending && g.step(0) && !g.objects.get(filtered));
        SoundRequest r;
        r.resource = "cue.wav";
        r.space = SoundSpace::Local;
        std::string error;
        CHECK(!g.server.PlayCinematicSound(nullptr, r, error));
        r.speaker = h;
        CHECK(!g.server.PlayCinematicSound(h, r, error));
        r.speaker = nullptr;
        r.volume = NAN;
        CHECK(!g.server.PlayCinematicSound(h, r, error));
        r.volume = 1;
        std::vector<SoundId> ids;
        for (unsigned i = 0; i < 128; ++i)
        {
            auto id = g.server.PlayCinematicSound(h, r, error);
            CHECK(id);
            ids.push_back(id);
        }
        CHECK(!g.server.PlayCinematicSound(h, r, error) && g.server.audio.voices().size() == 128);
        for (auto id : ids)
            CHECK(g.server.KillSound(id));
        CHECK(g.server.audio.advance(1, {}, {}) && g.server.audio.mixed.empty());
        g.server.audio.inspect_pcm = true;
        CHECK(g.server.audio.advance(1, {}, {}) && g.server.audio.mixed.size() == 24000 &&
              g.server.audio.dropped_frames == 36000);
        CHECK(!g.server.audio.advance(NAN, {}, {}));
        return true;
    }
    bool device(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(setup(g, root));
        CHECK(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
        std::string error;
        CHECK(g.server.audio.open_output(error));
        auto h = cine(g);
        SoundRequest r;
        r.resource = "cue.wav";
        r.space = SoundSpace::Local;
        CHECK(g.server.PlayCinematicSound(h, r, error));
        CHECK(g.server.advance_audio(.01, {}, {}));
        CHECK(g.server.audio.has_output() && g.server.audio.submitted_frames == 480 && g.server.audio.peak > .49f &&
              g.server.audio.output_error.empty());
        CHECK(g.unload_world() && g.server.audio.voices().empty() && g.server.audio.submitted_frames == 0);
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
            for (const auto& e : g.entries)
                ++counts[e.state];
            CHECK(counts[EntryState::Created] == 187 && counts[EntryState::EditorOnly] == 372 &&
                  counts[EntryState::Unsupported] == 17 && counts[EntryState::Failed] == 0);
            CHECK(g.objects.snapshot().size() == 194 && g.enter_player(error) && g.objects.snapshot().size() == 197);
            HOBJECT target = nullptr;
            for (auto id : g.objects.snapshot())
                if (std::string_view(g.objects.get(id)->state.m_Name) == "cintrig1")
                    target = g.objects.get(id)->handle;
            CHECK(target && trigger(g, target) && g.step(1.0 / 60));
            auto* c = get(g, target);
            CHECK(c && c->sound && c->starts == 1 && !c->failures);
            auto id = c->sound;
            CHECK(id > previous && !g.server.audio.get(previous));
            previous = id;
            const auto* v = g.server.audio.get(id);
            CHECK(v && v->request.space == SoundSpace::Local && v->clip->duration() > 0);
            std::cout << "cycle=" << cycle << " cue=" << v->request.resource << " rate=" << v->clip->rate
                      << " frames=" << v->clip->frames() << " duration=" << v->clip->duration() << '\n';
            g.server.audio.inspect_pcm = true;
            for (unsigned i = 0; i < 6; ++i)
                CHECK(g.step(1.0 / 60));
            v = g.server.audio.get(id);
            CHECK(v && v->cursor > 0 && g.server.audio.mixed_frames == 4800 && g.objects.snapshot().size() == 188);
            CHECK(g.unload_world() && g.objects.snapshot().empty() && g.server.audio.voices().empty() &&
                  !g.server.audio.cached_clips() && !g.server.audio.get(id) && !g.server.message_count() &&
                  !g.server.list_count() && !g.server.string_count());
            g.server.audio.inspect_pcm = false;
        }
        // Run the two authored crash cues to completion in isolation, preserving their exact DAT properties.
        std::vector<std::uint8_t> bytes;
        World world;
        CHECK(g.resources.read("worlds/01_subwaya.dat", bytes, error) && read_world(bytes, world, error));
        HOBJECT target = nullptr;
        for (const auto& o : world.objects)
            if (o.class_name == "CinematicTrigger")
            {
                Properties props;
                for (const auto& prop : o.properties)
                    props.emplace(prop.name, prop.value);
                auto h = cine(g, props);
                CHECK(h);
                if (std::string_view(g.objects.get(h)->state.m_Name) == "cinhi5")
                    target = h;
            }
        CHECK(target && trigger(g, target));
        unsigned cue_changes = 0;
        std::size_t last = 0;
        for (unsigned i = 0; i < 2400 && g.objects.get(target); ++i)
        {
            CHECK(g.step(.05));
            if (auto* c = get(g, target))
            {
                CHECK(!c->failures);
                if (c->current != last)
                {
                    last = c->current;
                    ++cue_changes;
                }
            }
        }
        CHECK(!g.objects.get(target) && cue_changes == 2 && g.server.audio.voices().empty());
        CHECK(g.unload_world());
        std::cout << "two authored crash cues completed; 2 full session reloads clean\n";
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
        unsigned worlds = 0, controllers = 0, cues = 0, missing = 0, invalid = 0;
        std::map<unsigned, unsigned> encodings;
        for (const auto& [path, resource] : g.resources.index())
        {
            (void)resource;
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
                if (o.class_name == "CinematicTrigger")
                {
                    ++controllers;
                    Properties props;
                    for (const auto& prop : o.properties)
                        props.emplace(prop.name, prop.value);
                    auto h = cine(g, props);
                    CHECK(h);
                    auto* c = get(g, h);
                    CHECK(c);
                    for (const auto& cue : c->cues)
                        if (!cue.sound.empty())
                        {
                            ++cues;
                            std::shared_ptr<const AudioClip> clip;
                            if (!g.server.audio.load(g.resources, cue.sound, clip, error))
                            {
                                if (!g.resources.find(cue.sound))
                                    ++missing;
                                else
                                    ++invalid;
                                std::cout << path << " sound=" << cue.sound << " error=" << error << '\n';
                            }
                            else
                                ++encodings[clip->encoding];
                        }
                    if (path == "worlds/01_subwaya.dat")
                    {
                        std::cout << "SUBWAY " << g.objects.get(h)->state.m_Name << '\n';
                        for (const auto& cue : c->cues)
                            if (!cue.sound.empty())
                                std::cout << "  " << cue.delay << ' ' << cue.sound << " speaker=" << cue.speaker
                                          << " message=" << cue.target << ':' << cue.message << '\n';
                    }
                }
            CHECK(g.objects.clear());
            g.server.audio.clear();
        }
        std::cout << "worlds=" << worlds << " controllers=" << controllers << " cues=" << cues << " missing=" << missing
                  << " invalid=" << invalid << '\n';
        for (auto [type, count] : encodings)
            std::cout << "encoding=" << type << " cues=" << count << '\n';
        CHECK(worlds == 57 && controllers == 79 && cues == 201 && missing == 3 && !invalid && encodings[1] == 198);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"decode", decode},     {"mixer", mixer},       {"spatial", spatial},
        {"sequence", sequence}, {"lifetime", lifetime}, {"failures", failures},
        {"device", device},     {"catalog", catalog},   {"subway", subway}};
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
