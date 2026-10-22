#include "app/menu_input.h"
#include "game/menu_audio.h"
#include "game/original/MenuIds.h"
#include "player_fixture.h"
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __func__ << ':' << __LINE__ << ": " #x << " | " << error << '\n';                             \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    bool manifest()
    {
        std::string error;
        Resources empty;
        empty.set_demo(true);
        StringResources text;
        CHECK(load_menu_strings(empty, text, error) && text.language == 1033 && text.strings.size() == 27);
        CHECK(text.strings.at(503) == "PRIME GIBS" && text.strings.at(539) == "Invert Y-Axis");
        CHECK(!text.strings.contains(529) && !text.strings.contains(600));
        CHECK(validate_menu_strings(MenuEdition::Demo, text, error));
        CHECK(!validate_menu_strings(MenuEdition::Retail, text, error));
        for (const auto& entry : demo_menu_text())
        {
            auto missing = text;
            missing.strings.erase(entry.id);
            CHECK(!validate_menu_strings(MenuEdition::Demo, missing, error));
        }
        std::vector<MenuText> records(demo_menu_text().begin(), demo_menu_text().end());
        records.push_back(records.front());
        CHECK(!make_demo_menu_strings(records, text, error) && text.strings.empty());
        records.pop_back();
        records[0].id = 9999;
        CHECK(!make_demo_menu_strings(records, text, error) && text.strings.empty());
        records[0] = demo_menu_text().front();
        for (auto bad : {std::string{}, std::string(257, 'a'), std::string("bad\nlabel"), std::string("bad\0label", 9)})
        {
            records[0].text = bad;
            CHECK(!make_demo_menu_strings(records, text, error) && text.strings.empty());
        }
        empty.set_demo(false);
        CHECK(!load_menu_strings(empty, text, error) && error.find("cres.dll") != std::string::npos);
        const auto& demo = menu_resources(MenuEdition::Demo);
        CHECK(std::string_view(demo.title("mainmenu.pcx")) == "interface/mainmenus/mainmenu_title.pcx");
        CHECK(!demo.title("loadgame.pcx") && !demo.title("invented.pcx"));
        CHECK(!demo.promotion(MenuPromotion::None) && !demo.promotion(MenuPromotion(99)));
        CHECK(std::string_view(demo.ambience[0]) == demo.ambience[1] &&
              std::string_view(demo.ambience[2]) == demo.ambience[3]);
        return true;
    }
    bool flow()
    {
        std::string error;
        Menu menu(MenuEdition::Demo);
        menu.open(false);
        CHECK(menu.items.size() == 5 && menu.items[3].string_id == 503);
        for (const auto& item : menu.items)
            CHECK(menu.enabled(item));
        MenuInput input;
        input.controls.focused = true;
        input.reset(menu, false);
        auto key = [&](SDL_Scancode code, bool repeat = false, bool player = false, bool alive = true)
        {
            SDL_Event e{};
            e.type = SDL_EVENT_KEY_DOWN;
            e.key.windowID = 42;
            e.key.scancode = code;
            e.key.repeat = repeat;
            input.event(e, 42, menu, player, alive, true, 640, 480);
        };
        menu.selected = 1;
        menu.activate();
        CHECK(menu.promotion == MenuPromotion::Bloodbath && !menu.promotion_exit);
        const auto revision = menu.revision;
        menu.select(1);
        menu.pointer(180, 140, true, true, true);
        menu.adjust(1);
        key(SDL_SCANCODE_RETURN, true);
        key(SDL_SCANCODE_F10);
        CHECK(menu.revision == revision && !input.inspector);
        key(SDL_SCANCODE_SPACE);
        CHECK(menu.page == MenuPage::Main && menu.promotion == MenuPromotion::None && menu.selected == 0);
        menu.selected = 0;
        menu.activate();
        CHECK(menu.page == MenuPage::Solo && menu.items.size() == 2);
        menu.selected = 1;
        menu.activate();
        CHECK(menu.promotion == MenuPromotion::Modes);
        key(SDL_SCANCODE_ESCAPE);
        CHECK(menu.page == MenuPage::Main && menu.selected == 0);
        for (unsigned difficulty = 0; difficulty < 3; ++difficulty)
        {
            menu.build(MenuPage::Solo);
            menu.activate();
            CHECK(menu.page == MenuPage::Difficulty && menu.selected == 1);
            menu.selected = int(difficulty);
            menu.activate();
            auto request = menu.take_command();
            CHECK(request.action == MenuAction::NewGame && request.story && request.character == 0 &&
                  request.difficulty == difficulty);
        }
        menu.build(MenuPage::Options);
        CHECK(menu.items.size() == 7 && !menu.enabled(menu.items[2]) && menu.enabled(menu.items[3]) &&
              menu.items[6].literal == "CHEATS" && menu.enabled(menu.items[6]));
        menu.select(1);
        menu.select(1);
        CHECK(menu.selected == 3);
        menu.activate();
        CHECK(menu.page == MenuPage::Mouse && menu.items.size() == 3 && !menu.enabled(menu.items[1]));
        menu.adjust(1);
        CHECK(menu.settings.sensitivity == 170);
        menu.select(1);
        menu.activate();
        CHECK(menu.settings.invert_y);
        menu.take_command();
        for (auto absent : {MenuPage::Load, MenuPage::Save, MenuPage::Character, MenuPage::Quit})
        {
            menu.build(absent);
            CHECK(menu.page == MenuPage::Main);
        }
        menu.close();
        input.reset(menu, true);
        key(SDL_SCANCODE_W, false, true);
        CHECK(input.controls.keys[SDL_SCANCODE_W]);
        key(SDL_SCANCODE_ESCAPE, false, true);
        CHECK(menu.active && input.paused(menu) && !input.controls.keys[SDL_SCANCODE_W] &&
              !input.controls.mouse_requested);
        key(SDL_SCANCODE_ESCAPE, false, true);
        CHECK(!menu.active && !input.paused(menu) && input.controls.mouse_requested);
        key(SDL_SCANCODE_RETURN, true, true, false);
        CHECK(!input.demo.restart);
        key(SDL_SCANCODE_RETURN, false, true, false);
        CHECK(input.demo.restart);
        input.demo.restart = false;
        menu.end_demo();
        input.reset(menu, false);
        CHECK(menu.active && !menu.in_world && menu.page == MenuPage::Main && menu.promotion == MenuPromotion::Info &&
              !menu.promotion_exit);
        key(SDL_SCANCODE_RETURN);
        CHECK(menu.active && menu.promotion == MenuPromotion::None && menu.take_command().action != MenuAction::Quit);
        menu.selected = 4;
        menu.activate();
        CHECK(menu.promotion == MenuPromotion::Info && menu.promotion_exit);
        key(SDL_SCANCODE_SPACE);
        CHECK(menu.take_command().action == MenuAction::Quit && !input.demo.restart);
        return true;
    }
    bool override_bad(Resources& resources, const std::filesystem::path& scratch, const std::string& path,
                      std::string& error)
    {
        // A small synthetic corrupt override; never copy or mutate commercial assets.
        test_player::Directory directory;
        auto* d = &directory;
        std::size_t begin = 0, slash;
        while ((slash = path.find('/', begin)) != std::string::npos)
        {
            d = &d->children[path.substr(begin, slash - begin)];
            begin = slash + 1;
        }
        d->files[path.substr(begin)] = {0, 1, 2};
        test_model::Writer w;
        w.b.resize(168);
        w.b[0] = w.b[62] = w.b[124] = 13;
        w.b[1] = w.b[63] = w.b[125] = 10;
        w.b[126] = 26;
        w.patch(127, 1);
        auto [offset, size] = test_player::write_dir(w, directory);
        w.patch(131, offset);
        w.patch(135, size);
        std::filesystem::create_directories(scratch);
        auto file = scratch / "corrupt.rez";
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(w.b.data()), w.b.size());
        out.close();
        return bool(out) && resources.mount(file, error);
    }
    bool assets(const char* root, const std::filesystem::path& scratch)
    {
        std::string error;
        Resources resources;
        Config config;
        config.demo = true;
        config.data_dir = root;
        CHECK(resources.mount_game(config, error));
        Menu menu(MenuEdition::Demo);
        menu.open(false);
        MenuCanvas canvas(resources);
        CHECK(canvas.initialize(error));
        // Source AddLargeTextItemOption uses the edition's authored fading font.
        // Build 108 is 41px high; the retail asset is 31px. Do not normalize either.
        HudCanvas fonts(resources);
        HudFont large;
        const auto& art = menu_resources(MenuEdition::Demo);
        CHECK(fonts.load_font(art.large_bitmaps[0], art.large_font, large, 1) && large.height == 41);
        // The XR Options page (VR, SETUP and CHEATS rows) shares the list at two thirds of a row.
        menu.vr = menu.setup = true;
        for (auto page : {MenuPage::Main, MenuPage::Solo, MenuPage::Difficulty, MenuPage::Options, MenuPage::Display,
                          MenuPage::Audio, MenuPage::Mouse, MenuPage::Vr, MenuPage::Cheats})
        {
            menu.build(page);
            CHECK(canvas.compose(menu, 0, error));
            CHECK(page != MenuPage::Options || menu.items.size() == 9);
            for (const auto& item : menu.items)
            {
                CHECK(item.rect.bottom <= 425 && item.rect.right <= 625);
                if (!menu.small())
                    CHECK(item.rect.bottom - item.rect.top <= int(large.height) &&
                          item.rect.bottom - item.rect.top >= int(large.height) * 2 / 3);
            }
            auto revision = canvas.revision;
            CHECK(canvas.compose(menu, 0, error) && canvas.revision == revision);
        }
        for (auto promotion : {MenuPromotion::Bloodbath, MenuPromotion::Info, MenuPromotion::Modes})
        {
            menu.show_promotion(promotion);
            CHECK(canvas.compose(menu, 0, error));
            HudCanvas image(resources);
            const auto* original =
                image.CreateSurfaceFromBitmap(menu_resources(MenuEdition::Demo).promotion(promotion));
            CHECK(original && canvas.surface->rgba == original->rgba);
            menu.dismiss_promotion();
        }
        Menu retail;
        CHECK(!canvas.compose(retail, 0, error) && error.find("edition mismatch") != std::string::npos);
        MenuAudio audio(resources);
        CHECK(audio.validate_resources(error) && audio.mixer.voices().empty() && audio.starts == 0);
        for (unsigned seed = 0; seed < 8; ++seed)
        {
            audio.random_state = seed * 1000;
            audio.focus(true);
            CHECK(audio.ambient && audio.error.empty());
            auto voice = audio.mixer.get(audio.ambient);
            CHECK(voice);
            CHECK(voice->request.resource.ends_with("ambient02.wav") ||
                  voice->request.resource.ends_with("ambient04.wav"));
            audio.focus(false);
        }
        for (auto cue : {MenuCue::Select, MenuCue::Enter, MenuCue::Back})
            audio.play(cue);
        CHECK(audio.advance(.1) && audio.failures == 0);
        for (const auto* path :
             {"interface/mainmenus/mainmenu_title.pcx", "interface/mainmenus/mm_font.fnt", "screens/info.pcx"})
        {
            auto broken = resources;
            CHECK(override_bad(broken, scratch, path, error));
            MenuCanvas failure(broken);
            CHECK(!failure.initialize(error) && !error.empty());
        }
        for (const auto* path : {"sounds/interface/mainmenus/ambient02.wav", "sounds/interface/mainmenus/enter01.wav"})
        {
            auto broken = resources;
            CHECK(override_bad(broken, scratch, path, error));
            MenuAudio failure(broken);
            CHECK(!failure.validate_resources(error) && error.find(path) != std::string::npos);
        }
        Resources missing;
        missing.set_demo(true);
        MenuCanvas missing_art(missing);
        CHECK(!missing_art.initialize(error));
        MenuAudio missing_audio(missing);
        CHECK(!missing_audio.validate_resources(error));
        std::cout << "Demo menu: all nine pages, three exact promotions, edition isolation, all audio, "
                     "corrupt/missing resources\n";
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string mode = argv[1];
    if (mode == "manifest")
        return manifest() ? 0 : 1;
    if (mode == "flow")
        return flow() ? 0 : 1;
    if (mode == "assets" && argc == 4)
        return assets(argv[2], argv[3]) ? 0 : 1;
    return 2;
}
