#include "game/original/AI_Mgr.h"
#include "game/original/AmmoPickups.h"
#include "game/original/CameraObj.h"
#include "game/original/ExitHint.h"
#include "game/original/SmellHint.h"
#include "game/original_globals.h"
#include "game/server_adapter.h"
#include <cmath>

namespace OpenBlood2
{
    bool GameSession::rehearse_destination(const WorldMessage& message, std::string_view start, std::string& error)
    {
        error.clear();
        if (objects.busy() || ServerAdapter::active_ != &server || g_pServerDE != &server || !server.bound_)
        {
            error = "Destination rehearsal requires the current bound session outside callbacks";
            return false; // Reentrant/foreign calls do not replace the outer report.
        }
        DestinationTrial next;
        next.attempts = destination_trial.attempts + 1;
        next.source_world = world_path;
        next.source_revision = world_revision;
        const auto fail = [&]
        {
            next.error = error;
            destination_trial = std::move(next);
            return false;
        };
        if (start.size() > 100 || start.find('\0') != std::string_view::npos)
        {
            error = "Invalid destination start-point name";
            return fail();
        }
        next.requested_start = start;
        WorldRequest request;
        PlayerRestoreState player;
        if (!parse_world_request(message, request, error) || !capture_player(player, error))
            return fail();
        next.destination = request.filename;
        // NEW_LEVEL preserves the current difficulty, unlike a fresh-game request.
        const float difficulty = server.GetVarValueFloat(server.GetGameConVar("GAME_DIFFICULTY"));
        if (!std::isfinite(difficulty) || difficulty < 0 || difficulty > 3 || std::floor(difficulty) != difficulty)
        {
            error = "Invalid current difficulty for destination rehearsal";
            return fail();
        }
        WorldMessage fresh;
        if (!original_world_request(request.filename.c_str(), 0, LOADTYPE_NEW_GAME, static_cast<DBYTE>(difficulty),
                                    fresh))
        {
            error = "Invalid normalized destination request";
            return fail();
        }
        OriginalGlobals globals;
        globals.exchange(); // Keeps source animation tables and their live voices owned.
        server.bound_ = false;
        ServerAdapter::active_ = nullptr;
        g_pServerDE = nullptr;
        // All early failures stay inside this lambda. Its session and original
        // destructors finish under their own adapter before restoring the source.
        const bool observed = [&]
        {
            GameSession trial;
            trial.resources = resources; // Copy the mounted index, never commercial payloads into the repository.
            if (!trial.initialize() || !trial.load_world(fresh, error))
                return false;
            next.world_loaded = true;
            for (std::size_t i = 0; i < trial.entries.size(); ++i)
            {
                const auto& original = trial.world.objects[i];
                const auto& result = trial.entries[i];
                DestinationTrialEntry row;
                row.class_name = original.class_name;
                if (const auto* value = original.find("Name"))
                    if (const auto* name = std::get_if<std::string>(value))
                        row.name = *name;
                row.state = result.state;
                row.detail = result.detail;
                switch (row.state)
                {
                    case EntryState::Created:
                        ++next.created;
                        break;
                    case EntryState::WorldSound:
                        ++next.sounds;
                        break;
                    case EntryState::CollectedPickup:
                        ++next.collected;
                        break;
                    case EntryState::EditorOnly:
                        ++next.editor_only;
                        break;
                    case EntryState::Filtered:
                        ++next.filtered;
                        break;
                    case EntryState::Unsupported:
                        ++next.unknown;
                        break;
                    case EntryState::AbsentOriginalClass:
                    case EntryState::MissingResource:
                    case EntryState::MissingWorldModel:
                        ++next.omitted;
                        break;
                    case EntryState::Failed:
                        ++next.failed;
                        break;
                }
                next.entries.push_back(std::move(row));
            }
            // This deliberately exercises original named entry even in a partial
            // inspection world. It does NOT make missing classes/services ready.
            next.player_entered =
                trial.restore_player(player, error, PlayerRestoreMode::KeepAlive, next.requested_start);
            next.selected_start = trial.restoration.selected_start;
            next.entry_camera_active = g_hActiveCamera != nullptr;
            if (auto* restored = trial.server_shell.Player(trial.local_client))
            {
                next.health = restored->m_damage.m_fHitPoints;
                next.armor = restored->m_damage.m_fArmorPoints;
                next.bullets = restored->m_InventoryMgr.GetAmmoCount(1);
                next.keys = restored->m_InventoryMgr.keys.size();
            }
            next.runtime_objects = trial.objects.snapshot().size();
            next.attachments = trial.objects.attachments().size();
            next.voices = trial.server.audio.voices().size();
            return next.player_entered;
        }();
        next.cleanup_complete = !ServerAdapter::active_ && !g_pServerDE && globals.empty();
        globals.exchange();
        ServerAdapter::active_ = &server;
        g_pServerDE = &server;
        server.bound_ = true;
        if (!next.cleanup_complete)
            error = "Destination rehearsal cleanup did not release all original globals";
        else if (!observed && error.empty())
            error = "Destination initialization/player entry failed";
        next.error = error;
        destination_trial = std::move(next);
        // Success means initialization/entry was observed and then destroyed.
        // It is never a completed or authorized level transfer.
        return observed && destination_trial.cleanup_complete;
    }
} // namespace OpenBlood2
