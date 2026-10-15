#pragma once
#include "B2BaseClass.h"
#include <string>

// Selected KeyPickup plus the single-player PickupObject branches it needs.
// General pickups, respawn/bounce FX, localized resource strings remain pending.
class KeyPickup : public B2BaseClass
{
  public:
    KeyPickup() : B2BaseClass(OT_MODEL) {}
    OpenBlood2::KeyItem item;
    std::int32_t resource_id = 0;
    std::string pickup_target, pickup_command, error;
    bool picked_up = false;
    std::uint64_t attempts = 0, sound_requests = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    HOBJECT touching_ = nullptr;
};
