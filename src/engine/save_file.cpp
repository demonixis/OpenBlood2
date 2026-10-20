#include "engine/save_file.h"
#include "engine/atomic_file.h"
#include "game/game_settings.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <set>

namespace OpenBlood2
{
    namespace
    {
        namespace F = save_format;
        constexpr std::array<std::uint32_t, 9> order{F::meta,  F::fingerprint, F::console,     F::clock, F::objects,
                                                     F::links, F::attachments, F::level_entry, F::end};
        bool fail(SaveError& error, SaveFault fault, std::string text)
        {
            if (error.fault == SaveFault::None)
                error = {fault, std::move(text)};
            return false;
        }
        std::string tag_name(std::uint32_t tag)
        {
            std::string out;
            for (unsigned shift = 0; shift < 32; shift += 8)
            {
                const auto c = static_cast<char>((tag >> shift) & 255);
                if (c < 32 || c > 126)
                {
                    static constexpr char hex[] = "0123456789abcdef";
                    out = "0x";
                    for (int nibble = 7; nibble >= 0; --nibble)
                        out.push_back(hex[(tag >> (nibble * 4)) & 15]);
                    return out;
                }
                out.push_back(c);
            }
            while (!out.empty() && out.back() == ' ')
                out.pop_back();
            return out;
        }
        std::size_t section_limit(std::uint32_t tag)
        {
            switch (tag)
            {
            case F::meta:
                return F::max_meta;
            case F::fingerprint:
                return F::max_fingerprint;
            case F::console:
                return F::max_console_payload;
            case F::clock:
                return F::max_clock;
            case F::objects:
                return std::size_t(F::max_file);
            case F::links:
                return F::max_links;
            case F::attachments:
                return F::max_attachments;
            case F::level_entry:
                return std::size_t(F::max_level_entry);
            default:
                return 0; // END is empty.
            }
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
            void f64(double v)
            {
                u64(std::bit_cast<std::uint64_t>(v));
            }
            void raw(std::span<const std::uint8_t> v)
            {
                bytes_.insert(bytes_.end(), v.begin(), v.end());
            }
            void text(const std::string& v)
            {
                u32(std::uint32_t(v.size()));
                raw({reinterpret_cast<const std::uint8_t*>(v.data()), v.size()});
            }

          private:
            std::vector<std::uint8_t>& bytes_;
        };

        // Reads one section payload. The first failure latches; later reads return zero values.
        class In
        {
          public:
            In(std::span<const std::uint8_t> bytes, std::string section, SaveError& error)
                : bytes_(bytes), section_(std::move(section)), error_(error)
            {
            }
            bool good() const
            {
                return good_;
            }
            bool bad(SaveFault fault, const std::string& text)
            {
                if (good_)
                    fail(error_, fault, section_ + ": " + text);
                good_ = false;
                return false;
            }
            std::span<const std::uint8_t> take(std::size_t n)
            {
                if (!good_)
                    return {};
                if (n > bytes_.size() - pos_)
                {
                    bad(SaveFault::Truncated, "payload ends early");
                    return {};
                }
                auto out = bytes_.subspan(pos_, n);
                pos_ += n;
                return out;
            }
            std::uint8_t u8()
            {
                auto b = take(1);
                return b.empty() ? 0 : b[0];
            }
            std::uint32_t u32()
            {
                auto b = take(4);
                std::uint32_t v = 0;
                for (unsigned i = 0; i < b.size(); ++i)
                    v |= std::uint32_t(b[i]) << (8 * i);
                return v;
            }
            std::uint64_t u64()
            {
                auto b = take(8);
                std::uint64_t v = 0;
                for (unsigned i = 0; i < b.size(); ++i)
                    v |= std::uint64_t(b[i]) << (8 * i);
                return v;
            }
            double f64()
            {
                return std::bit_cast<double>(u64());
            }
            Sha256Digest digest()
            {
                Sha256Digest out{};
                auto b = take(out.size());
                std::copy(b.begin(), b.end(), out.begin());
                return out;
            }
            // The length is checked against the field bound and the remaining bytes before allocating.
            std::string text(std::size_t max, const char* what)
            {
                const auto n = u32();
                if (!good_)
                    return {};
                if (n > max)
                {
                    bad(SaveFault::Bound, std::string(what) + " exceeds " + std::to_string(max) + " bytes");
                    return {};
                }
                auto b = take(n);
                std::string out(b.begin(), b.end());
                if (out.find('\0') != std::string::npos)
                {
                    bad(SaveFault::Format, std::string(what) + " contains NUL");
                    return {};
                }
                return out;
            }
            std::uint32_t count(std::size_t max, const char* what)
            {
                const auto n = u32();
                if (good_ && n > max)
                    bad(SaveFault::Bound, std::string(what) + " count " + std::to_string(n) + " exceeds " +
                                              std::to_string(max));
                return good_ ? n : 0;
            }
            bool finish()
            {
                if (good_ && pos_ != bytes_.size())
                    bad(SaveFault::Format, std::to_string(bytes_.size() - pos_) + " unread payload bytes");
                return good_;
            }

