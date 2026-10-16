#pragma once
#include "Trigger.h"

// Selected original ExitTrigger callbacks; save/load stays unsupported.
class ExitTrigger : public Trigger
{
  public:
    OpenBlood2::WorldExit exit;
    std::string start_point;
    DRESULT last_delivery = DE_NOTFOUND;
    std::uint64_t attempts = 0, deliveries = 0;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    void SendMessages() override;
    void SendMessage(unsigned, const std::string&) override
    {
        SendMessages();
    }
};
