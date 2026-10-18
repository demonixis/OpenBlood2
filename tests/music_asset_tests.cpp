#include "engine/music_asset.h"
#include "game/original/Music.h"
#include "game/server_adapter.h"
#include <algorithm>
#include <fstream>
#include <iostream>
using namespace OpenBlood2;
namespace fs = std::filesystem;
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
    using Bytes = std::vector<std::uint8_t>;
    void u32(Bytes& bytes, std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
    Bytes chunk(std::string_view tag, const Bytes& payload)
    {
        Bytes b(tag.begin(), tag.end());
        u32(b, payload.size());
        b.insert(b.end(), payload.begin(), payload.end());
        if (payload.size() & 1)
            b.push_back(0);
        return b;
    }
    Bytes text(std::string_view value)
    {
        Bytes b;
        for (auto c : value)
        {
            b.push_back(c);
            b.push_back(0);
        }
        b.push_back(0);
        b.push_back(0);
        return b;
    }
    Bytes container(std::string_view form, const Bytes& children, std::string_view tag = "RIFF")
    {
        Bytes b(form.begin(), form.end());
        b.insert(b.end(), children.begin(), children.end());
        return chunk(tag, b);
    }
    Bytes section(std::string_view style = "Shared")
    {
        return container("AASE", chunk("sref", text(style)));
    }
    bool reader()
    {
        std::string error;
        MusicAssetInfo info;
        auto bytes = section();
        CHECK(read_music_asset(bytes, MusicAssetKind::Section, info, error));
        CHECK(info.chunks.size() == 2 && info.chunks[1].offset == 12 && info.chunks[1].depth == 1 &&
              info.style_reference == "Shared");
        bytes.assign(bytes.size(), 0);
        CHECK(info.chunks[0].form == "AASE" && info.style_reference == "Shared");
        bytes = section();
        for (std::size_t size = 0; size < bytes.size(); ++size)
        {
            CHECK(!read_music_asset(std::span(bytes).first(size), MusicAssetKind::Section, info, error));
            CHECK(info.chunks.empty() && info.style_reference.empty() && !error.empty());
        }
        CHECK(!read_music_asset(bytes, MusicAssetKind::Style, info, error));
        auto bad = bytes;
        bad[16] = 255;
        CHECK(!read_music_asset(bad, MusicAssetKind::Section, info, error));
        bad = bytes;
        bad.push_back(0);
        CHECK(!read_music_asset(bad, MusicAssetKind::Section, info, error));
        Bytes refs = chunk("sref", text("a")), duplicate = refs;
        refs.insert(refs.end(), duplicate.begin(), duplicate.end());
        CHECK(!read_music_asset(container("AASE", refs), MusicAssetKind::Section, info, error));
        for (auto name : {"", "../outside", "bad/name", "bad\\name", "..", "/bad", "c:bad"})
            CHECK(!read_music_asset(section(name), MusicAssetKind::Section, info, error));
        for (Bytes s : {Bytes{65, 0}, Bytes{65, 0, 0}, Bytes{65, 1, 0, 0}, Bytes{65, 0, 0, 0, 0, 0}})
            CHECK(!read_music_asset(container("AASE", chunk("sref", s)), MusicAssetKind::Section, info, error));
        CHECK(!read_music_asset(container("AASE", {}), MusicAssetKind::Section, info, error));
        auto unknown = chunk("unkn", {1, 2, 3});
        CHECK(read_music_asset(container("AASY", unknown), MusicAssetKind::Style, info, error));
        CHECK(info.chunks.size() == 2 && info.chunks[1].size == 3);
        unknown.pop_back();
        CHECK(!read_music_asset(container("AASY", unknown), MusicAssetKind::Style, info, error));
        auto nested = chunk("note", {});
        for (unsigned i = 0; i < 17; ++i)
            nested = container("TEST", nested, "LIST");
        CHECK(!read_music_asset(container("AASY", nested), MusicAssetKind::Style, info, error));
        Bytes many;
        for (unsigned i = 0; i < 8192; ++i)
        {
            const auto part = chunk("note", {});
            many.insert(many.end(), part.begin(), part.end());
        }
        CHECK(!read_music_asset(container("AASY", many), MusicAssetKind::Style, info, error));
        CHECK(read_music_asset(container("DLS ", container("wvpl", {}, "LIST")), MusicAssetKind::Instruments, info,
                               error));
        std::cout << "Reader: exact ranges, padding, recursion/count limits, references and owned output verified\n";
        return true;
    }
    std::map<std::string, std::string> properties()
    {
        return {{"MusicDirectory", "Music\\Test"},
                {"InstrumentFiles", "bank.dls init.sty"},
                {"AmbientList", "a1.sec a2.sec"},
                {"CruisingList", "c1.sec"},
                {"HarddrivingList", "h1.sec"}};
    }
    bool plan_test()
    {
        std::string error;
        MusicAssetPlan plan;
        auto props = properties();
        CHECK(CMusic::BuildAssetPlan(props, plan, error));
        CHECK(plan.directory == "music/test" && plan.requests.size() == 19);
        const std::vector<std::string> expected{"bank.dls", "init.sty", "hta.sec", "cta.sec", "ats.sec",
                                                "sta.sec",  "htc.sec",  "atc.sec", "cts.sec", "stc.sec",
                                                "cth.sec",  "ath.sec",  "cts.sec", "sth.sec", "s.sec",
                                                "a1.sec",   "a2.sec",   "c1.sec",  "h1.sec"};
        for (std::size_t i = 0; i < expected.size(); ++i)
            CHECK(plan.requests[i].filename == expected[i]);
        props["AmbientList"] = "\"a space.sec\"; ignored \"malformed tail";
        CHECK(CMusic::BuildAssetPlan(props, plan, error));
        CHECK(plan.requests[15].filename == "a space.sec");
        props["AmbientList"] = "";
        CHECK(CMusic::BuildAssetPlan(props, plan, error) && plan.requests.size() == 17);
        for (const auto& [key, value] : properties())
        {
            auto missing = properties();
            missing.erase(key);
            CHECK(!CMusic::BuildAssetPlan(missing, plan, error));
            CHECK(plan.requests.empty() && plan.directory.empty());
        }
        for (auto value : {"../escape", "a.sec \"open", "x.sty", "dir/a.sec"})
        {
            props = properties();
            props["AmbientList"] = value;
            CHECK(!CMusic::BuildAssetPlan(props, plan, error));
        }
        props = properties();
        props["InstrumentFiles"] = "bank.dls init.sty extra";
        CHECK(!CMusic::BuildAssetPlan(props, plan, error));
        props = properties();
        props["MusicDirectory"] = "../Music";
        CHECK(!CMusic::BuildAssetPlan(props, plan, error));
        props = properties();
        props["AmbientList"] = std::string(80, 'a') + ".sec";
        CHECK(!CMusic::BuildAssetPlan(props, plan, error));
        std::cout << "Original plan: reverse transition loading, repeated cts.sec, playlist order and first-command "
                     "parsing verified\n";
        return true;
    }
    bool write(const fs::path& file, const Bytes& bytes)
    {
        std::ofstream out(file, std::ios::binary);
        return bool(out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    }
    bool catalog_test(const fs::path& root)
    {
        std::string error;
        std::error_code ec;
        fs::remove_all(root, ec);
        CHECK(!ec);
        fs::create_directories(root / "Music/Test", ec);
        CHECK(!ec);
        auto props = properties();
        MusicAssetPlan plan;
        CHECK(CMusic::BuildAssetPlan(props, plan, error));
        for (const auto& request : plan.requests)
            CHECK(write(root / "Music/Test" / request.filename,
                        request.kind == MusicAssetKind::Section
                            ? section()
                            : container(request.kind == MusicAssetKind::Style ? "AASY" : "DLS ", {})));
        CHECK(write(root / "Music/Test/SHARED.STY", container("AASY", {})));
        MusicCatalog catalog;
        catalog.inspect(root, props);
        CHECK(catalog.error.empty() && catalog.assets.size() == 19);
        CHECK(std::all_of(catalog.assets.begin(), catalog.assets.end(),
                          [](const auto& a) { return a.container_valid && a.error.empty(); }));
        for (const auto& asset : catalog.assets)
            if (asset.kind == MusicAssetKind::Section)
                CHECK(asset.container_valid && !asset.section && !asset.section_error.empty());
        CHECK(!catalog.assets.front().bank && !catalog.assets.front().bank_error.empty());
        CHECK(!catalog.assets[1].style && !catalog.assets[1].style_error.empty()); // Structural-only fake style.

        CHECK(catalog.assets[1].bands && catalog.assets[1].bands->bands.empty() &&
              catalog.assets[1].bands_error.empty());
        const auto init_file = root / "Music/Test" / catalog.assets[1].filename;
        CHECK(write(init_file, container("AASY", container("AABN", {}))));
        catalog.inspect(root, props);
        CHECK(catalog.assets[1].container_valid && !catalog.assets[1].bands && !catalog.assets[1].bands_error.empty());
        CHECK(write(init_file, container("AASY", {})));
        catalog.inspect(root, props);
        const auto initial = catalog.bytes_read;
        catalog.inspect(root, props);
        CHECK(catalog.bytes_read == initial);
        fs::remove(root / "Music/Test/a1.sec", ec);
        CHECK(!ec);
        catalog.inspect(root, props);
        CHECK(std::count_if(catalog.assets.begin(), catalog.assets.end(),
                            [](const auto& a) { return !a.container_valid; }) == 1);
        CHECK(write(root / "Music/Test/shared.sty", container("AASY", {})));
        const bool same_file = fs::equivalent(root / "Music/Test/shared.sty", root / "Music/Test/SHARED.STY", ec);
        CHECK(!ec);
        catalog.inspect(root, props);
        if (!same_file)
            CHECK(catalog.assets.back().error.starts_with("Ambiguous"));
        else
            std::cout << "Case ambiguity fixture unavailable on case-insensitive filesystem\n";
        fs::remove(root / "Music/Test/shared.sty", ec);
        CHECK(!ec);
        fs::remove(root / "Music/Test/SHARED.STY", ec);
        CHECK(!ec);
        CHECK(write(root / "outside.sty", container("AASY", {})));
        fs::create_symlink(root / "outside.sty", root / "Music/Test/shared.sty", ec);
        CHECK(!ec);
        catalog.inspect(root, props);
        CHECK(!catalog.assets.back().container_valid &&
              catalog.assets.back().error.find("symlinked") != std::string::npos);
        fs::remove(root / "Music/Test/shared.sty", ec);
        CHECK(!ec);
        CHECK(write(root / "Music/Test/shared.sty", {}));
        fs::resize_file(root / "Music/Test/shared.sty", max_music_asset_bytes + 1, ec);
        CHECK(!ec);
        catalog.inspect(root, props);
        CHECK(catalog.assets.back().error.find("budget") != std::string::npos);
        CHECK(catalog.bytes_read < initial);
        Bytes sh(94), sc(120);
        sh[32] = 4;
        sh[34] = 4;
        sh[46] = 7;
        sc[36] = 120;
        sc[40] = 1;
        sc[43] = 3;
        sc[44] = 192;
        sc[56] = 7;
        std::string personality_label = "Silence";
        auto complete_section = [&]
        {
            auto parts = chunk("sref", text("Shared"));
            const auto header = chunk("secn", sc);
            parts.insert(parts.end(), header.begin(), header.end());
            const auto label = chunk("prnm", text(personality_label));
            parts.insert(parts.end(), label.begin(), label.end());
            return container("AASE", parts);
        };
        CHECK(write(root / "Music/Test/a1.sec", complete_section()));
        auto find_section = [&]() -> const MusicAssetRecord*
        {
            for (const auto& a : catalog.assets)
                if (a.filename == "a1.sec")
                    return &a;
            return nullptr;
        };
        // The still-oversized style cannot yield a successful timeline.
        catalog.inspect(root, props);
        CHECK(find_section() && find_section()->section && !find_section()->timeline &&
              !find_section()->timeline_error.empty());
        fs::remove(root / "Music/Test/shared.sty", ec);
        CHECK(!ec);
        catalog.inspect(root, props);
        CHECK(!find_section()->timeline && !find_section()->timeline_error.empty());
        CHECK(write(root / "Music/Test/SHARED.STY", container("AASY", chunk("styl", sh))));
        catalog.inspect(root, props);
        CHECK(find_section()->timeline && find_section()->section_style && find_section()->timeline_error.empty() &&
              find_section()->timeline->mutes.size() == 4);
        const std::weak_ptr<const ImaTimeline> old_timeline = find_section()->timeline;
        const std::weak_ptr<const ImaStyle> old_bound_style = find_section()->section_style;
        sh[46] = 8;
        CHECK(write(root / "Music/Test/SHARED.STY", container("AASY", chunk("styl", sh))));
        catalog.inspect(root, props);
        CHECK(old_timeline.expired() && old_bound_style.expired() && !find_section()->section_style &&
              !find_section()->timeline && find_section()->timeline_error.find("disagree") != std::string::npos);
        sc[56] = 0;
        CHECK(write(root / "Music/Test/a1.sec", complete_section()));
        catalog.inspect(root, props);
        CHECK(find_section()->timeline); // Explicit zero identity: exact filename profile.
        CHECK(!find_section()->harmony && !find_section()->harmony_error.empty());
        const auto be = [](Bytes& b, std::size_t pos, std::uint32_t v)
        {
            for (int i = 0; i < 4; ++i)
                b[pos + i] = static_cast<std::uint8_t>(v >> (24 - 8 * i));
        };
        Bytes per(1176);
        std::copy_n("REPs", 4, per.begin());
        be(per, 4, per.size() - 8);
        be(per, 8, 1164);
        std::copy_n("Silence", 7, per.begin() + 32);
        per[28] = 0x91;
        CHECK(write(root / "Music/Test/SILENCE.PER", per));
        catalog.inspect(root, props);
        CHECK(find_section()->harmony && find_section()->harmony_error.empty() && catalog.personalities.size() == 1);
        CHECK(find_section()->harmony->base.scale_mask == 0x91);
        const std::weak_ptr<const ImaPersonality> old_personality = catalog.personalities.front().metadata;
        const std::weak_ptr<const ImaHarmony> old_harmony = find_section()->harmony;
        per[32] = 'X';
        CHECK(write(root / "Music/Test/SILENCE.PER", per));
        catalog.inspect(root, props);
        CHECK(old_personality.expired() && old_harmony.expired() && !find_section()->harmony &&
              find_section()->harmony_error.find("disagree") != std::string::npos);
        per[32] = 'S';
        CHECK(write(root / "Music/Test/SILENCE.PER", per));
        sc[72] = 1;
        CHECK(write(root / "Music/Test/a1.sec", complete_section()));
        catalog.inspect(root, props);
        CHECK(!find_section()->harmony && find_section()->harmony_error.find("identity") != std::string::npos);
        sc[72] = 0;
        for (const auto* invalid : {"../Silence", "sub/Silence", "sub\\Silence", ""})
        {
            personality_label = invalid;
            CHECK(write(root / "Music/Test/a1.sec", complete_section()));
            catalog.inspect(root, props);
            CHECK(!find_section()->harmony && !find_section()->harmony_error.empty());
        }
        personality_label = "Silence";
        CHECK(write(root / "Music/Test/a1.sec", complete_section()));
        CHECK(write(root / "Music/Test/silence.per", per));
        catalog.inspect(root, props);
        CHECK(!find_section()->harmony && catalog.personalities.front().error.find("Ambiguous") != std::string::npos);
        fs::remove(root / "Music/Test/silence.per", ec);
        CHECK(!ec);
        // An opaque child length escaping its envelope rejects metadata, even with valid header strings.
        per.insert(per.end(), {'N', 'E', 'C', 's', 0, 0, 0, 1});
        be(per, 4, per.size() - 8);
        CHECK(write(root / "Music/Test/SILENCE.PER", per));
        catalog.inspect(root, props);
        CHECK(!find_section()->harmony && !catalog.personalities.front().metadata &&
              !catalog.personalities.front().error.empty());
        per.resize(1176);
        be(per, 4, per.size() - 8);
        CHECK(write(root / "Music/Test/SILENCE.PER", per));
        sc[40] = 0;
        sc[41] = 8; // 2048 measures -> 8192 mutes per section; 32768 survey limit.
        for (const auto& r : plan.requests)
            if (r.kind == MusicAssetKind::Section)
                CHECK(write(root / "Music/Test" / r.filename, complete_section()));
        catalog.inspect(root, props);
        std::size_t prepared = 0, limited = 0, total = 0;
        for (const auto& a : catalog.assets)
            if (a.section)
            {
                if (a.timeline)
                {
                    ++prepared;
                    total += a.timeline->records();
                }
                else
                {
                    CHECK(a.timeline_error.find("budget") != std::string::npos);
                    ++limited;
                }
            }
        CHECK(prepared == 4 && limited == 12 && total == 32768);
        catalog.inspect(root, {});
        CHECK(!catalog.error.empty() && catalog.assets.empty() && catalog.bytes_read == 0);
        std::cout
            << "Catalog: deduplicated reads, explicit missing/ambiguous/symlink/oversize failures and reset verified\n";
        return true;
    }
    bool private_test(const fs::path& root)
    {
        std::string error;
        std::size_t sections = 0, styles = 0, banks = 0, chunks = 0, bytes_count = 0;
        for (const auto& file : fs::recursive_directory_iterator(root / "Music"))
        {
            auto ext = file.path().extension().string();
            for (auto& c : ext)
                if (c >= 'A' && c <= 'Z')
                    c += 'a' - 'A';
            if (ext != ".sec" && ext != ".sty" && ext != ".dls")
                continue;
            std::ifstream in(file.path(), std::ios::binary);
            Bytes bytes{std::istreambuf_iterator<char>(in), {}};
            MusicAssetInfo info;
            if (!read_music_asset(bytes,
                                  ext == ".sec"   ? MusicAssetKind::Section
                                  : ext == ".sty" ? MusicAssetKind::Style
                                                  : MusicAssetKind::Instruments,
                                  info, error))
                std::cerr << file.path() << '\n';
            CHECK(error.empty());
            sections += ext == ".sec";
            styles += ext == ".sty";
            banks += ext == ".dls";
            chunks += info.chunks.size();
            bytes_count += bytes.size();
        }
        CHECK(sections == 156 && styles == 39 && banks == 5);
        std::cout << "Private music containers: " << sections << " sections / " << styles << " styles / " << banks
                  << " banks / " << chunks << " chunks / " << bytes_count << " bytes\n";
        GameSession game;
        CHECK(game.initialize() && game.resources.mount_game({root, "test"}, error));
        CHECK(game.resources.content_root() == root);
        Resources copied = game.resources;
        CHECK(!game.resources.mount_game({root / "absent", "test"}, error));
        CHECK(game.resources.content_root() == root && copied.content_root() == root);
        for (const char* world : {"01_SubwayA", "02_CityHub"})
        {
            CHECK(game.request_world(world, error));
            const auto& catalog = game.music_catalog;
            std::cout << world << " music directory " << catalog.plan.directory
                      << " requests=" << catalog.plan.requests.size() << " files=" << catalog.assets.size()
                      << " bytes=" << catalog.bytes_read << " error=" << catalog.error << '\n';
            CHECK(catalog.error.empty() && !catalog.assets.empty());
            for (const auto& asset : catalog.assets)
                if (!asset.container_valid)
                    std::cerr << asset.filename << ": " << asset.error << '\n';
            CHECK(std::all_of(catalog.assets.begin(), catalog.assets.end(),
                              [](const auto& a) { return a.container_valid; }));
            CHECK(catalog.assets.front().bank && catalog.assets.front().bank_error.empty());
            const auto& bank = *catalog.assets.front().bank;
            CHECK(bank.waves.size() == 35 && bank.instruments.size() == 14);
            CHECK(catalog.personalities.size() == 1 && catalog.personalities.front().metadata &&
                  catalog.personalities.front().error.empty());
            const std::weak_ptr<const ImaPersonality> personality_lifetime = catalog.personalities.front().metadata;
            std::weak_ptr<const ImaHarmony> harmony_lifetime;
            const std::weak_ptr<const DlsBank> bank_lifetime = catalog.assets.front().bank;
            std::weak_ptr<const ImaStyle> style_lifetime;
            const std::weak_ptr<const ImaBands> bands_lifetime = catalog.assets[1].bands;
            std::weak_ptr<const ImaSection> section_lifetime;
            std::weak_ptr<const ImaTimeline> timeline_lifetime;
            std::size_t section_index = 0;
            const ImaNote* first_note = nullptr;
            std::size_t style_index = 0, style_notes = 0;
            for (std::size_t i = 0; i < catalog.assets.size(); ++i)
            {
                const auto& asset = catalog.assets[i];
                if (asset.kind != MusicAssetKind::Instruments)
                {
                    CHECK(asset.bands && asset.bands_error.empty() && asset.bands->bands.size() == 1);
                    std::array<ImaMidiSetup, 80> setup;
                    CHECK(ima_band_setup(asset.bands->bands.front(), setup, error));
                }
                if (asset.kind == MusicAssetKind::Section)
                {
                    CHECK(asset.section && asset.section_error.empty() && asset.section->commands.size() == 1);
                    ImaSectionTiming timing;
                    CHECK(ima_section_timing(*asset.section, timing, error));
                    CHECK(asset.timeline && asset.section_style && asset.timeline_error.empty() &&
                          !asset.timeline->mutes.empty() && asset.timeline->duration == timing.duration_ticks &&
                          asset.timeline->chords.size() == 1);
                    CHECK(asset.harmony && asset.harmony_error.empty() && asset.harmony->chords.size() == 1 &&
                          asset.harmony->base.scale_mask == 0xab5ab5 && !asset.harmony->chords.front().harmony.enabled);
                    harmony_lifetime = asset.harmony;
                    timeline_lifetime = asset.timeline;
                    section_lifetime = asset.section;
                    section_index = i;
                }
                if (asset.kind != MusicAssetKind::Style)
                    continue;
                CHECK(asset.style && asset.style_error.empty());
                style_notes += asset.style->notes;
                if (asset.style->notes && !first_note)
                    for (const auto& p : asset.style->patterns)
                        for (const auto& g : p.grids)
                            if (!g.notes.empty() && !first_note)
                            {
                                first_note = g.notes.data();
                                style_index = i;
                                style_lifetime = asset.style;
                            }
            }
            CHECK(style_notes == 3049 && first_note && !style_lifetime.expired());
            const auto* samples = bank.waves.front().samples.data();
            const auto observed = catalog.bytes_read;
            CHECK(game.enter_player(error) && game.step(1.0 / 60));
            CHECK(game.music_catalog.bytes_read == observed &&
                  game.music_catalog.assets.front().bank->waves.front().samples.data() ==
                      samples); // No per-frame/UI-dependent file work.
            CHECK(catalog.assets[style_index].style.get() == style_lifetime.lock().get());
            CHECK(!bands_lifetime.expired() && catalog.assets[1].bands.get() == bands_lifetime.lock().get());
            CHECK(!section_lifetime.expired() &&
                  catalog.assets[section_index].section.get() == section_lifetime.lock().get());
            CHECK(!timeline_lifetime.expired() &&
                  catalog.assets[section_index].timeline.get() == timeline_lifetime.lock().get());
            CHECK(!harmony_lifetime.expired() &&
                  catalog.assets[section_index].harmony.get() == harmony_lifetime.lock().get());
            const auto& cursor_asset = catalog.assets[section_index];
            ImaCursorSeed cursor_seed;
            cursor_seed.random_channels = 0xffff;
            CHECK(game.music_catalog.cursor.start(cursor_asset.section, cursor_asset.section_style,
                                                  cursor_asset.timeline, cursor_asset.harmony, cursor_seed, error));
            const std::array<std::uint16_t, 16> cursor_draws{};
            CHECK(game.music_catalog.cursor.step(cursor_draws, error));
            CHECK(game.unload_world() && game.music_catalog.personalities.empty() &&
                  game.music_catalog.assets.empty() && game.music_catalog.plan.requests.empty() &&
                  !game.music_catalog.cursor.active() && !game.music_catalog.cursor.last_step());
            CHECK(bank_lifetime.expired() && style_lifetime.expired() && bands_lifetime.expired() &&
                  section_lifetime.expired() && timeline_lifetime.expired() && personality_lifetime.expired() &&
                  harmony_lifetime.expired());
            CHECK(game.objects.snapshot().empty() && !game.server.message_count() && !game.server.string_count());
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view test = argv[1];
    return (test == "reader"                 ? reader()
            : test == "plan"                 ? plan_test()
            : test == "catalog" && argc == 3 ? catalog_test(fs::absolute(argv[2]))
            : test == "private" && argc == 3 ? private_test(argv[2])
                                             : false)
               ? 0
               : 1;
}
