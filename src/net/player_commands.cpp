#include "net/player_commands.h"
#include "net/admission.h"
#include "net/codec.h"
#include <type_traits>
namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t magic = 0x43503242; // B2PC.
        bool action(const CommandAction& v)
        {
            // Authored start/script rotations need not be normalized to [-pi, pi].
            // Match the existing typed PlayerUse/PlayerRotation finite bound.
            if (!std::isfinite(v.aim.pitch) || !std::isfinite(v.aim.yaw) || std::abs(v.aim.pitch) > 1e6f ||
                std::abs(v.aim.yaw) > 1e6f)
                return false;
            if (v.kind != CommandKind::Use && (v.aim.pitch != 0 || v.aim.yaw != 0))
                return false;
            if (v.kind == CommandKind::WeaponSlot)
                return v.argument < 10;
            if (v.kind == CommandKind::Item)
                return v.argument >= 1 && v.argument <= 9 && v.argument != 6;
            return v.kind >= CommandKind::NextWeapon && v.kind <= CommandKind::Detonate && !v.argument;
        }
    } // namespace
    bool valid_command(const CommandMessage& message)
    {
        return std::visit(
            [](const auto& v)
            {
                if (!v.owner.world || !v.owner.player || !v.owner.object || !v.owner.object.generation ||
                    v.owner.object.slot >= 65536 || !v.owner.force_revision || !v.sequence)
                    return false;
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>, PlayerCommand>)
                    return action(v.action);
                return true;
            },
            message);
    }
    bool is_command_packet(std::span<const std::uint8_t> bytes)
    {
        Reader r(bytes);
        return bytes.size() >= 4 && r.u32() == magic;
    }
    bool encode_command(const CommandMessage& message, std::vector<std::uint8_t>& out)
    {
        if (!valid_command(message))
            return false;
        Writer w(command_packet_limit);
        w.u32(magic);
        w.u16(protocol_version);
        w.u8(std::uint8_t(message.index() + 1));
        std::visit(
            [&](const auto& v)
            {
                w.u64(v.owner.world);
                w.u64(v.owner.player);
                w.u32(v.owner.object.slot);
                w.u32(v.owner.object.generation);
                w.u64(v.owner.force_revision);
                w.u64(v.sequence);
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>, PlayerCommand>)
                {
                    w.u8(std::uint8_t(v.action.kind));
                    w.u8(v.action.argument);
                    w.f32(v.action.aim.pitch);
                    w.f32(v.action.aim.yaw);
                }
                else
                    w.u8(v.applied);
            },
            message);
        if (!w.good())
            return false;
        out = w.bytes();
        return true;
    }
    DecodeCommand decode_command(std::span<const std::uint8_t> bytes, CommandMessage& out)
    {
        if (bytes.size() < 7 || bytes.size() > command_packet_limit)
            return DecodeCommand::Malformed;
        Reader r(bytes);
        if (r.u32() != magic)
            return DecodeCommand::Malformed;
        if (r.u16() != protocol_version)
            return DecodeCommand::Protocol;
        const auto kind = r.u8();
        MovementIdentity owner;
        owner.world = r.u64();
        owner.player = r.u64();
        owner.object = {r.u32(), r.u32()};
        owner.force_revision = r.u64();
        const auto sequence = r.u64();
        CommandMessage message;
        if (kind == 1)
        {
            CommandAction v;
            v.kind = CommandKind(r.u8());
            v.argument = r.u8();
            v.aim = {r.f32(), r.f32()};
            message = PlayerCommand{owner, sequence, v};
        }
        else if (kind == 2)
        {
            const auto applied = r.u8();
            if (applied > 1)
                return DecodeCommand::Malformed;
            message = CommandAck{owner, sequence, applied != 0};
        }
        else
            return DecodeCommand::Malformed;
        if (!r.done() || !valid_command(message))
            return DecodeCommand::Malformed;
        out = std::move(message);
        return DecodeCommand::Ok;
    }
} // namespace OpenBlood2::Net
