#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool prepare_pickup_smoke(GameSession&, ObjectId&);
    bool collect_pickup_smoke(GameSession&, ObjectId);
} // namespace OpenBlood2
