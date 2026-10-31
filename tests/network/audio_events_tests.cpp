#include "audio_fixture.h"
#include "client/network_audio.h"
#include <cmath>
#include <iostream>
#include <random>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    SoundStart tone(std::uint64_t id = 1)
    {
        SoundStart s;
        s.id = id;
        s.owner = {3, 4};
        s.parameters.resource = "tone.wav";
        s.parameters.space = SoundSpace::Local;
        s.rate = 8000;
        s.channels = 1;
        s.frames = 8000;
        return s;
    }
    bool codec()
    {
        std::string error;
        AudioEvent start{7, 9, 1, tone()}, end{7, 9, 2, SoundEnd{1, true}};
        std::vector<std::uint8_t> bytes;
        CHECK(encode_audio(end, bytes));
        const std::vector<std::uint8_t> golden{0x42, 0x32, 0x41, 0x55, 20, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 0,
                                               0,    0,    0,    2,    0,  0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1};
        CHECK(bytes == golden);
        std::mt19937 random(42);
        for (const auto& event : {start, end})
        {
            CHECK(encode_audio(event, bytes) && bytes.size() <= audio_packet_limit);
            AudioEvent decoded;
            CHECK(decode_audio(bytes, decoded) == DecodeAudio::Ok);
            std::vector<std::uint8_t> again;
            CHECK(encode_audio(decoded, again) && again == bytes);
            for (std::size_t n = 0; n < bytes.size(); ++n)
            {
                decoded = end;
                CHECK(decode_audio(std::span(bytes).first(n), decoded) != DecodeAudio::Ok);
                CHECK(decoded.sequence == end.sequence && std::holds_alternative<SoundEnd>(decoded.value));
            }
            for (int n = 0; n < 20000; ++n)
            {
                auto corrupted = bytes;
                corrupted[random() % corrupted.size()] ^= std::uint8_t(1 + random() % 255);
                if (decode_audio(corrupted, decoded) == DecodeAudio::Ok)
                    CHECK(encode_audio(decoded, again) && again == corrupted);
            }
            auto bad = bytes;
            bad.push_back(0);
            CHECK(decode_audio(bad, decoded) == DecodeAudio::Malformed);
            bad = bytes;
            bad[4] = 13;
            CHECK(decode_audio(bad, decoded) == DecodeAudio::Protocol);
        }
        for (unsigned i = 0; i < 12; ++i)
        {
            auto bad = start;
            auto& s = std::get<SoundStart>(bad.value);
            if (i == 0)
                bad.world = 0;
            if (i == 1)
                s.id = std::uint64_t(1) << 63;
            if (i == 2)
                s.owner.generation = 0;
            if (i == 3)
                s.parameters.resource = "../secret.wav";
            if (i == 4)
                s.parameters.resource.assign(101, 'a');
            if (i == 5)
                s.parameters.volume = NAN;
            if (i == 6)
                s.cursor = double(s.frames);
            if (i == 7)
                s.frames = 128u * 1024 * 1024 + 1;
            if (i == 8)
                s.rate = 7999;
            if (i == 9)
                s.channels = 3;
            if (i == 10)
                s.speaker = {4, 1};
            if (i == 11)
                s.position.x = INFINITY;
            bytes = golden;
            CHECK(!encode_audio(bad, bytes) && bytes == golden);
        }
        std::cout << "Audio codec: two golden/round-trip variants, all truncations, 40000 mutations\n";
        return true;
    }
    bool cosmetic(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(test_network_audio::setup(resources, root, error));
        NetworkAudio audio;
        CHECK(audio.begin({7, 9, {3, 4}}));
        ClientWorld world;
        CHECK(world.begin(7));
        auto parameters = tone().parameters;
        const auto retained = audio.play_voice(resources, parameters, error);
        CHECK(retained);
        parameters.space = SoundSpace::Spatial;
        SoundId first = 0;
        for (unsigned i = 0; i < 127; ++i)
        {
            const auto id = audio.play_effect(resources, parameters, {1, 2, 3}, error);
            CHECK(id);
            CHECK(audio.mixer.get(id)->auto_remove && audio.mixer.get(id)->position.x == 1);
            if (!i)
                first = id;
        }
        CHECK(!audio.play_effect(resources, parameters, {}, error));
        AudioEvent event{7, 9, 1, tone()};
        CHECK(audio.receive(event, resources, error));
        CHECK(audio.effects_displaced == 1 && !audio.local_displaced && !audio.mixer.get(first) &&
              audio.mixer.get(retained));
        CHECK(audio.advance(1, world, {}, {}));
        CHECK(audio.mixer.voices().size() == 2); // Server and retained speech handles persist, cosmetics retire.
        bool done = false;
        CHECK(audio.voice_done(retained, done) && done && audio.stop_voice(retained));
        event.sequence = 2;
        event.value = SoundEnd{1, true};
        CHECK(audio.receive(event, resources, error));
        CHECK(audio.advance(0, world, {}, {}) && audio.mixer.voices().empty());
        for (unsigned i = 0; i < 256; ++i)
        {
            CHECK(audio.play_effect(resources, parameters, {}, error) && audio.advance(1, world, {}, {}));
            CHECK(audio.mixer.voices().empty());
        }
        CHECK(audio.play_effect(resources, parameters, {}, error));
        CHECK(audio.begin({8, 10, {3, 5}}) && audio.mixer.voices().empty());
        parameters.loop = true;
        CHECK(!audio.play_effect(resources, parameters, {}, error));
        std::cout << "Network cosmetic audio: shared 128-voice budget, server priority, retained speech protection, "
                     "spatial PCM, completion reclamation and world cleanup passed\n";
        return true;
    }
    bool receiver(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(test_network_audio::setup(resources, root, error));
        NetworkAudio audio;
        CHECK(audio.begin({7, 9, {3, 4}}));
        audio.mixer.inspect_pcm = true;
        ClientWorld world;
        CHECK(world.begin(7));
        AudioEvent event{7, 9, 1, tone()};
        CHECK(audio.receive(event, resources, error));
        CHECK(audio.mixer.get(1) && !audio.mixer.get(1)->owner && !audio.mixer.get(1)->request.speaker);
        // Start + natural completion may arrive together: play the whole delayed one-shot.
        event.sequence = 2;
        event.value = SoundEnd{1, true};
        CHECK(audio.receive(event, resources, error));
        CHECK(audio.advance(.25, world, {}, {}) && audio.mixer.peak == .5f && audio.mixer.get(1)->cursor == 2000);
        CHECK(audio.receive(event, resources, error) && audio.completed == 1);
        auto bad = event;
        bad.sequence = 4;
        CHECK(!audio.receive(bad, resources, error) && audio.sequence() == 2);
        bad = event;
        bad.world = 8;
        CHECK(!audio.receive(bad, resources, error));
        bad = event;
        bad.player = 10;
        CHECK(!audio.receive(bad, resources, error));
        CHECK(audio.advance(.75, world, {}, {}) && audio.mixer.voices().empty());
        event.sequence = 3;
        event.value = tone(1);
        CHECK(!audio.receive(event, resources, error) && audio.sequence() == 2);
        auto loop = tone(2);
        loop.parameters.loop = true;
        loop.parameters.streaming = true;
        loop.cursor = 4000;
        loop.loops = 3;
        event.value = loop;
        CHECK(audio.receive(event, resources, error));
        CHECK(audio.mixer.get(2)->cursor == 4000 && audio.mixer.get(2)->loops == 3);
        CHECK(audio.advance(.75, world, {}, {}) && audio.mixer.get(2)->cursor == 2000 &&
              audio.mixer.get(2)->loops == 4);
        event.sequence = 4;
        event.value = SoundEnd{2, true};
        CHECK(!audio.receive(event, resources, error)); // A loop cannot end naturally.
        event.value = SoundEnd{2, false};
        CHECK(audio.receive(event, resources, error) && audio.mixer.voices().empty());
        event.sequence = 5;
        event.value = tone(3);
        std::get<SoundStart>(event.value).frames = 7999;
        CHECK(!audio.receive(event, resources, error) && audio.sequence() == 4 && audio.mixer.voices().empty());
        event.value = tone(3);
        std::get<SoundStart>(event.value).parameters.resource = "missing.wav";
        CHECK(!audio.receive(event, resources, error) && audio.sequence() == 4);
        // Arrival can precede speaker membership. Retain the initial position until the replica exists.
        auto attached = tone(3);
        attached.parameters.space = SoundSpace::Spatial;
        attached.position = {3, 4, 5};
        attached.speaker = {4, 2};
        event.value = attached;
        CHECK(audio.receive(event, resources, error) && audio.advance(.01, world, {}, {}));
        CHECK(audio.mixer.get(3)->position.x == 3);
        ClientWorldFrame frame;
        frame.world = 7;
        frame.sequence = 1;
        ClientObject speaker;
        speaker.id = attached.speaker;
        speaker.type = 0;
        speaker.pose.position = {44, 55, 66};
        frame.objects.push_back(speaker);
        CHECK(world.apply(std::move(frame), error) && audio.advance(.01, world, {}, {}));
        CHECK(audio.mixer.get(3)->position.x == 44);
        event.sequence = 6;
        event.value = SoundEnd{3, false};
        CHECK(audio.receive(event, resources, error));
        // Full logical budget refuses admission atomically; a retired tail can be displaced.
        for (std::uint64_t id = 4; id < 132; ++id)
        {
            event.sequence = audio.sequence() + 1;
            event.value = tone(id);
            CHECK(audio.receive(event, resources, error));
        }
        event.sequence = audio.sequence() + 1;
        event.value = tone(132);
        CHECK(!audio.receive(event, resources, error) && audio.mixer.voices().size() == 128);
        event.value = SoundEnd{4, true};
        CHECK(audio.receive(event, resources, error));
        event.sequence = audio.sequence() + 1;
        event.value = tone(132);
        CHECK(audio.receive(event, resources, error) && audio.displaced == 1 && !audio.mixer.get(4));
        CHECK(audio.mixer.voices().size() == 128);
        audio.clear();
        CHECK(audio.mixer.voices().empty());
        std::cout << "Network audio receiver: independent PCM, streaming loop cursor, strict identities, private "
                     "world, atomic failures, 128 voices\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "codec")
        return codec() ? 0 : 1;
    if (argc == 3 && std::string_view(argv[1]) == "cosmetic")
        return cosmetic(argv[2]) ? 0 : 1;
    return argc == 3 && std::string_view(argv[1]) == "receiver" && receiver(argv[2]) ? 0 : 1;
}
