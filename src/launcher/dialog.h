#pragma once
#include <SDL3/SDL.h>
#include <memory>
#include <mutex>
#include <string>

namespace OpenBlood2::Launcher
{
    struct DialogResult
    {
        bool executable = false;
        std::string path, error;
    };
    class Dialog
    {
      public:
        using Show = void (*)(SDL_DialogFileCallback, void*, SDL_Window*, bool);
        explicit Dialog(Show show = nullptr) : show_(show) {}
        bool open(SDL_Window* parent, bool executable);
        bool poll(DialogResult& result);
        void abandon()
        {
            pending_.reset();
        }
        bool pending() const
        {
            return bool(pending_);
        }

      private:
        struct Completion
        {
            std::mutex mutex;
            bool ready = false;
            DialogResult result;
        };
        static void SDLCALL completed(void* userdata, const char* const* files, int);
        std::shared_ptr<Completion> pending_;
        Show show_ = nullptr;
    };
} // namespace OpenBlood2::Launcher
