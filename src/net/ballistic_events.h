#pragma once
#include "engine/object_id.h"
#include "game/ballistic_settings.h"
#include <span>
#include <string>
namespace OpenBlood2::Net
{
    inline constexpr std::size_t ballistic_packet_limit = 8192, ballistic_queue_limit = 256;
    // Authoritative impact recipe. Decoded resources, particle arrays, renderer
    // objects and original pointers never cross this boundary.
    struct BallisticEvent
    {
        std::uint64_t world = 0, player = 0, sequence = 0;
        ObjectId source;
        std::string name;
        ShotgunEffectSettings settings;
    };
    bool valid_ballistic(const BallisticEvent&);
    bool is_ballistic_packet(std::span<const std::uint8_t>);
    bool encode_ballistic(const BallisticEvent&, std::vector<std::uint8_t>&);
    enum class DecodeBallistic
    {
        Ok,
        Malformed,
        Protocol
    };
    DecodeBallistic decode_ballistic(std::span<const std::uint8_t>, BallisticEvent&);
} // namespace OpenBlood2::Net
