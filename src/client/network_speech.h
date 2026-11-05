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
        bool receive(const Net::PrivateEvent&, double time, bool external_camera, std::span<const Net::RosterPlayer>,
                     std::string& error);
        bool update(double time, std::string& error);
        void clear();
        CVoiceMgr manager;
        CCommLink communication;
        ClientAnnouncer announcer;
        struct Notice
        {
            std::string text;
            float expires = 0;
        };
        const std::vector<Notice>& notices() const
        {
            return notices_;
        }
        std::uint64_t frag_events = 0, frag_communications = 0, frag_notices = 0, missing_players = 0,
                      finish_events = 0;
        std::uint64_t joined_notices = 0, left_notices = 0, level_changes = 0;
        std::string level_sound_error;
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
        SoundId level_bell_ = 0;
        bool announced_change_ = false;
        std::uint64_t sequence_ = 0;
        double clock_ = 0;
        float time_ = 0;
        bool set_time(double, std::string&);
        std::vector<Notice> notices_;
        std::vector<Net::RosterPlayer> known_players_;
        void notice(std::string);
    };
} // namespace OpenBlood2
