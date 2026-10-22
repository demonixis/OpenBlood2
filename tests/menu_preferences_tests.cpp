#include "app/movement_controls.h"
#include "engine/audio.h"
#include "game/menu_preferences.h"
#include <cmath>
#include <fstream>
#include <iostream>
using namespace OpenBlood2;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x << " " << error << '\n';                                                  \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (false)
int test_menu_preferences(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    std::string error;
    const auto root = path_from_utf8(argv[1]);
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    const auto path = root / "preferences.cfg";
    std::filesystem::remove(path, ec);
    MenuSettings defaults, settings;
    CHECK(load_menu_preferences(path, settings, error) && settings == defaults);
    settings = {false, true, true, 15, 35, 730};
    CHECK(save_menu_preferences(path, settings, error));
    MenuSettings loaded;
    CHECK(load_menu_preferences(path, loaded, error) && loaded == settings);
    {
        // The shared atomic writer keeps the exact preference bytes.
        std::ifstream in(path, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(bytes == "version=5\nsound=0\nmusic=1\nsound_volume=15\nmusic_volume=35\nsensitivity=730\n"
                       "invert_y=1\ndetail=2\npixel_lighting=0\nrenderer=0\nquality=0\nnormal_maps=1\n"
                       "vr_turn=45\nvr_laser=1\nvr_crosshair=1\nvr_move_hand=0\n");
    }
    settings.sound = true;
    CHECK(save_menu_preferences(path, settings, error));
    CHECK(load_menu_preferences(path, loaded, error) && loaded == settings);
    // Version 2 keeps the display settings; version 1 files load with display defaults.
    settings.detail = 1;
    settings.pixel_lighting = true;
    CHECK(save_menu_preferences(path, settings, error));
    CHECK(load_menu_preferences(path, loaded, error) && loaded == settings);
    {
        std::ofstream out(path);
        out << "version=1\nsound=1\nmusic=1\nsound_volume=15\nmusic_volume=35\nsensitivity=730\ninvert_y=1\n";
    }
    CHECK(load_menu_preferences(path, loaded, error) && loaded.detail == 2 && !loaded.pixel_lighting &&
          loaded.sensitivity == 730 && loaded.renderer == 0 && loaded.normal_maps);
    {
        // Version 2 files keep their display settings and load the renderer defaults.
        std::ofstream out(path);
        out << "version=2\nsound=1\nmusic=1\nsound_volume=15\nmusic_volume=35\nsensitivity=730\ninvert_y=1\n"
               "detail=1\npixel_lighting=1\n";
    }
    CHECK(load_menu_preferences(path, loaded, error) && loaded.detail == 1 && loaded.pixel_lighting &&
          loaded.renderer == 0 && loaded.quality == 0 && loaded.normal_maps);
    {
        std::ofstream out(path);
        out << "version=3\nsound=1\nmusic=1\nsound_volume=15\nmusic_volume=35\nsensitivity=730\ninvert_y=1\n"
               "detail=1\npixel_lighting=0\nrenderer=1\nquality=1\nnormal_maps=0\n";
    }
    CHECK(load_menu_preferences(path, loaded, error) && loaded.renderer == 1 && loaded.quality == 1 &&
          !loaded.normal_maps && loaded.vr_turn == 45 && loaded.vr_laser && loaded.vr_crosshair);
    {
        // Version 4 adds the VR options, version 5 the movement reference; a turn step outside the
        // offered values is refused.
        MenuSettings vr;
        vr.vr_turn = 0;
        vr.vr_laser = false;
        vr.vr_move_hand = true;
        CHECK(save_menu_preferences(path, vr, error));
        CHECK(load_menu_preferences(path, loaded, error) && loaded == vr && loaded.vr_crosshair);
        {
            std::ofstream v4(path);
            v4 << "version=4\nsound=1\nmusic=1\nsound_volume=15\nmusic_volume=35\nsensitivity=730\ninvert_y=1\n"
                  "detail=1\npixel_lighting=0\nrenderer=1\nquality=1\nnormal_maps=0\nvr_turn=30\nvr_laser=0\n"
                  "vr_crosshair=1\n";
        }
        CHECK(load_menu_preferences(path, loaded, error) && loaded.vr_turn == 30 && !loaded.vr_laser &&
              !loaded.vr_move_hand);
        vr.vr_turn = 20;
        CHECK(!save_menu_preferences(path, vr, error) && !error.empty());
        std::ofstream out(path);
        out << "version=4\nsound=1\nmusic=1\nsound_volume=15\nmusic_volume=35\nsensitivity=730\ninvert_y=1\n"
               "detail=1\npixel_lighting=0\nrenderer=1\nquality=1\nnormal_maps=0\nvr_turn=20\nvr_laser=1\n"
               "vr_crosshair=1\n";
    }
    CHECK(!load_menu_preferences(path, loaded, error) && !error.empty());
    for (const auto text : {"renderer=2\nquality=0\nnormal_maps=0\n", "renderer=0\nquality=0\nnormal_maps=2\n",
                            "renderer=0\nquality=0\n"})
    {
        {
            std::ofstream out(path);
            out << "version=3\nsound=1\nmusic=1\nsound_volume=15\nmusic_volume=35\nsensitivity=730\ninvert_y=1\n"
                   "detail=1\npixel_lighting=0\n"
                << text;
        }
        CHECK(!load_menu_preferences(path, loaded, error) && !error.empty());
    }
    {
        // Version 2 with version 3 keys is incomplete/unsupported.
        std::ofstream out(path);
        out << "version=2\nsound=1\nmusic=1\nsound_volume=15\nmusic_volume=35\nsensitivity=730\ninvert_y=1\n"
               "detail=1\npixel_lighting=0\nrenderer=1\n";
    }
    CHECK(!load_menu_preferences(path, loaded, error) && !error.empty());
    {
        std::ofstream out(path);
        out << "version=1\nsound=1\nmusic=1\nsound_volume=15\nmusic_volume=35\nsensitivity=730\ninvert_y=1\n"
               "detail=1\n";
    }
    CHECK(!load_menu_preferences(path, loaded, error) && !error.empty());
    {
        std::ofstream out(path);
        out << "version=2\nsound=1\nmusic=1\nsound_volume=15\nmusic_volume=35\nsensitivity=730\ninvert_y=1\n"
               "detail=3\npixel_lighting=0\n";
    }
    CHECK(!load_menu_preferences(path, loaded, error) && !error.empty());
    settings.detail = 2;
    settings.pixel_lighting = false;
    CHECK(save_menu_preferences(path, settings, error));
    CHECK(load_menu_preferences(path, loaded, error) && loaded == settings);
    for (const auto text : {"version=1\n", "version=999\n", "version=1\nversion=1\n", "volume=5\n", "sound=nan\n"})
    {
        {
            std::ofstream out(path);
            out << text;
        }
        CHECK(!load_menu_preferences(path, loaded, error) && !error.empty() && loaded == settings);
    }
    CHECK(save_menu_preferences(path, settings, error));
    auto invalid = settings;
    invalid.sensitivity = 99;
    CHECK(!save_menu_preferences(path, invalid, error));
    CHECK(load_menu_preferences(path, loaded, error) && loaded == settings);
    CHECK(!save_menu_preferences(path / "impossible.cfg", settings, error) &&
          error.starts_with("Cannot create preferences directory: "));
    for (const auto& entry : std::filesystem::directory_iterator(root))
        CHECK(path_text(entry.path().filename()).find(".tmp-") == std::string::npos);
    MovementControls input;
    input.focused = input.enabled = input.mouse_captured = true;
    PlayerRotation angle;
    input.mouse_x = 10;
    input.mouse_y = 20;
    input.look(angle, 0, false);
    CHECK(std::abs(angle.yaw - .025f) < 1e-6 && std::abs(angle.pitch - .05f) < 1e-6);
    angle = {};
    input.mouse_x = 10;
    input.mouse_y = 20;
    input.invert_y = true;
    input.mouse_sensitivity = .005f;
    input.look(angle, 0, false);
    CHECK(std::abs(angle.yaw - .05f) < 1e-6 && std::abs(angle.pitch + .1f) < 1e-6);
    auto bank = std::make_shared<DlsBank>();
    bank->waves.push_back({0, 48000, {0, 1, -1, .5f}, {}});
    DlsRegion region;
    region.key_high = region.velocity_high = 127;
    region.channel = 1;
    region.settings.unity_note = 60;
    region.settings.loop = DlsLoop{1, 2};
    DlsInstrument instrument;
    instrument.regions.push_back(region);
    bank->instruments.push_back(instrument);
    AudioMixer full, half, mute, sound_muted;
    for (auto* mixer : {&full, &half, &mute, &sound_muted})
    {
        mixer->inspect_pcm = true;
        CHECK(mixer->instruments.play(bank, 0, 60, 100, 2, 1, error));
    }
    CHECK(half.set_gains(1, .5f) && mute.set_gains(1, 0) && sound_muted.set_gains(0, 1));
    for (auto* mixer : {&full, &half, &mute, &sound_muted})
        CHECK(mixer->advance(.01, {}, {}));
    CHECK(full.peak > 0 && full.mixed == sound_muted.mixed && mute.peak == 0);
    for (unsigned i = 0; i < full.mixed.size(); ++i)
        CHECK(std::abs(half.mixed[i] - .5f * full.mixed[i]) < 1e-6);
    CHECK(mute.instruments.sample_clock() == 480 &&
          mute.instruments.voices()[0].cursor == full.instruments.voices()[0].cursor);
    CHECK(!mute.set_gains(-1, 0) && !mute.set_gains(0, 2));
    CHECK(mute.pause_output(true, error) && mute.advance(.1, {}, {}));
    CHECK(mute.instruments.sample_clock() == 480);
    std::cout
        << "Preferences atomic replacement/reload/errors, SDL sensitivity and independent music gain clocks passed\n";
    return 0;
}
