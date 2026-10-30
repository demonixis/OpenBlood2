#include "game/server_adapter.h"
#include <charconv>
#include <cmath>

namespace OpenBlood2
{
    bool GameSession::publish_client_world(std::string& error)
    {
        if (objects.busy())
        {
            error = "Client publication during a server callback rejected";
            return false;
        }
        ClientWorldFrame frame;
        frame.world = client_session.world.frame().world;
        frame.sequence = client_session.world.frame().sequence + 1;
        frame.time = server.world_time();
        auto identity = [&](HOBJECT handle)
        {
            const auto* record = objects.get(handle);
            return record && record->life == Life::Alive ? record->id : ObjectId{};
        };
        const auto ids = objects.snapshot();
        frame.objects.reserve(ids.size());
        for (auto id : ids)
        {
            const auto* r = objects.get(id);
            if (!r || r->life != Life::Alive)
                continue;
            ClientObject out;
            out.id = id;
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
                }
            }
            out.world_model_index = r->world_model_index;
            out.world_model_bound = bool(r->world_model);
            frame.objects.push_back(std::move(out));
        }
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
        frame.player = identity(client_shell.player);
        frame.seeing_eye = identity(client_shell.seeing_eye);
        frame.orb = identity(client_shell.orb_view);
        const auto* owner = server_shell.Player(local_client);
        frame.player_alive = owner && !owner->m_damage.IsDead();
        for (const auto& fx : server.polygrids.active)
            frame.polygrids.push_back({fx.effect.id, identity(fx.owner)});
        for (const auto& fx : server.particle_streams.active)
            frame.particle_streams.push_back({fx.effect.id, identity(fx.owner)});
        return client_session.receive(std::move(frame), error);
    }
} // namespace OpenBlood2
