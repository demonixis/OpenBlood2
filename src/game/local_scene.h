#pragma once
#include "app/client_scene.h"
namespace OpenBlood2
{
    class GameSession;
    // Compatibility bridge for immediate solo presentation; never owns the inputs.
    ClientScene local_scene(const GameSession&, const ClientWorld* = nullptr, const ClientEffects* = nullptr);
} // namespace OpenBlood2
