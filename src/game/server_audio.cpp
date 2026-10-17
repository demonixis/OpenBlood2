#include "game/server_adapter.h"
namespace OpenBlood2
{
    SoundId ServerAdapter::PlayObjectLoopSound(HOBJECT owner, const SoundRequest& request, std::string& error)
    {
        trace_service("PlayObjectLoopSound");
        if (!IsObjectAlive(owner) || !client_resources || request.speaker != owner ||
            request.space != SoundSpace::Spatial || !request.loop)
        {
            error = "Object loop requires a live attached owner and a spatial looping request";
            return 0;
        }
        DVector position{};
        GetObjectPos(owner, &position);
        return audio.play(*client_resources, owner, request, position, error);
    }
    SoundId ServerAdapter::PlayCharacterSound(HOBJECT object, const SoundRequest& request, bool retain,
                                              std::string& error)
    {
        trace_service("PlayCharacterSound");
        if (!IsObjectAlive(object) || !client_resources || request.speaker != object ||
            request.space != SoundSpace::Spatial || request.loop)
        {
            error = "Character sound requires a live attached owner and a spatial one-shot request";
            return 0;
        }
        DVector position{};
        GetObjectPos(object, &position);
        const auto id = audio.play(*client_resources, object, request, position, error);
        if (auto* voice = audio.get(id))
            voice->auto_remove = !retain;
        return id;
    }
    SoundId ServerAdapter::PlayCinematicSound(HOBJECT owner, const SoundRequest& request, std::string& error)
    {
        trace_service("PlayCinematicSound");
        if (!IsObjectAlive(owner) || !client_resources || request.space == SoundSpace::Ambient ||
            (request.space == SoundSpace::Local && request.speaker) ||
            (request.space != SoundSpace::Local && !IsObjectAlive(request.speaker)))
        {
            error = "Cinematic sound owner/speaker unavailable";
            return 0;
        }
        DVector position{};
        if (request.space != SoundSpace::Local)
            GetObjectPos(request.speaker, &position);
        return audio.play(*client_resources, owner, request, position, error);
    }
    SoundId ServerAdapter::PlaySoundAt(HOBJECT owner, const SoundRequest& request, DVector position, std::string& error)
    {
        trace_service("PlaySoundAt");
        if (!IsObjectAlive(owner) || !client_resources || request.speaker || request.space == SoundSpace::Local)
        {
            error = "Fixed sound requires a live owner and ambient/spatial request without attachment";
            return 0;
        }
        return audio.play(*client_resources, owner, request, position, error);
    }
    SoundId ServerAdapter::PlayDetachedSound(HOBJECT source, const SoundRequest& request, DVector position,
                                             std::string& error)
    {
        trace_service("PlayDetachedSound");
        if (request.loop || request.space != SoundSpace::Spatial)
        {
            error = "Detached sound requires a non-looping spatial request";
            return 0;
        }
        const auto id = PlaySoundAt(source, request, position, error);
        if (auto* voice = audio.get(id))
            voice->world_owned = voice->auto_remove = true;
        return id;
    }
    SoundId ServerAdapter::PlayWorldSound(HOBJECT source, const SoundRequest& request, bool file_stream,
                                          std::string& error)
    {
        trace_service("PlayWorldSound");
        if (!IsObjectAlive(source) || !client_resources || request.speaker || request.space == SoundSpace::Local ||
            !request.loop)
        {
            error = "World sound requires a live source and an unattached ambient/spatial loop";
            return 0;
        }
        if (file_stream)
        {
            error = "FileStream/TIMESYNC playback is not implemented";
            return 0;
        }
        DVector position{};
        GetObjectPos(source, &position);
        const auto id = audio.play(*client_resources, source, request, position, error);
        if (auto* voice = audio.get(id))
            voice->world_owned = true;
        return id;
    }
    bool ServerAdapter::RandomInt(int low, int high, int& value)
    {
        trace_service("RandomInt");
        if (low > high)
            return false;
        value = std::uniform_int_distribution<int>(low, high)(random_);
        return true;
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
    bool ServerAdapter::GetSoundDuration(SoundId id, float& duration)
    {
        trace_service("GetSoundDuration");
        duration = 0;
        const auto* voice = audio.get(id);
        if (!voice || !voice->clip)
            return false;
        duration = static_cast<float>(voice->clip->duration());
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
            if (!entry.world_owned && !IsObjectAlive(entry.owner))
            {
                removed.push_back(id);
                continue;
            }
            if (entry.request.speaker != nullptr)
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
        const bool ok = audio.advance(seconds, listener, rotation);
        removed.clear();
        for (const auto& [id, entry] : audio.voices())
            if (entry.auto_remove && entry.done)
                removed.push_back(id);
        for (auto id : removed)
            audio.kill(id);
        return ok;
    }
} // namespace OpenBlood2
