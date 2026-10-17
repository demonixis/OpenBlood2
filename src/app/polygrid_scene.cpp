#include "polygrid_scene.h"
namespace OpenBlood2
{
    bool update_polygrid_scene(const GameSession& game, PolyGridView& scene, std::string& error)
    {
        error.clear();
        scene.begin_frame();
        for (const auto& instance : game.server.polygrids.active)
        {
            const auto* record = game.objects.get(instance.owner);
            if (!record || record->life != Life::Alive)
                continue;
            const auto& fx = instance.effect;
            if (fx.sprite && fx.track.frame >= fx.sprite->frames.size())
            {
                error = "PolyGrid current sprite frame unavailable";
                return false;
            }
            const auto path = fx.sprite && fx.track.frame < fx.sprite->frames.size()
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
