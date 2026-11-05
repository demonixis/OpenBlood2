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
        if (width == width_ && height == height_ &&
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
            std::string text = "(-) " + p.name; // Original unknown-ping branch; measured pings remain separate.
            // Original DrawClip draws the whole string if it fits, then also
            // draws the shortened string; preserve that harmless repeated draw.
            if (font_.text_width(text) < 145)
                font_.draw(surface_, text, int(portrait->width), text_y, 0);
            while (text.size() > 1)
            {
                text.pop_back();
                if (font_.text_width(text) < 145)
                {
                    font_.draw(surface_, text, int(portrait->width), text_y, 0);
                    break;
                }
            }
            font_.draw(surface_, std::to_string(p.frags), int(bar_->width) - 20, text_y, 1);
        }
        for (std::size_t i = 0; i < surface_.rgba.size(); i += 4)
            if (surface_.rgba[i] == 255 && surface_.rgba[i + 1] == 0 && surface_.rgba[i + 2] == 255)
                surface_.rgba[i + 3] = 0;
        previous_.assign(players.begin(), players.end());
        width_ = width;
        height_ = height;
        ++revision;
        return true;
    }
} // namespace OpenBlood2
