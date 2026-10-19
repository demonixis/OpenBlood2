#pragma once
#include "launcher/state.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <mutex>
#include <optional>

namespace OpenBlood2::Launcher
{
    struct DemoPackage
    {
        std::uint64_t bytes;
        std::string sha256;
        std::string rez_sha256;
    };
    const DemoPackage& demo_package();
    inline constexpr char demo_page[] = "https://archive.org/details/B2demo";
    enum class InstallStage
    {
        Downloading,
        Checking,
        Extracting,
        Validating,
        Complete
    };
    struct InstallProgress
    {
        std::atomic<bool> cancel{false};
        std::atomic<InstallStage> stage{InstallStage::Checking};
        std::atomic<std::uint64_t> bytes{0}, total{0};
    };
    bool file_sha256(const std::filesystem::path&, std::string&, std::string&, InstallProgress&);
    // Qualified ZIP -> newly created destination, removed on any failure. No installer code is executed.
    bool extract_demo(const std::filesystem::path& zip, const std::filesystem::path& destination, const DemoPackage&,
                      InstallProgress&, std::string& error);
    bool demo_url_allowed(std::string_view url);
    struct InstallResult
    {
        std::filesystem::path directory;
        std::string error;
        bool success = false;
    };
    class DemoInstaller
    {
      public:
        ~DemoInstaller();
        // Empty local_archive means the qualified Archive.org download, only on explicit user action.
        bool start(const std::filesystem::path& storage, const std::filesystem::path& local_archive = {});
        void cancel()
        {
            progress.cancel = true;
        }
        bool poll(InstallResult&);
        bool pending() const
        {
            return thread_ != nullptr;
        }
        InstallProgress progress;

      private:
        static int SDLCALL execute(void*);
        void work(InstallResult&);
        SDL_Thread* thread_ = nullptr;
        std::filesystem::path storage_, local_;
        std::mutex mutex_;
        std::optional<InstallResult> result_;
    };
} // namespace OpenBlood2::Launcher
