#include "polygrid_scene.h"
#include <algorithm>
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
        for (const auto& instance : game.server.polygrids.active)
        {
            const auto found = std::find_if(world.frame().polygrids.begin(), world.frame().polygrids.end(),
                                            [&](const auto& entry) { return entry.id == instance.effect.id; });
            const auto* record = found != world.frame().polygrids.end() ? world.get(found->owner) : nullptr;
            if (!record)
                continue;
            const auto& fx = instance.effect;
            if (fx.sprite && fx.track.frame >= fx.sprite->frames.size())
            {
                error = "PolyGrid current sprite frame unavailable";
                return false;
            }
            const auto path = fx.sprite && fx.track.has_frame && fx.track.frame < fx.sprite->frames.size()
                                  ? std::string_view(fx.sprite->frames[fx.track.frame])
                                  : std::string_view{};
            if (!scene.add(fx.id, (record->user_flags & 1u) != 0, fx.pose, fx.settings.dims, fx.width, fx.heights,
                           fx.palette, fx.scale, fx.sprite ? fx.settings.pan : std::array<float, 2>{},
                           fx.settings.alpha, path, game.resources, error))
                return false;
        }
        return true;
    }
} // namespace OpenBlood2
