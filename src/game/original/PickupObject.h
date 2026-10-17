#pragma once
#include "B2BaseClass.h"
#include <string>
namespace OpenBlood2
{
    // Shared original PickupObject::PickedUp sound branch, also used by KeyPickup.
    PickupReport PlayPickupSound(HOBJECT, std::string_view);
} // namespace OpenBlood2
// Selected original single-player pickup rules; see docs/PICKUP_CONTRACTS.md.
class PickupObject : public B2BaseClass
{
  public:
    PickupObject() : B2BaseClass(OT_MODEL) {}
    OpenBlood2::PickupMessage value;
    std::string file, display_name, pickup_sound = "sounds\\powerups\\powerup4.wav";
    std::string pickup_target, pickup_command, error;
    std::int32_t resource_id = 0;
    bool picked_up = false, rotate_fx = false;
    std::uint64_t attempts = 0, sound_requests = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    HOBJECT touching_ = nullptr;
};
