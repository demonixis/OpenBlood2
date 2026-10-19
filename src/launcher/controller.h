#pragma once
#include "launcher/content.h"
#include "launcher/process.h"

namespace OpenBlood2::Launcher
{
    class Controller
    {
      public:
        enum class Status
        {
            Empty,
            Querying,
            Validating,
            Ready,
            Running,
            Finished,
            Failed
        };
        bool prepare(const Installation& item, const std::filesystem::path& executable);
        bool play(const std::filesystem::path& directory);
        void update();
        void cancel();
        bool busy() const;
        bool playable() const
        {
            return status_ == Status::Ready || status_ == Status::Finished;
        }
        Status status() const
        {
            return status_;
        }
        const std::string& error() const
        {
            return error_;
        }
        const ContentSummary& content() const
        {
            return summary_;
        }
        const LauncherInfo& info() const
        {
            return probe_.info();
        }
        const std::filesystem::path& logs() const
        {
            return logs_;
        }
        int exit_code() const
        {
            return exit_code_;
        }

      private:
        void fail(std::string error);
        bool current_executable();
        Probe probe_;
        ContentJob validation_;
        GameProcess game_;
        Installation item_;
        ContentSummary summary_;
        std::filesystem::path executable_, directory_, logs_;
        std::filesystem::file_time_type executable_time_{};
        std::uintmax_t executable_size_ = 0;
        std::string error_;
        Status status_ = Status::Empty;
        bool launching_ = false;
        int exit_code_ = 0;
    };
} // namespace OpenBlood2::Launcher