          private:
            std::span<const std::uint8_t> bytes_;
            std::size_t pos_ = 0;
            std::string section_;
            SaveError& error_;
            bool good_ = true;
        };

        bool valid_text(const std::string& value, std::size_t max)
        {
            return value.size() <= max && value.find('\0') == std::string::npos;
        }
        bool normalized(const std::string& value, std::size_t max, bool single_name)
        {
            std::string key;
            return value.size() <= max && resource_path(value, key) && key == value &&
                   (!single_name || key.find('/') == std::string::npos);
        }
        bool validate(const SaveMeta& m, SaveError& error)
        {
            const auto bad = [&](const std::string& text) { return fail(error, SaveFault::Format, "META: " + text); };
            if (m.kind != SaveKind::Slot && m.kind != SaveKind::Quick && m.kind != SaveKind::LevelEntry)
                return bad("unknown save kind");
            if ((m.kind == SaveKind::Slot && m.slot > 7) || (m.kind != SaveKind::Slot && m.slot != 0))
                return bad("invalid slot");
            if (m.display_name.empty() || !valid_text(m.display_name, F::max_display_name))
                return bad("invalid display name");
            if (!normalized(m.world_path, F::max_world_path, false))
                return bad("world path is not a normalized resource path");
            if (!valid_text(m.world_title, F::max_world_title))
                return bad("invalid world title");
            if (m.game_type != GAMETYPE_SINGLE && m.game_type != GAMETYPE_ACTION && m.game_type != GAMETYPE_CUSTOM)
                return bad("game type " + std::to_string(m.game_type) + " is not single-player");
            if (m.character > 3 || m.difficulty > DIFFICULTY_INSANE)
                return bad("invalid character or difficulty");
            if (m.version.empty() || !valid_text(m.version, F::max_version_text))
                return bad("invalid version text");
            if (!std::isfinite(m.simulated_seconds) || m.simulated_seconds < 0)
                return bad("simulated seconds must be finite and nonnegative");
            return true;
        }
        bool validate(const SaveFingerprint& f, SaveError& error)
        {
            const auto bad = [&](SaveFault fault, const std::string& text)
            { return fail(error, fault, "FPRT: " + text); };
            if (f.edition != SaveEdition::Retail && f.edition != SaveEdition::Demo)
                return bad(SaveFault::Format, "unknown edition");
            if (f.mounts.empty() || f.mounts.size() > F::max_mounts)
                return bad(SaveFault::Bound, "mount count must be 1.." + std::to_string(F::max_mounts));
            for (const auto& mount : f.mounts)
                if ((mount.role != MountRole::Base && mount.role != MountRole::Mod) ||
                    !normalized(mount.name, F::max_archive_name, true))
                    return bad(SaveFault::Format, "invalid archive role or name");
            if (f.classes.size() > F::max_classes)
                return bad(SaveFault::Bound, "class count exceeds " + std::to_string(F::max_classes));
            for (std::size_t i = 0; i < f.classes.size(); ++i)
            {
                const auto& c = f.classes[i];
                if (c.name.empty() || !valid_text(c.name, F::max_class_name) || c.payload_version == 0)
                    return bad(SaveFault::Format, "invalid class name or payload version");
                if (i && !(f.classes[i - 1].name < c.name))
                    return bad(SaveFault::Format, "class schema is not strictly ascending");
            }
            if (f.class_schema != class_schema_digest(f.classes))
                return bad(SaveFault::Format, "class schema digest does not match its classes");
            return true;
        }
        bool validate(const std::vector<ConsoleVariable>& console, SaveError& error)
        {
            if (console.size() > F::max_console)
                return fail(error, SaveFault::Bound, "CONS: more than " + std::to_string(F::max_console) + " pairs");
            std::set<std::string_view> names;
            for (const auto& v : console)
                if (v.name.empty() || !valid_text(v.name, F::max_console_name) ||
                    !valid_text(v.value, F::max_console_value) || !names.insert(v.name).second)
                    return fail(error, SaveFault::Format, "CONS: empty, oversized or duplicate variable");
            return true;
        }

