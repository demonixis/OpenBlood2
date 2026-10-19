#include "launcher/content.h"
#include "engine/resources.h"

namespace OpenBlood2::Launcher
{
    namespace fs = std::filesystem;
    bool validate_content(const Installation& item, ContentSummary& output, std::string& error,
                          const std::vector<fs::path>& overlays)
    {
        error.clear();
        if (!item.directory.is_absolute() || !valid_text(path_text(item.directory), 4096) ||
            (item.edition != Edition::Retail && item.edition != Edition::Demo))
        {
            error = "Select an absolute data directory and a supported edition";
            return false;
        }
        ContentSummary next;
        next.installation = item;
        Config config{item.directory, "launcher", item.edition == Edition::Demo, overlays};
        Resources resources;
        if (!inspect_data(config, next.metadata, error) || !resources.mount_game(config, error))
            return false;
        if (resources.index().empty())
        {
            error = "Selected edition has an empty resource index";
            return false;
        }
        next.resources = resources.index().size();
        next.mounts = resources.mounts().size();
        output = std::move(next);
        return true;
    }
    ContentJob::~ContentJob()
    {
        cancel();
    }
    void ContentJob::cancel()
    {
        if (thread_)
        {
            cancelled_ = true;
            SDL_WaitThread(thread_, nullptr);
            thread_ = nullptr;
        }
        std::lock_guard lock(mutex_);
        result_.reset();
    }
    bool ContentJob::start(const Installation& item, const std::vector<fs::path>& overlays)
    {
        if (pending())
            return false;
        item_ = item;
        overlays_ = overlays;
        cancelled_ = false;
        thread_ = SDL_CreateThread(execute, "Launcher content validation", this);
        return thread_ != nullptr;
    }
    int SDLCALL ContentJob::execute(void* userdata)
    {
        auto& owner = *static_cast<ContentJob*>(userdata);
        ContentResult result;
        result.valid = validate_content(owner.item_, result.summary, result.error, owner.overlays_);
        if (!owner.cancelled_)
        {
            std::lock_guard lock(owner.mutex_);
            owner.result_ = std::move(result);
        }
        return 0;
    }
    bool ContentJob::poll(ContentResult& result)
    {
        {
            std::lock_guard lock(mutex_);
            if (!result_)
                return false;
            result = std::move(*result_);
            result_.reset();
        }
        SDL_WaitThread(thread_, nullptr);
        thread_ = nullptr;
        return true;
    }
} // namespace OpenBlood2::Launcher
