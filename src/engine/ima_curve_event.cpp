#include "engine/ima_curve_event.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <numbers>

namespace OpenBlood2
{
    namespace
    {
        std::int16_t word(int value)
        {
            return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
        }
        struct CurveTables
        {
            std::array<int, 193> logarithmic{}, sine{};
            CurveTables()
            {
                for (int i = 0; i <= 192; ++i)
                {
                    logarithmic[i] = static_cast<int>(19200 * std::log10(1.0 + 9.0 * i / 192));
                    sine[i] = static_cast<int>(9600 * (1 - std::cos(std::numbers::pi * i / 192)));
                }
                // Exact mathematical boundary; libm rounding can fall just below it.
                sine[96] = 9600;
            }
        };
        int table_value(std::uint8_t shape, int index)
        {
            static const CurveTables tables;
            switch (shape)
            {
                case 2:
                    return 19200 - tables.logarithmic[192 - index];
                case 3:
                    return tables.logarithmic[index];
                case 4:
                    return tables.sine[index];
                default:
                    return 100 * index;
            }
        }
    } // namespace

    bool prepare_ima_curve_events(const ImaCurve& curve, std::int32_t tick, std::vector<ImaTimedMidiMessage>& out,
                                  std::string& error, std::size_t message_budget)
    {
        out.clear();
        error.clear();
        if (curve.type < 3 || curve.type > 6 || curve.channel >= 16 || curve.points.size() > 4096)
        {
            error = "IMA curve event: unsupported type/channel or point budget exceeded";
            return false;
        }
        const auto budget = std::min<std::size_t>(65536, message_budget);
        const auto status = static_cast<std::uint8_t>((curve.type == 3   ? 0xe0
                                                       : curve.type == 4 ? 0xb0
                                                       : curve.type == 5 ? 0xd0
                                                                         : 0xa0) |
                                                      ((curve.channel + 4) & 15));
        std::vector<ImaTimedMidiMessage> next;
        const auto emit = [&](int time, int value, bool replace = false)
        {
            const auto absolute = std::int64_t(tick) + time;
            if (absolute < std::numeric_limits<std::int32_t>::min() ||
                absolute > std::numeric_limits<std::int32_t>::max() || (!replace && next.size() >= budget))
            {
                error = "IMA curve event: signed tick overflow or message budget exceeded";
                return false;
            }
            const auto bits = static_cast<std::uint16_t>(value);
            const ImaTimedMidiMessage message{
                static_cast<std::int32_t>(absolute), status,
                static_cast<std::uint8_t>(curve.type == 3 ? bits & 127 : curve.controller),
                static_cast<std::uint8_t>(curve.type == 3 ? (bits >> 7) & 254 : bits & 255)};
            if (replace)
                next.back() = message;
            else
                next.push_back(message);
            return true;
        };
        for (const auto& point : curve.points)
        {
            if (point.shape == 1)
            {
                if (!emit(point.start, point.flags & 2 ? point.value : point.target))
                    return false;
                continue;
            }
            const int delta = word(int(point.target) - point.value);
            const int duration = word(int(point.end) - point.start);
            const auto value_at = [&](int index)
            {
                const int value = word(point.value + table_value(point.shape, index) * delta / 19200);
                return point.flags & 2 ? word(point.value - value + point.target) : word(value);
            };
            int last_value = value_at(point.flags & 1 ? 192 : 0);
            int last_time = point.start;
            if (!emit(last_time, last_value))
                return false;
            for (int i = 1; i < 192; ++i)
            {
                const int time = word(point.start + i * duration / 192);
                if (time - last_time < 3)
                    continue;
                const int value = value_at(point.flags & 1 ? 192 - i : i);
                if (value == last_value || time == last_time)
                    continue;
                if (!emit(time, value))
                    return false;
                last_time = time;
                last_value = value;
            }
            const int final_value = value_at(point.flags & 1 ? 0 : 192);
            // Style curves omit unchanged endpoints and replace the last event when
            // its time is already the endpoint. Section curves use another routine.
            if (final_value != last_value && !emit(point.end, final_value, point.end == last_time))
                return false;
        }
        out = std::move(next);
        return true;
    }
} // namespace OpenBlood2
