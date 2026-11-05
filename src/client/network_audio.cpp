#include "client/network_audio.h"
#include <algorithm>
namespace OpenBlood2
{
    void NetworkAudio::clear()
    {
        mixer.clear();
        voices_.clear();
    }
    bool NetworkAudio::begin(const Net::Joined& joined)
    {
        if (!joined.world || !joined.player || !joined.object || !joined.object.generation || joined_.world)
            return false;
        clear();
        joined_ = joined;
        sequence_ = high_id_ = 0;
        return true;
    }
    bool NetworkAudio::receive(const Net::AudioEvent& event, const Resources& resources, std::string& error)
    {
        error.clear();
        if (!joined_.world || !Net::valid_audio(event) || event.world != joined_.world ||
            event.player != joined_.player)
        {
            error = "Invalid audio recipient/world";
            return false;
        }
        if (event.sequence <= sequence_)
            return true;
        if (sequence_ == UINT64_MAX || event.sequence != sequence_ + 1)
        {
            error = "Reliable audio event gap";
            return false;
        }
        if (const auto* sound = std::get_if<Net::SoundStart>(&event.value))
        {
            if (sound->id <= high_id_)
            {
                error = "Reused/retired sound identity";
                return false;
            }
            AudioVoice voice;
            SoundRequest request;
            static_cast<SoundParameters&>(request) = sound->parameters;
            if (!mixer.prepare_voice(resources, request, sound->position, voice, error))
                return false;
            if (voice.clip->rate != sound->rate || voice.clip->channels != sound->channels ||
                voice.clip->frames() != sound->frames)
            {
                error = "Network sound asset metadata mismatch";
                return false;
            }
            // Network latency can leave a naturally completed server voice still
            // playing here. Only such retired tails can be displaced by new starts.
            if (voices_.size() >= 128)
            {
                const auto tail =
                    std::find_if(voices_.begin(), voices_.end(), [](const auto& p) { return p.second.retired; });
                if (tail == voices_.end())
                {
                    error = "Network audio voice capacity exhausted";
                    return false;
                }
                mixer.kill(tail->first);
                voices_.erase(tail);
                ++displaced;
            }
            voice.id = sound->id;
            voice.cursor = sound->cursor;
            voice.loops = sound->loops;
            voice.world_owned = true;
            mixer.voices_.emplace(sound->id, std::move(voice));
            voices_.emplace(sound->id, Entry{sound->owner, sound->speaker, false});
            high_id_ = sound->id;
            ++started;
        }
        else
        {
            const auto& end = std::get<Net::SoundEnd>(event.value);
            const auto found = voices_.find(end.id);
            if (found == voices_.end() || found->second.retired || (end.completed && mixer.get(end.id)->request.loop))
            {
                error = "Unknown/retired network sound stop";
                return false;
            }
            if (end.completed)
            {
                found->second.retired = true;
                ++completed;
            }
            else
            {
                mixer.kill(end.id);
                voices_.erase(found);
                ++stopped;
            }
        }
        sequence_ = event.sequence;
        return true;
    }
    bool NetworkAudio::advance(double seconds, const ClientWorld& world, DVector listener, DRotation rotation)
    {
        for (const auto& [id, entry] : voices_)
            if (entry.speaker && world.frame().world == joined_.world)
                if (const auto* object = world.get(entry.speaker))
                    mixer.get(id)->position = object->pose.position;
        const bool ok = mixer.advance(seconds, listener, rotation);
        std::erase_if(voices_,
                      [&](const auto& pair)
                      {
                          if (!pair.second.retired || !mixer.get(pair.first)->done)
                              return false;
                          mixer.kill(pair.first);
                          return true;
                      });
        return ok;
    }
} // namespace OpenBlood2
