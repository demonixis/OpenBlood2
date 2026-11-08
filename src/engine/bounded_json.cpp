#include "engine/bounded_json.h"
#include <charconv>
#include <cmath>
#include <system_error>

namespace OpenBlood2
{
    // Define after Json is complete: Clang/libstdc++ 16 otherwise instantiates
    // pair<string, Json>'s destruction while checking its default constructor.
    Json::Json() = default;
    Json::~Json() = default;
    Json::Json(Json&&) noexcept = default;
    Json& Json::operator=(Json&&) noexcept = default;
    const Json* Json::find(std::string_view key) const
    {
        for (const auto& [name, value] : members)
            if (name == key)
                return &value;
        return nullptr;
    }

    namespace
    {
        class JsonReader
        {
          public:
            JsonReader(std::string_view text, const JsonLimits& limits) : text_(text), limits_(limits) {}
            bool document(Json& out, std::string_view prefix, std::string& error)
            {
                if (!value(out, 0))
                {
                    error = std::string(prefix) + ": " + error_ + " at byte " + std::to_string(at_);
                    return false;
                }
                space();
                if (at_ != text_.size())
                {
                    error = std::string(prefix) + ": trailing data at byte " + std::to_string(at_);
                    return false;
                }
                return true;
            }

          private:
            void space()
            {
                while (at_ < text_.size() &&
                       (text_[at_] == ' ' || text_[at_] == '\t' || text_[at_] == '\n' || text_[at_] == '\r'))
                    ++at_;
            }
            bool fail(std::string message)
            {
                error_ = std::move(message);
                return false;
            }
            bool literal(std::string_view word)
            {
                if (text_.substr(at_, word.size()) != word)
                    return fail("invalid literal");
                at_ += word.size();
                return true;
            }
            bool string(std::string& out)
            {
                if (at_ >= text_.size() || text_[at_] != '"')
                    return fail("expected string");
                ++at_;
                out.clear();
                while (at_ < text_.size() && text_[at_] != '"')
                {
                    char c = text_[at_++];
                    if (static_cast<unsigned char>(c) < 0x20)
                        return fail("control character in string");
                    if (c == '\\')
                    {
                        if (at_ >= text_.size())
                            return fail("unterminated escape");
                        c = text_[at_++];
                        switch (c)
                        {
                        case '"':
                        case '\\':
                        case '/': break;
                        case 'n': c = '\n'; break;
                        case 't': c = '\t'; break;
                        default: return fail("unsupported escape"); // \u is not needed by any document.
                        }
                    }
                    if (out.size() >= limits_.string)
                        return fail("string longer than " + std::to_string(limits_.string) + " bytes");
                    out.push_back(c);
                }
                if (at_ >= text_.size())
                    return fail("unterminated string");
                ++at_;
                return true;
            }
            bool value(Json& out, int depth)
            {
                if (depth > limits_.depth)
                    return fail("nesting deeper than " + std::to_string(limits_.depth));
                if (++values_ > limits_.values)
                    return fail("more than " + std::to_string(limits_.values) + " values");
                space();
                if (at_ >= text_.size())
                    return fail("unexpected end");
                const char c = text_[at_];
                if (c == '{')
                {
                    out.kind = Json::Kind::Object;
                    ++at_;
                    space();
                    if (at_ < text_.size() && text_[at_] == '}')
                        return ++at_, true;
                    for (;;)
                    {
                        space();
                        std::pair<std::string, Json> member;
                        if (!string(member.first))
                            return false;
                        space();
                        if (at_ >= text_.size() || text_[at_++] != ':')
                            return fail("expected ':'");
                        if (!value(member.second, depth + 1))
                            return false;
                        for (const auto& existing : out.members)
                            if (existing.first == member.first)
                                return fail("duplicate key");
                        out.members.push_back(std::move(member));
                        space();
                        if (at_ < text_.size() && text_[at_] == ',')
                        {
                            ++at_;
                            continue;
                        }
                        if (at_ < text_.size() && text_[at_] == '}')
                            return ++at_, true;
                        return fail("expected ',' or '}'");
                    }
                }
                if (c == '[')
                {
                    out.kind = Json::Kind::Array;
                    ++at_;
                    space();
                    if (at_ < text_.size() && text_[at_] == ']')
                        return ++at_, true;
                    for (;;)
                    {
                        out.items.emplace_back();
                        if (!value(out.items.back(), depth + 1))
                            return false;
                        space();
                        if (at_ < text_.size() && text_[at_] == ',')
                        {
                            ++at_;
                            continue;
                        }
                        if (at_ < text_.size() && text_[at_] == ']')
                            return ++at_, true;
                        return fail("expected ',' or ']'");
                    }
                }
                if (c == '"')
                {
                    out.kind = Json::Kind::String;
                    return string(out.text);
                }
                if (c == 't' || c == 'f')
                {
                    out.kind = Json::Kind::Boolean;
                    out.boolean = c == 't';
                    return literal(c == 't' ? "true" : "false");
                }
                if (c == 'n')
                    return literal("null");
                // RFC 8259 number grammar subset accepted by from_chars (no leading '+').
                const auto* first = text_.data() + at_;
                const auto* last = text_.data() + text_.size();
                const auto parsed = std::from_chars(first, last, out.number);
                if (parsed.ec != std::errc{} || parsed.ptr == first || !std::isfinite(out.number))
                    return fail("invalid number");
                out.kind = Json::Kind::Number;
                at_ += std::size_t(parsed.ptr - first);
                return true;
            }
            std::string_view text_;
            JsonLimits limits_;
            std::size_t at_ = 0, values_ = 0;
            std::string error_;
        };
    } // namespace

    bool parse_json(std::string_view text, Json& out, const JsonLimits& limits, std::string_view prefix,
                    std::string& error)
    {
        out = {};
        return JsonReader(text, limits).document(out, prefix, error);
    }
} // namespace OpenBlood2
