#pragma once
#include "engine/sprite_view.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool update_sprite_scene(const GameSession&, SpriteView&, std::string& error);
}
