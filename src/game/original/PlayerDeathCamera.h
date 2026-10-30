#pragma once
#include "client/collision.h"

// Selected original CPlayerCamera DEATH mode. No independent object or clock.
class CPlayerDeathCamera
{
  public:
    void Begin(float now)
    {
        if (!active)
        {
            active = true;
            start = now;
        }
    }
    bool Update(const OpenBlood2::ClientCollisionWorld&, OpenBlood2::ClientObjectId, DVector body, DRotation target,
                float time, DVector eye);
    bool active = false;
    float start = 0, angle = 0, fraction = 0;
    DVector position{}, requested{}, anchor{};
    DRotation rotation;
    OpenBlood2::ClientSegmentHit hit;
    OpenBlood2::SegmentResult result = OpenBlood2::SegmentResult::Miss;
    std::string error;
};
