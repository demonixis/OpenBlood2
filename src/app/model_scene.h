#pragma once
#include "engine/model_view.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool update_explosion_model_scene(const GameSession&, ModelView&, std::string&);
    bool update_model_scene(GameSession&, ModelView&, std::string&);
} // namespace OpenBlood2
