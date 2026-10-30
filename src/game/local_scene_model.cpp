#include "app/model_scene.h"
#include "game/local_scene.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool update_model_scene(GameSession& game, ModelView& scene, std::string& error,
                            const std::array<ViewWeaponCamera, 2>* weapons)
    {
        return game.publish_client_world(error) && update_model_scene(local_scene(game), scene, error, weapons);
    }
    bool update_model_scene(const GameSession& game, const ClientWorld& world, ModelView& scene, std::string& error,
                            const std::array<ViewWeaponCamera, 2>* weapons)
    {
        return update_model_scene(local_scene(game, &world), scene, error, weapons);
    }
    bool update_explosion_model_scene(GameSession& game, ModelView& scene, std::string& error,
                                      const ClientCameraView* camera)
    {
        return game.publish_client_world(error) &&
               update_explosion_model_scene(local_scene(game), scene, error, camera);
    }
    bool update_explosion_model_scene(const GameSession& game, const ClientEffects& effects, ModelView& scene,
                                      std::string& error, const ClientCameraView* camera)
    {
        return update_explosion_model_scene(local_scene(game, nullptr, &effects), scene, error, camera);
    }
} // namespace OpenBlood2
