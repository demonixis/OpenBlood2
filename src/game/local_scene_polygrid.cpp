#include "app/polygrid_scene.h"
#include "game/local_scene.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool update_polygrid_scene(GameSession& game, PolyGridView& scene, std::string& error)
    {
        return game.publish_client_world(error) && update_polygrid_scene(local_scene(game), scene, error);
    }
    bool update_polygrid_scene(const GameSession& game, const ClientWorld& world, PolyGridView& scene,
                               std::string& error)
    {
        return update_polygrid_scene(local_scene(game, &world), scene, error);
    }
} // namespace OpenBlood2
