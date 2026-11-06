#include "app/gi_overlay.h"
#include "engine/gi_world.h"
#include "game/server_adapter.h"
#include <imgui.h>
#include <cmath>

namespace OpenBlood2
{
    namespace
    {
        constexpr std::array<float, 4> empty_color{.25f, .95f, .35f, 1}, solid_color{.95f, .25f, .2f, 1},
                                       unknown_color{1, 0, 1, 1};
        const char* contents_name(PointContents contents)
        {
            return contents == PointContents::Empty   ? "empty"
                   : contents == PointContents::Solid ? "solid"
                                                      : "unknown";
        }
        ImVec4 status_color(GiStatus status)
        {
            switch (status)
            {
            case GiStatus::Ready: return {.45f, .9f, .45f, 1};
            case GiStatus::Stale: return {.95f, .8f, .3f, 1};
            case GiStatus::Missing: return {.6f, .6f, .6f, 1};
            default: return {.95f, .4f, .35f, 1};
            }
        }
        void unavailable(const char* lot, const char* what)
        {
            ImGui::TextDisabled("Not available yet: %s arrives with lot %s.", what, lot);
        }
    } // namespace

    void GiTools::cell_gizmo(const WorldModel& primary, DVector p, std::vector<LineVertex>& gizmos)
    {
        const auto contents = point_contents(primary, p);
        const auto& color = contents == PointContents::Empty   ? empty_color
                            : contents == PointContents::Solid ? solid_color
                                                               : unknown_color;
        const float h = cell_size / 2;
        const auto center = [&](double v) { return float(std::floor(v / cell_size) * cell_size + h); };
        xr::Pose pose;
        pose.position = {center(p.x), center(p.y), center(p.z)};
        add_box(gizmos, pose, {h, h, h}, color);
    }

    void GiTools::forget()
    {
        stale_ = true;
    }

    void GiTools::refresh(const GameSession& game, const Config& config)
    {
        status_ = {};
        world_revision_ = game.world_revision;
        stale_ = false;
        status_.roots = gi_search_roots(config.gi_dir);
        if (game.world_path.empty())
            return;
        status_.world_path = game.world_path;
        GiExpectation expected;
        if (!gi_expectation(game.resources, game.world_path, expected, status_.error))
            return;
        gi_file_name(game.world_path, expected.world, status_.file_name);
        status_.expected = expected;
        if (!find_gi_file(status_.roots, game.world_path, expected, status_.lookup, status_.error))
            status_.lookup = {};
    }

    std::string GiTools::summary() const
    {
        if (status_.world_path.empty())
            return "no world";
        if (!status_.error.empty())
            return "error";
        if (status_.lookup.chosen >= 0)
            return "ready file, not applied (GI rendering: lot G8)";
        for (const auto& candidate : status_.lookup.candidates)
            if (candidate.status != GiStatus::Missing)
            {
                std::string text = std::string(gi_status_name(candidate.status)) + " file";
                if (candidate.status == GiStatus::Stale)
                    text += " (" + gi_stale_text(candidate.stale) + ")";
                return text;
            }
        return "no baked file";
    }

    void GiTools::status_panel(const GameSession& game, const Config& config)
    {
        if (ImGui::Button("Refresh"))
            refresh(game, config);
        ImGui::SameLine();
        ImGui::TextUnformatted(("GI: " + summary()).c_str());
        ImGui::TextDisabled("GI rendering is disabled until lot G8: the original lightmaps are in use.");
        ImGui::Separator();
        if (status_.world_path.empty())
        {
            ImGui::TextUnformatted("No world loaded.");
            return;
        }
        ImGui::Text("World %s", status_.world_path.c_str());
        if (!status_.error.empty())
        {
            ImGui::TextColored(status_color(GiStatus::Failed), "%s", status_.error.c_str());
            return;
        }
        if (status_.expected)
        {
            const auto& e = *status_.expected;
            ImGui::Text("DAT %s  albedo %s", digest_text(e.world).substr(0, 12).c_str(),
                        digest_text(e.albedo).substr(0, 12).c_str());
            ImGui::Text("Edition %s  algorithm %u", gi_edition_name(e.edition), e.algorithm);
            ImGui::Text("File %s", status_.file_name.c_str());
        }
        if (ImGui::BeginTable("gi_roots", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                 ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Source");
            ImGui::TableSetupColumn("Status");
            ImGui::TableSetupColumn("Path / detail");
            ImGui::TableHeadersRow();
            for (std::size_t i = 0; i < status_.lookup.candidates.size(); ++i)
            {
                const auto& c = status_.lookup.candidates[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(c.source.c_str());
                ImGui::TableNextColumn();
                std::string text = gi_status_name(c.status);
                if (int(i) == status_.lookup.chosen)
                    text += " (chosen)";
                ImGui::TextColored(status_color(c.status), "%s", text.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(c.path.c_str());
                if (c.status == GiStatus::Stale)
                    ImGui::TextDisabled("stale: %s", gi_stale_text(c.stale).c_str());
                else if (!c.detail.empty())
                    ImGui::TextDisabled("%s", c.detail.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::Separator();
        const auto& camera = game.client_shell.view.position;
        ImGui::Text("Camera %.0f %.0f %.0f", camera.x, camera.y, camera.z);
        if (!game.world.models.empty())
            ImGui::Text("Root BSP contents: %s", contents_name(point_contents(game.world.models[0], camera)));
        ImGui::Checkbox("Show camera cell", &camera_cell);
        ImGui::SameLine();
        ImGui::TextDisabled("(%g-unit lattice: green empty, red solid, magenta unknown)", cell_size);
    }

    bool GiTools::draw(MenuInput& input, GameSession& game, const Config& config, std::vector<LineVertex>& gizmos)
    {
        gizmos.clear();
        if (input.gi == GiOverlay::Closed)
        {
            stale_ = true; // Opening again re-reads the folders.
            return false;
        }
        if (stale_ || world_revision_ != game.world_revision || status_.world_path != game.world_path)
            refresh(game, config);
        if (camera_cell && game.client_shell.m_bInWorld && !game.world.models.empty())
            cell_gizmo(game.world.models[0], game.client_shell.view.position, gizmos);
        if (input.gi == GiOverlay::Pinned)
        {
            const auto& io = ImGui::GetIO();
            ImGui::SetNextWindowPos({io.DisplaySize.x - 12, 12}, ImGuiCond_Always, {1, 0});
            ImGui::SetNextWindowBgAlpha(.6f);
            if (ImGui::Begin("##gi_pinned", nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings))
                ImGui::Text("GI tools (pinned, F8): %s", summary().c_str());
            ImGui::End();
            return false;
        }
        bool open = true;
        ImGui::SetNextWindowSize({620, 460}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({40, 40}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("GI tools (F8)", &open, ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::Checkbox("Pin (keep markers and status during play)", &input.gi_pin);
            if (ImGui::BeginTabBar("gi_panels"))
            {
                if (ImGui::BeginTabItem("Status"))
                {
                    status_panel(game, config);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Runtime"))
                {
                    unavailable("G8", "the GI switches, intensities and views");
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Light probes"))
                {
                    unavailable("G4", "probe placement");
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Reflection probes"))
                {
                    unavailable("G4", "reflection probe placement");
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Bake"))
                {
                    unavailable("G10", "baking from the game");
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
        if (!open)
        {
            input.close_gi();
            return true;
        }
        return false;
    }
} // namespace OpenBlood2
