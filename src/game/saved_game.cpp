#include "game/game_settings.h"
#include "game/original/FlagObjects.h"
#include "game/original_globals.h"
#include "game/server_adapter.h"
#include "game/world_validation.h"
#include <limits>
#include <set>

// Saved games through a candidate session (save_load_contracts lot 4): the original shell
// SaveGame into the in-memory store, and loads that build a restore-mode candidate, isolate
// it like NEW_LEVEL and promote it, or destroy it and leave the current session untouched.
namespace OpenBlood2
{
    namespace
    {
        bool single_player(unsigned type)
        {
            return type == GAMETYPE_SINGLE || type == GAMETYPE_ACTION || type == GAMETYPE_CUSTOM;
        }
        // A saved console number: decimal digits only, no sign or spaces, at most `max`.
        bool console_number(const SaveImage& image, std::string_view name, unsigned max, unsigned& out)
        {
            for (const auto& variable : image.console)
                if (!CaseLess{}(variable.name, name) && !CaseLess{}(name, variable.name))
                {
                    if (variable.value.empty() || variable.value.size() > 2 ||
                        variable.value.find_first_not_of("0123456789") != std::string::npos)
                        return false;
                    out = static_cast<unsigned>(std::stoul(variable.value));
                    return out <= max;
                }
            return false;
        }
        constexpr const char* ctf_refusal = "CTF games cannot be saved (multiplayer, out of scope)";
        // The CTF state a save would lose: a flag of either team, a flag stand, or a player on a
        // team or carrying a flag. FlagObject and FlagStand are NotSaved and never in a save.
        bool ctf_state(GameSession& game, std::string& why)
        {
            if (g_ctf.flags[0] || g_ctf.flags[1])
                why = "a team flag is in the world";
            for (auto id : game.objects.snapshot())
                if (const auto* r = game.objects.get(id); why.empty() && r && r->life == Life::Alive)
                {
                    if (dynamic_cast<const FlagObject*>(r->game.get()) || dynamic_cast<const FlagStand*>(r->game.get()))
                        why = std::string("CTF object ") + r->state.m_Name;
                    else if (const auto* p = dynamic_cast<const CPlayerObj*>(r->game.get());
                             p && (p->ctf_team || p->m_hFlagObject || p->m_hFlagAttach))
                        why = "a player is on a CTF team or carries a flag";
                }
            return !why.empty();
        }
        std::string joined(const std::set<std::string>& names)
        {
            std::string text;
            for (const auto& name : names)
                text += (text.empty() ? "" : ", ") + name;
            return text;
        }
    } // namespace

