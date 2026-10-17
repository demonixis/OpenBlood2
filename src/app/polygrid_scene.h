#pragma once
#include "engine/polygrid_view.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool update_polygrid_scene(const GameSession&, PolyGridView&, std::string&);
}
