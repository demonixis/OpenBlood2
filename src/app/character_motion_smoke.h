#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool prepare_character_motion_smoke(GameSession&, ObjectId&);
    bool check_character_motion_smoke(GameSession&, ObjectId);
} // namespace OpenBlood2
