#include "game/game_settings.h"
#include "game/original_globals.h"
#include "game/server_adapter.h"
#include "game/world_validation.h"
#include <cmath>
#include <limits>

namespace OpenBlood2
{
    bool GameSession::queue_level_change(const WorldMessage& message, std::string_view start, std::string& error)
    {
        error.clear();
        WorldRequest request;
        if (objects.busy() || ServerAdapter::active_ != &server || g_pServerDE != &server || !server.bound_)
        {
            error = "Level change requires the active session outside callbacks";
            return false;
        }
        if (!parse_world_request(message, request, error, resources.nightmares()))
            return false;
        if (request.load_type != LOADTYPE_NEW_LEVEL || start.size() > 100 || start.find('\0') != start.npos)
        {
            error = "Level change requires NEW_LEVEL and a bounded start-point name";
            return false;
        }
        // One owned request: later original exit messages replace earlier intent.
        // No pointer, HSTRING or captured player state crosses this boundary.
        pending_level_change = LevelChangeRequest{message, std::string(start)};
        return true;
    }

    LevelChangeResult GameSession::process_level_change(std::unique_ptr<GameSession>& current, std::string& error)
    {
        error.clear();
        if (!current || !current->pending_level_change)
            return LevelChangeResult::None;
        auto& source = *current;
        if (source.objects.busy() || ServerAdapter::active_ != &source.server || g_pServerDE != &source.server ||
            !source.server.bound_)
        {
            error = "Level commit requires the current owner outside callbacks";
            return LevelChangeResult::Rejected; // Do not consume an outer request from a reentrant call.
        }
        auto pending = std::move(*source.pending_level_change);
        source.pending_level_change.reset();
        LevelChangeReport report;
        report.attempts = source.level_change.attempts + 1;
        report.committed = source.level_change.committed;
        report.rejected = source.level_change.rejected;
        report.source_world = source.world_path;
        report.source_revision = source.world_revision;
        report.requested_start = pending.start;
        auto rejected = [&]
        {
            report.error = error;
            ++report.rejected;
            source.level_change = std::move(report);
            source.client_shell.world_exit.error = error;
            ++source.client_shell.world_exit.blocked;
            return LevelChangeResult::Rejected;
        };
        WorldRequest request;
        PlayerRestoreState saved;
        if (!parse_world_request(pending.message, request, error, source.resources.nightmares()))
            return rejected();
        if (request.load_type != LOADTYPE_NEW_LEVEL || pending.start.size() > 100 ||
            pending.start.find('\0') != pending.start.npos)
        {
            error = "Invalid queued NEW_LEVEL request";
            return rejected();
        }
        report.destination = request.filename;
        if (!source.capture_player(saved, error))
            return rejected();
        report.player_captured = true;
        // The server keeps its NEW_GAME settings across NEW_LEVEL (LoadWorld's SetGameInfo runs
        // only for LOADTYPE_NEW_GAME), so the destination takes the source's, not the message's.
        const float difficulty = source.server.GetVarValueFloat(source.server.GetGameConVar("GAME_DIFFICULTY"));
        const float game_type = source.server.GetVarValueFloat(source.server.GetGameConVar("GAME_GAMETYPE"));
        if (!std::isfinite(difficulty) || difficulty < 0 || difficulty > 3 || std::floor(difficulty) != difficulty ||
            !std::isfinite(game_type) || game_type < 0 || std::floor(game_type) != game_type ||
            !local_game_type(static_cast<unsigned>(game_type)) ||
            source.world_revision == std::numeric_limits<std::uint64_t>::max())
        {
            error = "Invalid current difficulty or game type, or exhausted world revision";
            return rejected();
        }
        WorldMessage fresh;
        if (!original_world_request(request.filename.c_str(), static_cast<DBYTE>(game_type), LOADTYPE_NEW_GAME,
                                    static_cast<DBYTE>(difficulty), fresh))
        {
            error = "Invalid normalized level request";
            return rejected();
        }
        OriginalGlobals source_globals;
        source_globals.exchange();
        source.server.bound_ = false;
        ServerAdapter::active_ = nullptr;
        g_pServerDE = nullptr;
        auto destination = std::make_unique<GameSession>();
        destination->resources = source.resources;
        const bool prepared = [&]
        {
            if (!destination->initialize() || !destination->load_world(fresh, error))
                return false;
            destination->client_shell.m_nLastLoadType = LOADTYPE_NEW_LEVEL; // The client's StartNewWorld type.
            report.world_loaded = true;
            if (!check_world_initialization(*destination, report, error))
                return false;
            if (!destination->restore_player(saved, error, PlayerRestoreMode::KeepAlive, pending.start))
                return false;
            report.player_entered = true;
            report.selected_start = destination->restoration.selected_start;
            report.start_fallback = destination->restoration.start_fallback;
            if (const auto* restored = destination->server_shell.Player(destination->local_client))
            {
                report.health = restored->m_damage.m_fHitPoints;
                report.armor = restored->m_damage.m_fArmorPoints;
                report.bullets = restored->m_InventoryMgr.GetAmmoCount(1);
                report.keys = restored->m_InventoryMgr.keys.size();
            }
            if (destination->pending_level_change)
            {
                error = "Destination start trigger requested another level during entry";
                return false;
            }
            if (!check_world_initialization(*destination, report, error) ||
                !check_world_presentation(*destination, report, error))
                return false;
            const auto* player = destination->server_shell.Player(destination->local_client);
            if (!player || player->m_damage.IsDead())
            {
                error = "Destination start trigger removed or killed the restored player";
                return false;
            }
            return true;
        }();
        if (!prepared)
        {
            destination.reset(); // Destruct all original objects/tables under THEIR adapter.
            report.cleanup_complete = !ServerAdapter::active_ && !g_pServerDE && source_globals.empty();
            source_globals.exchange();
            ServerAdapter::active_ = &source.server;
            g_pServerDE = &source.server;
            source.server.bound_ = true;
            if (error.empty())
                error = "Destination initialization failed";
            if (!report.cleanup_complete)
                error += "; destination cleanup invariant failed";
            return rejected();
        }
        // Keep the complete destination at stable addresses while the source's
        // original destructors run. The two worlds never traverse each other's lists.
        OriginalGlobals destination_globals;
        destination_globals.exchange();
        destination->server.bound_ = false;
        source_globals.exchange();
        ServerAdapter::active_ = &source.server;
        g_pServerDE = &source.server;
        source.server.bound_ = true;
        if (!source.server.audio.transfer_output_to(destination->server.audio, error))
        {
            source_globals.exchange();
            source.server.bound_ = false;
            destination_globals.exchange();
            ServerAdapter::active_ = &destination->server;
            g_pServerDE = &destination->server;
            destination->server.bound_ = true;
            destination.reset();
            report.cleanup_complete = !ServerAdapter::active_ && !g_pServerDE && source_globals.empty();
            source_globals.exchange();
            ServerAdapter::active_ = &source.server;
            g_pServerDE = &source.server;
            source.server.bound_ = true;
            return rejected();
        }
        // No fallible preparation remains. Replace ownership, never move a session,
        // reconstruct the old DAT, replay destination initialization or fresh-respawn.
        destination->client_shell.voice_manager.InheritHistory(source.client_shell.voice_manager);
        auto exit = source.client_shell.world_exit;
        ++exit.completed;
        exit.error.clear();
        destination->world_revision = source.world_revision + 1;
        destination->server_shell.start_point_name = pending.start;
        destination->client_shell.world_exit = std::move(exit);
        destination->client_shell.m_bFirstWorld = false;
        current.reset();
        report.cleanup_complete = !ServerAdapter::active_ && !g_pServerDE && source_globals.empty();
        destination_globals.exchange();
        ServerAdapter::active_ = &destination->server;
        g_pServerDE = &destination->server;
        destination->server.bound_ = true;
        report.source_retained = false;
        report.destination_revision = destination->world_revision;
        ++report.committed;
        if (!report.cleanup_complete)
            report.error = error = "Committed destination; source cleanup invariant failed";
        destination->level_change = std::move(report);
        current = std::move(destination);
        return LevelChangeResult::Committed;
    }
} // namespace OpenBlood2
