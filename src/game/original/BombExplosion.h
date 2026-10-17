#pragma once
#include "basedefs_de.h"
// Original small/medium bomb SFX request, using existing checked component services.
namespace OpenBlood2
{
    void PublishBombExplosion(HOBJECT, DVector, DVector, unsigned, const char*, float damage, float radius);
}
