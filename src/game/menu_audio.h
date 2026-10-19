#pragma once
#include "engine/audio.h"
#include "game/menu.h"
namespace OpenBlood2
{
    class MenuAudio
    {
      public:
        explicit MenuAudio(const Resources& resources) : resources_(resources) {}
        bool validate_resources(std::string& error);
        void focus(bool active);
        void play(MenuCue);
        bool advance(double seconds);
        AudioMixer mixer;
        std::string error;
        SoundId ambient = 0;
        std::uint64_t starts = 0, failures = 0;
        std::uint32_t random_state = 0x4d454e55; // Private menu-only random stream; injectable in tests.
      private:
        const Resources& resources_;
        bool active_ = false;
        void prune();
        SoundId start(const std::string&, bool loop);
    };
} // namespace OpenBlood2
