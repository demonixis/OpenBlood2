#pragma once
#include "client/announcer.h"
#include "client/network_audio.h"
#include "game/original/CommLink.h"
#include "game/original/VoiceMgr.h"
#include "net/player_events.h"
namespace OpenBlood2
{
    class NetworkSpeech final : public VoiceServices
    {
      private:
        NetworkAudio& audio_;
        const Resources& resources_;

      public:
        NetworkSpeech(NetworkAudio& audio, const Resources& resources)
            : audio_(audio), resources_(resources), manager(*this)
        {
        }
        ~NetworkSpeech() override
        {
            communication.Reset(*this);
        }
        bool begin(const Net::Joined&);
        bool receive(const Net::PrivateEvent&, double time, bool external_camera, std::string& error);
        bool update(double time, std::string& error);
        void clear();
        CVoiceMgr manager;
        CCommLink communication;
        ClientAnnouncer announcer;
        float time() const override
        {
            return time_;
        }
        SoundId play_voice(const SoundParameters& p, std::string& error) override
        {
            return audio_.play_voice(resources_, p, error);
        }
        bool stop_voice(SoundId id) override
        {
            return audio_.stop_voice(id);
        }
        bool voice_done(SoundId id, bool& done) const override
        {
            return audio_.voice_done(id, done);
        }
        bool voice_duration(SoundId id, float& duration) const override
        {
            return audio_.voice_duration(id, duration);
        }

      private:
        Net::Joined joined_;
        std::uint64_t sequence_ = 0;
        double clock_ = 0;
        float time_ = 0;
        bool set_time(double, std::string&);
    };
} // namespace OpenBlood2
