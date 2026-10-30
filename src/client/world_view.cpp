#include "client/world_view.h"
#include "engine/collision.h"
#include "engine/sky.h"
#include <algorithm>

namespace OpenBlood2
{
    bool world_draws(const WorldMesh& mesh, const ClientWorld& world, std::vector<WorldDraw>& draws,
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
        for (const auto& object : world.frame().objects)
        {
            const auto* r = &object;
            if (r->type == OT_CONTAINER)
            {
                ++stats.containers;
                continue;
            }
            if (r->type != OT_WORLDMODEL)
                continue;
            // One range per world model in file order: draw the model creation bound.
            const std::size_t match = r->world_model_index;
            if (!match || match >= mesh.ranges.size() || !r->world_model_bound ||
                CaseLess{}(mesh.ranges[match].name, r->filename) || CaseLess{}(r->filename, mesh.ranges[match].name))
            {
                error = "Missing diagnostic world model binding";
                draws.clear();
                return false;
            }
            bound[match] = true;
            if (r->flags & FLAG_SKYOBJECT)
                continue;
            if (!(r->flags & FLAG_VISIBLE))
            {
                ++stats.hidden;
                continue;
            }
            WorldViewUniform uniform;
            if (!world_view_uniform({}, {}, 1, {static_cast<std::uint32_t>(match), r->pose.position, r->pose.rotation},
                                    uniform))
            {
                error = "Invalid diagnostic world model transform";
                draws.clear();
                return false;
            }
            draws.push_back({static_cast<std::uint32_t>(match), r->pose.position, r->pose.rotation});
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
    bool client_sky_draws(const World& world, const WorldMesh& mesh, const ClientWorld& store, DVector eye,
                          std::vector<WorldDraw>& draws, DVector& sky_eye, std::string& e)
    {
        draws.clear();
        sky_eye = {};
        e.clear();
        bool visible = false;
        for (auto id : store.frame().sky.objects)
        {
            const auto* r = store.get(id);
            if (!r || !(r->flags & FLAG_VISIBLE))
                continue;
            visible = true;
            if (r->type == OT_SPRITE && r->sprite)
                continue; // Sprite mesh uses the same sky camera and interleaved slot order.
            std::size_t match = r->world_model_index; // Bound at creation (first in file order).
            if (match >= mesh.ranges.size() || CaseLess{}(mesh.ranges[match].name, r->filename) ||
                CaseLess{}(r->filename, mesh.ranges[match].name))
                match = 0;
            WorldViewUniform uniform;
            if (!r->world_model_bound || !match ||
                !world_view_uniform({}, {}, 1, {unsigned(match), r->pose.position, r->pose.rotation}, uniform))
            {
                e = "Missing or invalid sky world model";
                draws.clear();
                return false;
            }
            draws.push_back({unsigned(match), r->pose.position, r->pose.rotation, r->alpha});
        }
        if (visible && !sky_view_position({store.frame().sky.min, store.frame().sky.max, store.frame().sky.view_min,
                                           store.frame().sky.view_max},
                                          world.min, world.max, eye, sky_eye))
        {
            e = "Invalid sky/world projection extents";
            draws.clear();
            return false;
        }
        return true;
    }
} // namespace OpenBlood2
