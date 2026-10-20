#pragma once
#include "Trigger.h"
#include <string_view>
namespace OpenBlood2
{
    const char* conversation_text(std::string_view);
}
class ConversationTrigger final : public Trigger
{
  public:
    std::array<std::string, 8> waves;
    unsigned count = 0, current = 0;
    DBYTE character = 0;
    bool repeat_last = false, loop = false;
    std::uint64_t attempts = 0, queued = 0, unmapped = 0;
    DRESULT last_delivery = DE_NOTFOUND;
    std::string error;

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
