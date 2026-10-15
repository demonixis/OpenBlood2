#include "engine/container.h"
#include "game/original/VolumeBrush.h"
#include "game/server_adapter.h"

namespace OpenBlood2
{
    bool ServerAdapter::GetContainers(HOBJECT object, DVector head, ContainerState& state)
    {
        trace_service("GetContainers");
        state = {};
        const auto* body = objects_.get(object);
        if (!body || body->life != Life::Alive || !physics_vector(head))
            return false;
        bool head_found = false;
        for (auto id : objects_.snapshot())
        {
            const auto* r = objects_.get(id);
            if (!r || r->life != Life::Alive || !r->container)
                continue;
            const auto* brush = dynamic_cast<const VolumeBrush*>(r->game.get());
            if (!brush)
                continue;
            if (r->container->overlaps(body->state.m_Pos, body->dimensions))
            {
                ++state.total;
                if (state.count < state.body.size())
                    state.body[state.count++] = brush->info;
            }
            // Original camera asks for one point container, then checks Hidden.
            // Stable object-slot order replaces the unavailable retail BSP ordering.
            if (!head_found && r->container->contains(head))
            {
                head_found = true;
                state.head = brush->info;
                if (state.head.hidden)
                    state.head.code = CC_NOTHING;
            }
        }
        return true;
    }
    bool ServerAdapter::affect_containers(HOBJECT object, ContainerPhysics& physics)
    {
        auto* body = objects_.get(object);
        if (!body || body->life != Life::Alive)
            return false;
        for (auto id : objects_.snapshot())
        {
            auto* r = objects_.get(id);
            if (!r || r->life != Life::Alive || !r->container ||
                !r->container->overlaps(body->state.m_Pos, body->dimensions))
                continue;
            if (!objects_.engine_message(r->handle, MID_AFFECTPHYSICS, &physics))
                return false;
            if (body->life != Life::Alive)
                break;
        }
        return true;
    }
    bool ServerAdapter::damage_containers()
    {
        if (objects_.busy())
            return objects_.fail(ObjectError::Busy, "Container damage inside a callback rejected");
        for (auto id : objects_.snapshot())
        {
            const auto* r = objects_.get(id);
            if (!r || r->life != Life::Alive || r->container || r->activity != OBJSTATE_ACTIVE ||
                (r->state.m_ObjectType != OT_NORMAL && r->state.m_ObjectType != OT_MODEL))
                continue;
            ContainerPhysics physics;
            physics.object = r->handle;
            physics.damage_only = true;
            physics.player = r->client_movement;
            if (!affect_containers(r->handle, physics))
                return false;
        }
        return true;
    }
} // namespace OpenBlood2
