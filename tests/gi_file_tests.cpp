// GI lot G2: the .ob2gi container, its keys, freshness rules, naming and search order
// (gi_contracts.md). The private case keys every mounted retail world.
#include "engine/config.h"
#include "engine/gi_file.h"
#include "engine/resources.h"
#include "engine/user_directory.h"
#include "resource_fixture.h"
#include "scratch_dir.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

using namespace OpenBlood2;
namespace fs = std::filesystem;

#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " #x "\n";                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace
{
    Sha256Digest digest_of(std::string_view text)
    {
        return sha256({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
    }
    GiFile sample()
    {
        GiFile file;
        file.key.world = digest_of("world");
        file.key.albedo = digest_of("albedo");
        file.key.placement = digest_of("placement");
        file.key.edition = GiEdition::Nightmares;
        file.key.world_path = "worlds/01_subwaya.dat";
        file.settings = "bounces=2\nsamples=256\n";
        file.key.settings = digest_of(file.settings);
        file.sections.push_back({gi_format::lightmaps, {1, 2, 3, 4, 5}});
        file.sections.push_back({gi_format::probes, {}});
        return file;
    }
    bool same(const GiFile& a, const GiFile& b)
    {
        if (a.key.world != b.key.world || a.key.albedo != b.key.albedo || a.key.settings != b.key.settings ||
            a.key.placement != b.key.placement || a.key.algorithm != b.key.algorithm ||
            a.key.edition != b.key.edition || a.key.world_path != b.key.world_path || a.settings != b.settings ||
            a.sections.size() != b.sections.size())
            return false;
        for (std::size_t i = 0; i < a.sections.size(); ++i)
            if (a.sections[i].tag != b.sections[i].tag || a.sections[i].payload != b.sections[i].payload)
                return false;
        return true;
    }
    void put32(std::vector<std::uint8_t>& b, std::size_t at, std::uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            b[at + i] = std::uint8_t(v >> (8 * i));
    }
    // Recomputes the declared size and the SHA-256 trailer so a test reaches the check it targets.
    void reseal(std::vector<std::uint8_t>& b)
    {
        const std::uint64_t size = b.size();
        for (int i = 0; i < 8; ++i)
            b[16 + i] = std::uint8_t(size >> (8 * i));
        const auto trailer = sha256(std::span(b).first(b.size() - 32));
        std::copy(trailer.begin(), trailer.end(), b.end() - 32);
    }
    // Offset of the first section header (after the fixed header and the world path).
    std::size_t first_section(const std::vector<std::uint8_t>& b)
    {
        const std::size_t path = 8 + 4 + 4 + 8 + 128 + 4 + 4;
        return path + 4 + (std::size_t(b[path]) | std::size_t(b[path + 1]) << 8) + 4;
    }
    bool refuses(const std::vector<std::uint8_t>& bytes, std::string_view reason)
    {
        GiFile output = sample(), untouched = sample();
        std::string error;
        if (read_gi_file(bytes, output, error))
        {
            std::cerr << "accepted a file that should fail with: " << reason << '\n';
            return false;
        }
        if (error.find(reason) == std::string::npos || !same(output, untouched))
        {
            std::cerr << "expected '" << reason << "', got '" << error << "'\n";
            return false;
        }
        return true;
    }

    bool roundtrip()
    {
        const auto file = sample();
        std::vector<std::uint8_t> a, b;
        std::string error;
        CHECK(write_gi_file(file, a, error) && write_gi_file(file, b, error));
        CHECK(a == b); // Deterministic bytes.
        CHECK(std::memcmp(a.data(), "OB2GI\x1a\0\0", 8) == 0);
        GiFile read;
        CHECK(read_gi_file(a, read, error) && same(read, file));
        CHECK(read.find(gi_format::lightmaps) && read.find(gi_format::lightmaps)->payload.size() == 5);
        CHECK(read.find(gi_format::probes) && read.find(gi_format::reflections) == nullptr);
        std::vector<std::uint8_t> c;
        CHECK(write_gi_file(read, c, error) && c == a);
        // INFO alone (no optional section) and the largest settings text.
        GiFile minimal = file;
        minimal.sections.clear();
        minimal.settings.assign(gi_format::max_settings, 'x');
        minimal.key.settings = digest_of(minimal.settings);
        CHECK(write_gi_file(minimal, c, error) && read_gi_file(c, read, error) && same(read, minimal));
        CHECK(gi_tag_name(gi_format::vertices) == "VTX" && gi_tag_name(0x01020304) == "????");
        return true;
    }

    bool write_refusals()
    {
        std::vector<std::uint8_t> bytes{9};
        std::string error;
        auto refused = [&](GiFile file, std::string_view reason)
        {
            const bool ok = !write_gi_file(file, bytes, error) && error.find(reason) != std::string::npos &&
                            bytes == std::vector<std::uint8_t>{9};
            if (!ok)
                std::cerr << "write: expected '" << reason << "', got '" << error << "'\n";
            return ok;
        };
        auto file = sample();
        file.key.settings[0] ^= 1;
        CHECK(refused(file, "settings hash"));
        file = sample();
        file.settings.assign(gi_format::max_settings + 1, 'x');
        file.key.settings = digest_of(file.settings);
        CHECK(refused(file, "settings text"));
        file = sample();
        file.settings = std::string("a\0b", 3);
        file.key.settings = digest_of(file.settings);
        CHECK(refused(file, "settings text"));
        for (const auto* path : {"", "Worlds\\01_SubwayA.dat", "worlds/01_subwaya.dtx", "../x.dat"})
        {
            file = sample();
            file.key.world_path = path;
            CHECK(refused(file, "world path"));
        }
        file = sample();
        file.key.world_path = "worlds/" + std::string(122, 'a') + ".dat"; // 133 bytes.
        CHECK(refused(file, "world path"));
        file = sample();
        file.sections = {{gi_format::probes, {}}, {gi_format::lightmaps, {}}};
        CHECK(refused(file, "out of order"));
        file.sections = {{gi_format::probes, {}}, {gi_format::probes, {}}};
        CHECK(refused(file, "duplicate"));
        file.sections = {{gi_format::info, {}}};
        CHECK(refused(file, "INFO"));
        file.sections = {{gi_format::tag('X', 'Y', 'Z', 'W'), {}}};
        CHECK(refused(file, "XYZW"));
        return true;
    }

    bool read_refusals()
    {
        std::vector<std::uint8_t> good;
        std::string error;
        CHECK(write_gi_file(sample(), good, error));
        const std::size_t section = first_section(good);
        auto bytes = good;
        bytes[0] = 'X';
        CHECK(refuses(bytes, "not an .ob2gi file"));
        CHECK(refuses(std::vector<std::uint8_t>(good.begin(), good.begin() + 40), "not an .ob2gi file"));
        bytes = good;
        put32(bytes, 8, 2);
        reseal(bytes);
        CHECK(refuses(bytes, "unsupported version 2"));
        bytes = good;
        put32(bytes, 12, 0x04030201);
        reseal(bytes);
        CHECK(refuses(bytes, "byte order"));
        bytes = good;
        bytes.pop_back();
        CHECK(refuses(bytes, "declared size"));
        bytes = good;
        bytes.push_back(0);
        CHECK(refuses(bytes, "declared size"));
        bytes = good;
        bytes[bytes.size() - 1] ^= 1;
        CHECK(refuses(bytes, "SHA-256 trailer"));
        bytes = good;
        bytes[section + 30] ^= 1; // Inside the INFO payload: CRC first, the trailer is resealed.
        reseal(bytes);
        CHECK(refuses(bytes, "CRC-32"));
        bytes = good;
        bytes[24] ^= 1; // World digest without resealing.
        CHECK(refuses(bytes, "SHA-256 trailer"));
        // Settings text and its hash disagree (both CRC and trailer valid).
        auto file = sample();
        std::vector<std::uint8_t> other;
        file.key.settings = digest_of("something else");
        file.settings = "something else";
        CHECK(write_gi_file(file, other, error));
        std::copy(good.begin() + 8 + 4 + 4 + 8 + 64, good.begin() + 8 + 4 + 4 + 8 + 96, other.begin() + 88);
        reseal(other);
        CHECK(refuses(other, "settings hash does not match INFO"));
        // Unknown edition and nonzero reserved bytes.
        bytes = good;
        bytes[156] = 3;
        reseal(bytes);
        CHECK(refuses(bytes, "unknown edition 3"));
        bytes = good;
        bytes[158] = 1;
        reseal(bytes);
        CHECK(refuses(bytes, "reserved"));
        // A world path that is not normalized.
        bytes = good;
        bytes[164] = 'W';
        reseal(bytes);
        CHECK(refuses(bytes, "world path"));
        // Section count, unknown tag, nonzero flags and section order.
        bytes = good;
        put32(bytes, section - 4, 0);
        reseal(bytes);
        CHECK(refuses(bytes, "section count 0"));
        bytes = good;
        put32(bytes, section - 4, 8); // One more than the known tags (seven since lot GD2's DIR).
        reseal(bytes);
        CHECK(refuses(bytes, "section count 8"));
        bytes = good;
        put32(bytes, section, gi_format::tag('Z', 'Z', 'Z', 'Z'));
        reseal(bytes);
        CHECK(refuses(bytes, "unknown section ZZZZ"));
        bytes = good;
        put32(bytes, section, gi_format::lightmaps); // INFO not first.
        reseal(bytes);
        CHECK(refuses(bytes, "out of order"));
        bytes = good;
        put32(bytes, section + 4, 1);
        reseal(bytes);
        CHECK(refuses(bytes, "nonzero flags"));
        // A section length past the body, and trailing bytes after the last section.
        bytes = good;
        put32(bytes, section + 8, 0x7fffffff);
        reseal(bytes);
        CHECK(refuses(bytes, "truncated"));
        bytes = good;
        bytes.insert(bytes.end() - 32, {0, 0, 0});
        reseal(bytes);
        CHECK(refuses(bytes, "3 trailing bytes"));
        // One extra (fourth) section declared but absent.
        bytes = good;
        put32(bytes, section - 4, 4);
        reseal(bytes);
        CHECK(refuses(bytes, "truncated section header"));
        return true;
    }

    // Deterministic mutation fuzzing: every corruption is refused (or round-trips), never a crash.
    bool fuzz()
    {
        std::vector<std::uint8_t> good;
        std::string error;
        CHECK(write_gi_file(sample(), good, error));
        std::uint32_t state = 0x6f6232u;
        auto next = [&]
        {
            state = state * 1664525u + 1013904223u;
            return state;
        };
        for (int round = 0; round < 4000; ++round)
        {
            auto bytes = good;
            const int flips = 1 + int(next() % 4);
            for (int i = 0; i < flips; ++i)
                bytes[next() % bytes.size()] ^= std::uint8_t(1 + next() % 255);
            if (round % 2)
                reseal(bytes); // Reach the structural checks behind the trailer.
            if (round % 7 == 0)
                bytes.resize(next() % bytes.size());
            GiFile read;
            if (read_gi_file(bytes, read, error))
            {
                std::vector<std::uint8_t> again;
                CHECK(write_gi_file(read, again, error) && again == bytes);
            }
        }
        return true;
    }

    bool status()
    {
        const auto file = sample();
        GiExpectation expected;
        expected.world = file.key.world;
        expected.albedo = file.key.albedo;
        expected.edition = file.key.edition;
        unsigned stale = 99;
        CHECK(gi_status(file.key, expected, stale) == GiStatus::Ready && stale == 0);
        expected.settings = digest_of("other settings");
        CHECK(gi_status(file.key, expected, stale) == GiStatus::Stale && stale == gi_stale_settings);
        expected.settings = file.key.settings;
        expected.placement = digest_of("other placement");
        expected.albedo = digest_of("other albedo");
        expected.algorithm = gi_algorithm_version + 1;
        expected.edition = GiEdition::Retail;
        CHECK(gi_status(file.key, expected, stale) == GiStatus::Stale &&
              stale == (gi_stale_albedo | gi_stale_algorithm | gi_stale_edition | gi_stale_placement));
        CHECK(gi_stale_text(stale) == "albedo, algorithm, edition, placement");
        expected.world = digest_of("another world");
        CHECK(gi_status(file.key, expected, stale) == GiStatus::Foreign && stale == 0);
        CHECK(std::string_view(gi_status_name(GiStatus::Missing)) == "missing" &&
              std::string_view(gi_edition_name(GiEdition::Demo)) == "demo");
        return true;
    }

    bool naming()
    {
        const auto world = digest_of("world");
        std::string name;
        CHECK(gi_file_name("Worlds\\01_SubwayA.dat", world, name));
        CHECK(name == "worlds/01_subwaya." + digest_text(world).substr(0, 12) + ".ob2gi");
        CHECK(gi_file_name("worlds_ao/multi/bb_x.DAT", world, name) && name.starts_with("worlds_ao/multi/bb_x."));
        CHECK(!gi_file_name("worlds/01_subwaya.dtx", world, name));
        CHECK(!gi_file_name("", world, name));
        CHECK(!gi_file_name("worlds/" + std::string(122, 'a') + ".dat", world, name));
        return true;
    }

    bool write_bytes(const fs::path& path, std::span<const std::uint8_t> bytes)
    {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
        out.close();
        return !out.fail();
    }

    bool search(const fs::path& root)
    {
        CHECK(Testing::reset_scratch_dir(root));
        // Order: --gi-dir, then the (isolated) user directory, then the packaged directory.
        const auto roots = gi_search_roots(root / "configured");
        fs::path user;
        std::string error;
        CHECK(user_directory(user, error));
        CHECK(roots.size() >= 3 && roots[0].source == "--gi-dir" && roots[0].path == path_text(root / "configured"));
        CHECK(roots[1].source == "user" && roots[1].path == path_text(user / "gi"));
        CHECK(roots.back().source == "packaged");
        CHECK(gi_search_roots({}).front().source == "user");

        const auto file = sample();
        GiExpectation expected;
        expected.world = file.key.world;
        expected.albedo = file.key.albedo;
        expected.edition = file.key.edition;
        std::string name;
        CHECK(gi_file_name(file.key.world_path, expected.world, name));
        const std::vector<GiRoot> test_roots{{"a", path_text(root / "a")}, {"b", path_text(root / "b")},
                                             {"c", path_text(root / "c")}, {"d", path_text(root / "d")}};
        GiLookup lookup;
        CHECK(find_gi_file(test_roots, file.key.world_path, expected, lookup, error));
        CHECK(lookup.candidates.size() == 4 && !lookup.file && lookup.chosen == -1);
        for (const auto& candidate : lookup.candidates)
            CHECK(candidate.status == GiStatus::Missing && candidate.detail.empty());

        // a: invalid, b: stale, c: ready, d: ready (shadowed by c).
        std::vector<std::uint8_t> bytes;
        CHECK(write_bytes(root / "a" / name, std::vector<std::uint8_t>{1, 2, 3}));
        auto stale = file;
        stale.key.albedo = digest_of("old albedo");
        CHECK(write_gi_file(stale, bytes, error) && write_bytes(root / "b" / name, bytes));
        CHECK(save_gi_file(root / "c" / name, file, error));
        CHECK(save_gi_file(root / "d" / name, file, error));
        CHECK(find_gi_file(test_roots, "Worlds\\01_SubwayA.dat", expected, lookup, error));
        CHECK(lookup.candidates[0].status == GiStatus::Failed && !lookup.candidates[0].detail.empty());
        CHECK(lookup.candidates[1].status == GiStatus::Stale && lookup.candidates[1].stale == gi_stale_albedo);
        CHECK(lookup.candidates[2].status == GiStatus::Ready && lookup.candidates[3].status == GiStatus::Ready);
        CHECK(lookup.chosen == 2 && lookup.file && same(*lookup.file, file));
        CHECK(lookup.candidates[2].path == path_text(root / "c") + "/" + name);

        // A file for another world copied under this world's name is foreign, never applied.
        auto other = file;
        other.key.world_path = "worlds/02_cityhub.dat";
        CHECK(save_gi_file(root / "c" / name, other, error));
        CHECK(find_gi_file(test_roots, file.key.world_path, expected, lookup, error));
        CHECK(lookup.candidates[2].status == GiStatus::Foreign && lookup.chosen == 3);
        CHECK(!find_gi_file(test_roots, "worlds/x.dtx", expected, lookup, error) && !error.empty());

        // Atomic save: a refused write keeps the previous file.
        auto broken = file;
        broken.key.settings[0] ^= 1;
        CHECK(!save_gi_file(root / "d" / name, broken, error));
        CHECK(read_gi_bytes(path_text(root / "d" / name), bytes, error));
        GiFile read;
        CHECK(read_gi_file(bytes, read, error) && same(read, file));
        return Testing::remove_scratch_dir(root);
    }

    // Albedo key over a synthetic REZ: sorted, case-normalized, absent textures keyed, and
    // sensitive to the providing entry.
    bool write_rez(const fs::path& path, const std::map<std::string, std::vector<std::uint8_t>>& files)
    {
        test_resource::Directory dir;
        for (const auto& [name, bytes] : files)
            dir.children["tex"].files[name] = bytes;
        test_resource::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13; // REZ v1 header line breaks, EOF marker and version.
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        const auto [offset, size] = test_resource::write_dir(w, dir);
        w.patch(131, offset);
        w.patch(135, size);
        return write_bytes(path, w.b);
    }
    bool albedo(const fs::path& root)
    {
        CHECK(Testing::reset_scratch_dir(root));
        World world;
        world.models.resize(2);
        world.models[0].textures = {"Tex\\A.dtx", "tex/b.dtx", ""};
        world.models[1].textures = {"tex/missing.dtx", "tex/a.dtx"};
        auto key = [&](const World& w, const fs::path& rez, Sha256Digest& out)
        {
            Resources resources;
            std::string error;
            const bool ok = resources.mount(rez, error) && gi_albedo_key(w, resources, out, error);
            if (!ok)
                std::cerr << error << '\n';
            return ok;
        };
        CHECK(write_rez(root / "one.rez", {{"a.dtx", {1, 2, 3}}, {"b.dtx", {4, 5}}}));
        Sha256Digest first{}, again{}, changed{};
        CHECK(key(world, root / "one.rez", first) && key(world, root / "one.rez", again) && first == again);
        World reordered = world;
        std::swap(reordered.models[0], reordered.models[1]);
        CHECK(key(reordered, root / "one.rez", again) && again == first); // Order-free.
        CHECK(write_rez(root / "two.rez", {{"a.dtx", {1, 2, 3, 4}}, {"b.dtx", {4, 5}}}));
        CHECK(key(world, root / "two.rez", changed) && changed != first); // Entry size changed.
        CHECK(write_rez(root / "three.rez", {{"a.dtx", {1, 2, 3}}, {"b.dtx", {4, 5}}, {"missing.dtx", {6}}}));
        CHECK(key(world, root / "three.rez", changed) && changed != first); // Absent became present.
        World fewer = world;
        fewer.models[1].textures = {"tex/a.dtx"};
        CHECK(key(fewer, root / "one.rez", changed) && changed != first); // Texture set changed.
        World invalid = world;
        invalid.models[0].textures.push_back("../escape.dtx");
        Resources resources;
        std::string error;
        CHECK(resources.mount(root / "one.rez", error) && !gi_albedo_key(invalid, resources, changed, error));
        CHECK(error.find("escape") != std::string::npos);
        return Testing::remove_scratch_dir(root);
    }

    // Private: every mounted world gets an expectation and a name; keys are deterministic, and a
    // file baked from them is Ready, then Stale once the albedo key moves.
    bool retail(const fs::path& data, const fs::path& scratch)
    {
        Config config;
        config.data_dir = data;
        Resources resources;
        std::string error;
        CHECK(resources.mount_game(config, error));
        std::size_t worlds = 0;
        for (const auto& [path, resource] : resources.index())
        {
            if (!path.starts_with("worlds/") || !path.ends_with(".dat"))
                continue;
            GiExpectation expected, again;
            if (!gi_expectation(resources, path, expected, error))
            {
                std::cerr << path << ": " << error << '\n';
                return false;
            }
            CHECK(gi_expectation(resources, path, again, error));
            CHECK(again.world == expected.world && again.albedo == expected.albedo);
            CHECK(expected.edition == GiEdition::Retail && expected.albedo != Sha256Digest{});
            std::string name;
            CHECK(gi_file_name(path, expected.world, name));
            std::cout << path << ' ' << digest_text(expected.world).substr(0, 12) << " albedo "
                      << digest_text(expected.albedo).substr(0, 12) << '\n';
            ++worlds;
        }
        CHECK(worlds == 58);
        CHECK(Testing::reset_scratch_dir(scratch));
        GiExpectation expected;
        CHECK(gi_expectation(resources, "Worlds\\01_SubwayA.dat", expected, error));
        GiFile file;
        file.key.world = expected.world;
        file.key.albedo = expected.albedo;
        file.key.world_path = "worlds/01_subwaya.dat";
        file.key.settings = digest_of(file.settings);
        std::string name;
        CHECK(gi_file_name(file.key.world_path, expected.world, name));
        CHECK(save_gi_file(scratch / name, file, error));
        const std::vector<GiRoot> roots{{"test", path_text(scratch)}};
        GiLookup lookup;
        CHECK(find_gi_file(roots, file.key.world_path, expected, lookup, error) && lookup.chosen == 0);
        file.key.albedo[0] ^= 1;
        CHECK(save_gi_file(scratch / name, file, error));
        CHECK(find_gi_file(roots, file.key.world_path, expected, lookup, error) && lookup.chosen == -1 &&
              lookup.candidates[0].status == GiStatus::Stale && lookup.candidates[0].stale == gi_stale_albedo);
        return Testing::remove_scratch_dir(scratch);
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view name(argv[1]);
    const fs::path arg = argc > 2 ? path_from_utf8(argv[2]) : fs::path{};
    bool ok = false;
    if (name == "roundtrip")
        ok = roundtrip();
    else if (name == "write_refusals")
        ok = write_refusals();
    else if (name == "read_refusals")
        ok = read_refusals();
    else if (name == "fuzz")
        ok = fuzz();
    else if (name == "status")
        ok = status();
    else if (name == "naming")
        ok = naming();
    else if (name == "search" && argc == 3)
        ok = search(arg);
    else if (name == "albedo" && argc == 3)
        ok = albedo(arg);
    else if (name == "retail" && argc == 4)
        ok = retail(arg, path_from_utf8(argv[3]));
    else
        return 2;
    return ok ? 0 : 1;
}
