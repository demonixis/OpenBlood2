#pragma once
#include "launcher/state.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <mutex>
#include <optional>

namespace OpenBlood2::Launcher
{
    struct ContentSummary
    {
        Installation installation;
        DataStatus metadata;
        std::size_t resources = 0, mounts = 0;
    };
    bool validate_content(const Installation&, ContentSummary&, std::string&,
                          const std::vector<std::filesystem::path>& overlays = {});
    struct ContentResult
    {
        ContentSummary summary;
        std::string error;
        bool valid = false;
    };
    class ContentJob
    {
      public:
        ~ContentJob();
        bool start(const Installation& item, const std::vector<std::filesystem::path>& overlays = {});
        bool poll(ContentResult& result);
        void cancel();
        bool pending() const
        {
            return thread_ != nullptr;
        }

      private:
        std::mutex mutex_;
        std::optional<ContentResult> result_;
        static int SDLCALL execute(void* userdata);
        SDL_Thread* thread_ = nullptr;
        Installation item_;
        std::vector<std::filesystem::path> overlays_;
        std::atomic<bool> cancelled_ = false;
    };
} // namespace OpenBlood2::Launcher
