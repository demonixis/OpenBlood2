#include "engine/crash_capture.h"

#include <cstdlib>
#include <fstream>
#include <string_view>

#if defined(__linux__) && !BLOOD2_CRASH_SANITIZED
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <vector>

extern char** environ;
#endif

namespace OpenBlood2
{
    namespace fs = std::filesystem;
#if defined(__linux__) && !BLOOD2_CRASH_SANITIZED
    namespace
    {
        constexpr std::array fatal_signals{SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
        constexpr int helper_socket = 3;
        constexpr std::size_t stack_bytes = 64 * 1024;
        std::atomic_flag capturing = ATOMIC_FLAG_INIT;
        int capture_socket = -1, context_file = -1;
        unsigned capture_timeout_ms = 0;
        pid_t capture_process = 0;

        struct CrashEvent
        {
            int signal, code;
            pid_t process, thread;
            std::uintptr_t address, instruction;
        };

        std::int64_t milliseconds()
        {
            timespec now{};
            clock_gettime(CLOCK_MONOTONIC, &now);
            return std::int64_t(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
        }

        bool wait_readable(int fd, unsigned timeout_ms)
        {
            const auto deadline = milliseconds() + timeout_ms;
            for (;;)
            {
                const auto remaining = deadline - milliseconds();
                if (remaining <= 0)
                    return false;
                pollfd item{fd, POLLIN, 0};
                const auto result = poll(&item, 1, int(remaining));
                if (result > 0)
                    return (item.revents & POLLIN) != 0;
                if (result == 0 || errno != EINTR)
                    return false;
            }
        }

        // Fixed stack buffer only. No formatter, unwinder, heap, SDL or logger in a signal handler.
        char* append_number(char* out, std::uint64_t value, unsigned base)
        {
            char reversed[32];
            unsigned used = 0;
            do
            {
                reversed[used++] = "0123456789abcdef"[value % base];
                value /= base;
            } while (value);
            while (used)
                *out++ = reversed[--used];
            return out;
        }

        template <std::size_t N> char* append_text(char* out, const char (&text)[N])
        {
            for (std::size_t i = 0; i + 1 < N; ++i)
                *out++ = text[i];
            return out;
        }

        void terminate_signal(int signal)
        {
            struct sigaction action{};
            action.sa_handler = SIG_DFL;
            sigemptyset(&action.sa_mask);
            sigaction(signal, &action, nullptr);
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, signal);
            sigprocmask(SIG_UNBLOCK, &mask, nullptr);
            raise(signal);
            _exit(128 + signal);
        }

        void fatal_signal(int signal, siginfo_t* info, void* context)
        {
            if (capturing.test_and_set(std::memory_order_relaxed))
                terminate_signal(signal);
            CrashEvent event{};
            event.signal = signal;
            event.code = info ? info->si_code : 0;
            event.process = capture_process;
            event.thread = pid_t(syscall(SYS_gettid));
            event.address = info && info->si_code > 0 ? reinterpret_cast<std::uintptr_t>(info->si_addr) : 0;
#if defined(__x86_64__)
            event.instruction = static_cast<ucontext_t*>(context)->uc_mcontext.gregs[REG_RIP];
#elif defined(__aarch64__)
            event.instruction = static_cast<ucontext_t*>(context)->uc_mcontext.pc;
#else
            (void)context;
#endif
            char text[256];
            auto* end = append_text(text, "signal=");
            end = append_number(end, unsigned(signal), 10);
            end = append_text(end, "\npid=");
            end = append_number(end, unsigned(event.process), 10);
            end = append_text(end, "\nthread=");
            end = append_number(end, unsigned(event.thread), 10);
            end = append_text(end, "\nfault_address=0x");
            end = append_number(end, event.address, 16);
            end = append_text(end, "\ninstruction=0x");
            end = append_number(end, event.instruction, 16);
            end = append_text(end, "\n");
            const auto ignored = write(context_file, text, std::size_t(end - text));
            (void)ignored;
            if (send(capture_socket, &event, sizeof(event), MSG_NOSIGNAL | MSG_DONTWAIT) == sizeof(event) &&
                wait_readable(capture_socket, capture_timeout_ms + 5000))
            {
                char reply;
                const auto received = recv(capture_socket, &reply, 1, MSG_DONTWAIT);
                (void)received;
            }
            terminate_signal(signal);
        }

        bool traced()
        {
            std::ifstream status("/proc/self/status");
            std::string line;
            while (std::getline(status, line))
                if (line.starts_with("TracerPid:"))
                    return line.find_first_not_of(" \t0", 10) != std::string::npos;
            return true; // Do not replace an unknown debugger state.
        }

        std::string find_debugger(const std::string& requested)
        {
            std::error_code ec;
            if (requested.find('/') != std::string::npos)
            {
                const auto absolute = fs::absolute(requested, ec);
                return !ec && access(absolute.c_str(), X_OK) == 0 && fs::is_regular_file(absolute, ec)
                           ? absolute.string()
                           : std::string{};
            }
            const char* path = std::getenv("PATH");
            std::string_view paths = path ? path : "";
            while (!paths.empty())
            {
                const auto colon = paths.find(':');
                const auto part = paths.substr(0, colon);
                if (!part.empty())
                {
                    const auto candidate = fs::absolute(fs::path(part) / requested, ec);
                    if (!ec && access(candidate.c_str(), X_OK) == 0 && fs::is_regular_file(candidate, ec))
                        return candidate.string();
                }
                if (colon == std::string_view::npos)
                    break;
                paths.remove_prefix(colon + 1);
            }
            return {};
        }

        enum class ChildWait
        {
            Exited,
            Unavailable,
            TimedOut
        };

        ChildWait wait_child(pid_t child, unsigned timeout_ms, int& status)
        {
            const auto deadline = milliseconds() + timeout_ms;
            do
            {
                const auto result = waitpid(child, &status, WNOHANG);
                if (result == child)
                    return ChildWait::Exited;
                if (result < 0 && errno != EINTR)
                    return ChildWait::Unavailable;
                const timespec delay{0, 10000000};
                nanosleep(&delay, nullptr);
            } while (milliseconds() < deadline);
            return ChildWait::TimedOut;
        }

        void reap(pid_t child)
        {
            int status;
            while (waitpid(child, &status, 0) < 0 && errno == EINTR)
            {
            }
        }

        void collect(const CrashEvent& event, const char* debugger, unsigned timeout_ms, std::uint64_t limit)
        {
            std::ofstream report("crash-report.txt");
            report << "signal=" << event.signal << "\nsi_code=" << event.code << "\npid=" << event.process
                   << "\nfaulting_thread=" << event.thread << "\ndebugger=" << debugger
                   << "\ndump_limit_bytes=" << limit << "\ntimeout_ms=" << timeout_ms << '\n';
            report.flush();
            std::error_code ec;
            const auto executable = fs::path("/proc") / std::to_string(event.process) / "exe";
            const auto executable_bytes = fs::file_size(executable, ec);
            if (!ec && executable_bytes > 512ull * 1024 * 1024)
                ec = std::make_error_code(std::errc::file_too_large);
            if (!ec)
                fs::copy_file(executable, "crash-executable", fs::copy_options::none, ec);
            report << "executable_copy=" << (ec ? ec.message() : "saved") << '\n';
            fs::permissions("crash-executable", fs::perms::owner_read | fs::perms::owner_write, ec);
            const std::string attach = "attach " + std::to_string(event.process);
            const char* arguments[] = {debugger, "-nx",
                                       "-nh",    "-batch",
                                       "-iex",   "set auto-load off",
                                       "-iex",   "set debuginfod enabled off",
                                       "-ex",    "set pagination off",
                                       "-ex",    "set confirm off",
                                       "-ex",    "set print elements 32",
                                       "-ex",    "set print frame-arguments scalars",
                                       "-ex",    "set backtrace limit 128",
                                       "-ex",    attach.c_str(),
                                       "-ex",    "thread apply all bt full 128",
                                       "-ex",    "thread apply all info registers",
                                       "-ex",    "info sharedlibrary",
                                       "-ex",    "generate-core-file crash.core.partial",
                                       "-ex",    "detach",
                                       nullptr};
            const auto collector = getpid();
            const auto child = fork();
            if (child == 0)
            {
                // A dead collector must never leave a hung debugger attached to the game.
                if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 || getppid() != collector)
                    _exit(125);
                setpgid(0, 0);
                const rlimit file_limit{limit, limit};
                if (setrlimit(RLIMIT_FSIZE, &file_limit) != 0)
                    _exit(125);
                const int output = open("crash-stacktrace.txt", O_WRONLY | O_CREAT | O_EXCL, 0600);
                if (output < 0 || dup2(output, STDOUT_FILENO) < 0 || dup2(output, STDERR_FILENO) < 0)
                    _exit(125);
                close(output);
                close(helper_socket);
                execv(debugger, const_cast<char* const*>(arguments));
                _exit(127);
            }
            bool finished = false;
            int status = 0;
            if (child > 0)
            {
                setpgid(child, child);
                report << "debugger_pid=" << child << '\n';
                report.flush();
                const auto waited = wait_child(child, timeout_ms, status);
                finished = waited == ChildWait::Exited;
                if (waited == ChildWait::TimedOut)
                {
                    kill(-child, SIGKILL);
                    kill(child, SIGKILL);
                    reap(child);
                }
            }
            const bool succeeded = finished && WIFEXITED(status) && WEXITSTATUS(status) == 0;
            // A debugger failure or truncated dump is never promoted to a successful artifact.
            std::ifstream core("crash.core.partial", std::ios::binary);
            char magic[4]{};
            core.read(magic, sizeof(magic));
            const bool elf = core && std::memcmp(magic, "\177ELF", 4) == 0;
            bool saved = false;
            if (succeeded && elf)
            {
                fs::rename("crash.core.partial", "crash.core", ec);
                saved = !ec;
            }
            report << "debugger_completed=" << finished << "\ndebugger_wait_status=" << status
                   << "\ncore_saved=" << saved << '\n';
            if (!saved)
                report << "capture_failed: inspect crash-stacktrace.txt and any crash.core.partial; native crash "
                          "handling remains enabled\n";
        }

