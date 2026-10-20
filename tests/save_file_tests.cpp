#include "engine/atomic_file.h"
#include "engine/save_fingerprint.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <bit>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <random>

using namespace OpenBlood2;
using Bytes = std::vector<std::uint8_t>;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    namespace F = save_format;
    std::filesystem::path root;
    // Independent little-endian encoder for golden layouts; it does not share code with the codec.
    struct Le
    {
        Bytes data;
        Le& u8(unsigned v)
        {
            data.push_back(std::uint8_t(v));
            return *this;
        }
        Le& u32(std::uint32_t v)
        {
            for (int i = 0; i < 4; ++i)
                u8(v >> (8 * i));
            return *this;
        }
        Le& u64(std::uint64_t v)
        {
            for (int i = 0; i < 8; ++i)
                u8(unsigned(v >> (8 * i)));
            return *this;
        }
        Le& f64(double v)
        {
            return u64(std::bit_cast<std::uint64_t>(v));
        }
        Le& raw(std::span<const std::uint8_t> v)
        {
            data.insert(data.end(), v.begin(), v.end());
            return *this;
        }
        Le& ascii(std::string_view v)
        {
            data.insert(data.end(), v.begin(), v.end());
            return *this;
        }
        Le& str(std::string_view v)
        {
            u32(std::uint32_t(v.size()));
            return ascii(v);
        }
    };
    std::uint32_t le32(const Bytes& b, std::size_t at)
    {
        return b[at] | b[at + 1] << 8 | b[at + 2] << 16 | std::uint32_t(b[at + 3]) << 24;
    }
    void put32(Bytes& b, std::size_t at, std::uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            b[at + i] = std::uint8_t(v >> (8 * i));
    }
    // Section boundaries of a valid file: offset of each section header.
    std::vector<std::size_t> sections(const Bytes& b)
    {
        std::vector<std::size_t> out;
        for (std::size_t pos = F::header_size; pos + 16 <= b.size();)
        {
            out.push_back(pos);
            std::uint64_t length = le32(b, pos + 8) | std::uint64_t(le32(b, pos + 12)) << 32;
            pos += 16 + std::size_t(length) + 4;
        }
        return out;
    }
    // Recomputes the CRC of the section at `at` after a deliberate payload edit.
    void recrc(Bytes& b, std::size_t at)
    {
        const std::size_t length = le32(b, at + 8);
        put32(b, at + 16 + length, SDL_crc32(0, b.data() + at, 16 + length));
    }
    SaveMeta sample_meta(SaveKind kind = SaveKind::Slot, std::uint8_t slot = 3)
    {
        SaveMeta m;
        m.kind = kind;
        m.slot = kind == SaveKind::Slot ? slot : 0;
        m.display_name = "Subway [Oct 20 12:34:56]";
        m.utc_seconds = 1792500000;
        m.world_path = "worlds/01_subwaya.dat";
        m.world_title = "Subway";
        m.game_type = 0;
        m.character = 1;
        m.difficulty = 2;
        m.version = "0.3.0";
        m.simulated_seconds = 123.5;
        return m;
    }
    SaveFingerprint sample_fingerprint()
    {
        SaveFingerprint f;
        f.mounts.push_back({MountRole::Base, "blood2.rez", 1000, {}});
        f.mounts.push_back({MountRole::Mod, "mod.rez", 20, {}});
        f.mounts[0].directory.fill(0x11);
        f.mounts[1].directory.fill(0x22);
        f.index.fill(0x33);
        f.world.fill(0x44);
        f.classes = {{"CPlayerObj", 1}, {"Door", 2}};
        f.class_schema = class_schema_digest(f.classes);
        return f;
    }
    SaveContainer sample(bool with_auto)
    {
        SaveContainer s;
        s.meta = sample_meta();
        s.fingerprint = sample_fingerprint();
        s.console = {{"Difficulty", "2"}, {"GameType", "0"}, {"Empty", ""}};
        s.clock = {1, 2, 3};
        s.objects = {4, 5, 6, 7};
        s.links = {8};
        s.attachments = {};
        if (with_auto)
        {
            auto entry = sample(false);
            entry.meta = sample_meta(SaveKind::LevelEntry);
            Bytes bytes;
            SaveError error;
            encode_save(entry, bytes, error);
            s.level_entry = bytes;
        }
        return s;
    }
    Bytes encoded(const SaveContainer& s)
    {
        Bytes out;
        SaveError error;
        if (!encode_save(s, out, error))
            std::cerr << "encode failed: " << error.text << '\n';
        return out;
    }
    SaveFault decode_fault(const Bytes& b)
    {
        SaveContainer out;
        SaveError error;
        return decode_save(b, out, error) ? SaveFault::None : error.fault;
    }

    bool digests()
    {
        const auto hex = [](std::string_view text)
        { return digest_text(sha256({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()})); };
        CHECK(hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        CHECK(hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        CHECK(hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
        Bytes million(1000000, 'a');
        CHECK(digest_text(sha256(million)) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
        // Arbitrary update splits match the one-shot digest.
        std::minstd_rand random(7);
        for (int round = 0; round < 64; ++round)
        {
            Bytes data(random() % 300);
            for (auto& b : data)
                b = std::uint8_t(random());
            Sha256 hash;
            for (std::size_t pos = 0; pos < data.size();)
            {
                const auto n = std::min<std::size_t>(data.size() - pos, random() % 70);
                hash.update(std::span(data).subspan(pos, n));
                pos += n;
            }
            CHECK(hash.finish() == sha256(data));
        }
        // The section CRC is the standard reflected CRC-32 (check value of "123456789").
        CHECK(SDL_crc32(0, "123456789", 9) == 0xCBF43926u);
        return true;
    }
    bool golden()
    {
        SaveContainer s;
        s.meta = sample_meta(SaveKind::Quick);
        s.fingerprint.mounts.push_back({MountRole::Base, "blood2.rez", 0x0102030405ull, {}});
        s.fingerprint.class_schema = class_schema_digest({});
        const auto bytes = encoded(s);
        // META payload layout.
        Le meta;
        meta.u8(1).u8(0).str("Subway [Oct 20 12:34:56]").u64(1792500000).str("worlds/01_subwaya.dat").str("Subway");
        meta.u32(0).u32(1).u32(2).str("0.3.0").f64(123.5);
        Le fprt;
        fprt.u8(0).u32(1).u8(0).str("blood2.rez").u64(0x0102030405ull).raw(Sha256Digest{});
        fprt.raw(Sha256Digest{}).raw(Sha256Digest{}).u32(0).raw(class_schema_digest({}));
        Le empty_cons;
        empty_cons.u32(0);
        Le file;
        file.ascii("OB2SAVE").u8(0x1A).u32(1).u32(8).u64(0);
        const auto section = [&](std::string_view tag, const Bytes& payload)
        {
            const auto start = file.data.size();
            file.ascii(tag).u32(1).u64(payload.size()).raw(payload);
            file.u32(SDL_crc32(0, file.data.data() + start, file.data.size() - start));
        };
        section("META", meta.data);
        section("FPRT", fprt.data);
        section("CONS", empty_cons.data);
        section("CLCK", {});
        section("OBJS", {});
        section("LINK", {});
        section("ATCH", {});
        section("END ", {});
        for (int i = 0; i < 8; ++i)
            file.data[16 + i] = std::uint8_t(std::uint64_t(file.data.size()) >> (8 * i));
        CHECK(bytes == file.data);
        // Console pairs, opaque payloads and a nested AUTO keep the same framing.
        auto full = sample(true);
        const auto all = encoded(full);
        const auto at = sections(all);
        CHECK(at.size() == 9 && le32(all, 12) == 9);
        const std::array<const char*, 9> tags{"META", "FPRT", "CONS", "CLCK", "OBJS", "LINK", "ATCH", "AUTO", "END "};
        for (unsigned i = 0; i < at.size(); ++i)
            CHECK(std::memcmp(all.data() + at[i], tags[i], 4) == 0 && le32(all, at[i] + 4) == 1);
        Le cons;
        cons.u32(3).str("Difficulty").str("2").str("GameType").str("0").str("Empty").str("");
        CHECK(std::equal(cons.data.begin(), cons.data.end(), all.begin() + at[2] + 16) &&
              le32(all, at[2] + 8) == cons.data.size());
        CHECK(Bytes(all.begin() + at[7] + 16, all.begin() + at[7] + 16 + full.level_entry->size()) ==
              *full.level_entry);
        return true;
    }
    bool roundtrip()
    {
        for (const bool with_auto : {false, true})
        {
            const auto s = sample(with_auto);
            const auto bytes = encoded(s);
            CHECK(!bytes.empty());
            SaveContainer back;
            SaveError error;
            CHECK(decode_save(bytes, back, error) && back == s && error.fault == SaveFault::None);
            CHECK(encoded(back) == bytes);
            SaveMeta meta;
            CHECK(read_save_meta(bytes, bytes.size(), meta, error) && meta == s.meta);
            // The quick read needs only the header and META.
            const auto meta_end = sections(bytes)[1];
            CHECK(read_save_meta(std::span(bytes).first(meta_end), bytes.size(), meta, error) && meta == s.meta);
            CHECK(!read_save_meta(std::span(bytes).first(meta_end - 1), bytes.size(), meta, error) &&
                  error.fault == SaveFault::Truncated);
        }
        // Every save kind and boundary value.
        for (const auto kind : {SaveKind::Slot, SaveKind::Quick, SaveKind::LevelEntry})
        {
            auto s = sample(false);
            s.meta = sample_meta(kind, 7);
            s.meta.display_name = std::string(255, 'n');
            s.meta.world_title = "";
            s.meta.game_type = kind == SaveKind::Quick ? 8 : 9;
            s.meta.difficulty = 3;
            s.meta.character = 3;
            s.meta.simulated_seconds = 0;
            s.console.assign(256, {});
            for (unsigned i = 0; i < 256; ++i)
                s.console[i] = {std::string(60, 'a') + std::to_string(1000 + i), std::string(1024, 'v')};
            s.clock.assign(F::max_clock, 9);
            SaveContainer back;
            SaveError error;
            CHECK(decode_save(encoded(s), back, error) && back == s);
        }
        // A nested AUTO keeps its own bytes exactly.
        const auto s = sample(true);
        SaveContainer inner;
        SaveError error;
        CHECK(decode_save(*s.level_entry, inner, error) && inner.meta.kind == SaveKind::LevelEntry &&
              !inner.level_entry);
        return true;
    }
    bool truncation()
    {
        for (const bool with_auto : {false, true})
        {
            const auto bytes = encoded(sample(with_auto));
            for (std::size_t n = 0; n < bytes.size(); ++n)
            {
                Bytes cut(bytes.begin(), bytes.begin() + n);
                CHECK(decode_fault(cut) != SaveFault::None);
                // A truncated file keeps a header that declares the full length.
                if (n >= F::header_size)
                    CHECK(decode_fault(cut) == SaveFault::Truncated);
                SaveMeta meta;
                SaveError error;
                CHECK(!read_save_meta(cut, cut.size(), meta, error) && error.fault != SaveFault::None);
            }
            // Truncating the header's length field to match still fails on the missing section.
            auto cut = bytes;
            cut.resize(sections(bytes).back());
            for (int i = 0; i < 8; ++i)
                cut[16 + i] = std::uint8_t(std::uint64_t(cut.size()) >> (8 * i));
            CHECK(decode_fault(cut) == SaveFault::Truncated);
        }
        return true;
    }
    bool bitflips()
    {
        for (const bool with_auto : {false, true})
        {
            const auto bytes = encoded(sample(with_auto));
            std::size_t refused = 0;
            for (std::size_t i = 0; i < bytes.size(); ++i)
                for (int bit = 0; bit < 8; ++bit)
                {
                    auto flipped = bytes;
                    flipped[i] ^= std::uint8_t(1 << bit);
                    CHECK(decode_fault(flipped) != SaveFault::None);
                    ++refused;
                }
            CHECK(refused == bytes.size() * 8);
            std::cout << "bitflips: " << refused << " flips of a " << bytes.size() << "-byte file refused\n";
            // A flip inside a payload is reported as a CRC failure.
            auto flipped = bytes;
            flipped[sections(bytes)[0] + 20] ^= 4;
            CHECK(decode_fault(flipped) == SaveFault::Crc);
        }
        return true;
    }
    bool bounds()
    {
        const auto bytes = encoded(sample(false));
        const auto at = sections(bytes);
        // A declared payload above the section bound is refused before any read.
        for (unsigned i = 0; i < 8; ++i)
        {
            auto big = bytes;
            put32(big, at[i] + 8, 0xFFFFFFF0u);
            put32(big, at[i] + 12, 0x7FFFFFFFu);
            CHECK(decode_fault(big) == SaveFault::Bound);
        }
        auto end = bytes;
        put32(end, at[7] + 8, 1); // END must be empty.
        CHECK(decode_fault(end) == SaveFault::Bound);
        // A string length above its field bound is refused without allocating it.
        auto name = bytes;
        put32(name, at[0] + 16 + 2, 0xFFFFFFFFu);
        recrc(name, at[0]);
        CHECK(decode_fault(name) == SaveFault::Bound);
        put32(name, at[0] + 16 + 2, 256);
        recrc(name, at[0]);
        CHECK(decode_fault(name) == SaveFault::Bound);
        // A string within its bound but past the payload end is a truncated payload.
        put32(name, at[0] + 16 + 2, 200);
        recrc(name, at[0]);
        CHECK(decode_fault(name) == SaveFault::Truncated);
        // Count fields above their bounds.
        auto mounts = bytes;
        put32(mounts, at[1] + 16 + 1, 33);
        recrc(mounts, at[1]);
        CHECK(decode_fault(mounts) == SaveFault::Bound);
        auto console = bytes;
        put32(console, at[2] + 16, 257);
        recrc(console, at[2]);
        CHECK(decode_fault(console) == SaveFault::Bound);
        // An inexact payload (extra byte inside a CRC-valid section) is refused.
        SaveError error;
        {
            auto extra = bytes;
            const auto pos = at[0] + 16 + le32(bytes, at[0] + 8);
            extra.insert(extra.begin() + pos, 0);
            put32(extra, at[0] + 8, le32(bytes, at[0] + 8) + 1);
            recrc(extra, at[0]);
            for (int i = 0; i < 8; ++i)
                extra[16 + i] = std::uint8_t(std::uint64_t(extra.size()) >> (8 * i));
            CHECK(decode_fault(extra) == SaveFault::Format);
        }
        // Whole-file limits.
        Bytes huge(F::max_file + 1);
        std::copy(bytes.begin(), bytes.begin() + F::header_size, huge.begin());
        CHECK(decode_fault(huge) == SaveFault::Bound);
        SaveMeta meta;
        CHECK(!read_save_meta(bytes, F::max_file + 1, meta, error) && error.fault == SaveFault::Bound);
        // Encoding refuses every out-of-bound or invalid value and writes nothing.
        std::vector<std::function<void(SaveContainer&)>> invalid{
            [](auto& s) { s.meta.display_name = std::string(256, 'n'); },
            [](auto& s) { s.meta.display_name.clear(); },
            [](auto& s) { s.meta.display_name = std::string("a\0b", 3); },
            [](auto& s) { s.meta.world_path = "Worlds\\01_SubwayA.dat"; },
            [](auto& s) { s.meta.world_path = "../x.dat"; },
            [](auto& s) { s.meta.world_title = std::string(256, 't'); },
            [](auto& s) { s.meta.game_type = 2; },
            [](auto& s) { s.meta.character = 4; },
            [](auto& s) { s.meta.difficulty = 4; },
            [](auto& s) { s.meta.slot = 8; },
            [](auto& s) { s.meta.kind = SaveKind(3); },
            [](auto& s) { s.meta.version.clear(); },
            [](auto& s) { s.meta.simulated_seconds = -1; },
            [](auto& s) { s.meta.simulated_seconds = std::numeric_limits<double>::infinity(); },
            [](auto& s) { s.meta.simulated_seconds = std::numeric_limits<double>::quiet_NaN(); },
            [](auto& s) { s.fingerprint.mounts.clear(); },
            [](auto& s)
            {
                const auto mount = s.fingerprint.mounts[0];
                s.fingerprint.mounts.assign(33, mount);
            },
            [](auto& s) { s.fingerprint.mounts[0].name = "dir/blood2.rez"; },
            [](auto& s) { s.fingerprint.mounts[0].role = MountRole(2); },
            [](auto& s) { s.fingerprint.edition = SaveEdition(2); },
            [](auto& s) { std::swap(s.fingerprint.classes[0], s.fingerprint.classes[1]); },
            [](auto& s) { s.fingerprint.classes[1].name = s.fingerprint.classes[0].name; },
            [](auto& s) { s.fingerprint.classes[0].payload_version = 0; },
            [](auto& s) { s.fingerprint.classes[0].name = std::string(65, 'c'); },
            [](auto& s) { s.fingerprint.class_schema[0] ^= 1; },
            [](auto& s) { s.console.assign(257, {"x", ""}); },
            [](auto& s) { s.console.push_back({"Difficulty", "3"}); },
            [](auto& s) { s.console.push_back({"", "3"}); },
            [](auto& s) { s.console.push_back({std::string(65, 'k'), ""}); },
            [](auto& s) { s.console.push_back({"Long", std::string(1025, 'v')}); },
            [](auto& s) { s.clock.assign(F::max_clock + 1, 0); },
            [](auto& s) { s.links.assign(F::max_links + 1, 0); },
            [](auto& s) { s.level_entry = Bytes{1, 2, 3}; },
            [](auto& s) { s.level_entry = encoded(sample(true)); }, // AUTO inside AUTO.
            [](auto& s) { s.level_entry = encoded(sample(false)); }, // A slot, not a level entry.
            [](auto& s)
            {
                s.meta = sample_meta(SaveKind::LevelEntry);
                s.level_entry = sample(true).level_entry;
            },
        };
        for (std::size_t i = 0; i < invalid.size(); ++i)
        {
            auto s = sample(false);
            invalid[i](s);
            Bytes out{42};
            SaveError refused;
            if (encode_save(s, out, refused) || out != Bytes{42} || refused.fault == SaveFault::None ||
                refused.text.empty())
            {
                std::cerr << "invalid container " << i << " was accepted\n";
                return false;
            }
        }
        // The 64 MiB file bound applies to the sum of the sections.
        auto large = sample(false);
        large.objects.assign(F::max_file - 64, 0);
        Bytes out;
        CHECK(!encode_save(large, out, error) && error.fault == SaveFault::Bound && out.empty());
        return true;
    }
    bool versions()
    {
        const auto bytes = encoded(sample(true));
        const auto at = sections(bytes);
        auto newer = bytes;
        put32(newer, 8, 2);
        SaveContainer out;
        SaveError error;
        CHECK(!decode_save(newer, out, error) && error.fault == SaveFault::Version &&
              error.text.find("newer") != std::string::npos);
        put32(newer, 8, 0);
        CHECK(!decode_save(newer, out, error) && error.fault == SaveFault::Version &&
              error.text.find("unsupported") != std::string::npos);
        SaveMeta meta;
        put32(newer, 8, 2);
        CHECK(!read_save_meta(newer, newer.size(), meta, error) && error.fault == SaveFault::Version);
        for (const auto pos : at)
        {
            auto section = bytes;
            put32(section, pos + 4, 2);
            CHECK(decode_fault(section) == SaveFault::Crc); // The CRC covers the version field.
            recrc(section, pos);
            CHECK(!decode_save(section, out, error) && error.fault == SaveFault::Version &&
                  error.text.find("newer") != std::string::npos);
        }
        // A newer section version inside the nested AUTO is refused as well.
        auto nested = bytes;
        const auto inner = at[7] + 16;
        const auto inner_meta = inner + F::header_size;
        put32(nested, inner_meta + 4, 7);
        recrc(nested, inner_meta);
        recrc(nested, at[7]);
        CHECK(!decode_save(nested, out, error) && error.fault == SaveFault::Version &&
              error.text.starts_with("AUTO: "));
        return true;
    }
    bool order()
    {
        const auto plain = encoded(sample(false));
        const auto at = sections(plain);
        // Swap CONS and CLCK.
        {
            Bytes swapped(plain.begin(), plain.begin() + at[2]);
            swapped.insert(swapped.end(), plain.begin() + at[3], plain.begin() + at[4]);
            swapped.insert(swapped.end(), plain.begin() + at[2], plain.begin() + at[3]);
            swapped.insert(swapped.end(), plain.begin() + at[4], plain.end());
            CHECK(decode_fault(swapped) == SaveFault::Order);
        }
        // Unknown tag with a valid CRC.
        auto unknown = plain;
        std::memcpy(unknown.data() + at[3], "XTRA", 4);
        recrc(unknown, at[3]);
        SaveContainer out;
        SaveError error;
        CHECK(!decode_save(unknown, out, error) && error.fault == SaveFault::Order &&
              error.text.find("XTRA") != std::string::npos);
        // Duplicate META in place of FPRT.
        {
            Bytes duplicate(plain.begin(), plain.begin() + at[1]);
            duplicate.insert(duplicate.end(), plain.begin() + at[0], plain.begin() + at[1]);
            duplicate.insert(duplicate.end(), plain.begin() + at[2], plain.end());
            for (int i = 0; i < 8; ++i)
                duplicate[16 + i] = std::uint8_t(std::uint64_t(duplicate.size()) >> (8 * i));
            CHECK(decode_fault(duplicate) == SaveFault::Order);
        }
        // Missing END: a ninth section count with no AUTO.
        auto count = plain;
        put32(count, 12, 9);
        CHECK(decode_fault(count) == SaveFault::Order);
        put32(count, 12, 7);
        CHECK(decode_fault(count) == SaveFault::Order);
        put32(count, 12, 0);
        CHECK(decode_fault(count) == SaveFault::Order);
        // A trailing byte is refused whether or not the header length includes it.
        auto trailing = plain;
        trailing.push_back(0);
        CHECK(decode_fault(trailing) == SaveFault::Format);
        for (int i = 0; i < 8; ++i)
            trailing[16 + i] = std::uint8_t(std::uint64_t(trailing.size()) >> (8 * i));
        CHECK(decode_fault(trailing) == SaveFault::Format);
        // Bad magic.
        auto magic = plain;
        magic[0] = 'X';
        CHECK(decode_fault(magic) == SaveFault::Format);
        // An AUTO whose snapshot is not a level entry, or a level entry carrying AUTO.
        auto with_auto = sample(true);
        const auto bytes = encoded(with_auto);
        const auto auto_at = sections(bytes)[7];
        auto slot_inside = bytes;
        slot_inside[auto_at + 16 + F::header_size + 16] = std::uint8_t(SaveKind::Slot);
        recrc(slot_inside, auto_at + 16 + F::header_size);
        recrc(slot_inside, auto_at);
        CHECK(decode_fault(slot_inside) == SaveFault::Format);
        auto level = bytes;
        level[F::header_size + 16] = std::uint8_t(SaveKind::LevelEntry);
        level[F::header_size + 17] = 0;
        recrc(level, F::header_size);
        CHECK(decode_fault(level) == SaveFault::Order);
        return true;
    }

    // Minimal REZ v1 archive with files in its root directory.
    Bytes rez(const std::vector<std::pair<std::string, Bytes>>& files)
    {
        Le w;
        w.data.resize(168);
        w.data[0] = w.data[62] = w.data[124] = 13;
        w.data[1] = w.data[63] = w.data[125] = 10;
        w.data[126] = 26;
        put32(w.data, 127, 1);
        std::vector<std::uint32_t> offsets;
        for (const auto& [name, data] : files)
        {
            offsets.push_back(std::uint32_t(w.data.size()));
            w.raw(data);
        }
        const auto dir = w.data.size();
        for (std::size_t i = 0; i < files.size(); ++i)
        {
            const auto dot = files[i].first.find('.');
            const auto base = files[i].first.substr(0, dot);
            std::uint32_t extension = 0;
            for (const char c : files[i].first.substr(dot + 1))
                extension = extension << 8 | std::uint8_t(std::toupper(c));
            w.u32(0).u32(offsets[i]).u32(std::uint32_t(files[i].second.size())).u32(0);
            w.u32(i).u32(extension).u32(0).ascii(base).u8(0).u8(0);
        }
        put32(w.data, 131, std::uint32_t(dir));
        put32(w.data, 135, std::uint32_t(w.data.size() - dir));
        return w.data;
    }
    bool save(const std::filesystem::path& path, const Bytes& data)
    {
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
        return bool(f);
    }
    bool mount(Resources& r, std::initializer_list<std::pair<std::string, MountRole>> archives)
    {
        std::string error;
        for (const auto& [name, role] : archives)
            if (!r.mount(root / name, error, role))
            {
                std::cerr << error << '\n';
                return false;
            }
        return true;
    }
    bool fingerprint()
    {
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        const Bytes world{1, 2, 3, 4, 5}, other_world{1, 2, 3, 4, 6};
        const auto base = rez({{"test.dat", world}, {"other.dat", other_world}});
        CHECK(save(root / "BLOOD2.REZ", base));
        CHECK(save(root / "mod.rez", rez({{"test.dat", other_world}})));
        CHECK(save(root / "extra.rez", rez({{"skin.dtx", {9}}})));
        // The directory digest covers the header and the directory block.
        Resources r;
        CHECK(mount(r, {{"BLOOD2.REZ", MountRole::Base}}));
        Bytes directory_bytes(base.begin(), base.begin() + 168);
        directory_bytes.insert(directory_bytes.end(), base.begin() + le32(base, 131), base.end());
        CHECK(r.mounts()[0].directory_digest == sha256(directory_bytes) && r.mounts()[0].size == base.size() &&
              r.mounts()[0].role == MountRole::Base);
        Sha256Digest from_bytes{};
        std::vector<Resource> entries;
        std::string text;
        CHECK(read_rez(base, entries, text, &from_bytes) && from_bytes == r.mounts()[0].directory_digest);

        const std::vector<SaveClass> classes{{"CPlayerObj", 1}, {"Door", 1}};
        SaveFingerprint saved, current;
        SaveError error;
        CHECK(build_fingerprint(r, "TEST.DAT", classes, saved, error));
        CHECK(saved.edition == SaveEdition::Retail && saved.mounts.size() == 1 &&
              saved.mounts[0].name == "blood2.rez" && saved.world == sha256(world) && saved.classes == classes &&
              saved.class_schema == class_schema_digest(classes));
        // Deterministic: a second mount of the same files produces the same fingerprint.
        Resources again;
        CHECK(mount(again, {{"BLOOD2.REZ", MountRole::Base}}));
        CHECK(build_fingerprint(again, "test.dat", classes, current, error) && current == saved);
        const SaveClassSchema registered{{"CPlayerObj", 1}, {"Door", 1}, {"Trigger", 3}};
        CHECK(check_fingerprint(saved, current, registered, error) && error.fault == SaveFault::None);
        // The fingerprint survives the container.
        auto container = sample(false);
        container.fingerprint = saved;
        SaveContainer back;
        CHECK(decode_save(encoded(container), back, error) && back.fingerprint == saved);

        const auto refused = [&](const SaveFingerprint& now, const SaveClassSchema& schema,
                                 std::initializer_list<std::string_view> texts)
        {
            SaveError e;
            if (check_fingerprint(saved, now, schema, e) || e.fault != SaveFault::Fingerprint)
                return false;
            for (const auto t : texts)
                if (e.text.find(t) == std::string::npos)
                {
                    std::cerr << "missing '" << t << "' in: " << e.text << '\n';
                    return false;
                }
            return true;
        };
        // A mod added: the mod overrides the world, so the DAT digest differs too.
        Resources modded;
        CHECK(mount(modded, {{"BLOOD2.REZ", MountRole::Base}, {"mod.rez", MountRole::Mod}}));
        CHECK(build_fingerprint(modded, "test.dat", classes, current, error));
        CHECK(current.mounts[1].role == MountRole::Mod);
        CHECK(refused(current, registered, {"mod archive mod.rez added", "world DAT differs"}));
        // A data-only mod that leaves the world alone is still refused, naming it.
        Resources extra;
        CHECK(mount(extra, {{"BLOOD2.REZ", MountRole::Base}, {"extra.rez", MountRole::Mod}}));
        CHECK(build_fingerprint(extra, "test.dat", classes, current, error) && current.world == saved.world);
        CHECK(refused(current, registered, {"mod archive extra.rez added"}));
        // The saved set has a mod the installation lacks.
        {
            SaveFingerprint with_mod;
            CHECK(build_fingerprint(extra, "test.dat", classes, with_mod, error));
            SaveError e;
            CHECK(build_fingerprint(r, "test.dat", classes, current, error));
            CHECK(!check_fingerprint(with_mod, current, registered, e) &&
                  e.text.find("mod archive extra.rez removed") != std::string::npos);
        }
        // A changed base archive (different directory block, same name).
        std::filesystem::create_directories(root / "changed", ec);
        CHECK(save(root / "changed" / "BLOOD2.REZ", rez({{"test.dat", world}, {"new.dat", {7}}})));
        Resources changed;
        CHECK(changed.mount(root / "changed" / "BLOOD2.REZ", text));
        CHECK(build_fingerprint(changed, "test.dat", classes, current, error) && current.world == saved.world);
        CHECK(refused(current, registered, {"base archive blood2.rez changed"}));
        // Mount order.
        Resources ab, ba;
        CHECK(mount(ab, {{"BLOOD2.REZ", MountRole::Base}, {"extra.rez", MountRole::Base}}));
        CHECK(mount(ba, {{"extra.rez", MountRole::Base}, {"BLOOD2.REZ", MountRole::Base}}));
        SaveFingerprint order_ab, order_ba;
        CHECK(build_fingerprint(ab, "test.dat", classes, order_ab, error));
        CHECK(build_fingerprint(ba, "test.dat", classes, order_ba, error));
        {
            SaveError e;
            CHECK(!check_fingerprint(order_ab, order_ba, registered, e) &&
                  e.text.find("archive mount order differs") != std::string::npos);
        }
        // Same mounts, different effective index (only reachable through stored values).
        current = saved;
        current.index[0] ^= 1;
        CHECK(refused(current, registered, {"effective resource index differs"}));
        // World DAT.
        current = saved;
        current.world[5] ^= 1;
        CHECK(refused(current, registered, {"world DAT differs"}));
        // Edition.
        current = saved;
        current.edition = SaveEdition::Demo;
        CHECK(refused(current, registered, {"edition retail, installed demo"}));
        // Classes: unregistered and payload version, all named.
        CHECK(refused(saved, {{"CPlayerObj", 2}}, {"class Door is not registered",
                                                   "class CPlayerObj payload version 1, build 2"}));
        // Several differences are listed together.
        current = saved;
        current.edition = SaveEdition::Demo;
        current.world.fill(0);
        CHECK(refused(current, {}, {"edition", "world DAT", "class CPlayerObj", "class Door"}));
        // A missing world, an unnormalized path and an invalid class schema.
        CHECK(!build_fingerprint(r, "missing.dat", classes, current, error) &&
              error.fault == SaveFault::Fingerprint);
        CHECK(!build_fingerprint(r, "../test.dat", classes, current, error) &&
              error.fault == SaveFault::Fingerprint);
        CHECK(!build_fingerprint(r, "test.dat", {{"Door", 1}, {"CPlayerObj", 1}}, current, error) &&
              error.fault == SaveFault::Format);
        CHECK(!build_fingerprint(r, "test.dat", {{"Door", 0}}, current, error) && error.fault == SaveFault::Format);
        Resources none;
        CHECK(!build_fingerprint(none, "test.dat", classes, current, error) && error.fault == SaveFault::Bound);
        // Demo edition is recorded.
        Resources demo;
        CHECK(mount(demo, {{"BLOOD2.REZ", MountRole::Base}}));
        demo.set_demo(true);
        CHECK(build_fingerprint(demo, "test.dat", classes, current, error) && current.edition == SaveEdition::Demo);
        // The index digest names its mount ordinal: the same entries from another archive differ.
        Sha256Digest one{}, two{};
        CHECK(resource_index_digest(r, one, text));
        // Host paths do not enter it: the same archive under another name hashes identically.
        CHECK(save(root / "copy.rez", base));
        Resources copy;
        CHECK(mount(copy, {{"copy.rez", MountRole::Base}}));
        CHECK(resource_index_digest(copy, two, text) && one == two);
        Resources shifted;
        CHECK(mount(shifted, {{"extra.rez", MountRole::Base}, {"copy.rez", MountRole::Base}}));
        CHECK(resource_index_digest(shifted, two, text) && one != two);
        return true;
    }
    bool atomic()
    {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        const auto path = root / "nested" / "slot0.ob2sav";
        std::string error;
        const Bytes first{1, 2, 3}, second{4, 5};
        CHECK(write_file_atomic(path, first, "save", error));
        Bytes read;
        CHECK(read_file_bounded(path, 3, read, error) && read == first);
        CHECK(!read_file_bounded(path, 2, read, error) && read.empty() && !error.empty());
        CHECK(!read_file_bounded(root / "missing", 10, read, error));
        CHECK(!read_file_bounded(root / "nested", 10, read, error)); // A directory.
        // Stale temporaries survive a default write and are removed by an opted-in one.
        const auto stale = root / "nested" / "slot0.ob2sav.tmp-123";
        const auto unrelated = root / "nested" / "slot1.ob2sav.tmp-123";
        CHECK(save(stale, {9}) && save(unrelated, {9}));
        CHECK(write_file_atomic(path, second, "save", error) && std::filesystem::exists(stale));
        CHECK(write_file_atomic(path, second, "save", error, {.remove_stale_temporaries = true}));
        CHECK(!std::filesystem::exists(stale) && std::filesystem::exists(unrelated));
        CHECK(read_file_bounded(path, 10, read, error) && read == second);
        // A failure keeps the previous file and leaves no temporary.
        CHECK(!write_file_atomic(path / "impossible", first, "save", error) &&
              error.starts_with("Cannot create save directory: "));
        std::filesystem::create_directories(root / "target_is_dir", ec);
        CHECK(!write_file_atomic(root / "target_is_dir", first, "save", error) &&
              error.starts_with("Cannot atomically write save: "));
        for (const auto& entry : std::filesystem::directory_iterator(root))
            CHECK(path_text(entry.path().filename()).find(".tmp-") == std::string::npos);
        CHECK(read_file_bounded(path, 10, read, error) && read == second);
        // Save files: atomic replacement, quick META read, full read, refused encodes keep the file.
        const auto slot = root / "saves" / "slot1.ob2sav";
        const auto s = sample(true);
        SaveError failure;
        CHECK(write_save_file(slot, s, failure));
        SaveMeta meta;
        SaveContainer back;
        CHECK(read_save_meta_file(slot, meta, failure) && meta == s.meta);
        CHECK(read_save_file(slot, back, failure) && back == s);
        auto invalid = s;
        invalid.meta.game_type = 2;
        CHECK(!write_save_file(slot, invalid, failure) && failure.fault == SaveFault::Format);
        CHECK(read_save_file(slot, back, failure) && back == s);
        CHECK(!write_save_file(slot / "x", s, failure) && failure.fault == SaveFault::Io);
        CHECK(!read_save_file(root / "saves" / "none.ob2sav", back, failure) && failure.fault == SaveFault::Io);
        CHECK(!read_save_meta_file(root / "saves" / "none.ob2sav", meta, failure) && failure.fault == SaveFault::Io);
        // A truncated file on disk is reported as truncated by both readers.
        auto bytes = encoded(s);
        bytes.resize(bytes.size() / 2);
        CHECK(save(root / "saves" / "cut.ob2sav", bytes));
        CHECK(!read_save_file(root / "saves" / "cut.ob2sav", back, failure) && failure.fault == SaveFault::Truncated);
        CHECK(!read_save_meta_file(root / "saves" / "cut.ob2sav", meta, failure) &&
              failure.fault == SaveFault::Truncated);
        return true;
    }
    // Deterministic mutation fuzzing: the decoders must refuse or round-trip, never crash.
    bool fuzz()
    {
        std::minstd_rand random(20261020);
        const std::array<Bytes, 2> seeds{encoded(sample(false)), encoded(sample(true))};
        unsigned accepted = 0;
        for (unsigned round = 0; round < 20000; ++round)
        {
            auto bytes = seeds[round & 1];
            const unsigned edits = 1 + random() % 4;
            for (unsigned e = 0; e < edits && !bytes.empty(); ++e)
            {
                const auto pos = random() % bytes.size();
                switch (random() % 5)
                {
                case 0:
                    bytes[pos] = std::uint8_t(random());
                    break;
                case 1:
                    bytes.insert(bytes.begin() + pos, std::uint8_t(random()));
                    break;
                case 2:
                    bytes.erase(bytes.begin() + pos);
                    break;
                case 3:
                    bytes.resize(pos);
                    break;
                default:
                    // Large little-endian lengths at a random position.
                    if (pos + 4 <= bytes.size())
                        put32(bytes, pos, random() % 2 ? 0xFFFFFFFFu : std::uint32_t(random()));
                }
            }
            SaveContainer out;
            SaveError error;
            if (decode_save(bytes, out, error))
            {
                ++accepted;
                CHECK(encoded(out) == bytes);
            }
            else
                CHECK(error.fault != SaveFault::None && !error.text.empty());
            SaveMeta meta;
            if (!read_save_meta(bytes, bytes.size(), meta, error))
                CHECK(error.fault != SaveFault::None);
        }
        // Mutations that keep the bytes identical are the only accepted inputs in practice.
        std::cout << "fuzz: " << accepted << " mutated inputs accepted\n";
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    const std::map<std::string, bool (*)()> cases{
        {"digests", digests},     {"golden", golden},   {"roundtrip", roundtrip}, {"truncation", truncation},
        {"bitflips", bitflips},   {"bounds", bounds},   {"versions", versions},   {"order", order},
        {"fingerprint", fingerprint}, {"atomic", atomic}, {"fuzz", fuzz}};
    if (argc != 3 || !cases.count(argv[1]))
        return 2;
    root = path_from_utf8(argv[2]) / argv[1];
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (!cases.at(argv[1])())
        return 1;
    std::cout << argv[1] << " passed\n";
    return 0;
}
