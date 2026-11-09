#pragma once
#include "LegacyEffects.h"
#include "game/legacy_weapon_effect.h"

class CClientWeaponSFX final : public TransientClientSFX
{
  public:
    bool Setup(const OpenBlood2::LegacyWeaponEffect&, std::string& error);
};
