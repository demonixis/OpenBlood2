#include "engine/sky.h"
#include "engine/collision.h"
#include <cmath>
namespace OpenBlood2
{
    namespace
    {
        bool valid(const SkyDefinition& d)
        {
            return physics_vector(d.min) && physics_vector(d.max) && physics_vector(d.view_min) &&
                   physics_vector(d.view_max) && d.min.x < d.max.x && d.min.y < d.max.y && d.min.z < d.max.z &&
                   d.view_min.x <= d.view_max.x && d.view_min.y <= d.view_max.y && d.view_min.z <= d.view_max.z;
        }
    } // namespace
    bool SkyState::set(const SkyDefinition& d)
    {
        if (!valid(d))
        {
            error = "Invalid sky definition";
            return false;
        }
        definition = d;
        defined = true;
        ++definitions;
        return true;
    }
    bool SkyState::validate(const Objects& store, HOBJECT h, unsigned index, std::string& e) const
    {
        const auto* r = store.get(h);
        if (!r || r->life != Life::Alive || index >= objects.size())
            e = "Invalid sky object/index";
        else if (r->state.m_ObjectType != OT_WORLDMODEL || !r->world_model)
            e = "Only bound world models are supported in the sky";
        else
        {
            // The original internal INSKY guard runs before slot replacement.
            for (auto entry : objects)
                if (entry == r->id)
                {
                    e.clear();
                    return true;
                }
            const auto* previous = store.get(objects[index]);
            if (previous && previous->life == Life::Alive)
                e = "Duplicate sky index";
            else
            {
                e.clear();
                return true;
            }
        }
        return false;
    }
    bool SkyState::add(const Objects& store, HOBJECT h, unsigned index)
    {
        std::string failure;
        if (!validate(store, h, index, failure))
        {
            error = std::move(failure);
            return false;
        }
        const auto id = store.get(h)->id;
        // Original addition of an already registered object does not change its index.
        for (auto entry : objects)
            if (entry == id)
                return true;
        objects[index] = id;
        ++additions;
        return true;
    }
    bool SkyState::remove(const Objects& store, HOBJECT h)
    {
        const auto* r = store.get(h);
        if (!r || r->life != Life::Alive)
        {
            error = "Invalid sky removal identity";
            return false;
        }
        for (auto& entry : objects)
            if (entry == r->id)
                entry = {};
        return true;
    }
    bool sky_view_position(const SkyDefinition& d, DVector min, DVector max, DVector eye, DVector& out)
    {
        out = {};
        if (!valid(d) || !physics_vector(min) || !physics_vector(max) || !physics_vector(eye) || min.x >= max.x ||
            min.y >= max.y || min.z >= max.z)
            return false;
        // Blood II SkyDef contract; later LithTech common_draw.cpp supplies the explicit formula.
        // No clamp: inspection cameras outside world extents extrapolate the view box.
        auto axis = [](float a, float b, float x, float lo, float hi)
        { return float(double(lo) + (double(x) - a) / (double(b) - a) * (double(hi) - lo)); };
        const DVector p{axis(min.x, max.x, eye.x, d.view_min.x, d.view_max.x),
                        axis(min.y, max.y, eye.y, d.view_min.y, d.view_max.y),
                        axis(min.z, max.z, eye.z, d.view_min.z, d.view_max.z)};
        if (!physics_vector(p))
            return false;
        out = p;
        return true;
    }
    bool SkyState::draws(const World& world, const WorldMesh& mesh, const Objects& store, DVector eye,
                         std::vector<WorldDraw>& draws, DVector& sky_eye, std::string& e) const
    {
        draws.clear();
        sky_eye = {};
        e.clear();
        for (auto id : objects)
        {
            const auto* r = store.get(id);
            if (!r || r->life != Life::Alive || !(r->state.m_Flags & FLAG_VISIBLE))
                continue;
            std::size_t match = 0;
            for (std::size_t i = 1; i < mesh.ranges.size(); ++i)
                if (!CaseLess{}(mesh.ranges[i].name, r->state.m_Filename) &&
                    !CaseLess{}(r->state.m_Filename, mesh.ranges[i].name))
                {
                    if (match)
                    {
                        e = "Ambiguous sky world model";
                        draws.clear();
                        return false;
                    }
                    match = i;
                }
            WorldViewUniform uniform;
            if (!r->world_model || !match ||
                !world_view_uniform({}, {}, 1, {unsigned(match), r->state.m_Pos, r->state.m_Rotation}, uniform))
            {
                e = "Missing or invalid sky world model";
                draws.clear();
                return false;
            }
            draws.push_back({unsigned(match), r->state.m_Pos, r->state.m_Rotation, r->alpha});
        }
        if (!draws.empty() && !sky_view_position(definition, world.min, world.max, eye, sky_eye))
        {
            e = "Invalid sky/world projection extents";
            draws.clear();
            return false;
        }
        return true;
    }
} // namespace OpenBlood2
