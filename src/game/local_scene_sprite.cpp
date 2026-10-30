#include "app/sprite_scene.h"
#include "game/local_scene.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool update_sprite_scene(GameSession& game, SpriteView& scene, std::string& error, const ClientCameraView* camera,
                             const std::array<ViewWeaponCamera, 2>* weapons)
    {
        return game.publish_client_world(error) &&
               update_sprite_scene(local_scene(game), scene, error, camera, weapons);
    }
    bool update_sprite_scene(const GameSession& game, const ClientWorld& world, SpriteView& scene, std::string& error,
                             const ClientCameraView* camera, const std::array<ViewWeaponCamera, 2>* weapons)
    {
        return update_sprite_scene(local_scene(game, &world), scene, error, camera, weapons);
    }
} // namespace OpenBlood2
