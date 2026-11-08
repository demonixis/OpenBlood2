#include "engine/gi_runtime.h"
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
    namespace
    {
        template <typename Record>
        std::int64_t find_record(const std::vector<Record>& records, std::uint32_t model, std::uint32_t polygon)
        {
            if (model > UINT16_MAX || polygon > UINT16_MAX)
                return -1;
            using Key = std::pair<std::uint32_t, std::uint32_t>;
            const auto less = [](const Record& r, const Key& key) { return Key{r.model, r.polygon} < key; };
            const auto at = std::lower_bound(records.begin(), records.end(), Key{model, polygon}, less);
            return at != records.end() && at->model == model && at->polygon == polygon ? at - records.begin() : -1;
        }
        double since(Uint64 start)
        {
            return double(SDL_GetTicksNS() - start) / 1e6;
        }
    } // namespace

    const char* gi_view_name(GiView view)
    {
        switch (view)
        {
            case GiView::Final:
                return "Final";
            case GiView::Original:
                return "Original";
            case GiView::Direct:
                return "Direct only";
            case GiView::Indirect:
                return "Indirect only";
            case GiView::Lighting:
                return "Lighting only";
            case GiView::Validity:
                return "Texel validity";
        }
        return "?";
    }
    const char* gi_world_source_name(GiWorldSource source)
    {
        return source == GiWorldSource::Probes ? "Light probes" : "Texel gather";
    }
    bool valid_gi_composition(const GiComposition& c)
    {
        return c.k >= 0 && c.k <= gi_max_intensity && c.f >= 0 && c.f <= gi_max_intensity &&
               std::uint8_t(c.view) <= std::uint8_t(GiView::Validity) &&
               std::uint8_t(c.source) <= std::uint8_t(GiWorldSource::Probes);
    }

    std::int64_t GiWorldLight::block(std::uint32_t model, std::uint32_t polygon) const
    {
        return find_record(lightmaps.blocks, model, polygon);
    }
    std::int64_t GiWorldLight::set(std::uint32_t model, std::uint32_t polygon) const
    {
        return find_record(vertex.sets, model, polygon);
    }
    std::array<std::uint8_t, 3> GiWorldLight::compose(bool texel, std::uint32_t index,
                                                      std::array<std::uint8_t, 3> original, std::uint8_t* share) const
    {
        if (share)
            *share = 255;
        const bool from_probes = composition.source == GiWorldSource::Probes && probe_source_ready;
        const auto& values = from_probes ? (texel ? probe_texels : probe_corners)
                                         : (texel ? lightmaps.texels : vertex.corners);
        const auto& valid = from_probes ? (texel ? probe_texel_valid : probe_corner_valid)
                                        : (texel ? lightmaps.valid : vertex.valid);
        if (index >= values.size() || composition.view == GiView::Original)
            return original;
        if (composition.view == GiView::Validity)
        {
            const std::uint8_t on = texel ? 128 : 255; // Full colour once the page is doubled.
            return valid[index] ? std::array<std::uint8_t, 3>{0, on, 0} : std::array<std::uint8_t, 3>{on, 0, 0};
        }
        const auto energy = gi_rgb9e5_decode(values[index]);
        const float e[3]{energy.x, energy.y, energy.z}, a[3]{ambient.x, ambient.y, ambient.z};
        std::array<std::uint8_t, 3> out{};
        // The page alpha (lot G12): the share of the composed light that is not GI light beyond the fill.
        auto direct_share = [&]
        {
            if (!share)
                return;
            if (composition.view == GiView::Indirect)
            {
                *share = 0;
                return;
            }
            if (composition.view != GiView::Final && composition.view != GiView::Lighting)
                return;
            constexpr double weight[3]{.299, .587, .114};
            double page = 0, extra = 0;
            for (int c = 0; c < 3; ++c)
            {
                const double o = original[c];
                const double fill_ambient = a[c] > 0 ? std::min(o, double(a[c])) : 0.0;
                double fill = double(composition.f) * fill_ambient;
                if (!(fill >= 0))
                    fill = 0;
                double indirect = double(composition.k) * (e[c] > 0 ? double(e[c]) : 0.0) * (texel ? 255.0 / 2 : 255.0);
                if (!(indirect >= 0))
                    indirect = 0;
                const double x = indirect > fill ? indirect - fill : 0.0;
                page += weight[c] * out[c];
                extra += weight[c] * std::min(x, double(out[c]));
            }
            if (page > 0 && extra > 0)
                *share = static_cast<std::uint8_t>(std::clamp(std::floor(255 * (1 - extra / page) + .5), 0.0, 255.0));
        };
        if (texel && rebaked && index < direct.texels.size())
        {
            // A re-baked file (lot GD2): D' replaces D.
            const auto d = gi_rgb9e5_decode(direct.texels[index]);
            const float dv[3]{d.x, d.y, d.z};
            for (int c = 0; c < 3; ++c)
                switch (composition.view)
                {
                    case GiView::Direct:
                        out[c] = gi_compose_direct(dv[c], original[c], a[c], 0, 0, 0);
                        break;
                    case GiView::Indirect:
                        out[c] = gi_compose(0, 0, e[c], composition.k, 0, true);
                        break;
                    default: // Final and Lighting only.
                        out[c] = gi_compose_direct(dv[c], original[c], a[c], e[c], composition.k, composition.f);
                        break;
                }
            direct_share();
            return out;
        }
        for (int c = 0; c < 3; ++c)
            switch (composition.view)
            {
                case GiView::Direct:
                    out[c] = gi_compose(original[c], a[c], 0, 0, 0, texel);
                    break;
                case GiView::Indirect:
                    out[c] = gi_compose(0, 0, e[c], composition.k, 0, texel);
                    break;
                default: // Final and Lighting only.
                    out[c] = gi_compose(original[c], a[c], e[c], composition.k, composition.f, texel);
                    break;
            }
        direct_share();
        return out;
    }

    bool gi_world_light(const World& world, const GiFile& file, GiWorldLight& out, std::string& error)
    {
        const auto* lmap = file.find(gi_format::lightmaps);
        const auto* vtx = file.find(gi_format::vertices);
        if (!lmap || !vtx)
        {
            error = "The file has no LMAP/VTX sections (a bake older than lot G7)";
            return false;
        }
        GiWorldLight light;
        if (!read_gi_lightmaps(lmap->payload, light.lightmaps, error) ||
            !read_gi_vertex_light(vtx->payload, light.vertex, error) ||
            !check_gi_lightmaps(world, light.lightmaps, error) || !check_gi_vertex_light(world, light.vertex, error))
            return false;
        if (const auto* dir = file.find(gi_format::direct))
        {
            if (!read_gi_direct(dir->payload, light.direct, error))
                return false;
            if (light.direct.texels.size() != light.lightmaps.texels.size())
            {
                error = "The DIR section does not have one value per LMAP texel";
                return false;
            }
            light.rebaked = true;
        }
        light.block_first.reserve(light.lightmaps.blocks.size());
        std::uint32_t first = 0;
        for (const auto& b : light.lightmaps.blocks)
        {
            light.block_first.push_back(first);
            first += std::uint32_t(b.width) * b.height;
        }
        first = 0;
        light.set_first.reserve(light.vertex.sets.size());
        for (const auto& s : light.vertex.sets)
        {
            light.set_first.push_back(first);
            first += s.corners;
        }
        if (!world_info_ambient_any_case(world.info, light.ambient))
            light.ambient = {};
        const auto* prob = file.find(gi_format::probes);
        const auto* refl = file.find(gi_format::reflections);
        if (prob || refl)
        {
            const auto* plac = file.find(gi_format::placement);
            if (!plac)
            {
                error = std::string("The file has a ") + (prob ? "PROB" : "REFL") + " section without PLAC";
                return false;
            }
            GiLightLattice lattice;
            std::vector<GiReflectionProbe> reflections;
            if (!decode_gi_placement_section(plac->payload, lattice, reflections, error))
                return false;
            if (prob)
            {
                GiProbes probes;
                if (!read_gi_probes(prob->payload, probes, error) ||
                    !light.probes.build(std::move(lattice), std::move(probes), error))
                    return false;
                light.has_probes = true;
            }
            if (refl)
            {
                if (!read_gi_reflections(refl->payload, light.reflections, error) ||
                    !check_gi_reflections(reflections, light.reflections, error))
                    return false;
                light.reflection_probes = std::move(reflections);
                light.has_reflections = true;
            }
        }
        out = std::move(light);
        return true;
    }

    bool GiWorldLight::prepare_probe_source(const World& world, std::string& error)
    {
        if (probe_source_ready)
            return true;
        if (!has_probes)
        {
            error = "The file has no light probes";
            return false;
        }
        const auto start = SDL_GetTicksNS();
        auto ring = [](const Polygon& p)
        { return p.base_points == UINT16_MAX ? p.points.size() : std::min<std::size_t>(p.base_points, p.points.size()); };
        auto evaluate = [&](DVector point, DVector normal, std::uint32_t& value, std::uint8_t& valid)
        {
            GiProbeSample sample;
            valid = probes.sample(point, sample);
            value = valid ? gi_rgb9e5(gi_sh_irradiance(sample.sh, normal)) : 0;
        };
        std::vector<std::uint32_t> texels(lightmaps.texels.size()), corners(vertex.corners.size());
        std::vector<std::uint8_t> texel_valid(texels.size()), corner_valid(corners.size());
        for (std::size_t b = 0; b < lightmaps.blocks.size(); ++b)
        {
            const auto& block = lightmaps.blocks[b];
            const auto& model = world.models[block.model];
            const auto& polygon = model.polygons[block.polygon];
            LightmapFrame frame;
            if (!lightmap_frame(model, polygon, ring(polygon), frame))
            {
                error = "Light probes: invalid lightmap frame";
                return false;
            }
            const auto length = std::sqrt(vector_dot(frame.normal, frame.normal));
            const auto normal = length > 0 ? vector_scale(frame.normal, 1 / length) : DVector{0, 0, 1};
            for (std::uint32_t y = 0; y < block.height; ++y)
                for (std::uint32_t x = 0; x < block.width; ++x)
                {
                    const auto at = block_first[b] + y * block.width + x;
                    DVector position{};
                    if (!frame.position(x + .5, y + .5, position))
                        continue; // Degenerate frames draw nothing: 0.
                    evaluate(position, normal, texels[at], texel_valid[at]);
                }
        }
        for (std::size_t s = 0; s < vertex.sets.size(); ++s)
        {
            const auto& set = vertex.sets[s];
            const auto& model = world.models[set.model];
            const auto& polygon = model.polygons[set.polygon];
            // The plane normal, as the bake's receivers use it.
            auto normal = model.planes[model.surfaces[polygon.surface].plane].normal;
            const auto length = std::sqrt(vector_dot(normal, normal));
            normal = length > 0 ? vector_scale(normal, 1 / length) : DVector{0, 0, 1};
            for (std::uint32_t k = 0; k < set.corners; ++k)
                evaluate(model.points[polygon.points[k]], normal, corners[set_first[s] + k],
                         corner_valid[set_first[s] + k]);
        }
        probe_texels = std::move(texels);
        probe_corners = std::move(corners);
        probe_texel_valid = std::move(texel_valid);
        probe_corner_valid = std::move(corner_valid);
        probe_source_ready = true;
        probe_source_ms = since(start);
        return true;
    }

    const char* gi_runtime_state_name(GiRuntimeState state)
    {
        switch (state)
        {
            case GiRuntimeState::Off:
                return "off";
            case GiRuntimeState::NoWorld:
                return "no world";
            case GiRuntimeState::Missing:
                return "no ready file";
            case GiRuntimeState::Failed:
                return "failed";
            case GiRuntimeState::Applied:
                return "applied";
        }
        return "?";
    }

    std::string gi_status_line(const GiRuntimeStatus& s)
    {
        char cost[160];
        SDL_snprintf(cost, sizeof cost, "keys %.1f ms, lookup %.1f ms, sections %.1f ms", s.expectation_ms,
                     s.lookup_ms, s.decode_ms);
        if (s.state == GiRuntimeState::Applied)
            return std::string(s.lighting ? "GI applied: " : "GI read for reflection probes only: ") + s.world_path + " from " + s.file + " (" + std::to_string(s.blocks) + " blocks, " +
                   std::to_string(s.texels) + " texels, " + std::to_string(s.sets) + " corner sets, " +
                   std::to_string(s.corners) + " corners, " + std::to_string(s.valid_probes) + "/" +
                   std::to_string(s.probes) + " light probes, " + std::to_string(s.valid_reflections) + "/" +
                   std::to_string(s.reflection_probes) + " reflection probes" +
                   (s.rebaked ? ", re-baked direct light" : "") +
                   "; " + cost + ")";
        if (s.state == GiRuntimeState::Off || s.state == GiRuntimeState::NoWorld)
            return std::string("GI ") + gi_runtime_state_name(s.state);
        return "GI not applied to " + s.world_path + " (" + gi_runtime_state_name(s.state) + "): " + s.detail + " (" +
               cost + ")";
    }

    void load_gi_world_light(const Resources& resources, const World& world, std::string_view world_path,
                             std::span<const GiRoot> roots, std::optional<GiWorldLight>& light,
                             GiRuntimeStatus& status)
    {
        light.reset();
        const auto loads = status.loads + 1;
        status = {};
        status.loads = loads;
        status.world_path = world_path;
        if (world_path.empty() || world.models.empty())
        {
            status.state = GiRuntimeState::NoWorld;
            return;
        }
        std::string error;
        auto start = SDL_GetTicksNS();
        GiExpectation expected;
        const bool keyed = gi_expectation(resources, world_path, expected, error);
        status.expectation_ms = since(start);
        if (!keyed)
        {
            status.state = GiRuntimeState::Failed;
            status.detail = "Content keys: " + error;
            return;
        }
        start = SDL_GetTicksNS();
        const bool found = find_gi_file(roots, world_path, expected, status.lookup, error);
        status.lookup_ms = since(start);
        if (!found)
        {
            status.lookup = {};
            status.state = GiRuntimeState::Failed;
            status.detail = "Lookup: " + error;
            return;
        }
        if (!status.lookup.file || status.lookup.chosen < 0)
        {
            status.state = GiRuntimeState::Missing;
            for (const auto& c : status.lookup.candidates)
                if (c.status != GiStatus::Missing)
                {
                    if (!status.detail.empty())
                        status.detail += "; ";
                    status.detail += c.source + ": " + gi_status_name(c.status) +
                                     (c.status == GiStatus::Stale ? " (" + gi_stale_text(c.stale) + ")"
                                      : c.detail.empty()          ? std::string()
                                                                  : " (" + c.detail + ")");
                }
            if (status.detail.empty())
                status.detail = "No baked file in any search root";
            return;
        }
        status.file = status.lookup.candidates[std::size_t(status.lookup.chosen)].path;
        status.settings = status.lookup.file->settings;
        start = SDL_GetTicksNS();
        GiWorldLight decoded;
        const bool decoded_ok = gi_world_light(world, *status.lookup.file, decoded, error);
        status.decode_ms = since(start);
        status.lookup.file.reset(); // The decoded sections replace the payload.
        if (!decoded_ok)
        {
            status.state = GiRuntimeState::Failed;
            status.detail = error;
            return;
        }
        status.rebaked = decoded.rebaked;
        status.blocks = decoded.lightmaps.blocks.size();
        status.texels = decoded.lightmaps.texels.size();
        status.sets = decoded.vertex.sets.size();
        status.corners = decoded.vertex.corners.size();
        status.probes = decoded.probes.probes().size();
        status.valid_probes = decoded.probes.valid();
        status.probe_spacing = decoded.probes.lattice().spacing;
        status.reflection_probes = decoded.reflections.size();
        status.valid_reflections = std::size_t(std::count(decoded.reflections.valid.begin(),
                                                          decoded.reflections.valid.end(), std::uint8_t(1)));
        status.reflection_resolution = decoded.reflections.resolution;
        status.reflection_levels = decoded.reflections.levels;
        status.state = GiRuntimeState::Applied;
        light = std::move(decoded);
    }
} // namespace OpenBlood2
