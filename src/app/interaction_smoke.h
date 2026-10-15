#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool prepare_interaction_smoke(GameSession&, ObjectId& door);
    bool check_interaction_smoke(GameSession&, ObjectId door);
} // namespace OpenBlood2
