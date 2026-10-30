#include "../audio_fixture.h"
#include "game/original/CinematicTrigger.h"
#include "server/clock.h"
#include <cmath>
#include <iostream>

using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const std::filesystem::path& root)
    {
        GameSession game;
        CHECK(test_audio::setup(game, root));
        CHECK(!game.local_client && !game.client_shell.m_bInWorld && !game.client_session.audio.mixer.has_output());
        auto owner = game.objects.create(game.objects.find_class("Key"), {});
        CHECK(owner);
        SoundRequest request;
        request.resource = "cue.wav";
        request.space = SoundSpace::Spatial;
        std::string error;
        const auto id = game.server.PlaySoundAt(owner, request, {}, error);
        CHECK(id);
        float duration = 0;
        CHECK(game.server.GetSoundDuration(id, duration) && std::abs(duration - .1f) < 1e-6f);
        bool done = true;
        CHECK(game.server.IsSoundDone(id, done) && !done);
        const auto client_updates = game.client_shell.move_mgr.updates;
        for (unsigned i = 0; i < 12; ++i)
            CHECK(game.step_server(ServerClock::step));
        CHECK(game.objects.time() > .19 && game.objects.time() < .21);
        CHECK(game.server.IsSoundDone(id, done) && done);
        CHECK(game.client_session.audio.mixer.mixed.empty() && game.client_session.audio.mixer.submitted_frames == 0);
        CHECK(!game.client_session.audio.mixer.has_output() && !game.local_client && !game.client_shell.m_bInWorld);
        CHECK(game.client_shell.move_mgr.updates == client_updates);
        CHECK(!game.step_server(-1));
        CHECK(game.objects.time() > .19 && game.objects.time() < .21);
        CHECK(game.unload_world());
        CHECK(!game.server.IsSoundDone(id, done));
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
