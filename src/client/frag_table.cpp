// Selected ClientShellDLL/FragInfo.cpp: Bloodbath layout and DrawClip.
// Copyright (c) 1998 Monolith Productions Inc.
#include "client/frag_table.h"
#include <algorithm>
namespace OpenBlood2
{
    bool ClientFragTable::initialize(std::string& error)
    {
        if (bar_)
            return true;
        static constexpr const char* portraits[]{"interface/fragbar/c_frag.pcx", "interface/fragbar/o_frag.pcx",
                                                 "interface/fragbar/i_frag.pcx", "interface/fragbar/g_frag.pcx"};
        for (unsigned i = 0; i < 4; ++i)
            portraits_[i] = canvas_.CreateSurfaceFromBitmap(portraits[i]);
        const auto* bar = canvas_.CreateSurfaceFromBitmap("interface/fragbar/fragbar.pcx");
        if (!canvas_.load_font("interface/fragbar/frag_font_1.pcx", "interface/fragbar/frag_font_1.fnt", font_) ||
            !bar || !std::all_of(portraits_.begin(), portraits_.end(), [](auto* p) { return p; }))
        {
            error = canvas_.error;
            return false;
        }
        if (bar->width > 1024 || bar->height < 19 || bar->height > 128 || font_.height > bar->height || bar->width < 20)
        {
            error = "Invalid frag bar dimensions";
            return false;
        }
        for (const auto* portrait : portraits_)
            if (portrait->width > bar->width || portrait->height > bar->height)
            {
                error = "Invalid frag portrait dimensions";
                return false;
            }
        // Existing CoolFont service adapter; original CoolFont implementation is unavailable.
        font_.space_width = std::max(1u, font_.height / 4);
        bar_ = bar;
        return true;
    }
    void ClientFragTable::draw_player(std::string text, int x, int y)
    {
        constexpr int max_width = 145;
        const HudRect row{x, y, x + max_width, y + int(font_.height)};
        if (player_label_ && player_label_(text, surface_, font_, row))
            return;
        // Original DrawClip draws the whole string if it fits, then also
        // draws the shortened string; preserve that harmless repeated draw.
        if (font_.text_width(text) < max_width)
            font_.draw(surface_, text, x, y, 0);
        while (text.size() > 1)
        {
            text.pop_back();
            if (font_.text_width(text) < max_width)
            {
                font_.draw(surface_, text, x, y, 0);
                break;
            }
        }
    }

