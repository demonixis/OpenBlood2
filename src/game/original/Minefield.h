#pragma once
#include "VolumeBrush.h"

// Original VolumeBrushTypes.cpp Minefield callbacks, through typed local messages.
class Minefield : public VolumeBrush
{
  public:
    Minefield()
    {
        info.code = CC_MINEFIELD; // Original constructor; registered/world Code overrides it with CC_DAMAGE.
    }
    void UpdatePhysics(OpenBlood2::ContainerPhysics&) override;
    std::uint64_t explosions = 0;
    std::string error;

  protected:
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    void DamageCharacter(HOBJECT);
};
