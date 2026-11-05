#pragma once
#include "engine/hud_surface.h"
#include "net/player_events.h"
#include "net/transport.h"
namespace OpenBlood2
{
    // Original CFragInfo Bloodbath composition, on owned CPU surfaces.
    class ClientFragTable
    {
      public:
        explicit ClientFragTable(const Resources& resources) : canvas_(resources) {}
        bool compose(std::span<const Net::RosterPlayer>, unsigned width, unsigned height, std::string& error);
        const HudSurface& surface() const
        {
            return surface_;
        }
        int x = 0, y = 0;
        unsigned displayed = 0;
        std::uint64_t revision = 0;

      private:
        HudCanvas canvas_;
        HudFont font_;
        const HudSurface* bar_ = nullptr;
        std::array<const HudSurface*, 4> portraits_{};
        HudSurface surface_;
        std::vector<Net::RosterPlayer> previous_;
        unsigned width_ = 0, height_ = 0;
        bool initialize(std::string&);
    };
} // namespace OpenBlood2