        void encode(const SaveMeta& m, Out& out)
        {
            out.u8(std::uint8_t(m.kind));
            out.u8(m.slot);
            out.text(m.display_name);
            out.u64(m.utc_seconds);
            out.text(m.world_path);
            out.text(m.world_title);
            out.u32(m.game_type);
            out.u32(m.character);
            out.u32(m.difficulty);
            out.text(m.version);
            out.f64(m.simulated_seconds);
        }
        void encode(const SaveFingerprint& f, Out& out)
        {
            out.u8(std::uint8_t(f.edition));
            out.u32(std::uint32_t(f.mounts.size()));
            for (const auto& mount : f.mounts)
            {
                out.u8(std::uint8_t(mount.role));
                out.text(mount.name);
                out.u64(mount.size);
                out.raw(mount.directory);
            }
            out.raw(f.index);
            out.raw(f.world);
            out.u32(std::uint32_t(f.classes.size()));
            for (const auto& c : f.classes)
            {
                out.text(c.name);
                out.u32(c.payload_version);
            }
            out.raw(f.class_schema);
        }
        void encode(const std::vector<ConsoleVariable>& console, Out& out)
        {
            out.u32(std::uint32_t(console.size()));
            for (const auto& v : console)
            {
                out.text(v.name);
                out.text(v.value);
            }
        }

        bool decode(In& in, SaveMeta& m, SaveError& error)
        {
            const auto kind = in.u8();
            m.kind = static_cast<SaveKind>(kind);
            m.slot = in.u8();
            m.display_name = in.text(F::max_display_name, "display name");
            m.utc_seconds = in.u64();
            m.world_path = in.text(F::max_world_path, "world path");
            m.world_title = in.text(F::max_world_title, "world title");
            m.game_type = in.u32();
            m.character = in.u32();
            m.difficulty = in.u32();
            m.version = in.text(F::max_version_text, "version text");
            m.simulated_seconds = in.f64();
            return in.finish() && validate(m, error);
        }
        bool decode(In& in, SaveFingerprint& f, SaveError& error)
        {
            f.edition = static_cast<SaveEdition>(in.u8());
            const auto mounts = in.count(F::max_mounts, "mount");
            for (std::uint32_t i = 0; in.good() && i < mounts; ++i)
            {
                SaveMount mount;
                mount.role = static_cast<MountRole>(in.u8());
                mount.name = in.text(F::max_archive_name, "archive name");
                mount.size = in.u64();
                mount.directory = in.digest();
                f.mounts.push_back(std::move(mount));
            }
            f.index = in.digest();
            f.world = in.digest();
            const auto classes = in.count(F::max_classes, "class");
            for (std::uint32_t i = 0; in.good() && i < classes; ++i)
            {
                SaveClass c;
                c.name = in.text(F::max_class_name, "class name");
                c.payload_version = in.u32();
                f.classes.push_back(std::move(c));
            }
            f.class_schema = in.digest();
            return in.finish() && validate(f, error);
        }
        bool decode(In& in, std::vector<ConsoleVariable>& console, SaveError& error)
        {
            const auto count = in.count(F::max_console, "console variable");
            for (std::uint32_t i = 0; in.good() && i < count; ++i)
            {
                ConsoleVariable v;
                v.name = in.text(F::max_console_name, "console name");
                v.value = in.text(F::max_console_value, "console value");
                console.push_back(std::move(v));
            }
            return in.finish() && validate(console, error);
        }

