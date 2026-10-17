#pragma once
#include "B2BaseClass.h"
#include "engine/point_light.h"
#include "game/weapon_messages.h"

// Selected original CInvFlashlight; owns only its checked server effect child.
class CInvFlashlight
{
  public:
    explicit CInvFlashlight(HOBJECT owner) : owner(owner) {}
    ~CInvFlashlight();
    CInvFlashlight(const CInvFlashlight&) = delete;
    CInvFlashlight& operator=(const CInvFlashlight&) = delete;
    bool CreateLight();
    bool AddItem(DBYTE);
    bool ActivateItem();
    bool Update();
    bool SetActive(bool);
    HOBJECT owner = nullptr, light = nullptr;
    DBYTE count = 100;
    float remaining = 120;
    bool active = false, update_ok = true;
    std::uint64_t updates = 0, actions = 0;
    std::string error;
};
// Selected original client CFlashlightFX. An owned value replaces client OT_LIGHT.
class CFlashlightFX
{
  public:
    bool Update(ServerDE&, HOBJECT, HOBJECT);
    OpenBlood2::PointLight Current(ServerDE&) const;
    HOBJECT source = nullptr, owner = nullptr;
    OpenBlood2::PointLight light;
    OpenBlood2::SegmentHit hit;
    OpenBlood2::SegmentResult result = OpenBlood2::SegmentResult::Miss;
    DVector shade_position{}; // Source queries owner-position minus hit, not the hit point.
    float distance = 0, minimum_intensity = 132.f / 255;
    bool visible = false, shade_available = false;
    std::uint64_t updates = 0;
    std::string error;
};
