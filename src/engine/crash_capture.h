#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace OpenBlood2
{
    struct CrashSettings
    {
        std::filesystem::path directory;
        std::string debugger = "gdb";
        unsigned timeout_ms = 15000;
        std::uint64_t dump_bytes = 2ull * 1024 * 1024 * 1024;
    };

    // Start/stop on the main thread, before/after application worker lifetimes.
    // Native backend is Linux; other platforms report unavailable without installing handlers.
    class CrashCapture
    {
      public:
        CrashCapture();
        ~CrashCapture();
        CrashCapture(const CrashCapture&) = delete;
        CrashCapture& operator=(const CrashCapture&) = delete;
        bool start(const CrashSettings& settings, std::string& error);
        void stop();

      private:
        struct State;
        std::unique_ptr<State> state_;
    };

    // Private executable entry, before SDL/log initialization. Returns -1 for ordinary invocations.
    int dispatch_crash_helper(int argc, char** argv);
} // namespace OpenBlood2
