#include "game/server_adapter.h"
#include <algorithm>
#include <cmath>

namespace OpenBlood2
{
    const char* entry_state_name(EntryState s)
    {
        switch (s)
        {
            case EntryState::Created:
                return "Original instance";
            case EntryState::EditorOnly:
                return "Preprocessor only";
            case EntryState::Unsupported:
                return "Class not ported";
            case EntryState::Filtered:
                return "Original filter";
            case EntryState::Failed:
                return "Creation failed";
        }
        return "Unknown";
    }
    bool GameSession::request_world(std::string_view name, std::string& error)
    {
        WorldMessage message;
        if (name.find('\0') != std::string_view::npos ||
            !client_shell.StartNewWorld(std::string(name).c_str(), message))
        {
            error = "Invalid world request";
            return false;
        }
        return load_world(message, error);
    }
    bool GameSession::load_world(const WorldMessage& message, std::string& error)
    {
        if (objects.busy())
        {
            error = "World replacement during a callback is forbidden";
            return objects.fail(ObjectError::Busy, error);
        }
        WorldRequest request;
        World next;
        CollisionWorld next_collision;
        std::vector<std::uint8_t> bytes;
        // Validate everything before replacing the current inspectable world.
        if (!decode_world_request(message, request, error) || !resources.read(request.filename, bytes, error) ||
            !read_world(bytes, next, error))
            return false;
        if (next.models.empty() || !next_collision.build(next.models.front(), error))
            return false;
        if (!unload_world())
        {
            error = "World replacement during a callback is forbidden";
            return false;
        }
        collision = std::move(next_collision);
        server.collision_ready = true;
        server.service_calls.clear();
        server.trace_enabled = true;
        world = std::move(next);
        world_path = request.filename;
        server_shell.PreStartWorld();
        if (resources.find("models/characters/caleb.abc"))
            models.load("models/characters/caleb.abc", preview_model, preview_error);
        server.set_game_settings(request.difficulty, request.game_type);
        trace = {"Client StartNewWorld load-command excerpt -> CMSG_LOADWORLD (108)",
                 "Server LoadWorld read-order excerpt -> fresh single-player settings",
                 "REZ exact-path resolution -> DAT v56 structural validation",
                 "World properties -> original MID_PRECREATE / MID_INITIALUPDATE"};
        for (const auto& source : world.objects)
        {
            WorldEntry entry;
            // CF_NORUNTIME in original AppHeaders/engineobjects.c: these feed the
            // preprocessor and are not runtime classes missing from the adapter.
            if (source.class_name == "DirLight" || source.class_name == "Light" || source.class_name == "OutsideDef" ||
                source.class_name == "ObjectLight" || source.class_name == "LightAnimator" ||
                source.class_name == "GlobalDirLight")
            {
                entry.state = EntryState::EditorOnly;
                entry.detail = "Original CF_NORUNTIME; source properties retained";
            }
            else if (auto type = objects.find_class(source.class_name))
            {
                Properties props;
                for (const auto& p : source.properties)
                    props.emplace(p.name, p.value);
                auto h = objects.create(type, {}, Creation::World, props);
                if (const auto* r = objects.get(h); r && r->life == Life::Alive)
                {
                    entry.object = r->id;
                    entry.state = EntryState::Created;
                }
                else if (r && dynamic_cast<const B2BaseClass*>(r->game.get()) &&
                         static_cast<const B2BaseClass*>(r->game.get())->filtered)
                {
                    entry.state = EntryState::Filtered;
                    entry.detail = "Removed by original initial-update difficulty/game-type rules";
                }
                else
                {
                    entry.state = EntryState::Failed;
                    entry.detail = objects.diagnostics().empty() ? "Creation failed" : objects.diagnostics().back();
                }
            }
            else
            {
                entry.detail = "Required original class is not registered";
            }
            entries.push_back(std::move(entry));
        }
        objects.finish_frame();
        if (!server_shell.PostStartWorld())
        {
            error = "Original shell path snapshot failed";
            server.trace_enabled = false;
            unload_world();
            return false;
        }
        server.trace_enabled = false;
        state = SessionState::Inspection;
        std::map<std::string, std::string> requested;
        auto use = [&](const std::string& path, const char* purpose)
        {
            if (!path.empty())
                requested.emplace(path, purpose);
        };
        for (const auto& model : world.models)
            for (const auto& path : model.textures)
                use(path, "BSP texture");
        for (const auto& source : world.objects)
            for (auto key : {"Filename", "Skin", "SoundName", "SpriteSurfaceName", "OpenBusySound", "OpenStopSound",
                             "CloseBusySound", "CloseStopSound", "LockedSound", "UnlockedSound"})
                if (const auto* p = source.find(key))
                    if (const auto* path = std::get_if<std::string>(p))
                        use(*path, key);
        for (auto id : objects.snapshot())
        {
            const auto& r = *objects.get(id);
            if (!r.container && !r.world_model)
                use(r.state.m_Filename, "Original object model");
            use(r.state.m_SkinName, "Original object skin");
        }
        for (const auto& [path, purpose] : requested)
            resource_uses.push_back({path, purpose, resources.find(path) != nullptr});
        trace.push_back(
            "Server PreStartWorld / PostStartWorld entry subset: path data copied; player entry available separately");
        return true;
    }
    bool GameSession::pump_player_messages(std::string& error)
    {
        if (objects.busy())
        {
            error = "Player message pump during object callback rejected";
            return false;
        }
        for (unsigned pass = 0; pass < 8; ++pass)
        {
            std::vector<PlayerEvent> events;
            if (!server.take_client_events(local_client, events))
            {
                error = "Player association lost";
                return false;
            }
            if (events.empty())
                return true;
            for (const auto& event : events)
                if (!client_shell.OnMessage(event, server_shell))
                {
                    error = "Original player entry message rejected";
                    return false;
                }
        }
        error = "Player entry exceeded bounded message rounds";
        return false;
    }
    bool GameSession::enter_player(std::string& error)
    {
        error.clear();
        if (objects.busy())
        {
            error = "Player entry during a callback rejected";
            return false;
        }
        if (world_path.empty() || !server.collision_ready)
        {
            error = "Load a collision world before player entry";
            return false;
        }
        if (local_client)
        {
            error = "A local client is already connected";
            return false;
        }
        if (server.GetVarValueFloat(server.GetGameConVar("GAME_GAMETYPE")) != 0)
        {
            error = "Player entry supports fresh single-player sessions only";
            return false;
        }
        // Preflight before creating original objects. Keep inspection usable on
        // missing assets and do not leave an ABC-less original player alive.
        std::shared_ptr<const Model> model;
        if (!models.load("models/characters/caleb.abc", model, error))
            return false;
        if (!resources.find("skins/characters/caleb1.dtx"))
        {
            error = "Caleb skin is not indexed";
            return false;
        }
        const auto before = objects.snapshot();
        const bool was_tracing = server.trace_enabled;
        server.trace_enabled = true;
        local_client = server.connect_client();
        bool ok =
            local_client && server_shell.OnClientEnterWorld(local_client) && client_shell.OnEnterWorld(local_client);
        if (ok)
            ok = pump_player_messages(error) && client_shell.Update() &&
                 client_shell.SyncWeapons(models, resources, server_shell, error);
        if (!ok)
        {
            if (error.empty())
                error = "Original player creation/start-point entry failed";
            leave_player();
            // Roll back children (including a scent or muzzle light) created by a
            // failed original callback, while keeping the prior inspection world.
            for (auto id : objects.snapshot())
                if (std::find(before.begin(), before.end(), id) == before.end())
                    objects.remove(objects.get(id)->handle);
            objects.finish_frame();
        }
        else
        {
            trace.push_back(
                "Server OnClientEnterWorld -> CreatePlayer -> CPlayerObj InitialUpdate/Respawn -> GoToStartPoint");
            trace.push_back("Client OnEnterWorld -> PLAYERINIT_SINGLEPLAYER -> SINGLEPLAYER_INIT -> original "
                            "attribute/respawn subset");
            trace.push_back(client_shell.blocker);
        }
        server.trace_enabled = was_tracing;
        return ok;
    }
    bool GameSession::leave_player()
    {
        if (objects.busy())
            return false;
        client_shell.OnExitWorld();
        if (local_client)
        {
            server_shell.OnClientExitWorld(local_client);
            if (!objects.finish_frame() || !server.disconnect_client(local_client))
                return false;
            local_client = nullptr;
        }
        return true;
    }
    bool GameSession::select_weapon(DBYTE slot, std::string& error)
    {
        error.clear();
        if (objects.busy())
        {
            error = "Weapon change during callback rejected";
            return false;
        }
        auto* player = server_shell.Player(local_client);
        if (!player || player->m_damage.IsDead() || !player->m_InventoryMgr.GetWeapon(slot))
        {
            error = "Weapon slot unavailable";
            return false;
        }
        auto& inventory = player->m_InventoryMgr;
        const auto previous = inventory.m_nCurWeaponSlot;
        inventory.ChangeWeaponSlot(slot);
        if (!client_shell.SyncWeapons(models, resources, server_shell, error))
        {
            inventory.ChangeWeaponSlot(previous);
            return false;
        }
        client_shell.weapon_input = {};
        return true;
    }
    bool GameSession::run_world(std::string& error)
    {
        if (state == SessionState::Empty)
        {
            error = "No world loaded";
            return false;
        }
        if (objects.busy())
        {
            error = "RunWorld during a callback rejected";
            return false;
        }
        std::string entry_error;
        if (!local_client)
            enter_player(entry_error);
        state = SessionState::Blocked;
        const auto missing =
            std::count_if(entries.begin(), entries.end(), [](const auto& e)
                          { return e.state == EntryState::Unsupported || e.state == EntryState::Failed; });
        error = "RunWorld blocked: " + std::to_string(missing) +
                " uninstantiated required objects; other weapons, pickups, specialized containers/FX, full combat "
                "reactions, "
                "audio, autosave and remaining mover classes remain unported";
        if (!entry_error.empty())
            error += "; player entry: " + entry_error;
        if (trace.empty() || trace.back() != error)
            trace.push_back(error);
        return false;
    }
    bool GameSession::unload_world()
    {
        if (objects.busy())
            return objects.fail(ObjectError::Busy, "World unload during a callback rejected");
        if (!leave_player())
            return false;
        // Destruct original game objects while the adapter and its string pool live.
        const bool was_tracing = server.trace_enabled;
        server.trace_enabled = true;
        const bool cleared = objects.clear();
        server.trace_enabled = was_tracing;
        if (!cleared)
            return false;
        server.reset_world_time();
        server_shell.PreStartWorld();
        models.clear();
        collision.clear();
        server.collision_ready = false;
        server.gravity = {0, -2000, 0};
        preview_model.reset();
        preview_animation = {};
        preview_events.clear();
        preview_error.clear();
        preview_playing = true;
        entries.clear();
        resource_uses.clear();
        world = {};
        world_path.clear();
        trace.clear();
        state = SessionState::Empty;
        return true;
    }
    bool GameSession::step(double seconds)
    {
        // Shell and original-object updates do not depend on inspector visibility.
        if (!std::isfinite(seconds) || seconds < 0 || seconds > 60)
            return objects.fail(ObjectError::InvalidTime, "Invalid session delta");
        if (objects.busy())
            return objects.fail(ObjectError::Busy, "Reentrant session step rejected");
        const bool was_tracing = server.trace_enabled;
        server.trace_enabled = true;
        server_shell.Update(static_cast<float>(seconds));
        bool ok = objects.advance(seconds) && server.advance_physics(seconds) && server.advance_models(seconds);
        if (local_client)
        {
            if (!server.GetClientUserData(local_client))
                ok = leave_player() && ok;
            else
            {
                std::string error;
                const bool updated = pump_player_messages(error) &&
                                     client_shell.UpdateMovement(server_shell, seconds, error) &&
                                     client_shell.Update() && client_shell.UpdateUse(server_shell) &&
                                     client_shell.SyncWeapons(models, resources, server_shell, error) &&
                                     client_shell.UpdateWeapons(server_shell, seconds, error);
                if (!server.GetClientUserData(local_client))
                    ok = leave_player() && ok;
                else if (!updated || !pump_player_messages(error))
                {
                    trace.push_back(error);
                    leave_player();
                    ok = false;
                }
            }
        }
        if (seconds > 0)
            ok = server.damage_containers() && ok;
        ok = objects.finish_frame() && ok;
        if (ok && preview_model && preview_playing &&
            !advance_animation(*preview_model, preview_animation, seconds, preview_events, preview_error))
            preview_playing = false;
        server.trace_enabled = was_tracing;
        return ok;
    }
} // namespace OpenBlood2
