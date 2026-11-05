#include "client/network_speech.h"
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
    bool NetworkSpeech::receive(const Net::PrivateEvent& event, double now, bool external_camera, std::string& error)
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
        if (const auto* start = std::get_if<BloodbathStart>(&event.value))
            communication.Start(*this, announcer.bloodbath(*start, resources_), false);
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
        communication.Update(*this, manager);
        return true;
    }
} // namespace OpenBlood2
