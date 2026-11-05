#include "../resource_fixture.h"
#include "audio_fixture.h"
#include "client/network_speech.h"
#include <cmath>
#include <iostream>
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
    const Joined joined{7, 3, {4, 2}};
    bool fixture(Resources& resources, const std::filesystem::path& root, std::string& error)
    {
        CHECK(test_network_audio::setup(resources, root, error));
        std::vector<std::uint8_t> tone;
        CHECK(resources.read("tone.wav", tone, error));
        test_resource::Directory dir;
        auto& caleb = dir.children["sounds"].children["chosen"].children["caleb"];
        caleb.children["weapon"].files["a.wav"] = tone;
        caleb.children["weapon"].files["b.wav"] = tone;
        caleb.children["jump"].files["jump.wav"] = tone;
        caleb.children["unique"].files["msc1_cal.wav"] = tone;
        dir.children["sounds"].children["voice"].files["voice248.wav"] = tone;
        test_model::Writer long_wave;
        long_wave.b = tone;
        for (int i = 0; i < 3; ++i)
            long_wave.b.insert(long_wave.b.end(), tone.begin() + 44, tone.end());
        long_wave.patch(4, unsigned(long_wave.b.size() - 8));
        long_wave.patch(40, 64000);
        dir.files["long.wav"] = long_wave.b;
        test_model::Writer rez;
        rez.b.resize(168);
        rez.b[0] = rez.b[62] = rez.b[124] = 13;
        rez.b[1] = rez.b[63] = rez.b[125] = 10;
        rez.b[126] = 26;
        rez.patch(127, 1);
        auto [offset, size] = test_resource::write_dir(rez, dir);
        rez.patch(131, offset);
        rez.patch(135, size);
        std::ofstream file(root / "voices.rez", std::ios::binary);
        file.write(reinterpret_cast<const char*>(rez.b.data()), rez.b.size());
        file.close();
        CHECK(resources.mount(root / "voices.rez", error));
        return true;
    }
    bool speech(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(fixture(resources, root, error));
        NetworkAudio audio;
        CHECK(audio.begin(joined));
        audio.mixer.inspect_pcm = true;
        NetworkSpeech speech(audio, resources);
        CHECK(speech.begin(joined));
        ClientWorld world;
        CHECK(world.begin(joined.world));
        std::uint64_t sequence = 0;
        const auto send = [&](PrivateValue value, double now, bool camera = false)
        { return speech.receive({joined, ++sequence, 60, std::move(value)}, now, camera, {}, error); };
        CHECK(send(VoiceEvent{true, 0, 9}, .1));
        auto id = speech.manager.voice;
        CHECK(id >= client_sound_id_begin && audio.mixer.get(id) && audio.mixer.get(id)->request.streaming);
        CHECK(!audio.mixer.get(id)->owner && !audio.mixer.get(id)->request.speaker);
        CHECK(audio.advance(.25, world, {}, {}) && audio.mixer.peak == .5f && audio.mixer.get(id)->cursor == 2000);
        CHECK(send(VoiceEvent{false, 0, 0}, .2) && speech.manager.reason == "busy" && !speech.manager.played[0][0]);
        CHECK(send(VoiceStop{}, .3) && !speech.manager.voice && !audio.mixer.get(id));
        CHECK(send(VoiceEvent{false, 0, 0}, .4) && speech.manager.played[0][0]);
        CHECK(send(VoiceEvent{false, 0, 0}, .41) && speech.manager.reason == "unique already played");
        CHECK(send(VoiceStop{}, .45));
        CHECK(send(VoiceEvent{true, 0, 11}, .5) && !audio.mixer.get(speech.manager.voice)->request.streaming);
        CHECK(send(VoiceStop{}, .6));
        CHECK(send(VoiceEvent{true, 0, 9}, .7, true) && speech.manager.reason == "external camera");
        CHECK(speech.manager.PlaySound("long.wav", true));
        id = speech.manager.voice;
        CHECK(send(Conversation{"long.wav", "abc", 2}, 1) && !speech.manager.voice && !audio.mixer.get(id));
        CHECK(speech.communication.level == 1 && !speech.communication.voice);
        CHECK(speech.manager.PlaySound("tone.wav", false));
        id = speech.manager.voice;
        CHECK(send(Conversation{"tone.wav", "busy replacement", 0}, 1.1) && speech.communication.busy == 1);
        CHECK(!speech.manager.voice && !audio.mixer.get(id) && speech.communication.message.file == "long.wav");
        CHECK(speech.update(1.19, error) && !speech.communication.voice);
        CHECK(speech.update(1.21, error) && speech.communication.level == 2 && speech.communication.voice);
        id = speech.communication.voice;
        const auto* cue = audio.mixer.get(id);
        CHECK(cue && !cue->request.streaming && cue->request.priority == 2 && !cue->request.reverb_requested);
        CHECK(speech.communication.portrait == "interface/commlink/i_comm.pcx");
        CHECK(std::abs(speech.manager.next_play_time - 5.71f) < .00001f);
        CHECK(audio.advance(.25, world, {}, {}) && audio.mixer.peak == .5f && !audio.mixer.get(id)->done);
        CHECK(speech.update(3.72, error) && !speech.communication.voice && !audio.mixer.get(id));
        CHECK(speech.update(3.93, error) && !speech.communication.level);
        CHECK(send(VoiceEvent{true, 0, 9}, 4) && speech.manager.reason == "cooldown");
        CHECK(send(VoiceEvent{true, 0, 9}, 5.72) && speech.manager.voice);
        id = speech.manager.voice;
        const auto starts = audio.local_started;
        PrivateEvent duplicate{joined, sequence, 60, VoiceEvent{true, 0, 9}};
        CHECK(speech.receive(duplicate, 5.72, false, {}, error) && audio.local_started == starts &&
              speech.manager.voice == id);
        duplicate.owner.world++;
        CHECK(!speech.receive(duplicate, 6, false, {}, error) && speech.manager.voice == id);
        CHECK(!speech.update(5.71, error) && speech.manager.voice == id);
        CHECK(send(VoiceStop{}, 6));
        CHECK(speech.manager.PlaySound("tone.wav", false));
        id = speech.manager.voice;
        const auto requests = speech.communication.voice_manager_requests;
        const auto attempts = speech.communication.sound_attempts;
        CHECK(send(BloodbathStart{0}, 6.1));
        CHECK(speech.manager.voice == id && speech.communication.level == 1 &&
              speech.communication.voice_manager_requests == requests);
        CHECK(speech.update(6.31, error) && speech.communication.voice &&
              speech.communication.sound_attempts == attempts + 1 && !speech.manager.voice && !audio.mixer.get(id));
        CHECK(speech.communication.message.file == "Sounds\\Voice\\Voice248.wav" && speech.announcer.messages == 1);
        CHECK(audio.advance(.1, world, {}, {}) && audio.mixer.peak == .5f);
        speech.clear();
        CHECK(audio.mixer.voices().empty());
        CHECK(speech.manager.PlaySound("missing.wav", true) == false && speech.manager.failures &&
              !speech.manager.error.empty());
        CHECK(audio.local_started && !audio.started);
        std::cout << "Private speech: source gates, independent streams/PCM, unique flags, busy arbitration, CommLink "
                     "clocks and startup announcement\n";
        return true;
    }
    bool capacity(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(fixture(resources, root, error));
        NetworkAudio audio;
        CHECK(audio.begin(joined));
        audio.mixer.inspect_pcm = true;
        ClientWorld world;
        CHECK(world.begin(joined.world));
        SoundParameters parameters;
        parameters.resource = "tone.wav";
        parameters.space = SoundSpace::Local;
        const auto local = audio.play_voice(resources, parameters, error);
        CHECK(local >= client_sound_id_begin);
        bool done = true;
        float duration = 0;
        CHECK(audio.advance(.25, world, {}, {}) && audio.voice_done(local, done) && !done);
        CHECK(audio.voice_duration(local, duration) && duration == 1);
        for (std::uint64_t id = 1; id <= 128; ++id)
        {
            SoundStart s;
            s.id = id;
            s.parameters = parameters;
            s.rate = 8000;
            s.channels = 1;
            s.frames = 8000;
            CHECK(audio.receive({joined.world, joined.player, id, s}, resources, error));
        }
        CHECK(audio.mixer.voices().size() == 128 && audio.local_displaced == 1 && !audio.mixer.get(local));
        CHECK(audio.voice_done(local, done) && !done && audio.voice_duration(local, duration) && duration == 1);
        CHECK(!audio.stop_voice(1) && audio.mixer.get(1)); // Local code cannot kill a server handle.
        CHECK(!audio.play_voice(resources, parameters, error) && audio.local_failed == 1);
        CHECK(audio.advance(.5, world, {}, {}) && audio.voice_done(local, done) && !done);
        CHECK(audio.advance(.25, world, {}, {}) && audio.voice_done(local, done) && done);
        CHECK(audio.stop_voice(local) && !audio.voice_done(local, done));
        CHECK(audio.mixer.voices().size() == 128);
        std::cout << "Shared 128-voice budget: server admission preempts local PCM without early logical completion; "
                     "local identity isolation\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 3 && (std::string_view(argv[1]) == "capacity" ? capacity(argv[2]) : speech(argv[2])) ? 0 : 1;
}
