#include "client/object_scene.h"

namespace OpenBlood2
{
    bool append_client_models(const ClientWorld& world, bool player_visible, ModelView& scene,
                              const Resources& resources, std::string& error)
    {
        for (const auto& object : world.frame().objects)
        {
            const auto* record = &object;
            if (record->type != OT_MODEL)
                continue;
            const auto id = record->id;
            bool visible = (record->flags & FLAG_VISIBLE) != 0;
            if (id == world.frame().seeing_eye || id == world.frame().orb)
                visible = false; // The camera is inside this model; do not draw its interior.
            const bool unresolved = record->attached && (!record->attachment_resolved || !world.get(record->parent));
            if (unresolved)
                visible = false;
            if (!player_visible && (id == world.frame().player || (world.frame().player && record->attached &&
                                                                   record->parent == world.frame().player)))
                visible = false;
            if (!scene.add(record->model, record->animation, record->skin, record->pose, record->scale, visible,
                           (record->flags & FLAG_REALLYCLOSE) != 0, id, record->filename, resources, error,
                           record->hidden_nodes, record->alpha))
                return false;
            if (record->alpha < 1)
                scene.mark_translucent(scene.entries.size() - 1);
            if (unresolved)
                scene.entries.back().error = "Unresolved attachment pose";
        }
        return true;
    }
    bool append_client_sprites(const ClientWorld& world, SpriteView& scene, const Resources& resources,
                               std::string& error)
    {
        for (const auto& object : world.frame().objects)
        {
            const auto* record = &object;
            const auto id = record->id;
            if (record->type != OT_SPRITE || (record->flags & FLAG_SKYOBJECT))
                continue;
            if (!scene.add(record->sprite.get(), record->sprite_track, {record->pose.position, record->pose.rotation},
                           record->scale, record->flags, id, resources, error, record->alpha, record->color))
                return false;
        }
        return true;
    }
} // namespace OpenBlood2
