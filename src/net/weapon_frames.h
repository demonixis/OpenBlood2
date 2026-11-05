#pragma once
#include "net/movement.h"
namespace OpenBlood2::Net
{
    inline constexpr std::size_t weapon_frame_packet_limit = 96, weapon_frame_queue_limit = 64;
    // One original CViewWeapon tick. State names a loadout issued by this server,
    // never client-supplied ammunition, damage, cadence, origin or target.
    struct WeaponFrame
    {
        MovementIdentity owner;
        std::uint64_t sequence = 0, state = 0;
        PlayerRotation aim;
        bool primary = false, alternate = false;
    };
    struct WeaponFrameAck
    {
        MovementIdentity owner;
        std::uint64_t sequence = 0;
        std::array<std::uint64_t, 2> fired{}; // Actual authoritative fire count, not cosmetic acceptance.
        std::uint64_t blocked = 0;
    };
    using WeaponFrameMessage = std::variant<WeaponFrame, WeaponFrameAck>;
    enum class DecodeWeaponFrame
    {
        Ok,
        Malformed,
        Protocol
    };
    bool valid_weapon_frame(const WeaponFrameMessage&);
    bool is_weapon_frame_packet(std::span<const std::uint8_t>);
    bool encode_weapon_frame(const WeaponFrameMessage&, std::vector<std::uint8_t>&);
    DecodeWeaponFrame decode_weapon_frame(std::span<const std::uint8_t>, WeaponFrameMessage&);
} // namespace OpenBlood2::Net
