#include "launcher/controller.h"

namespace OpenBlood2::Launcher
{
    namespace fs = std::filesystem;
    bool Controller::busy() const
    {
        return status_ == Status::Querying || status_ == Status::Validating || status_ == Status::Running;
    }
    void Controller::fail(std::string error)
    {
        error_ = std::move(error);
        status_ = Status::Failed;
        launching_ = false;
    }
    void Controller::cancel()
    {
        if (status_ == Status::Running)
            return;
        probe_.cancel();
        validation_.cancel();
        status_ = Status::Empty;
        launching_ = false;
        error_.clear();
    }
    bool Controller::prepare(const Installation& item, const fs::path& executable,
                             const std::vector<fs::path>& overlays)
    {
        if (busy())
            return false;
        cancel();
        item_ = item;
        overlays_ = overlays;
        executable_ = executable;
        summary_ = {};
        if (!item.directory.is_absolute() || path_text(item.directory).find('"') != std::string::npos)
        {
            fail("Select an absolute data directory without double quotes");
            return false;
        }
        std::error_code ec;
        executable_size_ = fs::file_size(executable, ec);
        if (ec)
        {
            fail("Game executable is unavailable: " + path_text(executable));
            return false;
        }
        executable_time_ = fs::last_write_time(executable, ec);
        if (ec)
        {
            fail("Cannot inspect game executable");
            return false;
        }
        if (!probe_.start(executable_))
        {
            fail(probe_.error());
            return false;
        }
        status_ = Status::Querying;
        return true;
    }
    bool Controller::current_executable()
    {
        std::error_code ec;
        const auto size = fs::file_size(executable_, ec);
        if (ec || size != executable_size_)
        {
            fail("Game executable changed; check installation again");
            return false;
        }
        const auto time = fs::last_write_time(executable_, ec);
        if (ec || time != executable_time_)
        {
            fail("Game executable changed; check installation again");
            return false;
        }
        return true;
    }
    bool Controller::play(const fs::path& directory)
    {
        if (!playable() || !current_executable())
            return false;
        if (!directory.is_absolute())
        {
            fail("Launcher storage path must be absolute");
            return false;
        }
        directory_ = directory;
        launching_ = true;
        if (!validation_.start(item_, overlays_))
        {
            fail("Cannot start content validation: " + std::string(SDL_GetError()));
            return false;
        }
        status_ = Status::Validating;
        return true;
    }
    void Controller::update()
    {
        if (status_ == Status::Querying)
        {
            probe_.update();
            if (probe_.status() == Probe::Status::Failed)
            {
                fail(probe_.error());
                return;
            }
            if (probe_.status() == Probe::Status::Compatible)
            {
                if (!overlays_.empty() && !probe_.info().overlays)
                {
                    fail("This game build does not support mods. Update it or disable mods.");
                    return;
                }
                if (!validation_.start(item_, overlays_))
                {
                    fail("Cannot start content validation: " + std::string(SDL_GetError()));
                    return;
                }
                status_ = Status::Validating;
            }
        }
        if (status_ == Status::Validating)
        {
            ContentResult result;
            if (!validation_.poll(result))
                return;
            if (!result.valid)
            {
                fail(result.error);
                return;
            }
            summary_ = std::move(result.summary);
            if (!launching_)
            {
                status_ = Status::Ready;
                return;
            }
            launching_ = false;
            if (!current_executable())
                return;
            std::error_code ec;
            const auto parent = directory_ / "runs";
            fs::create_directories(parent, ec);
            if (ec)
            {
                fail("Cannot create launch directory: " + ec.message());
                return;
            }
            fs::path run;
            for (unsigned attempt = 0; attempt < 32; ++attempt)
            {
                const auto candidate = parent / (std::to_string(SDL_GetTicksNS()) + "-" + std::to_string(attempt));
                if (fs::create_directory(candidate, ec))
                {
                    run = candidate;
                    break;
                }
                if (ec)
                {
                    fail("Cannot create launch directory: " + ec.message());
                    return;
                }
            }
            if (run.empty())
            {
                fail("Cannot reserve a unique launch directory");
                return;
            }
            const auto config = run / "bootstrap.ini";
            logs_ = run / "logs";
            std::string error;
            if (!atomic_write(config, "data_dir=\"" + path_text(item_.directory) + "\"\n", error))
            {
                fail(error);
                return;
            }
            if (!game_.start(executable_, config, item_, logs_, error, overlays_))
            {
                fail(error);
                return;
            }
            status_ = Status::Running;
        }
        if (status_ == Status::Running && game_.update(exit_code_))
        {
            if (exit_code_ != 0)
                fail("Game exited with code " + std::to_string(exit_code_) + ". See game logs.");
            else
                status_ = Status::Finished;
        }
    }
} // namespace OpenBlood2::Launcher
