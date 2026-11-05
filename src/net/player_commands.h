#pragma once
#include "net/movement.h"
namespace OpenBlood2::Net
{
    inline constexpr std::size_t command_packet_limit = 64, command_queue_limit = 32;
    enum class CommandKind : std::uint8_t
    {
        WeaponSlot = 1,
        NextWeapon,
        PreviousWeapon,
        Use,
        Item,
        NextItem,
        PreviousItem,
        UseItem,
        Alternate,
        Detonate
    };
    struct CommandAction
    {
        CommandKind kind = CommandKind::Use;
        std::uint8_t argument = 0;
        PlayerRotation aim{}; // USE only; other commands have canonical zero aim.
    };
    struct PlayerCommand
    {
        MovementIdentity owner;
        std::uint64_t sequence = 0;
        CommandAction action;
    };
    struct CommandAck
    {
        MovementIdentity owner; // Identity of the request, including an expired force.
        std::uint64_t sequence = 0;
        bool applied = false;
    };
    using CommandMessage = std::variant<PlayerCommand, CommandAck>;
    enum class DecodeCommand
    {
        Ok,
        Malformed,
        Protocol
    };
    bool valid_command(const CommandMessage&);
    bool is_command_packet(std::span<const std::uint8_t>);
    bool encode_command(const CommandMessage&, std::vector<std::uint8_t>&);
    DecodeCommand decode_command(std::span<const std::uint8_t>, CommandMessage&);
} // namespace OpenBlood2::Net
