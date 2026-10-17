#pragma once
#include "B2BaseClass.h"
#include "engine/audio.h"

// Selected original ObjectDLL/SoundFX; strings are owned values, voices checked identities.
class SoundFX : public B2BaseClass
{
  public:
    enum State : DBYTE
    {
        Off,
        RampUp,
        On,
        RampDown
    };
    bool start_on = true, ambient = false, file_stream = false;
    State state = Off;
    std::string ramp_up, ramp_down, sound_path;
    float outer_radius = 0, inner_radius = 0;
    DBYTE volume = 0, priority = 0;
    OpenBlood2::SoundId sound = 0;
    DVector last_position{};
    std::uint64_t commands = 0, attempts = 0, failures = 0;
    std::string error;
    ~SoundFX() override;
    const char* state_name() const;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    void ReadProp();
    void UpdateSound();
};
