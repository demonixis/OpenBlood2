#include "game/menu_audio.h"
namespace OpenBlood2
{
    bool MenuAudio::validate_resources(std::string& failure)
    {
        const auto& assets = menu_resources(menu_edition(resources_));
        // Validate through the real bounded decoder/cache, without output or retained voices.
        const auto load = [&](const char* name, bool stream)
        {
            SoundRequest request;
            request.resource = std::string("sounds/interface/mainmenus/") + name;
            request.space = SoundSpace::Local;
            request.streaming = request.loop = stream;
            request.reverb_requested = false;
            const auto id = mixer.play(resources_, nullptr, request, {}, failure);
            if (!id)
            {
                failure = request.resource + ": " + failure;
                return false;
            }
            mixer.kill(id); // No advance/output: only checked decoding, then cache reuse at real playback.
            return true;
        };
        for (const auto* sound : assets.ambience)
            if (!load(sound, true))
                return false;
        for (const auto* sound : assets.cues)
            if (!load(sound, false))
                return false;
        failure.clear();
        return true;
    }
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
            const auto& assets = menu_resources(menu_edition(resources_));
            ambient = start(assets.ambience[random_state >> 30], true);
        }
        else if (ambient)
        {
            mixer.kill(ambient);
            ambient = 0;
        }
    }
    void MenuAudio::play(MenuCue cue)
    {
        const auto& assets = menu_resources(menu_edition(resources_));
        start(assets.cues[cue == MenuCue::Select ? 0 : cue == MenuCue::Enter ? 1 : 2], false);
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
