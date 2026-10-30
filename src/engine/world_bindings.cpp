#include "engine/objects.h"
#include "engine/world_view.h"
#include <algorithm>

namespace OpenBlood2
{
    bool world_draws(const WorldMesh& mesh, const Objects& objects, std::vector<WorldDraw>& draws,
                     WorldViewStats& stats, std::string& error)
    {
        draws.clear();
        stats.triangles = stats.draws = stats.hidden = stats.unbound = stats.containers = stats.batches = 0;
        error.clear();
        if (mesh.ranges.empty())
            return true;
        draws.push_back({0, {}, {}});
        std::vector<bool> bound(mesh.ranges.size());
        bound[0] = true;
        for (auto id : objects.snapshot())
        {
            const auto* r = objects.get(id);
            if (!r || r->life != Life::Alive)
                continue;
            if (r->state.m_ObjectType == OT_CONTAINER)
            {
                ++stats.containers;
                continue;
            }
            if (r->state.m_ObjectType != OT_WORLDMODEL)
                continue;
            // One range per world model in file order: draw the model creation bound.
            const std::size_t match = r->world_model_index;
            if (!match || match >= mesh.ranges.size() || !r->world_model ||
                CaseLess{}(mesh.ranges[match].name, r->state.m_Filename) ||
                CaseLess{}(r->state.m_Filename, mesh.ranges[match].name))
            {
                error = "Missing diagnostic world model binding";
                draws.clear();
                return false;
            }
            bound[match] = true;
            if (r->state.m_Flags & FLAG_SKYOBJECT)
                continue;
            if (!(r->state.m_Flags & FLAG_VISIBLE))
            {
                ++stats.hidden;
                continue;
            }
            WorldViewUniform uniform;
            if (!world_view_uniform({}, {}, 1, {static_cast<std::uint32_t>(match), r->state.m_Pos, r->state.m_Rotation},
                                    uniform))
            {
                error = "Invalid diagnostic world model transform";
                draws.clear();
                return false;
            }
            draws.push_back({static_cast<std::uint32_t>(match), r->state.m_Pos, r->state.m_Rotation});
        }
        stats.unbound = std::count(bound.begin(), bound.end(), false);
        for (const auto& draw : draws)
        {
            stats.triangles += mesh.ranges[draw.range].count / 3;
            stats.batches += mesh.ranges[draw.range].batches.empty() ? 1 : mesh.ranges[draw.range].batches.size();
        }
        stats.draws = draws.size();
        return true;
    }
} // namespace OpenBlood2
