#include "engine/ima_section.h"
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
    void word(Bytes& b, std::uint32_t v, unsigned n = 4)
    {
        for (unsigned i = 0; i < n; ++i)
            b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    }
    void append(Bytes& a, const Bytes& b)
    {
        a.insert(a.end(), b.begin(), b.end());
    }
    Bytes chunk(std::string_view tag, Bytes payload)
    {
        Bytes b(tag.begin(), tag.end());
        word(b, payload.size());
        append(b, payload);
        if (payload.size() & 1)
            b.push_back(0);
        return b;
    }
    Bytes form(std::string_view kind, Bytes children)
    {
        Bytes b(kind.begin(), kind.end());
        append(b, children);
        return chunk("RIFF", b);
    }
    void put(Bytes& b, unsigned pos, std::uint32_t value, unsigned n = 2)
    {
        for (unsigned i = 0; i < n; ++i)
            b.at(pos + i) = static_cast<std::uint8_t>(value >> (8 * i));
    }
    Bytes header()
    {
        Bytes b(120);
        put(b, 4, 'S');
        put(b, 36, 120);
        put(b, 40, 4);
        put(b, 42, 768);
        put(b, 44, 192);
        return b;
    }
    Bytes section(Bytes tracks = {}, Bytes h = header())
    {
        auto b = chunk("sref", {'S', 0, 0, 0});
        append(b, chunk("secn", h));
        append(b, tracks);
        return form("AASE", b);
    }
    Bytes records(unsigned stride, unsigned count)
    {
        Bytes b;
        word(b, stride, 2);
        b.resize(2 + stride * count);
        return b;
    }
    bool reader()
    {
        std::string error;
        auto h = header();
        put(h, 0, 0x87654321, 4);
        put(h, 38, 0x8000);
        put(h, 48, 0xabcdef01, 4);
        h[9] = 255; // Ignored label padding.
        for (unsigned i = 52; i < 120; ++i)
            h[i] = i;
        auto cmnd = records(8, 2);
        put(cmnd, 2, std::uint32_t(-999), 4);
        put(cmnd, 6, 0x1234ffff, 4);
        put(cmnd, 10, 192, 4);
        auto mute = records(8, 1);
        put(mute, 2, std::uint32_t(-192), 4);
        put(mute, 6, 0xdead);
        put(mute, 8, 0xbeef);
        auto chord = records(117, 1);
        put(chord, 2, 'C');
        chord[34] = 0xff;
        chord[35] = 0xfd;
        put(chord, 36, 0xfffe);
        put(chord, 38, 0x891, 4);
        put(chord, 42, 0xab5, 4);
        put(chord, 46, 0x12345678, 4);
        chord[50] = 0xfb;
        chord.back() = 255;
        auto tracks = chunk("cmnd", cmnd);
        append(tracks, chunk("mute", mute));
        append(tracks, chunk("chrd", chord));
        append(tracks, chunk("prnm", {'P', 0, 0, 0, 255, 255}));
        append(tracks, chunk("note", {255})); // Explicitly opaque section notes/curves.
        append(tracks, chunk("curv", {1}));
        append(tracks, form("UNKN", chunk("cmnd", {}))); // Nested unknown scope is not interpreted.
        auto data = section(tracks, h);
        ImaSection s;
        CHECK(read_ima_section(data, s, error));
        CHECK(s.name == "S" && s.personality == "P" && s.source.style_reference == "S");
        CHECK(s.word0 == 0x87654321 && s.word38 == -32768 && s.word48 == 0xabcdef01);
        CHECK(s.default_chord == 52 && s.padding53[2] == 55 && s.style_id[0] == 56 && s.personality_id[15] == 87 &&
              s.tail.back() == 119);
        CHECK(s.records() == 4 && s.commands[0].time == -999 && s.commands[0].value[3] == 0x12 &&
              s.commands[1].time == 192 && s.mutes[0].time == -192 && s.mutes[0].value == 0xdead &&
              s.mutes[0].word6 == 0xbeef);
        const auto& c = s.chords[0];
        CHECK(c.name == "C" && c.flags == 255 && c.beat == -3 && c.measure == -2 && c.chord_pattern == 0x891 &&
              c.scale_pattern == 0xab5 && c.word44 == 0x12345678 && c.root == -5 && c.tail.back() == 255);
        CHECK(s.opaque_note_chunks == 1 && s.opaque_curve_chunks == 1);
        data.assign(data.size(), 0);
        CHECK(c.name == "C" && s.commands[0].time == -999); // Owned after resource release.
        CHECK(read_ima_section(section(), s, error, 0) && s.records() == 0 && s.personality.empty());
        std::cout << "IMA section: signed fields, raw masks/tails, ordered records, labels and owned direct scope\n";
        return true;
    }
    bool timing()
    {
        std::string error;
        ImaSection s;
        CHECK(read_ima_section(section(), s, error));
        ImaSectionTiming t;
        CHECK(ima_section_timing(s, t, error) && t.duration_ticks == 3072 && t.microseconds_per_quarter == 500000);
        s.tempo_fraction = 65535;
        CHECK(ima_section_timing(s, t, error) && t.microseconds_per_quarter == 495931);
        s.tempo = 32767;
        s.measures = s.ticks_per_measure = 32767;
        CHECK(ima_section_timing(s, t, error) && t.duration_ticks == 1073676289 && t.microseconds_per_quarter == 1831);
        s.tempo = 1;
        s.tempo_fraction = 0;
        CHECK(ima_section_timing(s, t, error) && t.microseconds_per_quarter == 60000000);
        for (auto member :
             {&ImaSection::tempo, &ImaSection::measures, &ImaSection::ticks_per_measure, &ImaSection::ticks_per_beat})
        {
            const auto previous = s.*member;
            for (auto v : {0, -1, -32768})
            {
                s.*member = v;
                CHECK(!ima_section_timing(s, t, error) && t.duration_ticks == 0 && t.microseconds_per_quarter == 0);
            }
            s.*member = previous;
        }
        s.ticks_per_measure = 768;
        std::int16_t measure;
        CHECK(ima_section_command_measure(s, -769, measure) && measure == -1);
        CHECK(ima_section_command_measure(s, -767, measure) && measure == 0);
        CHECK(ima_section_command_measure(s, 768 * 32768, measure) && measure == -32768);
        s.ticks_per_measure = 1;
        CHECK(ima_section_command_measure(s, INT32_MIN, measure) && measure == 0);
        CHECK(ima_section_command_measure(s, INT32_MAX, measure) && measure == -1);
        s.ticks_per_measure = 0;
        CHECK(!ima_section_command_measure(s, 123, measure) && measure == 0);
        std::cout
            << "IMA authored timing: original tempo quantization, duration, signed division/wrap and zero guards\n";
        return true;
    }
    bool malformed()
    {
        std::string error;
        ImaSection s;
        const auto valid = section(chunk("cmnd", records(8, 1)));
        for (std::size_t i = 0; i < valid.size(); ++i)
        {
            s.name = "stale";
            CHECK(!read_ima_section(std::span(valid).first(i), s, error));
            CHECK(s.name.empty() && s.source.chunks.empty() && s.records() == 0 && !error.empty());
        }
        CHECK(!read_ima_section(valid, s, error, 0));
        CHECK(read_ima_section(valid, s, error, 1));
        CHECK(!read_ima_section(form("AASE", chunk("sref", {'S', 0, 0, 0})), s, error));
        CHECK(!read_ima_section(section(chunk("secn", header())), s, error));
        for (unsigned n : {0, 119, 121})
            CHECK(!read_ima_section(section({}, Bytes(n)), s, error));
        auto h = header();
        std::fill(h.begin() + 4, h.begin() + 36, 65);
        CHECK(!read_ima_section(section({}, h), s, error));
        h = header();
        h[4] = 255;
        CHECK(!read_ima_section(section({}, h), s, error));
        for (const auto* id : {"cmnd", "mute", "chrd"})
        {
            const auto stride = std::string_view(id) == "chrd" ? 117 : 8;
            for (auto payload : {Bytes{}, Bytes{8}, records(0, 0), records(stride + 1, 1), records(stride - 1, 1)})
                CHECK(!read_ima_section(section(chunk(id, payload)), s, error));
            auto bad = records(stride, 1);
            bad.pop_back();
            CHECK(!read_ima_section(section(chunk(id, bad)), s, error));
            auto twice = chunk(id, records(stride, 0));
            const auto copy = twice;
            append(twice, copy);
            CHECK(!read_ima_section(section(twice), s, error));
        }
        auto early = chunk("sref", {'S', 0, 0, 0});
        append(early, chunk("cmnd", records(8, 1)));
        append(early, chunk("secn", header()));
        CHECK(!read_ima_section(form("AASE", early), s, error));
        for (auto p : {Bytes{}, Bytes{65}, Bytes(82), Bytes{255, 0, 0, 0}, Bytes{65, 0}})
            CHECK(!read_ima_section(section(chunk("prnm", p)), s, error));
        auto twice = chunk("prnm", {0, 0});
        const auto copy = twice;
        append(twice, copy);
        CHECK(!read_ima_section(section(twice), s, error));
        auto chord = records(117, 1);
        chord[3] = 255;
        CHECK(!read_ima_section(section(chunk("chrd", chord)), s, error));
        auto many = chunk("cmnd", records(8, 4096));
        CHECK(read_ima_section(section(many), s, error) && s.records() == 4096);
        CHECK(!read_ima_section(section(many), s, error, 4095));
        append(many, chunk("mute", records(8, 1)));
        CHECK(!read_ima_section(section(many), s, error));
        CHECK(s.source.chunks.empty() && s.records() == 0);
        std::cout << "IMA section malformed: truncation, headers, stride, scope, labels and aggregate budgets\n";
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        std::vector<std::filesystem::path> files;
        for (const auto& f : std::filesystem::recursive_directory_iterator(root / "Music"))
            if (f.path().extension() == ".sec")
                files.push_back(f.path());
        std::sort(files.begin(), files.end());
        std::uint64_t hash = 14695981039346656037ull;
        std::size_t commands = 0, mutes = 0, chords = 0;
        for (const auto& file : files)
        {
            std::ifstream in(file, std::ios::binary);
            Bytes data{std::istreambuf_iterator<char>(in), {}};
            ImaSection s;
            CHECK(read_ima_section(data, s, error));
            ImaSectionTiming t;
            CHECK(ima_section_timing(s, t, error) && t.duration_ticks >= 768 && t.duration_ticks <= 6144);
            CHECK(s.commands.size() == 1 && s.chords.size() == 1 && s.personality == "Silence" &&
                  s.opaque_note_chunks == 0 && s.opaque_curve_chunks == 0);
            commands += s.commands.size();
            mutes += s.mutes.size();
            chords += s.chords.size();
            // Canonical original-reader output, in source chunk order. No proprietary bytes are exported.
            for (const auto& c : s.source.chunks)
            {
                if (c.depth != 1)
                    continue;
                Bytes mapped;
                if (c.id == "cmnd")
                    for (const auto& v : s.commands)
                    {
                        word(mapped, v.time);
                        mapped.insert(mapped.end(), v.value.begin(), v.value.end());
                    }
                if (c.id == "mute")
                    for (const auto& v : s.mutes)
                    {
                        word(mapped, v.time);
                        word(mapped, v.value, 2);
                        word(mapped, v.word6, 2);
                    }
                if (c.id == "chrd")
                    for (const auto& v : s.chords)
                    {
                        mapped.push_back(v.flags & 15);
                        mapped.push_back(static_cast<std::uint8_t>(v.beat));
                        word(mapped, v.measure, 2);
                        word(mapped, v.chord_pattern);
                        word(mapped, v.scale_pattern);
                        mapped.push_back(static_cast<std::uint8_t>(v.root));
                    }
                for (const auto byte : mapped)
                {
                    hash ^= byte;
                    hash *= 1099511628211ull;
                }
            }
        }
        CHECK(files.size() == 156 && commands == 156 && mutes == 3290 && chords == 156);
        std::cout << "Private IMA sections: " << files.size() << " / commands " << commands << " / mutes " << mutes
                  << " / chords " << chords << " / original reader FNV64 " << hash << '\n';
        CHECK(hash == 17214005195151465368ull);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view test = argv[1];
    return (test == "reader"                 ? reader()
            : test == "timing"               ? timing()
            : test == "malformed"            ? malformed()
            : test == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
