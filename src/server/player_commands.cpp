#include "server/session.h"
#include <algorithm>
namespace OpenBlood2
{
    ServerSession::CommandResult ServerSession::queue_command(std::uint64_t id, const Net::PlayerCommand& value)
    {
        const auto found = std::find_if(players_.begin(), players_.end(), [id](const auto& p) { return p.id == id; });
        if (found == players_.end() || !Net::valid_command(value) || value.owner.player != id)
            return CommandResult::Invalid;
        if (value.owner.world != world_id_ || value.owner.object != found->object ||
            value.sequence <= found->command_sequence)
        {
            ++commands_ignored;
            return CommandResult::Ignored;
        }
        // Reliable commands are ordered, never coalesced. A force may expire
        // between receipt and execution; it still gets one negative acknowledgement.
        if (value.sequence != found->command_sequence + 1 ||
            found->queued_commands + found->command_acks.size() >= Net::command_queue_limit)
            return CommandResult::Invalid;
        found->pending_controls.push_back(value);
        ++found->queued_commands;
        found->command_sequence = value.sequence;
        return CommandResult::Queued;
    }
    bool ServerSession::take_command_acks(std::uint64_t id, std::vector<Net::CommandAck>& out)
    {
        const auto found = std::find_if(players_.begin(), players_.end(), [id](const auto& p) { return p.id == id; });
        if (found == players_.end())
            return false;
        out.clear();
        out.swap(found->command_acks);
        return true;
    }
    bool ServerSession::apply_command(Player& p, const Net::PlayerCommand& command, std::string& error)
    {
        if (!refresh_movement(p, error))
            return false;
        auto* player = game_.server_shell.Player(p.client);
        if (!player)
        {
            error = "Network command owner is unavailable";
            return false;
        }
        bool applied = false;
        if (!changing_level() && command.owner == p.setup.owner &&
            ((!player->m_bDead && !player->m_damage.IsDead()) || command.action.kind == Net::CommandKind::Use))
        {
            auto& inventory = player->m_InventoryMgr;
            const auto& action = command.action;
            using Kind = Net::CommandKind;
            switch (action.kind)
            {
                case Kind::WeaponSlot:
                case Kind::NextWeapon:
                case Kind::PreviousWeapon:
                    if (!player->MovementBlocked() && !player->camera_movement_blocked)
                    {
                        const auto slot = action.kind == Kind::WeaponSlot
                                              ? std::optional<DBYTE>(action.argument)
                                              : inventory.CycleWeaponSlot(action.kind == Kind::NextWeapon);
                        // Original ChangeWeaponSlot cancels zoom even on
                        // an empty/current slot; do not prefilter that call.
                        const auto result = slot ? inventory.ChangeWeaponSlot(*slot) : InventoryResult::Current;
                        applied = result == InventoryResult::Ok || result == InventoryResult::Current;
                    }
                    break;
                case Kind::Use:
                    if (!player->MovementBlocked() && !player->camera_movement_blocked &&
                        !(inventory.eye && inventory.eye->active))
                        applied = game_.server_shell.OnMessage(p.client, PlayerUse{action.aim});
                    if (player->respawn_failed)
                    {
                        error = "Network respawn failed: " + player->entry_error;
                        return false;
                    }
                    break;
                case Kind::Item:
                    applied = game_.server_shell.OnMessage(p.client, PlayerItemCommand{action.argument});
                    break;
                case Kind::NextItem:
                case Kind::PreviousItem:
                    applied = game_.server_shell.OnMessage(p.client, PlayerItemCursor{action.kind == Kind::NextItem});
                    break;
                case Kind::UseItem:
                    applied = game_.server_shell.OnMessage(p.client, PlayerUseInventory{});
                    break;
                case Kind::Alternate:
                    applied = game_.server_shell.OnMessage(p.client, PlayerAltFire{});
                    break;
                case Kind::Detonate:
                    applied = game_.server_shell.OnMessage(p.client, PlayerDetonate{});
                    break;
            }
            if (!inventory.UpdateHandModels())
            {
                error = "Network command held-weapon update failed";
                return false;
            }
        }
        p.command_acks.push_back({command.owner, command.sequence, applied});
        commands_applied += applied;
        commands_blocked += !applied;
        return true;
    }
    bool ServerSession::apply_controls(std::string& error)
    {
        for (auto& p : players_)
        {
            if (!p.failure.empty())
                continue;
            // One weapon frame per server tick on average: at most half a second
            // of delayed frames is banked, and a backlog drains at twice real
            // time, so withheld frames never fire faster than 2x. It never
            // advances movement.
            p.weapon_credit = std::min(p.weapon_credit + 1, weapon_credit_limit);
            unsigned frames = 0;
            std::string failure;
            bool failed = false;
            while (!p.pending_controls.empty())
            {
                const bool weapon = std::holds_alternative<Net::WeaponFrame>(p.pending_controls.front());
                if (weapon && (!p.weapon_credit || frames == weapon_frames_per_tick))
                    break; // Later Control commands may not overtake this frame.
                auto value = std::move(p.pending_controls.front());
                p.pending_controls.pop_front();
                if (weapon)
                {
                    --p.queued_weapon_frames;
                    --p.weapon_credit;
                    ++frames;
                    if (!apply_weapon_frame(p, std::get<Net::WeaponFrame>(value), failure))
                    {
                        failed = true;
                        break;
                    }
                }
                else
                {
                    --p.queued_commands;
                    if (!apply_command(p, std::get<Net::PlayerCommand>(value), failure))
                    {
                        failed = true;
                        break;
                    }
                }
            }
            if (!failed)
                continue;
            // Like a movement error: only this player is removed (endpoint), the
            // match keeps running.
            p.failure = failure.empty() ? "Remote player control failed" : failure;
            p.pending_controls.clear();
            p.queued_commands = p.queued_weapon_frames = 0;
            ++control_failures;
            warnings_.push_back("Remote player " + std::to_string(p.id) + " removed: " + p.failure);
        }
        (void)error;
        return true;
    }
} // namespace OpenBlood2
