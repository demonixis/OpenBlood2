#pragma once
#include "game/server_adapter.h"
#include "net/admission.h"
#include "net/content.h"
#include "net/movement.h"
#include "net/player_events.h"
#include "net/world_snapshot.h"
#include "server/clock.h"
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace OpenBlood2
{
    // Dedicated simulation owner. The current shared GameSession storage is
    // transitional until the client replica owns all presentation state.
    class ServerSession
    {
      public:
        ~ServerSession();
        bool start(const Config&, std::string_view world, unsigned mode, std::string& error);
        bool tick(std::string& error);
        std::uint64_t ticks() const
        {
            return ticks_;
        }
        // Trusted local call after the endpoint's protocol/content/loading gates.
        bool join(const Net::JoinProfile&, Net::Joined&, std::string& error);
        bool snapshot(std::uint64_t player, Net::WorldSnapshot&, std::string& error);
        enum class MoveResult
        {
            Queued,
            Ignored,
            Invalid
        };
        MoveResult queue_movement(std::uint64_t player, const Net::MovementReport&);
        bool movement_setup(std::uint64_t player, Net::MovementSetup&, std::string& error);
        bool movement_ack(std::uint64_t player, std::optional<Net::MovementAck>&) const;
        // Reports are replayed in order, one resolved CMoveMgr substep at a time.
        // dropped: overflow of the bounded queue; catchups: one sweep to a
        // report's origin after a lost report; leashed: steps or catch-ups
        // refused by the speed limit or movement-time budget; failures: players
        // removed after a movement error (the match keeps running).
        std::uint64_t movement_reports = 0, movement_ignored = 0, movement_dropped = 0;
        std::uint64_t movement_steps = 0, movement_holds = 0, movement_catchups = 0, movement_leashed = 0;
        std::uint64_t movement_failures = 0;
        static constexpr std::size_t movement_queue_limit = 8;
        // Real time a player may still move: refilled 5 % faster than the
        // server clock (client/server clock drift), capped at half a second.
        static constexpr double movement_budget_rate = 1.05, movement_budget_limit = .5;
        bool weapon_loadout(std::uint64_t player, Net::WeaponLoadout&, std::string& error);
        bool take_private_events(std::uint64_t player, std::vector<Net::PrivateEvent>&, std::string& error);
        std::uint64_t private_events = 0, subsumed_events = 0;
        bool leave(std::uint64_t player);
        bool clear_players();
        std::size_t player_count() const
        {
            return players_.size();
        }
        std::uint64_t world_id() const
        {
            return world_id_;
        }
        unsigned mode() const
        {
            return mode_;
        }
        const GameSession& game() const
        {
            return game_;
        }
        // NPC runtime failures suspend that actor, as in solo; they are reported
        // once per object and never stop the match for every player.
        std::vector<std::string> take_warnings()
        {
            return std::exchange(warnings_, {});
        }
        const Sha256Digest& fingerprint() const
        {
            return fingerprint_;
        }

      private:
        GameSession game_;
        Sha256Digest fingerprint_{};
        bool started_ = false;
        std::vector<std::string> warnings_;
        std::vector<ObjectId> reported_npcs_;
        struct Player
        {
            std::uint64_t id;
            HCLIENT client;
            ObjectId object;
            Net::MovementSetup setup;
            std::vector<Net::PrivateEvent> events;
            std::uint64_t next_event = 1;
            ObjectId camera_eye, camera_orb;
            std::optional<Net::CameraState> camera;
            std::optional<Net::WeaponLoadout> weapons;
            std::deque<Net::MovementReport> pending;
            std::optional<Net::MovementAck> ack;
            PlayerControls controls;
            PlayerRotation aim;
            std::uint64_t source_revision = 0, pose_revision = 0;
            std::uint64_t received_sequence = 0, received_tick = 0, applied_sequence = 0;
            // Starts full: network jitter and the first reports may arrive in a burst.
            double movement_budget = movement_budget_limit;
            // Set once a movement error isolates this player; the endpoint then
            // removes the peer instead of stopping the server.
            std::string failure;
        };
        std::vector<Player> players_;
        std::uint64_t world_id_ = 0, next_player_ = 1, next_snapshot_ = 1;
        std::uint64_t ticks_ = 0;
        unsigned mode_ = 0;
        DRESULT capture_event(HCLIENT, const PlayerEvent&);
        DRESULT queue_private(Player&, ObjectId, const Net::PrivateValue&);
        bool refresh_camera(Player&, Net::CameraState&, std::string& error);
        bool refresh_movement(Player&, std::string& error);
        bool advance_players(double seconds, std::string& error);
        bool advance_player(Player&, double seconds, std::string& error);
        bool replay_movement(Player&, const Net::MovementReport&, std::string& error);
    };
} // namespace OpenBlood2
