#include "client/network_player_events.h"

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
