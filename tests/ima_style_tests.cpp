#include "engine/ima_style.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace OpenBlood2;
using Bytes = std::vector<std::uint8_t>;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " | " << error << '\n';                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    void word(Bytes& b, std::uint32_t v, unsigned size = 4)
    {
        for (unsigned i = 0; i < size; ++i)
            b.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
    }
    void append(Bytes& a, const Bytes& b)
    {
        a.insert(a.end(), b.begin(), b.end());
    }
    Bytes chunk(std::string_view id, Bytes p)
    {
        Bytes b(id.begin(), id.end());
        word(b, p.size());
        append(b, p);
        if (p.size() & 1)
            b.push_back(0);
        return b;
    }
    Bytes form(std::string_view id, std::string_view type, Bytes p)
    {
        Bytes b(type.begin(), type.end());
        append(b, p);
        return chunk(id, b);
    }
    void name(Bytes& b, std::size_t at, std::string_view text)
    {
        for (auto c : text)
        {
            b[at] = c;
            at += 2;
        }
    }
    Bytes style(Bytes children)
    {
        Bytes header(94);
        name(header, 0, "Synthetic");
        auto data = chunk("styl", header);
        append(data, children);
        return form("RIFF", "AASY", data);
    }
    Bytes pattern(Bytes children)
    {
        Bytes p;
        word(p, 0x12345678);
        word(p, 3072);
        word(p, 0xabcdef01);
        for (auto v : {48, 4, 192, 4})
            word(p, v, 2);
        p.resize(625);
        name(p, 20, "Fixture");
        auto data = chunk("patt", p);
        append(data, children);
        return form("RIFF", "AAPT", data);
    }
    Bytes grid(Bytes children, std::uint16_t index = 5)
    {
        Bytes p;
        word(p, index, 2);
        auto data = chunk("clik", p);
        append(data, children);
        return form("LIST", "AACL", data);
    }
    Bytes record()
    {
        return {9, 15, 0xe8, 0xff, 1, 0x80, 0x21, 0xa2, 60, 100, 0x0f, 0x10, 0xc0, 3, 4, 0, 7, 0x87};
    }
    Bytes note(const Bytes& record)
    {
        Bytes p{18, 0};
        append(p, record);
        return chunk("note", p);
    }
    Bytes encode(const ImaNote& n)
    {
        Bytes b{n.type, n.channel};
        word(b, static_cast<std::uint16_t>(n.time_offset), 2);
        word(b, n.variations, 2);
        for (auto x : {n.pitch_modifier, n.byte7, n.midi_note, n.velocity})
            b.push_back(x);
        word(b, n.music_value, 2);
        word(b, static_cast<std::uint16_t>(n.duration), 2);
        for (auto x : {n.time_range, n.duration_range, n.velocity_range, n.play_mode})
            b.push_back(x);
        return b;
    }
    bool reader()
    {
        std::string error;
        ImaStyle decoded;
        auto first = note(record());
        append(first, chunk("curv", {1, 2, 3})); // Explicitly opaque, not a validated curve.
        auto negative = record();
        negative[12] = 0xff;
        negative[13] = 0xff;
        append(first, note(negative));
        auto grids = grid(first);
        append(grids, grid(note({}), 0xffff)); // Preserve empty and authored out-of-order/signed indexes.
        auto p = pattern(grids);
        append(p, pattern(grid(note(record())))); // Same names do not merge patterns.
        auto bytes = style(p);
        // styl is first, payload starts at 20. Preserve signed fields and opaque bytes.
        const std::array<std::int16_t, 6> fields{6, 8, 48, -32768, 32767, 176};
        for (std::size_t i = 0; i < fields.size(); ++i)
        {
            const auto v = static_cast<std::uint16_t>(fields[i]);
            bytes[20 + 32 + 2 * i] = static_cast<std::uint8_t>(v);
            bytes[20 + 33 + 2 * i] = static_cast<std::uint8_t>(v >> 8);
        }
        for (unsigned i = 44; i < 94; ++i)
            bytes[20 + i] = i;
        CHECK(read_ima_style(bytes, decoded, error));
        CHECK(decoded.beats_per_measure == 6 && decoded.beat_unit == 8 && decoded.ticks_per_grid == 48 &&
              decoded.authored_ticks_per_beat == -32768 && decoded.authored_ticks_per_measure == 32767 &&
              decoded.tempo == 176 && decoded.word44 == 0x2d2c && decoded.identifier[0] == 46 &&
              decoded.identifier[15] == 61 && decoded.tail.back() == 93);
        CHECK(decoded.name == "Synthetic" && decoded.patterns.size() == 2 && decoded.notes == 3 && decoded.grids == 3 &&
              decoded.curve_chunks == 1);
        const auto& pat = decoded.patterns.front();
        CHECK(pat.name == "Fixture" && pat.word0 == 0x12345678 && pat.flags == 0xabcdef01 && pat.length == 3072);
        CHECK((pat.timing == std::array<std::int16_t, 4>{48, 4, 192, 4}));
        CHECK(pat.grids[0].index == 5 && pat.grids[1].index == -1 && pat.grids[1].notes.empty());
        CHECK(pat.grids[0].notes[0].time_offset == -24 && pat.grids[0].notes[1].duration == -1);
        CHECK(encode(pat.grids[0].notes[0]) == record() && encode(pat.grids[0].notes[1]) == negative);
        bytes.assign(bytes.size(), 0);
        CHECK(encode(pat.grids[0].notes[0]) == record()); // No retained input span.
        CHECK(std::any_of(decoded.source.chunks.begin(), decoded.source.chunks.end(),
                          [](const auto& c) { return c.id == "curv" && c.size == 3; }));
        // No interpretation of unknown bytes, event types or signed numeric extremes in a data reader.
        Bytes extremes(18, 255);
        CHECK(read_ima_style(style(pattern(grid(note(extremes)))), decoded, error));
        CHECK(encode(decoded.patterns[0].grids[0].notes[0]) == extremes);
        CHECK(read_ima_style(style({}), decoded, error) && decoded.patterns.empty()); // Init style.
        auto nested = form("LIST", "UNKN", pattern(grid(note(record()))));
        CHECK(read_ima_style(style(nested), decoded, error) && decoded.patterns.empty() && decoded.notes == 0);
        std::cout << "IMA records: signed fields, order, repeated names, empty grids, opaque curves and owned data\n";
        return true;
    }
    bool malformed()
    {
        std::string error;
        ImaStyle decoded;
        const auto valid = style(pattern(grid(note(record()))));
        for (std::size_t i = 0; i < valid.size(); ++i)
        {
            CHECK(!read_ima_style(std::span(valid).first(i), decoded, error));
            CHECK(decoded.patterns.empty() && decoded.source.chunks.empty() && !error.empty());
        }
        CHECK(!read_ima_style(valid, decoded, error, 0));
        CHECK(read_ima_style(valid, decoded, error, 1));
        for (unsigned stride : {0, 1, 17, 19, 65535})
        {
            Bytes b;
            word(b, stride, 2);
            append(b, record());
            CHECK(!read_ima_style(style(pattern(grid(chunk("note", b)))), decoded, error));
        }
        auto short_record = record();
        short_record.pop_back();
        CHECK(!read_ima_style(style(pattern(grid(note(short_record)))), decoded, error));
        CHECK(!read_ima_style(style(pattern(grid(chunk("note", {18})))), decoded, error));
        CHECK(!read_ima_style(style(pattern(grid(chunk("clik", {0, 0})))), decoded, error));
        CHECK(!read_ima_style(style(pattern(form("LIST", "AACL", note(record())))), decoded, error));
        CHECK(!read_ima_style(style(form("RIFF", "AAPT", grid(note(record())))), decoded, error));
        CHECK(!read_ima_style(form("RIFF", "AASY", {}), decoded, error));
        CHECK(!read_ima_style(form("RIFF", "AASY", chunk("styl", Bytes(93))), decoded, error));
        CHECK(!read_ima_style(style(form("RIFF", "AAPT", chunk("patt", Bytes(624)))), decoded, error));
        CHECK(!read_ima_style(style(pattern(form("LIST", "AACL", chunk("clik", {0})))), decoded, error));
        auto pair = grid(note(record()));
        append(pair, grid(note(record())));
        CHECK(!read_ima_style(style(pattern(pair)), decoded, error, 1));
        CHECK(read_ima_style(style(pattern(pair)), decoded, error, 2) && decoded.notes == 2);
        CHECK(!read_ima_style(style(chunk("styl", Bytes(94))), decoded, error));
        CHECK(!read_ima_style(style(pattern(chunk("patt", Bytes(625)))), decoded, error));
        auto bad = valid;
        // style header name, including final UTF-16 slot, must terminate and be ASCII in this profile.
        for (unsigned i = 0; i < 16; ++i)
            bad[20 + i * 2] = 'A';
        CHECK(!read_ima_style(bad, decoded, error));
        bad = valid;
        bad[21] = 1;
        CHECK(!read_ima_style(bad, decoded, error));
        // Bounds are per style, across grids/chunks; no partially published result on exhaustion.
        Bytes too_many;
        const auto r = record();
        for (unsigned i = 0; i < 65537; ++i)
            append(too_many, r);
        CHECK(!read_ima_style(style(pattern(grid(note(too_many)))), decoded, error));
        too_many.resize(65536 * 18);
        CHECK(read_ima_style(style(pattern(grid(note(too_many)))), decoded, error) && decoded.notes == 65536);
        Bytes patterns;
        for (unsigned i = 0; i < 257; ++i)
            append(patterns, pattern({}));
        CHECK(!read_ima_style(style(patterns), decoded, error));
        CHECK(decoded.name.empty() && decoded.source.chunks.empty() && decoded.patterns.empty());
        std::cout << "IMA malformed: truncations, singleton/header/stride/name errors and whole-style budgets\n";
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        std::vector<std::filesystem::path> files;
        for (const auto& f : std::filesystem::recursive_directory_iterator(root / "Music"))
            if (f.path().extension() == ".sty")
                files.push_back(f.path());
        std::sort(files.begin(), files.end());
        std::size_t patterns = 0, grids = 0, notes = 0, curves = 0;
        for (const auto& file : files)
        {
            std::ifstream in(file, std::ios::binary);
            Bytes data{std::istreambuf_iterator<char>(in), {}};
            ImaStyle style;
            if (!read_ima_style(data, style, error))
                std::cerr << file << '\n';
            CHECK(error.empty());
            patterns += style.patterns.size();
            grids += style.grids;
            notes += style.notes;
            curves += style.curve_chunks;
            // The isolated original reader probe established identical 18-byte records, in source order.
            // Independently re-encode every typed field and compare to all installed note payloads.
            Bytes expected, actual;
            for (const auto& c : style.source.chunks)
                if (c.id == "note")
                    expected.insert(expected.end(), data.begin() + c.offset + 10, data.begin() + c.offset + 8 + c.size);
            for (const auto& p : style.patterns)
                for (const auto& g : p.grids)
                    for (const auto& n : g.notes)
                        append(actual, encode(n));
            CHECK(actual == expected);
        }
        CHECK(files.size() == 39 && patterns == 131 && grids == 7166 && notes == 7030 && curves == 42);
        std::cout << "Private IMA: " << files.size() << " styles / " << patterns << " patterns / " << grids
                  << " grids / " << notes << " exact original-order notes / " << curves << " opaque curves\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view test = argv[1];
    return (test == "reader"                 ? reader()
            : test == "malformed"            ? malformed()
            : test == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
