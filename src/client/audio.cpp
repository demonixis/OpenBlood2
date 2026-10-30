#include "client/audio.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
namespace OpenBlood2
{
    SoundId ClientAudio::play_effect(const Resources& resources, const SoundParameters& parameters, DVector position,
                                     std::string& error)
    {
        error.clear();
        // Independent of server allocations, and never reused after a world/session reset.
        static SoundId next_effect = client_sound_id_begin;
        AudioVoice voice;
        SoundRequest request;
        static_cast<SoundParameters&>(request) = parameters;
        if (!frame_.world)
            error = "Client cosmetic sound has no world";
        else if (parameters.loop || parameters.streaming || parameters.space != SoundSpace::Spatial)
            error = "Client sound requires a buffered spatial one-shot";
        else if (mixer.voices_.size() >= 128 || next_effect == std::numeric_limits<SoundId>::max())
            error = "Client cosmetic sound capacity exhausted";
        else if (!mixer.prepare_voice(resources, request, position, voice, error))
        {
            ++effects_failed;
            return 0;
        }
        if (!error.empty())
        {
            ++effects_failed;
            return 0;
        }
        const auto id = next_effect++;
        voice.id = id;
        voice.world_owned = voice.auto_remove = true;
        mixer.voices_.emplace(id, std::move(voice));
        ++effects_started;
        return id;
    }
    bool ClientAudio::advance(double seconds, DVector listener, DRotation rotation)
    {
        const bool ok = mixer.advance(seconds, listener, rotation);
        std::erase_if(mixer.voices_, [](const auto& pair)
                      { return pair.first >= client_sound_id_begin && pair.second.auto_remove && pair.second.done; });
        return ok;
    }
    const ClientSound* ClientAudio::descriptor(SoundId id) const
    {
        const auto found = std::find_if(frame_.sounds.begin(), frame_.sounds.end(),
                                        [id](const auto& sound) { return sound.id == id; });
        return found == frame_.sounds.end() ? nullptr : &*found;
    }
    bool ClientAudio::begin(std::uint64_t world)
    {
        if (!world || world <= frame_.world)
            return false;
        mixer.clear();
        frame_ = {};
        frame_.world = world;
        high_id_ = 0;
        effects_started = effects_failed = effects_displaced = 0;
        return true;
    }
    bool ClientAudio::receive(ClientAudioFrame&& next, const Resources& resources, std::string& error)
    {
        error.clear();
        if (!next.world || next.world != frame_.world || !next.sequence || next.sequence <= frame_.sequence ||
            next.sounds.size() > 128)
        {
            error = "Invalid/stale client audio frame";
            return false;
        }
        const auto identity = [](ObjectId id)
        { return id ? (id.generation && id.slot < ClientWorld::object_limit) : !id.generation; };
        std::set<SoundId> ids;
        SoundId high = high_id_;
        // Validate identities and immutable descriptor values before loading any new resources.
        for (const auto& sound : next.sounds)
        {
            if (!sound.id || sound.id >= client_sound_id_begin || !ids.insert(sound.id).second ||
                !identity(sound.owner) || !identity(sound.speaker) || !physics_vector(sound.position) ||
                !std::isfinite(sound.cursor) || sound.cursor < 0 || !sound.frames ||
                sound.frames > 128u * 1024 * 1024 || sound.cursor > double(sound.frames) || sound.rate < 8000 ||
                sound.rate > 96000 || sound.channels < 1 || sound.channels > 2 || sound.error.size() > 1024 ||
                sound.error.find('\0') != std::string::npos)
            {
                error = "Invalid/duplicate client sound descriptor";
                return false;
            }
            if (const auto* voice = mixer.get(sound.id))
            {
                if (static_cast<const SoundParameters&>(voice->request) != sound.parameters ||
                    voice->clip->rate != sound.rate || voice->clip->channels != sound.channels ||
                    voice->clip->frames() != sound.frames)
                {
                    error = "Changed immutable client sound descriptor";
                    return false;
                }
            }
            else if (sound.id <= high_id_)
            {
                error = "Retired client sound identity";
                return false;
            }
            high = std::max(high, sound.id);
        }
        std::map<SoundId, AudioVoice> prepared;
        for (const auto& sound : next.sounds)
            if (!mixer.get(sound.id))
            {
                SoundRequest request;
                static_cast<SoundParameters&>(request) = sound.parameters;
                AudioVoice voice;
                if (!mixer.prepare_voice(resources, request, sound.position, voice, error))
                    return false;
                if (voice.clip->rate != sound.rate || voice.clip->channels != sound.channels ||
                    voice.clip->frames() != sound.frames)
                {
                    error = "Client sound asset metadata mismatch";
                    return false;
                }
                voice.id = sound.id;
                voice.cursor = sound.cursor;
                voice.loops = sound.loops;
                prepared.emplace(sound.id, std::move(voice));
            }
        // Admission is atomic for voices/frame identity. Existing voice addresses stay stable.
        std::erase_if(mixer.voices_, [&](const auto& pair)
                      { return pair.first < client_sound_id_begin && !ids.contains(pair.first); });
        // A local cosmetic burst cannot prevent admission of authoritative logical audio.
        // Keep the existing 128-voice mixer bound; displace the oldest local one-shots first.
        auto excess = mixer.voices_.size() + prepared.size() > 128 ? mixer.voices_.size() + prepared.size() - 128 : 0;
        for (auto it = mixer.voices_.lower_bound(client_sound_id_begin); excess && it != mixer.voices_.end(); --excess)
        {
            it = mixer.voices_.erase(it);
            ++effects_displaced;
        }
        mixer.voices_.merge(prepared);
        for (const auto& sound : next.sounds)
        {
            auto& voice = *mixer.get(sound.id);
            voice.position = sound.position;
            voice.done = voice.done || sound.done; // Never replay a client voice that finished ahead of simulation.
            voice.speaker_lost = sound.speaker_lost;
            voice.world_owned = sound.world_owned;
            voice.auto_remove = sound.auto_remove;
            if (!sound.error.empty())
                voice.error = sound.error;
        }
        high_id_ = high;
        frame_ = std::move(next);
        return true;
    }
} // namespace OpenBlood2
