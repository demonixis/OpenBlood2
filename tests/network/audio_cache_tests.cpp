#include "audio_fixture.h"
#include "client/announcer_data.h"
#include "client/network_audio.h"
#include <iostream>
#include <set>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x ": " << error << '\n';                                                    \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    const Joined joined{7, 3, {4, 2}};
    std::string name(unsigned i)
    {
        return "tone" + std::to_string(i) + ".wav";
    }
    bool aliases(Resources& resources, const std::filesystem::path& root, unsigned count, bool large,
                 std::string& error)
    {
        CHECK(test_network_audio::setup(resources, root, error));
        std::vector<std::uint8_t> small;
        CHECK(resources.read("tone.wav", small, error));
        test_model::Writer wave;
        wave.b = small;
        if (large)
        {
            wave.b.resize(16 * 1024 * 1024);
            for (std::size_t i = 44; i < wave.b.size(); i += 2)
            {
                wave.b[i] = 0;
                wave.b[i + 1] = 64;
            }
            wave.patch(4, unsigned(wave.b.size() - 8));
            wave.patch(40, unsigned(wave.b.size() - 44));
        }
        test_model::Writer rez;
        rez.b.resize(168);
        rez.b[0] = rez.b[62] = rez.b[124] = 13;
        rez.b[1] = rez.b[63] = rez.b[125] = 10;
        rez.b[126] = 26;
        rez.patch(127, 1);
        rez.b.insert(rez.b.end(), wave.b.begin(), wave.b.end());
        const auto offset = unsigned(rez.b.size());
        for (unsigned i = 0; i < count; ++i)
        {
            rez.u32(0);
            rez.u32(168);
            rez.u32(unsigned(wave.b.size()));
            rez.u32(0);
            rez.u32(i + 1);
            rez.u32(('w' << 16) | ('a' << 8) | 'v');
            rez.u32(0);
            const auto stem = "tone" + std::to_string(i);
            rez.b.insert(rez.b.end(), stem.begin(), stem.end());
            rez.u8(0);
            rez.u8(0);
        }
        rez.patch(131, offset);
        rez.patch(135, unsigned(rez.b.size()) - offset);
        std::ofstream file(root / "aliases.rez", std::ios::binary);
        file.write(reinterpret_cast<const char*>(rez.b.data()), rez.b.size());
        file.close();
        CHECK(file.good() && resources.mount(root / "aliases.rez", error));
        return true;
    }
    SoundParameters parameters(unsigned i, bool streaming = false)
    {
        SoundParameters p;
        p.resource = name(i);
        p.space = SoundSpace::Local;
        p.streaming = streaming;
        p.volume = .25f;
        return p;
    }
    SoundStart start(unsigned id, unsigned resource)
    {
        SoundStart s;
        s.id = id;
        s.parameters = parameters(resource);
        s.parameters.loop = true;
        s.rate = s.frames = 8000;
        s.channels = 1;
        return s;
    }
    bool count(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(aliases(resources, root, 520, false, error));
        NetworkAudio audio;
        CHECK(audio.begin(joined));
        audio.mixer.inspect_pcm = true;
        ClientWorld world;
        CHECK(world.begin(joined.world));
        CHECK(audio.receive({7, 3, 1, start(1, 0)}, resources, error));
        const auto local = audio.play_voice(resources, parameters(1, true), error);
        CHECK(local && audio.advance(.05, world, {}, {}));
        const auto server_clip = audio.mixer.get(1)->clip;
        const auto local_clip = audio.mixer.get(local)->clip;
        CHECK(audio.mixer.get(1)->cursor == 400 && audio.mixer.get(local)->cursor == 400);
        for (unsigned i = 2; i < 512; ++i)
        {
            const auto id = audio.play_voice(resources, parameters(i, i % 2), error);
            CHECK(id && audio.stop_voice(id));
            CHECK(audio.mixer.cached_clips() <= 128 && audio.mixer.cached_bytes() <= 256 * 1024 * 1024);
        }
        CHECK(audio.mixer.cache_evictions() == 384 && audio.mixer.cached_clips() == 128);
        CHECK(audio.mixer.get(1)->clip == server_clip && audio.mixer.get(local)->clip == local_clip);
        CHECK(audio.mixer.get(1)->cursor == 400 && audio.mixer.get(local)->cursor == 400);
        CHECK(audio.advance(.05, world, {}, {}) && audio.mixer.peak == .25f);
        CHECK(audio.mixer.get(1)->cursor == 800 && audio.mixer.get(local)->cursor == 800);
        float duration = 0;
        bool done = true;
        CHECK(audio.voice_duration(local, duration) && duration == 1 && audio.voice_done(local, done) && !done);
        CHECK(audio.receive({7, 3, 2, start(2, 512)}, resources, error) && audio.mixer.cache_evictions() == 385);
        CHECK(audio.receive({7, 3, 2, start(2, 512)}, resources, error) && audio.mixer.cache_evictions() == 385);
        CHECK(audio.receive({7, 3, 3, SoundEnd{2, false}}, resources, error));
        const auto retry =
            audio.play_voice(resources, parameters(2), error); // Previously evicted buffered clip reloads.
        CHECK(retry && audio.stop_voice(retry));
        CHECK(audio.stop_voice(local) && audio.receive({7, 3, 4, SoundEnd{1, false}}, resources, error));
        CHECK(audio.mixer.voices().empty() && !audio.local_failed);
        audio.clear();
        CHECK(!audio.mixer.cached_clips() && !audio.mixer.cached_bytes() && !audio.mixer.cache_evictions());

        // Default logical/solo policy remains strictly retaining, even with no live voice.
        AudioTimeline logical;
        for (unsigned i = 0; i < 128; ++i)
        {
            std::shared_ptr<const AudioClip> clip;
            CHECK(logical.load(resources, name(i), clip, error));
        }
        std::shared_ptr<const AudioClip> extra;
        CHECK(!logical.load(resources, name(128), extra, error) && !extra && !logical.cache_evictions());
        CHECK(logical.cached_clips() == 128);

        // Prepared external handles and completed-but-retained voices remain pinned.
        NetworkAudio pinned;
        CHECK(pinned.begin(joined));
        std::vector<SoundId> voices;
        for (unsigned i = 0; i < 128; ++i)
        {
            auto id = pinned.play_voice(resources, parameters(i, true), error);
            CHECK(id);
            voices.push_back(id);
        }
        auto retained = pinned.mixer.get(voices[0])->clip;
        CHECK(pinned.advance(1, world, {}, {}));
        CHECK(pinned.voice_done(voices[0], done) && done);
        CHECK(pinned.stop_voice(voices[0]));
        CHECK(!pinned.play_voice(resources, parameters(128), error) && !pinned.mixer.cache_evictions());
        retained.reset();
        auto id = pinned.play_voice(resources, parameters(128), error);
        CHECK(id && pinned.mixer.cache_evictions() == 1 && pinned.mixer.cached_clips() == 128);
        CHECK(pinned.mixer.get(voices[1])->done && pinned.voice_duration(voices[1], duration) && duration == 1);
        std::cout << "512 mixed sequential clips, live loop/stream PCM and cursors, server start/dedup, reload, pinned "
                     "completed/prepared clips, explicit saturation and unchanged logical retention passed\n";
        return true;
    }
    bool bytes(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(aliases(resources, root, 10, true, error));
        NetworkAudio audio;
        CHECK(audio.begin(joined));
        const auto anchor = audio.play_voice(resources, parameters(0), error);
        CHECK(anchor);
        const auto clip = audio.mixer.get(anchor)->clip;
        for (unsigned i = 1; i < 9; ++i)
        {
            const auto id = audio.play_voice(resources, parameters(i), error);
            CHECK(id && audio.stop_voice(id));
            CHECK(audio.mixer.cached_bytes() <= 256 * 1024 * 1024);
        }
        CHECK(audio.mixer.cached_clips() == 8 && audio.mixer.cache_evictions() == 1);
        CHECK(audio.mixer.cached_bytes() == 8 * clip->samples.size() * sizeof(float));
        CHECK(audio.mixer.get(anchor)->clip == clip && audio.mixer.get(anchor)->cursor == 0);
        ClientWorld world;
        CHECK(world.begin(7));
        audio.mixer.inspect_pcm = true;
        CHECK(audio.advance(.1, world, {}, {}) && audio.mixer.peak == .125f && audio.mixer.get(anchor)->cursor == 800);
        CHECK(audio.stop_voice(anchor));
        audio.clear();
        CHECK(!audio.mixer.cached_clips() && !audio.mixer.cached_bytes());
        std::cout << "256 MiB cache pressure: one unused clip reclaimed, live clip identity/PCM/cursor preserved, "
                     "eight cached clips within budget\n";
        return true;
    }
    bool retail(const std::filesystem::path& root)
    {
        std::string error;
        Config config;
        config.data_dir = root;
        Resources resources;
        CHECK(resources.mount_game(config, error));
        std::set<std::string> files;
        for (const auto table : {std::span<const AnnouncerData::Entry>(AnnouncerData::kill),
                                 std::span<const AnnouncerData::Entry>(AnnouncerData::suicide),
                                 std::span<const AnnouncerData::Entry>(AnnouncerData::humiliation)})
            for (const auto& entry : table)
                files.insert("Sounds\\Voice\\" + std::string(entry.file));
        NetworkAudio audio;
        CHECK(audio.begin(joined));
        ClientWorld world;
        CHECK(world.begin(7));
        audio.mixer.inspect_pcm = true;
        unsigned missing = 0;
        float peak = 0;
        for (const auto& file : files)
        {
            if (!resources.find(file))
            {
                ++missing; // Exact missing-path qualification belongs to combat_announcer_private.
                continue;
            }
            auto request = parameters(0);
            request.resource = file;
            const auto id = audio.play_voice(resources, request, error);
            CHECK(id && audio.advance(.1, world, {}, {}));
            peak = std::max(peak, audio.mixer.peak);
            CHECK(audio.stop_voice(id));
            CHECK(audio.mixer.cached_clips() <= 128 && audio.mixer.cached_bytes() <= 256 * 1024 * 1024);
        }
        CHECK(files.size() == 209 && missing <= 36 && audio.local_started + missing == 209 && peak > 0);
        CHECK(audio.mixer.cache_evictions() == audio.local_started - 128 && !audio.local_failed && !audio.started);
        std::cout << "Retail sequential private playback: starts=" << audio.local_started << ", missing=" << missing
                  << ", evictions=" << audio.mixer.cache_evictions() << ", PCM peak=" << peak << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    if (std::string_view(argv[1]) == "count")
        return count(argv[2]) ? 0 : 1;
    if (std::string_view(argv[1]) == "bytes")
        return bytes(argv[2]) ? 0 : 1;
    if (std::string_view(argv[1]) == "retail")
        return retail(argv[2]) ? 0 : 1;
    return 2;
}
