#pragma once
#include "client/collision.h"
#include "client/world.h"
#include "engine/point_light.h"

// Selected original client CFlashlightFX. An owned value replaces client OT_LIGHT.
class CFlashlightFX
{
  public:
    bool Update(const OpenBlood2::ClientWorld&, const OpenBlood2::ClientCollisionWorld*, OpenBlood2::ClientObjectId,
                OpenBlood2::ClientObjectId);
    OpenBlood2::PointLight Current(const OpenBlood2::ClientWorld&) const;
    OpenBlood2::ClientObjectId source, owner;
    OpenBlood2::PointLight light;
    OpenBlood2::ClientSegmentHit hit;
    OpenBlood2::SegmentResult result = OpenBlood2::SegmentResult::Miss;
    DVector shade_position{}; // Source queries owner-position minus hit, not the hit point.
    float distance = 0, minimum_intensity = 132.f / 255;
    bool visible = false, shade_available = false;
    std::uint64_t updates = 0;
    std::string error;
};
