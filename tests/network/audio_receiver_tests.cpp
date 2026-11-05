#include "audio_fixture.h"
#include "client/audio.h"
#include <cmath>
#include <iostream>
#include <limits>
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
        ClientAudio client;
        CHECK(client.begin(1));
        client.mixer.inspect_pcm = true;
        ClientAudioFrame frame;
        frame.world = 1;
        frame.sequence = 1;
        ClientSound sound;
        sound.id = 1;
        sound.owner = {2, 3};
        sound.parameters.resource = "tone.wav";
        sound.parameters.space = SoundSpace::Local;
        sound.rate = 8000;
        sound.channels = 1;
        sound.frames = 8000;
        frame.sounds.push_back(sound);
        auto copy = frame;
        CHECK(client.receive(std::move(copy), resources, error));
        auto* voice = client.mixer.get(1);
        CHECK(voice && !voice->owner && !voice->request.speaker);
        CHECK(client.mixer.advance(.25, {}, {}));
        CHECK(voice->cursor == 2000 && client.mixer.peak == .5f);
        copy = frame;
        CHECK(!client.receive(std::move(copy), resources, error) && voice->cursor == 2000);
        frame.sequence = 2;
        frame.sounds[0].cursor = 1000;
        copy = frame;
        CHECK(client.receive(std::move(copy), resources, error) && voice == client.mixer.get(1) &&
              voice->cursor == 2000);
        // Publication updates positions/status, never rewinds already rendered PCM.
        CHECK(client.mixer.advance(.25, {}, {}));
        CHECK(voice->cursor == 4000);
        for (int bad = 0; bad < 8; ++bad)
        {
            copy = frame;
            copy.sequence = 3;
            if (bad == 0)
                copy.world = 2;
            if (bad == 1)
                copy.sounds.push_back(copy.sounds[0]);
            if (bad == 2)
                copy.sounds.resize(129);
            if (bad == 3)
                copy.sounds[0].cursor = std::numeric_limits<double>::quiet_NaN();
            if (bad == 4)
                copy.sounds[0].parameters.volume = .3f;
            if (bad == 5)
                copy.sounds[0].frames = 7000;
            if (bad == 6)
                copy.sounds[0].owner = {2, 0};
            if (bad == 7)
                copy.sounds[0].error.assign(1025, 'x');
            CHECK(!client.receive(std::move(copy), resources, error) && client.frame().sequence == 2 &&
                  voice->cursor == 4000 && client.mixer.voices().size() == 1);
        }
        // A clip this client cannot prepare leaves only that voice silent: the
        // frame is admitted, the failure is counted once and never retried.
        ClientAudio silent;
        CHECK(silent.begin(1));
        copy = frame;
        copy.sounds.push_back(sound);
        copy.sounds.back().id = 2;
        copy.sounds.back().parameters.resource = "missing.wav";
        auto again = copy;
        CHECK(silent.receive(std::move(copy), resources, error) && silent.mixer.voices().size() == 1 &&
              silent.mirror_failed == 1 && !silent.mirror_error.empty() && silent.frame().sounds.size() == 2);
        again.sequence = frame.sequence + 1;
        CHECK(silent.receive(std::move(again), resources, error) && silent.mixer.voices().size() == 1 &&
              silent.mirror_failed == 1);
        ClientAudio late;
        CHECK(late.begin(1));
        copy = frame;
        CHECK(late.receive(std::move(copy), resources, error));
        CHECK(late.mixer.get(1)->cursor == 1000 && late.mixer.get(1)->clip != voice->clip);
        copy = frame;
        copy.sequence = 3;
        copy.sounds.clear();
        CHECK(client.receive(std::move(copy), resources, error) && client.mixer.voices().empty());
        copy = frame;
        copy.sequence = 4;
        CHECK(!client.receive(std::move(copy), resources, error));
        CHECK(client.begin(2));
        copy = frame;
        copy.world = 2;
        copy.sequence = 1;
        CHECK(client.receive(std::move(copy), resources, error));
        copy = frame;
        copy.sequence = 100;
        CHECK(!client.receive(std::move(copy), resources, error));
        // Buffered assets/cursors remain usable without a resource index or any server instance.
        resources = {};
        CHECK(client.mixer.advance(.25, {}, {}));
        CHECK(client.mixer.get(1)->cursor == 3000);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
