#include "engine/gi_file.h"
#include "engine/atomic_file.h"
#include "engine/sprite.h"
#include "engine/user_directory.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <set>

namespace OpenBlood2
{
    namespace
    {
        namespace F = gi_format;
        constexpr std::uint8_t magic[8]{'O', 'B', '2', 'G', 'I', 0x1a, 0, 0};
        // Fixed header: magic, version, endian marker, file size, four digests, algorithm,
        // edition and three zero bytes, path length (u16) and two zero bytes.
        constexpr std::size_t fixed_header = 8 + 4 + 4 + 8 + 32 * 4 + 4 + 4 + 4;
        constexpr std::size_t section_header = 4 + 4 + 8 + 4; // Tag, flags (0), size, CRC-32.

        int order_index(std::uint32_t tag)
        {
            for (std::size_t i = 0; i < std::size(F::order); ++i)
                if (F::order[i] == tag)
                    return int(i);
            return -1;
        }
        std::uint32_t crc(std::span<const std::uint8_t> bytes)
        {
            return SDL_crc32(0, bytes.data(), bytes.size());
        }
        class Out
        {
          public:
            explicit Out(std::vector<std::uint8_t>& bytes) : bytes_(bytes) {}
            void u8(std::uint8_t v)
            {
                bytes_.push_back(v);
            }
            void u16(std::uint16_t v)
            {
                u8(std::uint8_t(v));
                u8(std::uint8_t(v >> 8));
            }
            void u32(std::uint32_t v)
            {
                for (unsigned shift = 0; shift < 32; shift += 8)
                    u8(std::uint8_t(v >> shift));
            }
            void u64(std::uint64_t v)
            {
                for (unsigned shift = 0; shift < 64; shift += 8)
                    u8(std::uint8_t(v >> shift));
            }
            void raw(std::span<const std::uint8_t> v)
            {
                bytes_.insert(bytes_.end(), v.begin(), v.end());
            }

          private:
            std::vector<std::uint8_t>& bytes_;
        };
        class In
        {
          public:
            explicit In(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
            bool take(std::size_t n, std::span<const std::uint8_t>& out)
            {
                if (n > bytes_.size() - at_)
                    return false;
                out = bytes_.subspan(at_, n);
                at_ += n;
                return true;
            }
            template <typename T> bool uint(T& v)
            {
                std::span<const std::uint8_t> b;
                if (!take(sizeof(T), b))
                    return false;
                v = 0;
                for (std::size_t i = 0; i < sizeof(T); ++i)
                    v |= T(b[i]) << (8 * i);
                return true;
            }
            bool digest(Sha256Digest& d)
            {
                std::span<const std::uint8_t> b;
                if (!take(d.size(), b))
                    return false;
                std::copy(b.begin(), b.end(), d.begin());
                return true;
            }
            std::size_t at() const
            {
                return at_;
            }

          private:
            std::span<const std::uint8_t> bytes_;
            std::size_t at_ = 0;
        };
        std::span<const std::uint8_t> text_bytes(const std::string& text)
        {
            return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
        }
        bool valid_world_path(const std::string& path)
        {
            std::string normalized;
            return !path.empty() && path.size() <= F::max_world_path && resource_path(path, normalized) &&
                   normalized == path && path.ends_with(".dat");
        }
        bool valid_utf8(std::string_view text)
        {
            for (std::size_t i = 0; i < text.size();)
            {
                const auto c = static_cast<unsigned char>(text[i]);
                const std::size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
                if (!n || n > text.size() - i || (n == 1 && c == 0))
                    return false;
                for (std::size_t k = 1; k < n; ++k)
                    if ((static_cast<unsigned char>(text[i + k]) >> 6) != 2)
                        return false;
                i += n;
            }
            return true;
        }
    } // namespace

