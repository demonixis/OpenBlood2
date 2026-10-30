#pragma once
#include "client/effect_scene.h"
namespace OpenBlood2
{
    class GameSession;
    // Standalone local helper: publishes before invoking the independent client consumer.
    SceneEffects gather_scene_effects(GameSession&, DVector eye, DRotation view);
} // namespace OpenBlood2
