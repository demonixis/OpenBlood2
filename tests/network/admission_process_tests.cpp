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
#include <iostream>
#include <optional>
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
    bool client(const char* port_text, const char* data, bool observer, bool drive)
    {
        std::string error;
        unsigned port = 0;
        const auto end = port_text + std::char_traits<char>::length(port_text);
        const auto parsed = std::from_chars(port_text, end, port);
        CHECK(parsed.ec == std::errc{} && parsed.ptr == end && port && port <= 65535);
        Config config;
        config.data_dir = path_from_utf8(data);
        NetworkClient client;
        CHECK(client.connect(config, "127.0.0.1", std::uint16_t(port), {}, Clock::now(), error));
        const auto deadline = Clock::now() + std::chrono::seconds(18);
        Clock::time_point leave_at;
        bool announced = false, leaving = false, saw_arrival = false, saw_departure = false, displaced = false;
        unsigned startup = 0, camera_frames = 0;
        DVector initial;
        ObjectId other_player;
        while (Clock::now() < deadline)
        {
            MovementInput input;
            input.forward = drive && !leaving;
            CHECK(client.update(Clock::now(), input, !leaving, error));
            std::vector<PrivateEvent> events;
            client.take_events(events);
            for (const auto& value : events)
                startup += std::holds_alternative<BloodbathStart>(value.value);
            if (client.phase() == NetworkClient::Phase::Closed)
                return announced && leaving && client.reason() == "Disconnected";
            if (client.ready())
            {
                const auto& world = client.receiver().world();
                if (!announced)
                {
                    ModelView scene;
                    CHECK(append_client_models(world, true, scene, client.resources(), error));
                    CHECK(!scene.entries.empty() && client.receiver().collision().frame().primary);
                    CHECK(client.weapons().state() && client.weapons().views()[0].type == 1 &&
                          client.weapons().views()[0].model && !client.weapons().views()[1].model);
                    std::cout << "Replicated " << world.frame().objects.size() << " objects, " << scene.entries.size()
                              << " model entries through NetworkClient in world " << client.joined().world << "\n"
                              << std::flush;
                    announced = true;
                    initial = client.movement().movement().body.position;
                    leave_at = Clock::now() + std::chrono::seconds(observer ? 6 : 1);
                }
                for (const auto& object : world.frame().objects)
                {
                    std::string name;
                    if (object.id != client.joined().object && resource_path(object.filename, name) &&
                        name == "models/characters/caleb.abc")
                    {
                        other_player = object.id;
                        saw_arrival = true;
                    }
                }
                if (other_player && !world.get(other_player))
                    saw_departure = true;
                const auto delta = vector_subtract(client.movement().movement().body.position, initial);
                displaced |= vector_dot(delta, delta) > 1;
                ++camera_frames;
                CHECK(client.camera().position.x == client.movement().movement().body.position.x);
                if (!leaving && Clock::now() >= leave_at)
                {
                    CHECK(client.state_packets && (!observer || (saw_arrival && saw_departure)));
                    CHECK(client.event_sequence() && client.hud().stats.health > 0 && startup == 1);
                    CHECK(client.camera().kind == ClientCameraKind::FirstPerson && camera_frames);
                    CHECK(client.weapons().updates && !client.weapons().views()[0].shots &&
                          client.weapons().state()->alive && client.weapons().state()->fire_allowed);
                    if (observer)
                        CHECK(client.weapons().views()[0].state == WS_REST);
                    std::cout << "Private weapon type=" << unsigned(client.weapons().views()[0].type)
                              << ", ammo=" << client.weapons().state()->ammunition[0]
                              << ", animation_updates=" << client.weapons().updates
                              << ", shots=" << client.weapons().views()[0].shots << "\n";
                    std::cout << "Camera frames=" << camera_frames << ", predicted eye height="
                              << client.camera().view.position.y - client.movement().movement().body.position.y
                              << ", mapped clock=" << client.time() << "\n";
                    std::cout << "Private events=" << client.event_sequence()
                              << ", health=" << client.hud().stats.health << ", startup_announcements=" << startup
                              << "\n";
                    if (drive)
                    {
                        CHECK(displaced && client.reports >= 4 && client.movement().corrections >= 3);
                        std::cout << "Predicted movement: reports=" << client.reports
                                  << ", corrections=" << client.movement().corrections
                                  << ", replayed_frames=" << client.movement().replays
                                  << ", substeps=" << client.movement().manager().substeps << "\n";
                    }
                    if (observer)
                        std::cout << "Observed remote player creation and departure\n";
                    std::cout << std::flush;
                    CHECK(client.disconnect(Clock::now()));
                    leaving = true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        error = "NetworkClient process deadline";
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
    bool parent(const char* self, const char* server, const char* data, bool drive)
    {
        std::string error;
        Process host;
        const char* args[]{server,   "--data-dir", data,     "--world", "multi/bb_crypt", "--mode", "bloodbath",
                           "--bind", "127.0.0.1",  "--port", "0",       "--ticks",        "900",    nullptr};
        host.handle = SDL_CreateProcess(args, true);
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
                const auto end = host.output.find(", data ", start);
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
                while (clients[0].output.find("Replicated ") == std::string::npos && Clock::now() < deadline)
                {
                    CHECK(clients[0].read());
                    int code = 0;
                    CHECK(!SDL_WaitProcess(clients[0].handle, false, &code));
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                CHECK(clients[0].output.find("Replicated ") != std::string::npos);
            }
            const char* child_args[]{
                self,   "--peer", port.c_str(), data, i ? "visitor" : "observer", drive ? "movement" : "passive",
                nullptr};
            clients[i].handle = SDL_CreateProcess(child_args, true);
            error = SDL_GetError();
            CHECK(clients[i].handle);
        }
        bool finished[2]{};
        int codes[2]{};
        bool host_done = false;
        int host_code = 0;
        deadline = Clock::now() + std::chrono::seconds(25);
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
        CHECK(host.output.find("admitted=2, departed=2, rejected=0, timed_out=0") != std::string::npos);
        CHECK(clients[0].output.find("Replicated ") != std::string::npos &&
              clients[1].output.find("Replicated ") != std::string::npos);
        CHECK(clients[0].output.find("Observed remote player creation and departure") != std::string::npos);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 6 && std::string_view(argv[1]) == "--peer")
        return client(argv[2], argv[3], std::string_view(argv[4]) == "observer",
                      std::string_view(argv[5]) == "movement")
                   ? 0
                   : 1;
    return (argc == 3 || argc == 4) && parent(argv[0], argv[1], argv[2], argc == 4) ? 0 : 1;
}
