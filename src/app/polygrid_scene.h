#pragma once
#include "engine/polygrid_view.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool update_polygrid_scene(GameSession&, PolyGridView&, std::string&);
    bool update_polygrid_scene(const GameSession&, const ClientWorld&, PolyGridView&, std::string&);
} // namespace OpenBlood2
