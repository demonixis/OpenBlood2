#include "engine/resources.h"
#include "engine/world.h"
#include "net/admission.h"
#include "net/content.h"
#include "net/transport.h"
#include <SDL3/SDL_error.h>
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
            std::cerr << __LINE__ << ": " #x " : " << error << "\n";                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool client(const char* port_text, const char* data)
    {
        std::string error;
        unsigned port = 0;
        const auto end = port_text + std::char_traits<char>::length(port_text);
        const auto parsed = std::from_chars(port_text, end, port);
        CHECK(parsed.ec == std::errc{} && parsed.ptr == end && port && port <= 65535);
        Config config;
        config.data_dir = path_from_utf8(data);
        Resources resources;
        Sha256Digest digest;
        CHECK(resources.mount_game(config, error) && content_fingerprint(resources, digest, error));
        Transport transport;
        CHECK(transport.open_client(error));
        const auto peer = transport.connect("127.0.0.1", std::uint16_t(port), error);
        CHECK(peer);
        const auto send = [&](const AdmissionMessage& message)
        {
            std::vector<std::uint8_t> bytes;
            return encode_admission(message, bytes) && transport.send(peer, Channel::Control, bytes, error);
        };
        const auto deadline = Clock::now() + std::chrono::seconds(8);
        Clock::time_point leave_at;
        std::uint64_t world = 0;
        bool hello = false, ready = false, joined = false, leaving = false;
        while (Clock::now() < deadline)
        {
            if (joined && !leaving && Clock::now() >= leave_at)
            {
                CHECK(transport.disconnect(peer, 0));
                leaving = true;
            }
            Event event;
            const auto status = transport.poll(event, 5, error);
            CHECK(status != PollResult::Failed);
            if (status == PollResult::Idle)
                continue;
            if (event.kind == EventKind::Disconnected)
                return joined && leaving && event.reason == 0;
            if (event.kind == EventKind::Connected)
            {
                CHECK(!hello && send(Hello{Edition::Retail, digest}));
                hello = true;
            }
            else
            {
                AdmissionMessage message;
                CHECK(decode_admission(event.bytes, message) == DecodeAdmission::Ok);
                if (const auto* offer = std::get_if<WorldOffer>(&message))
                {
                    CHECK(hello && !ready);
                    std::vector<std::uint8_t> bytes;
                    World loaded;
                    CHECK(resources.read(offer->path, bytes, error) && read_world(bytes, loaded, error));
                    world = offer->world;
                    CHECK(send(Ready{world, {}}));
                    ready = true;
                }
                else if (const auto* accepted = std::get_if<Joined>(&message))
                {
                    CHECK(ready && !joined && accepted->world == world);
                    joined = true;
                    leave_at = Clock::now() + std::chrono::milliseconds(1500);
                    std::cout << "Joined player " << accepted->player << " world " << world
                              << " after independent data hash and DAT decode\n"
                              << std::flush;
                }
                else if (const auto* pulse = std::get_if<Heartbeat>(&message))
                {
                    CHECK(joined && pulse->world == world);
                    if (!leaving)
                        CHECK(send(*pulse));
                }
                else
                    CHECK(false);
            }
        }
        error = "Child client protocol deadline";
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
    bool parent(const char* self, const char* server, const char* data)
    {
        std::string error;
        Process host;
        const char* args[]{server,   "--data-dir", data,     "--world", "multi/bb_crypt", "--mode", "bloodbath",
                           "--bind", "127.0.0.1",  "--port", "0",       "--ticks",        "600",    nullptr};
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
        const char* child_args[]{self, "--peer", port.c_str(), data, nullptr};
        for (auto& peer : clients)
        {
            peer.handle = SDL_CreateProcess(child_args, true);
            error = SDL_GetError();
            CHECK(peer.handle);
        }
        bool finished[2]{};
        int codes[2]{};
        bool host_done = false;
        int host_code = 0;
        deadline = Clock::now() + std::chrono::seconds(20);
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
        CHECK(clients[0].output.find("Joined player") != std::string::npos &&
              clients[1].output.find("Joined player") != std::string::npos);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc == 4 && std::string_view(argv[1]) == "--peer")
        return client(argv[2], argv[3]) ? 0 : 1;
    return argc == 3 && parent(argv[0], argv[1], argv[2]) ? 0 : 1;
}