        int helper_main(const char* directory, const char* debugger, unsigned timeout_ms, std::uint64_t limit)
        {
            ucred peer{};
            socklen_t size = sizeof(peer);
            if (getsockopt(helper_socket, SOL_SOCKET, SO_PEERCRED, &peer, &size) != 0 || peer.pid != getppid() ||
                chdir(directory) != 0)
                return 2;
#ifdef SYS_close_range
            // Only stdio and the control socket belong to this clean, re-executed helper.
            syscall(SYS_close_range, unsigned(helper_socket + 1), ~0u, 0);
#endif
            umask(0077);
            setenv("LC_ALL", "C", 1);
            struct sigaction child_action{};
            child_action.sa_handler = SIG_DFL;
            sigemptyset(&child_action.sa_mask);
            if (sigaction(SIGCHLD, &child_action, nullptr) != 0)
                return 2;
            const char ready = 'R';
            if (send(helper_socket, &ready, 1, MSG_NOSIGNAL) != 1)
                return 2;
            CrashEvent event{};
            ssize_t received;
            do
            {
                received = recv(helper_socket, &event, sizeof(event), 0);
            } while (received < 0 && errno == EINTR);
            if (received != sizeof(event) || event.signal == 0)
                return 0; // Parent finished or disappeared without a captured signal.
            if (event.process != peer.pid || event.thread <= 0)
                return 2;
            collect(event, debugger, timeout_ms, limit);
            const char done = 'D';
            send(helper_socket, &done, 1, MSG_NOSIGNAL);
            return 0;
        }
    } // namespace

