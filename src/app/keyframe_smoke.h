#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool prepare_keyframe_smoke(GameSession&, ObjectId&, bool world_path = false);
    bool check_keyframe_smoke(GameSession&, ObjectId, bool world_path = false);
} // namespace OpenBlood2
