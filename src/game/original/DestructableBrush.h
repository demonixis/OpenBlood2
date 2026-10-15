#pragma once
#include "B2BaseClass.h"
#include "Destructable.h"
class CDestructableBrush : public B2BaseClass
{
  public:
    CDestructableBrush();
    CDestructable m_damage;
    bool destructable = true, pushable = false, box_physics = false, allow_marks = true, fire_through = false;
    float hit_points = 0, mass = 100;
    unsigned surface = 0;
    DBYTE debris_amount = 10;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
};
