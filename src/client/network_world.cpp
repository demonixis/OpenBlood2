#include "client/network_world.h"
#include <algorithm>

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
            if (object.world_model_bound || (object.collision.enabled && object.collision.container))
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
    bool NetworkWorldReceiver::begin(std::uint64_t world)
    {
        if (!world_.begin(world))
            return false;
        generations_.clear();
        collision_ = {};
        histories_.clear();
        membership_sequence_ = 0;
        membership_time_ = 0;
        return true;
    }
    bool NetworkWorldReceiver::receive(const Net::WorldUpdate& update, const World& geometry, ModelStore& models,
                                       SpriteStore& sprites, std::string& error)
    {
        error.clear();
        Net::WorldSnapshot next;
        if (const auto* baseline = std::get_if<Net::WorldSnapshot>(&update))
        {
            if (membership_sequence_)
            {
                error = "Duplicate initial world baseline";
                return false;
            }
            next = *baseline;
        }
        else
        {
            const auto& patch = std::get<Net::WorldPatch>(update);
            if (!membership_sequence_ || patch.base != membership_sequence_ || patch.values.sequence <= patch.base ||
                patch.values.world != world_.frame().world || patch.values.time < world_.frame().time ||
                !Net::valid_snapshot(patch.values) || patch.removed.size() > ClientWorld::object_limit)
            {
                error = "Stale or invalid world patch base";
                return false;
            }
            ObjectId previous;
            for (auto id : patch.removed)
            {
                if (!world_.get(id) || (previous && previous.slot >= id.slot))
                {
                    error = "World patch removes an unknown or unordered generation";
                    return false;
                }
                previous = id;
            }
            next = patch.values;
            next.objects.clear();
            auto current = Net::snapshot_values(world_.frame());
            const auto& incoming = patch.values.objects;
            std::size_t left = 0, right = 0, removed = 0;
            while (left < current.objects.size() || right < incoming.size())
            {
                if (left < current.objects.size() && removed < patch.removed.size() &&
                    current.objects[left].id == patch.removed[removed])
                {
                    ++left;
                    ++removed;
                    continue;
                }
                if (right == incoming.size() ||
                    (left < current.objects.size() && current.objects[left].id.slot < incoming[right].id.slot))
                    next.objects.push_back(std::move(current.objects[left++]));
                else
                {
                    const auto& object = incoming[right++];
                    const bool replacing =
                        left < current.objects.size() && current.objects[left].id.slot == object.id.slot;
                    if (replacing && current.objects[left].id != object.id)
                    {
                        error = "World patch replaces a generation without removing it";
                        return false;
                    }
                    const auto highest = object.id.slot < generations_.size() ? generations_[object.id.slot] : 0;
                    if (!replacing && object.id.generation <= highest)
                    {
                        error = "World patch resurrects a retired generation";
                        return false;
                    }
                    if (replacing && current.objects[left].particle_stream &&
                        (!object.particle_stream ||
                         !same_particle_stream(*current.objects[left].particle_stream, *object.particle_stream)))
                    {
                        error = "World patch changes an existing particle stream recipe";
                        return false;
                    }
                    if (replacing)
                        ++left;
                    next.objects.push_back(object);
                }
                if (next.objects.size() > ClientWorld::object_limit)
                {
                    error = "World patch exceeds object budget";
                    return false;
                }
            }
        }
        if ((next.player_alive && !next.player) ||
            (next.player && !std::any_of(next.objects.begin(), next.objects.end(),
                                         [&](const auto& object) { return object.id == next.player; })))
        {
            error = "World update has no matching recipient player";
            return false;
        }
        // Bound the resulting world too; small deltas cannot grow an unbounded
        // resource graph. Validation precedes asset resolution or replica mutation.
        std::vector<std::uint8_t> checked;
        if (!Net::encode_world_snapshot(next, checked))
        {
            error = "World patch result exceeds snapshot bounds";
            return false;
        }
        const auto sequence = next.sequence;
        // ClientWorld publication counts accepted packets, independently of the
        // server's sequence (one State sequence may span multiple datagrams).
        next.sequence = world_.frame().sequence + 1;
        auto candidate = world_;
        if (!receive_network_world(next, geometry, models, sprites, candidate, error))
            return false;
        if (!collision_.active() && std::any_of(next.objects.begin(), next.objects.end(),
                                                [](const auto& object) { return object.collision.enabled; }))
            if (!collision_.begin(next.world, geometry, error))
                return false;
        if (collision_.active() && !collision_.receive(candidate, error))
            return false;
        world_ = std::move(candidate);
        for (const auto& object : next.objects)
        {
            if (generations_.size() <= object.id.slot)
                generations_.resize(object.id.slot + 1);
            generations_[object.id.slot] = object.id.generation;
        }
        if (const auto* patch = std::get_if<Net::WorldPatch>(&update))
        {
            for (auto id : patch->removed)
                if (id.slot < histories_.size())
                    histories_[id.slot] = {};
            for (const auto& object : patch->values.objects)
                reset_history(object, next.time, sequence);
        }
        else
            for (const auto& object : next.objects)
                reset_history(object, next.time, sequence);
        membership_sequence_ = sequence;
        membership_time_ = next.time;
        return true;
    }
    void NetworkWorldReceiver::reset_history(const Net::ObjectState& object, double time, std::uint64_t sequence)
    {
        if (histories_.size() <= object.id.slot)
            histories_.resize(object.id.slot + 1);
        auto& history = histories_[object.id.slot];
        history.generation = object.id.generation;
        history.sequence = sequence;
        history.samples = {{time, Net::motion_values(object)}};
    }
} // namespace OpenBlood2
