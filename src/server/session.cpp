#include "server/session.h"
#include <algorithm>
#include "game/game_settings.h"
#include "game/original/AI_Mgr.h"
#include "game/world_validation.h"

namespace OpenBlood2
{
    bool ServerSession::start(const Config& config, std::string_view name, unsigned mode, std::string& error)
    {
        error.clear();
        if (started_ || !network_game_type(mode) || config.demo || config.nightmares || !config.overlays.empty())
        {
            error = "Dedicated startup requires a fresh retail multiplayer session without mods";
            return false;
        }
        WorldRequest request;
        request.game_type = DBYTE(mode);
        if (!world_resource_path(name, false, request.filename) || !request.filename.starts_with("worlds/multi/") ||
            (mode == GAMETYPE_CTF ? !request.filename.starts_with("worlds/multi/ctf_")
                                  : !request.filename.starts_with("worlds/multi/bb_")))
        {
            error = "Select a retail worlds/multi/bb_ map (Bloodbath/Team) or ctf_ map (CTF)";
            return false;
        }
        if (!game_.initialize())
        {
            error = "Cannot bind original server objects";
            return false;
        }
        // No local presentation receiver in a dedicated process. Immediate effect
        // creation can request impact sounds before the client-only update phase.
        game_.server.client_audio = nullptr;
        if (!game_.resources.mount_game(config, error) ||
            !Net::content_fingerprint(game_.resources, fingerprint_, error) || !game_.load_request(request, error))
            return false;
        WorldValidation report;
        if (!check_world_initialization(game_, report, error))
        {
            for (const auto& entry : report.entries)
                if (entry.state == EntryState::Failed || entry.state == EntryState::Unsupported)
                    error += "\n" + entry.class_name + " " + entry.name + ": " + entry.detail;
            return false;
        }
        started_ = true;
        return true;
    }
    bool ServerSession::tick(std::string& error)
    {
        error.clear();
        if (!started_)
        {
            error = "Server world is not started";
            return false;
        }
        if (!game_.step_server(ServerClock::step))
        {
            error = "Server simulation failed";
            for (const auto& detail : game_.step_errors())
                error += "\n" + detail;
            return false;
        }
        for (const auto id : game_.objects.snapshot())
            if (const auto* object = game_.objects.get(id); object && object->life == Life::Alive)
                if (const auto* ai = dynamic_cast<const AI_Mgr*>(object->game.get());
                    ai && !ai->runtime_error.empty() &&
                    std::find(reported_npcs_.begin(), reported_npcs_.end(), id) == reported_npcs_.end())
                {
                    reported_npcs_.push_back(id);
                    warnings_.push_back("Server NPC " + std::string(object->state.m_Name) + " suspended: " +
                                        ai->runtime_error);
                }
        return true;
    }
} // namespace OpenBlood2
