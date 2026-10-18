#include "engine/logging.h"
#include "build_info.h"
#include "engine/config.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace OpenBlood2
{
    namespace fs = std::filesystem;
    namespace
    {
        std::FILE* open_file(const fs::path& path)
        {
#ifdef _WIN32
            return _wfopen(path.c_str(), L"wb");
#else
            return std::fopen(path.c_str(), "wb");
#endif
        }

        // Explicit I/O failures: spdlog's stock rotating sink throws on open/rename errors.
        class SessionSink final : public spdlog::sinks::base_sink<std::mutex>
        {
          public:
            SessionSink(fs::path directory, std::size_t limit) : directory_(std::move(directory)), limit_(limit)
            {
                file_ = open_file(directory_ / "blood2.log");
            }
            ~SessionSink() override
            {
                if (file_)
                    std::fclose(file_);
            }
            bool healthy()
            {
                std::lock_guard lock(mutex_);
                return file_ != nullptr;
            }
            bool close()
            {
                std::lock_guard lock(mutex_);
                if (!file_)
                    return false;
                const bool success = close_unlocked();
                if (!success)
                    fail();
                return success;
            }

          private:
            void fail()
            {
                // Never recurse through the logger or SDL from a sink failure.
                std::fputs("[error] File logging failed; continuing with console only. Session is incomplete.\n",
                           stderr);
                std::fflush(stderr);
                if (file_)
                    std::fclose(file_);
                file_ = nullptr;
            }
            bool rotate()
            {
                if (!close_unlocked())
                    return false;
                std::error_code ec;
                fs::remove(directory_ / "blood2.2.log", ec);
                if (ec)
                    return false;
                const auto previous = directory_ / "blood2.1.log";
                const bool exists = fs::exists(previous, ec);
                if (ec)
                    return false;
                if (exists)
                {
                    fs::rename(previous, directory_ / "blood2.2.log", ec);
                    if (ec)
                        return false;
                }
                fs::rename(directory_ / "blood2.log", previous, ec);
                if (ec)
                    return false;
                file_ = open_file(directory_ / "blood2.log");
                bytes_ = 0;
                return file_ != nullptr;
            }
            bool close_unlocked()
            {
                const bool success = std::fclose(file_) == 0;
                file_ = nullptr;
                return success;
            }
            void sink_it_(const spdlog::details::log_msg& message) override
            {
                if (!file_)
                    return;
                spdlog::memory_buf_t formatted;
                formatter_->format(message, formatted);
                constexpr std::string_view truncated = " ... [log record truncated]\n";
                const auto maximum = std::min(limit_, std::size_t{64 * 1024});
                const bool truncate = formatted.size() > maximum;
                const auto count = truncate ? maximum - truncated.size() : formatted.size();
                const auto total = count + (truncate ? truncated.size() : 0);
                if (bytes_ + total > limit_ && !rotate())
                {
                    fail();
                    return;
                }
                if (std::fwrite(formatted.data(), 1, count, file_) != count ||
                    (truncate && std::fwrite(truncated.data(), 1, truncated.size(), file_) != truncated.size()) ||
                    std::fflush(file_) != 0)
                {
                    fail();
                    return;
                }
                bytes_ += total;
            }
            void flush_() override
            {
                if (file_ && std::fflush(file_) != 0)
                    fail();
            }
            fs::path directory_;
            std::size_t limit_, bytes_ = 0;
            std::FILE* file_ = nullptr;
        };

        void prune_completed(const fs::path& root, std::size_t keep)
        {
            std::error_code ec;
            std::vector<fs::path> completed;
            fs::directory_iterator it(root, ec), end;
            for (; !ec && it != end; it.increment(ec))
            {
                const auto& entry = *it;
                if (!path_text(entry.path().filename()).starts_with("session-"))
                    continue;
                if (entry.symlink_status(ec).type() != fs::file_type::directory || ec)
                    continue;
                if (fs::symlink_status(entry.path() / "finished.txt", ec).type() == fs::file_type::regular)
                    completed.push_back(entry.path());
                if (ec == std::errc::no_such_file_or_directory)
                    ec.clear();
            }
            if (ec)
            {
                spdlog::warn("Cannot inspect old log sessions: {}", ec.message());
                return;
            }
            std::sort(completed.begin(), completed.end());
            for (std::size_t i = 0; i + keep < completed.size(); ++i)
            {
                // Never recurse: unrelated files and unfinished/active sessions are preserved.
                for (const auto* name : {"blood2.log", "blood2.1.log", "blood2.2.log", "session.txt", "finished.txt"})
                {
                    fs::remove(completed[i] / name, ec);
                    if (ec)
                        break;
                }
                if (!ec)
                    fs::remove(completed[i], ec);
                if (ec)
                    spdlog::warn("Cannot prune log session {}: {}", path_text(completed[i]), ec.message());
            }
        }

        void SDLCALL sdl_log(void* userdata, int category, SDL_LogPriority priority, const char* message)
        {
            auto level = spdlog::level::info;
            switch (priority)
            {
                case SDL_LOG_PRIORITY_TRACE:
                    level = spdlog::level::trace;
                    break;
                case SDL_LOG_PRIORITY_VERBOSE:
                case SDL_LOG_PRIORITY_DEBUG:
                    level = spdlog::level::debug;
                    break;
                case SDL_LOG_PRIORITY_WARN:
                    level = spdlog::level::warn;
                    break;
                case SDL_LOG_PRIORITY_ERROR:
                    level = spdlog::level::err;
                    break;
                case SDL_LOG_PRIORITY_CRITICAL:
                    level = spdlog::level::critical;
                    break;
                default:
                    break;
            }
            static_cast<spdlog::logger*>(userdata)->log(level, "SDL[{}]: {}", category, message ? message : "");
        }
    } // namespace

    struct LogSession::State
    {
        fs::path directory;
        std::shared_ptr<spdlog::logger> previous, logger;
        std::shared_ptr<SessionSink> file;
        SDL_LogOutputFunction previous_sdl = nullptr;
        void* previous_sdl_data = nullptr;
        bool finished = false;
    };

    LogSession::LogSession() : state_(std::make_unique<State>()) {}

    LogSession::~LogSession()
    {
        if (state_->logger)
        {
            SDL_SetLogOutputFunction(state_->previous_sdl, state_->previous_sdl_data);
            state_->logger->flush();
            spdlog::set_default_logger(state_->previous);
        }
    }

    const fs::path& LogSession::directory() const
    {
        return state_->directory;
    }

    bool LogSession::start(const LogSettings& settings, std::string& error)
    {
        error.clear();
        if (state_->logger || settings.file_bytes < 256 ||
            (settings.level != "trace" && settings.level != "debug" && settings.level != "info" &&
             settings.level != "warn" && settings.level != "error"))
        {
            error = "Invalid or repeated logging initialization";
            return false;
        }
        state_->previous = spdlog::default_logger();
        auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console->set_pattern("[%^%l%$] %v");
        state_->logger = std::make_shared<spdlog::logger>("blood2", console);
        state_->logger->set_level(spdlog::level::from_str(std::string(settings.level)));
        spdlog::set_default_logger(state_->logger);
        SDL_GetLogOutputFunction(&state_->previous_sdl, &state_->previous_sdl_data);
        SDL_SetLogOutputFunction(sdl_log, state_->logger.get());

        auto root = settings.directory;
        if (root.empty())
        {
            char* pref = SDL_GetPrefPath("OpenBlood2", "Blood2");
            if (!pref)
            {
                error = "Cannot locate log directory: " + std::string(SDL_GetError());
                return false;
            }
            root = path_from_utf8(pref) / "logs";
            SDL_free(pref);
        }
        std::error_code ec;
        root = fs::absolute(root, ec);
        if (!ec)
            fs::create_directories(root, ec);
        if (ec)
        {
            error = "Cannot create log directory " + path_text(root) + ": " + ec.message();
            return false;
        }
        const auto stamp =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        bool created = false;
        for (unsigned attempt = 0; attempt < 100 && !created; ++attempt)
        {
            state_->directory = root / ("session-" + std::to_string(stamp) + "-" + std::to_string(attempt));
            created = fs::create_directory(state_->directory, ec);
            if (ec)
                break;
        }
        if (!created)
        {
            error = "Cannot create unique log session: " + ec.message();
            return false;
        }
        std::ofstream manifest(state_->directory / "session.txt", std::ios::binary);
        manifest << "OpenBlood2 " << build::version << "\nBuild (configure-time): " << build::revision
                 << "\nConfiguration: " << build::configuration << "\nCompiler: " << build::compiler
                 << "\nPlatform: " << SDL_GetPlatform() << "\nSDL pinned: " << build::sdl_revision
                 << "\nSDL runtime: " << SDL_GetRevision()
                 << "\nLog timestamps: UTC\nNo finished.txt means running or interrupted, not proof of a crash.\n";
        manifest.close();
        if (!manifest)
        {
            error = "Cannot write log session metadata: " + path_text(state_->directory);
            return false;
        }
        state_->file = std::make_shared<SessionSink>(state_->directory, settings.file_bytes);
        if (!state_->file->healthy())
        {
            error = "Cannot open log file: " + path_text(state_->directory / "blood2.log");
            return false;
        }
        state_->file->set_formatter(std::make_unique<spdlog::pattern_formatter>(
            "[%Y-%m-%dT%H:%M:%S.%eZ] [%l] [pid %P] [thread %t] %v", spdlog::pattern_time_type::utc));
        state_->logger->sinks().push_back(state_->file);
        spdlog::info("Log session: {}", path_text(state_->directory));
        spdlog::info("OpenBlood2 {} / {} / {} / {}; platform={}; SDL={} (pinned {})", build::version, build::revision,
                     build::configuration, build::compiler, SDL_GetPlatform(), SDL_GetRevision(), build::sdl_revision);
        prune_completed(root, settings.completed_sessions);
        return true;
    }

    void LogSession::finish(int exit_code)
    {
        if (!state_->logger || state_->finished)
            return;
        state_->finished = true;
        spdlog::info("Session finished: exit_code={}", exit_code);
        // Close before publishing completion: concurrent launches may prune only closed sessions.
        if (state_->file && state_->file->close())
        {
            const auto pending = state_->directory / "finished.tmp";
            std::ofstream marker(pending, std::ios::binary);
            marker << "exit_code=" << exit_code << '\n';
            marker.close();
            std::error_code ec;
            if (marker)
                fs::rename(pending, state_->directory / "finished.txt", ec);
            if (!marker || ec)
                spdlog::warn("Cannot write log session completion: {}", path_text(state_->directory));
        }
    }
} // namespace OpenBlood2