    GiEdition gi_edition(const Resources& resources)
    {
        return resources.demo() ? GiEdition::Demo : resources.nightmares() ? GiEdition::Nightmares : GiEdition::Retail;
    }
    const char* gi_edition_name(GiEdition edition)
    {
        switch (edition)
        {
        case GiEdition::Retail: return "retail";
        case GiEdition::Nightmares: return "nightmares";
        case GiEdition::Demo: return "demo";
        }
        return "unknown";
    }
    const GiSection* GiFile::find(std::uint32_t tag) const
    {
        for (const auto& section : sections)
            if (section.tag == tag)
                return &section;
        return nullptr;
    }
    std::string gi_tag_name(std::uint32_t tag)
    {
        std::string out;
        for (unsigned shift = 0; shift < 32; shift += 8)
        {
            const auto c = static_cast<char>((tag >> shift) & 255);
            out.push_back(c >= 32 && c <= 126 ? c : '?');
        }
        while (!out.empty() && out.back() == ' ')
            out.pop_back();
        return out;
    }

    bool write_gi_file(const GiFile& file, std::vector<std::uint8_t>& bytes, std::string& error)
    {
        error.clear();
        if (!valid_world_path(file.key.world_path))
            return error = "GI world path must be a normalized .dat resource path of at most 128 bytes", false;
        if (file.settings.size() > F::max_settings || !valid_utf8(file.settings))
            return error = "GI settings text must be UTF-8 without NUL, at most 4096 bytes", false;
        if (sha256(text_bytes(file.settings)) != file.key.settings)
            return error = "GI settings hash does not match the settings text", false;
        if (file.key.edition > GiEdition::Demo)
            return error = "Unknown GI edition", false;
        int previous = 0; // INFO.
        for (const auto& section : file.sections)
        {
            const int index = order_index(section.tag);
            if (index <= previous)
                return error = "GI section " + gi_tag_name(section.tag) + " is unknown, duplicate or out of order",
                       false;
            previous = index;
        }
        std::vector<std::uint8_t> out;
        Out o(out);
        o.raw(magic);
        o.u32(F::version);
        o.u32(F::endian_marker);
        o.u64(0); // File size, patched below.
        for (const auto* d : {&file.key.world, &file.key.albedo, &file.key.settings, &file.key.placement})
            o.raw(*d);
        o.u32(file.key.algorithm);
        o.u8(std::uint8_t(file.key.edition));
        o.u8(0);
        o.u8(0);
        o.u8(0);
        o.u16(std::uint16_t(file.key.world_path.size()));
        o.u16(0);
        o.raw(text_bytes(file.key.world_path));
        o.u32(std::uint32_t(1 + file.sections.size()));
        auto section = [&](std::uint32_t tag, std::span<const std::uint8_t> payload)
        {
            o.u32(tag);
            o.u32(0);
            o.u64(payload.size());
            o.u32(crc(payload));
            o.raw(payload);
        };
        std::vector<std::uint8_t> info;
        Out(info).u16(std::uint16_t(file.settings.size()));
        info.insert(info.end(), file.settings.begin(), file.settings.end());
        section(F::info, info);
        for (const auto& s : file.sections)
        {
            if (s.payload.size() > F::max_file || out.size() + s.payload.size() + section_header + 32 > F::max_file)
                return error = "GI file exceeds 256 MiB", false;
            section(s.tag, s.payload);
        }
        const std::uint64_t size = out.size() + 32;
        if (size > F::max_file)
            return error = "GI file exceeds 256 MiB", false;
        for (unsigned i = 0; i < 8; ++i)
            out[16 + i] = std::uint8_t(size >> (8 * i));
        const auto trailer = sha256(out);
        out.insert(out.end(), trailer.begin(), trailer.end());
        bytes = std::move(out);
        return true;
    }

