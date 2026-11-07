#pragma once
#include "engine/resources.h"
#include "engine/sha256.h"
#include "engine/world.h"
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The baked GI file (.ob2gi, gi_contracts.md lot G2): one bounded, validated, content-keyed file
// per world. Bytes are deterministic: no timestamps, host names or padding garbage.
namespace OpenBlood2
{
    namespace gi_format
    {
        constexpr std::uint32_t version = 1;
        constexpr std::uint32_t endian_marker = 0x01020304;
        constexpr std::uint64_t max_file = 256ull * 1024 * 1024;
        constexpr std::size_t max_world_path = 128, max_settings = 4096;
        constexpr std::uint32_t tag(char a, char b, char c, char d)
        {
            return std::uint32_t(std::uint8_t(a)) | std::uint32_t(std::uint8_t(b)) << 8 |
                   std::uint32_t(std::uint8_t(c)) << 16 | std::uint32_t(std::uint8_t(d)) << 24;
        }
        // Sections in their only accepted order; INFO is mandatory and first, the others optional.
        constexpr std::uint32_t info = tag('I', 'N', 'F', 'O');
        constexpr std::uint32_t placement = tag('P', 'L', 'A', 'C'); // Placement the bake used (G4/G5).
        constexpr std::uint32_t lightmaps = tag('L', 'M', 'A', 'P'); // Indirect light per texel (G7).
        constexpr std::uint32_t vertices = tag('V', 'T', 'X', ' ');  // Indirect light per corner (G7).
        constexpr std::uint32_t probes = tag('P', 'R', 'O', 'B');    // SH L2 light probes (G9).
        constexpr std::uint32_t reflections = tag('R', 'E', 'F', 'L'); // Reflection probes (G11).
        constexpr std::uint32_t order[]{info, placement, lightmaps, vertices, probes, reflections};
    } // namespace gi_format
    // Bumped whenever a bake would produce different numbers from the same inputs.
    // 2: lot G7 (sky, lightmaps, vertex light); 3: lot G9 (light probes).
    constexpr std::uint32_t gi_algorithm_version = 3;

    enum class GiEdition : std::uint8_t
    {
        Retail = 0,
        Nightmares = 1,
        Demo = 2
    };
    GiEdition gi_edition(const Resources&);
    const char* gi_edition_name(GiEdition);

    // What a bake was derived from. `settings` is the SHA-256 of the INFO settings text.
    struct GiKey
    {
        Sha256Digest world{}, albedo{}, settings{}, placement{};
        std::uint32_t algorithm = gi_algorithm_version;
        GiEdition edition = GiEdition::Retail;
        std::string world_path; // Normalized resource path, e.g. "worlds/01_subwaya.dat".
    };
    struct GiSection
    {
        std::uint32_t tag = 0;
        std::vector<std::uint8_t> payload; // Decoded by the lot that owns the tag.
    };
    struct GiFile
    {
        GiKey key;
        std::string settings; // Canonical bake settings text (UTF-8, at most 4096 bytes).
        std::vector<GiSection> sections; // Without INFO, which the container writes from `settings`.
        const GiSection* find(std::uint32_t tag) const;
    };
    std::string gi_tag_name(std::uint32_t tag);

    // Serializes `file`. Fails (naming the reason) on an invalid key, oversized text, a duplicate,
    // unknown or out-of-order section, or a file over the limit. key.settings must equal the
    // SHA-256 of `settings`.
    bool write_gi_file(const GiFile&, std::vector<std::uint8_t>& bytes, std::string& error);
    // Validates everything before returning: magic, version, endian marker, sizes, the world
    // path, every section (order, size, CRC-32), the settings hash and the SHA-256 trailer.
    // Trailing or missing bytes are refused. Output is untouched on failure.
    bool read_gi_file(std::span<const std::uint8_t>, GiFile&, std::string& error);
    // write_gi_file then an atomic replace (the previous file survives any failure).
    bool save_gi_file(const std::filesystem::path&, const GiFile&, std::string& error);

    // Freshness of a valid file against the mounted content.
    enum class GiStatus
    {
        Missing, // No file.
        Ready,   // Every compared key matches: the only status that is ever applied.
        Stale,   // Same world DAT, but other inputs changed (reasons below). Reported, never applied.
        Foreign, // Another world or DAT version: never applied, not a candidate.
        Failed   // Unreadable or invalid file.
    };
    const char* gi_status_name(GiStatus);
    enum GiStaleReason : unsigned
    {
        gi_stale_albedo = 1,
        gi_stale_algorithm = 2,
        gi_stale_edition = 4,
        gi_stale_settings = 8,
        gi_stale_placement = 16
    };
    std::string gi_stale_text(unsigned reasons); // "albedo, settings" ...
    // Settings and placement are compared only when given: the runtime accepts any bake settings,
    // while `--bake-gi --gi-if-stale` asks for its own.
    struct GiExpectation
    {
        Sha256Digest world{}, albedo{};
        GiEdition edition = GiEdition::Retail;
        std::uint32_t algorithm = gi_algorithm_version;
        std::optional<Sha256Digest> settings, placement;
    };
    GiStatus gi_status(const GiKey&, const GiExpectation&, unsigned& stale);

    // SHA-256 over the textures the world references (every model's texture list, plus the DTX
    // frames of SPR textures), in sorted order: path, then present/absent, then the providing
    // archive's directory digest, entry offset and size. Missing textures are keyed as absent.
    bool gi_albedo_key(const World&, const Resources&, Sha256Digest&, std::string& error);
    // World digest, albedo key, edition and algorithm for a mounted world.
    bool gi_expectation(const Resources&, std::string_view world_path, GiExpectation&, std::string& error);

    // "worlds/01_subwaya.<first 12 hex digits>.ob2gi" for "Worlds\01_SubwayA.dat".
    bool gi_file_name(std::string_view world_path, const Sha256Digest& world, std::string& name);

    // Where baked files are looked for, in order; the first Ready file wins.
    struct GiRoot
    {
        std::string source; // "--gi-dir", "user", "sdcard", "packaged".
        std::string path;   // UTF-8 directory; may be an Android asset path.
    };
    // configured (--gi-dir or gi_dir=), user_directory()/gi, /sdcard/openblood2/gi on Android,
    // then SDL_GetBasePath()/gi. Roots that cannot be resolved are skipped.
    std::vector<GiRoot> gi_search_roots(const std::filesystem::path& configured);
    struct GiCandidate
    {
        std::string source, path;
        GiStatus status = GiStatus::Missing;
        unsigned stale = 0;
        std::string detail; // Failure text for Failed.
    };
    struct GiLookup
    {
        std::vector<GiCandidate> candidates; // One per root, in search order.
        std::optional<GiFile> file;          // The first Ready file.
        int chosen = -1;                     // Its candidate index.
    };
    // Reads at most one file per root (SDL I/O, so Android assets work); never writes or deletes.
    bool find_gi_file(std::span<const GiRoot>, std::string_view world_path, const GiExpectation&, GiLookup&,
                      std::string& error);
    // Bounded whole-file read through SDL I/O. False with an empty error when the file is absent.
    bool read_gi_bytes(const std::string& path, std::vector<std::uint8_t>& bytes, std::string& error);
} // namespace OpenBlood2
