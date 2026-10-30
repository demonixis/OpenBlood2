#pragma once
#include "client/movement.h"
#include "engine/container.h"
#include <memory>
#include <vector>
namespace OpenBlood2
{
    struct ClientCollider
    {
        ObjectId id, ignored_by;
        DVector position{}, dimensions{}, velocity{}, acceleration{};
        std::uint32_t flags = 0, user_flags = 0;
        std::uint16_t type = 0;
        bool attached = false, world_model = false, container = false;
        bool dynamic = false, pushable = false;
        std::uint8_t priority = 0;
        // A client-owned deep copy of the posed geometry, never a server shape.
        std::shared_ptr<const ContainerShape> shape;
    };
    struct ClientCollisionFrame
    {
        std::uint64_t world = 0, sequence = 0;
        std::shared_ptr<const CollisionWorld> primary;
        std::vector<ClientCollider> objects;
    };
    struct ClientCollisionHit
    {
        ObjectId object;
        double fraction = 1, penetration = 0;
        DVector normal{}, point{};
        std::uint32_t polygon = UINT32_MAX, departed = 0;
        std::uint16_t texture_flags = 0;
        bool start_solid = false, allowed_overlap = false;
    };
    // A passive collision scene: no registry, original handles, callbacks, random
    // draws or server service. Publication and prediction are separate operations.
    class ClientCollisionWorld
    {
      public:
        bool begin(std::uint64_t world);
        bool apply(ClientCollisionFrame&&, std::string& error);
        const ClientCollisionFrame& frame() const
        {
            return frame_;
        }
        const ClientCollider* get(ObjectId) const;
        CollisionQuery sweep(ClientObjectId ignore, DVector from, DVector to, DVector half, ClientCollisionHit&,
                             ObjectId extra = {}, double allowed = 0, StartOverlap start = StartOverlap::Block) const;
        bool support(ClientObjectId owner, ClientMotionState&) const;
        // Destination/velocity have already been integrated by CMoveMgr.
        bool predict(ClientObjectId, const ClientMotionState&, DVector destination, DVector velocity,
                     DVector acceleration, ClientMotionState& result) const;

      private:
        bool push(const ClientCollider&, DVector destination, ObjectId ignore, DVector& reached) const;
        ClientCollisionFrame frame_;
    };
} // namespace OpenBlood2
