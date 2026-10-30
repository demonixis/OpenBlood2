#include "app/model_lighting.h"
#include "game/local_scene.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool ModelLighting::apply(GameSession& game, const WorldMesh& mesh, std::span<const WorldDraw> draws,
                              ModelView& scene, std::string& error)
    {
        return game.publish_client_world(error) && apply(game.client_session.world, game.client_shell.view.position,
                                                         game.client_shell.night_goggles, mesh, draws, scene, error);
    }
} // namespace OpenBlood2
