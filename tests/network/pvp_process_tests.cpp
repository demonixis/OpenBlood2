#include "app/scoreboard.h"
#include "client/network_client.h"
#include "client/object_scene.h"
#include "engine/resources.h"
#include "engine/world.h"
#include "net/admission.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_process.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iostream>
#include <optional>
#include <set>
#include <thread>

using namespace OpenBlood2;
using namespace OpenBlood2::Net;
using Clock = std::chrono::steady_clock;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " : " << error << "\n";                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{

    bool client(const char* port_text, const char* data, bool victim, bool hosted, bool shotgun, bool team, bool rifle,
                bool napalm)
    {
        std::string error;
        unsigned port = 0;
        const auto end = port_text + std::char_traits<char>::length(port_text);
        const auto parsed = std::from_chars(port_text, end, port);
        CHECK(parsed.ec == std::errc{} && parsed.ptr == end && port && port <= 65535);
        Config config;
        config.data_dir = path_from_utf8(data);
        NetworkClient client;
        ScoreboardControl scores;
        client.audio().mixer.inspect_pcm = true;
        float audio_peak = 0;
        JoinProfile profile;
        profile.name = victim ? "PvP victim" : "PvP attacker";
        profile.team = team ? (victim ? 2 : 1) : 0;
        profile.weapons = {DBYTE(napalm    ? WEAP_NAPALMCANNON
                                 : rifle   ? WEAP_SNIPERRIFLE
                                 : shotgun ? WEAP_SHOTGUN
                                           : WEAP_BERETTA)};
        std::set<SoundId> animation_voices;
        std::set<std::uint64_t> missing_gib_zero;
        std::size_t peak_explosion_models = 0, peak_fragments = 0, peak_rings = 0, peak_trail_particles = 0;
        std::set<std::pair<std::uint32_t, std::uint32_t>> trail_owners;
        // The graphical host keeps Caleb. Distinct remote characters identify
        // the two combatants without treating the idle host as a test target.
        profile.character = hosted ? (victim ? 2 : 1) : 0;
        const std::string target_model =
            hosted ? (victim ? "models/characters/ophelia.abc" : "models/characters/ishmael.abc")
                   : "models/characters/caleb.abc";
        CHECK(client.prepare(config, error));
        CHECK(client.connect("127.0.0.1", std::uint16_t(port), profile, Clock::now(), error));
        const auto deadline = Clock::now() + std::chrono::seconds(35);
        ObjectId other;
        bool announced = false, leaving = false, hurt = false, use_sent = false, respawned = false;
        unsigned deaths = 0, frags = 0;
        std::uint64_t death_force = 0, max_impulse = 0;
        Clock::time_point death_at{}, finish_at{}, next_status = Clock::now();
        float initial_ammo = 0, detour = 0;
        DVector stall_from{};
        Clock::time_point stall_at = Clock::now();
        while (Clock::now() < deadline)
        {
            for (const auto& report : client.effects().runtime().blood_spurts.reports)
                if (!report.sound_error.empty())
                {
                    CHECK(report.sound_resource == "sounds/gibs/flesh/gib_impact0.wav" &&
                          !client.resources().find(report.sound_resource));
                    missing_gib_zero.insert(report.id);
                }
            MovementInput input;
            bool firing = false, visible = false;
            if (client.ready() && !leaving)
            {
                if (!announced)
                {
                    announced = true;
                    initial_ammo = client.weapons().state()->ammunition[0];
                    CHECK(initial_ammo > 0);
                    std::cout << "Ready " << (victim ? "victim" : "attacker") << " id=" << client.joined().object.slot
                              << '\n'
                              << std::flush;
                }
                if (!other)
                    for (const auto& object : client.receiver().world().frame().objects)
                    {
                        std::string name;
                        if (object.id != client.joined().object && resource_path(object.filename, name) &&
                            name == target_model && !object.attached)
                        {
                            other = object.id;
                            break;
                        }
                    }
                const auto* target = client.receiver().world().get(other);
                if ((!victim || hosted) && target && !frags && !deaths && !use_sent)
                {
                    auto from = client.movement().movement().body.position;
                    from.y += client.movement().setup().eye_level;
                    auto to = target->pose.position;
                    const auto delta = vector_subtract(to, from);
                    ClientSegmentHit hit;
                    const auto result = client.receiver().collision().intersect(
                        from, to, {client.joined().world, client.joined().object}, hit);
                    visible = result == SegmentResult::Miss || (result == SegmentResult::Hit && hit.object == other);
                    // Spawns are a random draw: a wall may separate the markers.
                    // While the target is hidden, a stalled walk turns 60 degrees.
                    const auto now = Clock::now();
                    if (visible)
                        detour = 0;
                    else if (std::hypot(from.x - stall_from.x, from.z - stall_from.z) > 8)
                        stall_from = from, stall_at = now;
                    else if (now - stall_at > std::chrono::milliseconds(500))
                        detour += 1.0472f, stall_from = from, stall_at = now;
                    const float yaw = std::atan2(delta.x, delta.z) + detour;
                    const float pitch = visible ? -std::atan2(delta.y, std::hypot(delta.x, delta.z)) : 0.0f;
                    CHECK(client.look(pitch - client.pitch(), yaw - client.yaw()));
                    const bool in_range = !shotgun || std::hypot(delta.x, delta.z) <= 250;
                    firing = !victim && visible && in_range;
                    input.forward = !visible || (shotgun && !victim && !in_range);
                }
                if (victim)
                {
                    hurt = hurt || client.hud().stats.health < 100;
                    max_impulse = std::max(max_impulse, client.movement().impulse_sequence());
                    if (deaths && !use_sent && Clock::now() >= death_at + std::chrono::milliseconds(500))
                    {
                        CHECK(client.command({CommandKind::Use}, error));
                        use_sent = true;
                    }
                    if (use_sent && !respawned && client.weapons().state()->alive &&
                        client.movement().setup().owner.force_revision > death_force &&
                        client.hud().stats.health == 100 && client.command_acks == 1)
                    {
                        CHECK(client.hud().stats.health == 100 && client.command_acks == 1 &&
                              client.commands_applied == 1);
                        CHECK(client.movement().impulse_sequence() == 0);
                        respawned = true;
                        finish_at = Clock::now() + std::chrono::seconds(1);
                        std::cout << "Respawn confirmed: health=100, new force="
                                  << client.movement().setup().owner.force_revision
                                  << ", prior impulses=" << max_impulse << '\n'
                                  << std::flush;
                    }
                    if (respawned && Clock::now() >= finish_at)
                    {
                        CHECK(deaths == 1 && frags == 1 && hurt && max_impulse);
                        CHECK(client.disconnect(Clock::now()));
                        leaving = true;
                    }
                }
                else if (frags && other && !target)
                {
                    CHECK(frags == 1 && !deaths && client.weapon_ack() && client.weapon_ack()->fired[0] > 0);
                    CHECK(client.weapons().state()->ammunition[0] < initial_ammo);
                    std::cout << "Lethal PvP confirmed: frags=" << frags << ", shots=" << client.weapon_ack()->fired[0]
                              << ", blocked=" << client.weapon_ack()->blocked
                              << ", ammo=" << client.weapons().state()->ammunition[0] << '\n'
                              << std::flush;
                    CHECK(client.disconnect(Clock::now()));
                    leaving = true;
                }
            }
            std::set<std::pair<std::uint32_t, std::uint32_t>> prior_owners;
            if (napalm)
                for (const auto& o : client.world().frame().objects)
                    prior_owners.emplace(o.id.slot, o.id.generation);
            CHECK(client.update(Clock::now(), input, !leaving, error, {firing}));
            peak_explosion_models =
                std::max(peak_explosion_models, client.effects().runtime().explosion_models.active.size());
            peak_fragments = std::max(peak_fragments, client.effects().runtime().explosion_fragments.active.size());
            peak_rings = std::max(peak_rings, client.effects().runtime().explosion_rings.active.size());
            for (const auto& trail : client.effects().runtime().object_particles.active)
                if (napalm && trail.effect.recipe == 2)
                {
                    // Effects consume the preceding displayed frame before presentation refresh.
                    // Exact owner retirement is checked by the independent runtime test.
                    CHECK(trail.owner.world == client.joined().world && trail.owner.object.generation);
                    if (!trail_owners.contains({trail.owner.object.slot, trail.owner.object.generation}))
                        CHECK(client.world().get(trail.owner) ||
                              prior_owners.contains({trail.owner.object.slot, trail.owner.object.generation}));
                    trail_owners.emplace(trail.owner.object.slot, trail.owner.object.generation);
                    peak_trail_particles = std::max(peak_trail_particles, trail.effect.particles.size());
                }
            audio_peak = std::max(audio_peak, client.audio().mixer.peak);
            if (shotgun)
                for (const auto& [id, voice] : client.audio().mixer.voices())
                {
                    std::string path;
                    if (voice.request.priority == 2 && resource_path(voice.request.resource, path) &&
                        path.starts_with("sounds/weapons/shotgun/"))
                    {
                        CHECK(id < client_sound_id_begin && voice.request.space == SoundSpace::Spatial &&
                              voice.request.outer_radius == 1000 && voice.request.inner_radius == 100);
                        animation_voices.insert(id);
                    }
                }
            std::vector<PrivateEvent> events;
            client.take_events(events);
            for (const auto& event : events)
            {
                scores.receive(event);
                if (std::holds_alternative<PlayerDead>(event.value))
                {
                    ++deaths;
                    death_force = client.movement().setup().owner.force_revision;
                    death_at = Clock::now();
                    CHECK(victim && scores.visible && scores.dead);
                    std::cout << "Death received at force " << death_force << '\n' << std::flush;
                }
                if (const auto* frag = std::get_if<FragEvent>(&event.value))
                {
                    CHECK(frag->killer_frags == 1 && frag->victim == (victim ? client.joined().object : other) &&
                          frag->killer == (victim ? other : client.joined().object));
                    const auto* killer = client.players().player(frag->killer);
                    const auto* killed = client.players().player(frag->victim);
                    CHECK(killer && killed && killer->name == "PvP attacker" && killed->name == "PvP victim" &&
                          killer->frags == 1 && killed->frags == 0 &&
                          client.players().roster().size() == (hosted ? 3 : 2));
                    CHECK(killer->character == (hosted ? 1 : 0) && killed->character == (hosted ? 2 : 0));
                    if (team)
                    {
                        CHECK(killer->team == 1 && killed->team == 2 && client.players().team() == (victim ? 2 : 1));
                        CHECK(client.players().team_frags()[0] == 1 && client.players().team_frags()[1] == 0 &&
                              frag->team_frags == client.players().team_frags());
                        std::cout << "Team frag confirmed: blue=1, red=0, protected teams active\n";
                    }
                    CHECK(client.players().scores().size() == client.players().roster().size() &&
                          client.players().scores().front().object == frag->killer &&
                          client.players().scores().front().frags == 1);
                    CHECK(client.speech().joined_notices == (hosted ? 3 : 2));
                    CHECK(client.speech().frag_events == 1 && !client.speech().missing_players);
                    if (victim)
                        CHECK(client.speech().frag_notices == 1 && !client.speech().notices().empty() &&
                              !client.speech().frag_communications);
                    else
                        CHECK(client.speech().frag_communications == 1 && !client.speech().frag_notices);
                    ++frags;
                    std::cout << "Frag received: killer=" << frag->killer.slot << " victim=" << frag->victim.slot
                              << '\n'
                              << std::flush;
                }
            }
            if (client.phase() == NetworkClient::Phase::Closed)
            {
                CHECK(client.audio().started > 0 && client.audio().local_started > 0 && audio_peak > 0);
                CHECK(!scores.dead && !scores.visible);
                if (!victim)
                    CHECK(client.speech().left_notices == 1);
                std::cout << "Player notices: joined=" << client.speech().joined_notices
                          << ", left=" << client.speech().left_notices << '\n';
                CHECK(client.effects().received > 0 && !client.effects().failed);
                if (napalm)
                {
                    CHECK(client.effects().received_kinds[5] >= 6 && peak_explosion_models && peak_rings);
                    CHECK(!trail_owners.empty() && peak_trail_particles);
                    CHECK(client.effects().runtime().object_particles.active.empty());
                    std::cout << "Remote Napalm trails: owners=" << trail_owners.size()
                              << ", peak particles=" << peak_trail_particles << ", removed on teardown\n";
                    std::cout << "Remote explosion peaks: models=" << peak_explosion_models
                              << ", fragments=" << peak_fragments << ", rings=" << peak_rings
                              << ", recipes=" << client.effects().received_kinds[5] << '\n';
                }
                else
                    CHECK(client.effects().received_kinds[0] > 0 && client.effects().received_kinds[2] > 0);
                if (rifle)
                    CHECK(client.effects().received_kinds[3] > 0); // Original raw damage >25 guarantees jets.
                CHECK(client.audio().effects_failed == missing_gib_zero.size());
                std::cout << "Remote impacts: received=" << client.effects().received
                          << ", failed=" << client.effects().failed
                          << ", cosmetic_sounds=" << client.audio().effects_started
                          << ", audio_failures=" << client.audio().effects_failed
                          << ", verified_missing_gib_zero=" << missing_gib_zero.size()
                          << ", kinds=" << client.effects().received_kinds[0] << '/'
                          << client.effects().received_kinds[1] << '/' << client.effects().received_kinds[2] << '/'
                          << client.effects().received_kinds[3] << '/' << client.effects().received_kinds[4] << '\n';
                std::cout << "Remote audio: starts=" << client.audio().started << ", stops=" << client.audio().stopped
                          << ", natural=" << client.audio().completed
                          << ", private_starts=" << client.audio().local_started << ", peak=" << audio_peak << '\n';
                if (shotgun)
                {
                    CHECK(!animation_voices.empty());
                    std::cout << "Authoritative shotgun animation voices=" << animation_voices.size() << '\n';
                }
                return leaving && (victim ? respawned : frags == 1);
            }
            if (announced && Clock::now() >= next_status)
            {
                next_status = Clock::now() + std::chrono::seconds(2);
                const auto& pos = client.movement().movement().body.position;
                std::cout << "Status " << (victim ? "victim" : "attacker") << " pos=" << pos.x << ',' << pos.y << ','
                          << pos.z << " other=" << other.slot << " visible=" << visible
                          << " hp=" << client.hud().stats.health
                          << " shots=" << (client.weapon_ack() ? client.weapon_ack()->fired[0] : 0) << '\n'
                          << std::flush;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        error = "PvP client deadline";
        CHECK(false);
    }
    struct Process
    {
        SDL_Process* handle = nullptr;
        std::string output;
        ~Process()
        {
            if (handle)
            {
                int code = 0;
                if (!SDL_WaitProcess(handle, false, &code))
                {
                    SDL_KillProcess(handle, true);
                    SDL_WaitProcess(handle, true, &code);
                }
                SDL_DestroyProcess(handle);
            }
        }
        bool read()
        {
            auto* stream = SDL_GetProcessOutput(handle);
            if (!stream)
                return false;
            char bytes[4096];
            while (const auto count = SDL_ReadIO(stream, bytes, sizeof bytes))
            {
                if (output.size() + count > 65536)
                    return false;
                output.append(bytes, count);
            }
            return SDL_GetIOStatus(stream) != SDL_IO_STATUS_ERROR;
        }
    };
    bool parent(const char* self, const char* server, const char* data, bool hosted, bool shotgun, bool team,
                bool rifle, bool napalm)
    {
        std::string error;
        Process host;
        const char* dedicated_args[]{server,
                                     "--data-dir",
                                     data,
                                     "--world",
                                     "multi/bb_fields",
                                     "--mode",
                                     team ? "team" : "bloodbath",
                                     "--friendly-fire",
                                     team ? "off" : "on",
                                     "--bind",
                                     "127.0.0.1",
                                     "--port",
                                     "0",
                                     "--ticks",
                                     "2400",
                                     nullptr};
        const char* hosted_args[]{server,
                                  "--data-dir",
                                  data,
                                  "--host",
                                  "multi/bb_fields",
                                  "--port",
                                  "0",
                                  "--frames",
                                  "1200",
                                  "--name",
                                  "Idle host",
                                  "--mode",
                                  team ? "team" : "bloodbath",
                                  "--team",
                                  "blue",
                                  "--friendly-fire",
                                  team ? "off" : "on",
                                  "--capture",
                                  team ? "team-pvp-host.png" : "pvp-host.png",
                                  nullptr};
        host.handle = SDL_CreateProcess(hosted ? hosted_args : dedicated_args, true);
        error = SDL_GetError();
        CHECK(host.handle);
        auto deadline = Clock::now() + std::chrono::seconds(45);
        std::string port;
        while (Clock::now() < deadline && port.empty())
        {
            CHECK(host.read());
            const auto start = host.output.find(", UDP ");
            if (start != std::string::npos)
            {
                const auto end = host.output.find_first_not_of("0123456789", start + 6);
                if (end != std::string::npos)
                    port = host.output.substr(start + 6, end - start - 6);
            }
            if (port.empty())
            {
                int exit = 0;
                if (SDL_WaitProcess(host.handle, false, &exit))
                {
                    error = host.output;
                    CHECK(false);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        CHECK(!port.empty());
        Process clients[2];
        for (unsigned i = 0; i < 2; ++i)
        {
            if (i)
            {
                deadline = Clock::now() + std::chrono::seconds(15);
                while (clients[0].output.find("Ready ") == std::string::npos && Clock::now() < deadline)
                {
                    CHECK(clients[0].read());
                    int code = 0;
                    CHECK(!SDL_WaitProcess(clients[0].handle, false, &code));
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                CHECK(clients[0].output.find("Ready ") != std::string::npos);
            }
            const char* child_args[]{self,
                                     "--peer",
                                     port.c_str(),
                                     data,
                                     i ? "victim" : "attacker",
                                     hosted ? "host" : "dedicated",
                                     napalm    ? "napalm"
                                     : rifle   ? "rifle"
                                     : shotgun ? "shotgun"
                                               : "beretta",
                                     team ? "team" : "bloodbath",
                                     nullptr};
            clients[i].handle = SDL_CreateProcess(child_args, true);
            error = SDL_GetError();
            CHECK(clients[i].handle);
        }
        bool finished[2]{};
        int codes[2]{};
        bool host_done = false;
        int host_code = 0;
        deadline = Clock::now() + std::chrono::seconds(100);
        while (Clock::now() < deadline && (!finished[0] || !finished[1] || !host_done))
        {
            CHECK(host.read());
            if (!host_done)
                host_done = SDL_WaitProcess(host.handle, false, &host_code);
            for (unsigned i = 0; i < 2; ++i)
            {
                CHECK(clients[i].read());
                if (!finished[i])
                    finished[i] = SDL_WaitProcess(clients[i].handle, false, &codes[i]);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(host.read());
        for (auto& peer : clients)
            CHECK(peer.read());
        std::cout << host.output << clients[0].output << clients[1].output;
        CHECK(host_done && host_code == 0 && finished[0] && finished[1] && codes[0] == 0 && codes[1] == 0);
        CHECK(host.output.find(hosted ? "admitted=3, departed=3" : "admitted=2, departed=2, rejected=0, timed_out=0") !=
              std::string::npos);
        if (hosted)
        {
            std::error_code ec;
            CHECK(std::filesystem::file_size(team ? "team-pvp-host.png" : "pvp-host.png", ec) > 4096 && !ec);
        }
        CHECK(clients[0].output.find("Ready ") != std::string::npos &&
              clients[1].output.find("Ready ") != std::string::npos);
        CHECK(clients[0].output.find("Lethal PvP confirmed:") != std::string::npos);
        CHECK(clients[1].output.find("Respawn confirmed:") != std::string::npos);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 8 && std::string_view(argv[1]) == "--peer")
        return client(argv[2], argv[3], std::string_view(argv[4]) == "victim", std::string_view(argv[5]) == "host",
                      std::string_view(argv[6]) == "shotgun", std::string_view(argv[7]) == "team",
                      std::string_view(argv[6]) == "rifle", std::string_view(argv[6]) == "napalm")
                   ? 0
                   : 1;
    bool hosted = false, shotgun = false, team = false, rifle = false, napalm = false;
    for (int i = 3; i < argc; ++i)
    {
        if (std::string_view(argv[i]) == "--host")
            hosted = true;
        else if (std::string_view(argv[i]) == "--team")
            team = true;
        else if (std::string_view(argv[i]) == "--napalm")
            napalm = true;
        else if (std::string_view(argv[i]) == "--rifle")
            rifle = true;
        else if (std::string_view(argv[i]) == "--shotgun")
            shotgun = true;
        else
            return 2;
    }
    return argc >= 3 && argc <= 6 && parent(argv[0], argv[1], argv[2], hosted, shotgun, team, rifle, napalm) ? 0 : 1;
}
