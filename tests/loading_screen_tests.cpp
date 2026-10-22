#include "engine/resources.h"
#include "engine/world.h"
#include "game/original/LoadingScreen.h"
#include <SDL3/SDL.h>
#include <iostream>
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
    bool info()
    {
        std::string error;
        // Original GetWorldTitle: the last worldname wins, loadscreen prefixes the retail folder,
        // loadscreenID is atoi'd.
        auto retail = world_load_info(
            "WorldName \"Cabalco Transit System\"; LoadScreen \"B2Bumper01.pcx\"; LoadScreenID 1", "01_SubwayA", false);
        CHECK(retail.title == "Cabalco Transit System" &&
              retail.load_screen == "Interface\\LoadScreens\\B2Bumper01.pcx" && retail.load_screen_id == 1);
        auto last = world_load_info("worldname A; WORLDNAME \"B C\";loadscreenid 12x", "w", false);
        CHECK(last.title == "B C" && last.load_screen.empty() && last.load_screen_id == 12);
        // No title: the name given to StartNewWorld.
        auto bare = world_load_info("AmbientLight 1 2 3", "02_CityHub", false);
        CHECK(bare.title == "02_CityHub" && bare.load_screen.empty() && !bare.load_screen_id);
        // _DEMO: Demo_02 selects loading2.pcx whatever the DAT names; other worlds loading1.pcx.
        auto demo = world_load_info("WorldName \"Movin' On Up\"; LoadScreen \"B2Bumper01.pcx\"; LoadScreenID 8",
                                    "Demo_02", true);
        CHECK(demo.load_screen == "\\screens\\loading2.pcx" && demo.load_screen_id == 8);
        CHECK(world_load_info("LoadScreen x", "demo_01", true).load_screen == "\\screens\\loading1.pcx");
        // The header reader takes a prefix.
        std::vector<std::uint8_t> header{56, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 'a', ';', 'b'};
        std::string text;
        CHECK(read_world_info(header, text, error) && text == "a;b");
        header[0] = 55;
        CHECK(!read_world_info(header, text, error));
        header[0] = 56;
        header.pop_back();
        CHECK(!read_world_info(header, text, error));
        return true;
    }
    bool format()
    {
        std::string error;
        // Synthetic 1-pixel-tall font: every glyph 2 pixels wide, space 1.
        HudSurface bitmap{188, 1, std::vector<std::uint8_t>(188 * 4, 255)};
        HudFont font;
        font.surface = &bitmap;
        font.height = 1;
        font.space_width = 1;
        for (unsigned i = 0; i < 94; ++i)
        {
            font.widths[i] = 2;
            font.offsets[i + 1] = font.offsets[i] + 2;
        }
        HudSurface out{20, 6, std::vector<std::uint8_t>(20 * 6 * 4)};
        const auto lit = [&](int x, int y) { return out.rgba[(y * 20 + x) * 4 + 3] != 0; };
        // "~aa bb|c": tab of four spaces, then "aa" (4 px) at 4, "bb" would end at 13 > 10 -> next line.
        font.draw_format(out, "~aa bb|c", 0, 0, 10);
        CHECK(!lit(3, 0) && lit(4, 0) && lit(7, 0) && !lit(8, 0));
        CHECK(lit(0, 1) && lit(3, 1) && !lit(4, 1));
        CHECK(lit(0, 2) && !lit(2, 2));
        font.draw_solid(out, "a", 10, 5, 0, 0x102030);
        CHECK(out.rgba[(5 * 20 + 10) * 4] == 0x10 && out.rgba[(5 * 20 + 11) * 4 + 2] == 0x30);
        CHECK(font.text_width("ab c") == 7);
        return true;
    }
    bool save(const HudSurface& surface, const std::filesystem::path& file)
    {
        auto* image = SDL_CreateSurfaceFrom(int(surface.width), int(surface.height), SDL_PIXELFORMAT_RGBA32,
                                            const_cast<std::uint8_t*>(surface.rgba.data()), int(surface.width * 4));
        const bool saved = image && SDL_SavePNG(image, path_text(file).c_str());
        SDL_DestroySurface(image);
        return saved;
    }
    bool screens(const std::filesystem::path& content, bool demo, const std::filesystem::path& output)
    {
        std::string error;
        Config config;
        config.data_dir = content;
        config.demo = demo;
        Resources resources;
        CHECK(resources.mount_game(config, error));
        LoadingScreen screen(resources);
        if (!output.empty())
            std::filesystem::create_directories(output);
        const auto check = [&](const char* world, unsigned id, const char* title, const char* picture)
        {
            CHECK(screen.begin(world, false, error));
            CHECK(screen.active() && screen.info().load_screen_id == id && screen.info().title == title &&
                  screen.info().load_screen == picture);
            CHECK(screen.surface().width == LoadingScreen::width && screen.surface().height == LoadingScreen::height);
            const auto revision = screen.revision();
            CHECK(screen.draw(true, 0.5, error) && screen.revision() == revision + 1);
            if (!output.empty())
                CHECK(save(screen.surface(), output / (std::string(world) + ".png")));
            screen.end();
            CHECK(!screen.active() && !screen.draw(false, 0, error));
            return true;
        };
        if (demo)
        {
            CHECK(check("Demo_01", 7, "Center for Disease Management", "\\screens\\loading1.pcx"));
            CHECK(check("Demo_02", 8, "Movin' On Up", "\\screens\\loading2.pcx"));
            // Demo: no bottom line, so waiting and loading draws are identical.
            CHECK(screen.begin("Demo_01", false, error));
            const auto loading = screen.surface().rgba;
            CHECK(screen.draw(true, 0.25, error) && screen.surface().rgba == loading);
            return true;
        }
        CHECK(check("01_SubwayA", 1, "Cabalco Transit System", "Interface\\LoadScreens\\B2Bumper01.pcx"));
        CHECK(check("02_CityHub", 2, "Pickman St. Station", "Interface\\LoadScreens\\B2Bumper01.pcx"));
        // The waiting prompt pulses; loading messages are drawn at random per draw.
        CHECK(screen.begin("01_SubwayA", false, error));
        CHECK(screen.message_id >= 1000 && screen.message_id < 1020);
        CHECK(screen.draw(true, 0.0, error));
        const auto start = screen.surface().rgba;
        CHECK(screen.draw(true, 0.5, error) && screen.surface().rgba != start);
        // GAMETYPE_ACTION has no bumper text; an unknown world keeps its name as the title.
        CHECK(screen.begin("01_SubwayA", true, error));
        const auto action = screen.surface().rgba;
        CHECK(screen.begin("01_SubwayA", false, error) && screen.draw(true, 0.0, error));
        CHECK(screen.surface().rgba != action);
        CHECK(screen.begin("No_Such_World", false, error) && screen.info().title == "No_Such_World" &&
              screen.info().load_screen.empty() && !screen.info().load_screen_id);
        if (!output.empty())
            CHECK(save(screen.surface(), output / "default.png"));
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string mode = argv[1];
    if (mode == "info")
        return info() ? 0 : 1;
    if (mode == "format")
        return format() ? 0 : 1;
    if (argc < 3 || (mode != "retail" && mode != "demo"))
        return 2;
    return screens(path_from_utf8(argv[2]), mode == "demo",
                   argc > 3 ? path_from_utf8(argv[3]) : std::filesystem::path{})
               ? 0
               : 1;
}
