#include "client/network_client.h"
#include "ctf_route.h"
#include "game/game_settings.h"
#include <SDL3/SDL_process.h>
#include <charconv>
#include <iostream>
#include <thread>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
using Clock = NetworkClient::Clock;
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
    bool client(const char* data, const char* port_text, bool observer)
    {
        std::string error;
        unsigned port = 0;
        const auto end = port_text + std::char_traits<char>::length(port_text);
        const auto result = std::from_chars(port_text, end, port);
        CHECK(result.ec == std::errc{} && result.ptr == end && port && port <= 65535);
        Config config;
        config.data_dir = path_from_utf8(data);
        NetworkClient c;
        JoinProfile profile;
        profile.name = observer ? "CTF observer" : "CTF runner";
        profile.team = observer ? 2 : 1;
        profile.character = observer ? 1 : 0;
        CHECK(c.prepare(config, error) && c.connect("127.0.0.1", std::uint16_t(port), profile, Clock::now(), error));
        test_ctf::RoutePlanner planner;
        std::size_t at = 0;
        bool announced = false, planned = false, returning = false, carried = false, captured = false, changed = false,
             rotated = false, leaving = false;
        Joined first;
        ObjectId capturer;
        std::uint64_t force = 0, steps = 0;
        unsigned captures = 0, grabs = 0;
        DVector home;
        const auto deadline = Clock::now() + std::chrono::seconds(100);
        auto next_status = Clock::now();
        Clock::time_point leave_at{};
        while (Clock::now() < deadline)
        {
            MovementInput input;
            if (c.ready() && !leaving)
            {
                if (!announced)
                {
                    announced = true;
                    first = c.joined();
                    force = c.movement().setup().owner.force_revision;
                    CHECK(c.offer().mode == GAMETYPE_CTF && c.players().team() == profile.team);
                    std::cout << "Ready " << profile.name << " world=" << first.world << '\n' << std::flush;
                }
                if (c.joined().world == first.world)
                {
                    CHECK(c.movement().setup().owner.force_revision == force && c.weapons().state()->alive);
                    CHECK(c.command_acks == 0 &&
                          (!c.weapon_ack() || (!c.weapon_ack()->fired[0] && !c.weapon_ack()->fired[1])));
                    for (const auto& p : c.players().roster())
                        if (p.name == "CTF runner")
                            capturer = p.object;
                    for (const auto& object : c.world().frame().objects)
                    {
                        std::string path;
                        CHECK(resource_path(object.filename, path) || object.filename.empty());
                        if (path == "models/worldobjects/flagred.abc" && object.attached && object.parent == capturer)
                            carried = true;
                    }
                    const auto& body = c.movement().movement().body;
                    steps = std::max(steps, body.stair_steps);
                    if (!observer && !captured && !c.changing_level())
                    {
                        if (!planned || (!returning && c.players().flag_icon()))
                        {
                            DVector goal;
                            bool found = false;
                            for (const auto& object : c.world().frame().objects)
                            {
                                std::string path;
                                resource_path(object.filename, path);
                                if (path == "models/worldobjects/flagblue.abc")
                                    home = object.pose.position;
                                if (!planned && path == "models/worldobjects/flagred.abc")
                                {
                                    goal = object.pose.position;
                                    found = true;
                                }
                            }
                            if (planned)
                            {
                                returning = true;
                                goal = home;
                                found = true;
                                std::cout << "Carrying enemy flag, reports=" << c.reports << '\n' << std::flush;
                            }
                            CHECK(found);
                            planner.begin(body, goal);
                            planned = true;
                            at = 0;
                        }
                        if (planner.route.empty())
                        {
                            CHECK(planner.step(c.receiver().collision(), {c.joined().world, c.joined().object}));
                            if (!planner.route.empty())
                                std::cout << "Ground route nodes=" << planner.route.size()
                                          << ", visited=" << planner.visited << '\n';
                        }
                        else
                        {
                            while (at + 1 < planner.route.size() &&
                                   std::hypot(planner.route[at].x - body.position.x,
                                              planner.route[at].z - body.position.z) < 18 &&
                                   std::abs(planner.route[at].y - body.position.y) < 50)
                                ++at;
                            const auto delta = vector_subtract(planner.route[at], body.position);
                            CHECK(c.look(-c.pitch(), std::atan2(delta.x, delta.z) - c.yaw()));
                            input.forward = input.run = true;
                        }
                    }
                    if (Clock::now() >= next_status)
                    {
                        next_status = Clock::now() + std::chrono::seconds(5);
                        std::cout << "Position " << profile.name << ' ' << body.position.x << ' ' << body.position.y
                                  << ' ' << body.position.z << " route=" << at << '/' << planner.route.size()
                                  << " flag=" << unsigned(c.players().flag_icon()) << '\n'
                                  << std::flush;
                    }
                }
                else if (!rotated)
                {
                    CHECK(captured && changed && captures == 1 && carried && c.joined().world > first.world &&
                          c.joined().player > first.player);
                    CHECK(c.offer().path == "worlds/multi/ctf_cabana.dat" && c.players().team() == profile.team);
                    CHECK(!c.players().flag_icon() && c.players().team_frags()[0] == 0 &&
                          c.players().team_frags()[1] == 0);
                    for (const auto& p : c.players().roster())
                        CHECK(!p.frags);
                    rotated = true;
                    leave_at = Clock::now() + std::chrono::milliseconds(observer ? 1000 : 500);
                    CHECK(observer ? (grabs == 0 && c.speech().flag_sounds == 0)
                                   : (grabs == 1 && c.speech().flag_sounds == 1 && steps > 0 && c.reports > 100));
                    CHECK(!c.speech().missing_players);
                    std::cout << "Capture and score-limit rotation confirmed: " << profile.name
                              << ", captures=" << captures << ", stairs=" << steps << ", reports=" << c.reports
                              << ", weapon_wait_ticks=" << c.weapon_wait_ticks << '\n'
                              << std::flush;
                }
                if (rotated && Clock::now() >= leave_at)
                {
                    CHECK(c.disconnect(Clock::now()));
                    leaving = true;
                }
            }
            CHECK(c.update(Clock::now(), input, !leaving, error));
            std::vector<PrivateEvent> events;
            c.take_events(events);
            for (const auto& event : events)
            {
                CHECK(!std::holds_alternative<PlayerDead>(event.value) &&
                      !std::holds_alternative<FragEvent>(event.value));
                if (const auto* flag = std::get_if<FlagEvent>(&event.value))
                {
                    if (flag->kind == CTFEventKind::Grabbed)
                    {
                        CHECK(!observer && !grabs);
                        ++grabs;
                    }
                    if (flag->kind == CTFEventKind::Captured)
                    {
                        const auto* player = c.players().player(flag->capturer);
                        CHECK(!captured && player && player->name == "CTF runner" && flag->capturer == capturer &&
                              flag->points == 5 && flag->capturer_frags == 5);
                        CHECK(flag->team_frags[0] == 5 && flag->team_frags[1] == 0 &&
                              c.players().team_frags() == flag->team_frags && !c.players().flag_icon());
                        CHECK(observer ? c.speech().flag_notices == 1 : c.speech().flag_notices == 2);
                        captured = true;
                        ++captures;
                        std::cout << "Capture received " << profile.name << ", blue=5 red=0\n" << std::flush;
                    }
                }
                if (std::holds_alternative<LevelChanging>(event.value))
                {
                    CHECK(captured);
                    changed = true;
                }
            }
            if (c.phase() == NetworkClient::Phase::Closed)
            {
                CHECK(leaving && rotated);
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        error = "CTF route deadline";
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
            while (const auto n = SDL_ReadIO(stream, bytes, sizeof bytes))
            {
                if (output.size() + n > 131072)
                    return false;
                output.append(bytes, n);
            }
            return SDL_GetIOStatus(stream) != SDL_IO_STATUS_ERROR;
        }
    };
    bool parent(const char* self, const char* application, const char* data, bool hosted)
    {
        std::string error;
        Process host;
        const char* dedicated[]{
            application, "--data-dir", data,    "--world", "multi/ctf_jam", "--map", "multi/ctf_cabana",
            "--mode",    "ctf",        "--end", "frags",   "--frag-limit",  "5",     "--bind",
            "127.0.0.1", "--port",     "0",     "--ticks", "6000",          nullptr};
        const char* graphical[]{application,
                                "--data-dir",
                                data,
                                "--host",
                                "multi/ctf_jam",
                                "--map",
                                "multi/ctf_cabana",
                                "--mode",
                                "ctf",
                                "--end",
                                "frags",
                                "--frag-limit",
                                "5",
                                "--team",
                                "red",
                                "--name",
                                "Idle CTF host",
                                "--port",
                                "0",
                                // Frames, not ticks: at least the dedicated run's
                                // 100 s of server time on a 120 Hz display.
                                "--frames",
                                "12000",
                                "--capture",
                                "ctf-capture-host.png",
                                nullptr};
        host.handle = SDL_CreateProcess(hosted ? graphical : dedicated, true);
        error = SDL_GetError();
        CHECK(host.handle);
        auto deadline = Clock::now() + std::chrono::seconds(30);
        std::string port;
        while (Clock::now() < deadline && port.empty())
        {
            CHECK(host.read());
            auto at = host.output.find(", UDP ");
            if (at != std::string::npos)
            {
                auto end = host.output.find_first_not_of("0123456789", at + 6);
                if (end != std::string::npos)
                    port = host.output.substr(at + 6, end - at - 6);
            }
            int code = 0;
            if (SDL_WaitProcess(host.handle, false, &code))
            {
                error = host.output;
                CHECK(false);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(!port.empty());
        Process clients[2];
        const char* runner[]{self, "--peer", data, port.c_str(), "runner", nullptr};
        const char* observer[]{self, "--peer", data, port.c_str(), "observer", nullptr};
        clients[0].handle = SDL_CreateProcess(runner, true);
        CHECK(clients[0].handle);
        bool done[2]{}, host_done = false;
        int codes[2]{}, host_code = 0;
        deadline = Clock::now() + std::chrono::seconds(280); // 12000 host frames at 60 Hz.
        while (Clock::now() < deadline && (!done[0] || !done[1] || !host_done))
        {
            CHECK(host.read());
            if (!host_done)
                host_done = SDL_WaitProcess(host.handle, false, &host_code);
            for (unsigned i = 0; i < 2; ++i)
                if (clients[i].handle)
                {
                    CHECK(clients[i].read());
                    if (!done[i])
                        done[i] = SDL_WaitProcess(clients[i].handle, false, &codes[i]);
                }
            if (!clients[1].handle && clients[0].output.find("Carrying enemy flag") != std::string::npos)
            {
                clients[1].handle = SDL_CreateProcess(observer, true);
                CHECK(clients[1].handle);
            }
            if ((done[0] && codes[0]) || (done[1] && codes[1]))
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(host.read());
        for (auto& c : clients)
            if (c.handle)
                CHECK(c.read());
        std::cout << host.output << clients[0].output << clients[1].output;
        CHECK(host_done && host_code == 0 && done[0] && done[1] && !codes[0] && !codes[1]);
        for (const auto& c : clients)
            CHECK(c.output.find("Capture and score-limit rotation confirmed") != std::string::npos);
        CHECK(host.output.find("rotations=1") != std::string::npos);
        if (hosted)
        {
            std::error_code ec;
            CHECK(std::filesystem::file_size("ctf-capture-host.png", ec) > 4096 && !ec);
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 5 && std::string_view(argv[1]) == "--peer")
        return client(argv[2], argv[3], std::string_view(argv[4]) == "observer") ? 0 : 1;
    if (argc == 3 || (argc == 4 && std::string_view(argv[3]) == "--host"))
        return parent(argv[0], argv[1], argv[2], argc == 4) ? 0 : 1;
    return 2;
}
