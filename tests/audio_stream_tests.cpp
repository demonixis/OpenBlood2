#include "audio_fixture.h"
#include "engine/audio_stream.h"
#include "game/original/Sound.h"
#include "game/original/SoundFX.h"
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
    Resource file(const std::filesystem::path& root, const std::vector<std::uint8_t>& b)
    {
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        auto path = root / "slice.bin";
        std::ofstream f(path, std::ios::binary);
        std::array<char, 123> prefix{};
        f.write(prefix.data(), prefix.size());
        f.write(reinterpret_cast<const char*>(b.data()), b.size());
        f.write(prefix.data(), prefix.size());
        f.close();
        return {"test.wav", path, 123, std::uint32_t(b.size()), 0};
    }
    bool pcm(const std::filesystem::path& root)
    {
        for (unsigned channels : {1u, 2u})
            for (unsigned bits : {8u, 16u})
            {
                auto bytes = test_audio::wave(channels, bits, 44100, 20001);
                for (size_t i = 44; i < bytes.size(); ++i)
                    bytes[i] = std::uint8_t(i * 37 + 3);
                auto resource = file(root, bytes);
                std::string error;
                auto stream = PcmStream::open(resource, error);
                CHECK(stream && stream->frames == 20001 && stream->reads == 4);
                CHECK(stream->bytes_read == 44 && !stream->refills); // Metadata only, never whole payload preload.
                AudioClip full;
                CHECK(read_wave(bytes, full, error));
                for (auto frame : {4095u, 4096u, 20000u, 0u, 8191u, 8192u, 12505u})
                    for (unsigned ch = 0; ch < channels; ++ch)
                    {
                        float value = 9;
                        CHECK(stream->sample(frame, ch, value, error));
                        CHECK(std::abs(value - full.samples[frame * channels + ch]) < 1e-6f);
                    }
                CHECK(stream->refills <= 7 &&
                      stream->bytes_read <= 44 + 7 * PcmStream::window_frames * channels * (bits / 8));
                float untouched = 42;
                CHECK(!stream->sample(20001, 0, untouched, error) && untouched == 42);
                CHECK(!stream->sample(0, channels, untouched, error));
            }
        return true;
    }
    bool malformed(const std::filesystem::path& root)
    {
        std::string error;
        auto valid = test_audio::wave(1, 8, 8000, 3);
        for (unsigned kind = 0; kind < 11; ++kind)
        {
            auto b = valid;
            auto patch = [&](size_t at, unsigned n)
            {
                for (unsigned i = 0; i < 4; ++i)
                    b[at + i] = std::uint8_t(n >> (8 * i));
            };
            if (kind == 0)
                b[0] = 'X';
            if (kind == 1)
                patch(4, b.size() - 10);
            if (kind == 2)
                patch(40, 100);
            if (kind == 3)
                b[20] = 17;
            if (kind == 4)
                b[22] = 3;
            if (kind == 5)
                b[32] = 2;
            if (kind == 6)
                b[34] = 24;
            if (kind == 7)
                patch(24, 1);
            if (kind == 8)
                patch(28, 9);
            if (kind == 9)
            {
                b.resize(12);
                patch(4, 4);
            }
            if (kind == 10)
            {
                b.insert(b.end(), {'d', 'a', 't', 'a', 0, 0, 0, 0});
                patch(4, b.size() - 8);
            }
            CHECK(!PcmStream::open(file(root, b), error) && !error.empty());
        }
        // Same bounded terminal odd-data exceptions as the existing buffered decoder.
        for (unsigned kind = 0; kind < 3; ++kind)
        {
            auto b = valid;
            if (kind)
            {
                b.push_back(0);
                if (kind == 2)
                    b[4]++;
            }
            auto stream = PcmStream::open(file(root, b), error);
            AudioClip full;
            CHECK(stream && read_wave(b, full, error) && stream->frames == full.frames());
        }
        auto many = test_audio::wave();
        for (unsigned i = 0; i < 1023; ++i)
            many.insert(many.end(), {'J', 'U', 'N', 'K', 0, 0, 0, 0});
        for (unsigned i = 0; i < 4; ++i)
            many[4 + i] = std::uint8_t((many.size() - 8) >> (8 * i));
        CHECK(!PcmStream::open(file(root, many), error) && error.find("chunk list") != std::string::npos);
        auto bad = file(root, valid);
        bad.offset += 1000;
        CHECK(!PcmStream::open(bad, error));
        bad = file(root, valid);
        bad.size = 129 * 1024 * 1024;
        CHECK(!PcmStream::open(bad, error));
        return true;
    }
    bool mixer(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::Runtime);
        CHECK(owner);
        SoundRequest request;
        request.resource = "long.wav";
        request.loop = true;
        request.space = SoundSpace::Ambient;
        std::string error;
        auto a = g.server.PlaySoundAt(owner, request, {}, error);
        request.streaming = true;
        auto b = g.server.PlaySoundAt(owner, request, {}, error);
        CHECK(a && b);
        auto* buffered = g.server.audio.get(a);
        auto* streamed = g.server.audio.get(b);
        CHECK(!buffered->clip->stream && streamed->clip->stream && streamed->clip->samples.empty());
        CHECK(streamed->clip->frames() == 8000 && streamed->clip->duration() == 1);
        CHECK(g.server.audio.cached_clips() == 2 &&
              g.server.audio.cached_bytes() == 8000 * 4 + PcmStream::buffer_bytes);
        g.server.audio.inspect_pcm = true;
        for (auto dt : {.25, .25, .25, .25, .03, 1.2, 60.0})
        {
            CHECK(g.server.advance_audio(dt, {}, {}));
            CHECK(buffered->cursor == streamed->cursor && buffered->loops == streamed->loops && !streamed->done);
            for (auto v : g.server.audio.mixed)
                CHECK(std::abs(v - 1) < 1e-6f); // Two equal half-scale sources at every seam.
        }
        std::weak_ptr<PcmStream> lifetime = streamed->clip->stream;
        CHECK(streamed->clip->stream->refills == 2); // Both windows retain this whole short fixture.
        request.loop = false;
        auto c = g.server.PlaySoundAt(owner, request, {}, error);
        CHECK(c);
        CHECK(g.server.audio.get(c)->clip == streamed->clip);
        g.server.audio.inspect_pcm = false;
        CHECK(g.server.advance_audio(1.1, {}, {}));
        bool done = false;
        CHECK(g.server.IsSoundDone(c, done) && done);
        CHECK(g.objects.remove(owner) && g.server.advance_audio(0, {}, {}));
        g.objects.finish_frame();
        CHECK(g.server.audio.voices().empty() && !lifetime.expired() && g.unload_world() &&
              !g.server.audio.cached_bytes() && lifetime.expired());
        return true;
    }
    bool seams(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        std::string error;
        for (unsigned channels : {1u, 2u})
            for (unsigned bits : {8u, 16u})
            {
                auto bytes = test_audio::wave(channels, bits, 22050, 12289);
                for (size_t i = 44; i < bytes.size(); ++i)
                    bytes[i] = std::uint8_t(i * 37 + 3);
                test_player::Directory dir;
                dir.files["varied.wav"] = bytes;
                test_model::Writer w;
                w.b.resize(168);
                w.b[0] = w.b[62] = w.b[124] = 13;
                w.b[1] = w.b[63] = w.b[125] = 10;
                w.b[126] = 26;
                w.patch(127, 1);
                auto [off, size] = test_player::write_dir(w, dir);
                w.patch(131, off);
                w.patch(135, size);
                const auto path = root / (std::to_string(channels) + "-" + std::to_string(bits) + ".rez");
                std::ofstream f(path, std::ios::binary);
                f.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
                f.close();
                CHECK(g.resources.mount(path, error));
                AudioMixer buffered, streamed;
                buffered.inspect_pcm = streamed.inspect_pcm = true;
                SoundRequest request;
                request.resource = "varied.wav";
                request.loop = true;
                request.space = SoundSpace::Ambient;
                auto a = buffered.play(g.resources, nullptr, request, {}, error);
                request.streaming = true;
                auto b = streamed.play(g.resources, nullptr, request, {}, error);
                CHECK(a && b);
                for (double dt : {.18, .18, .18, .02, .17, .25, .003, 60.0, .25})
                {
                    CHECK(buffered.advance(dt, {}, {}) && streamed.advance(dt, {}, {}));
                    CHECK(buffered.mixed.size() == streamed.mixed.size());
                    for (size_t i = 0; i < buffered.mixed.size(); ++i)
                        CHECK(std::abs(buffered.mixed[i] - streamed.mixed[i]) < 1e-6f);
                    CHECK(buffered.get(a)->cursor == streamed.get(b)->cursor &&
                          buffered.get(a)->loops == streamed.get(b)->loops);
                }
                CHECK(streamed.get(b)->clip->stream->refills > 4 && streamed.cached_bytes() == PcmStream::buffer_bytes);
            }
        return true;
    }
    bool truncation(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::Runtime);
        CHECK(owner);
        SoundRequest request;
        request.resource = "long.wav";
        request.streaming = true;
        request.loop = true;
        std::string error;
        auto id = g.server.PlaySoundAt(owner, request, {}, error);
        CHECK(id);
        g.server.audio.inspect_pcm = true;
        CHECK(g.server.advance_audio(.02, {}, {}));
        auto* v = g.server.audio.get(id);
        auto cursor = v->cursor;
        std::error_code ec;
        std::filesystem::resize_file(root / "sound.rez", 0, ec);
        CHECK(!ec);
        CHECK(!g.server.advance_audio(.02, {}, {}));
        CHECK(v->cursor == cursor && !v->error.empty() && !v->done && g.server.audio.mixed.empty() &&
              !g.server.audio.output_error.empty());
        bool done = true;
        CHECK(!g.server.IsSoundDone(id, done) && done);
        CHECK(!g.server.PlaySoundAt(owner, request, {}, error));
        CHECK(g.unload_world() && g.server.audio.voices().empty() && !g.server.audio.cached_clips() &&
              g.server.audio.output_error.empty());
        CHECK(g.server.advance_audio(.02, {}, {}) && g.server.audio.output_error.empty());
        return true;
    }
    bool city(const std::filesystem::path& root)
    {
        GameSession g;
        CHECK(g.initialize());
        Config config;
        config.data_dir = root;
        std::string error;
        CHECK(g.resources.mount_game(config, error));
        for (unsigned cycle = 0; cycle < 2; ++cycle)
        {
            CHECK(g.request_world("02_cityhub", error));
            unsigned created = 0, transferred = 0, failed = 0, unknown = 0, fx = 0, fx_voices = 0, streams = 0;
            for (const auto& e : g.entries)
            {
                created += e.state == EntryState::Created;
                transferred += e.state == EntryState::WorldSound;
                failed += e.state == EntryState::Failed;
                unknown += e.state == EntryState::Unsupported;
            }
            for (auto id : g.objects.snapshot())
                if (auto* c = dynamic_cast<SoundFX*>(g.objects.get(id)->game.get()))
                {
                    ++fx;
                    CHECK(c->failures == 0 && c->file_stream);
                    fx_voices += c->sound != 0;
                }
            for (auto& [id, v] : g.server.audio.voices())
                if (v.request.streaming)
                {
                    ++streams;
                    CHECK(v.clip->stream && v.clip->samples.empty());
                }
            g.server.audio.inspect_pcm = true;
            CHECK(g.server.advance_audio(.2, {}, {}));
            for (auto& [id, v] : g.server.audio.voices())
                if (v.request.streaming)
                    CHECK(v.clip->stream->refills > 0 && v.error.empty());
            std::cout << "city created=" << created << " transferred=" << transferred << " unknown=" << unknown
                      << " failed=" << failed << " SoundFX=" << fx << " active=" << fx_voices << " streams=" << streams
                      << " cached_bytes=" << g.server.audio.cached_bytes() << '\n';
            CHECK(created == 432 && transferred == 31 && unknown == 21 && failed == 1 && fx == 5 && fx_voices == 4);
            CHECK(g.unload_world() && g.objects.snapshot().empty() && g.server.audio.voices().empty() &&
                  !g.server.audio.cached_bytes());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::map<std::string, std::function<bool(const std::filesystem::path&)>> cases{
        {"pcm", pcm},     {"malformed", malformed},   {"mixer", mixer},
        {"seams", seams}, {"truncation", truncation}, {"city", city}};
    auto it = cases.find(argv[1]);
    return it != cases.end() && it->second(argv[2]) ? 0 : 1;
}
