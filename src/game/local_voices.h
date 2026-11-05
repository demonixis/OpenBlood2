#pragma once
#include "client/voice_services.h"
class ServerDE;
namespace OpenBlood2
{
    // Preserve the immediate solo service path and its original sound handles.
    class LocalVoices final : public VoiceServices
    {
      public:
        LocalVoices(ServerDE& server, const HOBJECT& player) : server_(server), player_(player) {}
        float time() const override;
        SoundId play_voice(const SoundParameters&, std::string&) override;
        bool stop_voice(SoundId) override;
        bool voice_done(SoundId, bool&) const override;
        bool voice_duration(SoundId, float&) const override;

      private:
        ServerDE& server_;
        const HOBJECT& player_;
    };
} // namespace OpenBlood2
