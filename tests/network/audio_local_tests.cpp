#include "../audio_fixture.h"
#include "engine/audio_stream.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x " " << error << '\n';                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const std::filesystem::path& root)
    {
        std::string error;
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::Runtime);
        CHECK(owner);
        SoundRequest r;
        r.resource = "long.wav";
        r.speaker = owner;
        r.loop = true;
        const auto attached = g.server.PlayObjectLoopSound(owner, r, error);
        CHECK(attached);
        r.speaker = nullptr;
        r.space = SoundSpace::Ambient;
        const auto world = g.server.PlayWorldSound(owner, r, true, error);
        CHECK(world);
        CHECK(g.advance_audio(.05, {}, {}));
        auto* server = g.server.audio.get(world);
        auto* client = g.client_session.audio.mixer.get(world);
        CHECK(server && client && server->clip != client->clip && server->clip->stream != client->clip->stream);
        CHECK(server->cursor == 400 && client->cursor == 400 && server->clip->stream->bytes_read < 8000);
        const auto frame = g.client_session.audio.frame();
        ClientAudio second;
        CHECK(second.begin(frame.world));
        auto copy = frame;
        CHECK(second.receive(std::move(copy), g.resources, error));
        ClientAudio failed_client;
        CHECK(failed_client.begin(frame.world));
        copy = frame;
        CHECK(failed_client.receive(std::move(copy), g.resources, error));
        failed_client.mixer.get(world)->error = "synthetic client stream failure";
        CHECK(!failed_client.mixer.advance(.02, {}, {}) && server->error.empty() && client->error.empty());
        DVector pos{40, 0, 0};
        CHECK(g.server.TeleportObject(owner, &pos) == DE_OK && g.advance_audio(.02, {}, {}));
        CHECK(g.server.audio.get(attached)->position.x == 40 &&
              g.client_session.audio.mixer.get(attached)->position.x == 40);
        CHECK(second.mixer.get(attached)->position.x == 0); // Independent receiver retains its own publication.
        CHECK(g.objects.remove(owner) && g.advance_audio(.02, {}, {}));
        CHECK(!g.server.audio.get(attached) && !g.client_session.audio.mixer.get(attached) &&
              g.server.audio.get(world));
        CHECK(g.server.KillSound(world) && g.advance_audio(0, {}, {}));
        CHECK(!g.client_session.audio.mixer.get(world));
        CHECK(g.unload_world());
        copy = frame;
        CHECK(!g.client_session.audio.receive(std::move(copy), g.resources, error));
        CHECK(second.mixer.advance(.01, {}, {})); // Original world destroyed; copied descriptor/reader survives.
        return true;
    }
    bool device(const std::filesystem::path& root)
    {
        std::string error;
        GameSession g;
        CHECK(test_audio::setup(g, root));
        auto owner = g.objects.create(g.objects.find_class("BaseClass"), {}, Creation::Runtime);
        CHECK(owner);
        SoundRequest r;
        r.resource = "cue.wav";
        r.space = SoundSpace::Local;
        auto id = g.server.PlayCinematicSound(owner, r, error);
        CHECK(id);
        auto& audio = g.client_session.audio.mixer;
        CHECK(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
        CHECK(audio.open_output(error));
        audio.output_lead = .06;
        CHECK(g.advance_audio(1. / 60, {}, {}));
        CHECK(audio.get(id)->cursor > g.server.audio.get(id)->cursor && audio.lead_frames > 0);
        bool done = true;
        CHECK(g.server.IsSoundDone(id, done) && !done);
        CHECK(audio.pause_output(true, error) && audio.set_gains(0, 0));
        auto cursor = g.server.audio.get(id)->cursor;
        CHECK(g.advance_audio(.2, {}, {}));
        CHECK(g.server.audio.get(id)->cursor == cursor);
        CHECK(g.step_server(.2));
        CHECK(g.server.IsSoundDone(id, done) && done); // Dedicated progression ignores the client pause/device.
        float duration = 0;
        CHECK(g.server.GetSoundDuration(id, duration) && std::abs(duration - .1f) < 1e-6);
        CHECK(audio.pause_output(false, error) && g.advance_audio(0, {}, {}));
        CHECK(audio.get(id)->done);
        return true;
    }
    bool private_worlds(const std::filesystem::path& root)
    {
        for (const char* map : {"01_SubwayA", "02_CityHub"})
        {
            std::string error;
            GameSession g;
            CHECK(g.initialize());
            Config cfg;
            cfg.data_dir = root;
            CHECK(g.resources.mount_game(cfg, error));
            WorldMessage message;
            CHECK(g.client_shell.StartNewWorld(map, message) && g.load_world(message, error) && g.enter_player(error));
            g.client_session.audio.mixer.inspect_pcm = true;
            for (unsigned i = 0; i < 300; ++i)
            {
                CHECK(g.step(1. / 60));
                for (const auto& [id, v] : g.server.audio.voices())
                {
                    const auto* copy = g.client_session.audio.mixer.get(id);
                    CHECK(copy && copy->request.resource == v.request.resource && copy->clip != v.clip);
                    CHECK(copy->cursor == v.cursor && copy->done == v.done && copy->loops == v.loops);
                }
            }
            CHECK(g.client_session.audio.mixer.mixed_frames > 0 && g.unload_world() &&
                  g.client_session.audio.mixer.voices().empty());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    return (std::string_view(argv[1]) == "local"    ? run(argv[2])
            : std::string_view(argv[1]) == "device" ? device(argv[2])
                                                    : private_worlds(argv[2]))
               ? 0
               : 1;
}
