#include "game/original/AI_Mgr.h"
#include "game/original/BuiltinSprite.h"
#include "game/original/CharacterProfiles.h"
#include "game/original/ClientParticleStreamSFX.h"
#include "game/original/DestructableModel.h"
#include "game/original/DetailSprite.h"
#include "game/original/SkyObjects.h"
#include "game/original/Sound.h"
#include "game/server_adapter.h"
#include "game/world_validation.h"
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
            case EntryState::WorldSound:
                return "World-owned sound";
            case EntryState::EditorOnly:
                return "Preprocessor only";
            case EntryState::Unsupported:
                return "Class not ported";
            case EntryState::Filtered:
                return "Original filter";
            case EntryState::MissingWorldModel:
                return "Omitted: missing world model";
            case EntryState::AbsentOriginalClass:
                return "Omitted: absent original class";
            case EntryState::MissingResource:
                return "Omitted: missing authored resource";
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
        PreparedWorld next;
        // Structural CPU preparation is shared with read-only destination inspection.
        if (!decode_world_request(message, request, error) || !prepare_world(request, resources, next, error))
            return false;
        if (!unload_world())
        {
            error = "World replacement during a callback is forbidden";
            return false;
        }
        collision = std::move(next.collision);
        server.collision_ready = true;
        server.service_calls.clear();
        server.unavailable_history.clear();
        server.trace_enabled = true;
        world = std::move(next.world);
        world_path = request.filename;
        server.SetGameConVar("GAME_WORLDNAME", world_path.c_str());
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
            if (editor_only_world_class(source.class_name))
            {
                entry.state = EntryState::EditorOnly;
                entry.detail = "Original CF_NORUNTIME; source properties retained";
            }
            else if (absent_original_world_class(source.class_name))
            {
                entry.state = EntryState::AbsentOriginalClass;
                entry.detail = "No original base-game class registration; loader skips this source row";
            }
            else if (auto type = objects.find_class(source.class_name))
            {
                Properties props;
                for (const auto& p : source.properties)
                    props.emplace(p.name, p.value);
                auto text = [&](const char* key) -> std::string_view
                {
                    const auto* p = source.find(key);
                    const auto* value = p ? std::get_if<std::string>(p) : nullptr;
                    return value ? *value : std::string_view{};
                };
                const bool empty_model = source.class_name == "CDestructableModel" && text("InitFilename").empty() &&
                                         text("Filename").empty();
                std::string sprite_path;
                const bool missing_splat =
                    source.class_name == "DetailSprite" && resource_path(text("Filename"), sprite_path) &&
                    (sprite_path == "sprites/bloodsplat1.dtx" || sprite_path == "sprites/bloodsplat2.dtx" ||
                     sprite_path == "sprites/bloodsplat3.dtx") &&
                    !resources.find(sprite_path);
                auto h = objects.create(type, {}, Creation::World, props);
                if (const auto* r = objects.get(h); r && r->life == Life::Alive)
                {
                    entry.object = r->id;
                    entry.state = EntryState::Created;
                }
                else if (const auto* sound = r ? dynamic_cast<const Sound*>(r->game.get()) : nullptr)
                {
                    entry.sound = sound->sound;
                    entry.state = server.audio.get(sound->sound) ? EntryState::WorldSound : EntryState::Failed;
                    entry.detail =
                        entry.sound ? "Original Sound removed after transferring its loop to the world" : sound->error;
                }
                else if (const auto* sky = r ? dynamic_cast<const SkyObject*>(r->game.get()) : nullptr;
                         sky && !sky->error.empty())
                {
                    entry.state = EntryState::Failed;
                    entry.detail = sky->error;
                }
                else if (const auto* ai = r ? dynamic_cast<const AI_Mgr*>(r->game.get()) : nullptr;
                         ai && !ai->runtime_error.empty())
                {
                    entry.state = EntryState::Failed;
                    entry.detail = ai->runtime_error;
                }
                else if (const auto* sprite = r ? dynamic_cast<const DetailSprite*>(r->game.get()) : nullptr;
                         sprite && !sprite->error.empty())
                {
                    entry.state = EntryState::Failed;
                    entry.detail = sprite->error;
                }
                else if (const auto* sprite = r ? dynamic_cast<const BuiltinSprite*>(r->game.get()) : nullptr;
                         sprite && !sprite->error.empty())
                {
                    entry.state = EntryState::Failed;
                    entry.detail = sprite->error;
                }
                else if (const auto* stream =
                             r ? dynamic_cast<const CClientParticleStreamSFX*>(r->game.get()) : nullptr;
                         stream && !stream->error.empty())
                {
                    entry.state = EntryState::Failed;
                    entry.detail = stream->error;
                }
                else if (const auto* model = r ? dynamic_cast<const CDestructableModel*>(r->game.get()) : nullptr;
                         model && !model->error.empty())
                {
                    entry.state = EntryState::Failed;
                    entry.detail = model->error;
                    if (empty_model && model->error == "Model clip-zero dimensions unavailable")
                    {
                        entry.state = EntryState::MissingResource;
                        entry.detail = "Empty authored model filename; no replacement model";
                    }
                    if (!r->model_error.empty())
                        entry.detail += ": " + r->model_error;
                }
                else if (r && dynamic_cast<const B2BaseClass*>(r->game.get()) &&
                         static_cast<const B2BaseClass*>(r->game.get())->filtered)
                {
                    entry.state = EntryState::Filtered;
                    entry.detail = "Removed by original initial-update difficulty/game-type rules";
                }
                else if (!h && missing_splat && objects.error() == ObjectError::Unsupported &&
                         !objects.diagnostics().empty() &&
                         objects.diagnostics().back().find("Missing, oversized or non-SPR sprite resource:") !=
                             std::string::npos)
                {
                    entry.state = EntryState::MissingResource;
                    entry.detail = "Missing authored sprite: " + sprite_path + "; no path alias";
                }
                else if (!h && objects.error() == ObjectError::MissingWorldModel)
                {
                    entry.state = EntryState::MissingWorldModel;
                    entry.detail = objects.diagnostics().back() +
                                   "; engine rejected creation before initial update; world loading continues";
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
            error = "Original shell path snapshot failed: " + server_shell.paths.error();
            server.trace_enabled = false;
            unload_world();
            return false;
        }
        server.trace_enabled = false;
        state = SessionState::Inspection;
        std::map<std::string, std::string> music_properties;
        for (const char* key : {"MusicDirectory", "InstrumentFiles", "AmbientList", "CruisingList", "HarddrivingList"})
            if (const auto found = server.variables().find(key); found != server.variables().end())
                music_properties.emplace(key, found->second.text);
        music_catalog.inspect(resources.content_root(), music_properties);
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
            for (auto key :
                 {"Filename",      "Skin",           "SoundName",      "SpriteSurfaceName", "OpenBusySound",
                  "OpenStopSound", "CloseBusySound", "CloseStopSound", "LockedSound",       "UnlockedSound",
                  "InitFilename",  "InitSkin",       "DamageFilename", "DamageSkin",        "DestroyFilename",
                  "DestroySkin",   "SlidingSound",   "RampUpSound",    "RampDownSound",     "Sound"})
                if (const auto* p = source.find(key))
                    if (const auto* path = std::get_if<std::string>(p))
                        use(*path, key);
        for (auto id : objects.snapshot())
        {
            const auto& r = *objects.get(id);
            // Normal objects can retain a source filename that is a name, not a resource
            // (notably SkyPointer). Only model/sprite types consume this asset field.
            if (r.state.m_ObjectType == OT_MODEL || r.state.m_ObjectType == OT_SPRITE)
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
        ServiceTraceScope trace_scope(server.trace_enabled);
        if (objects.busy())
        {
            error = "Player message pump during object callback rejected";
            return false;
        }
        bool inspect_exit = false;
        for (unsigned pass = 0; pass < 8; ++pass)
        {
            std::vector<PlayerEvent> events;
            if (!server.take_client_events(local_client, events))
            {
                error = "Player association lost";
                return false;
            }
            if (events.empty())
            {
                if (inspect_exit)
                {
                    // At most one bounded destination scan per pump: inspect the final
                    // owned request, while preserving each source exit/capture/queue observation.
                    auto& exit = client_shell.world_exit;
                    std::string diagnostic;
                    if (!inspect_destination(exit.load, server_shell.start_point_name, diagnostic))
                        exit.error += "; destination scan: " + diagnostic;
                }
                return true;
            }
            for (const auto& event : events)
            {
                if (!client_shell.OnMessage(event, server_shell))
                {
                    error = "Original player entry message rejected";
                    return false;
                }
                auto& exit = client_shell.world_exit;
                if (exit.pending)
                {
                    exit.pending = false;
                    server_shell.CapturePlayerInventory(local_client);
                    server_shell.CapturePlayerDamage(local_client);
                    server_shell.CapturePlayerFields(local_client);
                    if (!queue_level_change(exit.load, server_shell.start_point_name, exit.error))
                        ++exit.blocked;
                    else
                        ++exit.queued;
                    inspect_exit = true;
                }
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
        const auto* profile = character_profile(client_shell.entry_profile.character);
        if (!profile)
        {
            error = "Invalid player profile";
            return false;
        }
        if (!models.load(profile->model, model, error))
            return false;
        if (!resources.find(profile->skin))
        {
            error = "Selected character skin is not indexed";
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
        ServiceTraceScope trace_scope(server.trace_enabled);
        if (objects.busy())
            return false;
        pending_level_change.reset();
        client_shell.OnExitWorld();
        if (state == SessionState::Ready)
            state = SessionState::Inspection;
        if (local_client)
        {
            server_shell.OnClientExitWorld(local_client);
            if (!objects.finish_frame() || !server.disconnect_client(local_client))
                return false;
            local_client = nullptr;
        }
        return true;
    }
    bool GameSession::command_weapon(DBYTE slot, std::string& error)
    {
        ServiceTraceScope trace_scope(server.trace_enabled);
        error.clear();
        auto* owner = server_shell.Player(local_client);
        // Original OnCommandOn dead/movement guard, then COMMAND_WEAPON_0..9.
        if (!client_shell.m_bInWorld || !owner || owner->m_bDead || owner->m_damage.IsDead() ||
            owner->movement_blocked || owner->camera_movement_blocked || slot >= 10)
        {
            error = "Weapon command blocked";
            return false;
        }
        const auto* current = owner->m_InventoryMgr.CurrentWeapon();
        const auto* requested = owner->m_InventoryMgr.GetWeapon(slot);
        // ChangeWeaponSlot's current-type no-op must not release held fire input.
        if (current && requested && current->data.m_nType == requested->data.m_nType)
            return true;
        return select_weapon(slot, error);
    }
    bool GameSession::select_weapon(DBYTE slot, std::string& error)
    {
        ServiceTraceScope trace_scope(server.trace_enabled);
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
        const auto changed = slot >= 10 ? inventory.SelectInventoryWeapon(WEAP_PROXIMITYBOMB + slot - 10)
                                        : inventory.ChangeWeaponSlot(slot);
        if (changed != InventoryResult::Ok && changed != InventoryResult::Current)
        {
            error = "Inventory weapon has no ammo";
            return false;
        }
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
        error.clear();
        if (objects.busy() || ServerAdapter::active_ != &server || g_pServerDE != &server || !server.bound_)
        {
            error = "RunWorld requires the active session outside callbacks";
            return false; // Reentrant/foreign calls must not overwrite the last report.
        }
        const auto attempts = startup.attempts + 1;
        startup = {};
        startup.attempts = attempts;
        startup.revision = world_revision;
        startup.time = objects.time();
        bool entered = false;
        const auto reject = [&]
        {
            startup.error = error;
            // Original start triggers are not transactional. Remove only a player
            // entered by this attempt; retain world mutations and the explicit report.
            if (entered && !leave_player())
                startup.error += "; failed player cleanup";
            state = world_path.empty() ? SessionState::Empty : SessionState::Blocked;
            if (trace.empty() || trace.back() != startup.error)
                trace.push_back(startup.error);
            error = startup.error;
            return false;
        };
        if (world_path.empty() || !server.collision_ready)
        {
            error = "No collision world loaded";
            return reject();
        }
        if (pending_level_change)
        {
            error = "RunWorld refused while a level change is pending";
            return reject();
        }
        if (!check_world_initialization(*this, startup, error))
            return reject();
        const auto check_actors = [&]
        {
            for (auto id : objects.snapshot())
                if (const auto* record = objects.get(id); record->life == Life::Alive)
                    if (const auto* ai = dynamic_cast<const AI_Mgr*>(record->game.get()); ai && ai->runtime_failed)
                    {
                        error = "Original NPC failure: " + std::string(record->state.m_Name) + ": " + ai->runtime_error;
                        return reject();
                    }
            return true;
        };
        if (!check_actors())
            return false;
        if (!local_client)
        {
            if (!enter_player(error))
            {
                error = "Player entry: " + error;
                return reject();
            }
            entered = startup.player_entered = true;
        }
        const auto* player = server_shell.Player(local_client);
        if (!client_shell.m_bInWorld || !player || client_shell.player != player->m_hObject ||
            !server.IsObjectAlive(player->m_hObject) || player->m_bDead || player->m_damage.IsDead())
        {
            error = "RunWorld requires a live player associated with the original client";
            return reject();
        }
        if (pending_level_change)
        {
            error = "Start trigger requested another level during entry";
            return reject();
        }
        if (!check_world_initialization(*this, startup, error) || !check_world_presentation(*this, startup, error))
            return reject();
        if (!check_actors())
            return false;
        if (client_shell.music.failures || !server.audio.music_error.empty())
        {
            error = "Original client music failed: " + client_shell.music.error + " " + server.audio.music_error;
            return reject();
        }
        state = SessionState::Ready;
        startup.accepted = true;
        trace.push_back("Original session startup validated; future simulation/resource requests remain checked");
        return true;
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
        if (g_pServerDE == &server)
            AI_Mgr::ResetRuntimeTables();
        server.reset_world_time();
        server_shell.PreStartWorld();
        server_shell.start_point_name.clear();
        models.clear();
        sprites.clear();
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
        music_catalog = {};
        world = {};
        ++world_revision;
        world_path.clear();
        trace.clear();
        state = SessionState::Empty;
        startup = {};
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
        const auto listener = client_shell.listener_position;
        const auto listener_rotation =
            client_shell.view.object && client_shell.view.settings.listener
                ? client_shell.view.rotation
                : rotation_from_euler({client_shell.rotation.pitch, client_shell.rotation.yaw, 0});
        bool ok = server.advance_audio(seconds, listener, listener_rotation) && objects.advance(seconds) &&
                  server.advance_physics(seconds) && server.advance_projectiles(seconds) &&
                  server.advance_models(seconds);
        if (local_client)
        {
            if (!server.GetClientUserData(local_client))
                ok = leave_player() && ok;
            else
            {
                std::string error;
                const bool updated = pump_player_messages(error) &&
                                     client_shell.UpdateMovement(server_shell, seconds, error) &&
                                     client_shell.Update() && client_shell.fade.Update(seconds) &&
                                     client_shell.UpdateZoom(seconds) && client_shell.UpdateUse(server_shell) &&
                                     client_shell.SyncWeapons(models, resources, server_shell, error) &&
                                     client_shell.UpdateWeapons(server_shell, seconds, error) &&
                                     (!server_shell.Player(local_client) ||
                                      server_shell.Player(local_client)->m_InventoryMgr.UpdateHandModels());
                if (!server.GetClientUserData(local_client))
                    ok = leave_player() && ok;
                else if (!updated || !pump_player_messages(error))
                {
                    if (error.empty())
                        error = "Original player update or held-weapon creation failed";
                    trace.push_back(error);
                    leave_player();
                    ok = false;
                }
            }
        }
        if (seconds > 0)
            ok = server.damage_containers() && ok;
        ok = objects.finish_frame() && ok;
        ok = objects.refresh_attachments() && ok;
        ok = server.advance_lights() && ok;
        auto* flash_owner = server_shell.Player(local_client);
        const auto* flash_item = flash_owner ? flash_owner->m_InventoryMgr.flashlight.get() : nullptr;
        ok = (!flash_item || flash_item->update_ok) && ok;
        ok = client_shell.flashlight.Update(server, flash_item ? flash_item->light : nullptr,
                                            flash_owner ? flash_owner->m_hObject : nullptr) &&
             ok;
        ok = server.explosion_sprites.advance(objects.time()) && ok;
        ok = server.blood_splats.advance(objects.time()) && ok;
        ok = server.weapon_sprites.advance(objects.time()) && ok;
        ok = server.explosion_models.advance(objects.time()) && ok;
        ok = server.laser_beams.advance(objects.time()) && ok;
        ok = server.explosion_lights.advance(objects.time()) && ok;
        ok = server.explosion_rings.advance(objects.time()) && ok;
        ok = server.explosion_flames.advance(objects.time()) && ok;
        ok = server.flesh_gibs.advance(objects.time(), server) && ok;
        ok = server.sparks.advance(objects.time()) && ok;
        ok = server.blood_spurts.advance(objects.time(), server) && ok;
        ok = server.explosion_fragments.advance(objects.time(), server) && ok;
        ok = server.object_particles.advance(objects.time(), server) && ok;
        ok = server.particle_streams.advance(objects.time(), server) && ok;
        ok = server.polygrids.advance(objects.time(), server) && ok;
        if (ok && preview_model && preview_playing &&
            !advance_animation(*preview_model, preview_animation, seconds, preview_events, preview_error))
            preview_playing = false;
        server.trace_enabled = was_tracing;
        return ok;
    }
    std::vector<std::string> GameSession::step_errors()
    {
        std::vector<std::string> out;
        const auto add = [&out](std::string_view label, const std::string& error)
        {
            if (!error.empty())
                out.push_back(std::string(label) + ": " + error);
        };
        add("Audio output", server.audio.output_error);
        add("Music", server.audio.music_error);
        add("Explosion sprites", server.explosion_sprites.update_error);
        add("Blood splats", server.blood_splats.update_error);
        add("Weapon sprites", server.weapon_sprites.update_error);
        add("Explosion models", server.explosion_models.update_error);
        add("Laser beams", server.laser_beams.update_error);
        add("Explosion lights", server.explosion_lights.update_error);
        add("Explosion rings", server.explosion_rings.update_error);
        add("Explosion flames", server.explosion_flames.update_error);
        add("Flesh gibs", server.flesh_gibs.update_error);
        add("Sparks", server.sparks.update_error);
        add("Blood spurts", server.blood_spurts.update_error);
        add("Explosion fragments", server.explosion_fragments.update_error);
        add("Object particles", server.object_particles.update_error);
        add("Particle streams", server.particle_streams.update_error);
        add("PolyGrids", server.polygrids.update_error);
        add("Flashlight effect", client_shell.flashlight.error);
        if (const auto* player = server_shell.Player(local_client))
            if (const auto* item = player->m_InventoryMgr.flashlight.get(); item && !item->update_ok)
                add("Flashlight item", item->error.empty() ? std::string("update failed") : item->error);
        return out;
    }
} // namespace OpenBlood2
