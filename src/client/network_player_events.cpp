#include "client/network_player_events.h"
#include <algorithm>

namespace OpenBlood2
{
    bool NetworkPlayerEvents::begin(const Net::Joined& owner)
    {
        if (!owner.world || !owner.player || !owner.object || !owner.object.generation || owner.object.slot >= 65536)
            return false;
        ClientHud next;
        if (!next.begin(owner.world) || !next.bind({owner.world, owner.object}))
            return false;
        owner_ = owner;
        hud_ = std::move(next);
        sequence_ = tick_ = 0;
        events_.clear();
        roster_.clear();
        highest_player_ = 0;
        return true;
    }
    const Net::RosterPlayer* NetworkPlayerEvents::player(ObjectId id) const
    {
        const auto found = std::find_if(roster_.begin(), roster_.end(), [&](const auto& p) { return p.object == id; });
        return found == roster_.end() ? nullptr : &*found;
    }
    bool NetworkPlayerEvents::replace_roster(const Net::PlayerRoster& state, std::string& error)
    {
        bool self = false;
        for (const auto& p : state.players)
        {
            const auto old = std::find_if(roster_.begin(), roster_.end(),
                                          [&](const auto& prior) { return prior.player == p.player; });
            if ((old == roster_.end() && p.player <= highest_player_) ||
                (old != roster_.end() &&
                 (p.object != old->object || p.name != old->name || p.character != old->character)) ||
                (p.player == owner_.player && p.object != owner_.object))
            {
                error = "Player roster changed an admission identity or revived a departed player";
                return false;
            }
            self |= p.player == owner_.player;
        }
        if (!self)
        {
            error = "Player roster omitted its recipient";
            return false;
        }
        highest_player_ = std::max(highest_player_, state.players.back().player);
        roster_ = state.players;
        return true;
    }
    NetworkPlayerEvents::Reception NetworkPlayerEvents::receive(const Net::PrivateEvent& event, std::string& error)
    {
        error.clear();
        if (!owner_.world || !Net::valid_private(event))
        {
            error = "Invalid private player event";
            return Reception::Refused;
        }
        if (event.owner != owner_ || event.sequence <= sequence_)
            return Reception::Ignored;
        if (sequence_ == UINT64_MAX || event.sequence != sequence_ + 1 || event.tick < tick_ ||
            events_.size() >= Net::private_queue_limit)
        {
            error = "Private event order, clock or inbox limit violated";
            return Reception::Refused;
        }
        const ClientObjectId owner{owner_.world, owner_.object};
        if (const auto* roster = std::get_if<Net::PlayerRoster>(&event.value))
            if (!replace_roster(*roster, error))
                return Reception::Refused;
        if (const auto* frag = std::get_if<Net::FragEvent>(&event.value))
            for (auto& p : roster_)
                if (p.object == frag->killer)
                    p.frags = frag->killer_frags; // Authoritative absolute score, including suicide.
        if (const auto* stats = std::get_if<PlayerStats>(&event.value))
        {
            if (!hud_.receive(owner, event.sequence, *stats))
            {
                error = "Private HUD statistics refused";
                return Reception::Refused;
            }
        }
        if (const auto* items = std::get_if<InventoryItems>(&event.value))
        {
            if (!hud_.receive(owner, event.sequence, *items))
            {
                error = "Private HUD inventory refused";
                return Reception::Refused;
            }
        }
        events_.push_back(event);
        sequence_ = event.sequence;
        tick_ = event.tick;
        return Reception::Applied;
    }
} // namespace OpenBlood2
