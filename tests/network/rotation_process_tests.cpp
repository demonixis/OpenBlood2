#include "client/network_client.h"
#include <SDL3/SDL_process.h>
#include <charconv>
#include <chrono>
#include <iostream>
#include <thread>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
using Clock = std::chrono::steady_clock;
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
    bool client(const char* port_text, const char* data, const char* name)
    {
        std::string error;
        unsigned port = 0;
        const auto end = port_text + std::char_traits<char>::length(port_text);
        const auto result = std::from_chars(port_text, end, port);
        CHECK(result.ec == std::errc{} && result.ptr == end && port && port <= 65535);
        Config config;
        config.data_dir = path_from_utf8(data);
        NetworkClient client;
        JoinProfile profile;
        profile.name = name;
        profile.character = name[0] == 'S' ? 2 : 1;
        CHECK(client.prepare(config, error));
        CHECK(client.connect("127.0.0.1", std::uint16_t(port), profile, Clock::now(), error));
        unsigned entries = 0, changes = 0;
        std::uint64_t previous_world = 0, previous_player = 0;
        bool leaving = false;
        const auto deadline = Clock::now() + std::chrono::seconds(55);
        while (Clock::now() < deadline)
        {
            CHECK(client.update(Clock::now(), {}, true, error));
            if (client.ready() && client.joined().world != previous_world)
            {
                CHECK(client.joined().world > previous_world && client.joined().player > previous_player);
                previous_world = client.joined().world;
                previous_player = client.joined().player;
                ++entries;
                CHECK(client.world().frame().world == previous_world &&
                      client.receiver().collision().frame().world == previous_world);
                for (const auto& p : client.players().roster())
                    CHECK(p.frags == 0);
                std::cout << "Ready " << name << " entry=" << entries << " epoch=" << previous_world
                          << " map=" << client.offer().path << '\n'
                          << std::flush;
            }
            std::vector<PrivateEvent> events;
            client.take_events(events);
            for (const auto& e : events)
                if (std::holds_alternative<LevelChanging>(e.value))
                {
                    ++changes;
                    CHECK(client.changing_level() && client.speech().level_sound_error.empty());
                    CHECK(!client.command({CommandKind::Use, 0, {}}, error));
                }
            if (entries >= 3 && changes >= 3 && !leaving)
            {
                CHECK(client.disconnect(Clock::now()));
                leaving = true;
            }
            if (client.phase() == NetworkClient::Phase::Closed)
            {
                CHECK(leaving && entries >= 3 && changes >= 3);
                std::cout << "Three world entries and transitions, reset frags/replica/collision, local bells, "
                             "retained connection and clean exit passed\n";
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        error = "Rotation client deadline";
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
    bool parent(const char* self, const char* server, const char* data, bool hosted)
    {
        std::string error;
        Process host;
        const char* dedicated_args[]{
            server,  "--data-dir", data,           "--world", "multi/bb_crypt", "--map",     "multi/bb_fields",
            "--end", "time",       "--time-limit", "0",       "--bind",         "127.0.0.1", "--port",
            "0",     "--ticks",    "2100",         nullptr};
        const char* hosted_args[]{server,
                                  "--data-dir",
                                  data,
                                  "--host",
                                  "multi/bb_crypt",
                                  "--map",
                                  "multi/bb_fields",
                                  "--end",
                                  "time",
                                  "--time-limit",
                                  "0",
                                  "--port",
                                  "0",
                                  "--frames",
                                  "750",
                                  "--name",
                                  "Rotation host",
                                  "--capture",
                                  "rotation-host.png",
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
            const char* child_args[]{self, "--peer", port.c_str(), data, i ? "Second" : "First", nullptr};
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
        const auto rotations = host.output.rfind("rotations=");
        CHECK(rotations != std::string::npos && host.output[rotations + 10] >= '2' &&
              host.output[rotations + 10] <= '9');
        if (hosted)
        {
            std::error_code ec;
            CHECK(std::filesystem::file_size("rotation-host.png", ec) > 4096 && !ec);
        }
        CHECK(clients[0].output.find("Three world entries") != std::string::npos &&
              clients[1].output.find("Three world entries") != std::string::npos);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 5 && std::string_view(argv[1]) == "--peer")
        return client(argv[2], argv[3], argv[4]) ? 0 : 1;
    if (argc == 3 || (argc == 4 && std::string_view(argv[3]) == "--host"))
        return parent(argv[0], argv[1], argv[2], argc == 4) ? 0 : 1;
    return 2;
}
