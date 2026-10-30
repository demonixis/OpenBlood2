#include "client/network_world.h"

namespace OpenBlood2
{
    bool receive_network_world(const Net::WorldSnapshot& snapshot, const World& geometry, ModelStore& models,
                               SpriteStore& sprites, ClientWorld& world, std::string& error)
    {
        error.clear();
        const auto& current = world.frame();
        if (!Net::valid_snapshot(snapshot) || snapshot.world != current.world ||
            snapshot.sequence <= current.sequence || snapshot.time < current.time)
        {
            error = "Invalid or stale network world snapshot";
            return false;
        }
        auto frame = Net::snapshot_frame_values(snapshot);
        for (std::size_t i = 0; i < frame.objects.size(); ++i)
        {
            auto& object = frame.objects[i];
            const auto& state = snapshot.objects[i];
            if (state.has_model)
            {
                if (!models.load(object.filename, object.model, error))
                    return false;
                if (object.animation.animation >= object.model->animations.size() ||
                    (!object.hidden_nodes.empty() && object.hidden_nodes.size() != object.model->nodes.size()))
                {
                    error = "Network model animation/node binding mismatch: " + object.filename;
                    return false;
                }
            }
            else if (!object.hidden_nodes.empty())
            {
                error = "Network hidden nodes require a model";
                return false;
            }
            if (state.has_sprite)
            {
                if (!sprites.load(object.filename, object.sprite, error, true))
                    return false;
                if (object.sprite_track.has_frame && !object.sprite->frames.empty() &&
                    object.sprite_track.frame >= object.sprite->frames.size())
                {
                    error = "Network sprite frame binding mismatch: " + object.filename;
                    return false;
                }
            }
            if (object.world_model_bound)
            {
                const auto index = object.world_model_index;
                if (index >= geometry.models.size() || CaseLess{}(geometry.models[index].name, object.filename) ||
                    CaseLess{}(object.filename, geometry.models[index].name))
                {
                    error = "Network world model binding mismatch: " + object.filename;
                    return false;
                }
            }
        }
        return world.apply(std::move(frame), error);
    }
} // namespace OpenBlood2