    bool read_gi_file(std::span<const std::uint8_t> bytes, GiFile& output, std::string& error)
    {
        error.clear();
        auto fail = [&](std::string text)
        {
            error = "Invalid GI file: " + std::move(text);
            return false;
        };
        if (bytes.size() > F::max_file)
            return fail("larger than 256 MiB");
        if (bytes.size() < fixed_header + 32 || !std::equal(std::begin(magic), std::end(magic), bytes.begin()))
            return fail("not an .ob2gi file");
        const auto body = bytes.first(bytes.size() - 32);
        const auto trailer = sha256(body);
        In in(body); // Headers and sections never reach into the trailer.
        std::span<const std::uint8_t> skip;
        in.take(8, skip);
        std::uint32_t version = 0, endian = 0, algorithm = 0, sections = 0;
        std::uint64_t size = 0;
        in.uint(version);
        in.uint(endian);
        in.uint(size);
        if (version != F::version)
            return fail("unsupported version " + std::to_string(version));
        if (endian != F::endian_marker)
            return fail("wrong byte order marker");
        if (size != bytes.size())
            return fail("declared size " + std::to_string(size) + " differs from " + std::to_string(bytes.size()));
        if (!std::equal(trailer.begin(), trailer.end(), bytes.end() - 32))
            return fail("SHA-256 trailer mismatch");
        GiFile file;
        in.digest(file.key.world);
        in.digest(file.key.albedo);
        in.digest(file.key.settings);
        in.digest(file.key.placement);
        in.uint(algorithm);
        file.key.algorithm = algorithm;
        std::uint8_t edition = 0, zero[3]{};
        std::uint16_t path_size = 0, zero16 = 0;
        in.uint(edition);
        for (auto& z : zero)
            in.uint(z);
        in.uint(path_size);
        in.uint(zero16);
        if (edition > std::uint8_t(GiEdition::Demo))
            return fail("unknown edition " + std::to_string(edition));
        if (zero[0] || zero[1] || zero[2] || zero16)
            return fail("nonzero reserved header bytes");
        file.key.edition = GiEdition(edition);
        std::span<const std::uint8_t> path;
        if (path_size > F::max_world_path || !in.take(path_size, path))
            return fail("world path too long or truncated");
        file.key.world_path.assign(path.begin(), path.end());
        if (!valid_world_path(file.key.world_path))
            return fail("world path is not a normalized .dat resource path");
        if (!in.uint(sections) || sections < 1 || sections > std::size(F::order))
            return fail("section count " + std::to_string(sections));
        int previous = -1;
        for (std::uint32_t i = 0; i < sections; ++i)
        {
            std::uint32_t tag = 0, flags = 0, checksum = 0;
            std::uint64_t length = 0;
            if (!in.uint(tag) || !in.uint(flags) || !in.uint(length) || !in.uint(checksum))
                return fail("truncated section header");
            const int index = order_index(tag);
            if (index < 0)
                return fail("unknown section " + gi_tag_name(tag));
            if (index <= previous || (i == 0) != (tag == F::info))
                return fail("section " + gi_tag_name(tag) + " duplicate or out of order");
            previous = index;
            if (flags)
                return fail("section " + gi_tag_name(tag) + " has nonzero flags");
            std::span<const std::uint8_t> payload;
            if (length > body.size() - in.at() || !in.take(std::size_t(length), payload))
                return fail("section " + gi_tag_name(tag) + " is truncated");
            if (crc(payload) != checksum)
                return fail("section " + gi_tag_name(tag) + " CRC-32 mismatch");
            if (tag == F::info)
            {
                if (payload.size() < 2)
                    return fail("INFO is truncated");
                const std::size_t n = std::size_t(payload[0]) | std::size_t(payload[1]) << 8;
                if (n > F::max_settings || payload.size() != 2 + n)
                    return fail("INFO settings length");
                file.settings.assign(payload.begin() + 2, payload.end());
                if (!valid_utf8(file.settings))
                    return fail("INFO settings are not UTF-8");
                if (sha256(text_bytes(file.settings)) != file.key.settings)
                    return fail("settings hash does not match INFO");
                continue;
            }
            file.sections.push_back({tag, {payload.begin(), payload.end()}});
        }
        if (in.at() != body.size())
            return fail(std::to_string(body.size() - in.at()) + " trailing bytes before the trailer");
        output = std::move(file);
        return true;
    }

