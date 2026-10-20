#include "engine/world_model.h"
#include "game/original/AI_Mgr.h"
#include "game/server_adapter.h"
#include <algorithm>
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
        if (!trace_service("SendToClient"))
            return DE_ERROR;
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
                    return std::isfinite(value.max_health) && value.max_health >= 0 && std::isfinite(value.max_armor) &&
                           value.max_armor >= 0 && std::isfinite(value.health) && std::isfinite(value.armor) &&
                           std::isfinite(value.bullets) && std::isfinite(value.ammo) && value.ammo >= 0 &&
                           std::isfinite(value.alternate_ammo) && value.alternate_ammo >= 0 &&
                           value.weapon_slot < inventory_weapon_slots && value.ammo_type < 13 &&
                           value.alternate_ammo_type < 13 && (!value.weapon || weapon_definition(value.weapon)) &&
                           std::all_of(
                               value.weapon_slots.begin(), value.weapon_slots.end(),
                               [](DBYTE type) { return !type || weapon_definition(type); });
                else if constexpr (std::is_same_v<T, MusicCommand>)
                    return valid_music_command(value);
                else if constexpr (std::is_same_v<T, VoiceEvent>)
                    return value.character >= 0 && value.character < 4 && value.event >= 0 &&
                           value.event < (value.group ? 12 : 9);
                else if constexpr (std::is_same_v<T, InventoryAction>)
                    return value.type == 1 || value.type == 2 || value.type == 3 || value.type == 5;
                else if constexpr (std::is_same_v<T, OrbView>)
                    return value.orb != nullptr;
                else if constexpr (std::is_same_v<T, SeeingEyeView>)
                    return (!value.active || value.eye) && std::isfinite(value.blind_seconds) &&
                           value.blind_seconds >= 0 && value.blind_seconds <= 10;
                else if constexpr (std::is_same_v<T, CreatureAttach>)
                    return value.creature && value.enemy;
                else if constexpr (std::is_same_v<T, CTFEvent>)
                    return value.team <= 2 &&
                           static_cast<DBYTE>(value.kind) <= static_cast<DBYTE>(CTFEventKind::Captured) &&
                           value.points >= 0 && value.points <= 1000000;
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
        if (!trace_service("SendBossHealth"))
            return DE_ERROR;
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
        if (!r || r->life != Life::Alive || r->attachment || (r->container() || r->world_model) || !rotation ||
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
    DRESULT ServerAdapter::ResolveInitialWorldOverlap(HOBJECT h)
    {
        trace_service("ResolveInitialWorldOverlap");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->attachment || r->world_model || r->container() || !collision ||
            !collision_ready || !physics_vector(r->dimensions) || r->dimensions.x <= 0 || r->dimensions.y <= 0 ||
            r->dimensions.z <= 0 || !(r->state.m_Flags & (FLAG_SOLID | FLAG_GRAVITY)))
            return DE_INVALIDPARAMS;
        const auto half = r->dimensions;
        // Separate horizontal/vertical budgets admit a ceiling plus its sloped trim.
        // The small margin includes the existing 0.001 separation on each of eight steps.
        const double horizontal_limit = std::min(half.x, half.z) + .01;
        const double vertical_limit = half.y + .01;
        const auto origin = r->state.m_Pos;
        bool centered = false;
        for (unsigned branch = 0; branch < 2; ++branch)
        {
            if (branch && !centered)
                break;
            auto position = origin;
            double horizontal = 0, vertical = 0;
            for (unsigned i = 0; i <= 8; ++i)
            {
                CollisionHit overlap;
                const auto query = collision->sweep(position, position, half, overlap);
                if (query == CollisionQuery::Invalid)
                    return DE_ERROR;
                if (query == CollisionQuery::Miss || !overlap.start_solid)
                {
                    if (i == 0)
                        return DE_OK;
                    const auto nudge = vector_subtract(position, origin);
                    if (TeleportObject(h, &position) != DE_OK)
                        return DE_ERROR;
                    r->placement_nudge = nudge;
                    ++r->placement_nudges;
                    return DE_OK;
                }
                if (i == 8 || !physics_vector(overlap.normal) || !std::isfinite(overlap.penetration))
                    break;
                auto normal = overlap.normal;
                if (i == 0)
                {
                    const double radius =
                        std::abs(normal.x) * half.x + std::abs(normal.y) * half.y + std::abs(normal.z) * half.z;
                    centered = std::abs(overlap.penetration - radius) < .001;
                    // A center exactly on a thin face has two equal separating directions.
                    // Retry the other side only if the first bounded, atomic solve fails.
                    if (branch)
                        normal = vector_scale(normal, -1);
                }
                const auto correction = vector_scale(normal, static_cast<float>(overlap.penetration + .001));
                horizontal += std::hypot(correction.x, correction.z);
                vertical += std::abs(correction.y);
                if (horizontal > horizontal_limit || vertical > vertical_limit)
                    break;
                position = vector_add(position, correction);
            }
        }
        // A marker exactly on a floor edge can make the nearest-axis solve
        // alternate between the adjacent triangles. Try the bounded vertical
        // escape as well, without crossing another surface with the center.
        // The following ground ray determines the actual supported height.
        auto lifted = vector_add(origin, {0, half.y + .001f, 0});
        CollisionHit clearance, route;
        if (collision->sweep(lifted, lifted, half, clearance) == CollisionQuery::Miss &&
            collision->sweep(origin, lifted, {}, route, 0, StartOverlap::Leave) == CollisionQuery::Miss)
        {
            if (TeleportObject(h, &lifted) != DE_OK)
                return DE_ERROR;
            r->placement_nudge = vector_subtract(lifted, origin);
            ++r->placement_nudges;
            return DE_OK;
        }
        return DE_ERROR; // No partial correction commits.
    }

    DRESULT ServerAdapter::ResolveInitialActorOverlap(HOBJECT h)
    {
        trace_service("ResolveInitialActorOverlap");
        auto* r = objects_.get(h);
        if (!r || r->life != Life::Alive || r->attachment || !collision || !collision_ready ||
            !Objects::is_kind_of(r->type, objects_.find_class("AI_Mgr")) || !physics_vector(r->dimensions) ||
            r->dimensions.x <= 0 || r->dimensions.y <= 0 || r->dimensions.z <= 0)
            return DE_INVALIDPARAMS;
        if (!(r->state.m_Flags & FLAG_SOLID))
            return DE_OK;
        // The same bounded face-adjacent placement used during ground entry also
        // runs after all world objects exist. Later source rows can overlap an
        // earlier actor; this is initialization only, never runtime depenetration.
        const auto ids = objects_.snapshot();
        CollisionHit contact;
        const auto origin = r->state.m_Pos;
        const auto query = sweep_scene(h, origin, origin, r->dimensions, contact, ids);
        if (query == CollisionQuery::Invalid)
            return DE_ERROR;
        if (query == CollisionQuery::Hit && contact.start_solid && contact.object)
        {
            const auto* other = objects_.get(contact.object);
            if (other && (other->character_body || other->state.m_ObjectType == OT_MODEL || other->world_model))
            {
                const auto combined = vector_add(r->dimensions, other->dimensions);
                const auto center = object_bounds_center(*other);
                std::vector<DVector> candidates{{center.x - combined.x - .001f, origin.y, origin.z},
                                                {center.x + combined.x + .001f, origin.y, origin.z},
                                                {origin.x, origin.y, center.z - combined.z - .001f},
                                                {origin.x, origin.y, center.z + combined.z + .001f}};
                if (!other->character_body)
                {
                    candidates.push_back({origin.x, center.y + combined.y + .001f, origin.z});
                    candidates.push_back({origin.x, center.y - combined.y - .001f, origin.z});
                }
                auto distance = [&](DVector p)
                {
                    const auto delta = vector_subtract(p, origin);
                    return vector_dot(delta, delta);
                };
                std::stable_sort(candidates.begin(), candidates.end(),
                                 [&](DVector a, DVector b) { return distance(a) < distance(b); });
                const float limit = std::max({combined.x, combined.z, other->character_body ? 0.f : combined.y}) + .01f;
                bool placed = false;
                for (auto candidate : candidates)
                {
                    CollisionHit hit;
                    const auto delta = vector_subtract(candidate, origin);
                    // Shell interiors have no unique escape normal. Use the
                    // nearest clear face-adjacent candidate, bounded by the
                    // actor's dimensions rather than a potentially huge BSP.
                    // A later platform may appear after this actor already dropped
                    // below it. A vertical return may use the authored spawn as its
                    // bounded anchor, but only along the unchanged original XZ.
                    // The complete endpoint and swept route still must be clear.
                    const auto authored = r->properties.find("Pos");
                    const auto* anchor =
                        authored == r->properties.end() ? nullptr : std::get_if<DVector>(&authored->second);
                    const auto* actor = dynamic_cast<const AI_Mgr*>(r->game.get());
                    const bool restore_drop =
                        actor && actor->runtime_updates == 0 && !objects_.busy() && other->world_model && anchor &&
                        physics_vector(*anchor) && std::abs(anchor->x - origin.x) < .01f &&
                        std::abs(anchor->z - origin.z) < .01f && origin.y < anchor->y && candidate.x == origin.x &&
                        candidate.z == origin.z && candidate.y >= anchor->y &&
                        candidate.y - anchor->y <= r->dimensions.y + .01f;
                    const bool shell_limit =
                        other->world_model &&
                        (std::hypot(delta.x, delta.z) > 2 * std::min(r->dimensions.x, r->dimensions.z) + .01f ||
                         (std::abs(delta.y) > r->dimensions.y + .01f && !restore_drop));
                    if (shell_limit || distance(candidate) > limit * limit ||
                        sweep_scene(h, candidate, candidate, r->dimensions, hit, ids) != CollisionQuery::Miss ||
                        sweep_scene(h, origin, candidate, r->dimensions, hit, ids, other->handle, .01,
                                    StartOverlap::Leave) != CollisionQuery::Miss)
                        continue;
                    if (TeleportObject(h, &candidate) != DE_OK)
                        return DE_ERROR;
                    r->placement_nudge = vector_add(r->placement_nudge, vector_subtract(candidate, origin));
                    ++r->placement_nudges;
                    placed = true;
                    break;
                }
                if (!placed)
                    return DE_ERROR;
            }
        }
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
        // Separate authored wall/floor overlaps only at initial placement. Ceilings
        // that a normal descent leaves retain their original ground-drop path.
        CollisionHit descent;
        const auto below = DVector{r->state.m_Pos.x, r->state.m_Pos.y - 1, r->state.m_Pos.z};
        // Only when the descent itself starts blocked: overlaps it leaves (a ceiling) keep the
        // ordinary path, which lowers the body out of them.
        if ((r->state.m_Flags & (FLAG_SOLID | FLAG_GRAVITY)) && physics_vector(r->dimensions) &&
            collision->sweep(r->state.m_Pos, below, r->dimensions, descent, 0.01, StartOverlap::Leave) ==
                CollisionQuery::Hit &&
            descent.start_solid)
        {
            // A refused nudge leaves the original MoveObject below to report the contact.
            ResolveInitialWorldOverlap(h);
        }
        if (Objects::is_kind_of(r->type, objects_.find_class("AI_Mgr")) && ResolveInitialActorOverlap(h) != DE_OK)
            return DE_ERROR;
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
            if (query == CollisionQuery::Hit && !hit.start_solid && (!found || hit.fraction < nearest.fraction))
            {
                nearest = hit;
                found = true;
            }
        }
        if (!found)
            return DE_NOTFOUND;
        if (nearest.start_solid)
            return DE_ERROR;
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
