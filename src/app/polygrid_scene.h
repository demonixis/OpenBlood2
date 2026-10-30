#pragma once
#include "app/client_scene.h"
#include "engine/polygrid_view.h"
namespace OpenBlood2
{
    class GameSession;
    bool update_polygrid_scene(const ClientScene&, PolyGridView&, std::string&);
    bool update_polygrid_scene(GameSession&, PolyGridView&, std::string&);
    bool update_polygrid_scene(const GameSession&, const ClientWorld&, PolyGridView&, std::string&);
} // namespace OpenBlood2
