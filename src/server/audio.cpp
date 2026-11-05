#include "server/session.h"
#include <algorithm>
namespace OpenBlood2
{
    bool ServerSession::queue_audio(Player& player, const AudioVoice& voice, bool started)
    {
        if (player.audio_failed)
            return false;
        Net::AudioEvent event{world_id_, player.id, player.next_audio, {}};
        if (started)
        {
            if (voice.done || !voice.error.empty())
                return true;
            // Local player-owned sounds are private. A cinematic controller's
            // PLAYSOUND_LOCAL voice remains global, as in the original service.
            const auto source = audio_sources_.find(voice.id);
            if (source == audio_sources_.end())
            {
                player.audio_failed = true;
                return false;
            }
            if (source->second.private_player && source->second.owner != player.object)
                return true;
            Net::SoundStart sound;
            sound.id = voice.id;
            sound.owner = source->second.owner;
            sound.speaker = source->second.speaker;
            sound.parameters = voice.request;
            sound.position = voice.position;
            sound.rate = voice.clip->rate;
            sound.channels = voice.clip->channels;
            sound.frames = voice.clip->frames();
            sound.cursor = voice.cursor;
            sound.loops = voice.loops;
            event.value = std::move(sound);
        }
        else
        {
            if (!player.audio_voices.contains(voice.id))
                return true;
            event.value = Net::SoundEnd{voice.id, voice.done && !voice.request.loop && !voice.speaker_lost &&
                                                      voice.error.empty()};
        }
        if (!player.next_audio || player.audio_events.size() >= Net::audio_queue_limit || !Net::valid_audio(event))
        {
            player.audio_failed = true;
            return false;
        }
        if (started)
            player.audio_voices.insert(voice.id);
        else
            player.audio_voices.erase(voice.id);
        player.audio_events.push_back(std::move(event));
        ++player.next_audio;
        return true;
    }
    void ServerSession::capture_audio(const AudioVoice& voice, bool started)
    {
        if (started)
        {
            const auto identity = [&](HOBJECT handle)
            {
                const auto* object = game_.objects.get(handle);
                return object ? object->id : ObjectId{};
            };
            const auto* owner = game_.objects.get(voice.owner);
            const bool private_player =
                voice.request.space == SoundSpace::Local && owner && dynamic_cast<const CPlayerObj*>(owner->game.get());
            audio_sources_.emplace(voice.id,
                                   AudioSource{identity(voice.owner), identity(voice.request.speaker), private_player});
        }
        for (auto& player : players_)
            if (player.audio_started)
                queue_audio(player, voice, started);
        if (!started)
            audio_sources_.erase(voice.id);
    }
    bool ServerSession::begin_audio(std::uint64_t id, std::string& error)
    {
        error.clear();
        const auto found = std::find_if(players_.begin(), players_.end(), [&](const auto& p) { return p.id == id; });
        if (found == players_.end() || found->audio_started)
        {
            error = "Audio recipient is unavailable/already started";
            return false;
        }
        found->audio_started = true;
        // Late admission starts only currently playing voices at their logical
        // cursor. No journal from before the client's baseline is replayed.
        for (const auto& [voice_id, voice] : game_.server.audio.voices())
            if (!queue_audio(*found, voice, true))
            {
                error = "Initial audio state exceeds the recipient budget";
                return false;
            }
        return true;
    }
    bool ServerSession::take_audio_events(std::uint64_t id, std::vector<Net::AudioEvent>& out, std::string& error)
    {
        error.clear();
        const auto found = std::find_if(players_.begin(), players_.end(), [&](const auto& p) { return p.id == id; });
        if (found == players_.end() || !found->audio_started || found->audio_failed)
        {
            error = "Audio recipient/history is unavailable or over capacity";
            return false;
        }
        out.clear();
        out.swap(found->audio_events);
        return true;
    }
} // namespace OpenBlood2
