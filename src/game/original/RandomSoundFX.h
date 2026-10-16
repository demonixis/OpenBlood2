#pragma once
#include "B2BaseClass.h"
#include "engine/audio.h"
#include <array>
// Selected original ObjectDLL/RandomSoundFX.h; value-owned resources and checked voice ID.
class RandomSoundFX : public B2BaseClass
{
  public:
    bool on = true, positional = false, sequential = false, loop_sequence = false;
    float position_radius = 0, sound_radius = 1000, min_wait = 60, max_wait = 60;
    DBYTE volume = 100;
    std::array<std::string, 10> sounds;
    unsigned count = 0, current = 0, last_choice = 0;
    OpenBlood2::SoundId sound = 0;
    DVector last_position{};
    std::uint64_t commands = 0, attempts = 0, failures = 0;
    std::string error;
    ~RandomSoundFX() override;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProp();
    bool Update();
    void ScheduleWait();
};
