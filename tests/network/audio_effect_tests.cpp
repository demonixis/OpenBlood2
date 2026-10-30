#include "audio_fixture.h"
#include "client/audio.h"
#include <iostream>
using namespace OpenBlood2;
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
    bool run(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(test_network_audio::setup(resources, root, error));
        AudioTimeline logical;
        SoundRequest request;
        request.resource = "tone.wav";
        const auto first = logical.play(resources, nullptr, request, {}, error);
        CHECK(first && first < client_sound_id_begin);
        ClientAudio client;
        CHECK(!client.play_effect(resources, request, {}, error) && client.begin(1));
        client.mixer.inspect_pcm = true;
        ClientAudioFrame frame;
        frame.world = 1;
        frame.sequence = 1;
        ClientSound sound;
        sound.id = first;
        sound.parameters = request;
        sound.rate = 8000;
        sound.channels = 1;
        sound.frames = 8000;
        frame.sounds.push_back(sound);
        auto deliver = [&]
        {
            auto copy = frame;
            return client.receive(std::move(copy), resources, error);
        };
        CHECK(deliver());
        const auto effect = client.play_effect(resources, request, {}, error);
        CHECK(effect >= client_sound_id_begin && client.effects_started == 1 && logical.voices().size() == 1);
        CHECK(!client.mixer.get(effect)->owner && !client.mixer.get(effect)->request.speaker &&
              client.mixer.get(effect)->auto_remove && !client.descriptor(effect));
        const auto second = logical.play(resources, nullptr, request, {}, error);
        CHECK(second == first + 1); // Client allocation never consumes a server sound identity.
        CHECK(client.advance(.25, {}, {}) && client.mixer.get(effect)->cursor == 2000 && client.mixer.peak > 0);
        CHECK(logical.get(first)->cursor == 0 && !logical.get(first)->done);
        frame.sequence++;
        CHECK(deliver() && client.mixer.get(effect)->cursor == 2000); // Reconciliation retains local playback.
        auto bad = frame;
        bad.sequence++;
        bad.sounds[0].id = effect;
        CHECK(!client.receive(std::move(bad), resources, error));
        CHECK(client.frame().sequence == frame.sequence && client.mixer.get(effect)->cursor == 2000);
        frame.sequence++;
        frame.sounds.clear();
        CHECK(deliver() && !client.mixer.get(first) && client.mixer.get(effect));
        CHECK(client.advance(.75, {}, {}) && !client.mixer.get(effect)); // Client owns cosmetic completion.
        CHECK(logical.get(first)->cursor == 0 && logical.advance(.25) && logical.get(first)->cursor == 2000);
        for (unsigned invalid = 0; invalid < 4; ++invalid)
        {
            auto parameters = static_cast<SoundParameters>(request);
            if (invalid == 0)
                parameters.loop = true;
            else if (invalid == 1)
                parameters.streaming = true;
            else if (invalid == 2)
                parameters.space = SoundSpace::Local;
            else
                parameters.resource = "missing.wav";
            CHECK(!client.play_effect(resources, parameters, {}, error) && !error.empty());
        }
        std::vector<SoundId> effects;
        for (unsigned i = 0; i < 128; ++i)
        {
            const auto id = client.play_effect(resources, request, {}, error);
            CHECK(id >= client_sound_id_begin);
            effects.push_back(id);
        }
        CHECK(!client.play_effect(resources, request, {}, error) && client.mixer.voices().size() == 128);
        const auto third = logical.play(resources, nullptr, request, {}, error);
        CHECK(third == second + 1 && logical.voices().size() == 3);
        frame.sequence++;
        sound.id = second;
        frame.sounds.push_back(sound);
        sound.id = third;
        frame.sounds.push_back(sound);
        CHECK(deliver() && client.mixer.voices().size() == 128 && client.effects_displaced == 2);
        CHECK(!client.mixer.get(effects[0]) && !client.mixer.get(effects[1]) && client.mixer.get(effects[2]));
        CHECK(client.mixer.get(second) && client.mixer.get(third));
        CHECK(logical.voices().size() == 3 && logical.get(first)->cursor == 2000);
        CHECK(client.begin(2) && client.mixer.voices().empty() && client.effects_started == 0 &&
              client.effects_failed == 0 && client.effects_displaced == 0);
        CHECK(!deliver()); // Prior world's logical frame cannot restore an old sound.
        const auto next = client.play_effect(resources, request, {}, error);
        CHECK(next > effects.back() && logical.voices().size() == 3);
        // Existing mixer path is the PCM oracle: one logical voice followed by two cosmetic voices.
        AudioMixer reference;
        reference.inspect_pcm = true;
        ClientAudio playback;
        CHECK(playback.begin(10));
        playback.mixer.inspect_pcm = true;
        const auto reference_id = reference.play(resources, nullptr, request, {}, error);
        ClientAudioFrame reference_frame;
        reference_frame.world = 10;
        reference_frame.sequence = 1;
        sound.id = reference_id;
        reference_frame.sounds.push_back(sound);
        CHECK(playback.receive(std::move(reference_frame), resources, error));
        for (const auto position : {DVector{50, 0, 0}, DVector{-100, 20, 0}})
        {
            CHECK(reference.play(resources, nullptr, request, position, error));
            CHECK(playback.play_effect(resources, request, position, error));
        }
        for (unsigned step = 0; step < 4; ++step)
        {
            CHECK(reference.advance(.25, {}, {}) && playback.advance(.25, {}, {}));
            CHECK(reference.mixed == playback.mixer.mixed && reference.peak == playback.mixer.peak);
        }
        CHECK(playback.mixer.voices().size() == 1); // Only the replicated logical voice remains.
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
