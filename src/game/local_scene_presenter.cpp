#include "app/scene_presenter.h"
#include "game/local_scene.h"
#include "game/server_adapter.h"
namespace OpenBlood2
{
    bool ScenePresenter::update(GameSession& game, std::string& error)
    {
        if (!game.publish_client_world(error))
        {
            error = "Local client world: " + error;
            return false;
        }
        return update(local_scene(game), error);
    }
    bool ScenePresenter::record(GameSession& game, SDL_GPUCommandBuffer* commands, SDL_GPUTexture* target,
                                SDL_GPUTextureFormat format, Uint32 width, Uint32 height, std::string& error)
    {
        return record(local_scene(game), commands, target, format, width, height, error);
    }
    bool ScenePresenter::record_eyes(GameSession& game, SDL_GPUCommandBuffer* commands, SDL_GPUTexture* target,
                                     SDL_GPUTextureFormat format, Uint32 width, Uint32 height,
                                     std::span<const StereoEye> eyes, std::string& error)
    {
        return record_eyes(local_scene(game), commands, target, format, width, height, eyes, error);
    }
} // namespace OpenBlood2