    bool ClientFragTable::compose(std::span<const Net::RosterPlayer> players, unsigned width, unsigned height,
                                  std::string& error)
    {
        error.clear();
        if (!width || !height || width > 16384 || height > 16384 || players.size() > Net::max_players)
        {
            error = "Invalid frag table viewport/row count";
            return false;
        }
        for (const auto& p : players)
            if (p.character >= 4 || p.name.empty() || p.name.size() > 100)
            {
                error = "Invalid frag table player";
                return false;
            }
        if (!initialize(error))
            return false;
        if (!teams_ && width == width_ && height == height_ &&
            std::equal(players.begin(), players.end(), previous_.begin(), previous_.end(),
                       [](const auto& a, const auto& b)
                       {
                           return a.player == b.player && a.object == b.object && a.name == b.name &&
                                  a.character == b.character && a.frags == b.frags;
                       }))
            return true;
        // Avoid the source unsigned underflow on tiny windows. Composition is
        // bounded by 16 rows, independent of a large physical display height.
        displayed = height <= 2 * bar_->height
                        ? 0
                        : std::min<unsigned>(players.size(), (height - 2 * bar_->height) / bar_->height);
        x = (int(width) - int(bar_->width)) / 2;
        y = displayed ? int(bar_->height + (height - 2 * bar_->height - displayed * bar_->height) / 2) : 0;
        surface_ = {bar_->width, displayed * bar_->height,
                    std::vector<std::uint8_t>(bar_->width * displayed * bar_->height * 4)};
        const bool scrunch = height < 350;
        canvas_.FillRect(&surface_, nullptr, hud_magenta);
        for (unsigned i = 0; i < displayed; ++i)
        {
            const auto& p = players[i];
            const auto* portrait = portraits_[p.character];
            const int row = int(i * (bar_->height - (scrunch ? 18 : 0)));
            if (!scrunch)
            {
                canvas_.DrawSurfaceToSurface(&surface_, bar_, nullptr, 0, row);
                canvas_.DrawSurfaceToSurface(&surface_, portrait, nullptr, 0, row);
            }
            const int text_y = row + int((bar_->height - font_.height) / 2);
            // Original unknown-ping branch; measured pings remain separate.
            draw_player("(-) " + p.name, int(portrait->width), text_y);
            font_.draw(surface_, std::to_string(p.frags), int(bar_->width) - 20, text_y, 1);
        }
        for (std::size_t i = 0; i < surface_.rgba.size(); i += 4)
            if (surface_.rgba[i] == 255 && surface_.rgba[i + 1] == 0 && surface_.rgba[i + 2] == 255)
                surface_.rgba[i + 3] = 0;
        teams_ = false;
        previous_.assign(players.begin(), players.end());
        width_ = width;
        height_ = height;
        ++revision;
        return true;
    }
    bool ClientFragTable::compose_teams(std::span<const Net::RosterPlayer> players, const std::array<int, 2>& totals,
                                        DBYTE column, unsigned width, unsigned height, std::string& error)
    {
        error.clear();
        if (!width || !height || width > 16384 || height > 16384 || players.size() > Net::max_players || column > 2 ||
            std::any_of(totals.begin(), totals.end(), [](int score) { return score < -1000000 || score > 1000000; }))
        {
            error = "Invalid team table viewport/count/scores";
            return false;
        }
        for (const auto& p : players)
            if (p.character >= 4 || p.team < 1 || p.team > 2 || p.name.empty() || p.name.size() > 100)
            {
                error = "Invalid team table player";
                return false;
            }
        if (!initialize(error))
            return false;
        if (!voice_)
        {
            voice_ = canvas_.CreateSurfaceFromBitmap("interface/fragbar/v_frag.pcx");
            if (!voice_ || voice_->width > bar_->width || voice_->height > bar_->height)
            {
                error = canvas_.error.empty() ? "Invalid team title portrait" : canvas_.error;
                voice_ = nullptr;
                return false;
            }
            for (DBYTE team = 1; team <= 2; ++team)
                team_names_[team - 1] = text_.team_name(team, resources_);
        }
        if (teams_ && width == width_ && height == height_ && column == team_column_ && totals == team_totals_ &&
            std::equal(players.begin(), players.end(), previous_.begin(), previous_.end()))
            return true;
        // CTeam::SortPlayersByFrags is a stable descending insertion sort at
        // draw time. Preserve the last displayed tie order, including hidden updates.
        for (DBYTE team = 1; team <= 2; ++team)
        {
            auto& ordered = team_order_[team - 1];
            std::erase_if(ordered,
                          [&](const auto& old)
                          {
                              return std::none_of(
                                  players.begin(), players.end(), [&](const auto& p)
                                  { return p.player == old.player && p.object == old.object && p.team == team; });
                          });
            for (const auto& p : players)
                if (p.team == team)
                {
                    const auto old = std::find_if(ordered.begin(), ordered.end(),
                                                  [&](const auto& row) { return row.player == p.player; });
                    if (old == ordered.end())
                        ordered.push_back(p);
                    else
                        *old = p;
                }
            std::stable_sort(ordered.begin(), ordered.end(),
                             [](const auto& a, const auto& b) { return a.frags > b.frags; });
        }
        const bool single = width < 500, scrunch = height < 350;
        const unsigned available = height > 2 * bar_->height ? height - 2 * bar_->height : 0;
        const unsigned maximum = available / bar_->height;
        displayed = 0;
        surface_ = {};
        x = y = 0;
        if (maximum && (!single || column))
        {
            const unsigned first = single ? column - 1 : 0, columns = single ? 1 : 2;
            unsigned largest = 0, shown = 0;
            for (unsigned team = first; team < first + columns; ++team)
            {
                largest = std::max(largest, unsigned(team_order_[team].size()));
                shown = std::max(shown, std::min(unsigned(team_order_[team].size()), maximum - 1));
            }
            const unsigned residual = (available - maximum * bar_->height) / 2;
            const unsigned row_step = bar_->height - (scrunch ? 18 : 0);
            const unsigned player_start =
                unsigned(std::max(0, int(residual + bar_->height + font_.height / 2) - (scrunch ? 20 : 0)));
            // Scrunch advances less than one full glyph row. Keep the last
            // glyph's bottom, not merely the following row's starting point.
            const unsigned player_bottom = shown ? player_start + (shown - 1) * row_step + bar_->height : 0;
            const unsigned bottom = std::max(residual + bar_->height, player_bottom);
            surface_ = {columns * bar_->width + (columns - 1) * 32, std::min(available, bottom), {}};
            surface_.rgba.resize(surface_.width * surface_.height * 4);
            canvas_.FillRect(&surface_, nullptr, hud_magenta);
            x = std::max(0, (int(width) - int(surface_.width)) / 2);
            y = std::max(0, int(height / 2) - int(bar_->height * (largest + 1) / 2) - int(bar_->height));
            const auto draw = [&](const HudSurface* portrait, std::string text, int score, int left, int top, DBYTE team)
            {
                if (!scrunch)
                {
                    canvas_.DrawSurfaceToSurface(&surface_, bar_, nullptr, left, top);
                    canvas_.DrawSurfaceToSurface(&surface_, portrait, nullptr, left, top);
                }
                const int text_y = top + int((bar_->height - font_.height) / 2);
                const int text_x = left + int(portrait->width);
                if (team)
                {
                    const HudRect label{text_x, text_y, std::min(text_x + 145, left + int(bar_->width) - 56),
                                        text_y + int(font_.height)};
                    if (!team_label_ || label.right <= label.left || !team_label_(team, surface_, font_, label))
                        font_.draw(surface_, text, text_x, text_y, 0);
                }
                else
                    draw_player(std::move(text), text_x, text_y);
                font_.draw(surface_, std::to_string(score), left + int(bar_->width) - 20, text_y, 1);
            };
            for (unsigned team = first; team < first + columns; ++team)
            {
                const int left = int((team - first) * (bar_->width + 32));
                draw(voice_, team_names_[team], totals[team], left, int(residual), DBYTE(team + 1));
                ++displayed;
                unsigned count = 0;
                for (const auto& p : team_order_[team])
                {
                    if (++count >= maximum)
                        break; // Source nCount < nMax leaves the title row.
                    draw(portraits_[p.character], "(-) " + p.name, p.frags, left,
                         int(player_start + (count - 1) * row_step), 0);
                    ++displayed;
                }
            }
            for (std::size_t i = 0; i < surface_.rgba.size(); i += 4)
                if (surface_.rgba[i] == 255 && surface_.rgba[i + 1] == 0 && surface_.rgba[i + 2] == 255)
                    surface_.rgba[i + 3] = 0;
        }
        teams_ = true;
        team_column_ = column;
        team_totals_ = totals;
        previous_.assign(players.begin(), players.end());
        width_ = width;
        height_ = height;
        ++revision;
        return true;
    }
} // namespace OpenBlood2
