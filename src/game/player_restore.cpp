#include "game/original/CharacterProfiles.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
    namespace
    {
        bool validate_engine(GameSession& game, const PlayerRestoreState& s, std::string& error,
                             PlayerRestoreMode mode = PlayerRestoreMode::SavedPosition)
        {
            auto fail = [&](const char* why)
            {
                error = why;
                return false;
            };
            if (s.world.empty() || game.world_path.empty() ||
                (mode == PlayerRestoreMode::SavedPosition && s.world != game.world_path) ||
                !game.server.collision_ready || !game.server_shell.world_started || game.server.IsMultiplayerGame())
                return fail("Player restoration requires a loaded local-game world; saved-position mode also "
                            "requires the same world");
            if (s.name.size() > 100 || s.name.find('\0') != std::string::npos)
                return fail("Invalid saved player object name");
            std::string model_path, skin_path, expected_model, expected_skin;
            const auto* profile = character_profile(s.player.character);
            if (!profile)
                return fail("Unsupported saved character");
            resource_path(profile->model, expected_model);
            resource_path(profile->skin, expected_skin);
            if (!resource_path(s.model, model_path) || model_path != expected_model ||
                !resource_path(s.skin, skin_path) || skin_path != expected_skin || !game.resources.find(s.skin))
                return fail("Saved player model/skin is unavailable or unsupported");
            const auto norm = vector_dot(s.rotation.m_Vec, s.rotation.m_Vec) + s.rotation.m_Spin * s.rotation.m_Spin;
            if (!physics_vector(s.position) || !physics_vector(s.dimensions) || s.dimensions.x <= 0 ||
                s.dimensions.y <= 0 || s.dimensions.z <= 0 || !physics_vector(s.scale) || s.scale.x <= 0 ||
                s.scale.y <= 0 || s.scale.z <= 0 || !physics_vector(s.velocity) || !physics_vector(s.acceleration) ||
                !physics_vector(s.rotation.m_Vec) || !std::isfinite(norm) || std::abs(norm - 1) > .001f ||
                !std::isfinite(s.next_update) || s.next_update < 0 || s.next_update > 3600 ||
                s.eye_level != (s.crouched ? 12.0f : profile->eye))
                return fail("Invalid saved player transform, body, eye or update deadline");
            auto channel = [](float v) { return std::isfinite(v) && v >= 0 && v <= 1; };
            if (!channel(s.color.x) || !channel(s.color.y) || !channel(s.color.z) || !channel(s.alpha))
                return fail("Invalid saved player color/alpha");
            if (s.player.dead || s.damage.dead || s.damage.death_delay != 0)
                return fail("Restoring a dead/dying player and its independent corpse is unavailable");
            std::shared_ptr<const Model> model;
            if (!game.models.load(s.model, model, error))
                return false;
            if ((!s.hidden_nodes.empty() && s.hidden_nodes.size() != model->nodes.size()) ||
                std::any_of(s.hidden_nodes.begin(), s.hidden_nodes.end(), [](auto v) { return v > 1; }))
                return fail("Invalid saved player node visibility mask");
            auto track = s.animation;
            std::vector<std::uint32_t> events;
            // Validate without dispatching model keys or advancing the restored clock.
            if (!advance_animation(*model, track, 0, events, error))
                return false;
            return true;
        }
    } // namespace
    bool GameSession::capture_player(PlayerRestoreState& output, std::string& error)
    {
        error.clear();
        auto* player = server_shell.Player(local_client);
        const auto* record = player ? objects.get(player->m_hObject) : nullptr;
        if (objects.busy() || !record || record->life != Life::Alive || !record->client_movement ||
            !player->attributes_received || !client_shell.m_bInWorld || client_shell.player != record->handle ||
            record->attachment || record->world_body || !record->character_body || record->model_body ||
            record->body_properties.mass != player->m_damage.m_fMass || record->body_properties.friction != 0 ||
            record->body_properties.pushable || record->body_properties.rotating_debris ||
            record->body_properties.polygon_bounds || record->activity != OBJSTATE_ACTIVE || record->auto_deactivate ||
            !record->scheduled || !std::isfinite(record->due) || player->door_push_pending)
        {
            error = "Player capture requires an active associated player outside callbacks/pending pushes";
            return false;
        }
        PlayerRestoreState s;
        s.world = world_path;
        s.name = record->state.m_Name;
        s.model = record->state.m_Filename;
        s.skin = record->state.m_SkinName;
        s.position = record->state.m_Pos;
        s.rotation = record->state.m_Rotation;
        s.scale = record->state.m_Scale;
        s.dimensions = record->dimensions;
        s.velocity = record->velocity;
        s.acceleration = record->acceleration;
        s.color = record->color;
        s.alpha = record->alpha;
        s.animation = record->animation;
        s.hidden_nodes = record->hidden_nodes;
        s.flags = record->state.m_Flags;
        s.user_flags = record->user_flags;
        s.blocking_priority = record->blocking_priority;
        s.next_update = std::max(0.0, record->due - objects.time());
        s.crouched = player->crouched;
        s.eye_level = player->eye_level;
        if (!player->SaveFields(s.player, error) || !player->m_InventoryMgr.Save(s.inventory, error) ||
            !player->m_damage.Save(s.damage, error) || !validate_engine(*this, s, error))
            return false;
        output = std::move(s);
        return true;
    }
    bool GameSession::restore_player(const PlayerRestoreState& s, std::string& error, PlayerRestoreMode mode,
                                     std::string_view start_name)
    {
        ServiceTraceScope trace_scope(server.trace_enabled);
        error.clear();
        ++restoration.attempts;
        auto failed = [&]()
        {
            restoration.error = error;
            return false;
        };
        if (objects.busy() || local_client || server.client_count() || client_shell.m_bInWorld)
        {
            error = "Leave the current player before restoring; callback/live-client replacement is unavailable";
            return failed();
        }
        if ((mode != PlayerRestoreMode::SavedPosition && mode != PlayerRestoreMode::KeepAlive) ||
            start_name.size() > 100 || start_name.find('\0') != std::string_view::npos ||
            (mode == PlayerRestoreMode::SavedPosition && !start_name.empty()))
        {
            error = "Invalid player restoration mode or start-point name";
            return failed();
        }
        const bool keep_alive = mode == PlayerRestoreMode::KeepAlive;
        const std::string owned_start(start_name);
        if (!validate_engine(*this, s, error, mode))
            return failed();
        const auto before = objects.snapshot();
        auto rollback = [&]()
        {
            leave_player();
            // Includes partially initialized owners and generated muzzle/hand children.
            for (auto id : objects.snapshot())
                if (std::find(before.begin(), before.end(), id) == before.end())
                    objects.remove(objects.get(id)->handle);
            objects.finish_frame();
            if (error.empty())
                error = "Restored player entry failed";
            return failed();
        };
        auto object = objects.create(objects.find_class("CPlayerObj"), {}, Creation::Restore);
        auto* record = objects.get(object);
        auto* player = record ? dynamic_cast<CPlayerObj*>(record->game.get()) : nullptr;
        if (!player || record->life != Life::Alive || !player->initialized)
            return rollback();
        // Replacement engine restore precedes original aggregate/value/Load-tail code.
        // No callbacks or world stepping can observe this unassociated staged owner.
        SAFE_STRCPY(record->state.m_Name, s.name.c_str());
        record->state.m_Pos = s.position;
        record->state.m_Scale = s.scale;
        record->state.m_Flags = s.flags;
        record->dimensions = s.dimensions;
        record->velocity = s.velocity;
        record->acceleration = s.acceleration;
        record->user_flags = s.user_flags;
        record->blocking_priority = s.blocking_priority;
        if (server.SetModelFilenames(object, s.model.c_str(), s.skin.c_str()) != DE_OK ||
            server.SetObjectRotation(object, &s.rotation) != DE_OK)
            return rollback();
        record->color = s.color;
        record->alpha = s.alpha;
        record->animation = s.animation;
        record->hidden_nodes = s.hidden_nodes;
        if (!keep_alive && server.ResizeObject(object, s.dimensions) != ResizeResult::Resized)
        {
            error = "Saved player body is blocked in the current world";
            return rollback();
        }
        if (!player->m_InventoryMgr.Load(
                s.inventory, keep_alive ? InventoryLoadType::KeepAlive : InventoryLoadType::Restore, error) ||
            !player->m_damage.Load(s.damage, error) ||
            !server.ConfigureCharacterBody(object, player->m_damage.m_fMass, 0) ||
            !player->LoadFields(s.player, error) || !player->RestoreModelState(error))
            return rollback();
        player->crouched = s.crouched;
        player->eye_level = s.eye_level;
        player->attributes_received = true; // Loaded profile, never a fresh PlayerInit message.
        player->restore_engine_loaded = true;
        record->due = objects.time() + s.next_update;
        record->scheduled = true;
        local_client = server.connect_client();
        if (!local_client ||
            !server_shell.OnRestoredClientEnterWorld(local_client, object, keep_alive ? owned_start.c_str() : nullptr))
        {
            error = player->entry_error.empty() ? "Restored client/start-point entry failed" : player->entry_error;
            return rollback();
        }
        if (keep_alive && server.ResizeObject(object, s.dimensions) != ResizeResult::Resized)
        {
            error = "Destination start-point body is blocked";
            return rollback();
        }
        if (!client_shell.OnEnterWorld(local_client, true) || !pump_player_messages(error) || !client_shell.Update() ||
            !client_shell.SyncWeapons(models, resources, server_shell, error) ||
            !player->m_InventoryMgr.UpdateHandModels() || !objects.finish_frame() || !objects.refresh_attachments())
            return rollback();
        record = objects.get(object);
        if (!record || record->life != Life::Alive || server.GetClientUserData(local_client) != object)
        {
            error = "Restored player removed during child cleanup";
            return rollback();
        }
        restoration.last_mode = mode;
        restoration.destination_world = world_path;
        restoration.requested_start = owned_start;
        restoration.selected_start = player->entry_start_name;
        restoration.start_fallback = player->entry_start_fallback;
        ++restoration.restored;
        restoration.last_player = record->id;
        restoration.error.clear();
        trace.push_back(
            keep_alive ? "Kept player -> inventory KeepAlive (keys discarded) -> original named start entry; no Respawn"
                       : "Restored player engine/aggregates -> original Load tail -> saved-client shell branch; no "
                         "Respawn/start trigger");
        return true;
    }
} // namespace OpenBlood2
