#include "game/client_replication.h"
#include "game/original/SeeingEye.h"
#include "game/original/VolumeBrush.h"
#include "game/server_adapter.h"
#include <charconv>
#include <cmath>

namespace OpenBlood2
{
    ClientCollisionValues capture_collision_values(const ObjectRecord& object, const Objects& objects)
    {
        ClientCollisionValues value;
        value.enabled = true;
        value.motion.dimensions = object.dimensions;
        value.motion.velocity = object.velocity;
        value.motion.acceleration = object.acceleration;
        value.world_model = bool(object.world_model);
        value.container = bool(object.container());
        value.volume_brush = dynamic_cast<const VolumeBrush*>(object.game.get()) != nullptr;
        value.dynamic = object.world_body || object.model_body;
        value.pushable = object.body_properties.pushable;
        value.priority = object.blocking_priority;
        const auto& shape = object.world_model ? object.world_model : object.container();
        if (shape)
        {
            value.motion.shape_origin = shape->origin;
            value.basis = object.world_model ? object.world_model_basis : object.state.m_Rotation;
            value.shape_transformed = object.world_model && object.world_model != object.world_model_base;
        }
        if (value.container)
        {
            ContainerInfo info;
            if (container_info(object, info))
                value.environment = {info.code, info.current, info.gravity, info.viscosity, info.hidden};
        }
        if (const auto* eye = dynamic_cast<const SeeingEye*>(object.game.get()); eye && !eye->dropped)
            if (const auto* owner = objects.get(eye->m_hFiredFrom); owner && owner->life == Life::Alive)
                value.ignored_by = owner->id;
        return value;
    }
    bool capture_client_objects(GameSession& game, ClientWorldFrame& frame, std::string& error, bool collision)
    {
        auto& objects = game.objects;
        auto& server = game.server;
        if (objects.busy())
        {
            error = "Client publication during a server callback rejected";
            return false;
        }
        frame.polygrids.clear();
        frame.particle_streams.clear();
        frame.time = server.world_time();
        auto identity = [&](HOBJECT handle)
        {
            const auto* record = objects.get(handle);
            return record && record->life == Life::Alive ? record->id : ObjectId{};
        };
        const auto ids = objects.snapshot();
        frame.objects.reserve(ids.size());
        std::size_t count = 0;
        for (auto id : ids)
        {
            const auto* r = objects.get(id);
            if (!r || r->life != Life::Alive)
                continue;
            auto& out = count < frame.objects.size() ? frame.objects[count] : frame.objects.emplace_back();
            ++count;
            {
                // Reset every field, keeping only the allocations of a reused frame.
                auto filename = std::move(out.filename);
                auto skin = std::move(out.skin);
                auto hidden_nodes = std::move(out.hidden_nodes);
                out = {};
                out.filename = std::move(filename);
                out.skin = std::move(skin);
                out.hidden_nodes = std::move(hidden_nodes);
            }
            out.id = id;
            out.pose_revision = r->pose_revision;
            out.type = r->state.m_ObjectType;
            out.flags = r->state.m_Flags;
            out.user_flags = r->user_flags;
            out.pose = {r->state.m_Pos, r->state.m_Rotation};
            out.scale = r->state.m_Scale;
            out.color = r->color;
            out.alpha = r->alpha;
            out.filename = r->state.m_Filename;
            out.skin = r->state.m_SkinName;
            out.model = r->model;
            out.animation = r->animation;
            out.hidden_nodes = r->hidden_nodes;
            out.sprite = r->sprite;
            out.sprite_track = r->sprite_track;
            out.attached = r->attachment != 0;
            if (out.attached)
            {
                const auto relation = objects.attachments().find(r->attachment);
                if (relation != objects.attachments().end())
                {
                    out.parent = identity(relation->second.parent);
                    out.attachment_resolved = relation->second.resolved;
                    // The checked attachment service refuses chains. Its child
                    // must snap with a teleported parent, including bone attachments.
                    if (const auto* parent = objects.get(out.parent))
                        out.pose_revision = std::max(out.pose_revision, parent->pose_revision);
                }
            }
            out.world_model_index = r->world_model_index;
            out.world_model_bound = bool(r->world_model);
            if (collision)
                out.collision = capture_collision_values(*r, objects);
        }
        frame.objects.resize(count);
        const auto& sky = server.sky;
        frame.sky = {sky.definition.min, sky.definition.max, sky.definition.view_min, sky.definition.view_max,
                     sky.objects};
        // Preserve GetServerConVarValueFloat followed by original integer ModelAdd truncation.
        auto channel = [&](const char* key)
        {
            const auto found = server.variables().find(key);
            if (found == server.variables().end())
                return 0.f;
            const auto& text = found->second.text;
            float value = 0;
            const auto [end, code] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (code != std::errc{} || end != text.data() + text.size() || !std::isfinite(value) ||
                std::abs(value) > 1e6f)
                return 0.f;
            return static_cast<float>(static_cast<int>(value));
        };
        frame.model_add = {channel("ModelAddR"), channel("ModelAddG"), channel("ModelAddB")};
        frame.model_dir_add = {channel("ModelDirAddR"), channel("ModelDirAddG"), channel("ModelDirAddB")};
        auto effect_identity = [&](ClientObjectId reference)
        {
            const auto* object =
                reference.world == game.client_session.world.frame().world ? objects.get(reference.object) : nullptr;
            return object && object->life == Life::Alive ? object->id : ObjectId{};
        };
        for (const auto& fx : server.polygrids.active)
            frame.polygrids.push_back({fx.effect.id, effect_identity(fx.owner)});
        for (const auto& fx : server.particle_streams.active)
            frame.particle_streams.push_back({fx.effect.id, effect_identity(fx.owner)});
        return true;
    }
    bool GameSession::publish_client_objects(std::string& error)
    {
        if (objects.busy())
        {
            error = "Client publication during a server callback rejected";
            return false;
        }
        // The scratch frame is the replica's previous frame (swapped out by receive).
        auto& frame = object_scratch_;
        frame.world = client_session.world.frame().world;
        frame.sequence = client_session.world.frame().sequence + 1;
        auto identity = [&](HOBJECT handle)
        {
            const auto* record = objects.get(handle);
            return record && record->life == Life::Alive ? record->id : ObjectId{};
        };
        frame.player = identity(client_shell.player);
        frame.seeing_eye = identity(client_shell.seeing_eye);
        frame.orb = identity(client_shell.orb_view);
        const auto* owner = server_shell.Player(local_client);
        frame.player_alive = owner && !owner->m_damage.IsDead();
        const bool accepted =
            capture_client_objects(*this, frame, error) && client_session.receive(std::move(frame), error);
        // The replaced (or refused) frame keeps no asset alive past this publication.
        for (auto& object : frame.objects)
        {
            object.model.reset();
            object.sprite.reset();
        }
        return accepted;
    }
    bool GameSession::publish_client_world(std::string& error)
    {
        if (!publish_client_objects(error))
            return false;
        if (!local_effects.publish(*this, client_session.effects, effects_replica_error))
        {
            error = effects_replica_error;
            return false;
        }
        return true;
    }
} // namespace OpenBlood2
