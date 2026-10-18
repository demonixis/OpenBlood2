#pragma once
#include "game/world_preparation.h"
namespace OpenBlood2
{
    class GameSession;
    // Explicit CPU checks shared by fresh startup and NEW_LEVEL; never advance simulation.
    bool check_world_initialization(GameSession&, WorldValidation&, std::string&);
    bool check_world_presentation(GameSession&, WorldValidation&, std::string&);
} // namespace OpenBlood2
