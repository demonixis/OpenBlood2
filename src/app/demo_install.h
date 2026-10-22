#pragma once
#include <SDL3/SDL_thread.h>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

// The original 1998 demo from Archive.org, checked against its qualified package and installed in
// the per-user content folder (setup_screen_contracts). No installer code is executed.
namespace OpenBlood2
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
    bool file_sha256(const std::filesystem::path&, std::string& digest, std::string& error, InstallProgress&);
    // Qualified ZIP -> newly created destination, removed on any failure.
    bool extract_demo(const std::filesystem::path& zip, const std::filesystem::path& destination, const DemoPackage&,
                      InstallProgress&, std::string& error);
    // https on port 443 of archive.org or one of its subdomains, without user information.
    bool demo_url_allowed(std::string_view url);
    struct InstallResult
    {
        std::filesystem::path directory; // The installed Game folder.
        std::string error;
        bool success = false;
    };
    class DemoInstaller
    {
      public:
        ~DemoInstaller();
        // `content` is the per-user content folder. Empty `local_archive` downloads the qualified
        // Archive.org package, only on explicit player action.
        bool start(const std::filesystem::path& content, const std::filesystem::path& local_archive = {});
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
        std::filesystem::path content_, local_;
        std::mutex mutex_;
        std::optional<InstallResult> result_;
    };
} // namespace OpenBlood2
