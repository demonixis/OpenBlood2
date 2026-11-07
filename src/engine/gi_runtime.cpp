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
    bool valid_gi_composition(const GiComposition& c)
    {
        return c.k >= 0 && c.k <= gi_max_intensity && c.f >= 0 && c.f <= gi_max_intensity &&
               std::uint8_t(c.view) <= std::uint8_t(GiView::Validity);
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
                                                      std::array<std::uint8_t, 3> original) const
    {
        const auto& values = texel ? lightmaps.texels : vertex.corners;
        const auto& valid = texel ? lightmaps.valid : vertex.valid;
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
        out = std::move(light);
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
            return "GI applied: " + s.world_path + " from " + s.file + " (" + std::to_string(s.blocks) + " blocks, " +
                   std::to_string(s.texels) + " texels, " + std::to_string(s.sets) + " corner sets, " +
                   std::to_string(s.corners) + " corners; " + cost + ")";
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
        status.blocks = decoded.lightmaps.blocks.size();
        status.texels = decoded.lightmaps.texels.size();
        status.sets = decoded.vertex.sets.size();
        status.corners = decoded.vertex.corners.size();
        status.state = GiRuntimeState::Applied;
        light = std::move(decoded);
    }
} // namespace OpenBlood2
