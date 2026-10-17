#pragma once
#include "B2BaseClass.h"

// Selected original CInvMedkit. Owned by CInventoryMgr; no engine child or list node.
class CInvMedkit
{
  public:
    void Init(HOBJECT owner)
    {
        owner_ = owner;
        count = 100;
    }
    bool AddItem(DBYTE);
    int Activate();
    bool ActivateItem()
    {
        return Activate() != 0;
    }
    DBYTE count = 100;
    float requested_gain = 0, actual_gain = 0;
    int activation_result = 0;
    std::uint64_t attempts = 0, action_messages = 0;
    std::string error;

  private:
    HOBJECT owner_ = nullptr; // Checked non-owning identity; never serialized.
};
