#pragma once
#include "B2BaseClass.h"
#include "Debris.h"
#include "Destructable.h"
class CDestructableBrush : public B2BaseClass
{
  public:
    CDestructableBrush();
    CDestructable m_damage;
    CDebris debris;
    unsigned GetSurfaceType() const
    {
        return surface ? surface : debris.GetType();
    }
    bool destructable = true, pushable = false, box_physics = false, allow_marks = true, fire_through = false;
    float hit_points = 0, mass = 100;
    unsigned surface = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
};
