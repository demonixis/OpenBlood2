#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool prepare_trigger_smoke(GameSession&, ObjectId& door, ObjectId& trigger);
    bool activate_trigger_smoke(GameSession&, ObjectId trigger, SoundId& denial_voice);
    bool check_trigger_audio_smoke(GameSession&, SoundId denial_voice);
} // namespace OpenBlood2
