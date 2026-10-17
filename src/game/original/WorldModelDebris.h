#pragma once
#include "DestructableBrush.h"
#include "Rotating.h"
class WorldModelDebris : public CDestructableBrush
{
  public:
    WorldModelDebris();
    bool Start();
    Rotating rotating;
    bool on = false, ready = false, failed = false;
    DVector initial_velocity{};
    float gravity_multiplier = 1;
    unsigned starts = 0, landings = 0;
    std::string error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    bool Fail(const char*);
    void Update();
};
