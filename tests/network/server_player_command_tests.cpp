#include "game/game_settings.h"
#include "server/session.h"
#include <iostream>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " " << error << '\n';                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool run(const char* data)
    {
        std::string error;
        Config config;
        config.data_dir = path_from_utf8(data);
        ServerSession session;
        CHECK(session.start(config, "multi/bb_crypt", GAMETYPE_BLOODBATH, error));
        Joined a, b;
        JoinProfile profile;
        profile.weapons = {WEAP_BERETTA};
        CHECK(session.join(profile, a, error) && session.join(profile, b, error));
        CHECK(session.tick(error));
        MovementSetup setup;
        CHECK(session.movement_setup(a.player, setup, error));
        auto& game = const_cast<GameSession&>(session.game());
        auto* first = dynamic_cast<CPlayerObj*>(game.objects.get(a.object)->game.get());
        auto* second = dynamic_cast<CPlayerObj*>(game.objects.get(b.object)->game.get());
        CHECK(first && second);
        std::vector<CommandAck> acks;
        using Reception = ServerSession::CommandResult;
        PlayerCommand command{setup.owner, 1, {CommandKind::WeaponSlot, 0}};
        CHECK(session.queue_command(a.player, command) == Reception::Queued);
        CHECK(first->m_InventoryMgr.m_nCurWeaponSlot == 1); // No mutation before tick.
        CHECK(session.queue_command(a.player, command) == Reception::Ignored);
        CHECK(session.tick(error) && first->m_InventoryMgr.m_nCurWeaponSlot == 0 &&
              second->m_InventoryMgr.m_nCurWeaponSlot == 1);
        CHECK(session.take_command_acks(a.player, acks) && acks.size() == 1 && acks[0].applied);
        CHECK(session.take_command_acks(b.player, acks) && acks.empty());
        command.sequence = 2;
        command.action = {CommandKind::NextWeapon};
        CHECK(session.queue_command(a.player, command) == Reception::Queued && session.tick(error));
        CHECK(first->m_InventoryMgr.m_nCurWeaponSlot == 1);
        CHECK(session.take_command_acks(a.player, acks) && acks.size() == 1 && acks[0].applied);
        first->SetZoomMode(true, 99);
        command.sequence = 3;
        command.action = {CommandKind::WeaponSlot, 9};
        CHECK(session.queue_command(a.player, command) == Reception::Queued && session.tick(error));
        CHECK(!first->m_bZoomView && first->m_InventoryMgr.m_nCurWeaponSlot == 1);
        CHECK(session.take_command_acks(a.player, acks) && acks.size() == 1 && !acks[0].applied);
        command.sequence = 4;
        command.action = {CommandKind::Use, 0, {0, 1}};
        const auto uses = first->use_commands;
        CHECK(session.queue_command(a.player, command) == Reception::Queued && session.tick(error));
        CHECK(first->use_commands == uses + 1 && second->use_commands == 0);
        CHECK(session.take_command_acks(a.player, acks) && acks.size() == 1 && acks[0].applied);
        CHECK(session.queue_command(a.player, command) == Reception::Ignored && session.tick(error));
        CHECK(first->use_commands == uses + 1);
        ++command.sequence;
        CHECK(session.queue_command(a.player, command) == Reception::Queued);
        CHECK(first->TeleportClientToServerPos() && session.tick(error));
        CHECK(first->use_commands == uses + 1);
        CHECK(session.take_command_acks(a.player, acks) && acks.size() == 1 && !acks[0].applied &&
              acks[0].owner == command.owner);
        CHECK(session.movement_setup(a.player, setup, error));
        command.owner = setup.owner;
        ++command.sequence;
        CHECK(session.queue_command(a.player, command) == Reception::Queued);
        first->movement_blocked = true;
        CHECK(session.tick(error) && session.take_command_acks(a.player, acks) && acks.size() == 1 && !acks[0].applied);
        CHECK(first->use_commands == uses + 1);
        first->movement_blocked = false;
        CHECK(first->m_InventoryMgr.AddItem(5, 1) == InventoryResult::Ok);
        command.action = {CommandKind::Item, 5};
        ++command.sequence;
        first->command_run = true;
        CHECK(session.queue_command(a.player, command) == Reception::Queued && session.tick(error));
        CHECK(session.take_command_acks(a.player, acks) && acks.size() == 1 && !acks[0].applied);
        CHECK(!first->m_InventoryMgr.binoculars->active);
        first->command_run = false;
        ++command.sequence;
        CHECK(session.queue_command(a.player, command) == Reception::Queued && session.tick(error));
        CHECK(session.take_command_acks(a.player, acks) && acks.size() == 1 && acks[0].applied);
        CHECK(first->m_InventoryMgr.binoculars->active && first->m_bZoomView && first->m_nZoomType == 98);
        CHECK(!second->m_bZoomView);
        ++command.sequence;
        CHECK(session.queue_command(a.player, command) == Reception::Queued && session.tick(error));
        CHECK(session.take_command_acks(a.player, acks) && acks.size() == 1 && acks[0].applied);
        CHECK(!first->m_InventoryMgr.binoculars->active && !first->m_bZoomView);
        command.sequence += 2;
        CHECK(session.queue_command(a.player, command) == Reception::Invalid); // Reliable stream gap.
        --command.sequence;
        for (unsigned i = 0; i < command_queue_limit; ++i)
        {
            command.action = {CommandKind::NextItem};
            CHECK(session.queue_command(a.player, command) == Reception::Queued);
            ++command.sequence;
        }
        CHECK(session.queue_command(a.player, command) == Reception::Invalid);
        CHECK(session.tick(error));
        CHECK(session.queue_command(a.player, command) == Reception::Invalid); // ACKs count towards bound.
        CHECK(session.take_command_acks(a.player, acks) && acks.size() == command_queue_limit);
        CHECK(session.queue_command(a.player, command) == Reception::Queued && session.tick(error));
        CHECK(session.take_command_acks(a.player, acks) && acks.size() == 1);
        auto wrong = command;
        ++wrong.sequence;
        wrong.owner.world++;
        CHECK(session.queue_command(a.player, wrong) == Reception::Ignored);
        wrong = command;
        wrong.owner.player = b.player;
        CHECK(session.queue_command(a.player, wrong) == Reception::Invalid);
        CHECK(session.leave(a.player) && session.queue_command(a.player, command) == Reception::Invalid);
        CHECK(session.take_command_acks(b.player, acks) && acks.empty());
        std::cout << "Original server commands: tick ordering, independent players, weapon cycle, empty-slot zoom, USE "
                     "dedup, force/block gates and bounded request/ACK queues passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    return argc == 2 && run(argv[1]) ? 0 : 1;
}
