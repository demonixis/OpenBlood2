#pragma once
#include "Trigger.h"
// Selected original ObjectivesTrigger callbacks; ownership uses std::string.
class ObjectivesTrigger : public Trigger
{
  public:
    OpenBlood2::MissionObjectives objectives;
    std::string sound;
    std::uint64_t deliveries = 0, objective_sound_requests = 0;
    DRESULT last_delivery = DE_NOTFOUND;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    void SendMessages() override;
};
