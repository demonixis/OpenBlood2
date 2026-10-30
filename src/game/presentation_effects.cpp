#include "game/presentation_effects.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    SceneEffects gather_scene_effects(GameSession& game, DVector eye, DRotation view)
    {
        std::string error;
        if (!game.publish_client_world(error))
        {
            SceneEffects refused;
            refused.invalid_fog = true;
            return refused; // Publication failure remains in the session diagnostic.
        }
        return gather_scene_effects(game.client_session.effects, eye, view);
    }
} // namespace OpenBlood2
