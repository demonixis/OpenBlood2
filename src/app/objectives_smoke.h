#pragma once
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool objective_smoke(GameSession&);
    bool show_authored_objective_for_smoke(GameSession&, unsigned resource, std::string& error);
}
