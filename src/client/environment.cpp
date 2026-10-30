#include "client/collision.h"
#include "engine/shape_collision.h"
namespace OpenBlood2
{
    bool ClientCollisionWorld::environment(ClientObjectId owner, const ClientMotionState& body, DVector head,
                                           ClientContainerState& state) const
    {
        state = {};
        const auto* object = get(owner.object);
        if (!owner.world || owner.world != frame_.world || !object || !frame_.primary || !physics_vector(head) ||
            !physics_vector(body.position) || !physics_vector(body.dimensions) || body.dimensions.x < 0 ||
            body.dimensions.y < 0 || body.dimensions.z < 0)
            return false;
        const auto center = shape_bounds_center(body.position, object->shape.get());
        bool head_found = false;
        for (const auto& volume : frame_.objects)
        {
            if (!volume.environment)
                continue;
            ClientContainerInfo info;
            static_cast<ContainerEnvironment&>(info) = *volume.environment;
            info.object = volume.id;
            // Container shapes are already posed in world coordinates, as in the
            // original local query. Do not add their DAT translation a second time.
            if (volume.shape->overlaps(center, body.dimensions))
            {
                ++state.total;
                if (state.count < state.body.size())
                    state.body[state.count++] = info;
            }
            if (!head_found && volume.shape->contains(head))
            {
                state.head = info;
                if (info.hidden)
                    state.head.code = CC_NOTHING;
                head_found = true; // A hidden/zero-code first volume masks later ones.
            }
        }
        return true;
    }
} // namespace OpenBlood2
