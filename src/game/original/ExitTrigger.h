#pragma once
#include "Trigger.h"

// Selected original ExitTrigger callbacks; Save/Load follow the original block (save/load lot 7).
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

  private:
    void Save(HMESSAGEWRITE);
    void Load(HMESSAGEREAD);
};
