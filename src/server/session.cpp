#include "server/session.h"
#include "game/client_replication.h"
#include "game/game_settings.h"
#include "game/original/AI_Mgr.h"
#include "game/original/FlagObjects.h"
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
        game_.server.remote_audio_sink = {};
        game_.server.remote_effect_sink = {};
        game_.server.remote_stream_sink = {};
        game_.server.remote_object_particle_sink = {};
    }
    bool ServerSession::start(const Config& config, std::string_view name, unsigned mode, std::string& error,
                              const MultiplayerRules& rules)
    {
        error.clear();
        if (started_ || !network_game_type(mode) || !rules.valid() || config.demo || config.nightmares ||
            !config.overlays.empty())
        {
            error = "Dedicated startup requires a fresh retail multiplayer session with valid rules and no mods";
            return false;
        }
        rotation_.clear();
        const auto append = [&](std::string_view map)
        {
            std::string path;
            if (!world_resource_path(map, false, path) ||
                !path.starts_with(mode == GAMETYPE_CTF ? "worlds/multi/ctf_" : "worlds/multi/bb_"))
                return false;
            rotation_.push_back(std::move(path));
            return true;
        };
        if (!append(name))
        {
            error = "Select a retail worlds/multi/bb_ map (Bloodbath/Team) or ctf_ map (CTF)";
            return false;
        }
        for (const auto& map : rules.following_maps)
            if (!append(map))
            {
                error = "Invalid multiplayer rotation map: " + map;
                return false;
            }
        if (!game_.initialize())
        {
            error = "Cannot bind original server objects";
            return false;
        }
        game_.server.client_audio = nullptr;
        if (!game_.resources.mount_game(config, error) ||
            !Net::content_fingerprint(game_.resources, fingerprint_, error))
            return false;
        for (const auto& path : rotation_)
            if (!game_.resources.find(path))
            {
                error = "Missing multiplayer rotation map: " + path;
                return false;
            }
        mode_ = mode;
        rules_ = rules;
        game_.server.friendly_fire = rules.friendly_fire;
        game_.server.negative_team_frags = rules.negative_team_frags;
        if (!load_map(0, error))
            return false;
        started_ = true;
        return true;
    }
    bool ServerSession::load_map(std::size_t index, std::string& error)
    {
        WorldRequest request;
        request.game_type = DBYTE(mode_);
        request.filename = rotation_[index];
        stream_emitters_.clear();
        particle_emitters_.clear();
        game_.server.remote_object_particle_sink = [this](ObjectId owner, std::string_view name, unsigned type,
                                                          float scale, DVector offset, std::string& failure)
        { return capture_particle(owner, name, type, scale, offset, failure); };
        game_.server.remote_stream_sink =
            [this](ObjectId owner, const ParticleStreamSettings& settings, std::string& failure)
        { return capture_stream(owner, settings, failure); };
        // Installed before the load like the owner-bound hooks: an instant effect
        // published while the world loads has no recipient yet and is discarded,
        // instead of filling the dedicated server's never-advanced client pools.
        game_.server.remote_effect_sink =
            [this](ObjectId source, std::string_view name, const InstantEffectRecipe& settings, std::string& error)
        { return capture_effect(source, name, settings, error); };
        if (!game_.load_request(request, error))
            return false;
        g_ctf.only_flag_scores = rules_.only_flag_scores;
        g_ctf.use_team_size = rules_.use_team_size;
        g_ctf.flag_value = int(rules_.flag_value); // World load resets original globals; reapply on every map.
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
        if (!next)
        {
            error = "Server world identity exhausted";
            return false;
        }
        world_id_ = next;
        map_index_ = index;
        ticks_ = 0;
        next_snapshot_ = 1;
        ending_tick_.reset();
        audio_sources_.clear();
        game_.server.remote_event_sink = [this](HCLIENT client, const PlayerEvent& event)
        { return capture_event(client, event); };
        game_.server.remote_audio_sink = [this](const AudioVoice& voice, bool start) { capture_audio(voice, start); };
        for (const auto& [id, voice] : game_.server.audio.voices())
            capture_audio(voice, true);
        return true;
    }
    bool ServerSession::rotate(std::string& error)
    {
        error.clear();
        if (!started_ || !rotation_due() || game_.objects.busy())
        {
            error = "Map rotation requires an expired match at a simulation boundary";
            return false;
        }
        // Leave through original cleanup while the old world exists. No player,
        // sound handle or queued private event survives the epoch replacement.
        if (!clear_players())
        {
            error = "Cannot clear old multiplayer players";
            return false;
        }
        game_.server.remote_event_sink = {};
        game_.server.remote_audio_sink = {};
        game_.server.remote_effect_sink = {};
        game_.server.remote_stream_sink = {};
        game_.server.remote_object_particle_sink = {};
        started_ = false; // A failed load cannot resume a partially destroyed world.
        if (!load_map((map_index_ + 1) % rotation_.size(), error))
            return false;
        started_ = true;
        ++rotations;
        return true;
    }
    void ServerSession::update_match()
    {
        if (ending_tick_)
            return;
        std::array<int, Net::max_players> scores{};
        std::size_t count = 0;
        if (team_game_type(mode_))
        {
            for (const auto score : game_.server.team_frags)
                scores[count++] = score; // Includes empty teams and departed members' frags.
        }
        else
            for (const auto& p : players_)
                if (const auto* player = game_.server_shell.Player(p.client))
                    scores[count++] = player->m_Frags;
        if (!match_limit_reached(rules_, ticks_, std::span(scores).first(count)))
            return;
        ending_tick_ = ticks_;
        for (auto& p : players_)
        {
            for (auto& view : p.weapon_timeline)
                view.CancelFiringState();
            // Original CancelFiringState sends SOUNDSTOP for the retained loop.
            if (auto* player = game_.server_shell.Player(p.client))
                player->StopWeaponSound();
            if (queue_private(p, p.object, Net::LevelChanging{}) != DE_OK)
                p.roster_failed = true; // Existing bounded-recipient disconnect path.
        }
    }
    DBYTE ServerSession::player_team(std::uint64_t player)
    {
        const auto p =
            std::find_if(players_.begin(), players_.end(), [&](const auto& row) { return row.id == player; });
        return p == players_.end() ? 0 : game_.server.GetClientTeam(p->client);
    }
    bool ServerSession::join(const Net::JoinProfile& profile, Net::Joined& joined, std::string& error)
    {
        error.clear();
        if (!started_ || !Net::valid_profile(profile) || !next_player_ || players_.size() >= Net::max_players ||
            game_.objects.busy())
        {
            error = "Invalid or unavailable multiplayer player admission";
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
        auto* player = game_.server_shell.OnClientEnterWorld(client, &error, profile.team);
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
        player->m_damage.replicate_velocity = true;
        publish_roster();
        if (players_.back().roster_failed)
        {
            error = "Cannot publish admitted player roster";
            leave(connection);
            return false;
        }
        if (changing_level() && queue_private(players_.back(), id, Net::LevelChanging{}) != DE_OK)
        {
            error = "Cannot publish current match transition";
            leave(connection);
            return false;
        }
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
        prune_streams();
        for (const auto& stream : stream_emitters_)
        {
            const auto target = std::lower_bound(frame.objects.begin(), frame.objects.end(), stream.owner.slot,
                                                 [](const auto& object, auto slot) { return object.id.slot < slot; });
            if (target != frame.objects.end() && target->id == stream.owner)
                target->particle_stream = stream.settings;
        }
        prune_particles();
        for (const auto& particle : particle_emitters_)
        {
            const auto target = std::lower_bound(frame.objects.begin(), frame.objects.end(), particle.owner.slot,
                                                 [](const auto& object, auto slot) { return object.id.slot < slot; });
            if (target != frame.objects.end() && target->id == particle.owner)
                target->object_particles.push_back(particle.recipe);
        }
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
        publish_roster(player); // Original removal precedes player/flag destruction.
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
        update_match(); // Original server-shell decision precedes this tick's object updates.
        if (!game_.step_server(ServerClock::step, [&](double seconds)
                               { return advance_players(seconds, error) && apply_controls(error); }))
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
                    warnings_.push_back("Server NPC " + std::string(object->state.m_Name) +
                                        " suspended: " + ai->runtime_error);
                }
        ++ticks_;
        ++total_ticks_;
        return true;
    }
} // namespace OpenBlood2
