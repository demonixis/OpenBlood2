#pragma once
#include "engine/ima_timeline.h"
#include <optional>

namespace OpenBlood2
{
    struct ImaPatternRequest
    {
        std::int16_t measures = 0;
        std::uint32_t flags = 0;
    };
    struct ImaSectionSelection
    {
        std::int16_t measure = 0;
        std::size_t chord = 0;           // Index into retained timeline/harmony, not raw section chords.
        std::optional<std::size_t> mute; // Index into the prepared mute timeline.
        std::uint16_t mute_mask = 0;
        ImaPatternRequest request;
    };
    enum class ImaPatternMatch
    {
        Empty,
        Length,
        LastEligible,
        HeadFallback
    };
    struct ImaPatternSelection
    {
        std::optional<std::size_t> pattern;
        ImaPatternMatch match = ImaPatternMatch::Empty;
    };
    // Original authored-section decision at an explicit tick (normal modes 2/3, no seek).
    // Requires a retained chord: the original empty-list global fallback is not guessed.
    // No clock, activation, random draw, I/O or event dispatch. Failure clears output.
    bool select_ima_section(const ImaSection&, const ImaTimeline&, std::int32_t tick, ImaSectionSelection&,
                            std::string& error);
    // Original flags/category/descending-length choice; bounded linear equivalent of its repeated scans.
    bool select_ima_pattern(const ImaStyle&, ImaPatternRequest, ImaPatternSelection&, std::string& error);
    const char* ima_pattern_match_name(ImaPatternMatch);
} // namespace OpenBlood2
