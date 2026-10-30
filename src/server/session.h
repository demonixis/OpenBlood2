#pragma once
#include "game/server_adapter.h"
#include "net/admission.h"
#include "net/content.h"
#include "net/movement.h"
#include "net/player_events.h"
#include "net/world_snapshot.h"
#include "server/clock.h"
#include <utility>
#include <string>
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
        std::uint64_t movement_reports = 0, movement_ignored = 0, movement_coalesced = 0;
        std::uint64_t movement_steps = 0, movement_holds = 0;
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
            std::optional<Net::MovementReport> pending;
            std::optional<Net::MovementAck> ack;
            PlayerControls controls;
            PlayerRotation aim;
            std::uint64_t source_revision = 0, pose_revision = 0;
            std::uint64_t received_sequence = 0, received_tick = 0;
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
    };
} // namespace OpenBlood2
