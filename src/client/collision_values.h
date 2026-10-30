#pragma once
#include "engine/container_types.h"
#include "engine/object_id.h"
#include <optional>

namespace OpenBlood2
{
    struct CollisionMotion
    {
        DVector dimensions{}, velocity{}, acceleration{}, shape_origin{};
    };
    // Values needed by the passive collision scene. Geometry is resolved from
    // this client's admitted DAT, never transferred as an engine pointer.
    struct ClientCollisionValues
    {
        bool enabled = false;
        CollisionMotion motion;
        ObjectId ignored_by;
        DRotation basis;
        std::optional<ContainerEnvironment> environment;
        bool volume_brush = false, world_model = false, container = false;
        bool dynamic = false, pushable = false, shape_transformed = false;
        std::uint8_t priority = 0;
    };
} // namespace OpenBlood2
