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
    std::string model_file, model_skin; // Original m_hstrFilename/m_hstrSkinName (set props only).
    DDWORD m_dwFlags = 0;               // PickupObject's copy of the create flags.
    bool picked_up = false;
    std::uint64_t attempts = 0, sound_requests = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    // Original ItemPickups.cpp:480-511, then PickupObject's block (PickupSave.cpp).
    void Save(HMESSAGEWRITE);
    void Load(HMESSAGEREAD);
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    HOBJECT touching_ = nullptr;
};
