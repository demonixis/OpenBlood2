#pragma once
#include "server_de.h"
// Original ClientShellDLL/CommLink numeric-character path. Clock work is outside drawing.
class CCommLink
{
  public:
    OpenBlood2::Conversation message;
    unsigned level = 0, text_level = 0;
    float start = 0, text_start = 0, length = 0, text_length = 0, text_scroll = 0, ratio = 0, text_ratio = 0;
    OpenBlood2::SoundId voice = 0;
    std::uint64_t received = 0, started = 0, busy = 0, sound_attempts = 0, sound_failures = 0,
                  voice_manager_requests = 0;
    float voice_manager_delay = 0;
    bool play_voice = false;
    std::string portrait, error;
    void Reset(ServerDE&);
    bool Start(ServerDE&, const OpenBlood2::Conversation&);
    void Update(ServerDE&, HOBJECT player);
};
