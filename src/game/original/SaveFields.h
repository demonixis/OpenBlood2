#pragma once
#include "cpp_engineobjects_de.h"
#include <cmath>
#include <string>

// Helpers shared by original Save/Load bodies (save_load_contracts lot 8). The port keeps
// owned std::string members where the original kept HSTRINGs; an empty string is written as
// the original NULL HSTRING.
namespace OpenBlood2::save_fields
{
    // Original relative times: Save writes `t - GetTime()` and Load adds GetTime() back, both
    // in float. The restored value is the nearest float that yields the saved difference again,
    // so a save taken right after a full restore reproduces the bytes.
    inline float restored_time(float relative, float now)
    {
        const float sum = relative + now;
        if (sum - now == relative)
            return sum;
        float lower = sum, upper = sum;
        for (int step = 0; step < 4; ++step)
        {
            lower = std::nextafter(lower, -INFINITY);
            upper = std::nextafter(upper, INFINITY);
            if (upper - now == relative)
                return upper;
            if (lower - now == relative)
                return lower;
        }
        return sum;
    }
    // The original HString encoding without a temporary HSTRING: string tokens are never reused
    // within a session, so every save would otherwise consume them. A non-null HString and
    // WriteToMessageString produce the same bytes.
    inline void write_string(HMESSAGEWRITE h, const std::string& text)
    {
        if (text.empty())
            g_pServerDE->WriteToMessageHString(h, nullptr);
        else
            g_pServerDE->WriteToMessageString(h, text.c_str());
    }
    // Reads a NULL or bounded HSTRING into `out`; a longer string fails the payload.
    inline bool read_string(HMESSAGEREAD h, std::string& out, std::size_t limit = MAX_GP_STRING_LEN)
    {
        auto* s = g_pServerDE;
        out.clear();
        const auto value = s->ReadFromMessageHString(h);
        if (!value)
            return true;
        const char* text = s->GetStringData(value);
        out = text ? text : "";
        s->FreeString(value);
        if (out.size() <= limit)
            return true;
        out.clear();
        s->RejectPayload(h, "saved string exceeds " + std::to_string(limit) + " bytes");
        return false;
    }
    // The original writes booleans as (DBYTE); anything but 0 or 1 is refused.
    inline bool read_bool(HMESSAGEREAD h, bool& out)
    {
        const auto value = g_pServerDE->ReadFromMessageByte(h);
        out = value == 1;
        if (value <= 1)
            return true;
        g_pServerDE->RejectPayload(h, "saved boolean is not 0 or 1");
        return false;
    }
    inline bool finite(float value, float bound = 1e6f)
    {
        return std::isfinite(value) && std::abs(value) <= bound;
    }
} // namespace OpenBlood2::save_fields
