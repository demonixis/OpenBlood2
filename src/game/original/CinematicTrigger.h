#pragma once
#include "B2BaseClass.h"
#include "engine/audio.h"
#include <array>
class CinematicTrigger : public B2BaseClass
{
  public:
    struct Cue
    {
        float delay = 0;
        std::string sound, speaker, target, message;
    };
    std::array<Cue, 10> cues;
    std::size_t current = 0;
    OpenBlood2::SoundId sound = 0;
    std::uint64_t starts = 0, messages = 0, failures = 0, commands = 0;
    HOBJECT last_speaker = nullptr;
    std::string error;
    ~CinematicTrigger() override;

  protected:
    DDWORD EngineMessageFn(DDWORD, void*, DFLOAT) override;
    DDWORD ObjectMessageFn(HOBJECT, DDWORD, HMESSAGEREAD) override;

  private:
    bool ReadProp();
    bool Update();
    void StartSound();
    HOBJECT PlayedBy(const std::string&);
};
