#include "app/demo_install.h"
#include "app/game_setup.h"
#include "engine/resources.h"
#include "scratch_dir.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <miniz.h>

using namespace OpenBlood2;
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
    // A one-entry REZ ("x" holding `value`).
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
        f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    }
    void write(const fs::path& path, std::string_view text)
    {
        std::ofstream(path, std::ios::binary) << text;
    }
    std::string read(const fs::path& path)
    {
        std::ifstream f(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(f), {}};
    }

    bool state(const fs::path& root)
    {
        std::string error;
        const auto file = root / "setup.cfg";
        SetupState loaded;
        loaded.added.push_back({});
        CHECK(load_setup(file, loaded, error) && loaded == SetupState{}); // Missing: empty.
        const Installation retail{Edition::Retail, root / "Blood2"}, demo{Edition::Demo, root / "Blood2 démo"};
        SetupState saved;
        saved.selected = demo;
        saved.added = {retail, demo};
        saved.mods = {{retail, root / "a.rez", true}, {retail, root / "b.rez", false}, {demo, root / "a.rez", true}};
        CHECK(save_setup(file, saved, error));
        CHECK(load_setup(file, loaded, error) && loaded == saved);
        CHECK(read(file).starts_with("version=1\nselected.edition=demo\n"));
        // Order and enabled mods follow the installation.
        auto config = setup_config(saved, retail);
        CHECK(config.data_dir == retail.directory && !config.demo && config.overlays == std::vector{root / "a.rez"});
        CHECK(setup_config(saved, demo).demo && setup_config(saved, demo).overlays.size() == 1);
        // Strict reading: unknown, duplicate, incomplete, relative, control characters, future versions.
        const auto refused = [&](std::string_view text)
        {
            write(file, text);
            SetupState out = saved;
            return !load_setup(file, out, error) && out == saved;
        };
        CHECK(refused("version=2\n"));
        CHECK(refused("selected.edition=retail\nselected.path=/a\n"));
        CHECK(refused("version=1\nunknown=1\n"));
        CHECK(refused("version=1\nversion=1\n"));
        CHECK(refused("version=1\nselected.edition=retail\n"));
        CHECK(refused("version=1\nselected.edition=full\nselected.path=/a\n"));
        CHECK(refused("version=1\nadded.0.edition=retail\nadded.0.path=relative\n"));
        CHECK(refused("version=1\nadded.0.edition=retail\nadded.0.path=/a\nadded.1.edition=retail\nadded.1.path=/a\n"));
        CHECK(refused("version=1\nmod.0.edition=retail\nmod.0.root=/a\nmod.0.path=/a/m.rez\nmod.0.enabled=2\n"));
        CHECK(refused("version=1\nmod.0.edition=retail\nmod.0.root=/a\nmod.0.path=/a/m.rez\n"));
        CHECK(refused("version=1\nadded.0.edition=retail\nadded.0.path=/a\x01\n"));
        CHECK(refused(std::string("version=1\n#") + std::string(70000, 'x') + "\n"));
        // CRLF and comments are accepted; gaps end the numbered records (the rest is unknown).
        write(file, "# setup\r\nversion=1\r\n");
        CHECK(load_setup(file, loaded, error) && loaded == SetupState{});
        CHECK(refused("version=1\nadded.1.edition=retail\nadded.1.path=/a\n"));
        // Saving validates first and keeps the previous file.
        CHECK(save_setup(file, saved, error));
        auto duplicate = saved;
        duplicate.added.push_back(retail);
        CHECK(!save_setup(file, duplicate, error) && load_setup(file, loaded, error) && loaded == saved);
        auto many = saved;
        many.mods.assign(max_setup_mods + 1, saved.mods.front());
        for (std::size_t i = 0; i < many.mods.size(); ++i)
            many.mods[i].archive = root / (std::to_string(i) + ".rez");
        CHECK(!save_setup(file, many, error));
        CHECK(!valid_setup_text("\xc3\x28", 10) && valid_setup_text("d\xc3\xa9mo", 10) && !valid_setup_text("abc", 2));
        return true;
    }

    bool detect(const fs::path& root)
    {
        std::string error;
        const auto retail = root / "Retail", demo = root / "Demo", both = root / "Both", none = root / "None";
        for (const auto& folder : {retail, demo, both, none})
            fs::create_directories(folder);
        write(retail / "blood2.rez", rez());
        write(demo / "B2DEMO.REZ", rez());
        write(both / "BLOOD2.REZ", rez());
        write(both / "b2demo.rez", rez());
        CHECK((folder_installations(retail) == std::vector<Installation>{{Edition::Retail, retail}}));
        CHECK((folder_installations(demo) == std::vector<Installation>{{Edition::Demo, demo}}));
        CHECK(folder_installations(both).size() == 2 && folder_installations(none).empty());
        // Downloaded demos are found in the user directory's content/<install>/Game.
        const auto user = root / "user";
        const auto downloaded = setup_content_folder(user) / "Blood2Demo-1.0-1" / "Game";
        fs::create_directories(downloaded);
        fs::create_directories(setup_content_folder(user) / ".demo-install-1" / "Game");
        write(downloaded / "B2Demo.rez", rez());
        write(setup_content_folder(user) / ".demo-install-1" / "Game" / "B2Demo.rez", rez());
        SetupState state;
        state.added = {{Edition::Retail, retail}, {Edition::Retail, none}};
        const Installation running{Edition::Retail, both};
        const auto entries = scan_setup(state, user, {running, {Edition::Retail, retail}});
        const auto find = [&](const Installation& item) -> const SetupEntry*
        {
            for (const auto& entry : entries)
                if (entry.installation == item)
                    return &entry;
            return nullptr;
        };
        CHECK(find({Edition::Retail, retail}) && find({Edition::Retail, retail})->added &&
              find({Edition::Retail, retail})->error.empty());
        CHECK(find({Edition::Retail, none}) && !find({Edition::Retail, none})->error.empty());
        CHECK(find(running) && !find(running)->added);
        CHECK(find({Edition::Demo, downloaded}) && find({Edition::Demo, downloaded})->error.empty());
        CHECK(std::none_of(
            entries.begin(), entries.end(), [](const SetupEntry& entry)
            { return path_text(entry.installation.directory).find(".demo-install") != std::string::npos; }));
        CHECK(std::count_if(entries.begin(), entries.end(), [&](const SetupEntry& entry)
                            { return entry.installation == Installation{Edition::Retail, retail}; }) == 1);
        // A stored selection is playable only while its data is.
        Config config;
        state.selected = Installation{Edition::Retail, retail};
        CHECK(setup_ready(state, config, error) && config.data_dir == retail && !config.demo);
        state.selected = Installation{Edition::Demo, retail};
        CHECK(!setup_ready(state, config, error) && !error.empty());
        state.selected.reset();
        CHECK(!setup_ready(state, config, error) && error.empty());
        // The chosen mods mount over the base archive in order.
        write(root / "one.rez", rez(51));
        write(root / "two.rez", rez(52));
        state.mods = {{{Edition::Retail, retail}, root / "one.rez", true},
                      {{Edition::Retail, retail}, root / "two.rez", true}};
        Resources resources;
        std::vector<std::uint8_t> bytes;
        CHECK(resources.mount_game(setup_config(state, {Edition::Retail, retail}), error) &&
              resources.read("x", bytes, error) && bytes[0] == 52);
        state.mods[1].enabled = false;
        CHECK(resources.mount_game(setup_config(state, {Edition::Retail, retail}), error) &&
              resources.read("x", bytes, error) && bytes[0] == 51);
        return true;
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
                at += std::ptrdiff_t(names[i].size());
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
        // The engine's SHA-256 replaces OpenSSL: the empty-file digest.
        write(root / "empty", std::string_view{});
        std::string digest;
        CHECK(file_sha256(root / "empty", digest, error, progress) &&
              digest == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
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
        // A corrupt CRC still fails when the recipe deliberately matches the modified archive.
        CHECK(package(root, {"Game/B2Demo.rez"}, recipe));
        auto data = read(root / "package.zip");
        CHECK(data.size() > 80);
        data[60] ^= 1;
        std::ofstream(root / "package.zip", std::ios::binary).write(data.data(), std::streamsize(data.size()));
        CHECK(file_sha256(root / "package.zip", recipe.sha256, error, progress));
        CHECK(!extract_demo(root / "package.zip", root / "crc", recipe, progress, error));
        CHECK(!fs::exists(root / "crc"));
        return true;
    }

    bool urls()
    {
        CHECK(demo_url_allowed("https://archive.org/download/B2demo/B2Demo.zip"));
        CHECK(demo_url_allowed("https://ia800000.us.archive.org/path"));
        CHECK(demo_url_allowed("https://ARCHIVE.org:443/path?x#y"));
        for (auto url : {"http://archive.org/path", "https://archive.org.evil.test/path", "file:///tmp/demo",
                         "https://evilarchive.org/path", "https://user@archive.org/path",
                         "https://archive.org:444/path", "https://.archive.org/", "https://[::1]/archive.org",
                         "https://archive.org%2eevil.test/", "https:///archive.org", "https://archive.org:/"})
            CHECK(!demo_url_allowed(url));
        return true;
    }

    // Manual: the qualified Archive.org download (or a local ZIP), never registered with CTest.
    bool install_real(const fs::path& root, const fs::path& zip)
    {
        DemoInstaller installer;
        CHECK(installer.start(root / "content", zip));
        CHECK(!installer.start(root / "content", zip));
        InstallResult result;
        const auto end = SDL_GetTicksNS() + 650000000000ull;
        std::uint64_t reported = 0;
        while (!installer.poll(result) && SDL_GetTicksNS() < end)
        {
            if (const auto bytes = installer.progress.bytes.load(); bytes >= reported + 4 * 1024 * 1024)
            {
                reported = bytes;
                std::cout << "Progress: " << bytes << " / " << installer.progress.total.load() << '\n';
            }
            SDL_Delay(20);
        }
        if (!result.error.empty())
            std::cerr << result.error << '\n';
        CHECK(result.success && fs::exists(result.directory / "B2Demo.rez"));
        CHECK(fs::exists(result.directory / "Readme.txt") && fs::exists(result.directory / "Music/A1/Ancient.dls"));
        CHECK((folder_installations(result.directory) == std::vector<Installation>{{Edition::Demo, result.directory}}));
        std::cout << "Installed: " << path_text(result.directory) << '\n';
        return true;
    }
} // namespace
int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::string_view mode(argv[1]);
    if (mode == "urls")
        return urls() ? 0 : 1;
    if (argc < 3)
        return 2;
    const auto root = fs::absolute(path_from_utf8(argv[2]));
    std::error_code ec;
    if (mode != "download" && mode != "install" && !OpenBlood2::Testing::remove_scratch_dir(root))
        return 2;
    fs::create_directories(root, ec);
    if (ec)
        return 2;
    const auto extra = argc > 3 ? fs::absolute(path_from_utf8(argv[3])) : fs::path{};
    const bool ok = mode == "state"       ? state(root)
                    : mode == "detect"    ? detect(root)
                    : mode == "installer" ? installer(root)
                    : mode == "install"   ? install_real(root, extra)
                    : mode == "download"  ? install_real(root, {})
                                          : false;
    SDL_Quit();
    return ok ? 0 : 1;
}
