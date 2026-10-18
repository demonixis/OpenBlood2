#pragma once
#include "engine/ima_style.h"
#include <optional>

namespace OpenBlood2
{
    struct ImaPatternChannels
    {
        std::array<std::array<std::uint16_t, 16>, 16> variation_flags{};
        std::array<std::uint8_t, 16> play_modes{};
    };
    // Original loader defaults, without changing authored data. No pitch conversion.
    ImaPatternChannels prepare_ima_pattern_channels(const ImaPattern&);

    struct ImaVariationChoice
    {
        std::array<std::uint16_t, 16> candidates{}, selected{};
        std::uint16_t fallback_channels = 0;
        std::size_t draws_used = 0;
    };
    // One original channel-variation decision, not pattern/command scheduling.
    // Draws are explicit original rand() values (0..32767), in ascending channel order.
    // Exactly one per random channel, even with one candidate. Failure clears output.
    bool choose_ima_variations(const ImaPatternChannels&, std::optional<std::uint16_t> required_flags,
                               const std::array<std::uint16_t, 16>& current, std::uint16_t random_channels,
                               std::uint16_t cycle_channels, std::span<const std::uint16_t> draws, ImaVariationChoice&,
                               std::string& error);
} // namespace OpenBlood2
