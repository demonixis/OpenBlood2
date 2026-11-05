#pragma once
#include "client/environment.h"
#include "engine/container.h"
#include "engine/segment_types.h"
#include <memory>
#include <optional>
#include <vector>
namespace OpenBlood2
{
    struct ClientCollider
    {
        ObjectId id, ignored_by;
        // Absence is distinct from a recognized built-in Container with code zero.
        std::optional<ContainerEnvironment> environment;
        bool volume_brush = false; // Original ray filter excludes VolumeBrush, not built-in Container.
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
    struct ClientSegmentHit
    {
        DVector point{}, normal{};
        ObjectId object;
        std::uint32_t polygon = UINT32_MAX;
        double fraction = 1;
        std::uint16_t texture_flags = 0;
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
        // An accepted frame swaps with the current scene: the argument then holds the
        // replaced one. A scene identical to the accepted one skips revalidation.
        bool apply(ClientCollisionFrame&&, std::string& error);
        const ClientCollisionFrame& frame() const
        {
            return frame_;
        }
        const ClientCollider* get(ObjectId) const;
        CollisionQuery sweep(ClientObjectId ignore, DVector from, DVector to, DVector half, ClientCollisionHit&,
                             ObjectId extra = {}, double allowed = 0, StartOverlap start = StartOverlap::Block) const;
        SegmentResult intersect(DVector from, DVector to, ClientObjectId ignore, ClientSegmentHit&,
                                SegmentObjects = SegmentObjects::SolidOnly, bool ignore_volumes = false,
                                SegmentStart = SegmentStart::IncludeInterior) const;
        bool support(ClientObjectId owner, ClientMotionState&) const;
        bool environment(ClientObjectId owner, const ClientMotionState&, DVector head, ClientContainerState&) const;
        // Destination/velocity have already been integrated by CMoveMgr.
        bool predict(ClientObjectId, const ClientMotionState&, DVector destination, DVector velocity,
                     DVector acceleration, ClientMotionState& result) const;

      private:
        bool push(const ClientCollider&, DVector destination, ObjectId ignore, DVector& reached) const;
        ClientCollisionFrame frame_;
    };
} // namespace OpenBlood2
