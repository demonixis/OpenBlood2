#include "CommLink.h"
#include "VoiceMgr.h"
#include <algorithm>
void CCommLink::Reset(OpenBlood2::VoiceServices& services)
{
    if (voice)
        services.stop_voice(voice);
    *this = {};
}
bool CCommLink::Start(OpenBlood2::VoiceServices& services, const OpenBlood2::Conversation& value,
                      bool shell_stopped_voice)
{
    ++received;
    voice_manager_requests += shell_stopped_voice; // Conversation shell StopAll, including a busy refusal.
    if (level)
    {
        ++busy;
        return false;
    }
    if (voice)
        services.stop_voice(voice);
    voice = 0;
    message = value;
    length = std::max(2.5f, static_cast<float>(message.text.size()) / 15.f);
    text_length = length / 1.5f;
    text_scroll = (length - text_length) / 2.f;
    play_voice = message.file != "default.wav";
    start = services.time();
    level = 1;
    static constexpr const char* portraits[] = {"c_comm.pcx", "o_comm.pcx", "i_comm.pcx", "g_comm.pcx", "v_comm.pcx"};
    portrait =
        std::string("interface/commlink/") + (message.character < 5 ? portraits[message.character] : "u_comm.pcx");
    error.clear();
    ++started;
    return true;
}
void CCommLink::Update(OpenBlood2::VoiceServices& services, CVoiceMgr& manager)
{
    if (!level)
        return;
    const auto time = services.time();
    switch (level)
    {
        case 1:
            if (time - start > .2f)
            {
                ratio = 1;
                level = 2;
                start = time;
                text_level = 1;
                text_start = time;
                if (play_voice)
                {
                    ++voice_manager_requests;
                    manager.StopAll();
                    if (voice)
                        services.stop_voice(voice);
                    OpenBlood2::SoundParameters r;
                    r.resource = message.file;
                    r.space = OpenBlood2::SoundSpace::Local;
                    r.streaming = false;
                    r.priority = 2;
                    r.reverb_requested = false;
                    ++sound_attempts;
                    voice = services.play_voice(r, error);
                    if (!voice)
                        ++sound_failures;
                    else
                    {
                        float duration = 3;
                        services.voice_duration(voice, duration);
                        voice_manager_delay = duration + .5f;
                        manager.SetNextPlayTime(voice_manager_delay);
                    }
                }
            }
            else
                ratio = (time - start) / .2f;
            break;
        case 2:
            // Original uses text-derived time, even if the voice is longer or already done.
            if (time - start > length)
            {
                level = 3;
                start = time;
                if (voice)
                    services.stop_voice(voice);
                voice = 0;
            }
            break;
        case 3:
            if (time - start > .2f)
            {
                ratio = 0;
                level = 0;
                start = time;
            }
            else
                ratio = 1 - (time - start) / .2f;
            break;
    }
    switch (text_level)
    {
        case 1:
            if (time - text_start > text_scroll)
            {
                text_level = 2;
                text_start = time;
                text_ratio = 1;
            }
            else
                text_ratio = (time - text_start) / text_scroll;
            break;
        case 2:
            if (time - text_start > text_length)
            {
                text_level = 3;
                text_start = time;
                text_ratio = 0;
            }
            break;
        case 3:
            if (time - text_start > text_scroll)
            {
                text_level = 0;
                text_start = time;
            }
            else
                text_ratio = (time - text_start) / text_scroll;
            break;
    }
}
