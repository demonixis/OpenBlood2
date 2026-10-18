#include "game/menu_audio.h"
namespace OpenBlood2
{
    void MenuAudio::prune()
    {
        std::vector<SoundId> finished;
        for (const auto& [id, voice] : mixer.voices())
            if (voice.done)
                finished.push_back(id);
        for (auto id : finished)
            mixer.kill(id);
    }
    SoundId MenuAudio::start(const std::string& file, bool loop)
    {
        prune();
        if (mixer.voices().size() >= 16)
        {
            // Rapid navigation keeps recent feedback; never evict the ambience.
            for (const auto& [id, voice] : mixer.voices())
                if (!voice.request.loop)
                {
                    mixer.kill(id);
                    break;
                }
        }
        if (mixer.voices().size() >= 16)
        {
            error = "Menu audio voice limit reached";
            ++failures;
            return 0;
        }
        SoundRequest request;
        request.resource = "sounds/interface/mainmenus/" + file;
        request.space = SoundSpace::Local;
        request.loop = loop;
        request.streaming = loop;
        request.reverb_requested = false;
        std::string failure;
        auto id = mixer.play(resources_, nullptr, request, {}, failure);
        if (id)
            ++starts;
        else
        {
            error = std::move(failure);
            ++failures;
        }
        return id;
    }
    void MenuAudio::focus(bool active)
    {
        if (active == active_)
            return;
        active_ = active;
        if (active)
        {
            random_state = random_state * 1664525u + 1013904223u;
            ambient = start("ambient0" + std::to_string(1 + (random_state >> 30)) + ".wav", true);
        }
        else if (ambient)
        {
            mixer.kill(ambient);
            ambient = 0;
        }
    }
    void MenuAudio::play(MenuCue cue)
    {
        start(cue == MenuCue::Select ? "select01.wav" : cue == MenuCue::Enter ? "enter01.wav" : "esc01.wav", false);
    }
    bool MenuAudio::advance(double seconds)
    {
        if (!mixer.advance(seconds, {}, {}))
        {
            error = mixer.output_error;
            ++failures;
            return false;
        }
        prune();
        return true;
    }
} // namespace OpenBlood2
