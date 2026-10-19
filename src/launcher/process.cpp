#include "launcher/process.h"
#include <array>

namespace OpenBlood2::Launcher
{
    Probe::~Probe()
    {
        cancel();
    }
    void Probe::cancel()
    {
        if (process_)
        {
            if (!SDL_WaitProcess(process_, false, nullptr))
            {
                SDL_KillProcess(process_, true);
                SDL_WaitProcess(process_, true, nullptr);
            }
            SDL_DestroyProcess(process_);
        }
        process_ = nullptr;
        output_ = errors_ = nullptr;
        status_ = Status::Idle;
    }
    void Probe::failed(std::string message)
    {
        cancel();
        error_ = std::move(message);
        status_ = Status::Failed;
    }
    bool Probe::start(const std::filesystem::path& executable, unsigned timeout_ms)
    {
        if (status_ == Status::Pending)
            return false;
        cancel();
        text_.clear();
        stderr_.clear();
        error_.clear();
        info_ = {};
        const auto path = path_text(executable);
        if (!executable.is_absolute() || !valid_text(path, 4096) || timeout_ms == 0 || timeout_ms > 5000)
        {
            failed("Invalid game executable or capability deadline");
            return false;
        }
        const char* args[] = {path.c_str(), "--launcher-info", nullptr};
        const auto props = SDL_CreateProperties();
        if (!props)
        {
            failed(SDL_GetError());
            return false;
        }
        const bool set =
            SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, const_cast<char**>(args)) &&
            SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL) &&
            SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP) &&
            SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_APP);
        if (set)
            process_ = SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props);
        if (!process_)
        {
            failed("Cannot query game: " + std::string(SDL_GetError()));
            return false;
        }
        output_ = SDL_GetProcessOutput(process_);
        errors_ = static_cast<SDL_IOStream*>(
            SDL_GetPointerProperty(SDL_GetProcessProperties(process_), SDL_PROP_PROCESS_STDERR_POINTER, nullptr));
        if (!output_ || !errors_)
        {
            failed("Cannot read game capability pipes");
            return false;
        }
        deadline_ = SDL_GetTicksNS() + Uint64(timeout_ms) * 1000000;
        status_ = Status::Pending;
        return true;
    }
    bool Probe::drain(SDL_IOStream* io, std::string& output)
    {
        std::array<char, 1024> chunk;
        for (unsigned n = 0; n < 8; ++n)
        {
            const auto size = SDL_ReadIO(io, chunk.data(), chunk.size());
            if (size > 4096 - output.size())
            {
                failed("Game capability output exceeds 4096 bytes");
                return false;
            }
            output.append(chunk.data(), size);
            if (size == 0)
            {
                const auto status = SDL_GetIOStatus(io);
                if (status != SDL_IO_STATUS_NOT_READY && status != SDL_IO_STATUS_EOF)
                {
                    failed("Cannot read game capabilities: " + std::string(SDL_GetError()));
                    return false;
                }
                break;
            }
        }
        return true;
    }
    void Probe::update()
    {
        if (status_ != Status::Pending)
            return;
        if (SDL_GetTicksNS() >= deadline_)
        {
            failed("Game capability query timed out");
            return;
        }
        if (!drain(output_, text_) || !drain(errors_, stderr_))
            return;
        int code = 0;
        if (!SDL_WaitProcess(process_, false, &code))
            return;
        if (!drain(output_, text_) || !drain(errors_, stderr_))
            return;
        SDL_DestroyProcess(process_);
        process_ = nullptr;
        output_ = errors_ = nullptr;
        if (code != 0)
        {
            failed("Game capability query failed (exit " + std::to_string(code) + "): " + stderr_);
            return;
        }
        std::string error;
        if (!parse_launcher_info(text_, info_, error))
        {
            failed(error);
            return;
        }
        status_ = Status::Compatible;
    }
    GameProcess::~GameProcess()
    {
        // Tracking destruction never terminates a playable child.
        if (process_)
            SDL_DestroyProcess(process_);
    }
    bool GameProcess::start(const std::filesystem::path& executable, const std::filesystem::path& config,
                            const Installation& item, const std::filesystem::path& logs, std::string& error,
                            const std::vector<std::filesystem::path>& overlays)
    {
        error.clear();
        if (process_)
        {
            error = "A game is already running";
            return false;
        }
        if (!executable.is_absolute() || !config.is_absolute() || !item.directory.is_absolute() || !logs.is_absolute())
        {
            error = "Game startup paths must be absolute";
            return false;
        }
        std::vector<std::string> values{path_text(executable),     "--config",  path_text(config), "--data-dir",
                                        path_text(item.directory), "--log-dir", path_text(logs)};
        if (item.edition == Edition::Demo)
            values.push_back("--demo");
        else if (item.edition != Edition::Retail)
        {
            error = "Unsupported edition";
            return false;
        }
        for (const auto& overlay : overlays)
        {
            if (!overlay.is_absolute())
            {
                error = "Mod path must be absolute";
                return false;
            }
            values.push_back("--rez");
            values.push_back(path_text(overlay));
        }
        std::vector<const char*> args;
        for (auto& value : values)
        {
            if (!valid_text(value, 4096))
            {
                error = "Invalid game argument";
                return false;
            }
            args.push_back(value.c_str());
        }
        args.push_back(nullptr);
        process_ = SDL_CreateProcess(args.data(), false);
        if (!process_)
            error = "Cannot start game: " + std::string(SDL_GetError());
        return process_ != nullptr;
    }
    bool GameProcess::update(int& exit_code)
    {
        if (!process_ || !SDL_WaitProcess(process_, false, &exit_code))
            return false;
        SDL_DestroyProcess(process_);
        process_ = nullptr;
        return true;
    }
} // namespace OpenBlood2::Launcher
