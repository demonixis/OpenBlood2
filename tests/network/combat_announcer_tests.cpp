#include "../resource_fixture.h"
#include "../string_resource_fixture.h"
#include "audio_fixture.h"
#include "client/network_speech.h"
#include <iostream>
#include <set>
using namespace OpenBlood2;
using namespace OpenBlood2::Net;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x " : " << error << '\n';                                                   \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool mount(Resources& resources, const std::filesystem::path& path, const test_resource::Directory& dir,
               std::string& error)
    {
        test_model::Writer rez;
        rez.b.resize(168);
        rez.b[0] = rez.b[62] = rez.b[124] = 13;
        rez.b[1] = rez.b[63] = rez.b[125] = 10;
        rez.b[126] = 26;
        rez.patch(127, 1);
        auto [offset, size] = test_resource::write_dir(rez, dir);
        rez.patch(131, offset);
        rez.patch(135, size);
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(rez.b.data()), rez.b.size());
        file.close();
        return file.good() && resources.mount(path, error);
    }
    bool tables(const char* data)
    {
        std::string error;
        Resources resources;
        if (data)
        {
            Config config;
            config.data_dir = path_from_utf8(data);
            CHECK(resources.mount_game(config, error));
        }
        ClientAnnouncer announcer;
        std::set<std::string> files;
        std::set<std::string> missing;
        unsigned selections = 0;
        const auto check = [&](const Conversation& line)
        {
            CHECK(line.character == 4 && !line.text.empty() && line.text.size() < 500 &&
                  line.file.starts_with("Sounds\\Voice\\Voice"));
            if (files.insert(line.file).second && data)
            {
                AudioTimeline audio; // Individual resource audit, not 209 simultaneous cached clips.
                AudioVoice voice;
                SoundRequest request;
                request.resource = line.file;
                if (!resources.find(line.file))
                {
                    missing.insert(line.file);
                    std::cout << "Missing original announcer WAV: " << line.file << '\n';
                }
                else
                    CHECK(audio.prepare_voice(resources, request, {}, voice, error));
            }
            ++selections;
            return true;
        };
        for (unsigned kind = 0; kind < 2; ++kind)
            for (unsigned index = 0; index < (kind ? 24 : 184); ++index)
            {
                std::string original_file;
                for (DBYTE attacker = 0; attacker < 4; ++attacker)
                    for (DBYTE victim = 0; victim < 4; ++victim)
                    {
                        Conversation line, wrapped;
                        CHECK(announcer.frag(std::uint16_t(index), {"A%s", attacker}, {"Victim", victim}, kind,
                                             resources, line));
                        CHECK(announcer.frag(std::uint16_t(index + (kind ? 24 : 184)), {"A%s", attacker},
                                             {"Victim", victim}, kind, resources, wrapped));
                        CHECK(line.file == wrapped.file && line.text == wrapped.text && check(line));
                        if (original_file.empty())
                            original_file = line.file;
                        // Each source frag/suicide entry includes ALL on at least
                        // one side, so its ANY-bit match retains the initial index.
                        CHECK(line.file == original_file);
                    }
            }
        const unsigned male[]{241, 242, 242, 243, 244, 247, 247, 241};
        const unsigned female[]{241, 241, 242, 243, 245, 245, 246, 246};
        for (unsigned index = 0; index < 8; ++index)
            for (DBYTE attacker = 0; attacker < 4; ++attacker)
                for (DBYTE victim = 0; victim < 4; ++victim)
                {
                    Conversation line;
                    CHECK(announcer.finish({std::uint16_t(index), attacker, victim}, resources, line));
                    CHECK(line.file == "Sounds\\Voice\\Voice" +
                                           std::to_string((victim == 1 || victim == 3 ? female : male)[index]) +
                                           ".wav");
                    CHECK(check(line));
                }
        Conversation edge, normal;
        CHECK(announcer.finish({65535, 3, 2}, resources, edge) && edge.file == "Sounds\\Voice\\Voice241.wav");
        CHECK(announcer.frag(65535, {"K", 3}, {"V", 0}, false, resources, edge));
        CHECK(announcer.frag(65535 % 184, {"K", 3}, {"V", 0}, false, resources, normal) && edge.text == normal.text);
        if (!data)
        {
            CHECK(announcer.frag(27, {"A%s", 0}, {"V", 1}, false, resources, edge));
            CHECK(edge.text == "A%s Whipped V’s Cream");
        }
        auto before = edge;
        CHECK(!announcer.frag(0, {"", 0}, {"V", 0}, false, resources, edge) && edge.text == before.text);
        CHECK(!announcer.finish({0, 4, 0}, resources, edge) && edge.file == before.file);
        CHECK(files.size() == 209 && selections == 3456);
        // Exact absent source paths observed in the mounted retail data. Accept
        // an installation supplying them, but never mask a new missing resource.
        std::set<std::string> documented_missing;
        for (unsigned file = 1; file <= 255; ++file)
            if (file == 7 || file == 8 || file == 9 || file == 49 || (file >= 68 && file <= 99))
                documented_missing.insert("Sounds\\Voice\\Voice" + std::to_string(file) + ".wav");
        for (const auto& path : missing)
            CHECK(documented_missing.contains(path));
        std::cout
            << "Announcer tables: " << selections
            << " selections, 209 unique paths, decoded=" << (data ? files.size() - missing.size() : 0)
            << ", missing=" << missing.size()
            << ", original ANY-bit versus strict gender selection, 16-bit indices and owned name formatting; fallbacks="
            << announcer.fallback_texts << '\n';
        return true;
    }
    bool formats(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(test_network_audio::setup(resources, root, error));
        test_resource::Directory dir;
        dir.files["cres.dll"] = test_strings::pe({{4100, u"%n"},
                                                  {4101, std::u16string(128, u'x')},
                                                  {4102, u"%s / %s"},
                                                  {4103, u"%s %s %s"},
                                                  {4300, u"%s eliminated %% himself"},
                                                  {4330, u"Bad %s"}})
                                    .b;
        CHECK(mount(resources, root / "strings.rez", dir, error));
        ClientAnnouncer announcer;
        Conversation line;
        CHECK(announcer.frag(0, {"A%s", 0}, {"V%n", 1}, false, resources, line) && line.text == "Destroyed");
        CHECK(announcer.frag(1, {"A%s", 0}, {"V%n", 1}, false, resources, line) && line.text == "Hosed");
        CHECK(announcer.frag(2, {"A%s", 0}, {"V%n", 1}, false, resources, line) && line.text == "A%s / V%n");
        CHECK(announcer.frag(3, {"A%s", 0}, {"V%n", 1}, false, resources, line) && line.text == "Toasted");
        CHECK(announcer.frag(0, {"A%s", 0}, {"A%s", 0}, true, resources, line) &&
              line.text == "A%s eliminated % himself");
        CHECK(announcer.finish({0, 0, 0}, resources, line) && line.text == "Humiliate Him!");
        CHECK(announcer.fallback_texts == 4 && !announcer.error.empty());
        announcer.set_translations([](unsigned id) -> std::string_view
        {
            switch (id)
            {
                case 25:
                    return "{0} 참가";
                case 7003:
                    return "{1}: {0} 깃발 점령";
                case 7008:
                    return "빨강 팀";
                case 4130:
                    return "{1} est anéanti par {0} (100%).";
                case 4131:
                    return "{0} incomplete";
                case 4100:
                    return "Unexpected {0}";
                default:
                    return {};
            }
        });
        std::string caption;
        CHECK(announcer.frag(30, {"A%s{1}", 0}, {"V%n{0}", 1}, false, resources, line, &caption));
        CHECK(line.text == "A%s{1} slagged V%n{0}" && line.file == "Sounds\\Voice\\Voice31.wav" &&
              caption == "V%n{0} est anéanti par A%s{1} (100%).");
        CHECK(announcer.frag(31, {"A%s", 0}, {"V%n", 1}, false, resources, line, &caption) && caption.empty() &&
              line.text == "A%s annihilated V%n");
        CHECK(announcer.frag(0, {"A%s", 0}, {"V%n", 1}, false, resources, line, &caption) && caption.empty() &&
              line.text == "Destroyed");
        CHECK(announcer.player_notice("A%s{1}", true, resources, caption) && caption == "A%s{1} 참가");
        CHECK(announcer.flag_notice(CTFEventKind::Captured, "A%s{1}", 2, false, resources, caption) &&
              caption == "빨강 팀: A%s{1} 깃발 점령");
        return true;
    }
    bool speech(const std::filesystem::path& root)
    {
        std::string error;
        Resources resources;
        CHECK(test_network_audio::setup(resources, root, error));
        std::vector<std::uint8_t> tone;
        CHECK(resources.read("tone.wav", tone, error));
        test_resource::Directory dir;
        for (unsigned file : {31, 241, 242, 222})
            dir.children["sounds"].children["voice"].files["voice" + std::to_string(file) + ".wav"] = tone;
        CHECK(mount(resources, root / "voices.rez", dir, error));
        Joined owners[]{{7, 1, {0, 1}}, {7, 2, {1, 1}}, {7, 3, {2, 1}}};
        std::vector<RosterPlayer> roster{
            {1, {0, 1}, "A%s", 0, 1}, {2, {1, 1}, "Victim", 1, 0}, {3, {2, 1}, "Observer", 2, 0}};
        NetworkAudio audio[3];
        NetworkSpeech a(audio[0], resources), b(audio[1], resources), c(audio[2], resources);
        constexpr auto translated = "{0} a réduit {1} en scories. Ce texte allongé ne change pas la durée de la voix.";
        const auto locale = [&](unsigned id) -> std::string_view { return id == 4130 ? translated : ""; };
        a.announcer.set_translations(locale);
        b.announcer.set_translations(locale);
        NetworkSpeech* clients[]{&a, &b, &c};
        ClientWorld world;
        CHECK(world.begin(7));
        for (unsigned i = 0; i < 3; ++i)
        {
            CHECK(audio[i].begin(owners[i]) && clients[i]->begin(owners[i]));
            audio[i].mixer.inspect_pcm = true;
            CHECK(clients[i]->receive({owners[i], 1, 0, FragEvent{{1, 1}, {0, 1}, 1, 30}}, .1, false, roster, error));
        }
        CHECK(a.communication.level == 1 && a.communication.message.text == "A%s slagged Victim" &&
              a.communication.length == 2.5f && a.subtitle().english.empty() && a.notices().empty());
        CHECK(b.notices().size() == 1 && c.notices().size() == 1 && !b.communication.level && !c.communication.level);
        CHECK(b.notices().back().text.starts_with("A%s a réduit Victim en scories.") &&
              c.notices().back().text == "A%s slagged Victim");
        CHECK(a.manager.PlaySound("tone.wav", true));
        const auto prior = a.manager.voice;
        CHECK(a.receive({owners[0], 1, 0, FragEvent{{1, 1}, {0, 1}, 1, 30}}, .2, false, roster, error));
        CHECK(a.frag_events == 1 && a.manager.voice == prior && a.communication.received == 1);
        CHECK(a.update(.31, error) && a.communication.voice && !a.manager.voice && !audio[0].mixer.get(prior));
        const auto subtitle = a.subtitle();
        CHECK(subtitle.english == "A%s slagged Victim" && subtitle.translated ==
              "A%s a réduit Victim en scories. Ce texte allongé ne change pas la durée de la voix.");
        CHECK(audio[0].advance(.1, world, {}, {}) && audio[0].mixer.peak == .5f);
        CHECK(!audio[1].local_started && !audio[2].local_started);
        const auto voice = a.communication.voice;
        CHECK(a.receive({owners[0], 2, 0, FinishHimVoice{0, 0, 1}}, .4, false, roster, error));
        CHECK(a.communication.busy == 1 && a.communication.voice == voice && a.subtitle().english == subtitle.english);
        a.clear();
        CHECK(a.subtitle().english.empty());
        CHECK(a.receive({owners[0], 3, 0, FinishHimVoice{0, 0, 1}}, 4, false, roster, error));
        CHECK(a.communication.message.text == "Humiliate Her!" &&
              a.communication.message.file == "Sounds\\Voice\\Voice241.wav");
        CHECK(a.update(4.21, error) && a.communication.voice && audio[0].advance(.1, world, {}, {}) &&
              audio[0].mixer.peak == .5f);
        CHECK(a.subtitle().english == "Humiliate Her!" && a.subtitle().translated.empty());
        for (unsigned i = 1; i <= 5; ++i)
            CHECK(b.receive({owners[1], i + 1, 0, FragEvent{{1, 1}, {0, 1}, int(i), 30}}, i, false, roster, error));
        CHECK(b.frag_notices == 6 && b.notices().size() == 4 && b.notices()[0].expires == 7);
        CHECK(b.update(6.99, error) && b.notices().size() == 4);
        CHECK(b.update(7, error) && b.notices().size() == 3);
        CHECK(b.receive({owners[1], 7, 0, FragEvent{{1, 1}, {0, 2}, 1, 30}}, 7, false, roster, error));
        CHECK(b.missing_players == 1 && b.notices().size() == 3 && b.frag_notices == 6);
        CHECK(b.update(10, error) && b.notices().empty());
        CHECK(b.receive({owners[1], 8, 0, FragEvent{{1, 1}, {1, 1}, -1, 23}}, 11, false, roster, error));
        CHECK(b.frag_communications == 1 && b.communication.message.text == "Victim Should Go Home To Mommy!");
        CHECK(b.update(11.21, error) && b.communication.voice && audio[1].advance(.1, world, {}, {}) &&
              audio[1].mixer.peak == .5f);
        b.clear();
        CHECK(b.receive({owners[1], 9, 0, FragEvent{{1, 1}, {1, 1}, -2, 0}}, 13, false, roster, error));
        CHECK(b.update(13.21, error) && !b.communication.voice && b.communication.sound_failures == 1 &&
              !b.communication.error.empty());
        CHECK(!b.receive({{8, 2, {1, 1}}, 10, 0, FinishHimVoice{}}, 14, false, roster, error));
        for (auto* client : clients)
            client->clear();
        CHECK(audio[0].mixer.voices().empty() && audio[1].mixer.voices().empty() && audio[2].mixer.voices().empty());
        CHECK(!audio[0].started && !audio[1].started && !audio[2].started);
        std::cout << "Combat speech: private killer/suicide/humiliation PCM, observer text, exact expiry/capacity, "
                     "duplicate/stale/missing identity, busy/delayed interruption and missing WAV passed\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view mode = argv[1];
    if (mode == "tables")
        return tables(argc == 3 ? argv[2] : nullptr) ? 0 : 1;
    if (argc != 3)
        return 2;
    if (mode == "formats")
        return formats(path_from_utf8(argv[2])) ? 0 : 1;
    if (mode == "speech")
        return speech(path_from_utf8(argv[2])) ? 0 : 1;
    return 2;
}
