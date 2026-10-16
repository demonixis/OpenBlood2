#pragma once
#include "cpp_engineobjects_de.h"
// Original held-model subset. Dropping/pickup spawning and client SFX are not ported.
class CHandWeaponModel : public BaseClass
{
  public:
    CHandWeaponModel() : BaseClass(OT_MODEL) {}
    bool visible = true, first_update = true, left = false, player_owned = false;
    DDWORD type = 0;
    HOBJECT owner = nullptr; // Checked identity replaces the source's borrowed CWeapon pointer.
    void SetVisible(DBOOL value)
    {
        visible = value;
    }

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
};
