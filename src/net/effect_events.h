#pragma once
#include "engine/object_id.h"
#include "game/impact_settings.h"
#include <span>
#include <string>
namespace OpenBlood2::Net
{
    inline constexpr std::size_t effect_packet_limit = 8192, effect_queue_limit = 2048;
    inline constexpr std::size_t effect_queue_bytes = 2 * 1024 * 1024;
    // Authoritative impact recipe. Decoded resources, particle arrays, renderer
    // objects and original pointers never cross this boundary.
    struct EffectEvent
    {
        std::uint64_t world = 0, player = 0, sequence = 0;
        ObjectId source;
        std::string name;
        InstantEffectRecipe recipe;
    };
    bool valid_effect(const EffectEvent&);
    bool is_effect_packet(std::span<const std::uint8_t>);
    bool encode_effect(const EffectEvent&, std::vector<std::uint8_t>&);
    enum class DecodeEffect
    {
        Ok,
        Malformed,
        Protocol
    };
    DecodeEffect decode_effect(std::span<const std::uint8_t>, EffectEvent&);
    // Several encoded effects per Effects-channel packet (B2EB): u16 count, then
    // u16 length + one complete B2EF encoding each. Batching changes no event.
    inline constexpr std::size_t effect_batch_limit = 16384, effect_batch_items = 256;
    bool is_effect_batch(std::span<const std::uint8_t>);
    // Appends encoded effects to `out` until the packet or item bound; returns
    // how many were consumed (0 only when none was given or one is invalid).
    std::size_t encode_effect_batch(std::span<const std::vector<std::uint8_t>> encoded, std::vector<std::uint8_t>& out);
    DecodeEffect decode_effect_batch(std::span<const std::uint8_t>, std::vector<EffectEvent>&);
} // namespace OpenBlood2::Net
