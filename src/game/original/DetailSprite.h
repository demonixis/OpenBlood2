#pragma once
#include "B2BaseClass.h"
#include "game/weapon_messages.h"
#include <string>
#include <vector>
// Selected original ObjectDLL/DetailSprite; save/load remain unavailable.
class DetailSprite : public B2BaseClass
{
  public:
    DetailSprite() : B2BaseClass(OT_SPRITE) {}
    DVector scale{1, 1, 1};
    bool flush = false, rotatable = false, chromakey = false, repeat = false;
    unsigned repeat_x = 1, repeat_y = 1;
    float spacing_x = 0, spacing_y = 0;
    std::string filename, error;
    OpenBlood2::SegmentResult flush_result = OpenBlood2::SegmentResult::Miss;
    OpenBlood2::SegmentHit flush_hit;
    std::vector<HOBJECT> children; // Diagnostic handles; original repeats have independent lifetimes.
  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;

  private:
    bool ReadProp();
    bool InitialUpdate();
    bool CreateAnotherSprite(DVector, DRotation);
};
