#include "launcher/dialog.h"
#include "launcher/state.h"

namespace OpenBlood2::Launcher
{
    bool Dialog::open(SDL_Window* parent, bool executable)
    {
        if (pending_)
            return false;
        pending_ = std::make_shared<Completion>();
        pending_->result.executable = executable;
        auto* token = new std::shared_ptr<Completion>(pending_);
        if (show_)
            show_(completed, token, parent, executable);
        else if (executable)
            SDL_ShowOpenFileDialog(completed, token, parent, nullptr, 0, nullptr, false);
        else
            SDL_ShowOpenFolderDialog(completed, token, parent, nullptr, false);
        return true;
    }
    void SDLCALL Dialog::completed(void* userdata, const char* const* files, int)
    {
        std::unique_ptr<std::shared_ptr<Completion>> token(static_cast<std::shared_ptr<Completion>*>(userdata));
        auto& completion = **token;
        std::lock_guard lock(completion.mutex);
        if (!files)
            completion.result.error = SDL_GetError();
        else if (files[0])
        {
            const auto size = SDL_strnlen(files[0], 4097);
            if (size > 4096 || !valid_text(std::string_view(files[0], size), 4096))
                completion.result.error = "Selected path is invalid or exceeds 4096 bytes";
            else
                completion.result.path.assign(files[0], size);
        }
        if (!files && completion.result.error.empty())
            completion.result.error = "Native dialog failed";
        completion.ready = true;
    }
    bool Dialog::poll(DialogResult& result)
    {
        auto current = pending_;
        if (!current)
            return false;
        std::lock_guard lock(current->mutex);
        if (!current->ready)
            return false;
        result = std::move(current->result);
        pending_.reset();
        return true;
    }
} // namespace OpenBlood2::Launcher
