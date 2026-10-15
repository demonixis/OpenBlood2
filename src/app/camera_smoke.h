#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool finish_camera_intro_for_smoke(GameSession&);
    bool prepare_camera_smoke(GameSession&, ObjectId&);
    bool check_camera_smoke(GameSession&, ObjectId);
} // namespace OpenBlood2
