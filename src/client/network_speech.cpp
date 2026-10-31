#include "client/network_speech.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool NetworkSpeech::begin(const Net::Joined& joined)
    {
        if (!joined.world || !joined.player || !joined.object || !joined.object.generation ||
            (joined_.world && joined.world <= joined_.world))
            return false;
        clear();
        joined_ = joined;
        sequence_ = 0;
        clock_ = time_ = 0;
        announced_change_ = false;
        manager.Init(resources_);
        return true;
    }
    void NetworkSpeech::clear()
    {
        communication.Reset(*this);
        manager.Reset();
        if (level_bell_)
            audio_.stop_voice(level_bell_);
        level_bell_ = 0;
        level_sound_error.clear();
        for (auto id : flag_voices_)
            audio_.stop_voice(id);
        flag_voices_.clear();
        flag_sound_error.clear();
        last_need_flag_ = 0;
        notices_.clear();
        known_players_.clear();
    }
    void NetworkSpeech::retire_flag_voices()
    {
        std::erase_if(flag_voices_,
                      [&](SoundId id)
                      {
                          bool done = false;
                          if (audio_.voice_done(id, done) && !done)
                              return false;
                          audio_.stop_voice(id);
                          return true;
                      });
    }
    void NetworkSpeech::notice(std::string text)
    {
        if (notices_.size() == 4)
            notices_.erase(notices_.begin());
        notices_.push_back({std::move(text), time_ + 5});
    }
    bool NetworkSpeech::set_time(double now, std::string& error)
    {
        if (!std::isfinite(now) || now < clock_ || now > 1e12)
        {
            error = "Invalid client speech clock";
            return false;
        }
        clock_ = now;
        time_ = float(now);
        return true;
    }
    bool NetworkSpeech::receive(const Net::PrivateEvent& event, double now, bool external_camera,
                                std::span<const Net::RosterPlayer> roster, std::string& error)
    {
        error.clear();
        if (!joined_.world || event.owner != joined_ || !Net::valid_private(event))
        {
            error = "Invalid private speech recipient/world";
            return false;
        }
        if (event.sequence <= sequence_)
            return true;
        if (!set_time(now, error))
            return false;
        if (std::holds_alternative<Net::LevelChanging>(event.value))
        {
            if (!announced_change_)
            {
                announced_change_ = true;
                ++level_changes;
                notice(announcer.level_change_notice(resources_));
                SoundParameters sound;
                sound.resource = "sounds/events/bellding1.wav";
                sound.space = SoundSpace::Local;
                sound.priority = 1; // Original SOUNDPRIORITY_MISC_MEDIUM, no stream/loop.
                sound.reverb_requested = false;
                level_bell_ = audio_.play_voice(resources_, sound, level_sound_error);
            }
        }
        else if (const auto* next = std::get_if<Net::PlayerRoster>(&event.value))
        {
            for (const auto& old : known_players_)
                if (old.player != joined_.player && std::none_of(next->players.begin(), next->players.end(),
                                                                 [&](const auto& p) { return p.player == old.player; }))
                {
                    std::string text;
                    if (announcer.player_notice(old.name, false, resources_, text))
                    {
                        notice(std::move(text));
                        ++left_notices;
                    }
                }
            for (const auto& p : next->players)
                if (std::none_of(known_players_.begin(), known_players_.end(),
                                 [&](const auto& old) { return p.player == old.player; }))
                {
                    std::string text;
                    if (announcer.player_notice(p.name, true, resources_, text, p.team))
                    {
                        notice(std::move(text));
                        ++joined_notices;
                    }
                }
            known_players_ = next->players;
        }
        else if (const auto* start = std::get_if<BloodbathStart>(&event.value))
            communication.Start(*this, announcer.bloodbath(*start, resources_), false);
        else if (const auto* frag = std::get_if<Net::FragEvent>(&event.value))
        {
            ++frag_events;
            const auto killer =
                std::find_if(roster.begin(), roster.end(), [&](const auto& p) { return p.object == frag->killer; });
            const auto victim =
                std::find_if(roster.begin(), roster.end(), [&](const auto& p) { return p.object == frag->victim; });
            Conversation line;
            if (killer == roster.end() || victim == roster.end())
                ++missing_players; // Original refuses unnamed kills; no invented identity/text.
            else if (announcer.frag(frag->index, {killer->name, killer->character}, {victim->name, victim->character},
                                    frag->killer == frag->victim, resources_, line))
            {
                if (frag->killer == joined_.object)
                {
                    ++frag_communications;
                    communication.Start(*this, line, false); // Source busy/delayed VoiceMgr stop.
                }
                else
                {
                    ++frag_notices;
                    // Original CMessageMgr defaults: four lines, independent
                    // five-second expiry, oldest replacement; white text output.
                    notice(std::move(line.text));
                }
            }
        }
        else if (const auto* flag = std::get_if<Net::FlagEvent>(&event.value))
        {
            ++flag_events;
            if (flag->kind == CTFEventKind::Grabbed || flag->kind == CTFEventKind::Returned)
            {
                retire_flag_voices();
                SoundParameters sound;
                sound.resource = "sounds_multipatch/CtfFlagGrab.wav";
                sound.space = SoundSpace::Local;
                sound.priority = 1;
                sound.reverb_requested = false;
                if (auto id = audio_.play_voice(resources_, sound, flag_sound_error))
                {
                    flag_voices_.push_back(id); // Overlapping one-shots retain independent completion.
                    ++flag_sounds;
                }
            }
            const bool need = flag->kind == CTFEventKind::NeedFlag;
            if (!need || last_need_flag_ + 20 < time_)
            {
                if (need)
                    last_need_flag_ = time_;
                const auto capturer = std::find_if(roster.begin(), roster.end(),
                                                   [&](const auto& p) { return p.object == flag->capturer; });
                if (flag->kind == CTFEventKind::Captured && capturer == roster.end())
                    ++missing_players;
                else
                {
                    std::string text;
                    if (announcer.flag_notice(flag->kind,
                                              capturer == roster.end() ? std::string_view{} : capturer->name,
                                              capturer == roster.end() ? 0 : capturer->team,
                                              flag->capturer == joined_.object, resources_, text))
                    {
                        notice(std::move(text));
                        ++flag_notices;
                    }
                }
            }
        }
        else if (const auto* finish = std::get_if<FinishHimVoice>(&event.value))
        {
            ++finish_events;
            Conversation line;
            if (announcer.finish(*finish, resources_, line))
                communication.Start(*this, line, false);
        }
        else if (const auto* conversation = std::get_if<Conversation>(&event.value))
        {
            manager.StopAll(); // Even a busy CommLink refusal stops the player's voice.
            communication.Start(*this, *conversation);
        }
        else if (const auto* voice = std::get_if<VoiceEvent>(&event.value))
        {
            if (voice->group)
                manager.PlayEventSound(voice->character, voice->event, external_camera);
            else
                manager.PlayUniqueSound(voice->character, voice->event, external_camera);
        }
        else if (std::holds_alternative<VoiceStop>(event.value))
            manager.StopAll();
        sequence_ = event.sequence;
        return true; // Source refusal/playback failure is diagnostic, not failed event receipt.
    }
    bool NetworkSpeech::update(double now, std::string& error)
    {
        error.clear();
        if (!joined_.world || !set_time(now, error))
            return false;
        std::erase_if(notices_, [&](const auto& notice) { return time_ >= notice.expires; });
        if (level_bell_)
        {
            bool done = false;
            if (!audio_.voice_done(level_bell_, done) || done)
            {
                audio_.stop_voice(level_bell_);
                level_bell_ = 0;
            }
        }
        retire_flag_voices();
        communication.Update(*this, manager);
        return true;
    }
} // namespace OpenBlood2
