#pragma once
#include "engine/config.h"
#include <SDL3/SDL_process.h>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// The F8 bake (gi_contracts.md, lot G10, decision 10): the game starts `blood2 --bake-gi …` as a child
// process, reads its GI_PROGRESS and GI_RESULT lines without blocking, and cancels it with SIGTERM (the
// child's handler stops the bake: exit 12, nothing written; a forced kill on Windows). The game process
// gains no worker threads, and the file equals the CLI's by construction.
namespace OpenBlood2
{
    // The running executable (the child is the same binary).
    bool gi_executable_path(std::filesystem::path&, std::string& error);
    // The child's arguments after the executable: the game's content (data directory, edition, data
    // mods), the placement directory, the output directory (empty: the child's default,
    // user_directory()/gi), the quality, the world and, for the original direct source, "--gi-direct
    // original" (lot GD2; re-baked is the child's default since Gate G).
    std::vector<std::string> gi_bake_arguments(const Config&, std::string_view world, std::string_view quality,
                                               const std::filesystem::path& placement_dir,
                                               const std::filesystem::path& out_dir, unsigned jobs = 0,
                                               bool rebaked = true);

    // `GI_PROGRESS stage=<stage> done=N total=M` (the stage may hold a space: "bounce 1/2").
    struct GiBakeProgress
    {
        std::string stage;
        std::uint64_t done = 0, total = 0;
    };
    bool parse_gi_progress(std::string_view line, GiBakeProgress&);
    // `GI_RESULT key=value …`: values run to the next " key=" (paths may hold spaces); a value that
    // starts with a quote is a JSON string (the child's error=), read to its closing quote and decoded.
    bool parse_gi_result(std::string_view line, std::map<std::string, std::string, std::less<>>&);
    // Overall progress 0-1 from the last progress line: survey 5 %, the bounces 90 % in equal parts,
    // the probes 5 %. Before the first line (mounting, scene, sky, receivers) it is 0.
    double gi_bake_fraction(const GiBakeProgress&);

    class GiBakeProcess
    {
      public:
        enum class State : std::uint8_t
        {
            Idle,
            Running,
            Finished,  // Exited on its own; exit_code says how (0 baked or current, 12 cancelled).
            Cancelled, // Cancelled by cancel() or stop(), whatever it printed.
            Failed     // Could not start, or killed by a signal.
        };
        GiBakeProcess() = default;
        GiBakeProcess(const GiBakeProcess&) = delete;
        GiBakeProcess& operator=(const GiBakeProcess&) = delete;
        ~GiBakeProcess();

        bool start(const std::filesystem::path& executable, const std::vector<std::string>& arguments,
                   std::string& error);
        // Non-blocking: reads what the child printed, then checks whether it exited.
        void poll();
        // Asks the child to stop (SIGTERM; a forced kill on Windows); poll() reports Cancelled once it exits.
        void cancel();
        // Cancels and waits (bounded: a forced kill after `timeout_ms`), then releases the process.
        void stop(std::uint32_t timeout_ms = 5000);
        bool running() const
        {
            return state == State::Running;
        }

        State state = State::Idle;
        int exit_code = 0;
        GiBakeProgress progress;
        std::map<std::string, std::string, std::less<>> result; // The GI_RESULT fields.
        std::deque<std::string> lines;                          // The last output lines (bounded).
        static constexpr std::size_t max_lines = 64, max_line = 4096;
        std::string error; // Why it failed to start, or the child's error= field.
        double elapsed_s() const;
        // Seconds left from the overall fraction; negative while unknown.
        double eta_s() const;

      private:
        void read_output();
        void line(std::string_view);
        void finish(int code);
        SDL_Process* process_ = nullptr;
        std::string partial_;
        std::uint64_t started_ns_ = 0, ended_ns_ = 0;
        bool cancelling_ = false;
    };
    const char* gi_bake_state_name(GiBakeProcess::State);
} // namespace OpenBlood2
