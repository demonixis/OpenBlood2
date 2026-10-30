#pragma once
#include "game/server_adapter.h"
#include "net/admission.h"
#include "net/content.h"
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
        // Trusted local call after the endpoint's protocol/content/loading gates.
        bool join(const Net::JoinProfile&, Net::Joined&, std::string& error);
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
        };
        std::vector<Player> players_;
        std::uint64_t world_id_ = 0, next_player_ = 1;
        unsigned mode_ = 0;
    };
} // namespace OpenBlood2
