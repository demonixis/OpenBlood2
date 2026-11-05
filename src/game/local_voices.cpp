#include "game/local_voices.h"
#include "game/legacy/server_de.h"
namespace OpenBlood2
{
    float LocalVoices::time() const
    {
        return server_.GetTime();
    }
    SoundId LocalVoices::play_voice(const SoundParameters& parameters, std::string& error)
    {
        SoundRequest request;
        static_cast<SoundParameters&>(request) = parameters;
        return server_.PlayCinematicSound(player_, request, error);
    }
    bool LocalVoices::stop_voice(SoundId id)
    {
        return server_.KillSound(id);
    }
    bool LocalVoices::voice_done(SoundId id, bool& done) const
    {
        return server_.IsSoundDone(id, done);
    }
    bool LocalVoices::voice_duration(SoundId id, float& duration) const
    {
        return server_.GetSoundDuration(id, duration);
    }
} // namespace OpenBlood2
