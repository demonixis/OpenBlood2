#include "server/session.h"
#include <algorithm>
#include <type_traits>

namespace OpenBlood2
{
    DRESULT ServerSession::capture_event(HCLIENT client, const PlayerEvent& value)
    {
        auto found = std::find_if(players_.begin(), players_.end(), [&](const auto& p) { return p.client == client; });
        const auto* owner = game_.objects.get(game_.server.GetClientUserData(client));
        if (found == players_.end() || !owner)
            return DE_INVALIDPARAMS;
        if (std::holds_alternative<PlayerInitRequest>(value) || std::holds_alternative<PlayerPosition>(value) ||
            std::holds_alternative<PlayerPhysics>(value))
        {
            // Admission already runs the original profile initialization. The
            // reliable movement setup observes both force/pose generations and
            // physics; forwarding these again would force/integrate twice.
            ++subsumed_events;
            return DE_OK;
        }
        if (!found->next_event || found->events.size() >= Net::private_queue_limit)
            return DE_ERROR;
        const auto identity = [&](HOBJECT handle)
        {
            const auto* record = game_.objects.get(handle);
            return record ? record->id : ObjectId{};
        };
        Net::PrivateEvent next;
        next.owner = {world_id_, found->id, owner->id};
        next.sequence = found->next_event;
        next.tick = ticks_;
        std::visit(
            [&](const auto& v)
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, SeeingEyeView>)
                    next.value = Net::EyeEvent{identity(v.eye), v.active, v.blind_seconds};
                else if constexpr (std::is_same_v<T, OrbView>)
                    next.value = Net::OrbEvent{identity(v.orb), v.active};
                else if constexpr (std::is_same_v<T, CreatureAttach>)
                    next.value = Net::AttachEvent{identity(v.creature), identity(v.enemy), v.active};
                else if constexpr (std::is_same_v<T, CTFEvent>)
                    next.value = Net::FlagEvent{v.kind, v.team, v.points, identity(v.capturer)};
                else if constexpr (!std::is_same_v<T, PlayerInitRequest> && !std::is_same_v<T, PlayerPosition> &&
                                   !std::is_same_v<T, PlayerPhysics>)
                    next.value = v;
            },
            value);
        if (!Net::valid_private(next))
            return DE_INVALIDPARAMS;
        // Snapshot identities now, including PendingRemoval/Destroying objects.
        // Queued values remain valid after Objects::finish_frame retires handles.
        found->events.push_back(std::move(next));
        ++found->next_event;
        ++private_events;
        return DE_OK;
    }
    bool ServerSession::take_private_events(std::uint64_t player, std::vector<Net::PrivateEvent>& out,
                                            std::string& error)
    {
        error.clear();
        const auto found =
            std::find_if(players_.begin(), players_.end(), [&](const auto& p) { return p.id == player; });
        if (found == players_.end())
        {
            error = "Private event recipient is unavailable";
            return false;
        }
        out.clear();
        out.swap(found->events);
        return true;
    }
} // namespace OpenBlood2
