#pragma once
#include "engine/runtime.h"
#include "game/menu.h"

namespace OpenBlood2
{
    // Application-owned window changes, performed before acquiring a swapchain image.
    class DisplayOptions
    {
      public:
        explicit DisplayOptions(Runtime& runtime) : runtime_(runtime) {}
        void refresh(Menu&);
        bool apply(Menu&, std::string& error);
        static void pace(Uint64 frame_start, int fps);

      private:
        Runtime& runtime_;
        SDL_DisplayID display_ = 0;
        MenuSettings applied_;
        bool set_window(const MenuSettings&, std::string& error);
    };
} // namespace OpenBlood2
