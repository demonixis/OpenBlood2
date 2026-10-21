#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace OpenBlood2
{
    // Physically based surface parameters of one texture for the enhanced renderer. Replacement
    // data: the original game has no material classes. specular scales the dielectric F0 (0.04);
    // normal scales generated normal-map relief (0 disables it for this class). emissive is the
    // glow strength of texels whose luminance exceeds emissive_threshold (0: no emission).
    struct SurfaceMaterial
    {
        std::string name = "default";
        float roughness = .8f, metallic = 0, specular = 1, normal = 1, emissive = 0, emissive_threshold = .5f;
        bool operator==(const SurfaceMaterial&) const = default;
    };
    // Where a path's material came from: an exact texture override, a rule, or the default.
    enum class SurfaceSource : std::uint8_t
    {
        Default,
        Rule,
        Texture
    };
    struct SurfaceResolution
    {
        const SurfaceMaterial* material = nullptr;
        SurfaceSource source = SurfaceSource::Default;
        std::size_t index = 0; // Rule or texture override index.
    };
    // Case-insensitive wildcard match of a resource path: '*' spans any run (including '/'),
    // '?' one character; '\\' and '/' compare equal. Bounded backtracking.
    bool surface_glob(std::string_view pattern, std::string_view path);
    // Versioned JSON manifest: named classes, ordered wildcard rules (first match wins) and
    // exact per-texture overrides that may replace single class values.
    //   {"version": 1, "classes": {"stone": {"roughness": .7, ...}},
    //    "rules": [{"match": "textures/*brick*", "class": "stone"}],
    //    "textures": {"textures/x/y.dtx": {"class": "metal", "roughness": .3}}}
    // Parsing is bounded (64 KiB, 256 classes, 1024 rules/textures) and fails atomically
    // with an explicit error; unknown keys and out-of-range values are rejected.
    class SurfaceManifest
    {
      public:
        bool parse(std::string_view json, std::string& error);
        // The built-in manifest (config/materials.json, embedded at build time).
        bool parse_builtin(std::string& error);
        // Unmatched paths resolve to the "default" class.
        const SurfaceMaterial& resolve(std::string_view path) const;
        // resolve() with the binding that decided it.
        SurfaceResolution lookup(std::string_view path) const;
        // The pattern of a rule, or the normalized path of a texture override, by lookup() index.
        const std::string& rule_pattern(std::size_t index) const
        {
            return rules_[index].pattern;
        }
        const std::string& override_path(std::size_t index) const
        {
            return textures_[index].path;
        }
        std::size_t classes() const
        {
            return classes_.size();
        }
        std::size_t rules() const
        {
            return rules_.size();
        }
        std::size_t overrides() const
        {
            return textures_.size();
        }
        static constexpr std::size_t byte_limit = 64 * 1024, class_limit = 256, rule_limit = 1024;

      private:
        struct Rule
        {
            std::string pattern;
            std::uint32_t material = 0;
        };
        struct Exact
        {
            std::string path; // Lowercase, '/' separators.
            SurfaceMaterial material;
        };
        std::vector<SurfaceMaterial> classes_{SurfaceMaterial{}};
        std::vector<Rule> rules_;
        std::vector<Exact> textures_;
    };
    // The text compiled into the binary.
    std::string_view builtin_surface_manifest();
} // namespace OpenBlood2
