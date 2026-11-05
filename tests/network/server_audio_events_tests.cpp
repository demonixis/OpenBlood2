#include "game/game_settings.h"
#include "server/session.h"
#include <algorithm>
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
    bool run(const char* data)
    {
        Config config;
        config.data_dir = path_from_utf8(data);
        std::string error;
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        auto& game = const_cast<GameSession&>(session.game());
        CHECK(!game.server.client_audio);
        CHECK(session.tick(error)); // Zero clients still advance logical audio.
        Joined first, second, late;
        CHECK(session.join({}, first, error) && session.join({}, second, error));
        CHECK(session.begin_audio(first.player, error) && session.begin_audio(second.player, error));
        std::vector<AudioEvent> a, b;
        CHECK(session.take_audio_events(first.player, a, error) && session.take_audio_events(second.player, b, error));
        const auto* owner = game.objects.get(first.object);
        CHECK(owner);
        const auto handle = owner->game->m_hObject;
        SoundRequest request;
        request.resource = "sounds/weapons/beretta/fire.wav";
        request.space = SoundSpace::Local;
        auto sound = game.server.PlayLocalSound(handle, request, error);
        CHECK(sound);
        CHECK(session.take_audio_events(first.player, a, error) && session.take_audio_events(second.player, b, error));
        CHECK(a.size() == 1 && b.empty() && std::get<SoundStart>(a[0].value).owner == first.object);
        CHECK(game.server.KillSound(sound));
        CHECK(session.take_audio_events(first.player, a, error) && session.take_audio_events(second.player, b, error));
        CHECK(a.size() == 1 && b.empty() && !std::get<SoundEnd>(a[0].value).completed);
        request.space = SoundSpace::Spatial;
        sound = game.server.PlayDetachedSound(handle, request, {10, 20, 30}, error);
        CHECK(sound);
        const auto duration = game.server.audio.get(sound)->clip->duration();
        CHECK(game.server.advance_audio(duration + .01) && !game.server.audio.get(sound));
        // No publication occurred between creation and retirement. Both events survive.
        CHECK(session.take_audio_events(first.player, a, error) && session.take_audio_events(second.player, b, error));
        CHECK(a.size() == 2 && b.size() == 2 && std::get<SoundStart>(a[0].value).id == sound &&
              std::get<SoundStart>(b[0].value).id == sound && std::get<SoundEnd>(a[1].value).completed);
        request.loop = true;
        request.speaker = handle;
        sound = game.server.PlayObjectLoopSound(handle, request, error);
        CHECK(sound);
        CHECK(game.server.advance_audio(.125));
        const auto cursor = game.server.audio.get(sound)->cursor;
        CHECK(session.join({}, late, error) && session.begin_audio(late.player, error));
        std::vector<AudioEvent> c;
        CHECK(session.take_audio_events(late.player, c, error));
        const auto found = std::find_if(c.begin(), c.end(),
                                        [&](const auto& e)
                                        {
                                            const auto* s = std::get_if<SoundStart>(&e.value);
                                            return s && s->id == sound;
                                        });
        CHECK(found != c.end() && std::get<SoundStart>(found->value).cursor == cursor && cursor > 0);
        CHECK(
            std::all_of(c.begin(), c.end(), [](const auto& e) { return std::holds_alternative<SoundStart>(e.value); }));
        CHECK(game.server.KillSound(sound));
        CHECK(session.take_audio_events(first.player, a, error) && session.take_audio_events(second.player, b, error));
        CHECK(session.take_audio_events(late.player, c, error));
        CHECK(c.size() == 1 && !std::get<SoundEnd>(c[0].value).completed);
        CHECK(session.leave(late.player));
        // Overflow belongs to the recipient that is not drained. The other player and logical timeline continue.
        request.loop = false;
        request.speaker = nullptr;
        for (unsigned i = 0; i < 257; ++i)
        {
            const auto id = game.server.PlayDetachedSound(handle, request, {}, error);
            CHECK(id && game.server.KillSound(id));
            CHECK(session.take_audio_events(second.player, b, error) && b.size() == 2);
        }
        CHECK(!session.take_audio_events(first.player, a, error));
        CHECK(session.take_audio_events(second.player, b, error) && b.empty());
        // Distinct cinematic owner/speaker: loss stops playback immediately but
        // retains the original logical handle until its owning controller polls it.
        const auto second_handle = game.objects.get(second.object)->game->m_hObject;
        request.speaker = handle;
        const auto cinematic = game.server.PlayCinematicSound(second_handle, request, error);
        CHECK(cinematic);
        CHECK(session.take_audio_events(second.player, b, error) && b.size() == 1);
        SoundRequest local = request;
        local.speaker = nullptr;
        local.space = SoundSpace::Local;
        const auto private_tail = game.server.PlayLocalSound(handle, local, error);
        CHECK(private_tail);
        CHECK(session.leave(first.player));
        Joined newcomer;
        CHECK(session.join({}, newcomer, error) && session.begin_audio(newcomer.player, error));
        CHECK(session.take_audio_events(newcomer.player, c, error));
        CHECK(std::none_of(c.begin(), c.end(),
                           [&](const auto& e)
                           {
                               const auto* s = std::get_if<SoundStart>(&e.value);
                               return s && s->id == private_tail;
                           }));
        CHECK(session.leave(newcomer.player));
        game.server.prepare_audio();
        CHECK(session.take_audio_events(second.player, b, error) && b.size() == 1 &&
              !std::get<SoundEnd>(b[0].value).completed);
        const auto* retained = game.server.audio.get(cinematic);
        CHECK(retained && retained->done && retained->speaker_lost);
        game.server.prepare_audio();
        CHECK(game.server.KillSound(cinematic));
        CHECK(session.take_audio_events(second.player, b, error) && b.empty());
        CHECK(session.leave(second.player) && session.tick(error));
        std::cout << "Server audio: private/global starts, same-step one-shot, late loop, explicit stop, isolated "
                     "512-event overflow, zero-client clocks\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
