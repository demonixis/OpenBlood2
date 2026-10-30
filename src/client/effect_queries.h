#pragma once
#include "client/collision.h"
namespace OpenBlood2
{
    // Read-only services used by original client FX. No object handle, callback,
    // simulation, damage, random generator or audio device crosses this boundary.
    class EffectQueries
    {
      public:
        virtual ~EffectQueries() = default;
        virtual SegmentResult effect_segment(DVector, DVector, ClientSegmentHit&,
                                             SegmentObjects = SegmentObjects::SolidOnly) = 0;
        virtual SegmentResult effect_world_segment(DVector, DVector, ClientSegmentHit&) = 0;
        virtual SegmentResult effect_world_ray(DVector, DVector, ClientSegmentHit&) = 0;
        virtual bool effect_container(DVector, ContainerCode&, bool visible_only = false) = 0;
    };
    class ClientEffectQueries final : public EffectQueries
    {
      public:
        // Null explicitly represents unavailable geometry; never fall back to a server query.
        explicit ClientEffectQueries(const ClientCollisionWorld* scene) : scene_(scene) {}
        SegmentResult effect_segment(DVector, DVector, ClientSegmentHit&,
                                     SegmentObjects = SegmentObjects::SolidOnly) override;
        SegmentResult effect_world_segment(DVector, DVector, ClientSegmentHit&) override;
        SegmentResult effect_world_ray(DVector, DVector, ClientSegmentHit&) override;
        bool effect_container(DVector, ContainerCode&, bool visible_only = false) override;

      private:
        const ClientCollisionWorld* scene_;
    };
    // Shared with the authoritative ray service: preserve the finite 1e7 domain.
    SegmentResult bound_collision_ray(DVector from, DVector direction, DVector& to);
} // namespace OpenBlood2