    bool save_gi_file(const std::filesystem::path& path, const GiFile& file, std::string& error)
    {
        std::vector<std::uint8_t> bytes;
        return write_gi_file(file, bytes, error) && write_file_atomic(path, bytes, "GI file", error);
    }

    const char* gi_status_name(GiStatus status)
    {
        switch (status)
        {
        case GiStatus::Missing: return "missing";
        case GiStatus::Ready: return "ready";
        case GiStatus::Stale: return "stale";
        case GiStatus::Foreign: return "foreign";
        case GiStatus::Failed: return "failed";
        }
        return "unknown";
    }
    std::string gi_stale_text(unsigned reasons)
    {
        std::string out;
        static constexpr std::pair<unsigned, const char*> names[]{{gi_stale_albedo, "albedo"},
                                                                  {gi_stale_algorithm, "algorithm"},
                                                                  {gi_stale_edition, "edition"},
                                                                  {gi_stale_settings, "settings"},
                                                                  {gi_stale_placement, "placement"}};
        for (const auto& [bit, name] : names)
            if (reasons & bit)
                out += (out.empty() ? "" : ", ") + std::string(name);
        return out;
    }
    GiStatus gi_status(const GiKey& key, const GiExpectation& expected, unsigned& stale)
    {
        stale = 0;
        if (key.world != expected.world)
            return GiStatus::Foreign;
        if (key.albedo != expected.albedo)
            stale |= gi_stale_albedo;
        if (key.algorithm != expected.algorithm)
            stale |= gi_stale_algorithm;
        if (key.edition != expected.edition)
            stale |= gi_stale_edition;
        if (expected.settings && key.settings != *expected.settings)
            stale |= gi_stale_settings;
        if (expected.placement && key.placement != *expected.placement)
            stale |= gi_stale_placement;
        return stale ? GiStatus::Stale : GiStatus::Ready;
    }

    bool gi_albedo_key(const World& world, const Resources& resources, Sha256Digest& key, std::string& error)
    {
        std::set<std::string> textures;
        for (const auto& model : world.models)
            for (const auto& path : model.textures)
            {
                std::string normalized;
                if (path.empty())
                    continue;
                if (!resource_path(path, normalized))
                    return error = "Invalid world texture path: " + path, false;
                textures.insert(normalized);
            }
        // SPR textures animate DTX frames: key the frames too.
        std::set<std::string> frames;
        for (const auto& path : textures)
        {
            if (!path.ends_with(".spr") || !resources.find(path))
                continue;
            std::vector<std::uint8_t> bytes;
            Sprite sprite;
            std::string text;
            if (!resources.read(path, bytes, text) || !read_sprite(bytes, sprite, text))
                continue; // Keyed by its own entry; an unreadable SPR renders as a failed material.
            for (const auto& frame : sprite.frames)
            {
                std::string normalized;
                if (resource_path(frame, normalized))
                    frames.insert(normalized);
            }
        }
        textures.insert(frames.begin(), frames.end());
        Sha256 hash;
        std::vector<std::uint8_t> record;
        auto u32 = [&](std::uint32_t v)
        {
            for (unsigned s = 0; s < 32; s += 8)
                record.push_back(std::uint8_t(v >> s));
        };
        u32(std::uint32_t(textures.size()));
        for (const auto& path : textures)
        {
            u32(std::uint32_t(path.size()));
            record.insert(record.end(), path.begin(), path.end());
            const auto* resource = resources.find(path);
            const Mount* mount = nullptr;
            if (resource)
                for (const auto& m : resources.mounts())
                    if (m.path == resource->archive)
                        mount = &m;
            record.push_back(resource && mount ? 1 : 0);
            if (resource && mount)
            {
                record.insert(record.end(), mount->directory_digest.begin(), mount->directory_digest.end());
                u32(resource->offset);
                u32(resource->size);
            }
            hash.update(record);
            record.clear();
        }
        key = hash.finish();
        return true;
    }
    bool gi_expectation(const Resources& resources, std::string_view world_path, GiExpectation& expected,
                        std::string& error)
    {
        std::string path;
        if (!resource_path(world_path, path) || !resources.find(path))
            return error = "World " + std::string(world_path) + " is not in the mounted resources", false;
        std::vector<std::uint8_t> bytes;
        World world;
        if (!resources.read(path, bytes, error) || !read_world(bytes, world, error))
            return false;
        GiExpectation result;
        result.world = sha256(bytes);
        result.edition = gi_edition(resources);
        if (!gi_albedo_key(world, resources, result.albedo, error))
            return false;
        expected = std::move(result);
        return true;
    }

