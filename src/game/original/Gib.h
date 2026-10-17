#pragma once
#include "CharacterNodes.h"
#include "Destructable.h"
// Original AI_Shared limb creation and runtime CGib limb subset.
bool HideLimb(HOBJECT, int, std::string&);
HOBJECT CreateLimb(HOBJECT, int, DVector, std::string&);
class CGib : public B2BaseClass
{
  public:
    CGib();
    bool initialized = false, kickable = true, first_update = true;
    int limb = -1, axis = 0, bounces = 0;
    float rotate_time = 0, remove_time = 0;
    DVector angles{}, angular_velocity{}, last_position{};
    std::uint64_t kicks = 0, impacts = 0, spurt_event = 0;
    bool add_blood_spurt = false;
    OpenBlood2::BodyPlacement placement;
    CDestructable damage;
    std::string error, sound_error, spurt_error;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;
    bool Update();
    void Touch(HOBJECT);
    void BounceSound();
    void RandomRotation();
};
