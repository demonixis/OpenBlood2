#pragma once
#include "B2BaseClass.h"
#include <string>
namespace OpenBlood2
{
    // Shared original PickupObject::PickedUp sound branch, also used by KeyPickup.
    PickupReport PlayPickupSound(HOBJECT, std::string_view);
} // namespace OpenBlood2
// Selected original single-player pickup rules; see Documentation/contracts/pickup_contracts.md.
class PickupObject : public B2BaseClass
{
  public:
    PickupObject() : B2BaseClass(OT_MODEL) {}
    OpenBlood2::PickupMessage value;
    std::string file, display_name, pickup_sound = "sounds\\powerups\\powerup4.wav";
    std::string pickup_target, pickup_command, error;
    std::int32_t resource_id = 0;
    bool picked_up = false, rotate_fx = false;
    // Original m_bBounce (item/weapon constructors) and the SpawnItem state it sets on
    // string creation. No update is scheduled afterwards, so the source spring never runs.
    bool bounce = false, bouncing = false;
    DVector rest_position{};
    float bounce_acceleration = 0;
    DDWORD m_dwFlags = 0; // Original copy of the create flags (PostPropRead), saved.
    std::uint64_t attempts = 0, sound_requests = 0;
    bool IsOfferingTo(HOBJECT receiver) const
    {
        return touching_ == receiver && !picked_up;
    }

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    void SpawnBounce();
    // Original PickupObject.cpp:617-674 (PickupSave.cpp).
    void Save(HMESSAGEWRITE);
    void Load(HMESSAGEREAD);
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    HOBJECT touching_ = nullptr;
};
