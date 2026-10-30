#include "engine/sky.h"
#include "engine/objects.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool SkyState::set(const SkyDefinition& d)
    {
        if (!valid_sky_definition(d))
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
        else if (!((r->state.m_ObjectType == OT_WORLDMODEL && r->world_model) ||
                   (r->state.m_ObjectType == OT_SPRITE && r->sprite)))
            e = "Only bound world models and sprites are supported in the sky";
        else
        {
            e.clear();
            return true;
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
        std::erase_if(registered,
                      [&](auto entry)
                      {
                          const auto* r = store.get(entry);
                          return !r || r->life != Life::Alive;
                      });
        for (auto entry : registered)
            if (entry == id)
                return true;
        registered.push_back(id);
        objects[index] = id;
        ++additions;
        return true;
    }
    bool SkyState::restore(const Objects& store, HOBJECT h, unsigned slot)
    {
        std::string failure;
        if (!validate(store, h, slot < objects.size() ? slot : 0, failure))
        {
            error = std::move(failure);
            return false;
        }
        const auto id = store.get(h)->id;
        std::erase_if(registered,
                      [&](auto entry)
                      {
                          const auto* r = store.get(entry);
                          return !r || r->life != Life::Alive;
                      });
        if (std::find(registered.begin(), registered.end(), id) == registered.end())
        {
            registered.push_back(id);
            ++additions;
        }
        if (slot < objects.size())
            objects[slot] = id;
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
        std::erase(registered, r->id);
        for (auto& entry : objects)
            if (entry == r->id)
                entry = {};
        return true;
    }
    bool SkyState::draws(const World& world, const WorldMesh& mesh, const Objects& store, DVector eye,
                         std::vector<WorldDraw>& draws, DVector& sky_eye, std::string& e) const
    {
        draws.clear();
        sky_eye = {};
        e.clear();
        bool visible = false;
        for (auto id : objects)
        {
            const auto* r = store.get(id);
            if (!r || r->life != Life::Alive || !(r->state.m_Flags & FLAG_VISIBLE))
                continue;
            visible = true;
            if (r->state.m_ObjectType == OT_SPRITE && r->sprite)
                continue; // Sprite mesh uses the same sky camera and interleaved slot order.
            std::size_t match = r->world_model_index; // Bound at creation (first in file order).
            if (match >= mesh.ranges.size() || CaseLess{}(mesh.ranges[match].name, r->state.m_Filename) ||
                CaseLess{}(r->state.m_Filename, mesh.ranges[match].name))
                match = 0;
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
        if (visible && !sky_view_position(definition, world.min, world.max, eye, sky_eye))
        {
            e = "Invalid sky/world projection extents";
            draws.clear();
            return false;
        }
        return true;
    }
} // namespace OpenBlood2
