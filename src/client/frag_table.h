#pragma once
#include "client/announcer.h"
#include "engine/hud_surface.h"
#include "net/player_events.h"
#include "net/transport.h"
#include <functional>
#include <utility>
namespace OpenBlood2
{
    // Original CFragInfo Bloodbath/Team composition, on owned CPU surfaces.
    class ClientFragTable
    {
      public:
        explicit ClientFragTable(const Resources& resources) : resources_(resources), canvas_(resources) {}
        using TeamLabel = std::function<bool(DBYTE, HudSurface&, const HudFont&, const HudRect&)>;
        void set_team_label(TeamLabel label)
        {
            team_label_ = std::move(label);
            width_ = 0; // Recompose the next visible table, including with an unchanged roster.
        }
        bool compose(std::span<const Net::RosterPlayer>, unsigned width, unsigned height, std::string& error);
        bool compose_teams(std::span<const Net::RosterPlayer>, const std::array<int, 2>& totals, DBYTE column,
                           unsigned width, unsigned height, std::string& error);
        const HudSurface& surface() const
        {
            return surface_;
        }
        int x = 0, y = 0;
        unsigned displayed = 0;
        std::uint64_t revision = 0;

      private:
        const Resources& resources_;
        HudCanvas canvas_;
        ClientAnnouncer text_;
        TeamLabel team_label_;
        const HudSurface* voice_ = nullptr;
        std::array<std::string, 2> team_names_;
        std::array<std::vector<Net::RosterPlayer>, 2> team_order_;
        std::array<int, 2> team_totals_{};
        DBYTE team_column_ = 0;
        bool teams_ = false;
        HudFont font_;
        const HudSurface* bar_ = nullptr;
        std::array<const HudSurface*, 4> portraits_{};
        HudSurface surface_;
        std::vector<Net::RosterPlayer> previous_;
        unsigned width_ = 0, height_ = 0;
        bool initialize(std::string&);
    };
} // namespace OpenBlood2
