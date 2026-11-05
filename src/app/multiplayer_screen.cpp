#include "app/multiplayer_screen.h"
#include <SDL3/SDL_timer.h>
#include <cstdio>
#include <imgui.h>
namespace OpenBlood2
{
    bool MultiplayerScreen::open(const Resources& resources, const Config& config, bool in_world, std::string& error)
    {
        close();
        error_.clear();
        discovery_error_.clear();
        if (!setup.load(resources, config, error))
            return false;
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
        if (setup.request(host, options, error_))
            pending = std::move(options);
    }
    void MultiplayerScreen::draw()
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
        if (font)
            ImGui::PushFont(font, 0);
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, {.5f, .5f});
        ImGui::SetNextWindowSize({std::min(720.f, viewport->Size.x - 24), std::min(720.f, viewport->Size.y - 24)},
                                 ImGuiCond_Always);
        ImGui::Begin("Multiplayer", &active, ImGuiWindowFlags_NoCollapse);
        ImGui::InputText("Name", setup.name.data(), setup.name.size());
        ImGui::Combo("Character", &setup.character, "Caleb\0Ophelia\0Ishmael\0Gabriella\0");
        ImGui::Combo("Preferred team", &setup.team, "Automatic\0Blue\0Red\0");
        if (replaces_solo_)
            ImGui::TextWrapped("Starting a multiplayer game ends the current single-player session.");
        if (ImGui::BeginTabBar("Session"))
        {
            if (ImGui::BeginTabItem("Host"))
            {
                if (ImGui::Combo("Mode", &setup.mode, "Bloodbath\0Team\0CTF\0"))
                {
                    setup.map = 0;
                    setup.rotation.clear();
                }
                const auto maps = setup.mode_maps();
                if (ImGui::BeginCombo("Map", maps.empty() ? "No map" : maps[setup.map].c_str()))
                {
                    for (std::size_t i = 0; i < maps.size(); ++i)
                        if (ImGui::Selectable(maps[i].c_str(), std::size_t(setup.map) == i))
                            setup.map = int(i);
                    ImGui::EndCombo();
                }
                ImGui::InputInt("UDP port", &setup.port);
                ImGui::Combo("Match end", &setup.end, "Frags / score\0Time\0First limit reached\0No limit\0");
                if (setup.end == 0 || setup.end == 2)
                    ImGui::InputInt("Score limit", &setup.frags);
                if (setup.end == 1 || setup.end == 2)
                    ImGui::InputInt("Time limit (minutes)", &setup.minutes);
                if (setup.mode)
                {
                    ImGui::Checkbox("Friendly fire", &setup.friendly_fire);
                    ImGui::Checkbox("Negative team frags", &setup.negative_team_frags);
                }
                if (setup.mode == 2)
                {
                    ImGui::InputInt("Points per capture", &setup.flag_value);
                    ImGui::Checkbox("Only flag captures score", &setup.only_flag_scores);
                    ImGui::Checkbox("Scale score by team size", &setup.use_team_size);
                }
                if (ImGui::TreeNode("Map rotation"))
                {
                    ImGui::TextWrapped(
                        "The starting map plays first. It repeats in a loop with the list below.");
                    ImGui::BeginDisabled(maps.empty() || setup.rotation.size() >= 49);
                    if (ImGui::Button("Add selected map"))
                        setup.rotation.push_back(maps[setup.map]);
                    ImGui::EndDisabled();
                    ImGui::SameLine();
                    if (ImGui::Button("Clear"))
                        setup.rotation.clear();
                    for (std::size_t i = 0; i < setup.rotation.size(); ++i)
                        ImGui::Text("%zu. %s", i + 1, setup.rotation[i].c_str());
                    ImGui::TreePop();
                }
                ImGui::TextWrapped("The game is visible on the local network. Leaving as host ends the game.");
                if (ImGui::Button("Start game"))
                    start(true);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Join"))
            {
                ImGui::InputText("Address or host:port", setup.address.data(), setup.address.size());
                if (ImGui::Button("Connect"))
                    start(false);
                ImGui::SeparatorText("Local network games");
                if (ImGui::Button("Refresh"))
                {
                    if (!browsing_)
                        browsing_ = browser_.open(false, discovery_error_);
                    refresh_ = -10;
                }
                if (!discovery_error_.empty())
                    ImGui::TextWrapped("%s", discovery_error_.c_str());
                if (browser_.servers().empty())
                    ImGui::TextWrapped("Searching. If a game does not appear, connect to its address.");
                for (const auto& s : browser_.servers())
                {
                    const auto endpoint = s.address.host + ":" + std::to_string(s.address.port);
                    ImGui::PushID(endpoint.c_str());
                    const bool compatible = s.offer.protocol == Net::protocol_version;
                    ImGui::TextWrapped("%s - %s - %u/%u", s.offer.name.c_str(), endpoint.c_str(),
                                       unsigned(s.offer.players), unsigned(s.offer.capacity));
                    ImGui::TextWrapped("%s - %s",
                                       s.offer.mode == 1   ? "Bloodbath"
                                       : s.offer.mode == 2 ? "CTF"
                                                           : "Team",
                                       s.offer.world.c_str());
                    if (!compatible)
                        ImGui::TextUnformatted("Incompatible game version");
                    else if (s.offer.players >= s.offer.capacity)
                        ImGui::TextUnformatted("Game full");
                    ImGui::BeginDisabled(!compatible || s.offer.players >= s.offer.capacity);
                    if (ImGui::Button("Join this game"))
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
            ImGui::TextWrapped("%s", error_.c_str());
        if (ImGui::Button("Back") ||
            (!editing &&
             (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))))
            active = false;
        ImGui::End();
        if (font)
            ImGui::PopFont();
        if (!active)
        {
            browser_.close();
            browsing_ = false;
        }
    }
} // namespace OpenBlood2
