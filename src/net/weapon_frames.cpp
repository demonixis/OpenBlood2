#include "net/weapon_frames.h"
#include "net/admission.h"
#include "net/codec.h"
#include <type_traits>
namespace OpenBlood2::Net
{
    namespace
    {
        constexpr std::uint32_t magic = 0x46573242; // B2WF.
    }
    bool valid_weapon_frame(const WeaponFrameMessage& message)
    {
        return std::visit(
            [](const auto& v)
            {
                if (!v.owner.world || !v.owner.player || !v.owner.object || !v.owner.object.generation ||
                    v.owner.object.slot >= 65536 || !v.owner.force_revision || !v.sequence)
                    return false;
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>, WeaponFrame>)
                    return v.state && std::isfinite(v.aim.pitch) && std::isfinite(v.aim.yaw) &&
                           std::abs(v.aim.pitch) <= 1e6f && std::abs(v.aim.yaw) <= 1e6f;
                return true;
            },
            message);
    }
    bool is_weapon_frame_packet(std::span<const std::uint8_t> bytes)
    {
        Reader r(bytes);
        return bytes.size() >= 4 && r.u32() == magic;
    }
    bool encode_weapon_frame(const WeaponFrameMessage& message, std::vector<std::uint8_t>& out)
    {
        if (!valid_weapon_frame(message))
            return false;
        Writer w(weapon_frame_packet_limit);
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
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>, WeaponFrame>)
                {
                    w.u64(v.state);
                    w.f32(v.aim.pitch);
                    w.f32(v.aim.yaw);
                    w.u8(v.primary | v.alternate << 1);
                }
                else
                {
                    for (const auto count : v.fired)
                        w.u64(count);
                    w.u64(v.blocked);
                }
            },
            message);
        if (!w.good())
            return false;
        out = w.bytes();
        return true;
    }
    DecodeWeaponFrame decode_weapon_frame(std::span<const std::uint8_t> bytes, WeaponFrameMessage& out)
    {
        if (bytes.size() < 7 || bytes.size() > weapon_frame_packet_limit)
            return DecodeWeaponFrame::Malformed;
        Reader r(bytes);
        if (r.u32() != magic)
            return DecodeWeaponFrame::Malformed;
        if (r.u16() != protocol_version)
            return DecodeWeaponFrame::Protocol;
        const auto kind = r.u8();
        MovementIdentity owner;
        owner.world = r.u64();
        owner.player = r.u64();
        owner.object = {r.u32(), r.u32()};
        owner.force_revision = r.u64();
        const auto sequence = r.u64();
        WeaponFrameMessage message;
        if (kind == 1)
        {
            const auto state = r.u64();
            const PlayerRotation aim{r.f32(), r.f32()};
            const auto buttons = r.u8();
            if (buttons > 3)
                return DecodeWeaponFrame::Malformed;
            message = WeaponFrame{owner, sequence, state, aim, bool(buttons & 1), bool(buttons & 2)};
        }
        else if (kind == 2)
        {
            WeaponFrameAck value{owner, sequence};
            for (auto& count : value.fired)
                count = r.u64();
            value.blocked = r.u64();
            message = value;
        }
        else
            return DecodeWeaponFrame::Malformed;
        if (!r.done() || !valid_weapon_frame(message))
            return DecodeWeaponFrame::Malformed;
        out = std::move(message);
        return DecodeWeaponFrame::Ok;
    }
} // namespace OpenBlood2::Net
