#pragma once
#include "game/legacy/basedefs_de.h"
#include <string>
namespace OpenBlood2
{
    struct SoundAlertReport
    {
        unsigned visited = 0, eligible = 0, delivered = 0, skipped = 0;
        bool complete = false;
        std::string error;
    };
} // namespace OpenBlood2
// Original ObjectUtilities::SendSoundTrigger. Deliveries are messages, not AI reactions.
bool SendSoundTrigger(HOBJECT, int, DVector, float, OpenBlood2::SoundAlertReport&);
