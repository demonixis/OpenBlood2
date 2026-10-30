#pragma once
#include "client/collision_values.h"
#include "net/codec.h"

namespace OpenBlood2::Net
{
    bool valid_collision_motion(const CollisionMotion&);
    bool valid_collision_values(const ClientCollisionValues&);
    bool same_collision_metadata(const ClientCollisionValues&, const ClientCollisionValues&);
    void write_collision_motion(Writer&, const CollisionMotion&);
    CollisionMotion read_collision_motion(Reader&);
    void write_collision_values(Writer&, const ClientCollisionValues&);
    bool read_collision_values(Reader&, ClientCollisionValues&);
} // namespace OpenBlood2::Net
