#include "app/scoreboard.h"
#include "client/network_client.h"
#include "game/game_settings.h"
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
            std::cerr << __LINE__ << ": " #x ": " << error << '\n';                                                    \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool rules()
    {
        std::string error;
        MultiplayerRules r;
        CHECK(r.valid() && r.frags == 25 && r.minutes == 10 && Options{}.network_rules.minutes == 15);
        std::array<int, 3> scores{-1, 24, 0};
        CHECK(!match_limit_reached(r, 999999, scores));
        scores[1] = 25;
        CHECK(match_limit_reached(r, 0, scores));
        r.end = MatchEnd::Time;
        CHECK(!match_limit_reached(r, 35999, scores) && match_limit_reached(r, 36000, {}));
        r.end = MatchEnd::Either;
        CHECK(match_limit_reached(r, 0, scores) && match_limit_reached(r, 36000, {}));
        r.end = MatchEnd::Never;
        CHECK(!match_limit_reached(r, UINT64_MAX, scores));
        r.frags = 0;
        r.end = MatchEnd::Frags;
        CHECK(!match_limit_reached(r, 0, {}) && match_limit_reached(r, 0, scores));
        r.minutes = 0;
        r.end = MatchEnd::Time;
        CHECK(match_limit_reached(r, 0, {}));
        CHECK(parse_match_option("--end", "either", r) && r.end == MatchEnd::Either);
        CHECK(!parse_match_option("--end", "both", r) && !parse_match_option("--frag-limit", "-1", r));
        CHECK(!parse_match_option("--frag-limit", "1000001", r));
        CHECK(!parse_match_option("--time-limit", "10001", r) && !parse_match_option("--time-limit", "4294967295", r));
        CHECK(parse_match_option("--time-limit", "10000", r) && r.valid() && !match_limit_reached(r, 36000, {}));
        for (int i = 0; i < 49; ++i)
            CHECK(parse_match_option("--map", "multi/bb_crypt", r));
        CHECK(!parse_match_option("--map", "multi/bb_fields", r));
        Options parsed;
        const std::array<std::string_view, 7> args{"--host", "multi/bb_crypt", "--end", "time", "--time-limit",
                                                   "0",      "--help"};
        CHECK(parse_options(args, parsed, error) && parsed.network_rules.minutes == 0);
        const std::array<std::string_view, 4> bad{"--connect", "localhost", "--end", "time"};
        CHECK(!parse_options(bad, parsed, error));
        std::cout << "Original frag/time/either/never thresholds, zero and large values, shared host/dedicated options "
                     "passed\n";
        return true;
    }
    bool session(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        ServerSession session;
        MultiplayerRules rules;
        rules.frags = 1;
        rules.following_maps = {"multi/bb_fields"};
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error, rules));
        Joined a, b;
        CHECK(session.join({}, a, error) && session.join({}, b, error));
        auto& game = const_cast<GameSession&>(session.game());
        auto* attacker = dynamic_cast<CPlayerObj*>(game.objects.get(a.object)->game.get());
        auto* victim = dynamic_cast<CPlayerObj*>(game.objects.get(b.object)->game.get());
        CHECK(attacker && victim);
        CHECK(session.begin_audio(a.player, error));
        CHECK(attacker->HandleWeaponSound(WeaponSoundAction::Loop, "sounds/weapons/beretta/fire.wav"));
        const auto loop = attacker->m_hWeapSound;
        CHECK(loop && game.server.audio.get(loop));
        victim->m_damage.last_damage.attacker = attacker->m_hObject;
        victim->UpdateFragStatus();
        CHECK(attacker->m_Frags == 1 && session.tick(error) && session.changing_level() && !session.rotation_due());
        CHECK(!attacker->m_hWeapSound && !game.server.audio.get(loop));
        std::vector<AudioEvent> sounds;
        CHECK(session.take_audio_events(a.player, sounds, error));
        CHECK(std::count_if(sounds.begin(), sounds.end(),
                            [&](const auto& event)
                            {
                                const auto* end = std::get_if<SoundEnd>(&event.value);
                                return end && end->id == loop && !end->completed;
                            }) == 1);
        std::vector<PrivateEvent> events;
        CHECK(session.take_private_events(a.player, events, error));
        CHECK(std::count_if(events.begin(), events.end(),
                            [](const auto& e) { return std::holds_alternative<LevelChanging>(e.value); }) == 1);
        Net::MovementSetup old;
        CHECK(session.movement_setup(a.player, old, error));
        CHECK(session.queue_command(a.player, {old.owner, 1, {CommandKind::Use, 0, {}}}) ==
              ServerSession::CommandResult::Queued);
        CHECK(session.tick(error));
        std::vector<CommandAck> acks;
        CHECK(session.take_command_acks(a.player, acks) && acks.size() == 1 && !acks.front().applied);
        // Original strict >7.5s, not >=; removing the winner does not cancel/restart the wait.
        CHECK(session.clear_players());
        while (session.ticks() < 450)
            CHECK(session.tick(error));
        CHECK(!session.rotation_due() && !session.rotate(error));
        CHECK(session.tick(error) && session.rotation_due());
        const auto old_world = session.world_id();
        CHECK(session.rotate(error) && session.world_id() > old_world && session.ticks() == 0 &&
              session.rotations == 1 && !session.changing_level() &&
              session.game().world_path == "worlds/multi/bb_fields.dat");
        Joined fresh;
        CHECK(session.join({}, fresh, error) && fresh.player > b.player && fresh.world != a.world);
        const auto* player = dynamic_cast<const CPlayerObj*>(game.objects.get(fresh.object)->game.get());
        CHECK(player && player->m_Frags == 0 && !player->m_bDead && !player->m_damage.IsDead());
        CHECK(session.queue_command(fresh.player, {old.owner, 2, {CommandKind::Use, 0, {}}}) !=
              ServerSession::CommandResult::Queued);
        CHECK(session.clear_players());
        std::cout << "Original frag triggers one end event, blocked intermission command, strict 450/451-tick delay, "
                     "weapon-loop stop, winner departure, fresh map/score/life and stale command rejection passed\n";
        return true;
    }
    bool retired_packets(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        MultiplayerRules rules;
        rules.end = MatchEnd::Time;
        rules.minutes = 0;
        rules.following_maps = {"multi/bb_fields"};
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error, rules));
        Transport wire;
        CHECK(wire.listen("127.0.0.1", 0, 1, error));
        NetworkClient client;
        CHECK(client.prepare(config, error) &&
              client.connect("127.0.0.1", wire.port(), {}, NetworkClient::Clock::now(), error));
        PeerId peer = 0;
        Joined joined;
        WorldSnapshot snapshot;
        MovementSetup setup;
        const auto send_admission = [&](const AdmissionMessage& v)
        {
            std::vector<std::uint8_t> bytes;
            return encode_admission(v, bytes) && wire.send(peer, Channel::Control, bytes, error);
        };
        const auto publish = [&]()
        {
            CHECK(session.movement_setup(joined.player, setup, error));
            std::vector<std::uint8_t> bytes;
            CHECK(encode_movement(setup, bytes) && wire.send(peer, Channel::Control, bytes, error));
            WeaponLoadout weapons;
            CHECK(session.weapon_loadout(joined.player, weapons, error));
            std::vector<PrivateEvent> events;
            CHECK(session.take_private_events(joined.player, events, error));
            for (const auto& e : events)
                CHECK(encode_private(e, bytes) && wire.send(peer, Channel::Control, bytes, error));
            return true;
        };
        const auto pump = [&]()
        {
            for (unsigned i = 0; i < 64; ++i)
            {
                Event event;
                const auto result = wire.poll(event, 0, error);
                CHECK(result != PollResult::Failed);
                if (result == PollResult::Idle)
                    break;
                if (event.kind == EventKind::Connected)
                {
                    peer = event.peer;
                    continue;
                }
                CHECK(event.kind != EventKind::Disconnected);
                if (is_weapon_frame_packet(event.bytes))
                {
                    WeaponFrameMessage value;
                    CHECK(decode_weapon_frame(event.bytes, value) == DecodeWeaponFrame::Ok);
                    if (const auto* frame = std::get_if<WeaponFrame>(&value))
                    {
                        if (frame->owner.world == session.world_id())
                        {
                            std::vector<std::uint8_t> bytes;
                            CHECK(encode_weapon_frame(WeaponFrameAck{frame->owner, frame->sequence, {}, 0}, bytes));
                            CHECK(wire.send(peer, Channel::Control, bytes, error));
                        }
                    }
                    continue;
                }
                if (is_movement_packet(event.bytes))
                    continue;
                AdmissionMessage value;
                CHECK(decode_admission(event.bytes, value) == DecodeAdmission::Ok);
                if (std::holds_alternative<Hello>(value))
                {
                    CHECK(send_admission(WorldOffer{session.world_id(), 1, 1, session.game().world_path}));
                }
                else if (const auto* ready = std::get_if<Ready>(&value))
                {
                    CHECK(session.join(ready->profile, joined, error) && send_admission(joined));
                    CHECK(session.snapshot(joined.player, snapshot, error));
                    std::vector<std::vector<std::uint8_t>> chunks;
                    CHECK(encode_baseline_chunks(snapshot, chunks));
                    for (const auto& bytes : chunks)
                        CHECK(wire.send(peer, Channel::Baseline, bytes, error));
                }
                else if (std::holds_alternative<BaselineReady>(value))
                    CHECK(publish());
            }
            wire.flush();
            CHECK(client.update(NetworkClient::Clock::now(), {}, false, error));
            std::vector<PrivateEvent> consumed;
            client.take_events(consumed);
            return true;
        };
        const auto await_world = [&](std::uint64_t world)
        {
            const auto deadline = NetworkClient::Clock::now() + std::chrono::seconds(10);
            while ((!client.ready() || client.joined().world != world) && NetworkClient::Clock::now() < deadline)
            {
                CHECK(pump());
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            CHECK(client.ready() && client.joined().world == world);
            return true;
        };
        CHECK(await_world(session.world_id()));
        const auto old_snapshot = snapshot;
        const auto old_setup = setup;
        const auto old_joined = joined;
        for (unsigned i = 0; i < 451; ++i)
            CHECK(session.tick(error));
        CHECK(session.rotation_due() && session.rotate(error));
        CHECK(send_admission(WorldOffer{session.world_id(), 1, 1, session.game().world_path}));
        // Fresh ENet sequence numbers carrying deliberately delayed old payloads:
        // cross-channel old baselines/states and setup must not poison new loading.
        std::vector<std::vector<std::uint8_t>> chunks;
        auto late = old_snapshot;
        ++late.sequence;
        late.time = .25;
        late.objects.front().user_flags ^= 2;
        WorldPatch patch;
        bool changed = false;
        CHECK(make_world_patch(old_snapshot, late, patch, changed) && changed && encode_patch_chunks(patch, chunks));
        for (const auto& bytes : chunks)
            CHECK(wire.send(peer, Channel::Baseline, bytes, error));
        CHECK(encode_state_packets(late, late.sequence, chunks));
        for (const auto& bytes : chunks)
            CHECK(wire.send(peer, Channel::State, bytes, error));
        std::vector<std::uint8_t> bytes;
        CHECK(encode_movement(old_setup, bytes) && wire.send(peer, Channel::Control, bytes, error));
        CHECK(encode_private({old_joined, 999, 0, PlayerDead{}}, bytes) &&
              wire.send(peer, Channel::Control, bytes, error));
        wire.flush();
        CHECK(await_world(session.world_id()));
        CHECK(client.offer().path == "worlds/multi/bb_fields.dat" && client.joined().player > old_joined.player &&
              !client.camera().death.active && client.world().frame().world == session.world_id());
        CHECK(session.clear_players());
        wire.close();
        std::cout << "Deliberately delayed baseline/state/setup/private death payloads across world replacement were "
                     "ignored without corrupting the fresh client world\n";
        return true;
    }
    bool endpoints(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        ServerSession server;
        MultiplayerRules rules;
        rules.end = MatchEnd::Time;
        rules.minutes = 0;
        rules.following_maps = {"multi/bb_fields"};
        CHECK(server.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error, rules));
        ServerNetwork network(server);
        CHECK(network.listen("127.0.0.1", 0, 3, error));
        NetworkClient clients[2];
        for (unsigned i = 0; i < 2; ++i)
        {
            JoinProfile p;
            p.name = i ? "Second" : "First";
            p.character = i;
            CHECK(clients[i].prepare(config, error));
            CHECK(clients[i].connect("127.0.0.1", network.port(), p, NetworkClient::Clock::now(), error));
        }
        std::array<std::uint64_t, 2> worlds{}, players{};
        std::array<unsigned, 2> entries{}, changes{};
        std::array<ScoreboardControl, 2> scores;
        auto now = NetworkClient::Clock::now();
        for (unsigned tick = 0; tick < 1800; ++tick)
        {
            now += std::chrono::microseconds(16667);
            CHECK(server.tick(error) && network.pump(now, error));
            for (unsigned i = 0; i < 2; ++i)
            {
                auto& c = clients[i];
                CHECK(c.update(now, {}, true, error));
                CHECK(c.phase() != NetworkClient::Phase::Closed);
                if (c.ready() && c.joined().world != worlds[i])
                {
                    CHECK(c.joined().world > worlds[i] && c.joined().player > players[i]);
                    worlds[i] = c.joined().world;
                    players[i] = c.joined().player;
                    ++entries[i];
                    scores[i] = {};
                    CHECK(c.world().frame().world == worlds[i] && c.receiver().collision().frame().world == worlds[i]);
                    for (const auto& p : c.players().roster())
                        CHECK(p.frags == 0);
                }
                std::vector<PrivateEvent> events;
                c.take_events(events);
                for (const auto& e : events)
                {
                    scores[i].receive(e);
                    if (std::holds_alternative<LevelChanging>(e.value))
                    {
                        ++changes[i];
                        CHECK(c.changing_level() && scores[i].visible && c.speech().level_sound_error.empty());
                    }
                }
                if (c.changing_level())
                {
                    const auto yaw = c.yaw();
                    CHECK(c.look(0, .25f) && c.yaw() == yaw);
                    CHECK(!c.command({CommandKind::Use, 0, {}}, error));
                }
            }
            if (entries[0] >= 3 && entries[1] >= 3 && changes[0] >= 3 && changes[1] >= 3)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(entries[0] >= 3 && entries[1] >= 3 && changes[0] >= 3 && changes[1] >= 3 && server.rotations >= 2);
        CHECK(server.game().world_path == "worlds/multi/bb_crypt.dat");
        CHECK(network.close());
        std::cout
            << "Two independent endpoints retained ENet peers across Crypt -> Fields -> Crypt, fresh "
               "replica/collision/player clocks, three level notices/bells, neutral input and reset frags passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "rules")
        return rules() ? 0 : 1;
    if (argc == 3 && std::string_view(argv[1]) == "session")
        return session(argv[2]) ? 0 : 1;
    if (argc == 3 && std::string_view(argv[1]) == "retired")
        return retired_packets(argv[2]) ? 0 : 1;
    if (argc == 3 && std::string_view(argv[1]) == "endpoints")
        return endpoints(argv[2]) ? 0 : 1;
    return 2;
}
