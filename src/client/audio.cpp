#include "client/audio.h"
#include "engine/collision.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
namespace OpenBlood2
{
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
            if (!sound.id || sound.id == std::numeric_limits<SoundId>::max() || !ids.insert(sound.id).second ||
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
        std::erase_if(mixer.voices_, [&](const auto& pair) { return !ids.contains(pair.first); });
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
