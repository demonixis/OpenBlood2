#include "engine/config.h"
#include "engine/crash_capture.h"
#include "engine/logging.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string_view>
#include <thread>
#if defined(__linux__)
#include <csignal>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

volatile int crash_sentinel = 0x13572468;

#if defined(__linux__)
namespace
{
    [[gnu::noinline]] void crash_leaf(volatile int* bad)
    {
        *bad = crash_sentinel; // The expected fault and source line in both the report and reopened core.
    }
    [[gnu::noinline]] void crash_middle(volatile int* bad)
    {
        crash_leaf(bad);
        crash_sentinel = 1;
    }
    [[gnu::noinline]] void crash_outer(volatile int* bad)
    {
        crash_middle(bad);
        crash_sentinel = 2;
    }
    [[gnu::noinline]] void crash_overflow(unsigned depth)
    {
        if (depth == 1000000)
            return;
        volatile char stack[8192];
        stack[depth % sizeof(stack)] = char(depth);
        crash_overflow(depth + 1);
        crash_sentinel = stack[depth % sizeof(stack)];
    }
    void existing_handler(int) {}

    int read_helper(const std::filesystem::path& directory)
    {
        std::ifstream file(directory / "crash-capture.txt");
        std::string line;
        while (std::getline(file, line))
            if (line.starts_with("helper_pid="))
                return std::atoi(line.c_str() + 11);
        return -1;
    }
} // namespace
#endif

int main(int argc, char** argv)
{
    if (const int helper = OpenBlood2::dispatch_crash_helper(argc, argv); helper >= 0)
        return helper;
#if defined(__linux__)
    if (argc > 1 && std::string_view(argv[1]) == "-nx")
    {
        sleep(60); // Deliberately hung debugger; production code must kill and reap it.
        return 1;
    }
#endif
    if (argc != 4)
        return 2;
    const std::string_view mode(argv[1]);
    OpenBlood2::LogSession logs;
    std::string error;
    if (!logs.start({OpenBlood2::path_from_utf8(argv[2])}, error))
    {
        std::cerr << error << '\n';
        return 2;
    }
    OpenBlood2::CrashSettings settings{logs.directory(), argv[3], 15000, 64 * 1024 * 1024};
    if (mode == "missing")
        settings.debugger = "/nonexistent/blood2-test-debugger";
    if (mode == "size_limit")
        settings.dump_bytes = 4096;
    if (mode == "timeout" || mode == "collector_loss")
    {
        settings.debugger = argv[0];
        settings.timeout_ms = mode == "timeout" ? 250 : 2000;
    }
#if defined(__linux__)
    // Avoid an additional systemd/kernel dump of this intentionally crashing fixture.
    const rlimit no_native_core{0, 0};
    setrlimit(RLIMIT_CORE, &no_native_core);
    struct sigaction before{}, after{};
    stack_t stack_before{}, stack_after{};
    sigaction(SIGSEGV, nullptr, &before);
    sigaltstack(nullptr, &stack_before);
    if (mode == "existing")
    {
        struct sigaction action{};
        action.sa_handler = existing_handler;
        sigemptyset(&action.sa_mask);
        sigaction(SIGSEGV, &action, nullptr);
    }
    if (mode == "child_reaper")
        signal(SIGCHLD, SIG_IGN);
#endif
    OpenBlood2::CrashCapture capture;
    const bool enabled = capture.start(settings, error);
    if (mode == "unavailable" || mode == "missing" || mode == "existing" || mode == "debugger" ||
        mode == "child_reaper")
    {
        if (enabled || error.empty())
            return 3;
        spdlog::info("Expected unavailable: {}", error);
#if defined(__linux__)
        if (mode == "existing")
        {
            sigaction(SIGSEGV, nullptr, &after);
            if (after.sa_handler != existing_handler)
                return 4;
            sigaction(SIGSEGV, &before, nullptr);
        }
        if (mode == "child_reaper")
        {
            sigaction(SIGCHLD, nullptr, &after);
            if (after.sa_handler != SIG_IGN)
                return 4;
            signal(SIGCHLD, SIG_DFL);
        }
#endif
        logs.finish(0);
        return 0;
    }
    if (!enabled)
    {
        std::cerr << error << '\n';
        return 5;
    }
#if defined(__linux__)
    if (mode == "lifecycle")
    {
        OpenBlood2::CrashCapture second;
        if (second.start(settings, error))
            return 6;
        capture.stop();
        capture.stop();
        sigaction(SIGSEGV, nullptr, &after);
        sigaltstack(nullptr, &stack_after);
        int status;
        if (after.sa_handler != before.sa_handler || stack_before.ss_flags != stack_after.ss_flags ||
            stack_before.ss_sp != stack_after.ss_sp || waitpid(-1, &status, WNOHANG) != -1 || errno != ECHILD)
            return 7;
        logs.finish(0);
        return 0;
    }
    if (mode == "dead_helper")
    {
        const auto helper = read_helper(logs.directory());
        if (helper <= 0 || kill(helper, SIGKILL) != 0)
            return 8;
        int status;
        waitpid(helper, &status, 0);
    }
    if (mode == "dump_failure")
        std::filesystem::create_directory(logs.directory() / "crash.core.partial");
    if (mode == "collector_loss")
    {
        const auto helper = read_helper(logs.directory());
        if (helper <= 0)
            return 8;
        const auto watcher = fork();
        if (watcher < 0)
            return 8;
        if (watcher == 0)
        {
            for (unsigned i = 0; i < 200; ++i)
            {
                if (std::filesystem::exists(logs.directory() / "crash-stacktrace.txt"))
                {
                    std::ifstream report(logs.directory() / "crash-report.txt");
                    std::string line;
                    while (std::getline(report, line))
                        if (line.starts_with("debugger_pid="))
                        {
                            kill(helper, SIGKILL);
                            _exit(0);
                        }
                }
                usleep(10000);
            }
            _exit(1);
        }
    }
    spdlog::error("LAST LOG BEFORE CRASH: {}", mode);
    if (mode == "abort")
        std::abort();
    if (mode == "overflow")
        crash_overflow(0);
    auto* bad = static_cast<volatile int*>(mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (bad == MAP_FAILED)
        return 9;
    if (mode == "thread")
    {
        std::thread worker([bad] { crash_outer(bad); });
        worker.join();
    }
    else
        crash_outer(bad);
#endif
    return 10;
}
