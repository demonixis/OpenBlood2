#include "game/server_adapter.h"
namespace OpenBlood2
{
    SoundId ServerAdapter::PlayCinematicSound(HOBJECT owner, const SoundRequest& request, std::string& error)
    {
        trace_service("PlayCinematicSound");
        if (!IsObjectAlive(owner) || !client_resources || (!request.local && !IsObjectAlive(request.speaker)))
        {
            error = "Cinematic sound owner/speaker unavailable";
            return 0;
        }
        DVector position{};
        if (!request.local)
            GetObjectPos(request.speaker, &position);
        return audio.play(*client_resources, owner, request, position, error);
    }
    bool ServerAdapter::IsSoundDone(SoundId id, bool& done)
    {
        trace_service("IsSoundDone");
        const auto* voice = audio.get(id);
        if (!voice)
            return false;
        done = voice->done;
        return true;
    }
    bool ServerAdapter::KillSound(SoundId id)
    {
        trace_service("KillSound");
        return audio.kill(id);
    }
    bool ServerAdapter::advance_audio(double seconds, DVector listener, DRotation rotation)
    {
        std::vector<SoundId> removed;
        for (const auto& [id, entry] : audio.voices())
        {
            if (!IsObjectAlive(entry.owner))
            {
                removed.push_back(id);
                continue;
            }
            if (!entry.request.local)
            {
                auto* voice = audio.get(id);
                if (!IsObjectAlive(voice->request.speaker))
                    voice->done = voice->speaker_lost = true;
                else
                    GetObjectPos(voice->request.speaker, &voice->position);
            }
        }
        for (auto id : removed)
            audio.kill(id);
        return audio.advance(seconds, listener, rotation);
    }
} // namespace OpenBlood2
