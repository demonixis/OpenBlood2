#pragma once
#include "game/server_adapter.h"
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
        bool start(const Config&, std::string_view world, unsigned mode, std::string& error);
        bool tick(std::string& error);
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
    };
} // namespace OpenBlood2
