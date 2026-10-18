#include "audio_fixture.h"
#include "game/key_messages.h"
#include "game/original/Music.h"
#include <algorithm>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " | " << error << '\n';                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool send(GameSession& g, const char* text)
    {
        auto str = g.server.CreateString(text);
        auto message = g.server.StartMessageToObject(nullptr, g.client_shell.player, 1000);
        const bool ok =
            message && g.server.WriteToMessageHString(message, str) == DE_OK && g.server.EndMessage(message) == DE_OK;
        g.server.FreeString(str);
        return ok;
    }
    bool parsing()
    {
        std::string error;
        MusicCommand command;
        for (auto [text, level] :
             std::initializer_list<std::pair<const char*, unsigned>>{{"LEVEL", 0},
                                                                     {"level nonsense", 0},
                                                                     {"  LeVeL   +258junk ignored", 2},
                                                                     {"LEVEL -1", 255},
                                                                     {"LEVEL 2147483647", 255},
                                                                     {"LEVEL -2147483648", 0},
                                                                     {"LEVEL \t3x", 3},
                                                                     {"LEVEL +", 0}})
        {
            CHECK(CMusic::ParseTrigger(text, command, error) && command.type == MusicCommandType::Level &&
                  command.level == level && command.name.empty());
        }
        for (const auto text : {"", "UNKNOWN 2", "LEVEL\t2", "LEVEL 2147483648", "LEVEL -2147483649", "MOTIF", "BREAK"})
            CHECK(!CMusic::ParseTrigger(text, command, error) && !error.empty());
        CHECK(CMusic::ParseTrigger("mOtIf Theme ignored", command, error) && command.type == MusicCommandType::Motif &&
              command.name == "Theme");
        CHECK(CMusic::ParseTrigger("BREAK Break1", command, error) && command.type == MusicCommandType::Break &&
              command.name == "Break1");
        CHECK(CMusic::ParseTrigger("MOTIF " + std::string(49, 'a'), command, error));
        CHECK(!CMusic::ParseTrigger("MOTIF " + std::string(50, 'a'), command, error));
        CHECK(!CMusic::ParseTrigger(std::string("LEVEL 2\0junk", 12), command, error));
        CHECK(!valid_music_command({MusicCommandType(255), 0, "bad"}));
        std::cout << "Original literal-space command parsing, atoi prefix/byte wrap, explicit overflow/name bounds "
                     "verified\n";
        return true;
    }
    bool delivery(const std::filesystem::path& root)
    {
        std::string error;
        GameSession g;
        CHECK(test_audio::setup(g, root));
        CHECK(g.objects.create(g.objects.find_class("GameStartPoint"), {}, Creation::World,
                               {{"Name", std::string("START")}, {"Pos", DVector{0, 100, 0}}}));
        CHECK(g.enter_player(error));
        auto& music = g.client_shell.music;
        CHECK(music.logical_level == 0 && music.failures == 1 &&
              !music.error.empty()); // Fixture intentionally has no music bank.
        auto* player = g.server_shell.Player(g.local_client);
        CHECK(player);
        const auto strings = g.server.string_count();
        CHECK(send(g, "mUsIc LeVeL 2"));
        CHECK(music.received == 0 && player->music_messages == 1 && player->unsupported_triggers == 0);
        CHECK(g.pump_player_messages(error));
        CHECK(music.logical_level == 2 && music.received == 1 && music.failures == 2 && !music.error.empty());
        CHECK(g.client_shell.m_bInWorld && g.server.GetClientUserData(g.local_client) == player->m_hObject);
        CHECK(send(g, "MUSIC LEVEL 2") && g.pump_player_messages(error));
        CHECK(music.refused == 1 && music.requests == 2);
        CHECK(send(g, "MUSIC LEVEL 255") && g.pump_player_messages(error));
        CHECK(music.logical_level == 2 && music.refused == 2);
        CHECK(send(g, "MUSIC MOTIF Theme"));
        player->last_player_trigger.assign("overwritten source buffer");
        CHECK(g.pump_player_messages(error));
        CHECK(music.last_command.name == "Theme" && music.source_noops == 1);
        CHECK(send(g, "MUSIC BREAK x") && g.pump_player_messages(error) && music.source_noops == 2);
        CHECK(send(g, "MUSIC LEVEL 2147483648") && !player->player_trigger_error.empty());
        CHECK(g.pump_player_messages(error) && music.received == 5);
        CHECK(g.server.SendToClient(g.local_client, MusicCommand{MusicCommandType::Motif, 0, std::string(50, 'x')}) ==
              DE_INVALIDPARAMS);
        CHECK(g.server.string_count() == strings);
        player->m_bDead = true; // Original command dispatch is outside the death guard.
        CHECK(send(g, "MUSIC MOTIF dead-player") && g.pump_player_messages(error) && music.source_noops == 3);
        player->m_bDead = false;
        CHECK(send(g, "MUSIC LEVEL 1"));
        CHECK(g.leave_player()); // Queued value must not reach a later player/client.
        CHECK(music.received == 0 && !g.server.audio.music_loop.active() && !g.server.audio.music_loop.pending());
        CHECK(g.enter_player(error) && music.received == 0 && music.logical_level == 0);
        CHECK(g.unload_world());
        std::cout
            << "Owned player/client delivery, mutation-before-failure, no-op motifs, stale-client teardown verified\n";
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        GameSession g;
        Config config;
        config.data_dir = root;
        CHECK(g.initialize() && g.resources.mount_game(config, error) && g.request_world("01_SubwayA", error) &&
              g.enter_player(error));
        auto& music = g.client_shell.music;
        auto& audio = g.server.audio;
        audio.inspect_pcm = true;
        CHECK(music.accepted >= 1 && music.error.empty() && audio.music_loop.active());
        CHECK(send(g, "MUSIC LEVEL 3") && g.pump_player_messages(error));
        CHECK(music.logical_level == 3 && audio.music_loop.level() == 3 && !audio.music_loop.pending());
        CHECK(g.server.advance_audio(.1, {}, {}));
        CHECK(std::any_of(audio.mixed.begin(), audio.mixed.end(), [](float v) { return v != 0; }));
        CHECK(send(g, "MUSIC LEVEL 1") && g.pump_player_messages(error));
        CHECK(audio.music_loop.pending() && audio.music_loop.level() == 3 && music.logical_level == 1);
        const auto boundary = audio.music_loop.switch_frame();
        while (audio.instruments.sample_clock() < boundary)
        {
            const auto frames = std::min<std::uint64_t>(4800, boundary - audio.instruments.sample_clock());
            CHECK(g.server.advance_audio(double(frames) / 48000, {}, {}));
        }
        CHECK(audio.music_loop.level() == 1 && !audio.music_loop.pending() && audio.music_error.empty());
        const auto committed = audio.music_loop.switches();
        CHECK(send(g, "MUSIC LEVEL 0") && g.pump_player_messages(error) && audio.music_loop.pending());
        CHECK(send(g, "MUSIC LEVEL 2") &&
              g.pump_player_messages(error)); // Logical silence -> immediate, while audible level remains ambient.
        CHECK(music.logical_level == 2 && audio.music_loop.level() == 2 && !audio.music_loop.pending());
        CHECK(audio.music_loop.switches() == committed + 1);
        CHECK(g.server.advance_audio(.1, {}, {}) && !audio.instruments.dispatch_failures);
        CHECK(send(g, "MUSIC LEVEL 1") && g.pump_player_messages(error) && audio.music_loop.pending());
        std::cout << "SubwayA live music: received " << music.received << "; accepted " << music.accepted
                  << "; committed " << audio.music_loop.switches() << "; first boundary " << boundary
                  << "; cancellations " << audio.instruments.cancelled_messages << '\n';
        CHECK(g.unload_world() && !audio.music_loop.pending() && !audio.music_loop.active() &&
              !audio.music_loop.template_events() && audio.instruments.pending_midi().empty());
        CHECK(g.request_world("02_CityHub", error) && g.enter_player(error));
        CHECK(music.logical_level == 0 && music.received == 0 && music.accepted == 1 && audio.music_loop.level() == 0);
        CHECK(g.unload_world());
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view mode = argv[1];
    return (mode == "parsing"
                ? parsing()
                : argc == 3 && (mode == "delivery" ? delivery(argv[2]) : mode == "private" && private_data(argv[2])))
               ? 0
               : 1;
}
