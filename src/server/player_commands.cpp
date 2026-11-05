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
            return false;
        bool applied = false;
        if (command.owner == p.setup.owner && !player->m_bDead && !player->m_damage.IsDead())
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
            // At most one second of delayed input is banked. Loss recovery may
            // process six weapon frames per tick; it never advances movement.
            p.weapon_credit = std::min(p.weapon_credit + 1, 60u);
            unsigned frames = 0;
            while (!p.pending_controls.empty())
            {
                const bool weapon = std::holds_alternative<Net::WeaponFrame>(p.pending_controls.front());
                if (weapon && (!p.weapon_credit || frames == 6))
                    break; // Later Control commands may not overtake this frame.
                auto value = std::move(p.pending_controls.front());
                p.pending_controls.pop_front();
                if (weapon)
                {
                    --p.queued_weapon_frames;
                    --p.weapon_credit;
                    ++frames;
                    if (!apply_weapon_frame(p, std::get<Net::WeaponFrame>(value), error))
                        return false;
                }
                else
                {
                    --p.queued_commands;
                    if (!apply_command(p, std::get<Net::PlayerCommand>(value), error))
                        return false;
                }
            }
        }
        return true;
    }
} // namespace OpenBlood2
