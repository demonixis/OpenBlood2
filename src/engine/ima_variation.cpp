#include "engine/ima_variation.h"
#include <algorithm>
#include <bit>

namespace OpenBlood2
{
    ImaPatternChannels prepare_ima_pattern_channels(const ImaPattern& pattern)
    {
        ImaPatternChannels out{pattern.variation_flags, pattern.play_modes};
        for (std::size_t ch = 0; ch < 16; ++ch)
        {
            std::uint16_t missing = 0x1800;
            for (auto flags : out.variation_flags[ch])
                missing &= static_cast<std::uint16_t>(~flags);
            for (auto& flags : out.variation_flags[ch])
                flags |= missing;
            if (!out.play_modes[ch])
                out.play_modes[ch] = ch == 5 ? 1 : ch == 4 ? 2 : 3;
        }
        return out;
    }

    bool choose_ima_variations(const ImaPatternChannels& pattern, std::optional<std::uint16_t> required_flags,
                               const std::array<std::uint16_t, 16>& current, std::uint16_t random_channels,
                               std::uint16_t cycle_channels, std::span<const std::uint16_t> draws,
                               ImaVariationChoice& out, std::string& error)
    {
        out = {};
        error.clear();
        if (draws.size() != static_cast<std::size_t>(std::popcount(random_channels)) ||
            std::any_of(draws.begin(), draws.end(), [](auto value) { return value > 32767; }))
        {
            error = "IMA variations: require one 15-bit random draw per random channel";
            return false;
        }
        for (std::size_t ch = 0; ch < 16; ++ch)
        {
            const auto channel_bit = static_cast<std::uint16_t>(1u << ch);
            auto& candidates = out.candidates[ch];
            if (required_flags)
                for (unsigned v = 0; v < 16; ++v)
                    if ((pattern.variation_flags[ch][v] & *required_flags) == *required_flags)
                        candidates |= static_cast<std::uint16_t>(1u << v);
            if (!candidates)
            {
                candidates = 0xffff;
                out.fallback_channels |= channel_bit;
            }
            auto& selected = out.selected[ch];
            selected = current[ch];
            if (random_channels & channel_bit)
            {
                auto index = draws[out.draws_used++] % std::popcount(candidates);
                for (unsigned v = 0; v < 16; ++v)
                    if (candidates & (1u << v))
                        if (index-- == 0)
                        {
                            selected = static_cast<std::uint16_t>(1u << v);
                            break;
                        }
            }
            else if (cycle_channels & channel_bit)
                selected = (selected & 0x8000) ? 1 : static_cast<std::uint16_t>(selected << 1);
        }
        return true;
    }
} // namespace OpenBlood2
