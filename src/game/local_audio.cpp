#include "game/server_adapter.h"
#include <cmath>
namespace OpenBlood2
{
    bool GameSession::publish_client_audio(std::string& error)
    {
        ClientAudioFrame frame;
        frame.world = client_session.world.frame().world;
        frame.sequence = client_session.audio.frame().sequence + 1;
        const auto identity = [&](HOBJECT handle)
        {
            const auto* record = objects.get(handle);
            return record && record->life == Life::Alive ? record->id : ObjectId{};
        };
        for (const auto& [id, voice] : server.audio.voices())
        {
            ClientSound sound;
            sound.id = id;
            sound.owner = identity(voice.owner);
            sound.speaker = identity(voice.request.speaker);
            sound.parameters = voice.request;
            sound.position = voice.position;
            sound.rate = voice.clip->rate;
            sound.channels = voice.clip->channels;
            sound.frames = voice.clip->frames();
            sound.cursor = voice.cursor;
            sound.loops = voice.loops;
            sound.done = voice.done;
            sound.speaker_lost = voice.speaker_lost;
            sound.world_owned = voice.world_owned;
            sound.auto_remove = voice.auto_remove;
            sound.error = voice.error;
            frame.sounds.push_back(std::move(sound));
        }
        return client_session.audio.receive(std::move(frame), resources, error);
    }
    bool GameSession::advance_audio(double seconds, DVector listener, DRotation rotation)
    {
        const float norm = vector_dot(rotation.m_Vec, rotation.m_Vec) + rotation.m_Spin * rotation.m_Spin;
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 60 || !physics_vector(listener) ||
            !physics_vector(rotation.m_Vec) || !std::isfinite(rotation.m_Spin) || !std::isfinite(norm) ||
            std::abs(norm - 1) > .001f)
            return false;
        // Solo pause freezes both timelines. step_server never consults client/device state.
        if (client_session.audio.mixer.paused())
            return true;
        server.prepare_audio();
        std::string error;
        const bool published = publish_client_audio(error);
        const bool played = published && client_session.audio.advance(seconds, listener, rotation);
        const bool simulated = server.advance_prepared_audio(seconds);
        const bool synchronized = publish_client_audio(error);
        if (!error.empty())
            audio_replica_error = error;
        return published && played && simulated && synchronized;
    }
} // namespace OpenBlood2
