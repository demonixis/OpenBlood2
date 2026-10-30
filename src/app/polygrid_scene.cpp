#include "polygrid_scene.h"
#include "client/effect_scene.h"
namespace OpenBlood2
{
    bool update_polygrid_scene(GameSession& game, PolyGridView& scene, std::string& error)
    {
        return game.publish_client_world(error) && update_polygrid_scene(game, game.client_session.world, scene, error);
    }
    bool update_polygrid_scene(const GameSession& game, const ClientWorld& world, PolyGridView& scene,
                               std::string& error)
    {
        error.clear();
        scene.begin_frame();
        if (world.frame().world != game.client_session.effects.frame().world)
        {
            error = "PolyGrid effects world mismatch";
            return false;
        }
        return append_effect_polygrids(game.client_session.effects, scene, game.resources, error);
    }
} // namespace OpenBlood2
