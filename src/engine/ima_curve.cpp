#include "engine/ima_curve.h"
#include "engine/formats/reader.h"
#include <algorithm>

namespace OpenBlood2
{
    bool read_ima_curves(std::span<const std::uint8_t> data, ImaCurves& out, std::string& error,
                         std::size_t record_budget)
    {
        out = {};
        MusicAssetInfo source;
        if (!read_music_asset(data, MusicAssetKind::Style, source, error))
            return false;
        ImaCurves next;
        const auto budget = std::min<std::size_t>(65536, record_budget);
        std::uint32_t pattern = 0, grid = 0;
        for (const auto& chunk : source.chunks)
        {
            if (chunk.depth == 1)
            {
                pattern = chunk.id == "RIFF" && chunk.form == "AAPT" ? chunk.offset : 0;
                grid = 0;
            }
            if (chunk.depth == 2)
                grid = pattern && chunk.id == "LIST" && chunk.form == "AACL" ? chunk.offset : 0;
            if (!grid || chunk.depth != 3 || chunk.id != "curv")
                continue;
            Reader r(data.subspan(chunk.offset + 8, chunk.size));
            auto fail = [&]()
            {
                error = "IMA curves: unsupported stride, truncated record or record budget exceeded";
                return false;
            };
            if (r.remaining() < 4 || r.u16() != 7 || r.u16() != 10)
                return fail();
            ++next.chunks;
            std::size_t ordinal = 0;
            while (r.remaining())
            {
                if (r.remaining() < 9 || next.records() >= budget)
                    return fail();
                ImaCurve curve;
                curve.pattern_offset = pattern;
                curve.grid_offset = grid;
                curve.chunk_offset = chunk.offset;
                curve.ordinal = ordinal++;
                curve.type = r.u8();
                curve.channel = r.u8();
                curve.time_offset = std::bit_cast<std::int16_t>(r.u16());
                curve.variations = r.u16();
                curve.controller = r.u8();
                const auto count = r.u16();
                if (count > budget - next.records() - 1 || count > r.remaining() / 10)
                    return fail();
                curve.points.reserve(count);
                for (unsigned i = 0; i < count; ++i)
                {
                    ImaCurvePoint point;
                    point.shape = r.u8();
                    point.flags = r.u8();
                    point.start = std::bit_cast<std::int16_t>(r.u16());
                    point.value = std::bit_cast<std::int16_t>(r.u16());
                    point.end = std::bit_cast<std::int16_t>(r.u16());
                    point.target = std::bit_cast<std::int16_t>(r.u16());
                    curve.points.push_back(point);
                }
                next.points += count;
                next.curves.push_back(std::move(curve));
            }
        }
        out = std::move(next);
        return true;
    }
} // namespace OpenBlood2
