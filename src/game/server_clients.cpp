#include "engine/world_model.h"
#include "game/server_adapter.h"
#include <cmath>

namespace OpenBlood2
{
    HCLIENT ServerAdapter::connect_client()
    {
        trace_service("ConnectClient");
        if (!bound_ || !clients_.empty() || client_tokens_.size() >= 65536 || objects_.busy())
            return nullptr;
        auto token = std::make_unique<ClientHandle>();
        auto h = token.get();
        client_tokens_.push_back(std::move(token));
        clients_.emplace(h, Client{});
        return h;
    }
    bool ServerAdapter::disconnect_client(HCLIENT client)
    {
        trace_service("DisconnectClient");
        auto it = clients_.find(client);
        if (it == clients_.end() || objects_.busy())
            return false;
        if (GetClientUserData(client))
            return false; // Exit callback must detach first.
        clients_.erase(it);
        return true;
    }
    HOBJECT ServerAdapter::GetClientUserData(HCLIENT client)
    {
        trace_service("GetClientUserData");
        auto it = clients_.find(client);
        if (it == clients_.end())
            return nullptr;
        const auto* record = objects_.get(it->second.player);
        if (!record || record->life != Life::Alive)
        {
            it->second.player = nullptr;
            it->second.events.clear();
            return nullptr;
        }
        return it->second.player;
    }
    DRESULT ServerAdapter::SetClientUserData(HCLIENT client, HOBJECT player)
    {
        trace_service("SetClientUserData");
        auto it = clients_.find(client);
        if (it == clients_.end())
            return DE_INVALIDPARAMS;
        if (player)
        {
            const auto* record = objects_.get(player);
            if (!record || record->life != Life::Alive ||
                !Objects::is_kind_of(record->type, objects_.find_class("CPlayerObj")))
                return DE_INVALIDPARAMS;
            if (it->second.player && it->second.player != player)
                return DE_ERROR;
        }
        if (auto* previous = objects_.get(it->second.player))
            previous->client_movement = false;
        if (auto* next = objects_.get(player))
            next->client_movement = true;
        if (!player)
            it->second.events.clear();
        it->second.player = player;
        return DE_OK;
    }
    DRESULT ServerAdapter::SendToClient(HCLIENT client, const PlayerEvent& event)
    {
        trace_service("SendToClient");
        if (!GetClientUserData(client))
            return DE_INVALIDPARAMS;
        const bool valid = std::visit(
            [](const auto& value)
            {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, PlayerRotation>)
                    return std::isfinite(value.pitch) && std::isfinite(value.yaw);
                else if constexpr (std::is_same_v<T, PlayerPosition>)
                    return physics_vector(value.position);
                else if constexpr (std::is_same_v<T, PlayerStats>)
                    return std::isfinite(value.health) && std::isfinite(value.armor) && std::isfinite(value.bullets);
                else if constexpr (std::is_same_v<T, InventoryAction>)
                    return value.type == 1 || value.type == 2;
                else if constexpr (std::is_same_v<T, BossHealth>)
                    return std::isfinite(value.fraction);
                else if constexpr (std::is_same_v<T, WorldExit>)
                    return value.next_world.size() <= 100 && value.next_world.find('\0') == std::string::npos &&
                           static_cast<DBYTE>(value.type) <= static_cast<DBYTE>(WorldExitType::Episode);
                else if constexpr (std::is_same_v<T, Conversation>)
                    return !value.file.empty() && value.file.size() < 200 && value.text.size() < 500 &&
                           value.file.find('\0') == std::string::npos && value.text.find('\0') == std::string::npos;
                else if constexpr (std::is_same_v<T, MissionObjectives>)
                    return value.resource <= 65535 - 6000 + 1 && value.title.size() <= MAX_GP_STRING_LEN &&
                           value.text.size() <= MAX_GP_STRING_LEN && value.title.find('\0') == std::string::npos &&
                           value.text.find('\0') == std::string::npos;
                else if constexpr (std::is_same_v<T, PlayerPhysics>)
                    return value.model.size() <= 100 && value.skin.size() <= 100 &&
                           value.model.find('\0') == std::string::npos && value.skin.find('\0') == std::string::npos &&
                           physics_vector(value.dimensions) && physics_vector(value.gravity) &&
                           value.dimensions.x >= 0 && value.dimensions.y >= 0 && value.dimensions.z >= 0 &&
                           std::isfinite(value.move_speed) && value.move_speed >= 0 && value.move_speed <= 10000 &&
                           std::isfinite(value.jump_speed) && value.jump_speed >= 0 && value.jump_speed <= 10000;
                else
                    return true;
            },
            event);
        if (!valid)
            return DE_INVALIDPARAMS;
        auto& events = clients_.at(client).events;
        if (events.size() >= 256)
            return DE_ERROR;
        events.push_back(event);
        return DE_OK;
    }
    DRESULT ServerAdapter::SendBossHealth(const BossHealth& value)
    {
        trace_service("SendBossHealth");
        if (!std::isfinite(value.fraction))
            return DE_INVALIDPARAMS;
        for (const auto& [client, state] : clients_)
        {
            (void)state;
            if (GetClientUserData(client))
                return SendToClient(client, value);
        }
        return DE_NOTFOUND;
    }
    bool ServerAdapter::take_client_events(HCLIENT client, std::vector<PlayerEvent>& events)
    {
        events.clear();
        if (!GetClientUserData(client))
            return false;
        events.swap(clients_.at(client).events);
        return true;
    }
    DRESULT ServerAdapter::GetObjectDims(HOBJECT h, DVector* out)
    {
        trace_service("GetObjectDims");
        if (out)
            *out = {};
        const auto* r = objects_.get(h);
        if (!r || !out)
            return DE_INVALIDPARAMS;
        *out = r->dimensions;
        return DE_OK;
    }
    DRESULT ServerAdapter::SetObjectRotation(HOBJECT h, const DRotation* rotation)
    {
        trace_service("SetObjectRotation");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->attachment || (r->container || r->world_model) || !rotation ||
            !physics_vector(rotation->m_Vec) || !std::isfinite(rotation->m_Spin))
            return DE_INVALIDPARAMS;
        const auto& v = rotation->m_Vec;
        const float norm = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z + rotation->m_Spin * rotation->m_Spin);
        if (!std::isfinite(norm) || norm < 1e-6f)
            return DE_INVALIDPARAMS;
        r->state.m_Rotation = {vector_scale(v, 1 / norm), rotation->m_Spin / norm};
        return DE_OK;
    }
    DBOOL ServerAdapter::CompareStringsUpper(HSTRING a, HSTRING b)
    {
        trace_service("CompareStringsUpper");
        const char* x = string_data(a);
        const char* y = string_data(b);
        return x && y && !CaseLess{}(x, y) && !CaseLess{}(y, x);
    }
    bool ServerAdapter::GetModelFilenames(HOBJECT h, std::string& model, std::string& skin)
    {
        trace_service("GetModelFilenames");
        const auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->state.m_ObjectType != OT_MODEL || !r->model || !r->state.m_Filename[0])
            return false;
        model = r->state.m_Filename;
        skin = r->state.m_SkinName;
        return true;
    }
    DRESULT ServerAdapter::SetModelFilenames(HOBJECT h, const char* model, const char* skin)
    {
        trace_service("SetModelFilenames");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->state.m_ObjectType != OT_MODEL || !objects_.models || !model || !skin ||
            std::strlen(model) > 100 || std::strlen(skin) > 100)
            return DE_INVALIDPARAMS;
        // Load before mutation. A missing replacement cannot discard the live model.
        std::shared_ptr<const Model> next;
        std::string error;
        if (!objects_.models->load(model, next, error))
        {
            objects_.fail(ObjectError::Unsupported, error);
            return DE_ERROR;
        }
        if (r->model != next)
        {
            r->model = std::move(next);
            r->hidden_nodes.clear();
            r->animation.animation = 0;
            r->animation.reset();
        }
        SAFE_STRCPY(r->state.m_Filename, model);
        SAFE_STRCPY(r->state.m_SkinName, skin);
        r->model_error.clear();
        return DE_OK;
    }
    DRESULT ServerAdapter::MoveObjectToGround(HOBJECT h)
    {
        trace_service("MoveObjectToGround");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->attachment || !collision || !collision_ready)
            return DE_INVALIDPARAMS;
        // Original ObjectUtilities::MoveObjectToGround: downward ray, animation-0
        // user dimensions for characters, then collision-aware MoveObject. The
        // otherwise unbounded CastRay is limited to the supported coordinate range.
        auto from = r->state.m_Pos;
        auto to = from;
        to.y = -10000000.0f;
        CollisionHit nearest;
        auto result = collision->sweep(from, to, {}, nearest);
        if (result == CollisionQuery::Invalid)
            return DE_ERROR;
        bool found = result == CollisionQuery::Hit;
        for (auto id : objects_.snapshot())
        {
            const auto* other = objects_.get(id);
            if (!other || other->life != Life::Alive || other->attachment || other->handle == h ||
                !(other->state.m_Flags & FLAG_SOLID))
                continue;
            CollisionHit hit;
            const auto query = sweep_object(*other, from, to, {}, hit);
            if (query == CollisionQuery::Invalid)
                return DE_ERROR;
            if (query == CollisionQuery::Hit && (!found || hit.fraction < nearest.fraction))
            {
                nearest = hit;
                found = true;
            }
        }
        if (!found || nearest.start_solid)
            return DE_NOTFOUND;
        DVector dims = r->dimensions;
        if (Objects::is_kind_of(r->type, objects_.find_class("CBaseCharacter")) &&
            GetModelAnimUserDims(h, &dims, 0) != DE_OK)
            return DE_ERROR;
        const float distance = from.y - nearest.point.y + 0.1f;
        if (distance > dims.y)
        {
            from.y = from.y - distance + (dims.y + 0.1f);
            return MoveObject(h, &from);
        }
        return DE_OK; // Already close enough; original helper does not move upward.
    }
} // namespace OpenBlood2
