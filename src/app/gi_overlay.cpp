#include "app/gi_overlay.h"
#include "app/model_lighting.h"
#include "engine/gi_world.h"
#include "game/server_adapter.h"
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace OpenBlood2
{
    namespace
    {
        using Color = std::array<float, 4>;
        constexpr Color empty_color{.25f, .95f, .35f, 1}, solid_color{.95f, .25f, .2f, 1}, unknown_color{1, 0, 1, 1};
        constexpr Color probe_color{.3f, .95f, .4f, 1}, relocated_color{1, .9f, .25f, 1}, rejected_color{1, .2f, 1, 1};
        constexpr Color influence_color{1, .6f, .15f, 1}, projection_color{.3f, .55f, 1, 1}, capture_color{1, 1, 1, 1},
                        selected_color{1, 1, .2f, 1}, include_color{.2f, .9f, .3f, 1}, exclude_color{.95f, .2f, .2f, 1};
        constexpr float probe_marker = 8, capture_marker = 24, volume_half = 256;
        constexpr std::uint32_t resolutions[] = {16, 32, 64, 128, 256};

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
        ImVec4 placement_color(GiPlacementStatus status)
        {
            switch (status)
            {
            case GiPlacementStatus::Ready: return {.45f, .9f, .45f, 1};
            case GiPlacementStatus::Missing: return {.6f, .6f, .6f, 1};
            case GiPlacementStatus::Foreign: return {.95f, .8f, .3f, 1};
            default: return {.95f, .4f, .35f, 1};
            }
        }
        constexpr ImVec4 error_color{.95f, .4f, .35f, 1};
        void unavailable(const char* lot, const char* what)
        {
            ImGui::TextDisabled("Not available yet: %s arrives with lot %s.", what, lot);
        }
        void cross(std::vector<LineVertex>& out, DVector p, float r, const Color& color)
        {
            add_line(out, {p.x - r, p.y, p.z}, {p.x + r, p.y, p.z}, color);
            add_line(out, {p.x, p.y - r, p.z}, {p.x, p.y + r, p.z}, color);
            add_line(out, {p.x, p.y, p.z - r}, {p.x, p.y, p.z + r}, color);
        }
        void box(std::vector<LineVertex>& out, const GiBox& b, const Color& color)
        {
            xr::Pose pose;
            pose.position = {(b.min.x + b.max.x) / 2, (b.min.y + b.max.y) / 2, (b.min.z + b.max.z) / 2};
            add_box(out, pose, {(b.max.x - b.min.x) / 2, (b.max.y - b.min.y) / 2, (b.max.z - b.min.z) / 2}, color);
        }
        double distance2(DVector a, DVector b)
        {
            const double x = double(a.x) - b.x, y = double(a.y) - b.y, z = double(a.z) - b.z;
            return x * x + y * y + z * z;
        }
        float clamp_coordinate(double v)
        {
            return float(std::clamp(v, -double(gi_placement_limits::max_coordinate),
                                    double(gi_placement_limits::max_coordinate)));
        }
        std::string box_size(const GiBox& b)
        {
            char text[64];
            std::snprintf(text, sizeof text, "%.0fx%.0fx%.0f", b.max.x - b.min.x, b.max.y - b.min.y,
                          b.max.z - b.min.z);
            return text;
        }
        // Drag widgets for a box; true once an edit is finished (not on every dragged frame).
        bool box_editor(const char* label, GiBox& b, bool& changed)
        {
            ImGui::PushID(label);
            ImGui::TextUnformatted(label);
            changed |= ImGui::DragFloat3("min", &b.min.x, 1, -gi_placement_limits::max_coordinate,
                                         gi_placement_limits::max_coordinate, "%.1f");
            bool done = ImGui::IsItemDeactivatedAfterEdit();
            changed |= ImGui::DragFloat3("max", &b.max.x, 1, -gi_placement_limits::max_coordinate,
                                         gi_placement_limits::max_coordinate, "%.1f");
            done |= ImGui::IsItemDeactivatedAfterEdit();
            ImGui::PopID();
            return done;
        }
        double elapsed_ms(std::chrono::steady_clock::time_point start)
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
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
        if (!game.world_path.empty())
        {
            status_.world_path = game.world_path;
            GiExpectation expected;
            if (gi_expectation(game.resources, game.world_path, expected, status_.error))
            {
                gi_file_name(game.world_path, expected.world, status_.file_name);
                status_.expected = expected;
                if (!find_gi_file(status_.roots, game.world_path, expected, status_.lookup, status_.error))
                    status_.lookup = {};
            }
        }
        if (placement_world_ != game.world_path || placement_revision_ != game.world_revision)
            load_placement(game, config);
    }

    void GiTools::load_placement(const GameSession& game, const Config& config)
    {
        // Unsaved edits survive a reload of the same DAT (the scene is rebuilt either way).
        const bool keep = placement_.edit && placement_.dirty && placement_world_ == game.world_path &&
                          status_.expected && placement_digest_ == status_.expected->world;
        placement_world_ = game.world_path;
        placement_revision_ = game.world_revision;
        scene_ready_ = false;
        selected_reflection = keep ? selected_reflection : -1;
        if (!keep)
            placement_ = {};
        if (game.world_path.empty() || game.world.models.empty())
        {
            placement_ = {};
            placement_.detail = "No world loaded.";
            return;
        }
        if (!status_.expected)
        {
            placement_ = {};
            placement_.status = GiPlacementStatus::Failed;
            placement_.detail = status_.error.empty() ? "The world digest is unavailable." : status_.error;
            return;
        }
        std::string error;
        if (!scene_.build(game.world, game.collision, error))
        {
            placement_ = {};
            placement_.status = GiPlacementStatus::Failed;
            placement_.detail = "Placement queries unavailable: " + error;
            return;
        }
        scene_ready_ = true;
        placement_digest_ = status_.expected->world;
        if (keep)
        {
            place_light_probes();
            return;
        }
        if (!gi_placement_directory(config.gi_placement_dir, placement_.directory, error))
        {
            placement_.status = GiPlacementStatus::Failed;
            placement_.detail = error;
            return;
        }
        read_placement_file();
    }

    void GiTools::read_placement_file()
    {
        auto& p = placement_;
        p.edit.reset();
        p.dirty = false;
        p.detail.clear();
        p.error.clear();
        p.lattice = {};
        p.stats = {};
        p.rejected.clear();
        selected_reflection = -1;
        if (!scene_ready_ || p.directory.empty())
            return;
        GiPlacement loaded;
        p.status = load_gi_placement(p.directory, placement_world_, placement_digest_, loaded, p.detail);
        if (p.status == GiPlacementStatus::Ready)
            p.edit = std::move(loaded);
        else if (p.status == GiPlacementStatus::Missing)
            p.edit = default_gi_placement(placement_world_, placement_digest_);
        if (p.edit)
            place_light_probes();
    }

    bool GiTools::place_light_probes()
    {
        auto& p = placement_;
        p.lattice = {};
        p.stats = {};
        p.rejected.clear();
        p.error.clear();
        if (!p.edit || !scene_ready_)
        {
            p.error = "No placement to resolve.";
            return false;
        }
        const auto start = std::chrono::steady_clock::now();
        const bool ok = resolve_light_probes(scene_, *p.edit, p.lattice, p.stats, &p.rejected, p.error);
        p.resolve_ms = elapsed_ms(start);
        if (!ok)
        {
            p.lattice = {};
            p.stats = {};
            p.rejected.clear();
        }
        return ok;
    }

    bool GiTools::place_reflection_probes()
    {
        auto& p = placement_;
        if (!p.edit || !scene_ready_ || !p.error.empty())
        {
            p.detail = "Resolve the light probes first: reflection rooms are fitted around them.";
            return false;
        }
        auto updated = *p.edit;
        auto stats = p.stats;
        std::string error;
        if (!OpenBlood2::place_reflection_probes(scene_, p.lattice, updated, stats, error))
        {
            p.detail = error;
            return false;
        }
        p.edit = std::move(updated);
        p.stats = stats;
        p.dirty = true;
        p.detail.clear();
        selected_reflection = -1;
        return true;
    }

    namespace
    {
        // Manual probes stay before automatic ones; returns the new index of `index`.
        std::size_t make_manual(std::vector<GiReflectionProbe>& probes, std::size_t index)
        {
            if (!probes[index].automatic)
                return index;
            auto probe = probes[index];
            probe.automatic = false;
            probes.erase(probes.begin() + std::ptrdiff_t(index));
            const auto at = std::find_if(probes.begin(), probes.end(), [](const auto& r) { return r.automatic; });
            const auto position = std::size_t(at - probes.begin());
            probes.insert(at, probe);
            return position;
        }
    } // namespace

    bool GiTools::add_reflection(DVector capture)
    {
        auto& p = placement_;
        if (!p.edit || !scene_ready_)
            return false;
        if (p.edit->reflections.size() >= p.edit->reflection_cap)
        {
            p.detail = "The reflection probe cap (" + std::to_string(p.edit->reflection_cap) + ") is reached.";
            return false;
        }
        GiReflectionProbe probe;
        std::string error;
        if (!fit_reflection_probe(scene_, capture, probe, error))
        {
            p.detail = error;
            return false;
        }
        auto& probes = p.edit->reflections;
        probe.automatic = true; // Inserted with the manual probes below.
        probes.push_back(probe);
        selected_reflection = int(make_manual(probes, probes.size() - 1));
        p.dirty = true;
        p.detail.clear();
        return true;
    }

    bool GiTools::move_reflection(std::size_t index, DVector capture)
    {
        auto& p = placement_;
        if (!p.edit || !scene_ready_ || index >= p.edit->reflections.size())
            return false;
        GiReflectionProbe fitted;
        std::string error;
        if (!fit_reflection_probe(scene_, capture, fitted, error))
        {
            p.detail = error;
            return false;
        }
        auto& probes = p.edit->reflections;
        const auto& old = probes[index];
        fitted.priority = old.priority;
        fitted.resolution = old.resolution;
        fitted.box_projection = old.box_projection;
        fitted.blend_distance = old.blend_distance;
        fitted.automatic = true;
        probes[index] = fitted;
        selected_reflection = int(make_manual(probes, index));
        p.dirty = true;
        p.detail.clear();
        return true;
    }

    void GiTools::delete_reflection(std::size_t index)
    {
        auto& p = placement_;
        if (!p.edit || index >= p.edit->reflections.size())
            return;
        p.edit->reflections.erase(p.edit->reflections.begin() + std::ptrdiff_t(index));
        selected_reflection = -1;
        p.dirty = true;
    }

    void GiTools::add_volume(bool include, DVector center)
    {
        auto& p = placement_;
        if (!p.edit)
            return;
        if (p.edit->volumes.size() >= gi_placement_limits::max_volumes)
        {
            p.detail = "The volume limit (256) is reached.";
            return;
        }
        GiProbeVolume volume;
        volume.include = include;
        volume.box.min = {clamp_coordinate(center.x - volume_half), clamp_coordinate(center.y - volume_half),
                          clamp_coordinate(center.z - volume_half)};
        volume.box.max = {clamp_coordinate(center.x + volume_half), clamp_coordinate(center.y + volume_half),
                          clamp_coordinate(center.z + volume_half)};
        p.edit->volumes.push_back(volume);
        p.dirty = true;
        place_light_probes();
    }

    void GiTools::delete_volume(std::size_t index)
    {
        auto& p = placement_;
        if (!p.edit || index >= p.edit->volumes.size())
            return;
        p.edit->volumes.erase(p.edit->volumes.begin() + std::ptrdiff_t(index));
        p.dirty = true;
        place_light_probes();
    }

    void GiTools::reset_placement()
    {
        auto& p = placement_;
        if (!scene_ready_)
            return;
        p.edit = default_gi_placement(placement_world_, placement_digest_);
        p.dirty = true;
        p.detail.clear();
        selected_reflection = -1;
        place_light_probes();
    }

    bool GiTools::save_placement()
    {
        auto& p = placement_;
        if (!p.edit)
            return false;
        if (!p.error.empty())
        {
            p.detail = "Not saved: the light probes do not resolve (" + p.error + ").";
            return false;
        }
        std::string error, name;
        if (!save_gi_placement(p.directory, *p.edit, error))
        {
            p.detail = "Not saved: " + error;
            return false;
        }
        gi_placement_name(placement_world_, name);
        p.status = GiPlacementStatus::Ready;
        p.dirty = false;
        p.detail = "Saved " + path_text(p.directory / path_from_utf8(name));
        return true;
    }

    void GiTools::revert_placement()
    {
        read_placement_file();
    }

    void GiTools::placement_gizmos(DVector camera, std::vector<LineVertex>& gizmos) const
    {
        const auto& p = placement_;
        if (!p.edit)
            return;
        const double radius2 = double(marker_radius) * marker_radius;
        std::size_t markers = 0;
        if (show_light_probes)
            for (const auto& probe : p.lattice.probes)
            {
                if (markers >= max_probe_markers)
                    break;
                if (distance2(probe.position, camera) > radius2)
                    continue;
                cross(gizmos, probe.position, probe_marker, probe.relocated ? relocated_color : probe_color);
                ++markers;
            }
        if (show_rejected)
            for (const auto& node : p.rejected)
            {
                if (markers >= max_probe_markers)
                    break;
                if (distance2(node.position, camera) > radius2)
                    continue;
                cross(gizmos, node.position, probe_marker / 2, rejected_color);
                ++markers;
            }
        if (show_volumes)
            for (const auto& volume : p.edit->volumes)
                box(gizmos, volume.box, volume.include ? include_color : exclude_color);
        if (show_reflections)
            for (std::size_t i = 0; i < p.edit->reflections.size(); ++i)
            {
                const auto& r = p.edit->reflections[i];
                const bool selected = int(i) == selected_reflection;
                box(gizmos, r.influence, selected ? selected_color : influence_color);
                box(gizmos, r.projection, projection_color);
                cross(gizmos, r.capture, capture_marker, selected ? selected_color : capture_color);
            }
    }

    void GiTools::baked_probe_gizmos(const GiProbeField& field, float k, DVector camera, float radius,
                                     std::vector<LineVertex>& gizmos)
    {
        const auto& lattice = field.lattice();
        const auto& probes = field.probes();
        const double radius2 = double(radius) * radius;
        constexpr int segments = 12;
        constexpr double turn = 6.283185307179586 / segments;
        std::size_t markers = 0;
        for (std::size_t i = 0; i < lattice.probes.size() && markers < max_sh_markers; ++i)
        {
            const auto center = lattice.probes[i].position;
            if (distance2(center, camera) > radius2)
                continue;
            ++markers;
            const std::span<const float, 27> sh(probes.sh.data() + i * gi_probe_limits::values, 27);
            const bool valid = probes.valid[i] != 0;
            for (int plane = 0; plane < 3; ++plane)
                for (int j = 0; j < segments; ++j)
                {
                    auto point = [&](double angle)
                    {
                        const float c = float(std::cos(angle)), s = float(std::sin(angle));
                        return plane == 0 ? DVector{c, s, 0} : plane == 1 ? DVector{c, 0, s} : DVector{0, c, s};
                    };
                    const auto a = point(j * turn), b = point((j + 1) * turn), mid = point((j + .5) * turn);
                    Color color = rejected_color;
                    if (valid)
                    {
                        const auto e = gi_sh_irradiance(sh, mid);
                        color = {std::clamp(k * e.x, .08f, 1.f), std::clamp(k * e.y, .08f, 1.f),
                                 std::clamp(k * e.z, .08f, 1.f), 1};
                    }
                    const auto from = vector_add(center, vector_scale(a, sh_radius)),
                               to = vector_add(center, vector_scale(b, sh_radius));
                    add_line(gizmos, {from.x, from.y, from.z}, {to.x, to.y, to.z}, color);
                }
        }
    }

    std::string GiTools::summary() const
    {
        if (status_.world_path.empty())
            return "no world";
        if (!status_.error.empty())
            return "error";
        if (status_.lookup.chosen >= 0)
        {
            if (!runtime_.status || !runtime_.enabled)
                return "ready file";
            if (!*runtime_.enabled)
                return "ready file, GI off (Options > Display)";
            return runtime_.status->state == GiRuntimeState::Applied
                       ? "ready file, applied"
                       : std::string("ready file, not applied (") + gi_runtime_state_name(runtime_.status->state) + ")";
        }
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

    std::string GiTools::runtime_summary() const
    {
        if (!runtime_.status || !runtime_.enabled)
            return "GI rendering: unavailable here.";
        if (!*runtime_.enabled)
            return "GI rendering: off (Options > Display GLOBAL ILLUMINATION); the original lightmaps are in use.";
        const auto& s = *runtime_.status;
        if (s.state == GiRuntimeState::Applied)
            return "GI rendering: applied from " + s.file + ".";
        if (s.state == GiRuntimeState::Off)
            return "GI rendering: on; the file is looked up at the next scene update.";
        return std::string("GI rendering: ") + gi_runtime_state_name(s.state) +
               (s.detail.empty() ? std::string() : " (" + s.detail + ")") + "; the original lightmaps are in use.";
    }

    void GiTools::runtime_panel()
    {
        if (!runtime_.status || !runtime_.enabled || !runtime_.composition)
        {
            ImGui::TextDisabled("The runtime switches need the game's scene presenter.");
            return;
        }
        const auto& s = *runtime_.status;
        ImGui::Checkbox("Global illumination", runtime_.enabled);
        ImGui::SameLine();
        ImGui::TextDisabled("(Options > Display GLOBAL ILLUMINATION, saved with the preferences)");
        const ImVec4 color = s.state == GiRuntimeState::Applied ? ImVec4{.4f, .9f, .4f, 1}
                             : s.state == GiRuntimeState::Off  ? ImVec4{.7f, .7f, .7f, 1}
                                                                : ImVec4{1, .6f, .3f, 1};
        ImGui::TextColored(color, "%s", runtime_summary().c_str());
        if (runtime_.reload && *runtime_.enabled)
        {
            if (ImGui::Button("Look up again"))
                ++*runtime_.reload;
            ImGui::SameLine();
            ImGui::TextDisabled("(re-reads the search roots, for example after a bake)");
        }
        if (s.state == GiRuntimeState::Applied)
        {
            ImGui::Text("Bake: %s", s.settings.c_str());
            ImGui::Text("File: %zu blocks (%zu texels), %zu corner sets (%zu corners)", s.blocks, s.texels, s.sets,
                        s.corners);
            ImGui::Text("Load: keys %.1f ms, lookup %.1f ms, sections %.1f ms", s.expectation_ms, s.lookup_ms,
                        s.decode_ms);
            if (runtime_.mesh)
                ImGui::Text("Composed: %zu blocks (%zu texels), %zu corner sets (%zu corners); world mesh %.1f ms",
                            runtime_.mesh->gi_blocks, runtime_.mesh->gi_texels, runtime_.mesh->gi_sets,
                            runtime_.mesh->gi_corners, runtime_.mesh_build_ms);
            if (s.probes)
                ImGui::Text("Light probes: %zu of %zu valid, spacing %u", s.valid_probes, s.probes, s.probe_spacing);
            else
                ImGui::TextDisabled("Light probes: none in this file (a bake older than lot G9)");
        }
        ImGui::Separator();
        auto& c = *runtime_.composition;
        ImGui::SliderFloat("Intensity k", &c.k, 0, gi_max_intensity, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Ambient floor f", &c.f, 0, gi_max_intensity, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        if (runtime_.probes)
            ImGui::SliderFloat("Probe intensity p", &runtime_.probes->intensity, 0, gi_max_intensity, "%.2f",
                               ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::Button("Defaults"))
        {
            c.k = gi_default_k, c.f = gi_default_f;
            if (runtime_.probes)
                runtime_.probes->intensity = 1;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("k %.0f, f %.0f (Gate B), p 1; not saved", double(gi_default_k), double(gi_default_f));
        const bool has_probes = runtime_.light && runtime_.light->has_probes;
        if (runtime_.probes)
        {
            ImGui::Checkbox("Light probes (models)", &runtime_.probes->models);
            ImGui::SameLine();
            ImGui::TextDisabled("(ambient max(k p E/2, f AmbientLight) and a pseudo-light; not in Original or "
                                "Direct only)");
            if (runtime_.model_lighting)
            {
                const auto& m = *runtime_.model_lighting;
                ImGui::Text("Models: %zu probe-lit (%zu sampled, %zu reused, %zu without a valid probe), %zu "
                            "pseudo-lights in the top four, %.3f ms",
                            m.probe_lit, m.probe_sampled, m.probe_reused, m.probe_missing, m.probe_sources,
                            m.probe_ms);
            }
        }
        ImGui::TextUnformatted("World source");
        ImGui::SameLine();
        if (ImGui::RadioButton(gi_world_source_name(GiWorldSource::Texels), c.source == GiWorldSource::Texels))
            c.source = GiWorldSource::Texels;
        ImGui::SameLine();
        ImGui::BeginDisabled(!has_probes);
        if (ImGui::RadioButton(gi_world_source_name(GiWorldSource::Probes), c.source == GiWorldSource::Probes))
            c.source = GiWorldSource::Probes;
        ImGui::EndDisabled();
        if (has_probes && runtime_.light->probe_source_ready)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(probes evaluated per texel in %.1f ms)", runtime_.light->probe_source_ms);
        }
        ImGui::TextUnformatted("View");
        for (const auto view : gi_views)
        {
            if (view != GiView::Final && view != GiView::Direct && view != GiView::Lighting)
                ImGui::SameLine();
            if (ImGui::RadioButton(gi_view_name(view), c.view == view))
                c.view = view;
        }
        ImGui::Separator();
        ImGui::TextWrapped("Every change recomposes the lightmap pages and vertex colours from the decoded file (a "
                           "world mesh rebuild, no new bake). Texel validity: green where a sample was valid, red "
                           "where it was filled; polygons the file does not list keep their stored light.");
        ImGui::TextWrapped("Light probes: E evaluated at every texel centre and corner with its face normal, in "
                           "place of the texel gather (an F8 comparison; elements without a valid probe keep their "
                           "stored light at f = 1). Models take the probes whatever the world source.");
        ImGui::TextDisabled("The enhanced renderer's static-light specular reads the total GI luminance until G12.");
    }

    std::string GiTools::placement_summary() const
    {
        const auto& p = placement_;
        if (placement_world_.empty())
            return "placement: no world";
        std::string text = std::string("placement ") + gi_placement_status_name(p.status);
        if (p.dirty)
            text += " (unsaved)";
        if (!p.edit)
            return text + ", not editable";
        if (!p.error.empty())
            return text + ", light probes do not resolve";
        return text + ": " + std::to_string(p.stats.probes) + " light probes at " +
               std::to_string(p.lattice.spacing) + ", " + std::to_string(p.edit->reflections.size()) +
               " reflection probes";
    }

    void GiTools::status_panel(const GameSession& game, const Config& config)
    {
        if (ImGui::Button("Refresh"))
            refresh(game, config);
        ImGui::SameLine();
        ImGui::TextUnformatted(("GI: " + summary()).c_str());
        ImGui::TextDisabled("%s", runtime_summary().c_str());
        ImGui::TextUnformatted(placement_summary().c_str());
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

    namespace
    {
        // The header both placement panels share: file, status, last message and the file buttons.
        // Returns false when there is nothing to edit.
        bool placement_header(GiTools& tools)
        {
            const auto& p = tools.placement();
            const auto directory = p.directory.empty() ? std::string("(no directory)") : path_text(p.directory);
            ImGui::Text("Placement %s", directory.c_str());
            ImGui::TextColored(placement_color(p.status), "%s%s", gi_placement_status_name(p.status),
                               p.dirty ? " (unsaved edits)" : "");
            if (!p.detail.empty())
                ImGui::TextWrapped("%s", p.detail.c_str());
            if (!p.edit)
            {
                if (p.status == GiPlacementStatus::Foreign || p.status == GiPlacementStatus::Failed)
                {
                    ImGui::TextWrapped("The file is kept untouched. Starting from defaults replaces it only when "
                                       "saved.");
                    if (ImGui::Button("Start from defaults"))
                        tools.reset_placement();
                }
                return tools.placement().edit.has_value();
            }
            std::string invalid;
            if (!validate_gi_placement(*p.edit, invalid))
                ImGui::TextColored(error_color, "%s", invalid.c_str());
            ImGui::BeginDisabled(!p.dirty && p.status == GiPlacementStatus::Ready);
            if (ImGui::Button("Save"))
                tools.save_placement();
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!p.dirty);
            if (ImGui::Button("Revert"))
                tools.revert_placement();
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Reset to defaults"))
                tools.reset_placement();
            ImGui::Separator();
            return true;
        }
    } // namespace

    void GiTools::volume_table()
    {
        auto& volumes = placement_.edit->volumes;
        if (volumes.empty())
        {
            ImGui::TextDisabled("No volumes: the lattice covers the whole world.");
            return;
        }
        int erase = -1;
        bool resolve = false;
        for (std::size_t i = 0; i < volumes.size(); ++i)
        {
            ImGui::PushID(int(i));
            auto& v = volumes[i];
            ImGui::TextColored(v.include ? ImVec4{.2f, .9f, .3f, 1} : ImVec4{.95f, .2f, .2f, 1}, "#%zu %s", i,
                               v.include ? "include" : "exclude");
            ImGui::SameLine();
            if (ImGui::SmallButton(v.include ? "Make exclude" : "Make include"))
            {
                v.include = !v.include;
                placement_.dirty = resolve = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete"))
                erase = int(i);
            bool changed = false;
            if (box_editor("box", v.box, changed))
                resolve = true;
            placement_.dirty |= changed;
            ImGui::PopID();
        }
        if (erase >= 0)
            delete_volume(std::size_t(erase));
        else if (resolve)
            place_light_probes();
    }

    void GiTools::light_panel(const GameSession& game)
    {
        if (!placement_header(*this))
            return;
        auto& p = placement_;
        auto& e = *p.edit;
        int spacing = int(e.spacing), budget = int(e.budget);
        float clearance = e.clearance;
        bool resolve = false;
        constexpr auto enter = ImGuiInputTextFlags_EnterReturnsTrue;
        if (ImGui::InputInt("Spacing", &spacing, 8, 64, enter))
        {
            spacing = std::clamp(spacing, int(gi_placement_limits::min_spacing),
                                 int(gi_placement_limits::max_spacing)) / 8 * 8;
            resolve |= std::uint32_t(spacing) != e.spacing;
            e.spacing = std::uint32_t(spacing);
        }
        if (ImGui::InputInt("Budget", &budget, 256, 4096, enter))
        {
            budget = std::clamp(budget, 1, int(gi_placement_limits::max_budget));
            resolve |= std::uint32_t(budget) != e.budget;
            e.budget = std::uint32_t(budget);
        }
        if (ImGui::InputFloat("Clearance", &clearance, 1, 8, "%.1f", enter))
        {
            clearance = std::clamp(clearance, 0.f, gi_placement_limits::max_clearance);
            resolve |= clearance != e.clearance;
            e.clearance = clearance;
        }
        if (resolve)
        {
            p.dirty = true;
            place_light_probes();
        }
        if (ImGui::Button("Place light probes"))
            place_light_probes();
        ImGui::SameLine();
        ImGui::TextDisabled("(the lattice is derived: the file keeps only these settings and volumes)");
        if (!p.error.empty())
            ImGui::TextColored(error_color, "%s", p.error.c_str());
        else
        {
            const auto& s = p.stats;
            ImGui::Text("%llu probes at spacing %u (%llu relocated) in %.1f ms", (unsigned long long)s.probes,
                        p.lattice.spacing, (unsigned long long)s.relocated, p.resolve_ms);
            ImGui::Text("Lattice %u x %u x %u = %llu nodes", p.lattice.dims[0], p.lattice.dims[1], p.lattice.dims[2],
                        (unsigned long long)s.nodes);
            ImGui::Text("Rejected: excluded %llu, solid %llu, unknown %llu, clearance %llu, world model %llu",
                        (unsigned long long)s.excluded, (unsigned long long)s.solid, (unsigned long long)s.unknown,
                        (unsigned long long)s.clearance, (unsigned long long)s.world_model);
            for (const auto& [step, count] : s.over_budget)
                ImGui::TextDisabled("Spacing %u: %llu probes, over the budget", step, (unsigned long long)count);
        }
        ImGui::Separator();
        ImGui::Checkbox("Probes", &show_light_probes);
        ImGui::SameLine();
        ImGui::Checkbox("Rejected nodes", &show_rejected);
        ImGui::SameLine();
        ImGui::Checkbox("Volumes", &show_volumes);
        ImGui::SliderFloat("Marker radius", &marker_radius, 256, 16384, "%.0f");
        ImGui::TextDisabled("Green probe, yellow relocated, magenta rejected (unknown, clearance, world model); at "
                            "most %zu markers.",
                            max_probe_markers);
        ImGui::Separator();
        ImGui::TextUnformatted("Volumes");
        const bool in_world = game.client_shell.m_bInWorld;
        ImGui::BeginDisabled(!in_world);
        if (ImGui::Button("Add include at camera"))
            add_volume(true, game.client_shell.view.position);
        ImGui::SameLine();
        if (ImGui::Button("Add exclude at camera"))
            add_volume(false, game.client_shell.view.position);
        ImGui::EndDisabled();
        volume_table();
        ImGui::Separator();
        baked_probe_panel(game);
    }

    void GiTools::baked_probe_panel(const GameSession& game)
    {
        ImGui::TextUnformatted("Baked light probes (the applied file)");
        if (!runtime_.light || !runtime_.light->has_probes)
        {
            ImGui::TextDisabled("None applied: turn GLOBAL ILLUMINATION on with a file baked since lot G9.");
            return;
        }
        const auto& field = runtime_.light->probes;
        const auto& lattice = field.lattice();
        ImGui::Text("%zu probes (%zu valid) at spacing %u, lattice %u x %u x %u", field.probes().size(), field.valid(),
                    lattice.spacing, lattice.dims[0], lattice.dims[1], lattice.dims[2]);
        ImGui::Checkbox("Show baked SH samples", &show_baked_probes);
        ImGui::SameLine();
        ImGui::TextDisabled("(spheres within the marker radius, at most %zu; magenta: invalid)", max_sh_markers);
        if (!game.client_shell.m_bInWorld)
            return;
        const auto camera = game.client_shell.view.position;
        GiProbeSample sample;
        const bool valid = field.sample(camera, sample);
        ImGui::Text("Camera cell %u %u %u", sample.cell[0], sample.cell[1], sample.cell[2]);
        if (ImGui::BeginTable("gi_probe_corners", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                         ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Corner");
            ImGui::TableSetupColumn("Probe");
            ImGui::TableSetupColumn("Distance");
            ImGui::TableSetupColumn("Weight");
            ImGui::TableHeadersRow();
            for (int corner = 0; corner < 8; ++corner)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d%d%d", corner & 1, corner >> 1 & 1, corner >> 2 & 1);
                ImGui::TableNextColumn();
                const auto probe = sample.probes[std::size_t(corner)];
                if (probe < 0)
                {
                    ImGui::TextDisabled("no probe");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    continue;
                }
                const bool ok = field.probes().valid[std::size_t(probe)] != 0;
                ImGui::TextColored(ok ? ImVec4{.45f, .9f, .45f, 1} : error_color, "#%lld%s", (long long)probe,
                                   ok ? "" : " invalid");
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", std::sqrt(distance2(lattice.probes[std::size_t(probe)].position, camera)));
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", double(sample.weights[std::size_t(corner)]));
            }
            ImGui::EndTable();
        }
        if (!valid)
        {
            ImGui::TextColored(error_color, "No valid probe around the camera: models here keep the original light.");
            return;
        }
        const auto& sh = sample.sh;
        ImGui::Text("SH (E, display units): L0 %.3f %.3f %.3f", sh[0], sh[1], sh[2]);
        ImGui::Text("L1 y %.3f %.3f %.3f  z %.3f %.3f %.3f  x %.3f %.3f %.3f", sh[3], sh[4], sh[5], sh[6], sh[7],
                    sh[8], sh[9], sh[10], sh[11]);
        ImGui::Text("L2 %.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f | %.3f %.3f %.3f", sh[12],
                    sh[13], sh[14], sh[15], sh[16], sh[17], sh[18], sh[19], sh[20], sh[21], sh[22], sh[23], sh[24],
                    sh[25], sh[26]);
        const auto up = gi_sh_irradiance(sh, {0, 1, 0}), down = gi_sh_irradiance(sh, {0, -1, 0});
        ImGui::Text("E up %.3f %.3f %.3f  down %.3f %.3f %.3f", up.x, up.y, up.z, down.x, down.y, down.z);
        if (runtime_.composition)
        {
            const float p = runtime_.probes ? runtime_.probes->intensity : 1;
            DVector ambient{};
            if (runtime_.model_lighting)
                ambient = vector_scale(runtime_.model_lighting->world_ambient, 1.f / 255);
            const auto split = gi_probe_model_light(sh, runtime_.composition->k * p / 2, runtime_.composition->f,
                                                    ambient);
            ImGui::Text("Model light: ambient %.3f %.3f %.3f, pseudo-light %.3f %.3f %.3f toward %.2f %.2f %.2f",
                        split.ambient.x, split.ambient.y, split.ambient.z, split.color.x, split.color.y,
                        split.color.z, split.direction.x, split.direction.y, split.direction.z);
        }
    }

    void GiTools::reflection_panel(const GameSession& game)
    {
        if (!placement_header(*this))
            return;
        auto& p = placement_;
        auto& e = *p.edit;
        auto& probes = e.reflections;
        const auto manual =
            std::size_t(std::count_if(probes.begin(), probes.end(), [](const auto& r) { return !r.automatic; }));
        int cap = int(e.reflection_cap);
        if (ImGui::InputInt("Cap", &cap, 1, 8, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            cap = std::clamp(cap, int(manual), int(gi_placement_limits::max_reflections));
            if (std::uint32_t(cap) != e.reflection_cap)
            {
                e.reflection_cap = std::uint32_t(cap);
                if (probes.size() > e.reflection_cap) // The lowest-ranked automatic probes go first.
                    probes.resize(e.reflection_cap);
                if (selected_reflection >= int(probes.size()))
                    selected_reflection = -1;
                p.dirty = true;
            }
        }
        if (ImGui::Button("Place automatically"))
            place_reflection_probes();
        ImGui::SameLine();
        const bool in_world = game.client_shell.m_bInWorld;
        ImGui::BeginDisabled(!in_world || probes.size() >= e.reflection_cap);
        if (ImGui::Button("Add at camera"))
            add_reflection(game.client_shell.view.position);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Checkbox("Show", &show_reflections);
        const auto& s = p.stats;
        ImGui::Text("%zu probes (%zu manual) of %u", probes.size(), manual, e.reflection_cap);
        if (s.room_candidates)
            ImGui::TextDisabled("Last placement: %llu rooms, %llu small, %llu overlapping, %llu capped, %u open",
                                (unsigned long long)s.room_candidates, (unsigned long long)s.small_rooms,
                                (unsigned long long)s.overlapping, (unsigned long long)s.capped, s.open);
        ImGui::TextDisabled("Orange influence, blue projection, white capture; the selected probe is yellow.");
        if (ImGui::BeginChild("gi_reflections", {0, 140}, ImGuiChildFlags_Borders))
        {
            for (std::size_t i = 0; i < probes.size(); ++i)
            {
                const auto& r = probes[i];
                char label[160];
                std::snprintf(label, sizeof label, "#%zu %s  capture %.0f %.0f %.0f  %s  priority %d##r%zu", i,
                              r.automatic ? "auto  " : "manual", r.capture.x, r.capture.y, r.capture.z,
                              box_size(r.influence).c_str(), r.priority, i);
                if (ImGui::Selectable(label, int(i) == selected_reflection))
                    selected_reflection = int(i) == selected_reflection ? -1 : int(i);
            }
        }
        ImGui::EndChild();
        if (selected_reflection < 0 || selected_reflection >= int(probes.size()))
            return;
        auto index = std::size_t(selected_reflection);
        ImGui::BeginDisabled(!in_world);
        if (ImGui::Button("Move to camera"))
            move_reflection(index, game.client_shell.view.position);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Fit to room"))
            move_reflection(index, probes[index].capture);
        ImGui::SameLine();
        if (ImGui::Button("Delete"))
        {
            delete_reflection(index);
            return;
        }
        // Edits apply to a copy, then make the probe manual (moving it among the manual probes).
        auto r = probes[index];
        bool changed = false;
        ImGui::DragFloat3("Capture", &r.capture.x, 1, -gi_placement_limits::max_coordinate,
                          gi_placement_limits::max_coordinate, "%.1f");
        changed |= ImGui::IsItemEdited();
        box_editor("Influence", r.influence, changed);
        box_editor("Projection", r.projection, changed);
        changed |= ImGui::DragFloat("Blend distance", &r.blend_distance, .5f, 0, gi_placement_limits::max_blend,
                                    "%.1f");
        int priority = r.priority;
        if (ImGui::InputInt("Priority", &priority))
        {
            r.priority = std::clamp(priority, -gi_placement_limits::max_priority, gi_placement_limits::max_priority);
            changed = true;
        }
        changed |= ImGui::Checkbox("Box projection", &r.box_projection);
        const std::string resolution = std::to_string(r.resolution);
        if (ImGui::BeginCombo("Resolution", resolution.c_str()))
        {
            for (const auto value : resolutions)
                if (ImGui::Selectable(std::to_string(value).c_str(), value == r.resolution))
                {
                    r.resolution = value;
                    changed = true;
                }
            ImGui::EndCombo();
        }
        if (changed)
        {
            probes[index] = r;
            selected_reflection = int(make_manual(probes, index));
            p.dirty = true;
        }
    }

    bool GiTools::draw(MenuInput& input, GameSession& game, const Config& config, std::vector<LineVertex>& gizmos,
                       const GiRuntimeLink& runtime)
    {
        gizmos.clear();
        runtime_ = runtime;
        if (input.gi == GiOverlay::Closed)
        {
            stale_ = true; // Opening again re-reads the folders (placement edits survive).
            return false;
        }
        if (stale_ || world_revision_ != game.world_revision || status_.world_path != game.world_path)
            refresh(game, config);
        if (game.client_shell.m_bInWorld)
        {
            if (camera_cell && !game.world.models.empty())
                cell_gizmo(game.world.models[0], game.client_shell.view.position, gizmos);
            placement_gizmos(game.client_shell.view.position, gizmos);
            if (show_baked_probes && runtime_.light && runtime_.light->has_probes)
                baked_probe_gizmos(runtime_.light->probes, runtime_.composition ? runtime_.composition->k : 1,
                                   game.client_shell.view.position, marker_radius, gizmos);
        }
        if (input.gi == GiOverlay::Pinned)
        {
            const auto& io = ImGui::GetIO();
            ImGui::SetNextWindowPos({io.DisplaySize.x - 12, 12}, ImGuiCond_Always, {1, 0});
            ImGui::SetNextWindowBgAlpha(.6f);
            if (ImGui::Begin("##gi_pinned", nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings))
            {
                ImGui::Text("GI tools (pinned, F8): %s", summary().c_str());
                if (runtime_.status && runtime_.enabled && *runtime_.enabled && runtime_.composition)
                    ImGui::Text("View %s, k %.2f, f %.2f, %s%s", gi_view_name(runtime_.composition->view),
                                runtime_.composition->k, runtime_.composition->f,
                                gi_world_source_name(runtime_.composition->source),
                                runtime_.probes && runtime_.probes->models ? ", probes on models" : "");
                ImGui::TextUnformatted(placement_summary().c_str());
            }
            ImGui::End();
            return false;
        }
        bool open = true;
        ImGui::SetNextWindowSize({660, 560}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({40, 40}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("GI tools (F8)", &open, ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::Checkbox("Pin (keep markers and status during play)", &input.gi_pin);
            if (ImGui::BeginTabBar("gi_panels"))
            {
                const auto tab = [&](const char* name, Panel panel)
                {
                    const ImGuiTabItemFlags flags = select == panel ? ImGuiTabItemFlags_SetSelected : 0;
                    return ImGui::BeginTabItem(name, nullptr, flags);
                };
                if (tab("Status", Panel::Status))
                {
                    status_panel(game, config);
                    ImGui::EndTabItem();
                }
                if (tab("Runtime", Panel::Runtime))
                {
                    runtime_panel();
                    ImGui::EndTabItem();
                }
                if (tab("Light probes", Panel::LightProbes))
                {
                    light_panel(game);
                    ImGui::EndTabItem();
                }
                if (tab("Reflection probes", Panel::ReflectionProbes))
                {
                    reflection_panel(game);
                    ImGui::EndTabItem();
                }
                if (tab("Bake", Panel::Bake))
                {
                    unavailable("G10", "baking from the game");
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            select.reset();
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