        bool version_ok(std::uint32_t version, std::uint32_t supported, const std::string& what, SaveError& error)
        {
            if (version == supported)
                return true;
            return fail(error, SaveFault::Version,
                        what + " version " + std::to_string(version) +
                            (version > supported ? " is newer than this build (reads " : " is unsupported (reads ") +
                            std::to_string(supported) + ")");
        }
        struct Header
        {
            std::uint32_t sections = 0;
        };
        // Checks magic, container version, section count and the declared total length.
        bool read_header(std::span<const std::uint8_t> bytes, std::uint64_t file_size, std::uint64_t limit,
                         Header& header, SaveError& error)
        {
            if (file_size > limit)
                return fail(error, SaveFault::Bound,
                            "save exceeds the " + std::to_string(limit >> 20) + " MiB limit");
            if (bytes.size() < F::header_size)
                return fail(error, SaveFault::Truncated, "save header is truncated");
            if (!std::equal(F::magic.begin(), F::magic.end(), bytes.begin()))
                return fail(error, SaveFault::Format, "not an OpenBlood2 save");
            In in(bytes.subspan(F::magic.size(), F::header_size - F::magic.size()), "header", error);
            const auto version = in.u32();
            header.sections = in.u32();
            const auto total = in.u64();
            if (!version_ok(version, F::container_version, "Container", error))
                return false;
            if (header.sections != order.size() && header.sections != order.size() - 1)
                return fail(error, SaveFault::Order, "invalid section count " + std::to_string(header.sections));
            if (total != file_size)
                return fail(error, total > file_size ? SaveFault::Truncated : SaveFault::Format,
                            "file is " + std::to_string(file_size) + " bytes, header declares " +
                                std::to_string(total));
            return true;
        }
        struct Section
        {
            std::uint32_t tag = 0;
            std::span<const std::uint8_t> payload;
        };
        // Reads the section at `pos`, checking its position, bound, CRC and version in that order.
        bool read_section(std::span<const std::uint8_t> bytes, std::size_t& pos, std::uint32_t expected,
                          Section& section, SaveError& error)
        {
            if (bytes.size() - pos < 16)
                return fail(error, SaveFault::Truncated, "section header is truncated");
            In in(bytes.subspan(pos, 16), "section header", error);
            section.tag = in.u32();
            const auto version = in.u32();
            const auto length = in.u64();
            if (std::find(order.begin(), order.end(), section.tag) == order.end())
                return fail(error, SaveFault::Order, "unknown section " + tag_name(section.tag));
            if (section.tag != expected)
                return fail(error, SaveFault::Order,
                            "section " + tag_name(section.tag) + " where " + tag_name(expected) + " is required");
            const auto name = tag_name(section.tag);
            if (length > section_limit(section.tag))
                return fail(error, SaveFault::Bound,
                            name + " payload exceeds " + std::to_string(section_limit(section.tag)) + " bytes");
            if (length + 4 > bytes.size() - pos - 16)
                return fail(error, SaveFault::Truncated, name + " section is truncated");
            const auto covered = bytes.subspan(pos, 16 + std::size_t(length));
            In tail(bytes.subspan(pos + covered.size(), 4), name, error);
            if (tail.u32() != crc(covered))
                return fail(error, SaveFault::Crc, name + " checksum mismatch");
            if (!version_ok(version, F::section_version, name + " section", error))
                return false;
            section.payload = covered.subspan(16);
            pos += covered.size() + 4;
            return true;
        }
        bool decode_container(std::span<const std::uint8_t> bytes, bool nested, SaveContainer& out, SaveError& error)
        {
            Header header;
            if (!read_header(bytes, bytes.size(), nested ? F::max_level_entry : F::max_file, header, error))
                return false;
            const bool has_auto = header.sections == order.size();
            if (nested && has_auto)
                return fail(error, SaveFault::Order, "the level-entry snapshot carries its own AUTO");
            SaveContainer result;
            std::size_t pos = F::header_size;
            for (const auto expected : order)
            {
                if (expected == F::level_entry && !has_auto)
                    continue;
                Section section;
                if (!read_section(bytes, pos, expected, section, error))
                    return false;
                In in(section.payload, tag_name(expected), error);
                bool ok = true;
                switch (expected)
                {
                case F::meta:
                    ok = decode(in, result.meta, error);
                    break;
                case F::fingerprint:
                    ok = decode(in, result.fingerprint, error);
                    break;
                case F::console:
                    ok = decode(in, result.console, error);
                    break;
                case F::clock:
                    result.clock.assign(section.payload.begin(), section.payload.end());
                    break;
                case F::objects:
                    result.objects.assign(section.payload.begin(), section.payload.end());
                    break;
                case F::links:
                    result.links.assign(section.payload.begin(), section.payload.end());
                    break;
                case F::attachments:
                    result.attachments.assign(section.payload.begin(), section.payload.end());
                    break;
                case F::level_entry:
                {
                    SaveContainer inner;
                    if (!decode_container(section.payload, true, inner, error))
                    {
                        error.text = "AUTO: " + error.text;
                        return false;
                    }
                    result.level_entry.emplace(section.payload.begin(), section.payload.end());
                    break;
                }
                default:
                    break; // END payload length is bounded to zero.
                }
                if (!ok)
                    return false;
            }
            if (pos != bytes.size())
                return fail(error, SaveFault::Format, "trailing bytes after END");
            if (nested && result.meta.kind != SaveKind::LevelEntry)
                return fail(error, SaveFault::Format, "AUTO does not hold a level-entry snapshot");
            if (!nested && result.meta.kind == SaveKind::LevelEntry && has_auto)
                return fail(error, SaveFault::Order, "a level-entry snapshot cannot carry AUTO");
            out = std::move(result);
            return true;
        }
        void write_section(Out& file, std::vector<std::uint8_t>& bytes, std::uint32_t tag,
                           std::span<const std::uint8_t> payload)
        {
            const auto start = bytes.size();
            file.u32(tag);
            file.u32(F::section_version);
            file.u64(payload.size());
            file.raw(payload);
            file.u32(crc(std::span(bytes).subspan(start)));
        }
    } // namespace

