#include "client/network_client.h"
#include "game/game_settings.h"
#include "game/original/FlagObjects.h"
#include "server/network.h"
#include <iostream>
#include <thread>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x ": " << error << '\n';                                 \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool state()
    {
        std::string error;
        const Joined owner{7, 1, {0, 1}};
        NetworkPlayerEvents client;
        CHECK(client.begin(owner, GAMETYPE_CTF));
        PlayerRoster roster{{{1, owner.object, "Blue", 0, 2, 1}, {2, {1, 1}, "Red", 1, -1, 2}}, GAMETYPE_CTF, {7, -3}};
        PrivateEvent event{owner, 1, 1, roster};
        const auto send = [&](PrivateValue value)
        {
            event.value = std::move(value);
            std::vector<std::uint8_t> bytes;
            PrivateEvent decoded;
            CHECK(encode_private(event, bytes));
            for (std::size_t n = 0; n < bytes.size(); ++n)
                CHECK(decode_private(std::span(bytes).first(n), decoded) == DecodePrivate::Malformed);
            CHECK(decode_private(bytes, decoded) == DecodePrivate::Ok &&
                  client.receive(decoded, error) == NetworkPlayerEvents::Reception::Applied);
            CHECK(client.receive(decoded, error) == NetworkPlayerEvents::Reception::Ignored);
            ++event.sequence;
            return true;
        };
        CHECK(send(roster) && send(FlagEvent{CTFEventKind::Grabbed, 1, 0, {}}) && client.flag_icon() == 1);
        CHECK(send(FlagEvent{CTFEventKind::Returned, 1, 0, {}}) && client.flag_icon() == 1);
        CHECK(send(FlagEvent{CTFEventKind::NeedFlag, 1, 0, {}}) && client.flag_icon() == 1);
        // Source clears every recipient's icon, even an unrelated carrier. Absolute totals are not added twice.
        CHECK(send(FlagEvent{CTFEventKind::Captured, 1, 5, {1, 1}, 4, {7, 2}}));
        CHECK(!client.flag_icon() && client.player({1, 1})->frags == 4 && client.team_frags()[1] == 2);
        CHECK(send(FlagEvent{CTFEventKind::Grabbed, 1, 0, {}}) && send(FlagEvent{CTFEventKind::Dropped, 1, 0, {}}) &&
              !client.flag_icon());
        for (unsigned bad = 0; bad < 7; ++bad)
        {
            FlagEvent value{CTFEventKind::Captured, 1, 5, {1, 1}, 4, {7, 2}};
            if (bad == 0)
                value.team = 0;
            if (bad == 1)
                value.capturer = {};
            if (bad == 2)
                value.capturer_frags = 1000001;
            if (bad == 3)
                value.team_frags[0] = -1000001;
            if (bad == 4)
                value.points = -1;
            if (bad == 5)
                value.kind = CTFEventKind::Dropped;
            if (bad == 6)
            {
                value = {CTFEventKind::Grabbed, 1, 0, {}};
                value.capturer.generation = 1;
            }
            event.value = value;
            CHECK(!valid_private(event) && client.receive(event, error) == NetworkPlayerEvents::Reception::Refused);
        }
        event.value = FlagEvent{CTFEventKind::Grabbed, 2, 0, {}};
        const auto sequence = client.sequence();
        CHECK(client.receive(event, error) == NetworkPlayerEvents::Reception::Refused &&
              client.sequence() == sequence && !client.flag_icon());
        roster.players.pop_back();
        roster.team_frags = {7, 2};
        CHECK(send(roster));
        CHECK(send(FlagEvent{CTFEventKind::Captured, 1, 0, {1, 1}, 4, {7, 2}}) && client.roster().size() == 1);
        CHECK(client.begin({8, 3, {0, 2}}, GAMETYPE_CTF) && !client.flag_icon() && !client.team_frags()[0]);
        CHECK(client.receive(event, error) == NetworkPlayerEvents::Reception::Ignored);
        CHECK(client.begin(owner, GAMETYPE_BLOODBATH));
        event = {owner, 1, 1, FlagEvent{CTFEventKind::Grabbed, 1, 0, {}}};
        CHECK(client.receive(event, error) == NetworkPlayerEvents::Reception::Refused && client.sequence() == 0);
        std::cout << "CTF flag codec truncation/bounds, absolute scores, icons, duplicate/departed/mode/team/epoch "
                     "gates passed\n";
        return true;
    }
    bool speech(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        Resources resources;
        CHECK(resources.mount_game(config, error));
        ClientAnnouncer announcer;
        std::string text;
        CHECK(announcer.flag_notice(CTFEventKind::Captured, "Red %s", 2, false, resources, text));
        CHECK(text == "Red %s captured the flag for Team Red!");
        CHECK(announcer.flag_notice(CTFEventKind::Captured, "Blue", 1, true, resources, text) &&
              text == "You captured the other team's flag!");
        CHECK(announcer.flag_notice(CTFEventKind::Captured, "Unknown team", 0, false, resources, text) &&
              text == "Unknown team captured the flag!");
        const Joined owner{7, 1, {0, 1}};
        NetworkAudio audio;
        CHECK(audio.begin(owner));
        NetworkSpeech speech(audio, resources);
        CHECK(speech.begin(owner));
        ClientWorld world;
        CHECK(world.begin(owner.world));
        const std::vector<RosterPlayer> roster{{1, owner.object, "Blue", 0, 0, 1}, {2, {1, 1}, "Red %s", 1, 0, 2}};
        std::uint64_t sequence = 0;
        const auto send = [&](FlagEvent flag, double now)
        {
            PrivateEvent e{owner, ++sequence, 60, flag};
            CHECK(speech.receive(e, now, false, roster, error));
            const auto sounds = speech.flag_sounds, notices = speech.flag_notices;
            CHECK(speech.receive(e, now, false, roster, error) && speech.flag_sounds == sounds &&
                  speech.flag_notices == notices);
            return true;
        };
        CHECK(send({CTFEventKind::Grabbed, 1, 0, {}}, 0) && send({CTFEventKind::Returned, 1, 0, {}}, 0));
        CHECK(speech.flag_sounds == 2 && speech.flag_notices == 2 && speech.flag_sound_error.empty());
        CHECK(audio.mixer.voices().size() == 2);
        for (const auto& [id, voice] : audio.mixer.voices())
            CHECK(id >= client_sound_id_begin && voice.request.space == SoundSpace::Local && !voice.request.loop &&
                  !voice.request.streaming && !voice.request.reverb_requested && voice.request.priority == 1);
        CHECK(send({CTFEventKind::Dropped, 1, 0, {}}, 1) && speech.flag_notices == 2 && speech.flag_sounds == 2);
        CHECK(send({CTFEventKind::NeedFlag, 1, 0, {}}, 20) && speech.flag_notices == 2);
        CHECK(send({CTFEventKind::NeedFlag, 1, 0, {}}, 20.1) && speech.flag_notices == 3);
        CHECK(send({CTFEventKind::NeedFlag, 1, 0, {}}, 40.1) && speech.flag_notices == 3);
        CHECK(send({CTFEventKind::NeedFlag, 1, 0, {}}, 40.2) && speech.flag_notices == 4);
        CHECK(send({CTFEventKind::Captured, 1, 5, {1, 1}, 5, {0, 5}}, 41) &&
              speech.notices().back().text == "Red %s captured the flag for Team Red!");
        CHECK(send({CTFEventKind::Captured, 1, 5, {9, 1}, 5, {0, 5}}, 42) && speech.missing_players == 1);
        CHECK(audio.advance(20, world, {}, {}));
        CHECK(speech.update(43, error) && audio.mixer.voices().empty());
        CHECK(send({CTFEventKind::Grabbed, 1, 0, {}}, 44) && audio.mixer.voices().size() == 1);
        CHECK(speech.begin({8, 2, {0, 2}}) && audio.mixer.voices().empty() && speech.notices().empty());
        Resources absent;
        ClientAnnouncer missing;
        CHECK(!missing.flag_notice(CTFEventKind::Grabbed, {}, 0, false, absent, text) && !missing.error.empty());
        std::cout << "External CTF strings/two-stage formatting, independent overlapping local sounds, strict 20s "
                     "gate, duplicate and map cleanup passed\n";
        return true;
    }
    bool endpoints(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        MultiplayerRules rules;
        rules.frags = 100;
        ServerSession server;
        CHECK(server.start(config, "multi/ctf_cabana", GAMETYPE_CTF, error, rules));
        ServerNetwork network(server);
        CHECK(network.listen("127.0.0.1", 0, 3, error));
        NetworkClient clients[3];
        unsigned active = 2;
        const auto enter = [&](unsigned i)
        {
            JoinProfile profile;
            profile.team = i == 1 ? 2 : 1;
            profile.name = "CTF " + std::to_string(i);
            CHECK(clients[i].prepare(config, error) &&
                  clients[i].connect("127.0.0.1", network.port(), profile, NetworkClient::Clock::now(), error));
            return true;
        };
        CHECK(enter(0) && enter(1));
        unsigned prior_count = 0;
        auto now = NetworkClient::Clock::now();
        const auto until = [&](auto condition)
        {
            for (unsigned tick = 0; tick < 1000; ++tick)
            {
                now += std::chrono::microseconds(16667);
                CHECK(server.tick(error) && network.pump(now, error));
                if (server.player_count() != prior_count)
                {
                    prior_count = server.player_count();
                    for (auto id : server.game().objects.snapshot())
                    {
                        const auto* r = server.game().objects.get(id);
                        if (dynamic_cast<const CPlayerObj*>(r->game.get()))
                            std::cout << "Admission body " << id.slot << " pos " << r->state.m_Pos.x << ' '
                                      << r->state.m_Pos.y << ' ' << r->state.m_Pos.z << " half " << r->dimensions.x
                                      << ' ' << r->dimensions.y << ' ' << r->dimensions.z << " nudge "
                                      << r->placement_nudges << '\n';
                    }
                }
                for (unsigned i = 0; i < active; ++i)
                {
                    CHECK(clients[i].update(now, {}, true, error));
                    std::vector<PrivateEvent> events;
                    clients[i].take_events(events);
                }
                if (condition())
                    return true;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            error = "CTF endpoint condition deadline";
            CHECK(false);
        };
        CHECK(until(
            [&]
            {
                return clients[0].ready() && clients[1].ready() && clients[0].players().roster().size() == 2 &&
                       clients[1].players().roster().size() == 2;
            }));
        auto& game = const_cast<GameSession&>(server.game());
        auto* blue = dynamic_cast<CPlayerObj*>(game.objects.get(clients[0].joined().object)->game.get());
        auto* red = dynamic_cast<CPlayerObj*>(game.objects.get(clients[1].joined().object)->game.get());
        FlagStand* base = nullptr;
        for (auto id : game.objects.snapshot())
            if (auto* b = dynamic_cast<FlagStand*>(game.objects.get(id)->game.get()); b && b->team == 1)
                base = b;
        CHECK(blue && red && base);
        const auto red_flag = game.objects.get(g_ctf.flags[1]->m_hObject)->id;
        const auto blue_flag = game.objects.get(g_ctf.flags[0]->m_hObject)->id;
        // Explicit rule stimuli: this verifies real ENet replication, not a client-routed capture.
        blue->GrabFlag(g_ctf.flags[1]->m_hObject);
        CHECK(blue->ctf_error.empty());
        CHECK(until(
            [&]
            {
                const auto* flag = clients[1].world().get(red_flag);
                return clients[0].players().flag_icon() == 1 && flag && flag->attached &&
                       flag->parent == clients[0].joined().object;
            }));
        CHECK(!clients[1].players().flag_icon() && clients[0].speech().flag_sounds == 1 &&
              clients[1].speech().flag_sounds == 0);
        blue->GiveFlag(base->m_hObject);
        CHECK(blue->ctf_error.empty() && blue->m_Frags == 5);
        CHECK(until(
            [&]
            {
                return clients[0].players().team_frags()[0] == 5 && clients[1].players().team_frags()[0] == 5 &&
                       !clients[0].players().flag_icon();
            }));
        CHECK(clients[0].players().player(clients[0].joined().object)->frags == 5 &&
              clients[1].speech().flag_notices == 1);
        CHECK(enter(2));
        active = 3;
        // Content preparation precedes each connection's monotonic-clock origin.
        now = std::max(now, NetworkClient::Clock::now());
        CHECK(until(
            [&]
            {
                return clients[2].ready() && clients[2].players().roster().size() == 3 &&
                       clients[2].players().team_frags()[0] == 5;
            }));
        CHECK(clients[2].players().player(clients[0].joined().object)->frags == 5 && !clients[2].players().flag_icon());
        red->GrabFlag(g_ctf.flags[0]->m_hObject);
        CHECK(red->ctf_error.empty());
        CHECK(until(
            [&]
            {
                const auto* flag = clients[2].world().get(blue_flag);
                return clients[1].players().flag_icon() == 2 && flag && flag->attached;
            }));
        CHECK(clients[1].disconnect(now));
        CHECK(until(
            [&]
            {
                const auto* flag = clients[2].world().get(blue_flag);
                return server.player_count() == 2 && clients[1].phase() == NetworkClient::Phase::Closed && flag &&
                       !flag->attached && clients[2].players().roster().size() == 2;
            }));
        CHECK(!g_ctf.flags[0]->with_player && clients[2].players().team_frags()[0] == 5 && network.close());
        std::cout << "CTF ENet admission, replicated carried flags, capture icons/scores/notices, late join and "
                     "carrier disconnect passed (server-triggered flag rules)\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "state")
        return state() ? 0 : 1;
    if (argc == 3 && std::string_view(argv[1]) == "speech")
        return speech(argv[2]) ? 0 : 1;
    if (argc == 3 && std::string_view(argv[1]) == "endpoints")
        return endpoints(argv[2]) ? 0 : 1;
    return 2;
}
