#pragma once
#include "B2BaseClass.h"
#include <string>
// Original ObjectDLL/SparksObj; the temporary CClientSparksSFX carrier is a typed publication.
class SparksObj : public B2BaseClass
{
  public:
    bool on = true;
    float count_min = 10, count_max = 30, duration = .4f, emission_radius = .7f;
    float delay_min = .5f, delay_max = 2, max_seconds = 0;
    float on_time = 0, start_time = 0, next_delay = 0;
    std::string sound, error, sound_error;
    std::uint64_t bursts = 0, commands = 0, effect = 0, voice = 0;
    DVector sound_position{}, direction{};

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProp();
    void Update();
};
