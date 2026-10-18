#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace OpenBlood2
{
    struct LogSettings
    {
        std::filesystem::path directory; // Empty selects the SDL user directory / logs.
        std::string_view level = "info";
        std::size_t file_bytes = 4 * 1024 * 1024;
        std::size_t completed_sessions = 10;
    };

    // Application-scoped: start before SDL initialization, finish after application teardown.
    // Missing completion means running or interrupted, not proof of a crash.
    class LogSession
    {
      public:
        LogSession();
        ~LogSession();
        LogSession(const LogSession&) = delete;
        LogSession& operator=(const LogSession&) = delete;
        bool start(const LogSettings& settings, std::string& error);
        void finish(int exit_code);
        const std::filesystem::path& directory() const;

      private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace OpenBlood2