    bool gi_file_name(std::string_view world_path, const Sha256Digest& world, std::string& name)
    {
        std::string path;
        if (!resource_path(world_path, path) || !path.ends_with(".dat") || path.size() > F::max_world_path)
            return false;
        name = path.substr(0, path.size() - 4) + "." + digest_text(world).substr(0, 12) + ".ob2gi";
        return true;
    }

    std::vector<GiRoot> gi_search_roots(const std::filesystem::path& configured)
    {
        std::vector<GiRoot> roots;
        if (!configured.empty())
            roots.push_back({"--gi-dir", path_text(configured)});
        std::filesystem::path user;
        std::string error;
        if (user_directory(user, error))
            roots.push_back({"user", path_text(user / "gi")});
#if defined(SDL_PLATFORM_ANDROID)
        roots.push_back({"sdcard", "/sdcard/openblood2/gi"});
#endif
        if (const char* base = SDL_GetBasePath())
            roots.push_back({"packaged", std::string(base) + "gi"});
        return roots;
    }
    bool read_gi_bytes(const std::string& path, std::vector<std::uint8_t>& bytes, std::string& error)
    {
        error.clear();
        SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
        if (!io)
            return false; // Absent (or not openable): a missing candidate.
        const Sint64 size = SDL_GetIOSize(io);
        bool ok = size >= 0 && std::uint64_t(size) <= F::max_file;
        if (!ok)
            error = "GI file size unavailable or larger than 256 MiB: " + path;
        else
        {
            bytes.resize(std::size_t(size));
            ok = SDL_ReadIO(io, bytes.data(), bytes.size()) == bytes.size();
            if (!ok)
                error = "Cannot read GI file " + path + ": " + SDL_GetError();
        }
        SDL_CloseIO(io);
        if (!ok)
            bytes.clear();
        return ok;
    }
    bool find_gi_file(std::span<const GiRoot> roots, std::string_view world_path, const GiExpectation& expected,
                      GiLookup& lookup, std::string& error)
    {
        lookup = {};
        error.clear();
        std::string name;
        if (!gi_file_name(world_path, expected.world, name))
            return error = "Invalid GI world path: " + std::string(world_path), false;
        std::string normalized;
        resource_path(world_path, normalized);
        for (const auto& root : roots)
        {
            GiCandidate candidate;
            candidate.source = root.source;
            candidate.path = root.path + "/" + name;
            std::vector<std::uint8_t> bytes;
            GiFile file;
            if (!read_gi_bytes(candidate.path, bytes, candidate.detail))
                candidate.status = candidate.detail.empty() ? GiStatus::Missing : GiStatus::Failed;
            else if (!read_gi_file(bytes, file, candidate.detail))
                candidate.status = GiStatus::Failed;
            else if (file.key.world_path != normalized)
            {
                candidate.status = GiStatus::Foreign;
                candidate.detail = "baked for " + file.key.world_path;
            }
            else
                candidate.status = gi_status(file.key, expected, candidate.stale);
            if (candidate.status == GiStatus::Ready && !lookup.file)
            {
                lookup.chosen = int(lookup.candidates.size());
                lookup.file = std::move(file);
            }
            lookup.candidates.push_back(std::move(candidate));
        }
        return true;
    }
} // namespace OpenBlood2
