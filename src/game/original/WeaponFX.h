#pragma once
#include "SurfaceTypes.h"
#include "engine/audio.h"
#include <random>
class ServerDE;
// Selected original CWeaponFX impact audio and wood/stone flash/smoke.
class CWeaponFX
{
  public:
    DBYTE m_nAmmoType = 0;
    SurfaceType m_eSurfaceType = SURFTYPE_UNKNOWN;
    DVector m_vDestPos{};
    OpenBlood2::SoundId sound = 0;
    std::string resource, error, sprite_error;
    std::uint64_t sprite_event = 0;
    int blood_roll = -1;
    bool blood_requested = false;
    std::uint64_t blood_events[2]{};
    std::string blood_error;
    std::uint64_t spark_event = 0;
    bool sparks_requested = false, sparks_big = false;
    std::string spark_error;
    std::uint64_t shotgun_event = 0;
    std::string shotgun_error;
    bool CreateSparks(ServerDE&, HOBJECT, DVector normal, float damage, bool melee, std::minstd_rand&);
    bool SelectBloodSpurt(float damage, std::minstd_rand&);
    bool CreateBloodSpurt(ServerDE&, HOBJECT, DVector normal, float damage, std::minstd_rand&);
    bool CreateSprites(ServerDE&, HOBJECT, DVector normal, std::minstd_rand&, bool ballistic_bubbles = false);
    bool PlayImpactSound(ServerDE&, HOBJECT source, std::minstd_rand&);
    std::string GetImpactSound(std::minstd_rand&) const;
};
