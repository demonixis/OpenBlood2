#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool prepare_path_manager_smoke(GameSession&, PathList&);
    bool check_path_manager_smoke(GameSession&, const PathList&, bool reloaded);
} // namespace OpenBlood2
