#include "server/session.h"
#include <algorithm>
#include "game/client_replication.h"
#include "game/game_settings.h"
#include "game/original/AI_Mgr.h"
#include "game/world_validation.h"
#include "net/transport.h"
#include <algorithm>
#include <atomic>

namespace OpenBlood2
{
    ServerSession::~ServerSession()
    {
        clear_players();
        game_.server.remote_event_sink = {};
    }
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
        static std::atomic<std::uint64_t> next_world{1};
        auto next = next_world.load(std::memory_order_relaxed);
        while (next && !next_world.compare_exchange_weak(next, next + 1, std::memory_order_relaxed))
        {
        }
        world_id_ = next;
        if (!world_id_)
        {
            error = "Server world identity exhausted";
            return false;
        }
        mode_ = mode;
        game_.server.remote_event_sink = [this](HCLIENT client, const PlayerEvent& event)
        { return capture_event(client, event); };
        started_ = true;
        return true;
    }
    bool ServerSession::join(const Net::JoinProfile& profile, Net::Joined& joined, std::string& error)
    {
        error.clear();
        if (!started_ || mode_ != GAMETYPE_BLOODBATH || !Net::valid_profile(profile) || !next_player_ ||
            players_.size() >= Net::max_players || game_.objects.busy())
        {
            error = "Invalid or unavailable Bloodbath player admission";
            return false;
        }
        const auto client = game_.server.connect_client();
        if (!client)
        {
            error = "No available original client connection";
            return false;
        }
        const auto connection = next_player_++;
        Player connected{};
        connected.id = connection;
        connected.client = client;
        players_.push_back(std::move(connected));
        PlayerInit init;
        init.name = profile.name;
        init.character = profile.character;
        init.skin = profile.skin;
        init.strength = profile.attributes[0];
        init.speed = profile.attributes[1];
        init.resistance = profile.attributes[2];
        init.magic = profile.attributes[3];
        init.weapons = profile.weapons;
        const auto failures = game_.objects.failure_count();
        auto* player = game_.server_shell.OnClientEnterWorld(client, &error);
        const auto object = player ? player->m_hObject : nullptr;
        const bool initialized = player && game_.server_shell.OnMessage(client, init);
        const auto* record = game_.objects.get(object);
        if (!initialized || !record || record->life != Life::Alive)
        {
            if (error.empty())
                error = record && record->life == Life::Alive && !player->entry_error.empty()
                            ? player->entry_error
                            : "Original player initialization failed";
            if (game_.objects.failure_count() != failures && !game_.objects.diagnostics().empty())
                error += ": " + game_.objects.diagnostics().back();
            game_.server_shell.OnClientExitWorld(client);
            game_.objects.finish_frame();
            game_.server.disconnect_client(client);
            players_.pop_back();
            return false;
        }
        const auto id = record->id;
        players_.back().object = id;
        joined = {world_id_, connection, id};
        return true;
    }
    bool ServerSession::snapshot(std::uint64_t player, Net::WorldSnapshot& out, std::string& error)
    {
        const auto found =
            std::find_if(players_.begin(), players_.end(), [&](const auto& p) { return p.id == player; });
        if (!started_ || !next_snapshot_ || found == players_.end())
        {
            error = "Snapshot recipient is unavailable";
            return false;
        }
        const auto* record = game_.objects.get(found->object);
        const auto* owner = game_.server_shell.Player(found->client);
        if (!record || record->life != Life::Alive || !owner)
        {
            error = "Snapshot player is unavailable";
            return false;
        }
        ClientWorldFrame frame;
        frame.world = world_id_;
        frame.sequence = next_snapshot_;
        frame.player = found->object;
        frame.player_alive = !owner->m_damage.IsDead();
        if (!capture_client_objects(game_, frame, error, true))
            return false;
        out = Net::snapshot_values(frame);
        ++next_snapshot_;
        return true;
    }
    bool ServerSession::leave(std::uint64_t player)
    {
        const auto found =
            std::find_if(players_.begin(), players_.end(), [&](const auto& p) { return p.id == player; });
        if (found == players_.end() || game_.objects.busy())
            return false;
        const auto client = found->client;
        game_.server_shell.OnClientExitWorld(client);
        if (!game_.objects.finish_frame() || !game_.server.disconnect_client(client))
            return false;
        players_.erase(found);
        return true;
    }
    bool ServerSession::clear_players()
    {
        while (!players_.empty())
            if (!leave(players_.back().id))
                return false;
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
        if (!game_.step_server(ServerClock::step, [&](double seconds) { return advance_players(seconds, error); }))
        {
            if (error.empty())
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
        ++ticks_;
        return true;
    }
} // namespace OpenBlood2
