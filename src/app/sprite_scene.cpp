#include "app/sprite_scene.h"
namespace OpenBlood2
{
    bool update_sprite_scene(const GameSession& game, SpriteView& scene, std::string& error)
    {
        const auto& view = game.client_shell.view;
        if (!scene.begin_frame(view.position, view.rotation, error))
            return false;
        for (auto id : game.objects.snapshot())
        {
            const auto* record = game.objects.get(id);
            if (!record || record->life != Life::Alive || record->state.m_ObjectType != OT_SPRITE)
                continue;
            if (!scene.add(record->sprite.get(), record->sprite_track, {record->state.m_Pos, record->state.m_Rotation},
                           record->state.m_Scale, record->state.m_Flags, id, game.resources, error))
                return false;
        }
        scene.sort();
        return true;
    }
} // namespace OpenBlood2
