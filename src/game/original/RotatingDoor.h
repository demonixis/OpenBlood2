#pragma once
#include "Door.h"
class RotatingDoor : public Door
{
  public:
    DVector rotation_angles{}, rotation_point{}, closed_angles{}, open_angles{}, original_position{}, angles{};
    DVector open_direction{};
    bool push_player_back = true;
    static bool CalcAngle(float&, float initial, float target, int direction, float speed, unsigned wave, float dt);

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    void Move(bool opening) override;
};
