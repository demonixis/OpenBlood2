#include "engine/ima_curve.h"
#include "game/music_catalog.h"
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
    void word(Bytes& b, std::uint32_t v, unsigned size = 2)
    {
        for (unsigned i = 0; i < size; ++i)
            b.push_back(std::uint8_t(v >> (8 * i)));
    }
    void append(Bytes& b, const Bytes& a)
    {
        b.insert(b.end(), a.begin(), a.end());
    }
    Bytes chunk(std::string_view tag, Bytes p)
    {
        Bytes b(tag.begin(), tag.end());
        word(b, p.size(), 4);
        append(b, p);
        if (p.size() & 1)
            b.push_back(0);
        return b;
    }
    Bytes form(std::string_view tag, std::string_view type, Bytes p)
    {
        Bytes b(type.begin(), type.end());
        append(b, p);
        return chunk(tag, b);
    }
    Bytes encode(const ImaCurve& c)
    {
        Bytes b{c.type, c.channel};
        word(b, std::uint16_t(c.time_offset));
        word(b, c.variations);
        b.push_back(c.controller);
        word(b, c.points.size());
        for (auto p : c.points)
        {
            b.push_back(p.shape);
            b.push_back(p.flags);
            for (auto v : {p.start, p.value, p.end, p.target})
                word(b, std::uint16_t(v));
        }
        return b;
    }
    ImaCurve curve()
    {
        ImaCurve c;
        c.type = 3;
        c.channel = 15;
        c.time_offset = -100;
        c.variations = 0x8001;
        c.controller = 233;
        c.points = {{4, 3, -1, -32768, 32767, -123}, {1, 255, 20, 90, -20, 50}};
        return c;
    }
    Bytes payload(const std::vector<ImaCurve>& curves)
    {
        Bytes b;
        word(b, 7);
        word(b, 10);
        for (auto c : curves)
            append(b, encode(c));
        return b;
    }
    Bytes file(Bytes data, bool nested = false)
    {
        auto g = chunk("clik", {3, 0});
        append(g, chunk("curv", data));
        if (nested)
            append(g, form("LIST", "JUNK", chunk("curv", {0}))); // Outside admitted scope.
        auto p = chunk("patt", Bytes(625));
        append(p, form("LIST", "AACL", g));
        auto s = chunk("styl", Bytes(94));
        append(s, form("RIFF", "AAPT", p));
        append(s, chunk("curv", {0})); // Direct-style unknown chunk is not a grid curve.
        return form("RIFF", "AASY", s);
    }
    bool reader()
    {
        std::string error;
        auto a = curve(), b = a;
        b.type = 255;
        b.channel = 255;
        b.time_offset = 200;
        b.points.clear();
        auto data = file(payload({a, b, a}), true);
        ImaCurves out;
        CHECK(read_ima_curves(data, out, error));
        CHECK(out.chunks == 1 && out.curves.size() == 3 && out.points == 4 && out.records() == 7);
        CHECK(encode(out.curves[0]) == encode(a) && encode(out.curves[1]) == encode(b) &&
              encode(out.curves[2]) == encode(a));
        CHECK(out.curves[0].pattern_offset < out.curves[0].grid_offset &&
              out.curves[0].grid_offset < out.curves[0].chunk_offset);
        CHECK(out.curves[0].ordinal == 0 && out.curves[2].ordinal == 2 &&
              out.curves[0].chunk_offset == out.curves[2].chunk_offset);
        data.assign(data.size(), 0);
        CHECK(out.curves[0].points == a.points);
        CHECK(read_ima_curves(file(payload({})), out, error, 0) && out.chunks == 1 && out.curves.empty());
        return true;
    }
    bool malformed()
    {
        std::string error;
        auto good = payload({curve()});
        ImaCurves out;
        for (std::size_t n = 0; n < good.size(); ++n)
        {
            if (n == 4)
                continue; // A complete empty chunk is valid.
            CHECK(!read_ima_curves(file(Bytes(good.begin(), good.begin() + n)), out, error));
            CHECK(out.curves.empty() && out.chunks == 0 && out.points == 0 && !error.empty());
        }
        for (auto pos : {0, 2})
            for (auto value : {0, 6, 8, 11, 255})
            {
                auto bad = good;
                bad[pos] = std::uint8_t(value);
                CHECK(!read_ima_curves(file(bad), out, error));
            }
        auto bad = good;
        bad[11] = 255;
        bad[12] = 255;
        CHECK(!read_ima_curves(file(bad), out, error));
        CHECK(!read_ima_curves(file(good), out, error, 2));
        CHECK(read_ima_curves(file(good), out, error, 3) && out.records() == 3);
        auto late = good;
        append(late, {1, 2, 3});
        CHECK(!read_ima_curves(file(late), out, error) && out.curves.empty());
        const auto bytes = file(good);
        for (std::size_t n = 0; n < bytes.size(); ++n)
            CHECK(!read_ima_curves(std::span(bytes).first(n), out, error));
        // More than the hard bound fails even when the caller requests a larger budget.
        ImaCurve empty;
        auto many = payload(std::vector<ImaCurve>(65537, empty));
        CHECK(!read_ima_curves(file(many), out, error, 100000) && out.curves.empty());
        return true;
    }
    bool lifetime(const std::filesystem::path& root)
    {
        std::string error;
        std::error_code ec;
        std::filesystem::create_directories(root / "Music/Test", ec);
        CHECK(!ec);
        auto write = [&](Bytes b)
        {
            std::ofstream f(root / "Music/Test/init.sty", std::ios::binary);
            return bool(f.write(reinterpret_cast<const char*>(b.data()), b.size()));
        };
        const std::map<std::string, std::string> properties{{"MusicDirectory", "Music/Test"},
                                                            {"InstrumentFiles", "bank.dls init.sty"},
                                                            {"AmbientList", "a.sec"},
                                                            {"CruisingList", "c.sec"},
                                                            {"HarddrivingList", "h.sec"}};
        MusicCatalog catalog;
        CHECK(write(file(payload({curve()}))));
        catalog.inspect(root, properties);
        CHECK(catalog.assets.size() > 1 && catalog.assets[1].style && catalog.assets[1].curves &&
              catalog.assets[1].curves_error.empty());
        auto copy = catalog.assets[1].curves->curves[0];
        std::weak_ptr<const ImaCurves> weak = catalog.assets[1].curves;
        CHECK(write(file({0})));
        catalog.inspect(root, properties);
        CHECK(weak.expired() && catalog.assets[1].style && !catalog.assets[1].curves &&
              !catalog.assets[1].curves_error.empty());
        CHECK(copy.points == curve().points);
        CHECK(write(file(payload({curve()}))));
        catalog.inspect(root, properties);
        weak = catalog.assets[1].curves;
        CHECK(!weak.expired());
        catalog.inspect({}, {});
        CHECK(weak.expired());
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
        std::size_t chunks = 0, curves = 0, points = 0;
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto& path : files)
        {
            std::ifstream in(path, std::ios::binary);
            Bytes data{std::istreambuf_iterator<char>(in), {}};
            ImaCurves out;
            CHECK(read_ima_curves(data, out, error));
            chunks += out.chunks;
            curves += out.curves.size();
            points += out.points;
            for (std::size_t i = 0; i < out.curves.size();)
            {
                const auto offset = out.curves[i].chunk_offset;
                Bytes encoded;
                do
                {
                    auto bytes = encode(out.curves[i]);
                    append(encoded, bytes);
                    for (auto v : bytes)
                    {
                        hash ^= v;
                        hash *= 1099511628211ull;
                    }
                    ++i;
                } while (i < out.curves.size() && out.curves[i].chunk_offset == offset);
                const auto size = std::uint32_t(data[offset + 4]) | (std::uint32_t(data[offset + 5]) << 8) |
                                  (std::uint32_t(data[offset + 6]) << 16) | (std::uint32_t(data[offset + 7]) << 24);
                CHECK(encoded.size() + 4 == size &&
                      std::equal(encoded.begin(), encoded.end(), data.begin() + offset + 12));
            }
        }
        std::cout << "Private curves: " << files.size() << " styles / " << chunks << " chunks / " << curves
                  << " curves / " << points << " points; FNV64 " << hash << '\n';
        CHECK(files.size() == 39 && chunks == 42 && curves == 47 && points == 47 && hash == 4777673514653972974ull);
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    std::string_view mode = argv[1];
    return (mode == "reader"                  ? reader()
            : mode == "malformed"             ? malformed()
            : mode == "lifetime" && argc == 3 ? lifetime(argv[2])
            : mode == "private" && argc == 3  ? private_data(argv[2])
                                              : false)
               ? 0
               : 1;
}
