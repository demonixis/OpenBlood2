#include "engine/ima_band.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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
    Bytes fixture()
    {
        Bytes p(202);
        p[0] = 'B';
        p[170] = 'C';
        for (unsigned i = 32; i < 170; ++i)
            p[i] = i;
        return p;
    }
    Bytes encode(const ImaBand& b)
    {
        Bytes p(b.reserved32.begin(), b.reserved32.end());
        for (const auto& c : b.channels)
            p.push_back(c.legacy_program);
        for (const auto& c : b.channels)
            p.push_back(c.volume);
        for (const auto& c : b.channels)
            p.push_back(c.pan);
        for (const auto& c : b.channels)
            p.push_back(static_cast<std::uint8_t>(c.transpose));
        p.push_back(static_cast<std::uint8_t>(b.byte104));
        p.push_back(b.byte105);
        for (const auto& c : b.channels)
            word(p, c.bank, 2);
        for (const auto& c : b.channels)
            p.push_back(c.program);
        for (const auto& c : b.channels)
            p.push_back(c.byte154);
        return p;
    }
    bool reader()
    {
        std::string error;
        ImaBands result;
        auto payload = fixture();
        payload[104] = 255;
        payload[88] = 128;
        // Garbage padding after the first terminator does not change either name.
        payload[5] = 255;
        payload[172] = 255;
        auto a = form("AABN", chunk("band", payload));
        auto ordered = a;
        append(ordered, a);
        auto bytes = form("AASY", ordered);
        CHECK(read_ima_bands(bytes, MusicAssetKind::Style, result, error));
        CHECK(result.bands.size() == 2 && result.bands[0].offset < result.bands[1].offset);
        const auto& band = result.bands[0];
        CHECK(band.name == "B" && band.collection == "C" && band.byte104 == -1 && band.channels[0].transpose == -128);
        CHECK(encode(band) == Bytes(payload.begin() + 32, payload.begin() + 170));
        bytes.assign(bytes.size(), 0);
        CHECK(band.name == "B" && band.channels[15].byte154 == 169);
        auto section = chunk("sref", {'S', 0, 0, 0});
        append(section, a);
        CHECK(read_ima_bands(form("AASE", section), MusicAssetKind::Section, result, error));
        CHECK(result.bands.size() == 1 && result.source.style_reference == "S");
        CHECK(read_ima_bands(form("AASY", form("UNKN", a)), MusicAssetKind::Style, result, error));
        CHECK(result.bands.empty() && result.source.chunks.size() == 4); // Nested band remains opaque.
        CHECK(read_ima_bands(form("AASY", {}), MusicAssetKind::Style, result, error, 0));
        std::cout << "IMA bands: exact fields, signed bytes, duplicate names, ordered direct scope and owned ranges\n";
        return true;
    }
    bool setup()
    {
        std::string error;
        ImaBand band;
        band.collection = "C";
        for (unsigned i = 0; i < 16; ++i)
        {
            auto& c = band.channels[i];
            c.bank = 0x1234;
            c.program = i + 1;
            c.legacy_program = i + 80;
            c.volume = i + 90;
            c.pan = i + 20;
        }
        std::array<ImaMidiSetup, 80> events;
        CHECK(ima_band_setup(band, events, error));
        for (unsigned i = 0; i < 16; ++i)
        {
            const auto channel = static_cast<std::uint8_t>((i + 4) % 16);
            const auto cc = static_cast<std::uint8_t>(0xb0 + channel);
            CHECK((events[i * 5] == ImaMidiSetup{cc, 7, static_cast<std::uint8_t>(i + 90)}));
            CHECK((events[i * 5 + 1] == ImaMidiSetup{cc, 10, static_cast<std::uint8_t>(i + 20)}));
            CHECK((events[i * 5 + 2] == ImaMidiSetup{cc, 0, 36}));
            CHECK((events[i * 5 + 3] == ImaMidiSetup{cc, 32, 52}));
            CHECK((events[i * 5 + 4] ==
                   ImaMidiSetup{static_cast<std::uint8_t>(0xc0 + channel), static_cast<std::uint8_t>(i + 1), 0}));
        }
        CHECK(ima_band_patch(band.channels[5], true, true) == 0x80091a06);
        for (bool collection : {false, true})
            for (unsigned flags : {0, 0x4000, 0x8000, 0xc000})
            {
                band.collection = collection ? "C" : "";
                for (auto& c : band.channels)
                    c.bank = flags | 0x1234;
                CHECK(ima_band_setup(band, events, error));
                const bool gm = flags == 0x8000 || (!collection && flags == 0);
                CHECK(events[2].data2 == (gm ? 0 : 36) && events[3].data2 == (gm ? 0 : 52));
                CHECK(events[4].data1 == (gm ? 80 : 1));
            }
        // No partial valid prefix on a final-channel failure. Unknown numeric fields remain data.
        band.channels[15].pan = 128;
        CHECK(!ima_band_setup(band, events, error));
        CHECK(std::all_of(events.begin(), events.end(), [](const auto& e) { return e == ImaMidiSetup{}; }));
        band.channels[15].pan = 64;
        band.channels[0].volume = 255;
        CHECK(!ima_band_setup(band, events, error));
        band.channels[0].volume = 100;
        band.collection.clear();
        band.channels[0].bank = 0;
        band.channels[0].legacy_program = 255;
        CHECK(!ima_band_setup(band, events, error));
        band.collection = "C";
        CHECK(ima_band_setup(band, events, error) && error.empty()); // Unused GM field.
        std::cout
            << "IMA setup: original ordered controllers, bank flags, fallback, channel wrap and checked MIDI ranges\n";
        return true;
    }
    bool malformed()
    {
        std::string error;
        ImaBands out;
        auto header = chunk("band", fixture());
        const auto valid = form("AASY", form("AABN", header));
        for (std::size_t n = 0; n < valid.size(); ++n)
        {
            CHECK(!read_ima_bands(std::span(valid).first(n), MusicAssetKind::Style, out, error));
            CHECK(out.bands.empty() && out.source.chunks.empty() && !error.empty());
        }
        CHECK(!read_ima_bands(valid, MusicAssetKind::Instruments, out, error));
        CHECK(!read_ima_bands(valid, MusicAssetKind::Style, out, error, 0));
        CHECK(read_ima_bands(valid, MusicAssetKind::Style, out, error, 1));
        CHECK(!read_ima_bands(form("AASY", form("AABN", {})), MusicAssetKind::Style, out, error));
        auto twice = header;
        append(twice, header);
        CHECK(!read_ima_bands(form("AASY", form("AABN", twice)), MusicAssetKind::Style, out, error));
        for (unsigned n : {0, 201, 203})
            CHECK(!read_ima_bands(form("AASY", form("AABN", chunk("band", Bytes(n)))), MusicAssetKind::Style, out,
                                  error));
        for (unsigned field : {0, 170})
        {
            auto p = fixture();
            std::fill(p.begin() + field, p.begin() + field + 32, 65);
            CHECK(!read_ima_bands(form("AASY", form("AABN", chunk("band", p))), MusicAssetKind::Style, out, error));
            p = fixture();
            p[field] = 255;
            CHECK(!read_ima_bands(form("AASY", form("AABN", chunk("band", p))), MusicAssetKind::Style, out, error));
        }
        Bytes many;
        for (unsigned i = 0; i < 256; ++i)
            append(many, form("AABN", header));
        CHECK(read_ima_bands(form("AASY", many), MusicAssetKind::Style, out, error) && out.bands.size() == 256);
        CHECK(!read_ima_bands(form("AASY", many), MusicAssetKind::Style, out, error, 255));
        append(many, form("AABN", header));
        CHECK(!read_ima_bands(form("AASY", many), MusicAssetKind::Style, out, error));
        CHECK(out.bands.empty() && out.source.chunks.empty());
        std::cout << "IMA bands malformed: truncation, scope/header/name errors, fixed size and whole-file budgets\n";
        return true;
    }
    ImaBands selection_fixture(bool section, unsigned count, unsigned early, unsigned mask = 0)
    {
        Bytes children;
        for (unsigned i = 0; i <= count; ++i)
        {
            if (i == early)
                append(children, chunk(section ? "secn" : "styl", Bytes(120)));
            if (i < count)
            {
                auto p = fixture();
                p[104] = (mask & (1u << i)) ? (i % 2 ? 128 : 255) : 0;
                append(children, form("AABN", chunk("band", p)));
            }
        }
        if (section)
            append(children, chunk("sref", {'S', 0, 0, 0}));
        ImaBands result;
        std::string error;
        if (!read_ima_bands(form(section ? "AASE" : "AASY", children),
                            section ? MusicAssetKind::Section : MusicAssetKind::Style, result, error))
            std::abort();
        return result;
    }
    bool selection()
    {
        std::string error;
        std::optional<ImaBandChoice> choice;
        auto section = selection_fixture(true, 3, 0);
        auto style = selection_fixture(false, 4, 0, 15);
        CHECK(select_ima_band(section, &style, choice, error));
        CHECK((choice == ImaBandChoice{ImaBandScope::Section, 2})); // Section flag is irrelevant.
        section = selection_fixture(true, 3, 3);                    // All early bands are retained, but not loaded.
        CHECK(select_ima_band(section, &style, choice, error));
        CHECK((choice == ImaBandChoice{ImaBandScope::Style, 3})); // Negative flags also mark defaults.
        style.bands.back().byte104 = 0;
        CHECK(select_ima_band(section, &style, choice, error));
        CHECK((choice == ImaBandChoice{ImaBandScope::Style, 2}));
        for (auto& b : style.bands)
            b.byte104 = 0;
        CHECK(!select_ima_band(section, &style, choice, error) && !choice && !error.empty());
        CHECK(!select_ima_band(section, nullptr, choice, error) && !choice && !error.empty());
        section = selection_fixture(true, 1, 0);
        CHECK(select_ima_band(section, nullptr, choice, error) && error.empty());
        auto owner = std::make_shared<const ImaBands>(section);
        std::weak_ptr<const ImaBands> lifetime = owner;
        auto selected = std::shared_ptr<const ImaBand>(owner, &owner->bands[choice->index]);
        owner.reset();
        CHECK(!lifetime.expired() && selected->channels[0].volume == 56);
        selected.reset();
        CHECK(lifetime.expired());
        section.bands.resize(257);
        CHECK(!select_ima_band(section, &style, choice, error) && !choice);
        section = selection_fixture(true, 0, 0);
        style.source.chunks.front().form = "AASE";
        CHECK(!select_ima_band(section, &style, choice, error) && !choice);
        section.source.chunks.push_back({"secn", "", 500, 120, 1});
        CHECK(!select_ima_band(section, nullptr, choice, error) && !choice);
        section.source.chunks.clear();
        CHECK(!select_ima_band(section, nullptr, choice, error) && !choice);
        std::cout << "Band selection: section priority, reverse style defaults, early bands, missing defaults, "
                     "bounds/lifetime\n";
        return true;
    }
    bool selection_generated()
    {
        std::string error;
        std::uint64_t hash = 14695981039346656037ull;
        unsigned cases = 0;
        for (unsigned mask = 0; mask < 16; ++mask)
            for (unsigned style_early = 0; style_early <= 4; ++style_early)
                for (unsigned count = 0; count <= 3; ++count)
                    for (unsigned early = 0; early <= count; ++early)
                    {
                        const auto section = selection_fixture(true, count, early);
                        const auto style = selection_fixture(false, 4, style_early, mask);
                        std::optional<ImaBandChoice> choice;
                        const bool ok = select_ima_band(section, &style, choice, error);
                        CHECK(ok == choice.has_value() && ok == error.empty());
                        // Pinned to the isolated original loader/getter probe below, not a second chooser.
                        for (auto value :
                             {choice ? unsigned(choice->scope) + 1 : 0u, choice ? unsigned(choice->index) + 1 : 0u})
                            for (unsigned byte = 0; byte < 4; ++byte)
                            {
                                hash ^= (value >> (byte * 8)) & 255;
                                hash *= 1099511628211ull;
                            }
                        ++cases;
                    }
        std::cout << "Band selection generated: " << cases << " cases / FNV64 " << hash << '\n';
        CHECK(cases == 800 && hash == 9953174249627237157ull);
        return true;
    }
    bool private_data(const std::filesystem::path& root)
    {
        std::string error;
        std::vector<std::filesystem::path> files;
        for (const auto& f : std::filesystem::recursive_directory_iterator(root / "Music"))
            if (f.path().extension() == ".sty" || f.path().extension() == ".sec")
                files.push_back(f.path());
        std::sort(files.begin(), files.end());
        std::size_t bands = 0, events = 0, selected_sections = 0;
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto& file : files)
        {
            std::ifstream in(file, std::ios::binary);
            Bytes data{std::istreambuf_iterator<char>(in), {}};
            ImaBands decoded;
            CHECK(read_ima_bands(data, file.extension() == ".sty" ? MusicAssetKind::Style : MusicAssetKind::Section,
                                 decoded, error));
            CHECK(decoded.bands.size() == 1);
            if (file.extension() == ".sec")
            {
                std::optional<ImaBandChoice> choice;
                CHECK(select_ima_band(decoded, nullptr, choice, error));
                CHECK((choice == ImaBandChoice{ImaBandScope::Section, 0}));
                ++selected_sections;
            }
            for (const auto& b : decoded.bands)
            {
                ++bands;
                CHECK(encode(b) == Bytes(data.begin() + b.offset + 8 + 32, data.begin() + b.offset + 8 + 170));
                std::array<ImaMidiSetup, 80> setup;
                CHECK(ima_band_setup(b, setup, error));
                for (const auto& e : setup)
                {
                    ++events;
                    for (auto byte : {e.status, e.data1, e.data2})
                    {
                        hash ^= byte;
                        hash *= 1099511628211ull;
                    }
                }
            }
        }
        CHECK(files.size() == 195 && bands == 195 && events == 15600 && selected_sections == 156);
        std::cout << "Private IMA bands: " << bands << " / setup messages " << events << " / FNV64 " << hash << '\n';
        // Pinned against the isolated original IMUSIC25 band loader/apply probe, not this implementation.
        CHECK(hash == 7277285548350807734ull);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view test = argv[1];
    return (test == "reader"                 ? reader()
            : test == "selection"            ? selection()
            : test == "selection_generated"  ? selection_generated()
            : test == "setup"                ? setup()
            : test == "malformed"            ? malformed()
            : test == "private" && argc == 3 ? private_data(argv[2])
                                             : false)
               ? 0
               : 1;
}
