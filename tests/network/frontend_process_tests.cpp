#include <SDL3/SDL.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
namespace
{
    using Clock = std::chrono::steady_clock;
    struct Process
    {
        SDL_Process* handle = nullptr;
        std::string output;
        bool done = false;
        int code = -1;
        ~Process()
        {
            if (handle)
            {
                if (!done)
                {
                    SDL_KillProcess(handle, true);
                    SDL_WaitProcess(handle, true, &code);
                }
                SDL_DestroyProcess(handle);
            }
        }
        bool start(std::vector<const char*> args)
        {
            args.push_back(nullptr);
            handle = SDL_CreateProcess(args.data(), true);
            return handle != nullptr;
        }
        bool read()
        {
            if (!handle)
                return false;
            auto* stream = SDL_GetProcessOutput(handle);
            if (!stream)
                return false;
            char bytes[4096];
            while (const auto count = SDL_ReadIO(stream, bytes, sizeof bytes))
            {
                if (output.size() + count > 1024 * 1024)
                    return false;
                output.append(bytes, count);
            }
            if (!done)
                done = SDL_WaitProcess(handle, false, &code);
            return SDL_GetIOStatus(stream) != SDL_IO_STATUS_ERROR;
        }
    };
    bool run(const char* app, const char* server, const char* data, const char* folder, bool hosted, bool shutdown)
    {
        Process host, first, second;
        struct Report
        {
            Process &a, &b, &c;
            ~Report()
            {
                std::cout << "HOST\n" << a.output << "CLIENT 1\n" << b.output << "CLIENT 2\n" << c.output;
            }
        } report{host, first, second};
        if (!host.start(hosted ? std::vector<const char*>{app, "--data-dir", data, "--host", "multi/bb_crypt", "--port",
                                                          "0", "--frames", shutdown ? "180" : "900", "--name", "Host"}
                               : std::vector<const char*>{server, "--data-dir", data, "--world", "multi/bb_crypt",
                                                          "--port", "0", "--ticks", "2400"}))
            return false;
        const auto deadline = Clock::now() + std::chrono::seconds(110);
        std::string port;
        while (Clock::now() < deadline && port.empty())
        {
            if (!host.read() || host.done)
                return false;
            const auto start = host.output.find(", UDP ");
            if (start != std::string::npos)
            {
                const auto end = host.output.find_first_not_of("0123456789", start + 6);
                if (end != std::string::npos)
                    port = host.output.substr(start + 6, end - start - 6);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (port.empty())
            return false;
        const auto address = "127.0.0.1:" + port;
        const auto capture = (std::filesystem::path(folder) / (shutdown ? "host-shutdown-peer.png"
                                                               : hosted ? "host-peer.png"
                                                                        : "dedicated-peer.png"))
                                 .string();
        std::vector<const char*> client_args{app,      "--data-dir", data,        "--connect",    address.c_str(),
                                             "--name", "Visitor",    "--capture", capture.c_str()};
        if (!shutdown)
            client_args.insert(client_args.end(), {"--frames", "180"});
        if (!first.start(client_args))
            return false;
        if (!hosted && !second.start({app, "--data-dir", data, "--connect", address.c_str(), "--name", "Observer",
                                      "--frames", "240"}))
            return false;
        while (Clock::now() < deadline && (!host.done || !first.done || (!hosted && !second.done)))
        {
            if (!host.read() || !first.read() || (!hosted && !second.read()))
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!host.read() || !first.read() || (!hosted && !second.read()))
            return false;
        if (!host.done || host.code || !first.done || first.code || (!hosted && (!second.done || second.code)))
            return false;
        std::error_code ec;
        const auto size = std::filesystem::file_size(capture, ec);
        if (ec || size < 4096 || host.output.find("admitted=2, departed=2") == std::string::npos ||
            first.output.find("Network world ready:") == std::string::npos ||
            first.output.find(shutdown ? "reason=Server stopped" : "Network session ended: frames=180,") ==
                std::string::npos)
            return false;
        if (!hosted && second.output.find("Network session ended: frames=240,") == std::string::npos)
            return false;
        if (hosted && host.output.find(shutdown ? "Network session ended: frames=180,"
                                                : "Network session ended: frames=900,") == std::string::npos)
            return false;
        std::cout << "Graphical " << (hosted ? "host and remote client" : "dedicated and two remote clients")
                  << " passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc != 6 || !run(argv[1], argv[2], argv[3], argv[4], std::string_view(argv[5]) != "dedicated",
                          std::string_view(argv[5]) == "shutdown"))
    {
        std::cerr << "Graphical process fixture failed: " << SDL_GetError() << '\n';
        return 1;
    }
}