    const char* save_fault_name(SaveFault fault)
    {
        switch (fault)
        {
        case SaveFault::None:
            return "none";
        case SaveFault::Io:
            return "file error";
        case SaveFault::Bound:
            return "bound";
        case SaveFault::Truncated:
            return "truncated";
        case SaveFault::Format:
            return "format";
        case SaveFault::Version:
            return "version";
        case SaveFault::Crc:
            return "CRC";
        case SaveFault::Order:
            return "section order";
        case SaveFault::Fingerprint:
            return "fingerprint";
        }
        return "unknown";
    }
    Sha256Digest class_schema_digest(std::span<const SaveClass> classes)
    {
        std::vector<std::uint8_t> bytes;
        Out out(bytes);
        out.u32(std::uint32_t(classes.size()));
        for (const auto& c : classes)
        {
            out.text(c.name);
            out.u32(c.payload_version);
        }
        return sha256(bytes);
    }
    bool encode_save(const SaveContainer& save, std::vector<std::uint8_t>& output, SaveError& error)
    {
        error = {};
        if (!validate(save.meta, error) || !validate(save.fingerprint, error) || !validate(save.console, error))
            return false;
        const std::array<std::pair<std::uint32_t, std::size_t>, 4> opaque{
            {{F::clock, save.clock.size()},
             {F::objects, save.objects.size()},
             {F::links, save.links.size()},
             {F::attachments, save.attachments.size()}}};
        for (const auto& [tag, size] : opaque)
            if (size > section_limit(tag))
                return fail(error, SaveFault::Bound, tag_name(tag) + " payload exceeds its bound");
        if (save.level_entry)
        {
            SaveContainer inner;
            if (save.meta.kind == SaveKind::LevelEntry)
                return fail(error, SaveFault::Order, "a level-entry snapshot cannot carry AUTO");
            if (!decode_container(*save.level_entry, true, inner, error))
            {
                error.text = "AUTO: " + error.text;
                return false;
            }
        }
        std::vector<std::uint8_t> meta, fingerprint, console;
        Out meta_out(meta), fingerprint_out(fingerprint), console_out(console);
        encode(save.meta, meta_out);
        encode(save.fingerprint, fingerprint_out);
        encode(save.console, console_out);
        std::vector<std::uint8_t> bytes;
        Out file(bytes);
        file.raw(F::magic);
        file.u32(F::container_version);
        file.u32(std::uint32_t(save.level_entry ? order.size() : order.size() - 1));
        file.u64(0); // Patched below.
        write_section(file, bytes, F::meta, meta);
        write_section(file, bytes, F::fingerprint, fingerprint);
        write_section(file, bytes, F::console, console);
        write_section(file, bytes, F::clock, save.clock);
        write_section(file, bytes, F::objects, save.objects);
        write_section(file, bytes, F::links, save.links);
        write_section(file, bytes, F::attachments, save.attachments);
        if (save.level_entry)
            write_section(file, bytes, F::level_entry, *save.level_entry);
        write_section(file, bytes, F::end, {});
        if (bytes.size() > F::max_file)
            return fail(error, SaveFault::Bound, "save exceeds the 64 MiB limit");
        for (unsigned i = 0; i < 8; ++i)
            bytes[16 + i] = std::uint8_t(std::uint64_t(bytes.size()) >> (8 * i));
        output = std::move(bytes);
        return true;
    }
    bool decode_save(std::span<const std::uint8_t> bytes, SaveContainer& output, SaveError& error)
    {
        error = {};
        return decode_container(bytes, false, output, error);
    }
    bool read_save_meta(std::span<const std::uint8_t> prefix, std::uint64_t file_size, SaveMeta& output,
                        SaveError& error)
    {
        error = {};
        Header header;
        if (prefix.size() > file_size)
            return fail(error, SaveFault::Format, "prefix is longer than the file");
        if (!read_header(prefix, file_size, F::max_file, header, error))
            return false;
        std::size_t pos = F::header_size;
        Section section;
        if (!read_section(prefix, pos, F::meta, section, error))
            return false;
        In in(section.payload, "META", error);
        SaveMeta meta;
        if (!decode(in, meta, error))
            return false;
        output = std::move(meta);
        return true;
    }
    bool read_save_meta_file(const std::filesystem::path& path, SaveMeta& output, SaveError& error)
    {
        error = {};
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec)
            return fail(error, SaveFault::Io, "Cannot inspect " + path_text(path.filename()));
        // Only the header and the bounded META section are read.
        const auto prefix = std::min<std::uint64_t>(size, F::header_size + F::section_overhead + F::max_meta);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(prefix));
        std::ifstream input(path, std::ios::binary);
        if (!input || (prefix && !input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(prefix))))
            return fail(error, SaveFault::Io, "Cannot read " + path_text(path.filename()));
        return read_save_meta(bytes, size, output, error);
    }
    bool read_save_file(const std::filesystem::path& path, SaveContainer& output, SaveError& error)
    {
        error = {};
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (!ec && size > F::max_file)
            return fail(error, SaveFault::Bound, "save exceeds the 64 MiB limit");
        std::vector<std::uint8_t> bytes;
        std::string text;
        if (ec || !read_file_bounded(path, F::max_file, bytes, text))
            return fail(error, SaveFault::Io, text.empty() ? "Cannot inspect " + path_text(path.filename()) : text);
        return decode_save(bytes, output, error);
    }
    bool write_save_file(const std::filesystem::path& path, const SaveContainer& save, SaveError& error)
    {
        std::vector<std::uint8_t> bytes;
        if (!encode_save(save, bytes, error))
            return false;
        std::string text;
        if (!write_file_atomic(path, bytes, "save", text, {.remove_stale_temporaries = true}))
            return fail(error, SaveFault::Io, text);
        return true;
    }
} // namespace OpenBlood2
