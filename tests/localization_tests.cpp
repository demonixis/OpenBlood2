#include "client/announcer_data.h"
#include "engine/config.h"
#include "engine/localization.h"
#include <fstream>
#include <iostream>
#include <iterator>

using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << " " << error << '\n';                                                  \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool localization(const std::filesystem::path& directory)
    {
        using namespace std::string_view_literals;
        std::string error;
        Localization strings;
        for (const auto language : languages())
        {
            const auto tag = language_tag(language);
            CHECK(parse_language(tag) == language && !language_name(language).empty());
            if (language == Language::English)
            {
                CHECK(strings.load(language, error));
                CHECK(strings.text(TextKey::ThirdPerson, "Third person") == "Third person");
                continue;
            }
            std::ifstream file(directory / (std::string(tag) + ".ini"), std::ios::binary);
            CHECK(file);
            const std::string ini(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
            CHECK(strings.load_catalog(language, ini, error) && strings.language() == language);
            for (const auto key : {TextKey::Language, TextKey::Setup, TextKey::Cheats, TextKey::UnavailableOptions,
                                   TextKey::FirstPerson, TextKey::ThirdPerson})
                CHECK(!strings.text(key, {}).empty());
            for (const auto id : {502u, 521u, 522u, 523u, 524u, 525u, 526u})
                CHECK(!strings.text(id, {}).empty());
            for (const auto key : {"setup.language", "setup.play", "loading.continue", "menu.main_title",
                                   "hud.quicksave", "objectives.empty", "objectives.authored.1b23dee31a697548.text"})
                CHECK(!strings.text(key).empty());
            CHECK(!strings.text(5000).empty() && !strings.text(6000).empty());
            for (unsigned id = 3000; id != 3034; ++id)
                CHECK(!strings.text(id).empty());
            for (auto key : {"promotions.bloodbath_title", "promotions.bloodbath_tagline", "promotions.bloodbath_soon",
                             "promotions.blood_brother_title", "promotions.blood_brother_body",
                             "promotions.blood_feud_title", "promotions.blood_feud_body", "promotions.info_title",
                             "promotions.info_characters", "promotions.info_weapons", "promotions.info_levels",
                             "promotions.info_maps", "promotions.info_network", "promotions.info_editor",
                             "promotions.info_engine", "promotions.modes_title", "promotions.modes_caleb",
                             "promotions.modes_chosen_title", "promotions.modes_chosen"})
                CHECK(!strings.text(key).empty());
            using AnnouncerTable = std::span<const AnnouncerData::Entry>;
            for (const auto table : {AnnouncerTable(AnnouncerData::kill), AnnouncerTable(AnnouncerData::suicide),
                                     AnnouncerTable(AnnouncerData::humiliation)})
                for (const auto& entry : table)
                    CHECK(!strings.text(entry.resource).empty());
            for (unsigned id = 4001; id <= 4008; ++id)
                CHECK(!strings.text(id).empty());
            for (auto id : {25u, 26u, 7000u, 7001u, 7003u, 7004u, 7005u, 7006u, 7007u, 7008u, 7009u, 7011u})
                CHECK(!strings.text(id).empty());
            CHECK(!strings.dialogue("Sounds\\Chosen\\Caleb\\1ACaleb1.WAV").empty());
            CHECK(!strings.text("dialogue.sounds.events.cwvoice8").empty());
            CHECK(!strings.text("dialogue.sounds.events.tavoice1").empty());
        }
        CHECK(!parse_language("zh") && !parse_language("pt-br") && !parse_language("../../fr"));

        constexpr auto partial = "\xef\xbb\xbf; UTF-8 BOM and CRLF\r\n[menu]\r\n language = LANGUE\r\n"
                                 "[hud]\r\nthird_person=Troisième personne"sv;
        CHECK(strings.load_catalog(Language::French, partial, error));
        CHECK(strings.text(TextKey::Language, "LANGUAGE") == "LANGUE");
        CHECK(strings.text(TextKey::ThirdPerson, "Third person") == "Troisième personne");
        CHECK(strings.text(TextKey::FirstPerson, "First person") == "First person");
        CHECK(strings.text(502, "OPTIONS") == "OPTIONS");
        CHECK(strings.text("setup.play", "Play") == "Play");
        CHECK(strings.dialogue("1ACaleb1.wav").empty());
        CHECK(strings.load_catalog(Language::French, "[setup]\nmessage={1} puis {0}\nbad=seulement {0}", error));
        CHECK(strings.format("setup.message", "{0}, then {1}", {"A", "{0}"}) == "{0} puis A");
        CHECK(strings.format("setup.bad", "{0}, then {1}", {"A", "B"}) == "A, then B");

        constexpr std::string_view rejected[]{"[Menu]\nlanguage=LANGUE", "language=LANGUE",
                                              "[menu]\nlanguage=LANGUE\nlanguage=SPRACHE",
                                              "[menu]\n[menu]\nlanguage=LANGUE", "[menu]\nLanguage=LANGUE",
                                              "[resources]\n0502=OPTIONS", "[resources]\n65536=OPTIONS",
                                              "[setup]\nplay=Jouer##other_id", "[menu]\nweird..key=label",
                                              "[menu]\nlanguage=", "[menu]\nlanguage=LANG\0UE"sv,
                                              "[menu]\nlanguage=\xc0\xaf", "[menu]\nlanguage=\xed\xa0\x80",
                                              "[menu]\nlanguage=\xe9", "[menu]\nlanguage=L\tANGUE"};
        for (const auto invalid : rejected)
        {
            CHECK(strings.load_catalog(Language::French, partial, error));
            CHECK(!strings.load_catalog(Language::French, invalid, error) && !error.empty());
            CHECK(strings.language() == Language::English && strings.text(TextKey::Language, "LANGUAGE") == "LANGUAGE");
        }
        CHECK(!strings.load_catalog(Language::French, std::string(256 * 1024 + 1, ' '), error));
        CHECK(!strings.load_catalog(Language::French, "[menu]\nlanguage=" + std::string(1025, 'a'), error));
        return true;
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    return localization(path_from_utf8(argv[1])) ? 0 : 1;
}
