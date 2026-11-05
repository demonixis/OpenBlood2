#include "client/network_speech.h"
#include <algorithm>
#include <cmath>
namespace OpenBlood2
{
    bool NetworkSpeech::begin(const Net::Joined& joined)
    {
        if (!joined.world || !joined.player || !joined.object || !joined.object.generation || joined_.world)
            return false;
        clear();
        joined_ = joined;
        manager.Init(resources_);
        return true;
    }
    void NetworkSpeech::clear()
    {
        communication.Reset(*this);
        manager.Reset();
        notices_.clear();
        known_players_.clear();
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
        if (const auto* next = std::get_if<Net::PlayerRoster>(&event.value))
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
                    if (announcer.player_notice(p.name, true, resources_, text))
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
        communication.Update(*this, manager);
        return true;
    }
} // namespace OpenBlood2
