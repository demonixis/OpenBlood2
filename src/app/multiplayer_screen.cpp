#include "app/multiplayer_screen.h"
#include "engine/localization.h"
#include "engine/world.h"
#include "game/localized_font.h"
#include "game/save_slots.h"
#include <SDL3/SDL_timer.h>
#include <cstdio>
#include <imgui.h>
namespace OpenBlood2
{
    const char* multiplayer_label(const Localization* locale, const LocalizedFont* font, std::string_view key,
                                  const char* english)
    {
        if (locale && font)
        {
            const auto value = locale->text(key);
            if (!value.empty() && font->supports(value))
                return value.data();
        }
        return english;
    }

    bool MultiplayerScreen::open(const Resources& resources, const Config& config, bool in_world, std::string& error)
    {
        close();
        error_.clear();
        error_key_ = {};
        discovery_error_.clear();
        map_titles_.clear();
        if (!setup.load(resources, config, error))
            return false;
        for (const auto& path : setup.maps)
        {
            std::vector<std::uint8_t> header;
            std::string info, ignored;
            if (resources.read(path, header, ignored, 16 + 65536) && read_world_info(header, info, ignored))
                map_titles_.emplace(path, world_title(info, path));
        }
        replaces_solo_ = in_world;
        active = true;
        refresh_ = -10;
        browsing_ = browser_.open(false, discovery_error_);
        return true;
    }
    void MultiplayerScreen::close()
    {
        active = false;
        browsing_ = false;
        browser_.close();
        pending.reset();
    }
    void MultiplayerScreen::start(bool host)
    {
        Options options;
        if (setup.request(host, options, error_, &error_key_))
            pending = std::move(options);
    }
    void MultiplayerScreen::draw(const Localization* locale, const LocalizedFont* localized_font, ImFont* localized_face)
    {
        if (!active)
            return;
        const double now = double(SDL_GetTicksNS()) / 1e9;
        if (browsing_)
        {
            if (now - refresh_ >= 2)
            {
                browser_.query(now, discovery_error_);
                refresh_ = now;
            }
            std::string receive_error;
            if (!browser_.pump(now, nullptr, receive_error))
            {
                discovery_error_ = receive_error;
                browsing_ = false;
                browser_.close();
            }
        }
        // Esc/B cancel an active field or an open combo first; only then close
        // the screen. Sampled before the widgets, which clear it on Escape.
        const bool editing = ImGui::IsAnyItemActive() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
        const bool localized = locale && localized_face && locale->language() != Language::English;
        ImFont* face = localized ? localized_face : font;
        if (face)
            ImGui::PushFont(face, 0);
        const auto tr = [&](std::string_view key, const char* english)
        { return multiplayer_label(locale, localized ? localized_font : nullptr, key, english); };
        const auto map_label = [&](const std::string& path)
        {
            const auto title = map_titles_.find(path);
            if (title == map_titles_.end())
                return path.c_str(); // A peer can advertise a map we do not have.
            if (localized && localized_font)
            {
                const auto value = locale->world(path);
                if (!value.empty() && value.find("##") == value.npos && localized_font->supports(value))
                    return value.data();
            }
            return title->second.c_str();
        };
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, {.5f, .5f});
        ImGui::SetNextWindowSize({std::min(720.f, viewport->Size.x - 24), std::min(720.f, viewport->Size.y - 24)},
                                 ImGuiCond_Always);
        ImGui::Begin(tr("network.title", "Multiplayer"), &active, ImGuiWindowFlags_NoCollapse);
        ImGui::InputText(tr("network.name", "Name"), setup.name.data(), setup.name.size());
        ImGui::Combo(tr("network.character", "Character"), &setup.character, "Caleb\0Ophelia\0Ishmael\0Gabriella\0");
        const char* teams[]{tr("network.automatic", "Automatic"), tr("network.blue", "Blue"), tr("network.red", "Red")};
        ImGui::Combo(tr("network.preferred_team", "Preferred team"), &setup.team, teams, IM_ARRAYSIZE(teams));
        if (replaces_solo_)
            ImGui::TextWrapped("%s", tr("network.replaces_solo", "Starting a multiplayer game ends the current single-player session."));
        if (ImGui::BeginTabBar("Session"))
        {
            if (ImGui::BeginTabItem(tr("network.host", "Host")))
            {
                const char* modes[]{tr("network.bloodbath", "Bloodbath"), tr("network.team", "Team"), tr("network.ctf", "CTF")};
                if (ImGui::Combo(tr("network.mode", "Mode"), &setup.mode, modes, IM_ARRAYSIZE(modes)))
                {
                    setup.map = 0;
                    setup.rotation.clear();
                }
                const auto maps = setup.mode_maps();
                if (ImGui::BeginCombo(tr("network.map", "Map"),
                                      maps.empty() ? tr("network.no_map", "No map") : map_label(maps[setup.map])))
                {
                    for (std::size_t i = 0; i < maps.size(); ++i)
                    {
                        ImGui::PushID(maps[i].c_str());
                        if (ImGui::Selectable(map_label(maps[i]), std::size_t(setup.map) == i))
                            setup.map = int(i);
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                ImGui::InputInt(tr("network.udp_port", "UDP port"), &setup.port);
                const char* ends[]{tr("network.frags_score", "Frags / score"), tr("network.time", "Time"),
                                   tr("network.first_limit", "First limit reached"), tr("network.no_limit", "No limit")};
                ImGui::Combo(tr("network.match_end", "Match end"), &setup.end, ends, IM_ARRAYSIZE(ends));
                if (setup.end == 0 || setup.end == 2)
                    ImGui::InputInt(tr("network.score_limit", "Score limit"), &setup.frags);
                if (setup.end == 1 || setup.end == 2)
                    ImGui::InputInt(tr("network.time_limit", "Time limit (minutes)"), &setup.minutes);
                if (setup.mode)
                {
                    ImGui::Checkbox(tr("network.friendly_fire", "Friendly fire"), &setup.friendly_fire);
                    ImGui::Checkbox(tr("network.negative_team_frags", "Negative team frags"), &setup.negative_team_frags);
                }
                if (setup.mode == 2)
                {
                    ImGui::InputInt(tr("network.points_per_capture", "Points per capture"), &setup.flag_value);
                    ImGui::Checkbox(tr("network.only_flag_scores", "Only flag captures score"), &setup.only_flag_scores);
                    ImGui::Checkbox(tr("network.scale_team_size", "Scale score by team size"), &setup.use_team_size);
                }
                if (ImGui::TreeNode(tr("network.rotation", "Map rotation")))
                {
                    ImGui::TextWrapped(
                        "%s", tr("network.rotation_help", "The starting map plays first. It repeats in a loop with the list below."));
                    ImGui::BeginDisabled(maps.empty() || setup.rotation.size() >= 49);
                    if (ImGui::Button(tr("network.add_map", "Add selected map")))
                        setup.rotation.push_back(maps[setup.map]);
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    if (ImGui::Button(tr("network.clear", "Clear")))
                        setup.rotation.clear();
                    for (std::size_t i = 0; i < setup.rotation.size(); ++i)
                        ImGui::Text("%zu. %s", i + 1, map_label(setup.rotation[i]));
                    ImGui::TreePop();
                }
                ImGui::TextWrapped("%s", tr("network.lan_help", "The game is visible on the local network. Leaving as host ends the game."));
                if (ImGui::Button(tr("network.start_game", "Start game")))
                    start(true);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(tr("network.join", "Join")))
            {
                ImGui::InputText(tr("network.address", "Address or host:port"), setup.address.data(), setup.address.size());
                if (ImGui::Button(tr("network.connect", "Connect")))
                    start(false);
                ImGui::SeparatorText(tr("network.lan_games", "Local network games"));
                if (ImGui::Button(tr("network.refresh", "Refresh")))
                {
                    if (!browsing_)
                        browsing_ = browser_.open(false, discovery_error_);
                    refresh_ = -10;
                }
                if (!discovery_error_.empty())
                {
                    const auto detail = localized && localized_font
                                            ? locale->format("network.discovery_failed", discovery_error_, {discovery_error_})
                                            : discovery_error_;
                    ImGui::TextWrapped("%s", localized_font && localized_font->supports(detail)
                                                 ? detail.c_str() : discovery_error_.c_str());
                }
                if (browser_.servers().empty())
                    ImGui::TextWrapped("%s", tr("network.searching", "Searching. If a game does not appear, connect to its address."));
                for (const auto& s : browser_.servers())
                {
                    const auto endpoint = s.address.host + ":" + std::to_string(s.address.port);
                    ImGui::PushID(endpoint.c_str());
                    const bool compatible = s.offer.protocol == Net::protocol_version;
                    ImGui::TextWrapped("%s - %s - %u/%u", s.offer.name.c_str(), endpoint.c_str(),
                                       unsigned(s.offer.players), unsigned(s.offer.capacity));
                    ImGui::TextWrapped("%s - %s",
                                       s.offer.mode == 1   ? tr("network.bloodbath", "Bloodbath")
                                       : s.offer.mode == 2 ? tr("network.ctf", "CTF")
                                                           : tr("network.team", "Team"),
                                       map_label(s.offer.world));
                    if (!compatible)
                        ImGui::TextUnformatted(tr("network.incompatible", "Incompatible game version"));
                    else if (s.offer.players >= s.offer.capacity)
                        ImGui::TextUnformatted(tr("network.full", "Game full"));
                    ImGui::BeginDisabled(!compatible || s.offer.players >= s.offer.capacity);
                    if (ImGui::Button(tr("network.join_game", "Join this game")))
                    {
                        std::snprintf(setup.address.data(), setup.address.size(), "%s", endpoint.c_str());
                        start(false);
                    }
                    ImGui::EndDisabled();
                    ImGui::PopID();
                    ImGui::Separator();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (!error_.empty())
        {
            if (localized && localized_font && error_key_ == "network.invalid_host")
            {
                const auto detail = locale->format(error_key_, error_, {std::to_string(Net::discovery_port)});
                ImGui::TextWrapped("%s", localized_font->supports(detail) ? detail.c_str() : error_.c_str());
            }
            else
                ImGui::TextWrapped("%s", tr(error_key_, error_.c_str()));
        }
        if (ImGui::Button(tr("network.back", "Back")) ||
            (!editing &&
             (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))))
            active = false;
        ImGui::End();
        if (face)
            ImGui::PopFont();
        if (!active)
        {
            browser_.close();
            browsing_ = false;
        }
    }
} // namespace OpenBlood2