    struct CrashCapture::State
    {
        int socket = -1, context = -1;
        pid_t helper = -1;
        void* stack = MAP_FAILED;
        stack_t previous_stack{};
        std::array<struct sigaction, fatal_signals.size()> previous_actions{};
        std::size_t installed = 0;
        bool stack_installed = false, ptracer_set = false;
    };
#else
    struct CrashCapture::State
    {
    };
#endif

    CrashCapture::CrashCapture() : state_(std::make_unique<State>()) {}
    CrashCapture::~CrashCapture()
    {
        stop();
    }

    bool CrashCapture::start(const CrashSettings& settings, std::string& error)
    {
        error.clear();
#if defined(__linux__) && !BLOOD2_CRASH_SANITIZED
        if (state_->helper > 0 || capture_socket >= 0 || settings.directory.empty() || settings.timeout_ms < 100 ||
            settings.timeout_ms > 60000 || settings.dump_bytes < 4096 ||
            settings.dump_bytes > 2ull * 1024 * 1024 * 1024)
        {
            error = "Invalid or repeated crash capture initialization";
            return false;
        }
        if (traced() || prctl(PR_GET_DUMPABLE) != 1)
        {
            error = "Crash capture inactive: attached debugger or non-dumpable process";
            return false;
        }
        struct sigaction child_action{};
        if (sigaction(SIGCHLD, nullptr, &child_action) != 0 || child_action.sa_handler != SIG_DFL ||
            (child_action.sa_flags & SA_NOCLDWAIT))
        {
            error = "Crash capture inactive: existing child-process reaper";
            return false;
        }
        for (std::size_t i = 0; i < fatal_signals.size(); ++i)
            if (sigaction(fatal_signals[i], nullptr, &state_->previous_actions[i]) != 0 ||
                state_->previous_actions[i].sa_handler != SIG_DFL)
            {
                error = "Crash capture inactive: existing fatal-signal handler (debugger/sanitizer)";
                return false;
            }
        const auto debugger = find_debugger(settings.debugger);
        if (debugger.empty())
        {
            error = "Crash capture unavailable: GDB executable not found: " + settings.debugger;
            return false;
        }
        std::error_code ec;
        const auto directory = fs::absolute(settings.directory, ec);
        if (ec)
        {
            error = "Cannot resolve crash directory: " + ec.message();
            return false;
        }
        const auto executable = fs::read_symlink("/proc/self/exe", ec).string();
        if (ec)
        {
            error = "Cannot resolve crash helper executable: " + ec.message();
            return false;
        }
        state_->context =
            open((directory / "crash-context.txt").c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        int sockets[2]{-1, -1};
        if (state_->context < 0 || socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0)
        {
            error = "Cannot prepare crash files/channel: " + std::string(std::strerror(errno));
            stop();
            return false;
        }
        state_->socket = sockets[0];
        const auto timeout = std::to_string(settings.timeout_ms), limit = std::to_string(settings.dump_bytes);
        const char* arguments[] = {executable.c_str(),
                                   "--internal-crash-helper",
                                   directory.c_str(),
                                   debugger.c_str(),
                                   timeout.c_str(),
                                   limit.c_str(),
                                   nullptr};
        posix_spawn_file_actions_t actions;
        int setup = posix_spawn_file_actions_init(&actions);
        if (setup)
        {
            error = "Cannot prepare crash collector: " + std::string(std::strerror(setup));
            close(sockets[1]);
            stop();
            return false;
        }
        setup = posix_spawn_file_actions_addclose(&actions, sockets[0]);
        if (!setup)
            setup = posix_spawn_file_actions_adddup2(&actions, sockets[1], helper_socket);
        if (!setup && sockets[1] != helper_socket)
            setup = posix_spawn_file_actions_addclose(&actions, sockets[1]);
        pid_t helper = -1;
        const int launched = setup ? setup
                                   : posix_spawn(&helper, executable.c_str(), &actions, nullptr,
                                                 const_cast<char* const*>(arguments), environ);
        if (!launched)
            state_->helper = helper;
        posix_spawn_file_actions_destroy(&actions);
        close(sockets[1]);
        char ready = 0;
        if (launched || !wait_readable(state_->socket, 3000) || recv(state_->socket, &ready, 1, 0) != 1 || ready != 'R')
        {
            error = launched ? "Cannot start crash collector: " + std::string(std::strerror(launched))
                             : "Crash collector did not acknowledge startup";
            stop();
            return false;
        }
        if (prctl(PR_SET_PTRACER, state_->helper, 0, 0, 0) != 0)
        {
            error = "Cannot authorize crash collector: " + std::string(std::strerror(errno));
            stop();
            return false;
        }
        state_->ptracer_set = true;
        state_->stack = mmap(nullptr, stack_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        stack_t stack{};
        stack.ss_sp = state_->stack;
        stack.ss_size = stack_bytes;
        if (state_->stack == MAP_FAILED || sigaltstack(&stack, &state_->previous_stack) != 0)
        {
            error = "Cannot prepare alternate crash stack";
            stop();
            return false;
        }
        state_->stack_installed = true;
        capture_socket = state_->socket;
        context_file = state_->context;
        capture_timeout_ms = settings.timeout_ms;
        capture_process = getpid();
        capturing.clear();
        struct sigaction action{};
        action.sa_sigaction = fatal_signal;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigfillset(&action.sa_mask);
        for (auto signal : fatal_signals)
        {
            if (sigaction(signal, &action, nullptr) != 0)
            {
                error = "Cannot install crash signal handler";
                stop();
                return false;
            }
            ++state_->installed;
        }
        std::ofstream status(directory / "crash-capture.txt");
        status << "backend=linux-gdb\ndebugger=" << debugger << "\nhelper_pid=" << state_->helper
               << "\ntimeout_ms=" << settings.timeout_ms << "\ndump_limit_bytes=" << settings.dump_bytes << '\n';
        status.close();
        if (!status)
        {
            error = "Cannot write crash capture status";
            stop();
            return false;
        }
        return true;
#else
        (void)settings;
        error = "Crash capture inactive: unsupported platform or sanitizer build; native diagnostics remain active";
        return false;
#endif
    }

    void CrashCapture::stop()
    {
#if defined(__linux__) && !BLOOD2_CRASH_SANITIZED
        for (std::size_t i = 0; i < state_->installed; ++i)
            sigaction(fatal_signals[i], &state_->previous_actions[i], nullptr);
        if (capture_socket == state_->socket)
        {
            capture_socket = context_file = -1;
        }
        state_->installed = 0;
        if (state_->stack_installed)
        {
            sigaltstack(&state_->previous_stack, nullptr);
            state_->stack_installed = false;
        }
        if (state_->stack != MAP_FAILED)
        {
            munmap(state_->stack, stack_bytes);
            state_->stack = MAP_FAILED;
        }
        if (state_->socket >= 0)
        {
            close(state_->socket);
            state_->socket = -1;
        }
        if (state_->helper > 0)
        {
            int status;
            if (wait_child(state_->helper, 1000, status) == ChildWait::TimedOut)
            {
                kill(state_->helper, SIGKILL);
                reap(state_->helper);
            }
            state_->helper = -1;
        }
        if (state_->ptracer_set)
        {
            prctl(PR_SET_PTRACER, 0, 0, 0, 0);
            state_->ptracer_set = false;
        }
        if (state_->context >= 0)
        {
            close(state_->context);
            state_->context = -1;
        }
#endif
    }

    int dispatch_crash_helper(int argc, char** argv)
    {
        if (argc < 2 || std::string_view(argv[1]) != "--internal-crash-helper")
            return -1;
#if defined(__linux__) && !BLOOD2_CRASH_SANITIZED
        if (argc != 6)
            return 2;
        unsigned timeout = 0;
        std::uint64_t limit = 0;
        const std::string_view t(argv[4]), l(argv[5]);
        const auto a = std::from_chars(t.data(), t.data() + t.size(), timeout);
        const auto b = std::from_chars(l.data(), l.data() + l.size(), limit);
        if (a.ec != std::errc{} || a.ptr != t.data() + t.size() || b.ec != std::errc{} ||
            b.ptr != l.data() + l.size() || timeout < 100 || timeout > 60000 || limit < 4096 ||
            limit > 2ull * 1024 * 1024 * 1024)
            return 2;
        return helper_main(argv[2], argv[3], timeout, limit);
#else
        return 2;
#endif
    }
} // namespace OpenBlood2
