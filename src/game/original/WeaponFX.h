#pragma once
#include "SurfaceTypes.h"
#include "engine/audio.h"
#include <random>
class ServerDE;
// Selected original CWeaponFX impact audio only. Other weapon visuals remain unavailable.
class CWeaponFX
{
  public:
    DBYTE m_nAmmoType = 0;
    SurfaceType m_eSurfaceType = SURFTYPE_UNKNOWN;
    DVector m_vDestPos{};
    OpenBlood2::SoundId sound = 0;
    std::string resource, error;
    bool PlayImpactSound(ServerDE&, HOBJECT source, std::minstd_rand&);
    std::string GetImpactSound(std::minstd_rand&) const;
};
