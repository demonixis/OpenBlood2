#include "engine/surface_materials.h"
#include "surface_manifest.h" // Generated from config/materials.json.
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <memory>

namespace OpenBlood2
{
    namespace
    {
        // Minimal bounded JSON document: enough for the manifest, nothing more.
        struct Json
        {
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
        };
        class JsonReader
        {
          public:
            explicit JsonReader(std::string_view text) : text_(text) {}
            bool document(Json& out, std::string& error)
            {
                if (!value(out, 0))
                {
                    error = "Material manifest: " + error_ + " at byte " + std::to_string(at_);
                    return false;
                }
                space();
                if (at_ != text_.size())
                {
                    error = "Material manifest: trailing data at byte " + std::to_string(at_);
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
            bool fail(const char* message)
            {
                error_ = message;
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
                            case '/':
                                break;
                            case 'n':
                                c = '\n';
                                break;
                            case 't':
                                c = '\t';
                                break;
                            default:
                                return fail("unsupported escape"); // \u is not needed by any manifest key.
                        }
                    }
                    if (out.size() >= 512)
                        return fail("string longer than 512 bytes");
                    out.push_back(c);
                }
                if (at_ >= text_.size())
                    return fail("unterminated string");
                ++at_;
                return true;
            }
            bool value(Json& out, int depth)
            {
                if (depth > 8)
                    return fail("nesting deeper than 8");
                if (++values_ > 16384)
                    return fail("more than 16384 values");
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
            std::size_t at_ = 0, values_ = 0;
            std::string error_;
        };
        std::string normalized(std::string_view path)
        {
            std::string out(path);
            for (auto& c : out)
                c = c == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(c)));
            return out;
        }
        // Applies an object's material values over `material`. Unknown keys fail.
        bool apply_values(const Json& object, SurfaceMaterial& material, bool allow_class, std::string& class_name,
                          std::string& error)
        {
            for (const auto& [key, value] : object.members)
            {
                if (allow_class && key == "class")
                {
                    if (value.kind != Json::Kind::String)
                        return error = "Material manifest: class must be a string", false;
                    class_name = value.text;
                    continue;
                }
                float* field = key == "roughness"            ? &material.roughness
                               : key == "metallic"           ? &material.metallic
                               : key == "specular"           ? &material.specular
                               : key == "normal"             ? &material.normal
                               : key == "emissive"           ? &material.emissive
                               : key == "emissive_threshold" ? &material.emissive_threshold
                                                             : nullptr;
                const float maximum = key == "specular" || key == "normal" ? 4.f
                                      : key == "emissive"                  ? 8.f
                                      : key == "emissive_threshold"        ? .95f
                                                                           : 1.f;
                const float minimum = key == "roughness" ? .02f : 0.f;
                if (!field)
                    return error = "Material manifest: unknown key '" + key + "'", false;
                if (value.kind != Json::Kind::Number || value.number < minimum || value.number > maximum)
                    return error = "Material manifest: '" + key + "' outside [" + std::to_string(minimum) + ", " +
                                   std::to_string(maximum) + "]",
                           false;
                *field = float(value.number);
            }
            return true;
        }
    } // namespace
    std::string_view builtin_surface_manifest()
    {
        return blood2_surface_manifest;
    }
    bool surface_glob(std::string_view pattern, std::string_view path)
    {
        auto fold = [](char c)
        { return c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
        // Iterative matcher: backtracks only to the most recent '*', so work is O(pattern * path).
        std::size_t p = 0, s = 0, star = std::string_view::npos, resume = 0;
        while (s < path.size())
        {
            if (p < pattern.size() && (pattern[p] == '?' || fold(pattern[p]) == fold(path[s])) && pattern[p] != '*')
            {
                ++p;
                ++s;
            }
            else if (p < pattern.size() && pattern[p] == '*')
            {
                star = p++;
                resume = s;
            }
            else if (star != std::string_view::npos)
            {
                p = star + 1;
                s = ++resume;
            }
            else
                return false;
        }
        while (p < pattern.size() && pattern[p] == '*')
            ++p;
        return p == pattern.size();
    }
    bool SurfaceManifest::parse_builtin(std::string& error)
    {
        return parse(builtin_surface_manifest(), error);
    }
    bool SurfaceManifest::parse(std::string_view text, std::string& error)
    {
        error.clear();
        if (text.size() > byte_limit)
        {
            error = "Material manifest larger than 64 KiB";
            return false;
        }
        Json root;
        if (!JsonReader(text).document(root, error))
            return false;
        if (root.kind != Json::Kind::Object)
            return error = "Material manifest: root must be an object", false;
        std::vector<SurfaceMaterial> classes{SurfaceMaterial{}};
        std::vector<Rule> rules;
        std::vector<Exact> textures;
        bool versioned = false;
        auto find_class = [&](const std::string& name, std::uint32_t& index)
        {
            for (std::size_t i = 0; i < classes.size(); ++i)
                if (classes[i].name == name)
                {
                    index = std::uint32_t(i);
                    return true;
                }
            error = "Material manifest: unknown class '" + name + "'";
            return false;
        };
        // Classes first: rules and textures may appear before them in the file.
        for (const auto& [key, value] : root.members)
        {
            if (key == "version")
            {
                if (value.kind != Json::Kind::Number || value.number != 1)
                    return error = "Material manifest: unsupported version", false;
                versioned = true;
            }
            else if (key == "classes")
            {
                if (value.kind != Json::Kind::Object || value.members.size() > class_limit)
                    return error = "Material manifest: classes must be an object of at most 256 entries", false;
                for (const auto& [name, values] : value.members)
                {
                    if (values.kind != Json::Kind::Object || name.empty())
                        return error = "Material manifest: class '" + name + "' must be an object", false;
                    SurfaceMaterial material;
                    material.name = name;
                    std::string unused;
                    if (!apply_values(values, material, false, unused, error))
                        return false;
                    if (name == "default")
                        classes[0] = material;
                    else
                        classes.push_back(material);
                }
            }
            else if (key != "rules" && key != "textures" && key != "comment")
                return error = "Material manifest: unknown key '" + key + "'", false;
        }
        if (!versioned)
            return error = "Material manifest: missing version", false;
        for (const auto& [key, value] : root.members)
        {
            if (key == "rules")
            {
                if (value.kind != Json::Kind::Array || value.items.size() > rule_limit)
                    return error = "Material manifest: rules must be an array of at most 1024 entries", false;
                for (const auto& rule : value.items)
                {
                    const Json *match = nullptr, *name = nullptr;
                    for (const auto& [field, entry] : rule.members)
                    {
                        if (field == "match")
                            match = &entry;
                        else if (field == "class")
                            name = &entry;
                        else
                            return error = "Material manifest: unknown rule key '" + field + "'", false;
                    }
                    if (rule.kind != Json::Kind::Object || !match || !name || match->kind != Json::Kind::String ||
                        name->kind != Json::Kind::String || match->text.empty())
                        return error = "Material manifest: a rule needs string 'match' and 'class'", false;
                    Rule entry{match->text, 0};
                    if (!find_class(name->text, entry.material))
                        return false;
                    rules.push_back(std::move(entry));
                }
            }
            else if (key == "textures")
            {
                if (value.kind != Json::Kind::Object || value.members.size() > rule_limit)
                    return error = "Material manifest: textures must be an object of at most 1024 entries", false;
                for (const auto& [path, values] : value.members)
                {
                    if (values.kind != Json::Kind::Object || path.empty())
                        return error = "Material manifest: texture '" + path + "' must be an object", false;
                    std::string class_name = "default";
                    for (const auto& [field, entry] : values.members)
                        if (field == "class" && entry.kind == Json::Kind::String)
                            class_name = entry.text;
                    std::uint32_t base = 0;
                    if (!find_class(class_name, base))
                        return false;
                    Exact exact{normalized(path), classes[base]};
                    if (!apply_values(values, exact.material, true, class_name, error))
                        return false;
                    textures.push_back(std::move(exact));
                }
            }
        }
        classes_ = std::move(classes);
        rules_ = std::move(rules);
        textures_ = std::move(textures);
        ++revision_;
        return true;
    }
    const SurfaceMaterial& SurfaceManifest::resolve(std::string_view path) const
    {
        return *lookup(path).material;
    }
    SurfaceResolution SurfaceManifest::lookup(std::string_view path) const
    {
        if (!textures_.empty())
        {
            const auto key = normalized(path);
            for (std::size_t i = 0; i < textures_.size(); ++i)
                if (textures_[i].path == key)
                    return {&textures_[i].material, SurfaceSource::Texture, i};
        }
        for (std::size_t i = 0; i < rules_.size(); ++i)
            if (surface_glob(rules_[i].pattern, path))
                return {&classes_[rules_[i].material], SurfaceSource::Rule, i};
        return {&classes_[0], SurfaceSource::Default, 0};
    }
} // namespace OpenBlood2
