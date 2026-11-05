#include "client/network_audio.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    void NetworkAudio::clear()
    {
        mixer.clear();
        voices_.clear();
        local_voices_.clear();
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
            if (!mixer.prepare_voice(resources, request, sound->position, voice, error, true))
                return false;
            if (voice.clip->rate != sound->rate || voice.clip->channels != sound->channels ||
                voice.clip->frames() != sound->frames)
            {
                error = "Network sound asset metadata mismatch";
                return false;
            }
            // Prefer retiring delayed server tails, then preempt local speech.
            // Never displace another active server voice or finish speech early.
            if (mixer.voices_.size() >= 128)
            {
                const auto tail =
                    std::find_if(voices_.begin(), voices_.end(), [](const auto& p) { return p.second.retired; });
                if (tail != voices_.end())
                {
                    mixer.kill(tail->first);
                    voices_.erase(tail);
                    ++displaced;
                }
                else if (const auto local = mixer.voices_.lower_bound(client_sound_id_begin);
                         local != mixer.voices_.end())
                {
                    const auto found = local_voices_.find(local->first);
                    if (found == local_voices_.end())
                    {
                        error = "Unowned local playback voice";
                        return false;
                    }
                    auto& retained = found->second;
                    const auto& source = local->second;
                    retained.failed = !source.error.empty();
                    retained.completion_frame = double(mixer.instruments.sample_clock()) +
                                                std::ceil(std::max(0., (double(source.clip->frames()) - source.cursor) *
                                                                           AudioMixer::rate / source.clip->rate));
                    mixer.kill(local->first);
                    ++local_displaced;
                }
                else
                {
                    error = "Network audio voice capacity exhausted";
                    return false;
                }
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
    SoundId NetworkAudio::play_voice(const Resources& resources, const SoundParameters& parameters, std::string& error)
    {
        error.clear();
        static SoundId next_local = client_sound_id_begin;
        if (!joined_.world || parameters.space != SoundSpace::Local || parameters.loop || mixer.voices_.size() >= 128 ||
            local_voices_.size() >= 128 || next_local == UINT64_MAX)
        {
            error = "Invalid or over-capacity local retained voice";
            ++local_failed;
            return 0;
        }
        SoundRequest request;
        static_cast<SoundParameters&>(request) = parameters;
        AudioVoice voice;
        if (!mixer.prepare_voice(resources, request, {}, voice, error, true))
        {
            ++local_failed;
            return 0;
        }
        const auto id = next_local++;
        voice.id = id;
        voice.world_owned = true;
        local_voices_.emplace(id, LocalVoice{float(voice.clip->duration())});
        mixer.voices_.emplace(id, std::move(voice));
        ++local_started;
        return id;
    }
    bool NetworkAudio::stop_voice(SoundId id)
    {
        if (!local_voices_.erase(id))
            return false;
        mixer.kill(id);
        return true;
    }
    bool NetworkAudio::voice_done(SoundId id, bool& done) const
    {
        const auto found = local_voices_.find(id);
        if (found == local_voices_.end() || found->second.failed)
            return false;
        if (const auto* voice = mixer.get(id))
        {
            if (!voice->error.empty())
                return false;
            done = voice->done;
        }
        else
            done = double(mixer.instruments.sample_clock()) >= found->second.completion_frame;
        return true;
    }
    bool NetworkAudio::voice_duration(SoundId id, float& duration) const
    {
        duration = 0;
        const auto found = local_voices_.find(id);
        if (found == local_voices_.end())
            return false;
        duration = found->second.duration;
        return true;
    }
} // namespace OpenBlood2
