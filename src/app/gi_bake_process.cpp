#include "app/gi_bake_process.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace OpenBlood2
{
    bool gi_executable_path(std::filesystem::path& out, std::string& error)
    {
        std::error_code ec;
#ifdef __APPLE__
        std::uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::string buffer(size, '\0');
        if (!size || _NSGetExecutablePath(buffer.data(), &size) != 0)
            return error = "The executable path is unavailable", false;
        buffer.resize(std::strlen(buffer.c_str()));
        out = std::filesystem::weakly_canonical(path_from_utf8(buffer), ec);
#elif defined(_WIN32)
        std::wstring buffer(32768, L'\0');
        const auto length = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
        if (!length || length >= buffer.size())
            return error = "The executable path is unavailable", false;
        buffer.resize(length);
        out = std::filesystem::path(buffer);
#else
        out = std::filesystem::read_symlink("/proc/self/exe", ec);
#endif
        if (ec || out.empty())
            return error = "The executable path is unavailable", false;
        return true;
    }

    std::vector<std::string> gi_quality_arguments(const GiBakeSettings& settings)
    {
        auto hundredths = [](std::uint32_t value)
        {
            char text[16];
            std::snprintf(text, sizeof text, "%u.%02u", value / 100, value % 100);
            return std::string(text);
        };
        std::vector<std::string> args{"--gi-quality", gi_quality_name(settings.quality)};
        if (settings.quality == GiQuality::Custom)
        {
            args.insert(args.end(), {"--gi-bounces", std::to_string(settings.bounces), "--gi-rays",
                                     std::to_string(settings.rays), "--gi-sky", hundredths(settings.sky),
                                     "--gi-texel-samples", std::to_string(settings.texel_grid * settings.texel_grid),
                                     "--gi-denoise", std::to_string(settings.denoise), "--gi-reflection-strata",
                                     std::to_string(settings.reflection_strata)});
            if (settings.shadow_radius)
                args.insert(args.end(), {"--gi-shadow-radius", std::to_string(settings.shadow_radius),
                                         "--gi-shadow-rays", std::to_string(settings.shadow_rays)});
            if (settings.reflection_samples)
                args.insert(args.end(), {"--gi-reflection-samples", std::to_string(settings.reflection_samples)});
        }
        args.insert(args.end(), {"--gi-emissive", hundredths(settings.emissive)}); // On by default (Gate H).
        if (settings.direct == GiDirectSource::Original)
            args.insert(args.end(), {"--gi-direct", "original"});
        return args;
    }

    std::vector<std::string> gi_bake_arguments(const Config& config, std::string_view world,
                                               const GiBakeSettings& settings,
                                               const std::filesystem::path& placement_dir,
                                               const std::filesystem::path& out_dir, unsigned jobs)
    {
        std::vector<std::string> args{"--bake-gi", std::string(world)};
        const auto quality = gi_quality_arguments(settings);
        args.insert(args.end(), quality.begin(), quality.end());
        if (!config.data_dir.empty())
            args.insert(args.end(), {"--data-dir", path_text(config.data_dir)});
        if (config.demo)
            args.push_back("--demo");
        if (config.nightmares)
            args.push_back("--nightmares");
        for (const auto& overlay : config.overlays)
            args.insert(args.end(), {"--rez", path_text(overlay)});
        if (!placement_dir.empty())
            args.insert(args.end(), {"--gi-placement-dir", path_text(placement_dir)});
        if (!out_dir.empty())
            args.insert(args.end(), {"--gi-out", path_text(out_dir)});
        if (jobs)
            args.insert(args.end(), {"--gi-jobs", std::to_string(jobs)});
        return args;
    }

    namespace
    {
        bool number(std::string_view text, std::uint64_t& out)
        {
            const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
            return result.ec == std::errc{} && result.ptr == text.data() + text.size();
        }
        // " key=" with a lowercase/digit/underscore key starts at `at` (the space).
        std::size_t key_length(std::string_view line, std::size_t at)
        {
            std::size_t i = at + 1;
            while (i < line.size() && (std::islower(static_cast<unsigned char>(line[i])) ||
                                       std::isdigit(static_cast<unsigned char>(line[i])) || line[i] == '_'))
                ++i;
            return i > at + 1 && i < line.size() && line[i] == '=' ? i - at - 1 : 0;
        }
        int hex(char c)
        {
            return c >= '0' && c <= '9'   ? c - '0'
                   : c >= 'a' && c <= 'f' ? c - 'a' + 10
                   : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                          : -1;
        }
        // The body of a JSON string (between the quotes): the usual escapes, \uXXXX as UTF-8 (a lone
        // surrogate becomes U+FFFD). False on a malformed escape.
        bool json_body(std::string_view body, std::string& out)
        {
            out.clear();
            for (std::size_t i = 0; i < body.size(); ++i)
            {
                if (body[i] != '\\')
                {
                    out.push_back(body[i]);
                    continue;
                }
                if (++i >= body.size())
                    return false;
                switch (body[i])
                {
                    case '"':
                        out.push_back('"');
                        break;
                    case '\\':
                        out.push_back('\\');
                        break;
                    case '/':
                        out.push_back('/');
                        break;
                    case 'b':
                        out.push_back('\b');
                        break;
                    case 'f':
                        out.push_back('\f');
                        break;
                    case 'n':
                        out.push_back('\n');
                        break;
                    case 'r':
                        out.push_back('\r');
                        break;
                    case 't':
                        out.push_back('\t');
                        break;
                    case 'u':
                    {
                        if (i + 4 >= body.size())
                            return false;
                        std::uint32_t code = 0;
                        for (int k = 1; k <= 4; ++k)
                        {
                            const int v = hex(body[i + k]);
                            if (v < 0)
                                return false;
                            code = code * 16 + std::uint32_t(v);
                        }
                        i += 4;
                        if (code >= 0xD800 && code <= 0xDFFF)
                            code = 0xFFFD;
                        if (code < 0x80)
                            out.push_back(char(code));
                        else if (code < 0x800)
                        {
                            out.push_back(char(0xC0 | (code >> 6)));
                            out.push_back(char(0x80 | (code & 0x3F)));
                        }
                        else
                        {
                            out.push_back(char(0xE0 | (code >> 12)));
                            out.push_back(char(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back(char(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default:
                        return false;
                }
            }
            return true;
        }
    } // namespace

    bool parse_gi_progress(std::string_view line, GiBakeProgress& out)
    {
        constexpr std::string_view head = "GI_PROGRESS stage=";
        const auto start = line.find(head);
        if (start == std::string_view::npos)
            return false;
        line.remove_prefix(start + head.size());
        const auto done = line.rfind(" done="), total = line.rfind(" total=");
        if (done == std::string_view::npos || total == std::string_view::npos || total < done)
            return false;
        GiBakeProgress p;
        p.stage = std::string(line.substr(0, done));
        if (p.stage.empty() || !number(line.substr(done + 6, total - done - 6), p.done) ||
            !number(line.substr(total + 7), p.total) || p.done > p.total)
            return false;
        out = std::move(p);
        return true;
    }

    bool parse_gi_result(std::string_view line, std::map<std::string, std::string, std::less<>>& out)
    {
        constexpr std::string_view head = "GI_RESULT";
        const auto start = line.find(head);
        if (start == std::string_view::npos)
            return false;
        line.remove_prefix(start + head.size());
        std::map<std::string, std::string, std::less<>> fields;
        std::size_t at = 0;
        while (at < line.size())
        {
            const auto length = line[at] == ' ' ? key_length(line, at) : 0;
            if (!length)
                return false;
            const auto key = std::string(line.substr(at + 1, length));
            std::size_t value = at + 2 + length, end = value;
            if (end < line.size() && line[end] == '"')
            {
                for (++end; end < line.size() && line[end] != '"'; ++end)
                    if (line[end] == '\\')
                        ++end;
                if (end >= line.size())
                    return false;
                ++end;
                if (end < line.size() && line[end] != ' ')
                    return false;
                if (!json_body(line.substr(value + 1, end - value - 2), fields[key]))
                    return false;
            }
            else
            {
                while (end < line.size() && !(line[end] == ' ' && key_length(line, end)))
                    ++end;
                fields[key] = std::string(line.substr(value, end - value));
            }
            at = end;
        }
        if (fields.empty())
            return false;
        out = std::move(fields);
        return true;
    }

    double gi_bake_fraction(const GiBakeProgress& p)
    {
        if (!p.total)
            return 0;
        const double local = double(p.done) / double(p.total);
        if (p.stage == "survey")
            return .05 * local;
        if (p.stage == "probes")
            return .95 + .02 * local;
        if (p.stage == "reflections") // Lot G11.
            return .97 + .03 * local;
        unsigned pass = 0, passes = 0;
        if (std::sscanf(p.stage.c_str(), "bounce %u/%u", &pass, &passes) == 2 && pass >= 1 && pass <= passes)
            return .05 + .9 * ((pass - 1) + local) / passes;
        return 0;
    }

    GiBakeProcess::~GiBakeProcess()
    {
        stop();
    }

    bool GiBakeProcess::start(const std::filesystem::path& executable, const std::vector<std::string>& arguments,
                              std::string& message)
    {
        if (process_)
            return message = "A bake is already running", false;
        state = State::Idle;
        exit_code = 0;
        progress = {};
        result.clear();
        lines.clear();
        error.clear();
        partial_.clear();
        started_ns_ = ended_ns_ = 0;
        cancelling_ = false;
        const auto program = path_text(executable);
        std::vector<const char*> argv{program.c_str()};
        for (const auto& a : arguments)
            argv.push_back(a.c_str());
        argv.push_back(nullptr);
        const auto props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
        process_ = SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props);
        started_ns_ = SDL_GetTicksNS();
        if (!process_)
        {
            state = State::Failed;
            error = message = std::string("Could not start the bake: ") + SDL_GetError();
            return false;
        }
        state = State::Running;
        return true;
    }

    void GiBakeProcess::read_output()
    {
        auto* out = process_ ? SDL_GetProcessOutput(process_) : nullptr;
        if (!out)
            return;
        char buffer[4096];
        for (;;)
        {
            const auto n = SDL_ReadIO(out, buffer, sizeof buffer);
            if (!n)
                break;
            for (std::size_t i = 0; i < n; ++i)
            {
                if (buffer[i] == '\n')
                {
                    if (!partial_.empty() && partial_.back() == '\r')
                        partial_.pop_back();
                    line(partial_);
                    partial_.clear();
                }
                else if (partial_.size() < max_line)
                    partial_.push_back(buffer[i]);
            }
        }
    }

    void GiBakeProcess::line(std::string_view text)
    {
        GiBakeProgress p;
        if (parse_gi_progress(text, p))
            progress = std::move(p);
        else if (text.find("GI_RESULT") != std::string_view::npos)
        {
            std::map<std::string, std::string, std::less<>> fields;
            if (parse_gi_result(text, fields))
            {
                result = std::move(fields);
                if (const auto found = result.find("error"); found != result.end())
                    error = found->second;
            }
        }
        lines.emplace_back(text);
        while (lines.size() > max_lines)
            lines.pop_front();
    }

    void GiBakeProcess::finish(int code)
    {
        if (!partial_.empty())
        {
            line(partial_);
            partial_.clear();
        }
        exit_code = code;
        ended_ns_ = SDL_GetTicksNS();
        state = cancelling_ ? State::Cancelled : code < 0 ? State::Failed : State::Finished;
        if (state == State::Failed && error.empty())
            error = "The bake was terminated (signal " + std::to_string(-code) + ")";
        SDL_DestroyProcess(process_);
        process_ = nullptr;
    }

    void GiBakeProcess::poll()
    {
        if (!process_)
            return;
        read_output();
        int code = 0;
        if (SDL_WaitProcess(process_, false, &code))
        {
            read_output(); // What it printed before exiting.
            finish(code);
        }
    }

    void GiBakeProcess::cancel()
    {
        if (!process_ || cancelling_)
            return;
        cancelling_ = true;
#ifdef _WIN32
        // SDL's graceful kill posts WM_CLOSE, which a console child never reads: force it. The file is
        // written atomically at the very end, so a forced stop never leaves a partial .ob2gi file.
        SDL_KillProcess(process_, true);
#else
        SDL_KillProcess(process_, false); // SIGTERM: the bake's handler stops it (exit 12).
#endif
    }

    void GiBakeProcess::stop(std::uint32_t timeout_ms)
    {
        if (!process_)
            return;
        cancel();
        const auto deadline = SDL_GetTicks() + timeout_ms;
        while (process_ && SDL_GetTicks() < deadline)
        {
            poll();
            if (process_)
                SDL_Delay(5);
        }
        if (process_)
        {
            SDL_KillProcess(process_, true);
            int code = 0;
            SDL_WaitProcess(process_, true, &code);
            finish(code);
        }
    }

    double GiBakeProcess::elapsed_s() const
    {
        if (state == State::Idle)
            return 0;
        const auto end = process_ ? SDL_GetTicksNS() : ended_ns_ ? ended_ns_ : started_ns_;
        return double(end - started_ns_) / 1e9;
    }
    double GiBakeProcess::eta_s() const
    {
        const double f = gi_bake_fraction(progress);
        if (state != State::Running || f < .02)
            return -1;
        return elapsed_s() * (1 - f) / f;
    }

    const char* gi_bake_state_name(GiBakeProcess::State state)
    {
        switch (state)
        {
            case GiBakeProcess::State::Idle:
                return "idle";
            case GiBakeProcess::State::Running:
                return "running";
            case GiBakeProcess::State::Finished:
                return "finished";
            case GiBakeProcess::State::Cancelled:
                return "cancelled";
            case GiBakeProcess::State::Failed:
                break;
        }
        return "failed";
    }
} // namespace OpenBlood2