    bool GameSession::save_game(GameSave& out, std::string& error, DBYTE save_type)
    {
        error.clear();
        if (save_type != SAVETYPE_CURRENT && save_type != SAVETYPE_AUTO)
        {
            error = "Unknown save type";
            return false;
        }
        const auto refuse = [&](std::string why)
        {
            error = std::move(why);
            return false;
        };
        if (objects.busy() || ServerAdapter::active_ != &server || g_pServerDE != &server || !server.bound_)
            return refuse("A save requires the bound session outside callbacks");
        if (world_path.empty() || !server.collision_ready)
            return refuse("No world is loaded");
        if (pending_level_change)
            return refuse("A level change is pending");
        if (server.message_count() || objects.pending_count())
            return refuse("Live messages or deferred removals exist; save at a frame boundary");
        const float type = server.GetVarValueFloat(server.GetGameConVar("GAME_GAMETYPE"));
        if (type == static_cast<float>(GAMETYPE_CTF))
            return refuse(ctf_refusal);
        if (type < 0 || type > 9 || type != static_cast<float>(static_cast<unsigned>(type)) ||
            !single_player(static_cast<unsigned>(type)))
            return refuse("Saving is limited to single-player game types (D12)");
        if (resources.demo())
            return refuse("The demo edition has no saves (decision 7)");
        // Original MenuSaveGame: only in the world and alive; the save carries the local player.
        auto* player = server_shell.Player(local_client);
        if (!local_client || !client_shell.m_bInWorld || !player || client_shell.player != player->m_hObject ||
            !server.IsObjectAlive(player->m_hObject) || player->m_bDead || player->m_damage.IsDead() ||
            !player->attributes_received)
            return refuse("A save requires the live local player in the world");
        // CTF match state (the flags, their stands and a player's team or carried flag) is
        // multiplayer state the original never saves: refuse it by name before the save list.
        if (std::string why; ctf_state(*this, why))
            return refuse(std::string(ctf_refusal) + ": " + why);
        // A live object of a saveable class without the original marking would be left out of
        // the shell's list and lost silently: refuse it by name instead.
        std::set<std::string> unmarked;
        for (auto id : objects.snapshot())
            if (const auto* r = objects.get(id); r && r->life == Life::Alive)
            {
                const auto support = save_support(r->type);
                if ((support == SaveSupport::Original || support == SaveSupport::Unsupported) &&
                    !(r->user_flags & USRFLG_SAVEABLE))
                    unmarked.insert(r->type->name);
            }
        if (!unmarked.empty())
            return refuse("Objects of saveable classes are not marked USRFLG_SAVEABLE: " + joined(unmarked));
        // The fingerprint first: once SaveGame replaced Current, nothing else may fail.
        GameSave next;
        next.world_path = world_path;
        SaveError fingerprint;
        if (!build_fingerprint(resources, world_path, {}, next.fingerprint, fingerprint))
            return refuse("Fingerprint: " + fingerprint.text);
        // CMSG_SAVEGAME: the client nests its SaveClientData blob in the request, and the server
        // attaches it to the sender's player before SaveGame; the player's Save consumes it.
        const auto request = server.StartHMessageWrite();
        if (!request)
            return refuse("The save request message was refused");
        server.WriteToMessageString(request, world_path.c_str()); // m_szFilename, ignored by the server.
        server.WriteToMessageByte(request, save_type);
        client_shell.SaveClientData(request);
        const bool saved = server_shell.OnSaveGameMessage(local_client, request);
        server.EndHMessageWrite(request);
        player->ClearClientSaveData(); // A refused save never consumed it.
        if (!saved)
            return refuse("SaveObjects: " + server.last_save.error);
        next.image =
            *server.save_store.find(save_type == SAVETYPE_AUTO ? SaveStoreSlot::Auto : SaveStoreSlot::Current);
        // SaveObjects wrote the schema of the classes present: unique, ascending and versioned.
        next.fingerprint.classes = next.image.classes;
        next.fingerprint.class_schema = class_schema_digest(next.fingerprint.classes);
        out = std::move(next);
        ++game_saves;
        return true;
    }

    bool GameSession::attach_saved_player(std::string& error)
    {
        error.clear();
        const auto restored = server.last_restore.local_client;
        std::size_t players = 0;
        for (auto id : objects.snapshot())
            if (const auto* r = objects.get(id);
                r && r->life == Life::Alive && dynamic_cast<const CPlayerObj*>(r->game.get()))
                ++players;
        auto* player = restored ? dynamic_cast<CPlayerObj*>(server.HandleToObject(restored)) : nullptr;
        if (!restored)
            error = "The save holds no local player (D6)";
        else if (players != 1 || !player)
            error = "A saved game holds exactly one local player (D6); found " + std::to_string(players) + " players";
        else if (player->m_bDead || player->m_damage.IsDead())
            error = "Restoring a dead player and its independent corpse is unavailable";
        if (!error.empty())
            return false;
        // OnClientEnterWorld's saved-client branch: SetClient, PLAYERINIT and FORCEROTATION (the
        // client reads the eye level from the player), never GoToStartPoint or a fresh profile.
        local_client = server.connect_client();
        if (!local_client || !server_shell.OnRestoredClientEnterWorld(local_client, restored))
        {
            error = player->entry_error.empty() ? "Restored client association failed" : player->entry_error;
            return false;
        }
        if (!client_shell.OnEnterWorld(local_client, true))
        {
            error = "Client entry of the restored player failed";
            return false;
        }
        // SMSG_CLIENTSAVEDATA, queued by the player's Load, reaches LoadClientData after the entry reset.
        if (const auto data = server.take_client_save_data())
        {
            const bool loaded = client_shell.LoadClientData(data, error);
            server.EndHMessageRead(data);
            if (!loaded)
                return false;
        }
        if (!pump_player_messages(error) || !client_shell.Update() ||
            !client_shell.SyncWeapons(models, resources, server_shell, error) ||
            !player->m_InventoryMgr.UpdateHandModels() || !objects.finish_frame() || !objects.refresh_attachments() ||
            !server.IsObjectAlive(restored) || server.GetClientUserData(local_client) != restored)
        {
            if (error.empty())
                error = "Restored player entry failed";
            return false;
        }
        trace.push_back("Restored player -> saved-client shell branch (FORCEROTATION, no start point) -> "
                        "SMSG_CLIENTSAVEDATA -> LoadClientData");
        return true;
    }

