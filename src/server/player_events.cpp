#include "server/session.h"
#include <algorithm>
#include <type_traits>

namespace OpenBlood2
{
    void ServerSession::publish_roster(std::uint64_t excluded)
    {
        Net::PlayerRoster roster;
        for (const auto& p : players_)
        {
            if (p.id == excluded)
                continue;
            const auto* original = game_.server_shell.Player(p.client);
            const auto* name = original ? game_.server.GetStringData(original->GetPlayerName()) : nullptr;
            roster.players.push_back({p.id, p.object, name ? name : "", original ? original->m_nCharacter : DBYTE(0),
                                      original ? original->m_Frags : 0});
        }
        for (auto& p : players_)
            if (p.id != excluded && !p.roster_failed && queue_private(p, p.object, roster) != DE_OK)
                // A slow recipient must disconnect rather than retain a silently
                // incomplete roster. Its departure must still release gameplay.
                p.roster_failed = true;
    }
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
        const auto identity = [&](HOBJECT handle)
        {
            const auto* record = game_.objects.get(handle);
            return record ? record->id : ObjectId{};
        };
        Net::PrivateValue next;
        std::visit(
            [&](const auto& v)
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, SeeingEyeView>)
                    next = Net::EyeEvent{identity(v.eye), v.active, v.blind_seconds};
                else if constexpr (std::is_same_v<T, OrbView>)
                    next = Net::OrbEvent{identity(v.orb), v.active};
                else if constexpr (std::is_same_v<T, CreatureAttach>)
                    next = Net::AttachEvent{identity(v.creature), identity(v.enemy), v.active};
                else if constexpr (std::is_same_v<T, PlayerFragged>)
                    next = Net::FragEvent{identity(v.victim), identity(v.killer), v.killer_frags, v.index};
                else if constexpr (std::is_same_v<T, CTFEvent>)
                    next = Net::FlagEvent{v.kind, v.team, v.points, identity(v.capturer)};
                else if constexpr (!std::is_same_v<T, PlayerInitRequest> && !std::is_same_v<T, PlayerPosition> &&
                                   !std::is_same_v<T, PlayerPhysics>)
                    next = v;
            },
            value);
        const auto result = queue_private(*found, owner->id, next);
        if (result == DE_OK)
        {
            if (const auto* eye = std::get_if<Net::EyeEvent>(&next))
                found->camera_eye = eye->active ? eye->eye : ObjectId{};
            if (const auto* orb = std::get_if<Net::OrbEvent>(&next))
            {
                if (orb->active)
                    found->camera_orb = orb->orb;
                else if (found->camera_orb == orb->orb)
                    found->camera_orb = {};
            }
        }
        if (result == DE_OK && std::holds_alternative<PlayerDead>(value) && found->setup.owner.force_revision)
        {
            // A following USE may respawn before the normal publication pass.
            // Keep this actual dead loadout in the reliable timeline so both
            // CViewWeapons cancel the attack even without an intervening tick.
            Net::WeaponLoadout dead;
            std::string error;
            if (!weapon_loadout(found->id, dead, error))
                return DE_ERROR;
        }
        return result;
    }
    DRESULT ServerSession::queue_private(Player& player, ObjectId owner, const Net::PrivateValue& value)
    {
        if (!player.next_event || player.events.size() >= Net::private_queue_limit)
            return DE_ERROR;
        Net::PrivateEvent next{{world_id_, player.id, owner}, player.next_event, ticks_, value};
        if (!Net::valid_private(next))
            return DE_INVALIDPARAMS;
        // Snapshot identities now, including PendingRemoval/Destroying objects.
        // Queued values remain valid after Objects::finish_frame retires handles.
        player.events.push_back(std::move(next));
        ++player.next_event;
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
        if (found->roster_failed)
        {
            error = "Reliable player roster publication failed";
            return false;
        }
        out.clear();
        out.swap(found->events);
        return true;
    }
} // namespace OpenBlood2
