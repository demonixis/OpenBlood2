#pragma once
#include "engine/scene_effects.h"
namespace OpenBlood2
{
    class GameSession;
    // Read-only presentation snapshot; does not advance simulation, lights or particles.
    SceneEffects gather_scene_effects(const GameSession&, DVector eye, DRotation view);
} // namespace OpenBlood2
