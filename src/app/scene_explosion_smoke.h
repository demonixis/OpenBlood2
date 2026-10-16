#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool prepare_scene_explosion_smoke(GameSession&, ObjectId&);
    bool activate_scene_explosion_smoke(GameSession&, ObjectId);
    bool check_scene_explosion_smoke(GameSession&, ObjectId);
} // namespace OpenBlood2
