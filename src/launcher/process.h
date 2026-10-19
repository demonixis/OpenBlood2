#pragma once
#include "launcher/state.h"
#include "startup/protocol.h"
#include <SDL3/SDL.h>

namespace OpenBlood2::Launcher
{
    class Probe
    {
      public:
        enum class Status
        {
            Idle,
            Pending,
            Compatible,
            Failed
        };
        Probe() = default;
        Probe(const Probe&) = delete;
        Probe& operator=(const Probe&) = delete;
        ~Probe();
        bool start(const std::filesystem::path& executable, unsigned timeout_ms = 5000);
        void update();
        void cancel();
        Status status() const
        {
            return status_;
        }
        const std::string& error() const
        {
            return error_;
        }
        const LauncherInfo& info() const
        {
            return info_;
        }

      private:
        bool drain(SDL_IOStream* io, std::string& output);
        void failed(std::string message);
        SDL_Process* process_ = nullptr;
        SDL_IOStream* output_ = nullptr;
        SDL_IOStream* errors_ = nullptr;
        Status status_ = Status::Idle;
        Uint64 deadline_ = 0;
        std::string text_, stderr_, error_;
        LauncherInfo info_;
    };
    class GameProcess
    {
      public:
        GameProcess() = default;
        GameProcess(const GameProcess&) = delete;
        GameProcess& operator=(const GameProcess&) = delete;
        ~GameProcess();
        bool start(const std::filesystem::path& executable, const std::filesystem::path& config,
                   const Installation& item, const std::filesystem::path& logs, std::string& error,
                   const std::vector<std::filesystem::path>& overlays = {});
        bool update(int& exit_code);
        bool running() const
        {
            return process_ != nullptr;
        }

      private:
        SDL_Process* process_ = nullptr;
    };
} // namespace OpenBlood2::Launcher