    bool GameSession::load_game(std::unique_ptr<GameSession>& current, const GameSave& save, std::string& error,
                                std::span<const ClassInfo* const> extra_classes)
    {
        error.clear();
        if (!current || current->objects.busy() || ServerAdapter::active_ != &current->server ||
            g_pServerDE != &current->server || !current->server.bound_)
        {
            error = "A load requires the bound session outside callbacks";
            return false; // A foreign or reentrant call keeps the outer report.
        }
        auto& source = *current;
        GameLoadReport report;
        report.attempts = source.game_load.attempts + 1;
        report.committed = source.game_load.committed;
        report.rejected = source.game_load.rejected;
        report.source_world = source.world_path;
        report.source_revision = source.world_revision;
        report.source_store_revision = source.server.save_store.revision();
        report.world = save.world_path;
        const auto rejected = [&](const char* stage)
        {
            report.stage = stage;
            report.error = error;
            ++report.rejected;
            source.game_load = std::move(report);
            return false;
        };

        // 1. Validate the save against this installation before anything changes.
        unsigned game_type = 0, difficulty = 0;
        std::string normalized;
        if (source.pending_level_change)
            error = "A level change is pending";
        else if (source.world_revision == std::numeric_limits<std::uint64_t>::max())
            error = "World revisions are exhausted";
        else if (source.resources.demo())
            error = "The demo edition has no saves (decision 7)";
        else if (!resource_path(save.world_path, normalized) || normalized != save.world_path ||
                 !normalized.starts_with("worlds/") || !normalized.ends_with(".dat"))
            error = "The save's world path is not a normalized world DAT";
        else if (console_number(save.image, "GAME_GAMETYPE", 9, game_type) && game_type == GAMETYPE_CTF)
            error = "CTF saves cannot be loaded (multiplayer, out of scope)";
        else if (!save.image.console_saved || save.image.param != 0 ||
                 !console_number(save.image, "GAME_GAMETYPE", 9, game_type) || !single_player(game_type) ||
                 !console_number(save.image, "GAME_DIFFICULTY", 3, difficulty))
            error = "The save is not a full single-player save with its console (D12)";
        else if (save.fingerprint.classes != save.image.classes)
            error = "The fingerprint's class schema differs from the saved records";
        if (!error.empty())
            return rejected("admission");
        Objects registry; // The build's class schema, including any extra classes.
        bool registered = register_original_classes(registry);
        for (const auto* type : extra_classes)
            registered = registered && type && registry.register_class(*type);
        SaveFingerprint installed;
        SaveError fingerprint;
        if (!registered)
            error = "The class schema cannot be built";
        else if (!build_fingerprint(source.resources, save.world_path, save.fingerprint.classes, installed,
                                    fingerprint) ||
                 !check_fingerprint(save.fingerprint, installed, save_class_schema(registry), fingerprint))
            error = fingerprint.text;
        if (!error.empty())
            return rejected("fingerprint");

        // 2. Isolate the source's original globals and binding, as NEW_LEVEL does.
        OriginalGlobals source_globals;
        source_globals.exchange();
        source.server.bound_ = false;
        ServerAdapter::active_ = nullptr;
        g_pServerDE = nullptr;
        const auto resume_source = [&]
        {
            source_globals.exchange();
            ServerAdapter::active_ = &source.server;
            g_pServerDE = &source.server;
            source.server.bound_ = true;
        };

        // 3. The candidate: the mounted index and its own copy of the logical store, with the
        // loaded image as Current. The source's store stays untouched until promotion (D2).
        auto candidate = std::make_unique<GameSession>();
        candidate->resources = source.resources;
        candidate->server.save_store = source.server.save_store;
        candidate->server.save_store.put(SaveStoreSlot::Current, save.image);
        const char* stage = "candidate";
        const bool prepared = [&]
        {
            if (!candidate->initialize())
            {
                error = "Candidate session initialization failed";
                return false;
            }
            for (const auto* type : extra_classes)
                if (!candidate->objects.register_class(*type))
                {
                    error = "Candidate class registration failed";
                    return false;
                }
            // 4-8. Restore-mode world, then RestoreObjects with no step in between.
            stage = "world";
            WorldRequest request{save.world_path, static_cast<DBYTE>(game_type), LOADTYPE_RESTORESAVE,
                                 static_cast<DBYTE>(difficulty)};
            if (!candidate->load_saved_world(request, error))
            {
                if (!candidate->server.last_restore.file.empty())
                    stage = "restore";
                return false;
            }
            report.world_loaded = report.restored = true;
            report.records = candidate->server.last_restore.records;
            // 10. Exactly one saved local player (D6), reattached through the restored-client branch.
            stage = "player";
            if (!candidate->attach_saved_player(error))
                return false;
            // 11. The checks of checked startup and NEW_LEVEL.
            stage = "validation";
            if (!check_world_initialization(*candidate, report, error))
                return false;
            if (!check_restored_publications(*candidate, error))
            {
                error = "Restored publications: " + error;
                return false;
            }
            for (auto id : candidate->objects.snapshot())
                if (const auto* r = candidate->objects.get(id); r && r->life == Life::Alive)
                    if (const auto* ai = dynamic_cast<const AI_Mgr*>(r->game.get()); ai && ai->runtime_failed)
                    {
                        error = "Original NPC failure: " + std::string(r->state.m_Name) + ": " + ai->runtime_error;
                        return false;
                    }
            // RunWorld's player checks: the attached player must be live and associated.
            const auto* player = candidate->server_shell.Player(candidate->local_client);
            if (!candidate->client_shell.m_bInWorld || !player || candidate->client_shell.player != player->m_hObject ||
                !candidate->server.IsObjectAlive(player->m_hObject) || player->m_bDead || player->m_damage.IsDead())
                error = "RunWorld requires a live player associated with the original client";
            else if (candidate->pending_level_change)
                error = "A restored object requested a level change";
            else if (candidate->server.message_count())
                error = "Live messages remain after the restore";
            else if (candidate->client_shell.music.failures || !candidate->server.audio.music_error.empty())
                error = "Music: " + candidate->client_shell.music.error + " " + candidate->server.audio.music_error;
            if (!error.empty() || !check_world_presentation(*candidate, report, error))
                return false;
            candidate->state = SessionState::Ready;
            return true;
        }();
        if (!prepared)
        {
            candidate.reset(); // Destruct every candidate object and table under ITS adapter.
            report.cleanup_complete = !ServerAdapter::active_ && !g_pServerDE && source_globals.empty();
            resume_source();
            if (error.empty())
                error = "Saved-game load failed";
            if (!report.cleanup_complete)
                error += "; candidate cleanup invariant failed";
            return rejected(stage);
        }

        // 12. Promote exactly like NEW_LEVEL: audio output is the last fallible step.
        OriginalGlobals candidate_globals;
        candidate_globals.exchange();
        candidate->server.bound_ = false;
        resume_source();
        if (!source.server.audio.transfer_output_to(candidate->server.audio, error))
        {
            source_globals.exchange();
            source.server.bound_ = false;
            candidate_globals.exchange();
            ServerAdapter::active_ = &candidate->server;
            g_pServerDE = &candidate->server;
            candidate->server.bound_ = true;
            candidate.reset();
            report.cleanup_complete = !ServerAdapter::active_ && !g_pServerDE && source_globals.empty();
            resume_source();
            return rejected("audio");
        }
        // No fallible step remains. Replace ownership; never move a session or replay state.
        candidate->client_shell.voice_manager.InheritAcrossLoad(source.client_shell.voice_manager);
        candidate->world_revision = source.world_revision + 1;
        current.reset();
        report.cleanup_complete = !ServerAdapter::active_ && !g_pServerDE && source_globals.empty();
        candidate_globals.exchange();
        ServerAdapter::active_ = &candidate->server;
        g_pServerDE = &candidate->server;
        candidate->server.bound_ = true;
        report.source_retained = false;
        report.destination_revision = candidate->world_revision;
        report.destination_store_revision = candidate->server.save_store.revision();
        ++report.committed;
        if (!report.cleanup_complete)
            report.error = error = "Committed the load; source cleanup invariant failed";
        candidate->game_load = std::move(report);
        current = std::move(candidate);
        return error.empty();
    }
} // namespace OpenBlood2
