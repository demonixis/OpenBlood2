#pragma once
#include "game/world_preparation.h"
namespace OpenBlood2
{
    class GameSession;
    // Explicit CPU checks shared by fresh startup and NEW_LEVEL; never advance simulation.
    bool check_world_initialization(GameSession&, WorldValidation&, std::string&);
    bool check_world_presentation(GameSession&, WorldValidation&, std::string&);
    // Restored owner-bound publications (save_load_contracts lot 13): every live sky object is
    // registered once and holds at most one slot, every PolyGrid publishes exactly once, and every
    // VolumeBrush surface is a live, published PolyGrid owned by exactly one volume.
    bool check_restored_publications(const GameSession&, std::string&);
} // namespace OpenBlood2
