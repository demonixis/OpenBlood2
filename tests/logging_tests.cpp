#include "engine/config.h"
#include "engine/logging.h"

#include <SDL3/SDL.h>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>
#include <thread>
#include <vector>
#ifdef __unix__
#include <sys/resource.h>
#endif

namespace fs = std::filesystem;
using namespace OpenBlood2;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n";                                             \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)

namespace
{
    std::string read(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    bool exercise(std::string_view mode, const fs::path& root)
    {
        std::string error;
        if (mode == "failure")
        {
            fs::create_directories(root);
            std::ofstream(root / "blocker") << "not a directory";
            auto previous = spdlog::default_logger();
            SDL_LogOutputFunction callback;
            void* userdata;
            SDL_GetLogOutputFunction(&callback, &userdata);
            {
                LogSession logs;
                CHECK(!logs.start({root / "blocker"}, error));
                CHECK(!error.empty());
                spdlog::error("Console fallback still works");
                logs.finish(2);
            }
            CHECK(spdlog::default_logger() == previous);
            SDL_LogOutputFunction restored;
            void* restored_data;
            SDL_GetLogOutputFunction(&restored, &restored_data);
            CHECK(callback == restored && userdata == restored_data);
            return true;
        }
        if (mode == "retention")
        {
            const auto unfinished = root / "session-000-interrupted";
            fs::create_directories(unfinished);
            std::ofstream(unfinished / "blood2.log") << "keep interrupted evidence";
            std::ofstream(root / "unrelated.txt") << "keep user file";
            fs::path first;
            for (unsigned i = 0; i < 4; ++i)
            {
                LogSession logs;
                CHECK(logs.start({root, "info", 4096, 2}, error));
                if (!i)
                    first = logs.directory();
                logs.finish(0);
            }
            CHECK(!fs::exists(first));
            CHECK(read(unfinished / "blood2.log") == "keep interrupted evidence");
            CHECK(read(root / "unrelated.txt") == "keep user file");
            unsigned completed = 0;
            for (const auto& entry : fs::directory_iterator(root))
                if (fs::is_directory(entry) && fs::exists(entry.path() / "finished.txt"))
                    ++completed;
            CHECK(completed == 3); // Two previous completed sessions plus this run.
            return true;
        }
        LogSession logs;
        CHECK(logs.start({root, mode == "level" ? "warn" : "debug", mode == "rotation" ? 1024u : 1024u * 1024}, error));
        const auto directory = logs.directory();
        const auto file = directory / "blood2.log";
        CHECK(!read(directory / "session.txt").empty());
        CHECK(!fs::exists(directory / "finished.txt"));
        if (mode == "abrupt" || mode == "crash")
        {
            spdlog::error("LAST RECORD BEFORE ABRUPT TERMINATION");
            CHECK(read(file).find("LAST RECORD BEFORE ABRUPT TERMINATION") != std::string::npos);
            if (mode == "crash")
            {
#ifdef __unix__
                const rlimit no_core{0, 0};
                setrlimit(RLIMIT_CORE, &no_core);
#endif
                std::raise(SIGABRT); // No logger, formatter or lock is called from a signal handler.
            }
            std::_Exit(23);
        }
        if (mode == "rotation")
        {
            spdlog::info("{}", std::string(10000, 'X'));
            CHECK(read(file).find("[log record truncated]") != std::string::npos);
            for (unsigned i = 0; i < 100; ++i)
                spdlog::info("record {} {}", i, std::string(100, 'r'));
            CHECK(read(file).find("record 99") != std::string::npos);
            CHECK(fs::exists(directory / "blood2.2.log"));
            for (const auto* name : {"blood2.log", "blood2.1.log", "blood2.2.log"})
                CHECK(fs::file_size(directory / name) <= 1024);
            CHECK(!fs::exists(directory / "blood2.3.log"));
        }
        else if (mode == "write_failure")
        {
            // Block rotation without depending on root/unprivileged chmod behavior.
            fs::create_directories(directory / "blood2.2.log");
            std::ofstream(directory / "blood2.2.log" / "blocker") << "retain";
            for (unsigned i = 0; i < 40; ++i)
                spdlog::debug("{}", std::string(64000, 'x'));
            logs.finish(0);
            CHECK(!fs::exists(directory / "finished.txt"));
            CHECK(fs::exists(directory / "blood2.2.log" / "blocker"));
            return true;
        }
        else if (mode == "threads")
        {
            std::vector<std::thread> threads;
            for (unsigned t = 0; t < 4; ++t)
                threads.emplace_back(
                    [t]
                    {
                        for (unsigned i = 0; i < 100; ++i)
                            spdlog::info("worker {} item {} END", t, i);
                    });
            for (auto& thread : threads)
                thread.join();
            const auto text = read(file);
            for (unsigned t = 0; t < 4; ++t)
                for (unsigned i = 0; i < 100; ++i)
                {
                    const auto line = "worker " + std::to_string(t) + " item " + std::to_string(i) + " END";
                    const auto pos = text.find(line);
                    CHECK(pos != std::string::npos && text.find(line, pos + line.size()) == std::string::npos);
                }
        }
        else
        {
            spdlog::info("INFO visibility sentinel");
            spdlog::warn("{}", "literal braces {} and UTF-8 é");
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL bridge sentinel {literal}");
            const auto text = read(file);
            CHECK((text.find("INFO visibility sentinel") == std::string::npos) == (mode == "level"));
            CHECK(text.find("literal braces {} and UTF-8 é") != std::string::npos);
            CHECK(text.find("[error]") != std::string::npos);
            CHECK(text.find("SDL[0]: SDL bridge sentinel {literal}") != std::string::npos);
            CHECK(text.find("Z] [") != std::string::npos && text.find("[thread ") != std::string::npos);
        }
        logs.finish(7);
        CHECK(read(directory / "finished.txt") == "exit_code=7\n");
        logs.finish(0);
        CHECK(read(directory / "finished.txt") == "exit_code=7\n");
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const bool passed = exercise(argv[1], path_from_utf8(argv[2]));
    SDL_Quit();
    return passed ? 0 : 1;
}
