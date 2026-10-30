#pragma once
#include "B2BaseClass.h"
#include "client/flashlight.h"
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
