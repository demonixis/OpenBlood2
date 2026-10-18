#pragma once
#include "engine/music_asset.h"

namespace OpenBlood2
{
    struct ImaCurvePoint
    {
        std::uint8_t shape = 0, flags = 0;
        std::int16_t start = 0, value = 0, end = 0, target = 0;
        bool operator==(const ImaCurvePoint&) const = default;
    };
    struct ImaCurve
    {
        std::uint32_t pattern_offset = 0, grid_offset = 0, chunk_offset = 0;
        std::size_t ordinal = 0; // Within the source chunk, never sorted by time.
        std::uint8_t type = 0, channel = 0, controller = 0;
        std::int16_t time_offset = 0;
        std::uint16_t variations = 0;
        std::vector<ImaCurvePoint> points;
    };
    struct ImaCurves
    {
        std::vector<ImaCurve> curves;
        std::size_t chunks = 0, points = 0;
        std::size_t records() const
        {
            return curves.size() + points;
        }
    };
    // Installed style-grid curv profile: two u16 strides (7,10), then a header,
    // u16 point count and point records, repeated to the end of each chunk.
    // Separate from style-note validity; failure clears output. No event evaluation.
    bool read_ima_curves(std::span<const std::uint8_t>, ImaCurves&, std::string& error,
                         std::size_t record_budget = 65536);
} // namespace OpenBlood2
