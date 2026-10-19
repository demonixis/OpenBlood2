#include "engine/resources.h"
#include "launcher/artwork.h"
#include "launcher/controller.h"
#include "launcher/demo_install.h"
#include "launcher/mods.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <miniz.h>

using namespace OpenBlood2;
using namespace OpenBlood2::Launcher;
namespace fs = std::filesystem;
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::cerr << __LINE__ << ": " #x "\n";                                                                     \
            return false;                                                                                              \
        }                                                                                                              \
    } while (false)
namespace
{
    void u32(std::vector<unsigned char>& b, std::size_t at, std::uint32_t v)
    {
        for (unsigned i = 0; i < 4; ++i)
            b[at + i] = static_cast<unsigned char>(v >> (8 * i));
    }
    std::vector<unsigned char> rez(unsigned char value = 42)
    {
        std::vector<unsigned char> b(200);
        b[0] = b[62] = b[124] = 13;
        b[1] = b[63] = b[125] = 10;
        b[126] = 26;
        u32(b, 127, 1);
        u32(b, 131, 169);
        u32(b, 135, 31);
        b[168] = value;
        u32(b, 173, 168);
        u32(b, 177, 1);
        b[197] = 'x';
        return b;
    }
    void write(const fs::path& path, const std::vector<unsigned char>& bytes)
    {
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    std::string read(const fs::path& path)
    {
        std::ifstream f(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(f), {}};
    }
    bool package(const fs::path& root, const std::vector<std::string>& names, DemoPackage& result)
    {
        mz_zip_archive zip{};
        CHECK(mz_zip_writer_init_heap(&zip, 0, 0));
        const auto bytes = rez();
        std::vector<std::string> safe;
        for (std::size_t i = 0; i < names.size(); ++i)
        {
            auto name = std::to_string(i);
            name.resize(names[i].size(), 'x');
            safe.push_back(name);
            CHECK(mz_zip_writer_add_mem(&zip, name.c_str(), bytes.data(), bytes.size(), 0));
        }
        void* output = nullptr;
        size_t size = 0;
        CHECK(mz_zip_writer_finalize_heap_archive(&zip, &output, &size));
        std::vector<unsigned char> data(static_cast<unsigned char*>(output),
                                        static_cast<unsigned char*>(output) + size);
        mz_free(output);
        CHECK(mz_zip_writer_end(&zip));
        // Inject path fixtures after writing, bypassing miniz's own writer-side path checks.
        for (std::size_t i = 0; i < names.size(); ++i)
        {
            auto at = data.begin();
            while ((at = std::search(at, data.end(), safe[i].begin(), safe[i].end())) != data.end())
            {
                std::copy(names[i].begin(), names[i].end(), at);
                at += names[i].size();
            }
        }
        write(root / "package.zip", data);
        write(root / "payload.rez", bytes);
        InstallProgress progress;
        std::string error;
        result.bytes = size;
        CHECK(file_sha256(root / "package.zip", result.sha256, error, progress));
        CHECK(file_sha256(root / "payload.rez", result.rez_sha256, error, progress));
        return true;
    }
    bool installer(const fs::path& root)
    {
        InstallProgress progress;
        std::string error;
        DemoPackage recipe{};
        CHECK(package(root, {"Game/B2Demo.rez", "Game/Readme.txt", "Game/Music/A1/a.sec", "SETUP.EXE"}, recipe));
        const auto target = root / "installed";
        CHECK(extract_demo(root / "package.zip", target, recipe, progress, error));
        CHECK(fs::exists(target / "B2Demo.rez") && fs::exists(target / "Music/A1/a.sec") &&
              fs::exists(target / "Readme.txt"));
        CHECK(!fs::exists(target / "SETUP.EXE"));
        CHECK(!extract_demo(root / "package.zip", target, recipe, progress, error));
        CHECK(fs::exists(target / "B2Demo.rez"));
        auto wrong = recipe;
        wrong.sha256[0] = wrong.sha256[0] == '0' ? '1' : '0';
        CHECK(!extract_demo(root / "package.zip", root / "wrong", wrong, progress, error));
        CHECK(!fs::exists(root / "wrong"));
        wrong = recipe;
        ++wrong.bytes;
        CHECK(!extract_demo(root / "package.zip", root / "wrong", wrong, progress, error));
        wrong = recipe;
        wrong.rez_sha256 = "bad";
        CHECK(!extract_demo(root / "package.zip", root / "wrong", wrong, progress, error));
        CHECK(!fs::exists(root / "wrong"));
        progress.cancel = true;
        CHECK(!extract_demo(root / "package.zip", root / "cancelled", recipe, progress, error));
        CHECK(!fs::exists(root / "cancelled"));
        progress.cancel = false;
        for (const std::string& bad : std::vector<std::string>{
                 "../escape", "/absolute", "Game/../../escape", "Game\\..\\escape", "C:/escape", "Game/CON",
                 "Game/name.", "Game/name ", "Game/b2demo.REZ", "Game/Music", "Game/" + std::string(1025, 'x')})
        {
            CHECK(package(root, {"Game/B2Demo.rez", "Game/Music/a.sec", bad}, recipe));
            CHECK(!extract_demo(root / "package.zip", root / "bad", recipe, progress, error));
            CHECK(!fs::exists(root / "bad") && !fs::exists(root / "escape"));
        }
        std::vector<std::string> many{"Game/B2Demo.rez"};
        for (int i = 0; i < 256; ++i)
            many.push_back("Game/entry" + std::to_string(i));
        CHECK(package(root, many, recipe));
        CHECK(!extract_demo(root / "package.zip", root / "many", recipe, progress, error));
        // A corrupt CRC still fails when the outer test recipe deliberately matches the modified archive.
        CHECK(package(root, {"Game/B2Demo.rez"}, recipe));
        auto data = read(root / "package.zip");
        CHECK(data.size() > 80);
        data[60] ^= 1;
        std::ofstream(root / "package.zip", std::ios::binary).write(data.data(), data.size());
        CHECK(file_sha256(root / "package.zip", recipe.sha256, error, progress));
        CHECK(!extract_demo(root / "package.zip", root / "crc", recipe, progress, error));
        CHECK(!fs::exists(root / "crc"));
        CHECK(demo_url_allowed("https://archive.org/download/B2demo/B2Demo.zip"));
        CHECK(demo_url_allowed("https://ia800000.us.archive.org/path"));
        for (auto url :
             {"http://archive.org/path", "https://archive.org.evil.test/path", "file:///tmp/demo",
              "https://evilarchive.org/path", "https://user@archive.org/path", "https://archive.org:444/path"})
            CHECK(!demo_url_allowed(url));
        return true;
    }
    bool mods(const fs::path& root, const fs::path& fixture)
    {
        const auto data = root / "data";
        fs::create_directory(data);
        write(data / "BLOOD2.REZ", rez(42));
        write(data / "B2Demo.rez", rez(43));
        write(root / "one.rez", rez(51));
        write(root / "two.rez", rez(52));
        Installation item{Edition::Retail, data};
        std::vector<Mod> mods{{item, root / "one.rez", true}, {item, root / "two.rez", false}};
        std::string error;
        CHECK(save_mods(root / "mods.cfg", mods, error));
        std::vector<Mod> loaded;
        CHECK(load_mods(root / "mods.cfg", loaded, error) && loaded == mods);
        CHECK(enabled_mods(loaded, {Edition::Demo, data}).empty());
        Config config{data, "test", false, enabled_mods(loaded, item)};
        Resources resources;
        std::vector<std::uint8_t> bytes;
        CHECK(resources.mount_game(config, error) && resources.read("x", bytes, error) && bytes[0] == 51);
        loaded[1].enabled = true;
        config.overlays = enabled_mods(loaded, item);
        CHECK(resources.mount_game(config, error) && resources.read("x", bytes, error) && bytes[0] == 52);
        CHECK(resources.find("x")->overrides == 2);
        config.overlays = {root / "missing.rez"};
        CHECK(!resources.mount_game(config, error));
        CHECK(resources.read("x", bytes, error) && bytes[0] == 52); // rollback
        config.overlays = {data / "BLOOD2.REZ"};
        CHECK(!resources.mount_game(config, error));
        config.overlays = {root / "one.rez", root / "one.rez"};
        CHECK(!resources.mount_game(config, error));
        write(root / "BLOOD2NM.REZ", rez());
        config.overlays = {root / "BLOOD2NM.REZ"};
        CHECK(!resources.mount_game(config, error));
        auto code = rez();
        code[189] = 'L';
        code[190] = 'L';
        code[191] = 'D';
        write(root / "code.rez", code);
        config.overlays = {root / "code.rez"};
        CHECK(!resources.mount_game(config, error));
        config.overlays.clear();
        CHECK(resources.mount_game(config, error));
        CHECK(resources.read("x", bytes, error) && bytes[0] == 42);
        loaded.push_back(loaded.front());
        CHECK(!save_mods(root / "mods.cfg", loaded, error));
        std::ofstream(root / "future.cfg") << "version=2\n";
        CHECK(!load_mods(root / "future.cfg", loaded, error));
        CHECK(!save_mods(root / "future.cfg", mods, error) && read(root / "future.cfg") == "version=2\n");
        Options options;
        const auto overlay = path_text(root / "one.rez");
        const std::vector<std::string_view> args{"--rez", overlay, "--data-dir", "ignored"};
        CHECK(parse_options(args, options, error) && options.overlays.size() == 1);
        CHECK(!parse_options(std::vector<std::string_view>{"--rez", "relative.rez"}, options, error));
        Controller controller;
        CHECK(controller.prepare(item, fixture, {root / "one.rez"}));
        const auto wait = [&]()
        {
            const auto end = SDL_GetTicksNS() + 5000000000ull;
            while (controller.busy() && SDL_GetTicksNS() < end)
            {
                controller.update();
                SDL_Delay(1);
            }
            return !controller.busy();
        };
        CHECK(wait() && controller.playable());
        CHECK(controller.play(root / "state"));
        CHECK(wait() && controller.status() == Controller::Status::Finished);
        const auto argv = read(controller.logs() / "argv.txt");
        CHECK(argv.find("--rez\n" + overlay + "\n") != argv.npos);
        CHECK(controller.prepare(item, fixture));
        CHECK(wait() && controller.playable() && controller.play(root / "state"));
        CHECK(wait() && controller.status() == Controller::Status::Finished);
        CHECK(read(controller.logs() / "argv.txt").find("--rez") == std::string::npos);
        CHECK(controller.prepare(item, fixture, {root / "one.rez"}));
        CHECK(wait() && controller.playable());
        write(root / "one.rez", {});
        CHECK(controller.play(root / "state"));
        CHECK(wait() && !controller.playable());
        return true;
    }
    bool artwork(const fs::path& data)
    {
        std::vector<std::uint8_t> image{1, 2, 3};
        CHECK(!decode_artwork({}, image) && image.size() == 3);
        auto malformed = rez();
        malformed[0] = 'M';
        malformed[1] = 'Z';
        u32(malformed, 60, 0xffffffff);
        CHECK(!decode_artwork(malformed, image) && image.size() == 3);
        if (!data.empty())
            CHECK(load_artwork(data, image) && image.size() == 320 * 240 * 4);
        return true;
    }
    bool install_real(const fs::path& root, const fs::path& zip)
    {
        DemoInstaller installer;
        CHECK(installer.start(root, zip));
        CHECK(!installer.start(root, zip));
        InstallResult result;
        const auto end = SDL_GetTicksNS() + 650000000000ull;
        while (!installer.poll(result) && SDL_GetTicksNS() < end)
            SDL_Delay(20);
        if (!result.error.empty())
            std::cerr << result.error << '\n';
        CHECK(result.success && fs::exists(result.directory / "B2Demo.rez"));
        CHECK(fs::exists(result.directory / "Readme.txt") && fs::exists(result.directory / "Music/A1/Ancient.dls"));
        State state;
        state.installations.push_back({Edition::Demo, result.directory});
        state.selected = 0;
        std::string error;
        CHECK(save_state(root / "launcher.cfg", state, error));
        std::cout << "Installed: " << path_text(result.directory) << '\n';
        CHECK(artwork(result.directory));
        if (!zip.empty())
        {
            CHECK(installer.start(root, zip));
            installer.cancel();
            InstallResult cancelled;
            while (!installer.poll(cancelled) && SDL_GetTicksNS() < end)
                SDL_Delay(1);
            CHECK(!cancelled.success && cancelled.directory.empty());
            CHECK(fs::exists(result.directory / "B2Demo.rez"));
            for (const auto& entry : fs::directory_iterator(root / "content"))
                CHECK(!path_text(entry.path().filename()).starts_with(".demo-install-"));
        }
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 3)
        return 2;
    const std::string_view mode(argv[1]);
    const auto root = fs::absolute(path_from_utf8(argv[2]));
    std::error_code ec;
    if (mode != "download" && mode != "install")
        fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    if (ec)
        return 2;
    const auto extra = argc > 3 ? fs::absolute(path_from_utf8(argv[3])) : fs::path{};
    bool ok = mode == "installer"  ? installer(root)
              : mode == "mods"     ? mods(root, extra)
              : mode == "artwork"  ? artwork({})
              : mode == "install"  ? install_real(root, extra)
              : mode == "download" ? install_real(root, {})
                                   : false;
    SDL_Quit();
    return ok ? 0 : 1;
}
