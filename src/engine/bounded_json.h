#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace OpenBlood2
{
    // Minimal bounded JSON document shared by the material manifest and the GI placement files.
    // Objects keep their members in file order; duplicate keys, \u escapes, comments and trailing
    // data are refused.
    struct Json
    {
        Json();
        ~Json();
        Json(Json&&) noexcept;
        Json& operator=(Json&&) noexcept;
        enum class Kind
        {
            Null,
            Boolean,
            Number,
            String,
            Array,
            Object
        } kind = Kind::Null;
        double number = 0;
        bool boolean = false;
        std::string text;
        std::vector<Json> items;                           // Array values.
        std::vector<std::pair<std::string, Json>> members; // Object members in file order.
        // Member of an object, or nullptr.
        const Json* find(std::string_view key) const;
    };
    struct JsonLimits
    {
        int depth = 8;              // Deepest nesting accepted.
        std::size_t values = 16384; // Values in the whole document.
        std::size_t string = 512;   // Bytes per decoded string.
    };
    // Parses a whole document. On failure the error is "<prefix>: <reason> at byte <n>".
    bool parse_json(std::string_view text, Json& out, const JsonLimits& limits, std::string_view prefix,
                    std::string& error);
} // namespace OpenBlood2
