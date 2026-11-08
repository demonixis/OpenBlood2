// GI lot G10: the F8 bake's child process (gi_contracts.md): the GI_PROGRESS/GI_RESULT parsers, the
// overall fraction, the child's arguments, and GiBakeProcess against this executable started as a
// stub child (exit codes, merged stderr, bounded lines, graceful cancel, forced stop, destructor).
// `--bake-gi …` makes this executable a fake baker for the overlay tests (gi_overlay.retail).
#include "app/gi_bake_process.h"
#include "scratch_dir.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#ifndef _WIN32
#include <cerrno>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace OpenBlood2;

#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace
{
    std::atomic<bool> terminated{false};
    extern "C" void on_term(int)
    {
        terminated.store(true);
    }
    void pause_ms(int ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    // ---- Stub children ----

    int stub_ok()
    {
        for (int i = 0; i < 70; ++i)
            std::printf("filler %d\n", i);
        std::fflush(stdout);
        std::fprintf(stderr, "a stderr line\n");
        std::printf("GI_PROGRESS stage=survey done=3 total=10\n");
        std::printf("%s\n", std::string(10000, 'x').c_str());
        std::printf("\r\n"); // An empty CRLF line.
        std::printf("GI_PROGRESS stage=bounce 1/2 done=5 total=10\n");
        std::printf("GI_RESULT status=baked file=/tmp/a b/c.ob2gi bytes=42 name=\"q \\\"x\\\" \\u00e9\\n\"\n");
        std::printf("last line without newline");
        std::fflush(stdout);
        return 0;
    }
    int stub_fail()
    {
        std::printf("GI_RESULT status=failed world=worlds/x.dat error=\"GI scene: boom\" ms=1\n");
        std::fflush(stdout);
        return 4;
    }
    // Waits for SIGTERM (graceful cancel), as the real baker's handler does.
    int stub_slow()
    {
        std::signal(SIGTERM, on_term);
        std::printf("GI_PROGRESS stage=survey done=0 total=1\n");
        std::fflush(stdout);
        for (int i = 0; i < 3000 && !terminated.load(); ++i)
            pause_ms(10);
        if (!terminated.load())
            return 3;
        std::printf("GI_RESULT status=cancelled error=\"GI bake cancelled\"\n");
        std::fflush(stdout);
        return 12;
    }
#ifndef _WIN32
    int stub_stubborn()
    {
        std::signal(SIGTERM, SIG_IGN);
        std::printf("GI_PROGRESS stage=survey done=0 total=1\n");
        std::fflush(stdout);
        for (int i = 0; i < 3000; ++i)
            pause_ms(10);
        return 3;
    }
    int stub_pid(const char* file)
    {
        {
            std::ofstream out(file);
            out << getpid() << '\n';
        }
        std::printf("ready\n");
        std::fflush(stdout);
        for (int i = 0; i < 3000; ++i)
            pause_ms(10);
        return 3;
    }
#endif
    // The fake baker: its arguments go to <out>/args.txt; Preview takes ~0.2 s, Final ~1.5 s, a
    // graceful kill exits 12 and prints the cancelled result.
    int fake_baker(int argc, char** argv)
    {
        std::signal(SIGTERM, on_term);
        std::string out, quality = "final", world = argc > 2 ? argv[2] : "";
        for (int i = 1; i + 1 < argc; ++i)
        {
            if (std::string_view(argv[i]) == "--gi-out")
                out = argv[i + 1];
            if (std::string_view(argv[i]) == "--gi-quality")
                quality = argv[i + 1];
        }
        if (out.empty())
            return 2;
        std::error_code ec;
        std::filesystem::create_directories(path_from_utf8(out), ec);
        {
            std::ofstream args(path_from_utf8(out) / "args.txt");
            for (int i = 1; i < argc; ++i)
                args << argv[i] << '\n';
        }
        const int steps = quality == "preview" ? 10 : 30, delay = quality == "preview" ? 20 : 50;
        std::printf("GI scene: building\n");
        for (int i = 0; i <= steps; ++i)
        {
            if (terminated.load())
            {
                std::printf("GI_RESULT status=cancelled world=%s error=\"GI bake cancelled\"\n", world.c_str());
                std::fflush(stdout);
                return 12;
            }
            std::printf("GI_PROGRESS stage=bounce 1/1 done=%d total=%d\n", i, steps);
            std::fflush(stdout);
            pause_ms(delay);
        }
        std::printf("GI_RESULT status=baked world=%s file=%s/fake.ob2gi bytes=1 ms=1\n", world.c_str(), out.c_str());
        std::fflush(stdout);
        return 0;
    }

    // ---- Tests ----

    bool wait(GiBakeProcess& p, int timeout_ms, bool (*until)(const GiBakeProcess&))
    {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < end)
        {
            p.poll();
            if (until(p))
                return true;
            pause_ms(5);
        }
        return false;
    }
    bool stopped(const GiBakeProcess& p)
    {
        return !p.running();
    }
    bool progressed(const GiBakeProcess& p)
    {
        return !p.progress.stage.empty() || !p.running();
    }
    bool has_line(const GiBakeProcess& p, std::string_view text)
    {
        for (const auto& l : p.lines)
            if (l == text)
                return true;
        return false;
    }

    bool parse()
    {
        GiBakeProgress p;
        CHECK(parse_gi_progress("GI_PROGRESS stage=survey done=3 total=10", p));
        CHECK(p.stage == "survey" && p.done == 3 && p.total == 10);
        CHECK(parse_gi_progress("[info] GI_PROGRESS stage=bounce 2/3 done=0 total=7", p));
        CHECK(p.stage == "bounce 2/3" && p.done == 0 && p.total == 7);
        for (const char* bad : {"GI_PROGRESS stage=survey done=11 total=10", "GI_PROGRESS stage=survey done=1",
                                "GI_PROGRESS stage= done=1 total=2", "GI_PROGRESS stage=x done=1x total=2",
                                "GI_PROGRESS stage=x done=-1 total=2", "GI_PROGRESS stage=x total=2 done=1",
                                "progress stage=x done=1 total=2", ""})
            CHECK(!parse_gi_progress(bad, p));
        CHECK(p.stage == "bounce 2/3"); // Refusals leave the output.

        std::map<std::string, std::string, std::less<>> r;
        CHECK(parse_gi_result("GI_RESULT status=baked world=worlds/01_subwaya.dat file=/Users/a b/gi/x y.ob2gi "
                              "bytes=12 back_fraction=0,1,2",
                              r));
        CHECK(r.size() == 5 && r["status"] == "baked" && r["file"] == "/Users/a b/gi/x y.ob2gi");
        CHECK(r["bytes"] == "12" && r["back_fraction"] == "0,1,2");
        CHECK(parse_gi_result("GI_RESULT status=failed error=\"GI scene: \\\"a\\\" \\\\ \\u00e9 \\u0041\" ms=3", r));
        CHECK(r.size() == 3 && r["error"] == "GI scene: \"a\" \\ \xc3\xa9 A" && r["ms"] == "3");
        CHECK(parse_gi_result("GI_RESULT error=\"\"", r) && r["error"].empty());
        for (const char* bad : {"GI_RESULT", "GI_RESULT ", "GI_RESULT status", "GI_RESULT =x", "GI_RESULT error=\"open",
                                "GI_RESULT error=\"a\"b", "GI_RESULT error=\"\\q\"", "GI_RESULT error=\"\\u12\"",
                                "GI_RESULTstatus=x", "status=baked"})
            CHECK(!parse_gi_result(bad, r));
        CHECK(r.size() == 1); // Refusals leave the output.

        auto fraction = [](std::string stage, std::uint64_t done, std::uint64_t total)
        { return gi_bake_fraction({std::move(stage), done, total}); };
        CHECK(fraction("survey", 5, 10) == .025 && fraction("survey", 10, 10) == .05);
        CHECK(fraction("bounce 1/2", 0, 10) == .05 && std::abs(fraction("bounce 2/2", 10, 10) - .95) < 1e-12);
        CHECK(std::abs(fraction("bounce 1/2", 5, 10) - .275) < 1e-12);
        CHECK(fraction("probes", 0, 4) == .95 && std::abs(fraction("probes", 4, 4) - .97) < 1e-12);
        CHECK(std::abs(fraction("reflections", 0, 4) - .97) < 1e-12 && std::abs(fraction("reflections", 4, 4) - 1) < 1e-12);
        CHECK(fraction("bounce 3/2", 1, 2) == 0 && fraction("bounce 0/2", 1, 2) == 0 && fraction("other", 1, 2) == 0);
        CHECK(fraction("survey", 0, 0) == 0 && gi_bake_fraction({}) == 0);

        Config config;
        GiBakeSettings preview, final_quality, original;
        preview.quality = GiQuality::Preview;
        original.quality = GiQuality::Preview;
        original.direct = GiDirectSource::Original;
        auto args = gi_bake_arguments(config, "worlds/01_subwaya.dat", preview, {}, {});
        CHECK((args == std::vector<std::string>{"--bake-gi", "worlds/01_subwaya.dat", "--gi-quality", "preview"}));
        config.data_dir = "/games/blood 2";
        config.demo = true;
        config.nightmares = true;
        config.overlays = {"/mods/a.rez", "/mods/b c.rez"};
        args = gi_bake_arguments(config, "worlds/x.dat", final_quality, "/p q", "/o", 6);
        CHECK((args == std::vector<std::string>{"--bake-gi", "worlds/x.dat", "--gi-quality", "final", "--data-dir",
                                                "/games/blood 2", "--demo", "--nightmares", "--rez", "/mods/a.rez",
                                                "--rez", "/mods/b c.rez", "--gi-placement-dir", "/p q", "--gi-out",
                                                "/o", "--gi-jobs", "6"}));
        // Lot GD2: re-baked is the child's default (Gate G) and adds nothing; the original direct
        // source follows the quality.
        auto lone = gi_bake_arguments(config, "worlds/01_subwaya.dat", original, {}, {}, 0);
        CHECK((lone == std::vector<std::string>{"--bake-gi", "worlds/01_subwaya.dat", "--gi-quality", "preview",
                                                "--gi-direct", "original", "--data-dir", "/games/blood 2", "--demo",
                                                "--nightmares", "--rez", "/mods/a.rez", "--rez", "/mods/b c.rez"}));

        // Lots GQ1 and GE1: Ultra is its name alone; Custom gives every setting (shadow rays and GGX
        // samples only when set); emission goes with any preset; the child parses them all back into
        // the same settings.
        GiBakeSettings ultra;
        ultra.quality = GiQuality::Ultra;
        CHECK((gi_quality_arguments(ultra) == std::vector<std::string>{"--gi-quality", "ultra"}));
        ultra.emissive = 5;
        CHECK((gi_quality_arguments(ultra) == std::vector<std::string>{"--gi-quality", "ultra", "--gi-emissive", "0.05"}));
        GiBakeSettings custom;
        custom.quality = GiQuality::Custom;
        CHECK((gi_quality_arguments(custom) ==
               std::vector<std::string>{"--gi-quality", "custom", "--gi-bounces", "2", "--gi-rays", "256", "--gi-sky",
                                        "1.00", "--gi-texel-samples", "1", "--gi-denoise", "0",
                                        "--gi-reflection-strata", "2"}));
        custom = {GiQuality::Custom, 4, 1024, 250, GiDirectSource::Original, 3, 2, 10, 7, 5, 512, 400};
        const auto tuned = gi_quality_arguments(custom);
        CHECK((tuned == std::vector<std::string>{"--gi-quality", "custom", "--gi-bounces", "4", "--gi-rays", "1024",
                                                 "--gi-sky", "2.50", "--gi-texel-samples", "9", "--gi-denoise", "2",
                                                 "--gi-reflection-strata", "5", "--gi-shadow-radius", "10",
                                                 "--gi-shadow-rays", "7", "--gi-reflection-samples", "512",
                                                 "--gi-emissive", "4.00", "--gi-direct", "original"}));
        std::vector<std::string> command{"blood2", "--bake-gi", "worlds/x.dat"};
        command.insert(command.end(), tuned.begin(), tuned.end());
        std::vector<std::string_view> views(command.begin() + 1, command.end());
        Options options;
        std::string parse_error;
        CHECK(parse_options(views, options, parse_error));
        GiBakeArguments parsed;
        parsed.quality = options.gi_quality;
        parsed.bounces = options.gi_bounces;
        parsed.rays = options.gi_rays;
        parsed.sky = options.gi_sky;
        parsed.texel_samples = options.gi_texel_samples;
        parsed.denoise = options.gi_denoise;
        parsed.shadow_radius = options.gi_shadow_radius;
        parsed.shadow_rays = options.gi_shadow_rays;
        parsed.reflection_strata = options.gi_reflection_strata;
        parsed.reflection_samples = options.gi_reflection_samples;
        parsed.emissive = options.gi_emissive;
        GiBakeSettings back;
        CHECK(make_gi_bake_settings(parsed, back, parse_error));
        back.direct = options.gi_direct == "original" ? GiDirectSource::Original : GiDirectSource::Rebaked;
        CHECK(gi_settings_text(back) == gi_settings_text(custom));

        CHECK(std::string(gi_bake_state_name(GiBakeProcess::State::Idle)) == "idle");
        CHECK(std::string(gi_bake_state_name(GiBakeProcess::State::Cancelled)) == "cancelled");
        CHECK(std::string(gi_bake_state_name(GiBakeProcess::State::Failed)) == "failed");
        GiBakeProcess idle;
        CHECK(idle.state == GiBakeProcess::State::Idle && idle.elapsed_s() == 0 && idle.eta_s() < 0);
        idle.poll();
        idle.cancel();
        idle.stop();
        CHECK(idle.state == GiBakeProcess::State::Idle);
        return true;
    }

    bool process(const std::filesystem::path& scratch)
    {
        std::filesystem::path self;
        std::string error;
        CHECK(gi_executable_path(self, error) && error.empty());
        CHECK(self.stem() == "gi_bake_process_tests" && std::filesystem::is_regular_file(self));

        {
            GiBakeProcess p;
            CHECK(p.start(self, {"stub-ok"}, error) && p.running());
            CHECK(!p.start(self, {"stub-ok"}, error) && error == "A bake is already running");
            CHECK(wait(p, 20000, stopped));
            CHECK(p.state == GiBakeProcess::State::Finished && p.exit_code == 0 && p.error.empty());
            CHECK(p.progress.stage == "bounce 1/2" && p.progress.done == 5 && p.progress.total == 10);
            CHECK(p.result.size() == 4 && p.result["status"] == "baked" && p.result["file"] == "/tmp/a b/c.ob2gi");
            CHECK(p.result["name"] == "q \"x\" \xc3\xa9\n");
            CHECK(p.lines.size() == GiBakeProcess::max_lines && p.lines.back() == "last line without newline");
            CHECK(has_line(p, "a stderr line") && has_line(p, "")); // stderr merged; CR dropped.
            CHECK(has_line(p, std::string(GiBakeProcess::max_line, 'x')));
            CHECK(p.elapsed_s() > 0 && p.eta_s() < 0);
        }
        {
            GiBakeProcess p;
            CHECK(p.start(self, {"stub-fail"}, error) && wait(p, 20000, stopped));
            CHECK(p.state == GiBakeProcess::State::Finished && p.exit_code == 4 && p.error == "GI scene: boom");
            // A finished process starts again with fresh state.
            CHECK(p.start(self, {"stub-ok"}, error) && p.error.empty() && p.result.empty() && p.lines.empty());
            CHECK(wait(p, 20000, stopped) && p.exit_code == 0);
        }
        {
            GiBakeProcess p;
            const auto missing = scratch / "no such executable";
            if (p.start(missing, {}, error))
                CHECK(wait(p, 20000, stopped) && (p.state == GiBakeProcess::State::Failed || p.exit_code != 0));
            else
                CHECK(p.state == GiBakeProcess::State::Failed && !p.error.empty() && error == p.error);
        }
#ifndef _WIN32
        {
            // Graceful cancel: the child's SIGTERM handler exits 12.
            GiBakeProcess p;
            CHECK(p.start(self, {"stub-slow"}, error) && wait(p, 20000, progressed) && p.running());
            p.cancel();
            p.cancel(); // Once.
            CHECK(wait(p, 20000, stopped));
            CHECK(p.state == GiBakeProcess::State::Cancelled && p.exit_code == 12);
            CHECK(p.result["status"] == "cancelled" && p.error == "GI bake cancelled");
        }
        {
            // A child that ignores SIGTERM is killed after the stop timeout.
            GiBakeProcess p;
            CHECK(p.start(self, {"stub-stubborn"}, error) && wait(p, 20000, progressed) && p.running());
            const auto t0 = std::chrono::steady_clock::now();
            p.stop(300);
            const auto took = std::chrono::steady_clock::now() - t0;
            CHECK(took >= std::chrono::milliseconds(250) && took < std::chrono::seconds(5));
            CHECK(p.state == GiBakeProcess::State::Cancelled && p.exit_code != 0 && !p.running());
        }
        {
            // The destructor stops and reaps a running child.
            const auto file = scratch / "pid.txt";
            long pid = 0;
            {
                GiBakeProcess p;
                CHECK(p.start(self, {"stub-pid", path_text(file)}, error));
                CHECK(wait(p, 20000, [](const GiBakeProcess& q) { return has_line(q, "ready") || !q.running(); }));
                CHECK(p.running());
                std::ifstream in(file);
                CHECK(in >> pid && pid > 0);
            }
            CHECK(::kill(pid_t(pid), 0) == -1 && errno == ESRCH);
        }
#endif
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view name(argv[1]);
    if (name == "stub-ok")
        return stub_ok();
    if (name == "stub-fail")
        return stub_fail();
    if (name == "stub-slow")
        return stub_slow();
#ifndef _WIN32
    if (name == "stub-stubborn")
        return stub_stubborn();
    if (name == "stub-pid" && argc == 3)
        return stub_pid(argv[2]);
#endif
    if (name == "--bake-gi")
        return fake_baker(argc, argv);
    bool ok = false;
    if (name == "parse")
        ok = parse();
    else if (name == "process" && argc == 3)
    {
        const auto scratch = path_from_utf8(argv[2]);
        ok = Testing::reset_scratch_dir(scratch) && process(scratch) && Testing::remove_scratch_dir(scratch);
    }
    else
        return 2;
    return ok ? 0 : 1;
}
