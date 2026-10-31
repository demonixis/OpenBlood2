#include "server/session.h"
#include <algorithm>
namespace OpenBlood2
{
    bool ServerSession::begin_ballistics(std::uint64_t id)
    {
        const auto p = std::find_if(players_.begin(), players_.end(), [id](const auto& p) { return p.id == id; });
        if (p == players_.end() || p->ballistic_started)
            return false;
        p->ballistic_started = true;
        return true;
    }
    std::uint64_t ServerSession::capture_ballistic(ObjectId source, std::string_view name,
                                                   const ShotgunEffectSettings& settings, std::string& error)
    {
        error.clear();
        Net::BallisticEvent event{world_id_, 1, 1, source, std::string(name), settings};
        if (ballistic_events == UINT64_MAX || !Net::valid_ballistic(event))
        {
            error = "Invalid or exhausted authoritative ballistic publication";
            return 0;
        }
        for (auto& player : players_)
        {
            // Baseline-ready admission gate. These are one-shot impact
            // messages: never replay old shots/casings to a late arrival.
            if (!player.ballistic_started || player.ballistic_failed)
                continue;
            if (!player.next_ballistic || player.ballistics.size() >= Net::ballistic_queue_limit)
            {
                player.ballistic_failed = true;
                continue; // One slow recipient cannot alter the shot or other peers.
            }
            event.player = player.id;
            event.sequence = player.next_ballistic++;
            player.ballistics.push_back(event);
        }
        return ++ballistic_events;
    }
    bool ServerSession::take_ballistic_events(std::uint64_t id, std::vector<Net::BallisticEvent>& out,
                                              std::string& error)
    {
        error.clear();
        const auto p = std::find_if(players_.begin(), players_.end(), [id](const auto& p) { return p.id == id; });
        if (p == players_.end() || !p->ballistic_started || p->ballistic_failed)
        {
            error = "Ballistic recipient queue unavailable or over capacity";
            return false;
        }
        out.clear();
        out.swap(p->ballistics);
        return true;
    }
} // namespace OpenBlood2
